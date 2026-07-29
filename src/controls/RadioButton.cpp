#include <nfui/Controls/RadioButton.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>
#include <nfui/ThemeBroker.hpp>

namespace nfui {

bool RadioButton::create(const ControlCreateParams& params) noexcept {
    // CP31: BS_OWNERDRAW routes all painting through on_paint via the reflected
    // WM_DRAWITEM path. BS_AUTORADIOBUTTON cannot be ORed with BS_OWNERDRAW
    // because the button type bits overlap, so the selected state is managed
    // manually in the subclass-proc overrides (BM_GETCHECK / BM_SETCHECK /
    // click / space).
    if (!create_native(L"BUTTON", params, BS_OWNERDRAW)) {
        return false;
    }
    // CP-A2: disable comctl32 visual theming so the native button does not
    // double-paint its dark/HC chrome over our state_palette() chrome.
    theme_disable_window_theme(hwnd());
    return true;
}

void RadioButton::set_checked(bool checked) noexcept {
    if (checked_ == checked) return;
    checked_ = checked;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

LRESULT RadioButton::on_reflected_get_check() const noexcept {
    return checked_ ? BST_CHECKED : BST_UNCHECKED;
}

void RadioButton::on_reflected_set_check(LPARAM state) noexcept {
    set_checked(state == BST_CHECKED);
}

void RadioButton::on_subclass_lbutton_up() noexcept {
    set_checked(true);
}

void RadioButton::on_subclass_key_down(UINT vk, LPARAM) noexcept {
    if (vk == VK_SPACE) {
        set_checked(true);
    }
}

// CP-A2: full state matrix for the radio button. Mirrors CheckBox: borders,
// ring fill, and the inner dot all flow from `state_palette()` so a single
// StatePalette drives every visual variant. The focus ring draws from the
// resolved state's accent colour; disabled fades to text_secondary so the
// control never pretends to be interactive.
void RadioButton::on_paint(HDC dc, const PaintState& state) noexcept {
    HWND w = hwnd();
    if (dc == nullptr || w == nullptr) return;

    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    // Resolve the canonical ControlState and use state_palette() for every
    // colour so the chrome matches Edit / ComboBox / CheckBox exactly.
    const ControlState cs = visual_state();
    const StatePalette sp = state_palette(p, cs);
    const DpiScale scale(dpi_of(w));

    constexpr int kCircleLogical = 16;
    constexpr int kGapLogical = 8;
    constexpr int kDotInsetLogical = 4;
    constexpr int kRingWidthLogical = 1;
    const int circle_size = scale.logical_to_pixels(kCircleLogical);
    const int gap = scale.logical_to_pixels(kGapLogical);
    const int dot_inset = scale.logical_to_pixels(kDotInsetLogical);
    const int ring_width = scale.logical_to_pixels(kRingWidthLogical);

    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;

    fill_rect(target, paint_bounds, p.background);

    // The circular indicator sits at the left edge, vertically centered.
    const int circle_y = paint_bounds.top
        + (paint_bounds.bottom - paint_bounds.top - circle_size) / 2;
    const RECT circle{
        paint_bounds.left,
        circle_y,
        paint_bounds.left + circle_size,
        circle_y + circle_size,
    };

    const bool enabled = state.enabled;
    const bool selected = checked_;

    // Outer ring: a stroked circle in the resolved state's border colour.
    // Use at least 1 device px to avoid disappearing on very small rects.
    draw_ellipse(target, circle, sp.border,
                 ring_width < 1 ? 1 : ring_width);

    if (selected) {
        // Inner dot, inset evenly from the ring.
        RECT dot{
            circle.left + dot_inset,
            circle.top + dot_inset,
            circle.right - dot_inset,
            circle.bottom - dot_inset,
        };
        if (dot.right > dot.left && dot.bottom > dot.top) {
            // Filled with the resolved state's accent (hover/pressed/focused
            // already account for themselves in state_palette()).
            fill_ellipse(target, dot, enabled ? sp.accent : p.text_secondary);
        }
    }

    // Caption label to the right of the indicator.
    FontCache* cache = fonts();
    HFONT font = nullptr;
    if (cache != nullptr) {
        font = cache->regular(scale.dpi(), font_pt::ui);
    }
    const Color text_color = (cs == ControlState::disabled) ? p.text_secondary : p.text;
    RECT text_bounds{
        circle.right + gap,
        paint_bounds.top,
        paint_bounds.right,
        paint_bounds.bottom,
    };
    if (text_bounds.left < text_bounds.right) {
        draw_text(target, text_bounds, caption(), font, text_color,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    if (cs == ControlState::focused) {
        paint_focus_border(target, paint_bounds, sp.accent, 2);
    }
}

} // namespace nfui