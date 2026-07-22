#include <nfui/Controls/Button.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Easing.hpp>

namespace nfui {

bool Button::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams owner_params = params;
    owner_params.style &= ~WS_BORDER;
    // CP15: BS_FLAT alongside BS_OWNERDRAW is poison — Windows draws the
    // default flat-button chrome (light-grey fill RGB(240,238,230)) once
    // during the first paint cycle before our DRAWITEM handler reaches the
    // DC. In dark mode that frame is visible as a white flash on the
    // rounded corner until the next full repaint. Drop BS_FLAT so the
    // first-paint chrome goes straight to our owner-draw handler.
    return create_native(L"BUTTON", owner_params, BS_OWNERDRAW);
}

// CP17: discrete face for a state. Used both to seed the cross-fade endpoints
// (from = face_for(previous hover), to = face_for(current hover)) and as the
// fallback when animation is suppressed. Mirrors the per-state face logic the
// paint path used before CP17, factored out so the transition detector and the
// paint path cannot diverge.
Color Button::face_for(const PaintState& state, const ThemePalette& p, bool hc) const noexcept {
    if (!state.enabled) {
        // CP10B: HC disabled lifts to surface_hover; standard disabled is a
        // 12% border→surface tint (WCAG AA vs text_secondary).
        return hc ? p.surface_hover : alpha_blend(p.border, p.surface, 0.12f);
    }
    if (state.pressed) {
        // CP10B: HC pressed inverts to accent_text; standard pressed is an 82%
        // accent→background tint.
        return hc ? p.accent_text : alpha_blend(p.accent, p.background, 0.82f);
    }
    if (state.hover) {
        return p.accent_hover;
    }
    return p.accent;
}

