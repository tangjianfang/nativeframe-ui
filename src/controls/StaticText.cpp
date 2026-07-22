#include <nfui/Controls/StaticText.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

namespace nfui {

bool StaticText::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams static_params = params;
    static_params.style &= ~WS_TABSTOP;
    return create_native(L"STATIC", static_params, SS_LEFT);
}

void StaticText::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const Color bg = style_.background.value_or(p.background);
    const Color fg = state.enabled ? style_.foreground.value_or(p.text) : p.text_secondary;
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;
    fill_rect(target, paint_bounds, bg);
    FontCache* cache = fonts();
    HFONT font = nullptr;
    if (cache != nullptr) {
        const int dpi = dpi_of(hwnd());
        font = style_.use_semibold.value_or(false)
            ? cache->semibold(dpi, font_pt::ui)
            : cache->regular(dpi, font_pt::ui);
    }
    draw_text(target, paint_bounds, caption(), font, fg, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

} // namespace nfui
