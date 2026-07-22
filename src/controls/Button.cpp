#include <nfui/Controls/Button.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

namespace nfui {

bool Button::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams owner_params = params;
    owner_params.style &= ~WS_BORDER;
    return create_native(L"BUTTON", owner_params, BS_OWNERDRAW | BS_FLAT);
}

void Button::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* palette_ptr = palette();
    const ThemePalette& p = palette_ptr ? *palette_ptr : theme_palette(ThemeMode::light);
    const int theme_radius = theme_metrics().corner_radius_control;
    const int radius = style_.corner_radius.value_or(theme_radius);
    const bool hc = is_high_contrast(p);
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
    Color face = p.accent;
    Color text_color = p.accent_text;
    if (!state.enabled) {
        if (hc) {
            // CP10B (HC accessibility): the standard "tint border 12% toward
            // surface" disabled face only yields ~RGB(48,48,48) on the HC
            // palette, which fails 3:1 against the pure-black window. Lift
            // to surface_hover instead so the button still reads as a
            // separate element. Reference: docs/KNOWLEDGE/polish/
            // 2026-07-23-cp10-high-contrast.md.
            face = p.surface_hover;
            text_color = p.text_secondary;
        } else {
            // WCAG AA: lighten border 12% toward surface for disabled face so
            // text_secondary clears the 4.5:1 contrast threshold. Reference:
            // docs/KNOWLEDGE/polish/2026-07-22-button-disabled-state-color.md.
            face = alpha_blend(p.border, p.surface, 0.12f);
            text_color = p.text_secondary;
        }
    } else if (state.pressed) {
        if (hc) {
            // CP10B (HC accessibility): the standard "82% tint accent toward
            // background" formula collapses to a muddy ~RGB(46,42,11) on HC,
            // which fails 3:1. Invert the rest face (accent_text + accent
            // text_color) so the pressed state reads as a deliberate
            // "sunken" cue distinct from both rest (yellow) and hover (cyan).
            face = p.accent_text;
            text_color = p.accent;
        } else {
            // Keep pressed visually distinct from hover without adding a palette token.
            face = alpha_blend(p.accent, p.background, 0.82f);
        }
    } else if (state.hover) {
        face = p.accent_hover;
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
    fill_rounded_rect(target, paint_bounds, radius, face, border);
    FontCache* cache = fonts();
    HFONT font = nullptr;
    if (cache != nullptr) {
        const int dpi = dpi_of(hwnd());
        font = style_.use_semibold.value_or(false)
            ? cache->semibold(dpi, font_pt::ui)
            : cache->regular(dpi, font_pt::ui);
    }
    draw_text(target, paint_bounds, caption(), font, text_color,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

} // namespace nfui