void Button::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* palette_ptr = palette();
    const ThemePalette& p = palette_ptr ? *palette_ptr : theme_palette(ThemeMode::light);
    const int theme_radius = theme_metrics().corner_radius_control;
    const int radius = style_.corner_radius.value_or(theme_radius);
    const bool hc = is_high_contrast(p);
    // CP17: the hover cross-fade only runs on the enabled, non-pressed face
    // (rest ↔ hover). Pressed / disabled are anchor states that must read
    // at-a-glance (CP3), so they snap. HC and the system "animate controls"
    // setting also suppress the transition.
    const bool can_anim = !hc && state.enabled && !state.pressed && system_animations_enabled();
    const Color target_face = face_for(state, p, hc);

    // Seed a cross-fade on a hover transition. The first paint after the flip
    // samples at t≈0 → the old face, so there is no flash; the animation timer
    // then repaints the interpolation to the new face. If a fade is already
    // running (rapid enter/leave), seed `from` from the currently-displayed
    // color so the reversal is seamless instead of jumping to the previous
    // endpoint.
    if (state.hover != last_hover_) {
        if (can_anim) {
            PaintState prev = state;
            prev.hover = last_hover_;
            const Color from_face = hover_anim_.is_active()
                ? hover_anim_.sample(clock_now_ms(), ease_out_cubic)
                : face_for(prev, p, hc);
            hover_anim_.begin(from_face, target_face, clock_now_ms(), 120);
            start_anim_timer(16);
        } else {
            hover_anim_.cancel();
        }
        last_hover_ = state.hover;
    }

    Color face = target_face;
    if (can_anim && hover_anim_.is_active()) {
        face = hover_anim_.sample(clock_now_ms(), ease_out_cubic);
    } else if (!can_anim) {
        hover_anim_.cancel();
    }

    // CP10B (HC accessibility): in the high-contrast profile the bright accent
    // face swallows a 1px white border (yellow on white = 1.22:1). Use the
    // accent_text (black) as the border when the face is one of the bright
    // accents so the outline always clears 17:1. Border stays `p.border`
    // (white) when the face is dark (disabled/pressed-inverted).
    const Color accent_border = (p.accent.rgb == p.background.rgb)
        ? p.border
        : p.accent_text;
    const Color border = state.focused
        ? style_.border_color.value_or(accent_border)
        : style_.border_color.value_or(p.border);
    // text_color is constant across rest/hover (accent_text); only the anchor
    // states (disabled, HC-pressed) change it, and those do not animate.
    Color text_color = p.accent_text;
    if (!state.enabled) {
        text_color = p.text_secondary;
    } else if (state.pressed && hc) {
        text_color = p.accent;
    }
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;
    // RoundRect leaves the four outside corner pixels untouched. Clear those
    // pixels from the current palette first so a theme swap cannot preserve a
    // frame from the previous palette in the rounded boundary.
    fill_rect(target, paint_bounds, p.background);
    // CP16: rest and hover faces get a gentle vertical gradient so the
    // button reads as a 3D card with light from above. Pressed / disabled
    // stay flat (CP3 decision record: muted/anchor states need a single
    // tone so they read clearly at small sizes). HC stays flat too. The
    // CP17 interpolated face flows straight into the gradient endpoints.
    const bool use_gradient = !hc && (state.enabled) && !state.pressed;
    if (use_gradient) {
        const Color top    = lighten(face, 0.08f);
        const Color bottom = darken(face, 0.10f);
        paint_linear_gradient(target, paint_bounds, radius, top, bottom);
        // 1px hairline border so the gradient edge stays crisp against
        // the surrounding palette.background.
        const HPEN pen = CreatePen(PS_SOLID, 1, border.rgb);
        if (pen != nullptr) {
            const HGDIOBJ old_pen = SelectObject(target, pen);
            const HGDIOBJ old_brush = SelectObject(target, GetStockObject(NULL_BRUSH));
            RoundRect(target, paint_bounds.left, paint_bounds.top,
                      paint_bounds.right, paint_bounds.bottom, radius * 2, radius * 2);
            SelectObject(target, old_brush);
            SelectObject(target, old_pen);
            DeleteObject(pen);
        }
    } else {
        fill_rounded_rect(target, paint_bounds, radius, face, border);
    }
    // CP9 accessibility: keyboard focus previously reused `accent_hover` for the
    // border, which is the same colour as mouse-hover — so a tab-focused button
    // was visually indistinguishable from a hovered one (fails WCAG 2.4.7 Focus
    // Visible / 2.4.11). Draw a distinct inset ring in `accent_text` (guaranteed
    // to contrast the accent face per theme design, including the HC palette)
    // whenever ODS_FOCUS is reflected. The inset rounded rect re-fills the
    // interior with `face` (no visual change) and its border becomes the ring;
    // text is drawn afterwards so it stays on top. See
    // docs/KNOWLEDGE/polish/2026-07-23-cp9-accessibility.md.
    if (state.focused && state.enabled) {
        const DpiScale scale(dpi_of(hwnd()));
        const int inset = scale.logical_to_pixels(3);
        const RECT ring{
            paint_bounds.left + inset,
            paint_bounds.top + inset,
            paint_bounds.right - inset,
            paint_bounds.bottom - inset,
        };
        if (ring.right > ring.left && ring.bottom > ring.top) {
            const int ring_radius = radius > inset ? radius - inset : 0;
            const Color ring_color = style_.focus_ring_color.value_or(p.accent_text);
            fill_rounded_rect(target, ring, ring_radius, face, ring_color);
        }
    }
    FontCache* cache = fonts();
    HFONT font = nullptr;
    if (cache != nullptr) {
        const int dpi = dpi_of(hwnd());
        font = style_.use_semibold.value_or(false)
            ? cache->semibold(dpi, font_pt::ui)
            : cache->regular(dpi, font_pt::ui);
    }
    // CP18: optional leading vector glyph. The icon + gap + caption are
    // centred as a single unit inside the button (measuring the caption width
    // first) so a short label like "Add" does not sit right-of-centre. When
    // the group is wider than the button, the icon anchors at the left pad
    // and the caption ellipsises in the remaining space (no clipping). The
    // glyph colour defaults to the caption colour so it tracks disabled /
    // HC-pressed the same way the text does.
    RECT text_bounds = paint_bounds;
    const IconKind icon_kind = style_.icon.value_or(IconKind::none);
    if (icon_kind != IconKind::none) {
        const DpiScale scale(dpi_of(hwnd()));
        const int cell = scale.logical_to_pixels(16);
        const int pad = scale.logical_to_pixels(6);
        const int gap = scale.logical_to_pixels(4);
        const int cell_y = paint_bounds.top + (paint_bounds.bottom - paint_bounds.top - cell) / 2;
        // Measure the caption with the selected font so the icon+text pair
        // can be centred as a unit.
        int text_w = 0;
        if (font != nullptr) {
            const HGDIOBJ old = SelectObject(target, font);
            SIZE sz{};
            const std::size_t len = caption().size();
            const int n = (len <= static_cast<std::size_t>(0x7fffffff))
                ? static_cast<int>(len) : 0x7fffffff;
            if (GetTextExtentPoint32W(target, caption().data(), n, &sz)) {
                text_w = static_cast<int>(sz.cx);
            }
            if (old && old != HGDI_ERROR) SelectObject(target, old);
        }
        const int btn_w = paint_bounds.right - paint_bounds.left;
        const int group_w = cell + gap + text_w;
        const bool group_fits = (group_w + 2 * pad) <= btn_w;
        const int icon_x = group_fits
            ? paint_bounds.left + (btn_w - group_w) / 2   // centre the group
            : paint_bounds.left + pad;                    // too wide: anchor left
        const RECT glyph{ icon_x, cell_y, icon_x + cell, cell_y + cell };
        const Color icon_col = style_.icon_color.value_or(text_color);
        draw_vector_icon(target, icon_kind, glyph, icon_col,
                         scale.logical_to_pixels(2));
        text_bounds.left = glyph.right + gap;
        if (group_fits) {
            // Tight rect right after the glyph so the caption sits flush with
            // it and the pair reads as one centred unit.
            text_bounds.right = text_bounds.left + text_w;
        }
        if (text_bounds.left > text_bounds.right) text_bounds.left = text_bounds.right;
    }
    draw_text(target, text_bounds, caption(), font, text_color,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void Button::on_palette_changed() noexcept {
    // CP17: ColorAnimation stamped its from/to endpoints from the palette that
    // was live when the hover transition began. A set_palette() mid-fade (theme
    // cross-fade, WM_THEMECHANGED) replaces the palette, but the in-flight
    // animation would keep interpolating the OLD endpoints and then snap to
    // the new face at completion. Cancel it: the next paint recomputes face
    // from the live palette, so during a theme cross-fade the hovered button
    // tracks the interpolated palette smoothly. The animation tick self-stops
    // on its next fire (it sees !is_active).
    hover_anim_.cancel();
}

void Button::on_animation_tick(unsigned long long now_ms) noexcept {
    if (!hover_anim_.is_active()) {
        stop_anim_timer();
        return;
    }
    // Advance the machine; sample() clears `active` once the duration elapses.
    // The visual color is sampled again in on_paint — here we only need to know
    // whether more frames are required. Async InvalidateRect (never UpdateWindow)
    // so on_paint is not re-entered synchronously inside this tick.
    static_cast<void>(hover_anim_.sample(now_ms, ease_out_cubic));
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
    if (!hover_anim_.is_active()) {
        stop_anim_timer();
    }
}

} // namespace nfui
