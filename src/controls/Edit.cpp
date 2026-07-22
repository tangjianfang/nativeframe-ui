#include <nfui/Controls/Edit.hpp>

#include <nfui/Paint.hpp>

namespace nfui {
namespace {

constexpr UINT ocm_base = WM_USER + 0x1c00;

const ThemePalette& effective_palette(const ThemePalette* injected) noexcept {
    static const ThemePalette fallback = theme_palette(ThemeMode::light);
    return injected ? *injected : fallback;
}

} // namespace

bool Edit::create(const ControlCreateParams& params) noexcept {
    if (!create_native(L"EDIT", params, WS_BORDER | ES_LEFT | ES_AUTOHSCROLL)) {
        return false;
    }
    if (SetWindowSubclass(hwnd(), &Edit::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    return true;
}

void Edit::on_palette_changed() noexcept {
    // The edit client is still native so its caret and selection behavior remain
    // keyboard/accessibility compatible. Repaint the non-client frame as well as
    // the client so an injected theme never leaves the old border behind.
    RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
}

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
    const ThemePalette& p = effective_palette(palette());
    const bool enabled = IsWindowEnabled(hwnd()) != FALSE;
    const bool focused = GetFocus() == hwnd();
    const Color border = !enabled
        ? alpha_blend(p.border, p.background, 0.55f)
        : (focused ? p.accent_hover : p.border);
    SetDCBrushColor(dc, border.rgb);
    FrameRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    ReleaseDC(hwnd(), dc);
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
        // Native EDIT controls ask their parent for these colors. The Window
        // reflection path sends the request back here, allowing the native edit
        // renderer to retain caret/selection behavior while adopting the palette.
        HDC dc = reinterpret_cast<HDC>(wparam);
        const ThemePalette& p = effective_palette(edit->palette());
        const bool enabled = IsWindowEnabled(hwnd) != FALSE;
        const Color background = enabled
            ? p.surface
            : alpha_blend(p.surface, p.background, 0.55f);
        SetTextColor(dc, (enabled ? p.text : p.text_secondary).rgb);
        SetBkColor(dc, background.rgb);
        SetBkMode(dc, OPAQUE);
        SetDCBrushColor(dc, background.rgb);
        return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
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
