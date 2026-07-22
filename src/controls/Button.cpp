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
    const Color border = state.focused
        ? style_.border_color.value_or(p.accent_hover)
        : style_.border_color.value_or(p.border);
    Color face = p.accent;
    Color text_color = p.accent_text;
    if (!state.enabled) {
        // WCAG AA: lighten border 12% toward surface for disabled face so
        // text_secondary clears the 4.5:1 contrast threshold. Reference:
        // docs/KNOWLEDGE/polish/2026-07-22-button-disabled-state-color.md.
        face = alpha_blend(p.border, p.surface, 0.12f);
        text_color = p.text_secondary;
    } else if (state.pressed) {
        // Keep pressed visually distinct from hover without adding a palette token.
        face = alpha_blend(p.accent, p.background, 0.82f);
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
