#include <nfui/Controls/StatusBar.hpp>
#include <nfui/Paint.hpp>

#include <commctrl.h>

namespace nfui {
bool StatusBar::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams status_params = params;
    status_params.style = WS_CHILD | WS_VISIBLE;
    return create_native(STATUSCLASSNAMEW, status_params, SBARS_SIZEGRIP);
}

void StatusBar::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    fill_rect(dc, state.bounds, style_.surface_brush.value_or(p.surface));
}
} // namespace nfui
