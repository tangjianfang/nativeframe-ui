// NativeFrameUIComponentGallery
//
// V1.0 capability: the canonical "kitchen sink" surface. One tall window that
// renders every control class in its expected state, laid out as labelled
// vertical sections. Useful as a visual smoke test for consumers — if any
// control fails to instantiate or paint, its section will be obviously
// empty or out-of-place.

#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <commctrl.h>
#include <cstddef>
#include <iterator>
#include <windows.h>
#include <windowsx.h>

namespace {

constexpr int id_button_default  = 101;
constexpr int id_button_disabled = 102;
constexpr int id_button_hover    = 103;
constexpr int id_check_unchecked = 104;
constexpr int id_check_checked   = 105;
constexpr int id_check_indeterm  = 106;
constexpr int id_radio_a         = 107;
constexpr int id_radio_b         = 108;
constexpr int id_radio_c         = 109;
constexpr int id_edit            = 110;
constexpr int id_static_left     = 111;
constexpr int id_static_center   = 112;
constexpr int id_static_right    = 113;
constexpr int id_listbox         = 114;
constexpr int id_combobox        = 115;
constexpr int id_listview        = 116;
constexpr int id_treeview        = 117;
constexpr int id_iconview        = 118;
constexpr int id_status          = 119;
constexpr int id_tabs            = 120;
constexpr int id_progress        = 121;
constexpr int id_panel           = 122;
constexpr int id_splitter        = 123;

class GalleryWindow final : public nfui::Window {
public:
    explicit GalleryWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(mode_)) {
    }

    ~GalleryWindow() noexcept override {
        destroy_icon();
    }

    void set_initial_theme(nfui::ThemeMode mode) noexcept {
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIComponentGalleryWindow",
            L"NativeFrame UI ComponentGallery",
            WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            880,
            1320,
        };

        if (!create(params)) {
            return false;
        }

        apply_window_icon();
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        if (!create_children()) {
            return false;
        }
        layout_children();
        apply_native_fonts();

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            layout_children();
            return 0;
        case WM_DPICHANGED: {
            auto* suggested = reinterpret_cast<RECT*>(lparam);
            dpi_ = nfui::DpiScale(HIWORD(wparam));
            if (suggested != nullptr) {
                SetWindowPos(hwnd(),
                             nullptr,
                             suggested->left,
                             suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }
            apply_native_fonts();
            layout_children();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC hdc = BeginPaint(hwnd(), &paint);
            RECT client{};
            GetClientRect(hwnd(), &client);
            {
                nfui::MemoryDC mem(hdc, client);
                HDC target = mem.valid() ? mem.dc() : hdc;
                paint_background(target, client);
            }
            EndPaint(hwnd(), &paint);
            return 0;
        }
        case WM_DESTROY:
            destroy_icon();
            PostQuitMessage(0);
            return 0;
        default:
            return nfui::Window::handle_message(message, wparam, lparam);
        }
    }

private:
    template <typename ControlT>
    [[nodiscard]] bool init(ControlT& control, int id, std::wstring_view text) noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id,
            text,
            0, 0, 100, 28,
        };
        control.inject_theme(&palette_, &fonts_);
        return control.create(params);
    }

    [[nodiscard]] bool create_children() noexcept {
        // Buttons — 3 in a row.
        if (!init(button_default_,  id_button_default,  L"Default"))  return false;
        if (!init(button_disabled_, id_button_disabled, L"Disabled")) return false;
        if (!init(button_hover_,    id_button_hover,    L"Hover"))    return false;
        EnableWindow(button_disabled_.hwnd(), FALSE);

        // CheckBoxes — unchecked / checked / indeterminate.
        if (!init(check_unchecked_, id_check_unchecked, L"Unchecked"))      return false;
        if (!init(check_checked_,   id_check_checked,   L"Checked"))        return false;
        if (!init(check_indeterm_,  id_check_indeterm,  L"Indeterminate"))  return false;
        SendMessageW(check_checked_.hwnd(),  BM_SETCHECK, BST_CHECKED,       0);
        SendMessageW(check_indeterm_.hwnd(), BM_SETCHECK, BST_INDETERMINATE, 0);

        // RadioButtons — first selected.
        if (!init(radio_a_, id_radio_a, L"Option A")) return false;
        if (!init(radio_b_, id_radio_b, L"Option B")) return false;
        if (!init(radio_c_, id_radio_c, L"Option C")) return false;
        SendMessageW(radio_a_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);

        // Edit.
        if (!init(edit_, id_edit, L"editable sample")) return false;

        // StaticText — three alignments. CP8 wired up the new TextStyle
        // alignment fields so the labels actually match the rendered layout.
        if (!init(static_left_,   id_static_left,   L"Left aligned"))   return false;
        if (!init(static_center_, id_static_center, L"Center aligned")) return false;
        if (!init(static_right_,  id_static_right,  L"Right aligned"))  return false;
        nfui::TextStyle center_style{};
        center_style.align_h = nfui::StaticTextAlignH::center;
        static_center_.set_style(center_style);
        nfui::TextStyle right_style{};
        right_style.align_h = nfui::StaticTextAlignH::right;
        static_right_.set_style(right_style);

        // ListBox (5 items, second selected).
        if (!init(listbox_, id_listbox, L"")) return false;
        ListBox_AddString(listbox_.hwnd(), L"One");
        ListBox_AddString(listbox_.hwnd(), L"Two");
        ListBox_AddString(listbox_.hwnd(), L"Three");
        ListBox_AddString(listbox_.hwnd(), L"Four");
        ListBox_AddString(listbox_.hwnd(), L"Five");
        SendMessageW(listbox_.hwnd(), LB_SETCURSEL, 1, 0);

        // ComboBox (5 items in dropdown).
        if (!init(combobox_, id_combobox, L"")) return false;
        ComboBox_AddString(combobox_.hwnd(), L"Red");
        ComboBox_AddString(combobox_.hwnd(), L"Green");
        ComboBox_AddString(combobox_.hwnd(), L"Blue");
        ComboBox_AddString(combobox_.hwnd(), L"Yellow");
        ComboBox_AddString(combobox_.hwnd(), L"Purple");
        // CP32: pick a default selection so the closed combobox isn't visually empty.
        SendMessageW(combobox_.hwnd(), CB_SETCURSEL, 0, 0);

        // ListView (3 columns x 4 rows).
        if (!init(listview_, id_listview, L"")) return false;
        ListView_SetExtendedListViewStyle(listview_.hwnd(),
                                          LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        insert_listview_column(listview_.hwnd(), 0, L"Column A", 120);
        insert_listview_column(listview_.hwnd(), 1, L"Column B", 120);
        insert_listview_column(listview_.hwnd(), 2, L"Column C", 120);
        insert_listview_row(listview_.hwnd(), 0, L"A1", L"B1", L"C1");
        insert_listview_row(listview_.hwnd(), 1, L"A2", L"B2", L"C2");
        insert_listview_row(listview_.hwnd(), 2, L"A3", L"B3", L"C3");
        insert_listview_row(listview_.hwnd(), 3, L"A4", L"B4", L"C4");

        // TreeView (2 levels, root expanded).
        if (!init(treeview_, id_treeview, L"")) return false;
        HTREEITEM root = tree_item_insert(treeview_.hwnd(), TVI_ROOT, TVI_LAST, L"Root");
        tree_item_insert(treeview_.hwnd(), root, TVI_LAST, L"Child A");
        tree_item_insert(treeview_.hwnd(), root, TVI_LAST, L"Child B");
        tree_item_insert(treeview_.hwnd(), root, TVI_LAST, L"Child C");
        TreeView_Expand(treeview_.hwnd(), root, TVE_EXPAND);

        // IconView.
        if (!init(iconview_, id_iconview, L"")) return false;
        if (app_icon_ == nullptr) {
            app_icon_ = nfui::load_scaled_icon(instance_,
                                               MAKEINTRESOURCEW(IDI_NFUI_APP),
                                               32,
                                               dpi_.dpi());
        }
        if (app_icon_ != nullptr) {
            iconview_.set_icon(app_icon_);
        }

        // StatusBar.
        if (!init(status_, id_status, L"")) return false;
        SendMessageW(status_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Ready"));

        // TabControl (3 tabs).
        if (!init(tabs_, id_tabs, L"")) return false;
        // CP8A: widen horizontal padding to match the 12-DIP gallery rhythm.
        static_cast<void>(tabs_.set_padding(dpi_.logical_to_pixels(12), dpi_.logical_to_pixels(4)));
        insert_tab(tabs_.hwnd(), 0, L"Tab 1");
        insert_tab(tabs_.hwnd(), 1, L"Tab 2");
        insert_tab(tabs_.hwnd(), 2, L"Tab 3");

        // ProgressBar (60%).
        if (!init(progress_, id_progress, L"")) return false;
        SendMessageW(progress_.hwnd(), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(progress_.hwnd(), PBM_SETPOS, 60, 0);

        // Panel + Splitter.
        if (!init(panel_,    id_panel,    L"")) return false;
        // CP16: raise the demo Panel to elevation=2 so the gallery visually
        // demonstrates the shadow + surface gradient path. Splitter stays
        // at elevation=0 (its on_paint does not consume the elevation token
        // today and the splitter chrome should read flat).
        nfui::FrameStyle panel_style{};
        panel_style.elevation = 2;
        panel_.set_style(panel_style);
        if (!init(splitter_, id_splitter, L"")) return false;

        return true;
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font = fonts_.regular(dpi_value, 9);
        SendMessageW(listbox_.hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(combobox_.hwnd(),  WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(edit_.hwnd(),      WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(listview_.hwnd(),  WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(treeview_.hwnd(),  WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(tabs_.hwnd(),      WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(status_.hwnd(),    WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
    }

    void layout_children() {
        if (hwnd() == nullptr || status_.hwnd() == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        SendMessageW(status_.hwnd(), WM_SIZE, 0, 0);

        RECT status_rect{};
        GetWindowRect(status_.hwnd(), &status_rect);
        const int status_height = status_rect.bottom - status_rect.top;
        const int outer = dpi_.logical_to_pixels(16);
        const int gap   = dpi_.logical_to_pixels(12);
        const int label_w = dpi_.logical_to_pixels(140);
        const int row_h  = dpi_.logical_to_pixels(28);
        const int tall_h = dpi_.logical_to_pixels(120);
        const int left = client.left + outer;
        const int content_left = left + label_w;

        int y = client.top + outer + dpi_.logical_to_pixels(36);  // leave space for header

        // Place one control inside its section row. Each call advances `y`.
        auto place = [&](int id, int logical_w, int logical_h) {
            HWND h = GetDlgItem(hwnd(), id);
            if (h != nullptr) {
                MoveWindow(h,
                           content_left,
                           y,
                           dpi_.logical_to_pixels(logical_w),
                           dpi_.logical_to_pixels(logical_h),
                           TRUE);
            }
        };

        auto next_row = [&](int logical_h) {
            y += dpi_.logical_to_pixels(logical_h) + gap;
        };

        // Buttons.
        place(id_button_default,  110, 28);
        MoveWindow(GetDlgItem(hwnd(), id_button_disabled),
                   content_left + dpi_.logical_to_pixels(110) + gap,
                   y, dpi_.logical_to_pixels(110), row_h, TRUE);
        MoveWindow(GetDlgItem(hwnd(), id_button_hover),
                   content_left + 2 * (dpi_.logical_to_pixels(110) + gap),
                   y, dpi_.logical_to_pixels(110), row_h, TRUE);
        next_row(28);

        // CheckBoxes.
        place(id_check_unchecked, 120, 28);
        MoveWindow(GetDlgItem(hwnd(), id_check_checked),
                   content_left + dpi_.logical_to_pixels(120) + gap,
                   y, dpi_.logical_to_pixels(120), row_h, TRUE);
        MoveWindow(GetDlgItem(hwnd(), id_check_indeterm),
                   content_left + 2 * (dpi_.logical_to_pixels(120) + gap),
                   y, dpi_.logical_to_pixels(140), row_h, TRUE);
        next_row(28);

        // RadioButtons.
        place(id_radio_a, 100, 28);
        MoveWindow(GetDlgItem(hwnd(), id_radio_b),
                   content_left + dpi_.logical_to_pixels(100) + gap,
                   y, dpi_.logical_to_pixels(100), row_h, TRUE);
        MoveWindow(GetDlgItem(hwnd(), id_radio_c),
                   content_left + 2 * (dpi_.logical_to_pixels(100) + gap),
                   y, dpi_.logical_to_pixels(100), row_h, TRUE);
        next_row(28);

        // Edit.
        place(id_edit, 320, 28);
        next_row(28);

        // StaticText.
        place(id_static_left, 130, 28);
        MoveWindow(GetDlgItem(hwnd(), id_static_center),
                   content_left + dpi_.logical_to_pixels(130) + gap,
                   y, dpi_.logical_to_pixels(130), row_h, TRUE);
        MoveWindow(GetDlgItem(hwnd(), id_static_right),
                   content_left + 2 * (dpi_.logical_to_pixels(130) + gap),
                   y, dpi_.logical_to_pixels(130), row_h, TRUE);
        next_row(28);

        // ListBox.
        place(id_listbox, 200, 120);
        next_row(120);

        // ComboBox.
        place(id_combobox, 240, 28);
        next_row(28);

        // ListView.
        place(id_listview, 380, 120);
        next_row(120);

        // TreeView.
        place(id_treeview, 200, 120);
        next_row(120);

        // IconView.
        place(id_iconview, 32, 32);
        next_row(32);

        // TabControl.
        place(id_tabs, 380, 120);
        next_row(120);

        // ProgressBar.
        place(id_progress, 380, 28);
        next_row(28);

        // Panel + Splitter (V1.4 chrome).
        place(id_panel, 380, 120);
        HWND splitter_h = GetDlgItem(hwnd(), id_splitter);
        if (splitter_h != nullptr) {
            MoveWindow(splitter_h,
                       content_left + dpi_.logical_to_pixels(380) + gap,
                       y,
                       dpi_.logical_to_pixels(6),
                       dpi_.logical_to_pixels(120),
                       TRUE);
        }
        next_row(120);

        (void)label_w; (void)status_height; (void)tall_h; (void)left;  // silence unused
    }

    void paint_background(HDC target, const RECT& client) {
        nfui::fill_rect(target, client, palette_.background);

        const int dpi_value = dpi_.dpi();
        HFONT header_font  = fonts_.serif(dpi_value, 16);
        HFONT section_font = fonts_.semibold(dpi_value, 9);
        HFONT body_font    = fonts_.regular(dpi_value, 9);

        const int outer = dpi_.logical_to_pixels(16);
        const int label_w = dpi_.logical_to_pixels(140);
        const int left = client.left + outer;

        RECT title{left, client.top + outer, client.right - outer, client.top + outer + dpi_.logical_to_pixels(28)};
        nfui::draw_text(target,
                        title,
                        L"NativeFrame UI ComponentGallery",
                        header_font,
                        palette_.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        // CP32: footer sits above the status bar — measure its height and
        // walk up so the secondary text never paints on top of the chrome.
        RECT status_rect{};
        const int status_height = (status_.hwnd() != nullptr &&
                                   GetWindowRect(status_.hwnd(), &status_rect))
                                      ? (status_rect.bottom - status_rect.top)
                                      : dpi_.logical_to_pixels(24);
        const int footer_bottom = client.bottom - status_height - dpi_.logical_to_pixels(8);
        const int footer_top    = footer_bottom - dpi_.logical_to_pixels(20);
        RECT footer{left, footer_top, client.right - outer, footer_bottom};
        nfui::draw_text(target,
                        footer,
                        L"Every control class in its expected state. Vertical sections stack top to bottom.",
                        body_font,
                        palette_.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Section labels run down the left margin.
        static constexpr const wchar_t* labels[] = {
            L"Button",
            L"CheckBox",
            L"RadioButton",
            L"Edit",
            L"StaticText",
            L"ListBox",
            L"ComboBox",
            L"ListView",
            L"TreeView",
            L"IconView",
            L"TabControl",
            L"ProgressBar",
            L"Panel + Splitter",
        };

        const int gap = dpi_.logical_to_pixels(12);
        const int row_h  = dpi_.logical_to_pixels(28);
        const int tall_h = dpi_.logical_to_pixels(120);

        int heights[] = {
            row_h,
            row_h,
            row_h,
            row_h,
            row_h,
            tall_h,
            row_h,
            tall_h,
            tall_h,
            row_h,
            tall_h,
            row_h,
            tall_h,
        };

        int y = client.top + outer + dpi_.logical_to_pixels(36);
        for (std::size_t i = 0; i < std::size(labels); ++i) {
            RECT label{left, y, left + label_w - gap, y + heights[i]};
            nfui::draw_text(target,
                            label,
                            labels[i],
                            section_font,
                            palette_.text_secondary,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            y += heights[i] + gap;
        }
    }

    void apply_window_icon() noexcept {
        if (!resources_.has_icon(IDI_NFUI_APP)) {
            return;
        }
        HICON big = static_cast<HICON>(LoadImageW(instance_,
                                                  MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                  IMAGE_ICON,
                                                  GetSystemMetrics(SM_CXICON),
                                                  GetSystemMetrics(SM_CYICON),
                                                  LR_DEFAULTCOLOR));
        HICON small = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXSMICON),
                                                    GetSystemMetrics(SM_CYSMICON),
                                                    LR_DEFAULTCOLOR));
        if (big != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
        }
        if (small != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));
        }
    }

    void destroy_icon() noexcept {
        if (app_icon_ != nullptr) {
            DestroyIcon(app_icon_);
            app_icon_ = nullptr;
        }
    }

    static void insert_listview_column(HWND listview, int index, const wchar_t* text, int width) noexcept {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.cx = width;
        column.pszText = const_cast<LPWSTR>(text);
        ListView_InsertColumn(listview, index, &column);
    }

    static void insert_listview_row(HWND listview, int row, const wchar_t* c0, const wchar_t* c1, const wchar_t* c2) noexcept {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = const_cast<LPWSTR>(c0);
        ListView_InsertItem(listview, &item);

        LVITEMW sub1{};
        sub1.mask = LVIF_TEXT;
        sub1.iItem = row;
        sub1.iSubItem = 1;
        sub1.pszText = const_cast<LPWSTR>(c1);
        ListView_SetItem(listview, &sub1);

        LVITEMW sub2{};
        sub2.mask = LVIF_TEXT;
        sub2.iItem = row;
        sub2.iSubItem = 2;
        sub2.pszText = const_cast<LPWSTR>(c2);
        ListView_SetItem(listview, &sub2);
    }

    static HTREEITEM tree_item_insert(HWND treeview, HTREEITEM parent, HTREEITEM after, LPCWSTR text) noexcept {
        TVINSERTSTRUCTW item{};
        item.hParent = parent;
        item.hInsertAfter = after;
        item.item.mask = TVIF_TEXT;
        item.item.pszText = const_cast<LPWSTR>(text);
        return TreeView_InsertItem(treeview, &item);
    }

    static void insert_tab(HWND tabs, int index, LPCWSTR text) noexcept {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(text);
        TabCtrl_InsertItem(tabs, index, &item);
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemeMode mode_{nfui::ThemeMode::light};
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::DpiScale dpi_{96};
    HICON app_icon_{};

    nfui::Button      button_default_;
    nfui::Button      button_disabled_;
    nfui::Button      button_hover_;
    nfui::CheckBox    check_unchecked_;
    nfui::CheckBox    check_checked_;
    nfui::CheckBox    check_indeterm_;
    nfui::RadioButton radio_a_;
    nfui::RadioButton radio_b_;
    nfui::RadioButton radio_c_;
    nfui::Edit        edit_;
    nfui::StaticText  static_left_;
    nfui::StaticText  static_center_;
    nfui::StaticText  static_right_;
    nfui::ListBox     listbox_;
    nfui::ComboBox    combobox_;
    nfui::ListView    listview_;
    nfui::TreeView    treeview_;
    nfui::IconView    iconview_;
    nfui::StatusBar   status_;
    nfui::TabControl  tabs_;
    nfui::ProgressBar progress_;
    nfui::Panel       panel_;
    nfui::Splitter    splitter_;
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmd_line, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    // CP32: --theme <light|dark|high_contrast> lets the visual audit
    // capture each variant without restarting the binary. Defaults to light.
    auto parse_theme = [](PCWSTR cl) noexcept {
        if (cl == nullptr) return nfui::ThemeMode::light;
        const wchar_t* tag = wcsstr(cl, L"--theme");
        if (tag == nullptr) return nfui::ThemeMode::light;
        tag += 7;  // skip past "--theme"
        while (*tag == L' ' || *tag == L'\t') ++tag;
        // CP32: visual audit's quoteArgument wraps the value as "--theme \"dark\"";
        // skip the leading quote so the comparison sees 'dark', not '"dark'.
        if (*tag == L'"') ++tag;
        if (wcsncmp(tag, L"dark", 4) == 0 && (tag[4] == L' ' || tag[4] == 0 || tag[4] == L'"')) return nfui::ThemeMode::dark;
        if (wcsncmp(tag, L"high_contrast", 13) == 0) return nfui::ThemeMode::high_contrast;
        return nfui::ThemeMode::light;
    };
    const nfui::ThemeMode initial_mode = parse_theme(cmd_line);

    GalleryWindow window(instance);
    window.set_initial_theme(initial_mode);
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}