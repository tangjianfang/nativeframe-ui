#include <nfui/Controls/Splitter.hpp>
#include <nfui/Paint.hpp>

namespace nfui {
bool Splitter::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams splitter_params = params;
    // P1.4: drop SS_GRAYRECT; Splitter paints itself in palette.border (or the
    // consumer-supplied surface_brush). WS_CLIPCHILDREN keeps neighbour paint
    // from leaving streaks when the splitter is dragged.
    splitter_params.style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    return create_native(L"STATIC", splitter_params, 0);
}
void Splitter::set_ratio(double ratio) noexcept {
    if (ratio < 0.0) ratio_ = 0.0;
    else if (ratio > 1.0) ratio_ = 1.0;
    else ratio_ = ratio;
}
double Splitter::ratio() const noexcept { return ratio_; }

void Splitter::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const Color fill = style_.surface_brush.value_or(p.border);
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;
    fill_rect(target, paint_bounds, fill);
}
} // namespace nfui
