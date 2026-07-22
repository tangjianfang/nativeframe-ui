#include <nfui/Controls/Tooltip.hpp>
#include <cstdio>
#include <commctrl.h>

namespace nfui {
bool Tooltip::create(const ControlCreateParams& params) noexcept {
    if (!create_native(TOOLTIPS_CLASSW, params, 0)) {
        return false;
    }
    // P1.4: pull theme text/background through ComCtl32's documented tooltip
    // colour APIs (TTM_SETTIPTEXTCOLOR + TTM_SETTIPBKCOLOR). These work on
    // themed ComCtl32 v6, unlike the tab-control equivalents. The colour is
    // a COLORREF in **wParam** (lParam is reserved and must be zero) — a
    // long-standing bug in this file passed the colour via lParam, which
    // silently failed and left the native tooltip palette in place
    // (TTM_GETTIPTEXTCOLOR / TTM_GETTIPBKCOLOR round-tripped 0).
    //
    // Colours are also applied when the owner supplied explicit chrome_text /
    // chrome_bg overrides even without a palette — an explicit override should
    // always take effect, falling back to the light palette for the other
    // channel so text never collides with the background.
    on_palette_changed();
    return true;
}

void Tooltip::on_palette_changed() noexcept {
    if (hwnd() == nullptr) {
        return;
    }
    const ThemePalette* pal = palette();
    if (pal == nullptr && !style_.chrome_text.has_value() && !style_.chrome_bg.has_value()) {
        return;
    }
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const Color fg = style_.chrome_text.value_or(p.text);
    const Color bg = style_.chrome_bg.value_or(p.surface);
    SendMessageW(hwnd(), TTM_SETTIPTEXTCOLOR, fg.rgb, 0);
    SendMessageW(hwnd(), TTM_SETTIPBKCOLOR, bg.rgb, 0);
#ifdef _DEBUG
    // CP6: round-trip the values via TTM_GETTIPTEXTCOLOR / TTM_GETTIPBKCOLOR
    // so a regression that drops the colour (e.g. swapping wParam/lParam back)
    // surfaces immediately during development instead of silently regressing.
    const COLORREF got_fg = static_cast<COLORREF>(
        SendMessageW(hwnd(), TTM_GETTIPTEXTCOLOR, 0, 0));
    const COLORREF got_bg = static_cast<COLORREF>(
        SendMessageW(hwnd(), TTM_GETTIPBKCOLOR, 0, 0));
    if (got_fg != fg.rgb || got_bg != bg.rgb) {
        wchar_t buf[160];
        std::swprintf(buf, std::size(buf),
                      L"[nfui::Tooltip] colour round-trip mismatch: "
                      L"sent fg=0x%08X bg=0x%08X, got fg=0x%08X bg=0x%08X\n",
                      static_cast<unsigned>(fg.rgb), static_cast<unsigned>(bg.rgb),
                      static_cast<unsigned>(got_fg), static_cast<unsigned>(got_bg));
        OutputDebugStringW(buf);
    }
#endif
}
} // namespace nfui
