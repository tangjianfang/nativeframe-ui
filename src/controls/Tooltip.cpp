#include <nfui/Controls/Tooltip.hpp>

#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <cstdio>
#include <commctrl.h>

namespace nfui {
namespace {

// CP42: layout metrics in logical px, scaled at paint/measure time.
constexpr int tip_pad_logical = 6;      // text inset from the border
constexpr int tip_border_logical = 1;   // hairline border width
constexpr int tip_radius_logical = 4;   // rounded corner radius
constexpr int tip_max_chars = 1024;     // TTM_GETTEXTW scratch buffer

} // namespace

bool Tooltip::create(const ControlCreateParams& params) noexcept {
    // CP19: optional balloon style. TTS_BALLOON is a window style, so it must be
    // present at creation (set_style() after create cannot reshape the window).
    // The chrome-text/bg colour APIs still apply on top of the balloon shape.
    const DWORD extra = style_.balloon.value_or(false) ? TTS_BALLOON : 0;
    if (!create_native(TOOLTIPS_CLASSW, params, extra)) {
        return false;
    }
    // CP42: a second subclass installed on top of the base Control entry takes
    // over WM_PAINT for the self-paint path and keeps the window sized to the
    // text measured in the shared UI font. Messages the path does not own fall
    // through to the base subclass and then to ComCtl32 untouched.
    if (SetWindowSubclass(hwnd(), &Tooltip::paint_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
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

bool Tooltip::balloon_mode() const noexcept {
    if (hwnd() == nullptr) {
        return style_.balloon.value_or(false);
    }
    // The authoritative answer is the live window style: TTS_BALLOON can only
    // be set at create time, but reading GWL_STYLE keeps the check correct
    // even if a future caller toggles the style bit directly.
    return (GetWindowLongW(hwnd(), GWL_STYLE) & TTS_BALLOON) != 0;
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
    // CP42: a palette swap can arrive with a freshly injected FontCache (the
    // inject_theme order is set_palette → set_font_cache); re-measure on the
    // next show via WM_WINDOWPOSCHANGED, and invalidate for the visible case.
    InvalidateRect(hwnd(), nullptr, FALSE);
}

// CP42: measure the tip text in the shared UI font and size the window to fit.
// ComCtl32 sizes the window from its own internal font when TTM_UPDATETIPTEXT
// runs, which under dark mode / high DPI leaves metrics that disagree with the
// rest of the UI; owning the size here keeps the self-paint padding exact.
void Tooltip::resize_to_text() noexcept {
    if (hwnd() == nullptr || balloon_mode()) {
        return;
    }

    wchar_t text[tip_max_chars]{};
    SendMessageW(hwnd(), TTM_GETTEXTW, tip_max_chars,
                 reinterpret_cast<LPARAM>(text));
    if (text[0] == L'\0') {
        return;
    }

    const UINT dpi = dpi_of(hwnd());
    const DpiScale scale(dpi);
    const int pad = scale.logical_to_pixels(tip_pad_logical);
    const int border = scale.logical_to_pixels(tip_border_logical);
    const int max_tip_width = static_cast<int>(
        SendMessageW(hwnd(), TTM_GETMAXTIPWIDTH, 0, 0));
    const bool wrap = max_tip_width > 0;

    HDC screen_dc = GetDC(nullptr);
    if (screen_dc == nullptr) {
        return;
    }
    HFONT font = fonts() ? fonts()->regular(dpi, font_pt::ui) : nullptr;
    HGDIOBJ old_font = font != nullptr ? SelectObject(screen_dc, font) : nullptr;
    RECT measure{0, 0, wrap ? max_tip_width : 0x7FFF, 0};
    DrawTextW(screen_dc, text, -1, &measure,
              DT_CALCRECT | DT_NOPREFIX
                  | (wrap ? DT_WORDBREAK : DT_SINGLELINE));
    if (old_font != nullptr) {
        SelectObject(screen_dc, old_font);
    }
    ReleaseDC(nullptr, screen_dc);

    const int width = (measure.right - measure.left) + 2 * (pad + border);
    const int height = (measure.bottom - measure.top) + 2 * (pad + border);
    if (width <= 0 || height <= 0) {
        return;
    }

    RECT current{};
    GetWindowRect(hwnd(), &current);
    if (current.right - current.left == width && current.bottom - current.top == height) {
        return; // already sized — guards WM_WINDOWPOSCHANGED recursion
    }
    SetWindowPos(hwnd(), nullptr, 0, 0, width, height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// CP42: the full tip chrome from the injected palette. Rounded surface fill,
// hairline border, palette text — identical tokens to the rest of the self
// -painted chrome so tips read as part of the application, not the OS.
void Tooltip::paint_tip(HDC dc, const RECT& bounds) noexcept {
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const Color bg = style_.chrome_bg.value_or(p.surface);
    const Color fg = style_.chrome_text.value_or(p.text);
    const Color border = style_.accent.value_or(p.border);

    const UINT dpi = dpi_of(hwnd());
    const DpiScale scale(dpi);
    const int pad = scale.logical_to_pixels(tip_pad_logical);
    const int radius = scale.logical_to_pixels(tip_radius_logical);

    fill_rounded_rect(dc, bounds, radius, bg, border);

    wchar_t text[tip_max_chars]{};
    SendMessageW(hwnd(), TTM_GETTEXTW, tip_max_chars,
                 reinterpret_cast<LPARAM>(text));
    if (text[0] == L'\0') {
        return;
    }

    const int max_tip_width = static_cast<int>(
        SendMessageW(hwnd(), TTM_GETMAXTIPWIDTH, 0, 0));
    const bool wrap = max_tip_width > 0;
    RECT text_bounds{
        bounds.left + pad,
        bounds.top + pad,
        bounds.right - pad,
        bounds.bottom - pad,
    };
    if (text_bounds.right <= text_bounds.left || text_bounds.bottom <= text_bounds.top) {
        return;
    }
    HFONT font = fonts() ? fonts()->regular(dpi, font_pt::ui) : nullptr;
    draw_text(dc, text_bounds, text, font, fg,
              DT_LEFT | DT_TOP | DT_NOPREFIX
                  | (wrap ? DT_WORDBREAK : DT_SINGLELINE));
}

LRESULT CALLBACK Tooltip::paint_subclass_proc(HWND hwnd,
                                              UINT message,
                                              WPARAM wparam,
                                              LPARAM lparam,
                                              UINT_PTR subclass_id,
                                              DWORD_PTR ref_data) noexcept {
    auto* tip = reinterpret_cast<Tooltip*>(ref_data);
    if (tip == nullptr) {
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }

    switch (message) {
    case WM_PAINT: {
        if (tip->balloon_mode()) {
            break; // native balloon paint keeps the tail shape
        }
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        if (dc != nullptr) {
            RECT bounds{};
            GetClientRect(hwnd, &bounds);
            tip->paint_tip(dc, bounds);
            EndPaint(hwnd, &ps);
        }
        return 0; // fully replaced the native paint
    }
    case TTM_ADDTOOLW:
    case TTM_SETTOOLINFOW:
    case TTM_UPDATETIPTEXTW:
    case TTM_SETMAXTIPWIDTH: {
        // Chain first so ComCtl32 stores the new text / wrap width, then
        // re-measure with the shared UI font.
        const LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        tip->resize_to_text();
        return result;
    }
    case WM_WINDOWPOSCHANGED: {
        const LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        // ComCtl32 repositions the tip before each show; a monitor DPI
        // transition changes the metrics, so re-measure at the new position.
        if (IsWindowVisible(hwnd) != FALSE) {
            tip->resize_to_text();
        }
        return result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &Tooltip::paint_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }

    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui
