#include <nfui/Controls/ComboBox.hpp>

#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>
#include <nfui/VectorIcon.hpp>

#include <algorithm>

namespace nfui {
namespace {

constexpr UINT ocm_base = WM_USER + 0x1c00;

const ThemePalette& effective_palette(const ThemePalette* injected) noexcept {
    static const ThemePalette fallback = theme_palette(ThemeMode::light);
    return injected ? *injected : fallback;
}

} // namespace

bool ComboBox::create(const ControlCreateParams& params) noexcept {
    // Owner-draw applies to both the closed selection field and the transient
    // ComboLBox popup without installing a fragile hook on the popup HWND.
    if (!create_native(L"COMBOBOX", params,
                       CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL)) {
        return false;
    }
    const int row_height = font_pixel_height(font_pt::ui, dpi_of(hwnd())) + 8;
    SendMessageW(hwnd(), CB_SETITEMHEIGHT, 0, row_height);
    if (SetWindowSubclass(hwnd(), &ComboBox::visual_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    return true;
}

void ComboBox::on_palette_changed() noexcept {
    RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
    COMBOBOXINFO info{sizeof(info)};
    if (GetComboBoxInfo(hwnd(), &info) != FALSE && info.hwndList != nullptr) {
        InvalidateRect(info.hwndList, nullptr, FALSE);
    }
}

void ComboBox::draw_item(DRAWITEMSTRUCT* draw_info) noexcept {
    if (draw_info == nullptr || draw_info->hDC == nullptr) {
        return;
    }

    const ThemePalette& p = effective_palette(palette());
    const bool disabled = (draw_info->itemState & ODS_DISABLED) != 0;
    const bool selection_field = (draw_info->itemState & ODS_COMBOBOXEDIT) != 0;
    const bool selected = !selection_field && (draw_info->itemState & ODS_SELECTED) != 0;
    const Color background = !disabled && selected ? p.selection : p.surface;
    const Color foreground = disabled
        ? p.text_secondary
        : (selected ? p.selection_text : p.text);
    fill_rect(draw_info->hDC, draw_info->rcItem, background);

    if (draw_info->itemID == static_cast<UINT>(-1)) {
        return;
    }

    const LRESULT length = SendMessageW(hwnd(), CB_GETLBTEXTLEN,
                                        draw_info->itemID, 0);
    if (length == CB_ERR || length >= 255) {
        return;
    }
    wchar_t text[256]{};
    if (SendMessageW(hwnd(), CB_GETLBTEXT, draw_info->itemID,
                     reinterpret_cast<LPARAM>(text)) == CB_ERR) {
        return;
    }

    RECT text_bounds = draw_info->rcItem;
    text_bounds.left += 8;
    text_bounds.right = std::max(text_bounds.left, text_bounds.right - 8);
    HFONT font = fonts() ? fonts()->regular(dpi_of(hwnd()), font_pt::ui) : nullptr;
    draw_text(draw_info->hDC, text_bounds, text, font, foreground,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void ComboBox::paint_chrome() noexcept {
    if (!valid()) {
        return;
    }

    const ThemePalette& p = effective_palette(palette());
    const bool enabled = IsWindowEnabled(hwnd()) != FALSE;
    const bool focused = GetFocus() == hwnd();
    const bool dropped = SendMessageW(hwnd(), CB_GETDROPPEDSTATE, 0, 0) != FALSE;
    const Color border = !enabled
        ? alpha_blend(p.border, p.background, 0.55f)
        : (focused ? p.accent_hover : p.border);

    HDC window_dc = GetWindowDC(hwnd());
    if (window_dc != nullptr) {
        RECT bounds{};
        GetWindowRect(hwnd(), &bounds);
        OffsetRect(&bounds, -bounds.left, -bounds.top);
        SetDCBrushColor(window_dc, border.rgb);
        FrameRect(window_dc, &bounds,
                  static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        ReleaseDC(hwnd(), window_dc);
    }

    HDC dc = GetDC(hwnd());
    if (dc == nullptr) {
        return;
    }
    COMBOBOXINFO info{sizeof(info)};
    if (GetComboBoxInfo(hwnd(), &info) != FALSE) {
        RECT button = info.rcButton;
        RECT client{};
        GetClientRect(hwnd(), &client);
        IntersectRect(&button, &button, &client);
        const Color button_fill = !enabled
            ? alpha_blend(p.surface, p.background, 0.55f)
            : (dropped ? p.surface_hover : p.surface);
        fill_rect(dc, button, button_fill);

        const int center_x = (button.left + button.right) / 2;
        const int center_y = (button.top + button.bottom) / 2;
        // CP18: draw the dropdown chevron through the vector icon system
        // instead of a hand-rolled triangle. A 16-logical-px cell matches the
        // icon size used by Button, so the chevron reads consistent across
        // the whole chrome and at any DPI.
        const DpiScale scale(dpi_of(hwnd()));
        const int cell = scale.logical_to_pixels(16);
        const RECT glyph{
            center_x - cell / 2,
            center_y - cell / 2,
            center_x - cell / 2 + cell,
            center_y - cell / 2 + cell,
        };
        const Color arrow_color = enabled ? p.text_secondary :
            alpha_blend(p.text_secondary, p.background, 0.55f);
        draw_vector_icon(dc, IconKind::chevron_down, glyph, arrow_color, 1);
    }
    ReleaseDC(hwnd(), dc);
}

LRESULT CALLBACK ComboBox::visual_subclass_proc(HWND hwnd,
                                                 UINT message,
                                                 WPARAM wparam,
                                                 LPARAM lparam,
                                                 UINT_PTR subclass_id,
                                                 DWORD_PTR ref_data) noexcept {
    auto* combo = reinterpret_cast<ComboBox*>(ref_data);
    if (combo == nullptr) {
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }

    switch (message) {
    case ocm_base + WM_DRAWITEM: {
        auto* draw_info = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
        if (draw_info != nullptr && draw_info->CtlType == ODT_COMBOBOX) {
            combo->draw_item(draw_info);
            return TRUE;
        }
        break;
    }
    case ocm_base + WM_MEASUREITEM: {
        auto* measure_info = reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
        if (measure_info != nullptr && measure_info->CtlType == ODT_COMBOBOX) {
            measure_info->itemHeight = static_cast<UINT>(
                font_pixel_height(font_pt::ui, dpi_of(hwnd)) + 8);
            return TRUE;
        }
        break;
    }
    case ocm_base + WM_COMMAND: {
        LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        const UINT notification = HIWORD(wparam);
        if (notification == CBN_DROPDOWN || notification == CBN_CLOSEUP) {
            RedrawWindow(hwnd, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
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
    case WM_PAINT:
    case WM_NCPAINT: {
        LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        combo->paint_chrome();
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
        RemoveWindowSubclass(hwnd, &ComboBox::visual_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }

    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui
