#include <nfui/Controls/CheckBox.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>
#include <nfui/ThemeBroker.hpp>
#include <nfui/VectorIcon.hpp>

#include <algorithm>

namespace nfui {

bool CheckBox::create(const ControlCreateParams& params) noexcept {
    // CP31: BS_OWNERDRAW hands the entire client area to on_paint via the
    // reflected WM_DRAWITEM path. BS_AUTOCHECKBOX cannot be ORed with
    // BS_OWNERDRAW because the button type bits overlap, so the check state
    // is managed manually in the subclass-proc overrides (BM_GETCHECK /
    // BM_SETCHECK / click / space).
    if (!create_native(L"BUTTON", params, BS_OWNERDRAW)) {
        return false;
    }
    // CP-A2: disable comctl32 visual theming so the native button does not
    // double-paint its dark/HC chrome over our state_palette() chrome.
    theme_disable_window_theme(hwnd());
    return true;
}

void CheckBox::set_checked(bool checked) noexcept {
    if (checked_ == checked && !indeterminate_) return;
    checked_ = checked;
    indeterminate_ = false;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void CheckBox::set_indeterminate(bool indeterminate) noexcept {
    if (indeterminate_ == indeterminate) return;
    indeterminate_ = indeterminate;
    if (indeterminate) {
        checked_ = false;
    }
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void CheckBox::set_tristate(bool enabled) noexcept {
    tristate_ = enabled;
    // Demote from indeterminate to unchecked when tristate is disabled so the
    // visible state cannot be "stuck" in a value the control refuses to
    // re-enter.
    if (!tristate_ && indeterminate_) {
        indeterminate_ = false;
        if (hwnd() != nullptr) {
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
    }
}

LRESULT CheckBox::on_reflected_get_check() const noexcept {
    if (indeterminate_) return BST_INDETERMINATE;
    return checked_ ? BST_CHECKED : BST_UNCHECKED;
}

void CheckBox::on_reflected_set_check(LPARAM state) noexcept {
    if (state == BST_CHECKED) {
        set_checked(true);
    } else if (state == BST_INDETERMINATE) {
        if (tristate_) {
            set_indeterminate(true);
        } else {
            set_checked(false);
        }
    } else {
        set_checked(false);
    }
}

void CheckBox::on_subclass_lbutton_up() noexcept {
    if (indeterminate_) {
        // Indeterminate -> checked on click (matches native BS_AUTO3STATE).
        set_checked(true);
    } else {
        set_checked(!checked_);
    }
}

void CheckBox::on_subclass_key_down(UINT vk, LPARAM) noexcept {
    if (vk == VK_SPACE) {
        if (indeterminate_) {
            set_checked(true);
        } else {
            set_checked(!checked_);
        }
    }
}

// CP-A2: full state matrix for the check box. Borders, box fill, and the
// check / dash glyph tints all flow from `state_palette()` so a single
// StatePalette drives every visual variant. The focus ring draws from the
// resolved state's accent colour; disabled fades to text_secondary so the
// control never pretends to be interactive.
void CheckBox::on_paint(HDC dc, const PaintState& state) noexcept {
    HWND w = hwnd();
    if (dc == nullptr || w == nullptr) return;

    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    // Resolve the canonical ControlState and use state_palette() for every
    // colour. PaintState carries the same signals (hover/pressed/focused/
    // enabled) but state_palette() guarantees the chrome matches Edit and
    // ComboBox exactly across all six states.
    const ControlState cs = visual_state();
    const StatePalette sp = state_palette(p, cs);
    const DpiScale scale(dpi_of(w));

    // Box metrics: 16 logical px square, 8 logical px gap to the label,
    // corner radius from the theme tokens. The checkmark stroke is 2 logical
    // px so it stays crisp at high DPI.
    constexpr int kBoxLogical = 16;
    constexpr int kGapLogical = 8;
    constexpr int kCornerRadiusLogical = 4;   // rounded square, not a circle
    const int box_size = scale.logical_to_pixels(kBoxLogical);
    const int gap = scale.logical_to_pixels(kGapLogical);
    const int raw_radius = scale.logical_to_pixels(kCornerRadiusLogical);
    const int radius = std::min(raw_radius, box_size / 2);

    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;

    // CP28: clear with the window background so rounded-corner pixels never
    // carry a stale palette frame from the previous paint cycle.
    fill_rect(target, paint_bounds, p.background);

    paint_check(target, state, sp, box_size, radius);

    // Caption label to the right of the box.
    const int box_y = paint_bounds.top
        + (paint_bounds.bottom - paint_bounds.top - box_size) / 2;
    const RECT box{
        paint_bounds.left,
        box_y,
        paint_bounds.left + box_size,
        box_y + box_size,
    };

    FontCache* cache = fonts();
    HFONT font = nullptr;
    if (cache != nullptr) {
        font = cache->regular(scale.dpi(), font_pt::ui);
    }
    const Color text_color = (cs == ControlState::disabled) ? p.text_secondary : p.text;
    RECT text_bounds{
        box.right + gap,
        paint_bounds.top,
        paint_bounds.right,
        paint_bounds.bottom,
    };
    if (text_bounds.left < text_bounds.right) {
        draw_text(target, text_bounds, caption(), font, text_color,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    // CP19: 2px accent focus ring around the whole client rect when focused.
    if (cs == ControlState::focused) {
        paint_focus_border(target, paint_bounds, sp.accent, 2);
    }
}

// CP-A2: paint the rounded-square box + check / dash glyph. Owns no
// offscreen surface — the caller passes the resolved DC (MemoryDC or raw)
// and the box dimensions so we paint into the same buffer the label + focus
// ring will use. The state-resolved StatePalette drives every colour so a
// disabled box fades to the StatePalette's foreground, not the raw
// text_secondary, keeping chrome consistent with Edit/ComboBox.
void CheckBox::paint_check(HDC dc, const PaintState& state,
                           const StatePalette& sp,
                           int box_size, int radius) noexcept {
    HWND w = hwnd();
    if (w == nullptr || dc == nullptr) return;

    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const DpiScale scale(dpi_of(w));

    constexpr int kCheckStrokeLogical = 2;
    constexpr int kDashInsetLogical = 4;
    constexpr int kDashHeightLogical = 2;
    const int check_stroke = scale.logical_to_pixels(kCheckStrokeLogical);

    const RECT& b = state.bounds;
    const RECT paint_bounds{0, 0, b.right - b.left, b.bottom - b.top};

    const int box_y = paint_bounds.top
        + (paint_bounds.bottom - paint_bounds.top - box_size) / 2;
    const RECT box{
        paint_bounds.left,
        box_y,
        paint_bounds.left + box_size,
        box_y + box_size,
    };

    const bool checked = checked_;
    const bool indeterminate = indeterminate_;
    const bool enabled = state.enabled;

    // Border uses the resolved state's border colour (hover/pressed/focused
    // already nudge toward accent via state_palette()).
    const Color border = sp.border;
    // Checked / indeterminate fill with the resolved accent; unchecked shows
    // the resolved background (surface in rest, surface_hover in hover, etc.).
    const Color box_fill = (checked || indeterminate) ? sp.accent : sp.background;

    fill_rounded_rect(dc, box, radius, box_fill, border);

    if (checked) {
        // Vector checkmark drawn in the resolved foreground so it contrasts
        // the accent fill (accent_text on light/dark; on HC it is the WCAG
        // foreground).
        const int inset = scale.logical_to_pixels(2);
        RECT glyph{
            box.left + inset,
            box.top + inset,
            box.right - inset,
            box.bottom - inset,
        };
        if (glyph.right > glyph.left && glyph.bottom > glyph.top) {
            draw_vector_icon(dc, IconKind::check, glyph,
                             sp.foreground, check_stroke);
        }
    } else if (indeterminate) {
        // Short horizontal dash centered in the box.
        const int dash_inset = scale.logical_to_pixels(kDashInsetLogical);
        const int dash_h = scale.logical_to_pixels(kDashHeightLogical);
        const int dash_y = box.top + (box_size - dash_h) / 2;
        const RECT dash{
            box.left + dash_inset,
            dash_y,
            box.right - dash_inset,
            dash_y + dash_h,
        };
        fill_rect(dc, dash, enabled ? p.accent_text : p.text_secondary);
    }
}

} // namespace nfui