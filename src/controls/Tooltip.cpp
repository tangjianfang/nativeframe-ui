#include <nfui/Controls/Tooltip.hpp>
#include <commctrl.h>

namespace nfui {
bool Tooltip::create(const ControlCreateParams& params) noexcept {
    if (!create_native(TOOLTIPS_CLASSW, params, 0)) {
        return false;
    }
    // P1.4: pull theme text/background through ComCtl32's documented tooltip
    // colour APIs (TTM_SETTIPTEXTCOLOR + TTM_SETTIPBKCOLOR). These work on
    // themed ComCtl32 v6, unlike the tab-control equivalents. Apply only if a
    // palette was injected BEFORE create (the standard owner pattern); tooling
    // that wires a palette after create must refresh manually.
    //
    // Colours are also applied when the owner supplied explicit chrome_text /
    // chrome_bg overrides even without a palette — an explicit override should
    // always take effect, falling back to the light palette for the other
    // channel so text never collides with the background.
    const ThemePalette* pal = palette();
    if (pal != nullptr || style_.chrome_text.has_value() || style_.chrome_bg.has_value()) {
        const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
        const Color fg = style_.chrome_text.value_or(p.text);
        const Color bg = style_.chrome_bg.value_or(p.surface);
        SendMessageW(hwnd(), TTM_SETTIPTEXTCOLOR, 0, fg.rgb);
        SendMessageW(hwnd(), TTM_SETTIPBKCOLOR, 0, bg.rgb);
    }
    return true;
}
} // namespace nfui
