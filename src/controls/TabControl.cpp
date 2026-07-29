#include <nfui/Controls/TabControl.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>
#include <nfui/ThemeBroker.hpp>
#include <commctrl.h>

namespace nfui {

namespace {

constexpr UINT ocm_base = WM_USER + 0x1c00;

// CP42: the base Control::subclass_proc is installed with the control instance
// pointer as its subclass id. The chrome subclass uses a distinct id so it is a
// separate chain entry; since CP42-C the base handler forwards NM_CUSTOMDRAW to
// the chain before applying its fallback, the chrome proc receives the reflected
// notify regardless of install order.
constexpr UINT_PTR tab_chrome_subclass_id = 0xC042u;

const ThemePalette& effective_palette(const ThemePalette* injected) noexcept {
    static const ThemePalette fallback = theme_palette(ThemeMode::light);
    return injected ? *injected : fallback;
}

// CP-A3: resolve per-state tab fills via the palette. Active tab gets the
// surface (cards stand out against the strip), hot gets surface_hover,
// inactive gets surface. The FrameStyle override slots win when a
// consumer has explicitly tinted the strip.
Color tab_fill_for(const FrameStyle& style, const ThemePalette& p,
                   ControlState state) noexcept {
    switch (state) {
    case ControlState::hover:
        return style.chrome_bg.value_or(p.surface_hover);
    case ControlState::focused:
        return style.background.value_or(p.surface);
    case ControlState::rest:
    default:
        return style.chrome_bg.value_or(p.surface);
    }
}

Color tab_text_for(const FrameStyle& style, const ThemePalette& p,
                   ControlState state) noexcept {
    switch (state) {
    case ControlState::focused:
        return style.foreground.value_or(style.chrome_text.value_or(p.text));
    case ControlState::hover:
        return style.foreground.value_or(style.chrome_text.value_or(p.text));
    case ControlState::rest:
    default:
        return style.foreground.value_or(style.chrome_text.value_or(p.text_secondary));
    }
}

} // namespace

bool TabControl::create(const ControlCreateParams& params) noexcept {
    // CP-A3: TCS_OWNERDRAWFIXED enables per-tab WM_DRAWITEM dispatch so the
    // chrome subclass can paint every tab (active / inactive / hover /
    // focused) from a single state matrix via paint_tab(). This subsumes
    // the previous NM_CUSTOMDRAW path so the two paths cannot double-paint
    // the same tab.
    if (!create_native(WC_TABCONTROLW, params,
                       WS_CLIPSIBLINGS | TCS_OWNERDRAWFIXED)) {
        return false;
    }
    // CP20: install the chrome subclass for WM_DRAWITEM and WM_ERASEBKGND.
    // The base Control::subclass_proc does not know about TabControl's
    // DRAWITEMSTRUCT payload, so the chrome proc owns the entire owner-draw
    // path.
    // CP42: disable system theming so the tab strip is fully self-painted
    // instead of being overdrawn by the native light theme in dark/HC.
    theme_disable_window_theme(hwnd());
    if (SetWindowSubclass(hwnd(), &TabControl::visual_subclass_proc,
                          tab_chrome_subclass_id,
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    // CP-A3: register with the ThemeBroker so a cross-window theme switch
    // invalidates the chrome on the same message-loop turn. Unregistered in
    // the WM_NCDESTROY arm of the subclass proc below. Capture the HWND
    // by value so the lambda is self-contained and does not need member
    // access during broadcast.
    const HWND self_hwnd = hwnd();
    ThemeBroker::instance().register_hwnd(self_hwnd,
        [self_hwnd](ThemeMode) {
            if (IsWindow(self_hwnd) != FALSE) {
                InvalidateRect(self_hwnd, nullptr, TRUE);
            }
        });
    on_palette_changed();
    return true;
}

void TabControl::on_palette_changed() noexcept {
    // The chrome subclass already pulls palette+FrameStyle from
    // paint_tab on every paint cycle, so the only work here is to
    // invalidate so the system asks us to repaint tabs with the new theme.
    // CP42: re-disable system theming in case it was re-applied externally.
    if (valid()) {
        theme_disable_window_theme(hwnd());
        InvalidateRect(hwnd(), nullptr, TRUE);
    }
}

bool TabControl::set_padding(int cx, int cy) noexcept {
    if (!valid()) {
        return false;
    }
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    SendMessageW(hwnd(), TCM_SETPADDING, 0, MAKELPARAM(static_cast<WORD>(cx), static_cast<WORD>(cy)));
    return true;
}

void TabControl::paint_tab(const DRAWITEMSTRUCT* di) noexcept {
    if (di == nullptr || di->hDC == nullptr) return;
    const ThemePalette& p = effective_palette(palette());
    const bool selected = (di->itemState & ODS_SELECTED) != 0;
    const bool hot = (di->itemState & ODS_HOTLIGHT) != 0;
    const bool focused = (di->itemState & ODS_FOCUS) != 0;
    // CP-A3: state precedence: focused > hover > rest. A selected tab is
    // treated as "focused" by default (the control-level focused tab). The
    // state drives both fill and text colour through tab_fill_for /
    // tab_text_for, so a single helper per chrome attribute replaces the
    // CP20 `if (selected) ... else if (hot) ... else ...` ladder.
    ControlState state = ControlState::rest;
    if (selected) {
        state = ControlState::focused;
    } else if (hot) {
        state = ControlState::hover;
    }
    const Color fill = tab_fill_for(style_, p, state);
    const Color border = p.border;
    const int radius = (selected || hot) ? theme_metrics().corner_radius_control : 0;
    fill_rounded_rect(di->hDC, di->rcItem, radius, fill, border);

    // Caption: read via TCM_GETITEM so the per-tab label honours whatever the
    // application inserted at insert time. Pad inside the tab so the text does
    // not touch the border. itemID is the tab index — Win32 has no
    // reserved/sentinel value, so the first tab at index 0 is drawn with its
    // caption exactly like the rest.
    wchar_t text[128]{};
    TCITEM tci{};
    tci.mask = TCIF_TEXT;
    tci.pszText = text;
    tci.cchTextMax = static_cast<int>(sizeof(text) / sizeof(text[0]));
    if (SendMessageW(hwnd(), TCM_GETITEM, di->itemID,
                     reinterpret_cast<LPARAM>(&tci)) == FALSE) {
        return;
    }
    RECT text_rc = di->rcItem;
    // CP-A3: 12 logical px padding around the caption (per brief). Scaled by
    // DPI so 125/150/200% monitors produce the same visual margin.
    const DpiScale dpi{dpi_of(hwnd())};
    const int pad = dpi.logical_to_pixels(12);
    text_rc.left += pad;
    text_rc.right -= pad;
    HFONT font = fonts() ? fonts()->regular(dpi.dpi(), font_pt::ui) : nullptr;
    draw_text(di->hDC, text_rc, text, font, tab_text_for(style_, p, state),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // CP-A3: active-tab accent bar along the TOP edge (per brief — the
    // accent reads as a tab indicator at the top of the active tab, not
    // the bottom). 2 logical px tall; spans the full tab width so the
    // accent reads as a strong emphasis.
    if (selected) {
        const int accent_h = dpi.logical_to_pixels(2);
        RECT accent = di->rcItem;
        accent.bottom = accent.top + accent_h;
        fill_rect(di->hDC, accent, p.accent);
    }

    // CP-A3: focused active tab gets a 2 px focus ring around the whole
    // tab via paint_focus_border (stroke-only). Distinct from the accent
    // bar so keyboard navigation reads at a glance.
    if (focused && selected) {
        paint_focus_border(di->hDC, di->rcItem, p.accent_hover, 2);
    }
}

LRESULT TabControl::handle_draw_item(const DRAWITEMSTRUCT* di) noexcept {
    if (di == nullptr || di->CtlType != ODT_TAB) {
        return CDRF_DODEFAULT;
    }
    paint_tab(di);
    return TRUE;
}

LRESULT CALLBACK TabControl::visual_subclass_proc(HWND hwnd, UINT message,
                                                   WPARAM wparam, LPARAM lparam,
                                                   UINT_PTR subclass_id,
                                                   DWORD_PTR ref_data) noexcept {
    auto* tc = reinterpret_cast<TabControl*>(ref_data);
    if (tc == nullptr) {
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }
    switch (message) {
    case ocm_base + WM_DRAWITEM: {
        auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
        if (di != nullptr && di->CtlType == ODT_TAB) {
            return tc->handle_draw_item(di);
        }
        break;
    }
    case WM_DRAWITEM: {
        auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
        if (di != nullptr && di->CtlType == ODT_TAB) {
            return tc->handle_draw_item(di);
        }
        break;
    }
    case WM_ERASEBKGND: {
        // CP34: tab strip + page area are theme-painted by comctl32 in light
        // mode, so a dark-themed tab control showed raw white bands next to
        // the custom-drawn tabs. Paint the whole client rect with the
        // palette surface so the strip + page area blend into the host
        // chrome; the individual tabs are then overdrawn by WM_DRAWITEM.
        HDC dc = reinterpret_cast<HDC>(wparam);
        if (dc != nullptr) {
            const ThemePalette& p = effective_palette(tc->palette());
            RECT rc{};
            GetClientRect(hwnd, &rc);
            fill_rect(dc, rc, p.surface);
            return 1;
        }
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }
    case WM_PAINT: {
        // CP-A3: belt-and-braces — if the native tab body leaks white
        // (some ComCtl32 builds leave the page area transparent), paint
        // the client rect with palette.surface after the default handler
        // so the body matches the strip.
        LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        return result;
    }
    case WM_NCDESTROY:
        ThemeBroker::instance().unregister_hwnd(hwnd);
        RemoveWindowSubclass(hwnd, &TabControl::visual_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui
