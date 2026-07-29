#include <nfui/Controls/Edit.hpp>

#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/ThemeBroker.hpp>

#include <algorithm>

namespace nfui {
namespace {

constexpr UINT ocm_base = WM_USER + 0x1c00;

// Resolve the effective palette. Control::palette() returns the caller's
// injected palette pointer (may be null for an unstyled control). We keep the
// caller-provided pointer when set, falling back to the light palette only so
// paint always has a colour to fill — the WCAG concern is the modes that
// actually get displayed, not this fallback path.
const ThemePalette& effective_palette(const ThemePalette* injected) noexcept {
    static const ThemePalette fallback = theme_palette(ThemeMode::light);
    return injected ? *injected : fallback;
}

// CP-A2: native Edit paints its background via WM_CTLCOLOREDIT reflection.
// `background` is the literal cell background the text renderer composites
// over (matches `state_palette().background`); `foreground` swaps to
// text_secondary when disabled so a disabled field never reads as interactive.
struct EditColorPair { Color background; Color foreground; };

EditColorPair edit_colors_for_state(const ThemePalette& base,
                                     ControlState state) noexcept {
    const StatePalette sp = state_palette(base, ThemeMode::light, state);
    return { sp.background, sp.foreground };
}

} // namespace

bool Edit::create(const ControlCreateParams& params) noexcept {
    if (!create_native(L"EDIT", params, WS_BORDER | ES_LEFT | ES_AUTOHSCROLL)) {
        return false;
    }
    // CP-A2: disable comctl32's dark-mode visual style so the native chrome
    // (the field's content background + selection) does not double-paint over
    // our state_palette() chrome on Windows 10/11 dark mode.
    theme_disable_window_theme(hwnd());

    if (SetWindowSubclass(hwnd(), &Edit::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    return true;
}

void Edit::set_placeholder(std::wstring_view text) noexcept {
    placeholder_.assign(text.data(), text.size());
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void Edit::on_palette_changed() noexcept {
    // The edit client is still native so its caret and selection behavior remain
    // keyboard/accessibility compatible. Repaint the non-client frame as well as
    // the client so an injected theme never leaves the old border behind.
    RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
}

// CP-A2: border paint. Uses state_palette() so every one of the six states
// (rest/hover/pressed/focused/disabled/error) is expressed by a single helper
// rather than six hand-rolled paths in the subclass proc. Focused gets a 2px
// accent border (the focus ring); every other state gets a 1px border in the
// state-resolved colour.
void Edit::paint_border() noexcept {
    if (!valid()) {
        return;
    }

    HDC dc = GetWindowDC(hwnd());
    if (dc == nullptr) {
        return;
    }

    RECT bounds{};
    GetWindowRect(hwnd(), &bounds);
    OffsetRect(&bounds, -bounds.left, -bounds.top);
    const ThemePalette& base = effective_palette(palette());
    const ControlState state = visual_state();
    const StatePalette sp = state_palette(base, ThemeMode::light, state);
    const bool focused = (state == ControlState::focused);
    const int width = focused ? 2 : 1;
    paint_focus_border(dc, bounds, sp.border, width);
    ReleaseDC(hwnd(), dc);
}

// CP-A2: placeholder text drawn in the field's content area. Native Edit draws
// the caret + selection at the client origin; we paint the placeholder first
// so it lives underneath native content (rare in practice: a placeholder is
// only shown when the field is empty, in which case there is no caret or
// selection to compete with). Text is drawn DT_LEFT with an 8-logical-px
// padding so it sits flush with the caret position a typed value would have.
void Edit::paint_placeholder(HDC dc) noexcept {
    if (placeholder_.empty()) return;
    if (GetWindowTextLengthW(hwnd()) > 0) return;

    const ThemePalette& base = effective_palette(palette());
    const ControlState state = visual_state();
    const StatePalette sp = state_palette(base, ThemeMode::light, state);

    RECT client{};
    GetClientRect(hwnd(), &client);
    const DpiScale scale(dpi_of(hwnd()));
    const int pad = scale.logical_to_pixels(8);
    RECT text_bounds{
        client.left + pad,
        client.top,
        std::max(client.left + pad, client.right - pad),
        client.bottom,
    };
    HFONT font = fonts() ? fonts()->regular(scale.dpi(), font_pt::ui) : nullptr;
    draw_text(dc, text_bounds, placeholder_, font, sp.foreground,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

LRESULT CALLBACK Edit::visual_subclass_proc(HWND hwnd,
                                             UINT message,
                                             WPARAM wparam,
                                             LPARAM lparam,
                                             UINT_PTR subclass_id,
                                             DWORD_PTR ref_data) noexcept {
    auto* edit = reinterpret_cast<Edit*>(ref_data);
    if (edit == nullptr) {
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }

    switch (message) {
    case ocm_base + WM_CTLCOLOREDIT:
    case ocm_base + WM_CTLCOLORSTATIC: {
        // Native EDIT controls ask their parent for these colours. The Window
        // reflection path sends the request back here, allowing the native
        // edit renderer to retain caret/selection behaviour while adopting the
        // palette. We read `visual_state()` here rather than caching — the
        // focused/hover/disabled bits flip on every focus/mouse event and we
        // want each new WM_CTLCOLOREDIT to reflect the current state.
        HDC dc = reinterpret_cast<HDC>(wparam);
        const ThemePalette& base = effective_palette(edit->palette());
        const ControlState state = edit->visual_state();
        const EditColorPair pair = edit_colors_for_state(base, state);
        SetTextColor(dc, pair.foreground.rgb);
        SetBkColor(dc, pair.background.rgb);
        SetBkMode(dc, OPAQUE);
        SetDCBrushColor(dc, pair.background.rgb);
        return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
    }
    case WM_PAINT: {
        // CP-A2: paint the placeholder BEFORE forwarding to DefSubclassProc
        // so native text rendering composites over our placeholder (rare;
        // placeholder only shows when the field is empty). The native path
        // also handles caret + selection — we never replace it.
        LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        HDC dc = GetDC(hwnd);
        if (dc != nullptr) {
            edit->paint_placeholder(dc);
            ReleaseDC(hwnd, dc);
        }
        return result;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE: {
        LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        RedrawWindow(hwnd, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
        return result;
    }
    case WM_NCPAINT: {
        LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        edit->paint_border();
        return result;
    }
    case WM_THEMECHANGED:
    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE: {
        // System theme notifications do not carry the application's injected
        // palette. Chain first so native controls refresh their own theme data,
        // then invalidate so custom paint and native chrome converge on the
        // same message-loop turn.
        LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
        return result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &Edit::visual_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }

    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui