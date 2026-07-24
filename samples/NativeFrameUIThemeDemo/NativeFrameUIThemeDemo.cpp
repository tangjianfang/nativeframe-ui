// NativeFrameUIThemeDemo
//
// V1.0 capability: live theme transition showcase. The window renders a row
// of every supported control under the currently-selected ThemePalette. Three
// buttons at the top-right of the toolbar (Light / Dark / High Contrast) rebuild
// the demo rows in place so QA can compare palettes without restarting the
// binary.
//
// The toggle buttons are stable members of the window — only the demo controls
// are recreated when the palette changes.

#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <commctrl.h>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#include <windowsx.h>

namespace {

constexpr int id_theme_light = 101;
constexpr int id_theme_dark  = 102;
constexpr int id_theme_hc    = 103;

// Demonstrator control ids (distinct range so WM_COMMAND routing is obvious).
constexpr int id_ok_button      = 201;
constexpr int id_cancel_button  = 202;
constexpr int id_check_a        = 203;
constexpr int id_check_b        = 204;
constexpr int id_check_c        = 205;
constexpr int id_radio_a        = 206;
constexpr int id_radio_b        = 207;
constexpr int id_radio_c        = 208;
constexpr int id_edit           = 209;
constexpr int id_static_left    = 210;
constexpr int id_static_center  = 211;
constexpr int id_static_right   = 212;
constexpr int id_listbox        = 213;
constexpr int id_combobox       = 214;
constexpr int id_listview       = 215;
constexpr int id_treeview       = 216;
constexpr int id_iconview       = 217;
constexpr int id_progress       = 218;
constexpr int id_status         = 219;
constexpr int id_tabs           = 220;
constexpr int id_panel          = 221;
constexpr int id_splitter       = 222;

// CP37: tighter layout rhythm for the 940×700 compact window. The previous
// 16 / 12 / 48 / 20 / 44 / 16 cadence summed to 812 logical px of chrome +
// 736 of row content = 1548, far over the 700-tall viewport. New values
// shave ~250 logical px so every section fits inside the window.
constexpr int kOuterMargin         = 10;
constexpr int kGap                 = 8;
constexpr int kToolbarHeight       = 36;   // dedicated top row for title + toggles
constexpr int kSectionHeaderHeight = 16;
constexpr int kRowHeight           = 24;   // default row height for single-line controls
constexpr int kLabelWidth          = 140;  // fixed label column width
constexpr int kGroupGap            = 10;   // vertical space between sections

// CP32: per-row heights so the ListView / TreeView / Tabs / Panel rows have
// room for their multi-row content instead of clipping to the single-line
// 44 logical px default. CP37: trim each row ~36% so the 13-row stack fits
// inside the 700-tall compact window without the TreeView / Topbar section
// peeking past the bottom edge.
constexpr int kRowHeights[] = {
    24,  // Button
    24,  // CheckBox
    24,  // RadioButton
    24,  // Edit
    24,  // StaticText
    64,  // ListBox (5 rows × 13)
    24,  // ComboBox
    64,  // ListView (header + 3 rows × 13)
    64,  // TreeView (root + 3 children × 13)
    24,  // IconView
    24,  // ProgressBar
    48,  // TabControl (3 tabs visible band)
    40,  // Panel + Splitter
};

// Section group captions.
constexpr const wchar_t* kGroupButtons  = L"Buttons / Toggles";
constexpr const wchar_t* kGroupInputs   = L"Inputs / Lists";
constexpr const wchar_t* kGroupFeedback = L"Feedback";

// Per-row labels drawn to the left of each demonstrator control.
constexpr const wchar_t* kSectionButton      = L"Button";
constexpr const wchar_t* kSectionCheckBox    = L"CheckBox";
constexpr const wchar_t* kSectionRadio       = L"RadioButton";
constexpr const wchar_t* kSectionEdit        = L"Edit";
constexpr const wchar_t* kSectionStatic      = L"StaticText";
constexpr const wchar_t* kSectionListBox     = L"ListBox";
constexpr const wchar_t* kSectionComboBox    = L"ComboBox";
constexpr const wchar_t* kSectionListView    = L"ListView";
constexpr const wchar_t* kSectionTreeView    = L"TreeView";
constexpr const wchar_t* kSectionIconView    = L"IconView (app icon)";
constexpr const wchar_t* kSectionProgressBar = L"ProgressBar 40%";
constexpr const wchar_t* kSectionTabControl  = L"TabControl";
constexpr const wchar_t* kSectionSplitter    = L"Panel + Splitter";

struct DemoControls {
    std::unique_ptr<nfui::Button>      ok;
    std::unique_ptr<nfui::Button>      cancel;
    std::unique_ptr<nfui::CheckBox>    check_unchecked;
    std::unique_ptr<nfui::CheckBox>    check_checked;
    std::unique_ptr<nfui::CheckBox>    check_indeterminate;
    std::unique_ptr<nfui::RadioButton> radio_first;
    std::unique_ptr<nfui::RadioButton> radio_second;
    std::unique_ptr<nfui::RadioButton> radio_third;
    std::unique_ptr<nfui::Edit>        edit;
    std::unique_ptr<nfui::StaticText>  static_left;
    std::unique_ptr<nfui::StaticText>  static_center;
    std::unique_ptr<nfui::StaticText>  static_right;
    std::unique_ptr<nfui::ListBox>     listbox;
    std::unique_ptr<nfui::ComboBox>    combobox;
    std::unique_ptr<nfui::ListView>    listview;
    std::unique_ptr<nfui::TreeView>    treeview;
    std::unique_ptr<nfui::IconView>    iconview;
    std::unique_ptr<nfui::ProgressBar> progress;
    std::unique_ptr<nfui::StatusBar>   status;
    std::unique_ptr<nfui::TabControl>  tabs;
    std::unique_ptr<nfui::Panel>       panel;
    std::unique_ptr<nfui::Splitter>    splitter;
};

class ThemeDemoWindow final : public nfui::Window {
public:
    explicit ThemeDemoWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          mode_(nfui::ThemeMode::light),
          palette_(nfui::theme_palette(mode_)),
          app_icon_(nullptr) {
    }

    ~ThemeDemoWindow() noexcept override {
        destroy_icon();
        clear_demo();
    }

    // CP32: lets wWinMain seed the palette before create_main builds every
    // demo control. Without this, --theme dark would first paint light,
    // then a click on the Dark button would be required to see anything.
    [[nodiscard]] bool set_initial_theme(nfui::ThemeMode mode) noexcept {
        if (hwnd() != nullptr) return false;
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        return true;
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIThemeDemoWindow",
            L"NativeFrame UI ThemeDemo",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            // CP36: shrink from 980×1080 → 940×700.
            940,
            700,
        };

        if (!create(params)) {
            return false;
        }

        apply_window_icon();
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));

        if (!create_toggle_buttons()) {
            return false;
        }

        build_demo();
        layout_demo();

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            layout_demo();
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
            layout_demo();
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
            clear_demo();
            PostQuitMessage(0);
            return 0;
        default:
            return nfui::Window::handle_message(message, wparam, lparam);
        }
    }

    bool on_command(int command_id, HWND, UINT notification_code) override {
        switch (command_id) {
        case id_theme_light:
            if (notification_code == BN_CLICKED || notification_code == 0) {
                switch_mode(nfui::ThemeMode::light);
                return true;
            }
            break;
        case id_theme_dark:
            if (notification_code == BN_CLICKED || notification_code == 0) {
                switch_mode(nfui::ThemeMode::dark);
                return true;
            }
            break;
        case id_theme_hc:
            if (notification_code == BN_CLICKED || notification_code == 0) {
                switch_mode(nfui::ThemeMode::high_contrast);
                return true;
            }
            break;
        default:
            break;
        }
        return false;
    }

private:
    // DPI-scaled layout helpers. All constants above are logical pixels.
    [[nodiscard]] int outer_margin() const noexcept { return dpi_.logical_to_pixels(kOuterMargin); }
    [[nodiscard]] int gap() const noexcept { return dpi_.logical_to_pixels(kGap); }
    [[nodiscard]] int toolbar_height() const noexcept { return dpi_.logical_to_pixels(kToolbarHeight); }
    [[nodiscard]] int row_height() const noexcept { return dpi_.logical_to_pixels(kRowHeight); }
    [[nodiscard]] int label_width() const noexcept { return dpi_.logical_to_pixels(kLabelWidth); }
    [[nodiscard]] int section_header_height() const noexcept { return dpi_.logical_to_pixels(kSectionHeaderHeight); }
    [[nodiscard]] int group_gap() const noexcept { return dpi_.logical_to_pixels(kGroupGap); }

    [[nodiscard]] int section_top(const RECT& client) const noexcept {
        return client.top + outer_margin() + toolbar_height() + gap();
    }

    [[nodiscard]] int group_header_y(int group, const RECT& client) const noexcept {
        const int h = section_header_height();
        const int gg = group_gap();
        const int st = section_top(client);
        if (group == 0) return st;
        if (group == 1) {
            // Bottom of group 0 = st + (rows 0..2 heights)
            int y = st;
            for (int i = 0; i < 3; ++i) y += kRowHeights[i];
            return y + gg;
        }
        // Bottom of group 1 = st + (rows 0..8 heights) + h + gg
        int y = st + h + gg;
        for (int i = 0; i < 9; ++i) y += kRowHeights[i];
        return y + gg;
    }

    [[nodiscard]] int row_y(int row, const RECT& client) const noexcept {
        const int h = section_header_height();
        const int gg = group_gap();
        const int st = section_top(client);
        int y = st;
        int group_start = 0;
        // CP32: walk per-row heights from kRowHeights so each row's own
        // (taller) height contributes to the cumulative offset. Uniform
        // kRowHeight * N was clipping ListView / TreeView against the
        // single-line default.
        if (row >= 3) {
            y += h + gg;
            for (int i = 0; i < 3; ++i) {
                y += kRowHeights[i];
            }
            group_start = 3;
        }
        if (row >= 9) {
            y += h + gg;
            for (int i = 3; i < 9; ++i) {
                y += kRowHeights[i];
            }
            group_start = 9;
        }
        for (int i = group_start; i < row; ++i) {
            y += kRowHeights[i];
        }
        return y;
    }

    [[nodiscard]] bool create_toggle_buttons() noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id_theme_light,
            L"Light",
            0, 0, 120, 28,
        };

        theme_light_.inject_theme(&palette_, &fonts_);
        if (!theme_light_.create(params)) {
            return false;
        }

        params.control_id = id_theme_dark;
        params.text       = L"Dark";
        theme_dark_.inject_theme(&palette_, &fonts_);
        if (!theme_dark_.create(params)) {
            return false;
        }

        params.control_id = id_theme_hc;
        params.text       = L"High Contrast";
        theme_hc_.inject_theme(&palette_, &fonts_);
        if (!theme_hc_.create(params)) {
            return false;
        }

        apply_toggle_fonts();
        return true;
    }

    void apply_toggle_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font = fonts_.semibold(dpi_value, 9);
        SendMessageW(theme_light_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(theme_dark_.hwnd(),  WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(theme_hc_.hwnd(),    WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
    }

    void build_demo() {
        clear_demo();

        // Buttons: OK is the primary action; Cancel uses a custom border to
        // read as the secondary action. (A full ghost/outline fill would need
        // a surface/text override on the Button wrapper; the current V1 API
        // exposes border color as the secondary differentiator.)
        g_demo.ok = std::make_unique<nfui::Button>();
        g_demo.ok->set_style(nfui::ButtonStyle{.use_semibold = true});
        g_demo.cancel = std::make_unique<nfui::Button>();
        // Cancel reads as secondary via a subtle border accent. The wrapper
        // does not have a dedicated secondary flag, so we override the border
        // color to the theme accent and keep the transparent fill.
        g_demo.cancel->set_style(nfui::ButtonStyle{.border_color = palette_.accent});
        if (!create_child(*g_demo.ok,     id_ok_button,     L"OK"))     return;
        if (!create_child(*g_demo.cancel, id_cancel_button, L"Cancel")) return;

        // CheckBoxes — three states.
        g_demo.check_unchecked    = std::make_unique<nfui::CheckBox>();
        g_demo.check_checked      = std::make_unique<nfui::CheckBox>();
        g_demo.check_indeterminate = std::make_unique<nfui::CheckBox>();
        if (!create_child(*g_demo.check_unchecked,     id_check_a, L"Unchecked"))     return;
        if (!create_child(*g_demo.check_checked,       id_check_b, L"Checked"))       return;
        if (!create_child(*g_demo.check_indeterminate, id_check_c, L"Indeterminate")) return;
        SendMessageW(g_demo.check_checked->hwnd(),       BM_SETCHECK, BST_CHECKED,       0);
        SendMessageW(g_demo.check_indeterminate->hwnd(), BM_SETCHECK, BST_INDETERMINATE, 0);

        // RadioButtons — first pre-selected.
        g_demo.radio_first  = std::make_unique<nfui::RadioButton>();
        g_demo.radio_second = std::make_unique<nfui::RadioButton>();
        g_demo.radio_third  = std::make_unique<nfui::RadioButton>();
        if (!create_child(*g_demo.radio_first,  id_radio_a, L"First"))  return;
        if (!create_child(*g_demo.radio_second, id_radio_b, L"Second")) return;
        if (!create_child(*g_demo.radio_third,  id_radio_c, L"Third"))  return;
        SendMessageW(g_demo.radio_first->hwnd(), BM_SETCHECK, BST_CHECKED, 0);

        // Edit.
        g_demo.edit = std::make_unique<nfui::Edit>();
        if (!create_child(*g_demo.edit, id_edit, L"editable sample")) return;

        // StaticText — three alignments. CP8 wired up TextStyle::align_h so
        // the labels actually match the rendered layout (pre-CP8 every cell
        // rendered left-aligned regardless of the caption).
        g_demo.static_left   = std::make_unique<nfui::StaticText>();
        g_demo.static_center = std::make_unique<nfui::StaticText>();
        g_demo.static_right  = std::make_unique<nfui::StaticText>();
        if (!create_child(*g_demo.static_left,   id_static_left,   L"Left aligned"))   return;
        if (!create_child(*g_demo.static_center, id_static_center, L"Center aligned")) return;
        if (!create_child(*g_demo.static_right,  id_static_right,  L"Right aligned"))  return;
        nfui::TextStyle center_style{};
        center_style.align_h = nfui::StaticTextAlignH::center;
        g_demo.static_center->set_style(center_style);
        nfui::TextStyle right_style{};
        right_style.align_h = nfui::StaticTextAlignH::right;
        g_demo.static_right->set_style(right_style);

        // ListBox.
        g_demo.listbox = std::make_unique<nfui::ListBox>();
        g_demo.listbox->set_style(nfui::ListStyle{
            .selected_background = palette_.selection,
            .selected_foreground = palette_.selection_text,
        });
        if (!create_child(*g_demo.listbox, id_listbox, L"")) return;
        ListBox_AddString(g_demo.listbox->hwnd(), L"Alpha");
        ListBox_AddString(g_demo.listbox->hwnd(), L"Bravo");
        ListBox_AddString(g_demo.listbox->hwnd(), L"Charlie");
        ListBox_AddString(g_demo.listbox->hwnd(), L"Delta");
        ListBox_AddString(g_demo.listbox->hwnd(), L"Echo");
        // CP35: drop the LB_SETCURSEL 1 default that pre-highlighted
        // "Bravo" — the orange fill on a non-focused row read as a
        // stuck/ghost selection rather than a usable demo of the
        // selection palette. The list now shows its rows in a neutral
        // state until the user actually clicks one.

        // ComboBox.
        g_demo.combobox = std::make_unique<nfui::ComboBox>();
        if (!create_child(*g_demo.combobox, id_combobox, L"")) return;
        ComboBox_AddString(g_demo.combobox->hwnd(), L"Red");
        ComboBox_AddString(g_demo.combobox->hwnd(), L"Green");
        ComboBox_AddString(g_demo.combobox->hwnd(), L"Blue");
        ComboBox_AddString(g_demo.combobox->hwnd(), L"Yellow");
        ComboBox_AddString(g_demo.combobox->hwnd(), L"Purple");
        SendMessageW(g_demo.combobox->hwnd(), CB_SETCURSEL, 0, 0);

        // ListView (3 cols x 4 rows).
        g_demo.listview = std::make_unique<nfui::ListView>();
        g_demo.listview->set_style(nfui::ListViewStyle{
            .row_background      = palette_.surface,
            .row_foreground      = palette_.text,
            .selected_background = palette_.selection,
            .selected_foreground = palette_.selection_text,
            .header_caption      = palette_.text,
            .header_background   = palette_.surface,
        });
        if (!create_child(*g_demo.listview, id_listview, L"")) return;
        ListView_SetExtendedListViewStyle(g_demo.listview->hwnd(),
                                          LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        insert_listview_column(g_demo.listview->hwnd(), 0, L"Name",  140);
        insert_listview_column(g_demo.listview->hwnd(), 1, L"Type",  120);
        insert_listview_column(g_demo.listview->hwnd(), 2, L"Owner", 120);
        insert_listview_row(g_demo.listview->hwnd(), 0, L"Button",   L"Control", L"nfui_button");
        insert_listview_row(g_demo.listview->hwnd(), 1, L"ListBox",  L"Control", L"nfui_listbox");
        insert_listview_row(g_demo.listview->hwnd(), 2, L"ListView", L"Control", L"nfui_listview");
        insert_listview_row(g_demo.listview->hwnd(), 3, L"TreeView", L"Control", L"nfui_treeview");

        // TreeView — 2 levels, root expanded.
        g_demo.treeview = std::make_unique<nfui::TreeView>();
        g_demo.treeview->set_style(nfui::TreeViewStyle{
            .row_background      = palette_.surface,
            .row_foreground      = palette_.text,
            .line_color          = palette_.border,
            .selected_background = palette_.selection,
            .selected_foreground = palette_.selection_text,
        });
        if (!create_child(*g_demo.treeview, id_treeview, L"")) return;
        HTREEITEM root = tree_item_insert(g_demo.treeview->hwnd(), TVI_ROOT, TVI_LAST, L"Project");
        tree_item_insert(g_demo.treeview->hwnd(), root, TVI_LAST, L"Source");
        tree_item_insert(g_demo.treeview->hwnd(), root, TVI_LAST, L"Resources");
        tree_item_insert(g_demo.treeview->hwnd(), root, TVI_LAST, L"Tests");
        TreeView_Expand(g_demo.treeview->hwnd(), root, TVE_EXPAND);

        // IconView.
        g_demo.iconview = std::make_unique<nfui::IconView>();
        g_demo.iconview->set_style(nfui::IconViewStyle{
            .pixel_size  = 32,
            .padding     = 4,
        });
        if (!create_child(*g_demo.iconview, id_iconview, L"")) return;
        if (app_icon_ == nullptr) {
            app_icon_ = nfui::load_scaled_icon(instance_,
                                               MAKEINTRESOURCEW(IDI_NFUI_APP),
                                               32,
                                               dpi_.dpi());
        }
        if (app_icon_ != nullptr) {
            g_demo.iconview->set_icon(app_icon_);
        }

        // ProgressBar.
        g_demo.progress = std::make_unique<nfui::ProgressBar>();
        g_demo.progress->set_style(nfui::FrameStyle{
            .surface_brush = palette_.surface,
            .bar_color     = palette_.accent,
        });
        if (!create_child(*g_demo.progress, id_progress, L"")) return;
        SendMessageW(g_demo.progress->hwnd(), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(g_demo.progress->hwnd(), PBM_SETPOS, 40, 0);

        // StatusBar.
        g_demo.status = std::make_unique<nfui::StatusBar>();
        g_demo.status->set_style(nfui::FrameStyle{.surface_brush = palette_.surface});
        if (!create_child(*g_demo.status, id_status, L"")) return;
        SendMessageW(g_demo.status->hwnd(),
                     SB_SETTEXTW,
                     0,
                     reinterpret_cast<LPARAM>(L"ThemeDemo: ready"));

        // TabControl (3 tabs).
        g_demo.tabs = std::make_unique<nfui::TabControl>();
        if (!create_child(*g_demo.tabs, id_tabs, L"")) return;
        static_cast<void>(g_demo.tabs->set_padding(dpi_.logical_to_pixels(12), dpi_.logical_to_pixels(4)));
        insert_tab(g_demo.tabs->hwnd(), 0, L"General");
        insert_tab(g_demo.tabs->hwnd(), 1, L"Layout");
        insert_tab(g_demo.tabs->hwnd(), 2, L"Output");

        // Panel + Splitter (V1.4 chrome demonstration).
        g_demo.panel     = std::make_unique<nfui::Panel>();
        g_demo.splitter  = std::make_unique<nfui::Splitter>();
        g_demo.panel->set_style(nfui::FrameStyle{
            .accent        = palette_.border,
            .surface_brush = palette_.surface,
        });
        if (!create_child(*g_demo.panel,    id_panel,    L"")) return;
        if (!create_child(*g_demo.splitter, id_splitter, L"")) return;

        apply_native_fonts();
    }

    void layout_demo() {
        if (hwnd() == nullptr || g_demo.status == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        SendMessageW(g_demo.status->hwnd(), WM_SIZE, 0, 0);

        const int outer = outer_margin();
        const int gap_v = gap();
        const int toolbar_h = toolbar_height();
        const int row_h = row_height();
        const int label_w = label_width();

        // Theme toggles live on the right side of the top toolbar.
        const int right = client.right - outer;
        const int hc_w = dpi_.logical_to_pixels(160);
        const int btn_w = dpi_.logical_to_pixels(120);
        const int hc_x = right - hc_w;
        const int dark_x = hc_x - gap_v - btn_w;
        const int light_x = dark_x - gap_v - btn_w;
        const int toolbar_top = client.top + outer;

        MoveWindow(theme_light_.hwnd(),
                   light_x,
                   toolbar_top,
                   btn_w,
                   toolbar_h,
                   TRUE);
        MoveWindow(theme_dark_.hwnd(),
                   dark_x,
                   toolbar_top,
                   btn_w,
                   toolbar_h,
                   TRUE);
        MoveWindow(theme_hc_.hwnd(),
                   hc_x,
                   toolbar_top,
                   hc_w,
                   toolbar_h,
                   TRUE);

        auto place_row = [&](int index,
                             std::initializer_list<std::pair<int, int>> sizes) {
            const int y = row_y(index, client);
            const int h = dpi_.logical_to_pixels(kRowHeights[index]);
            int x = client.left + outer + label_w;
            for (auto [id, w] : sizes) {
                HWND child = GetDlgItem(hwnd(), id);
                if (child != nullptr) {
                    MoveWindow(child, x, y, dpi_.logical_to_pixels(w), h, TRUE);
                    x += dpi_.logical_to_pixels(w) + gap_v;
                }
            }
        };

        place_row(0, {{id_ok_button, 90}, {id_cancel_button, 90}});
        place_row(1, {{id_check_a, 110}, {id_check_b, 110}, {id_check_c, 130}});
        place_row(2, {{id_radio_a, 80}, {id_radio_b, 90}, {id_radio_c, 90}});
        place_row(3, {{id_edit, 320}});
        place_row(4, {{id_static_left, 120}, {id_static_center, 120}, {id_static_right, 120}});
        place_row(5, {{id_listbox, 200}});
        place_row(6, {{id_combobox, 200}});
        place_row(7, {{id_listview, 380}});
        place_row(8, {{id_treeview, 200}});
        place_row(9, {{id_iconview, 32}});
        place_row(10, {{id_progress, 380}});
        place_row(11, {{id_tabs, 380}});

        // Panel + Splitter share the last row.
        const int y = row_y(12, client);
        const int h = dpi_.logical_to_pixels(kRowHeights[12]);
        int x = client.left + outer + label_w;
        HWND panel_h    = GetDlgItem(hwnd(), id_panel);
        HWND splitter_h = GetDlgItem(hwnd(), id_splitter);
        if (panel_h != nullptr) {
            MoveWindow(panel_h,
                       x,
                       y,
                       dpi_.logical_to_pixels(380),
                       h,
                       TRUE);
        }
        if (splitter_h != nullptr) {
            MoveWindow(splitter_h,
                       x + dpi_.logical_to_pixels(380) + gap_v,
                       y,
                       dpi_.logical_to_pixels(6),
                       h,
                       TRUE);
        }
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font = fonts_.regular(dpi_value, 9);
        if (g_demo.ok != nullptr) {
            SendMessageW(g_demo.ok->hwnd(),      WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.cancel != nullptr) {
            SendMessageW(g_demo.cancel->hwnd(),  WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.check_unchecked != nullptr) {
            SendMessageW(g_demo.check_unchecked->hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.check_checked != nullptr) {
            SendMessageW(g_demo.check_checked->hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.check_indeterminate != nullptr) {
            SendMessageW(g_demo.check_indeterminate->hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.radio_first != nullptr) {
            SendMessageW(g_demo.radio_first->hwnd(),  WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.radio_second != nullptr) {
            SendMessageW(g_demo.radio_second->hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.radio_third != nullptr) {
            SendMessageW(g_demo.radio_third->hwnd(),  WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.listbox != nullptr) {
            SendMessageW(g_demo.listbox->hwnd(),    WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.combobox != nullptr) {
            SendMessageW(g_demo.combobox->hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.edit != nullptr) {
            SendMessageW(g_demo.edit->hwnd(),       WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.listview != nullptr) {
            SendMessageW(g_demo.listview->hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.treeview != nullptr) {
            SendMessageW(g_demo.treeview->hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.tabs != nullptr) {
            SendMessageW(g_demo.tabs->hwnd(),       WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (g_demo.status != nullptr) {
            SendMessageW(g_demo.status->hwnd(),     WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        apply_toggle_fonts();
    }

    void paint_background(HDC target, const RECT& client) {
        nfui::fill_rect(target, client, palette_.background);

        const int dpi_value = dpi_.dpi();
        const int outer = outer_margin();
        const int gap_v = gap();
        const int toolbar_h = toolbar_height();
        const int row_h = row_height();
        const int label_w = label_width();
        const int header_h = section_header_height();

        HFONT title_font   = fonts_.semibold(dpi_value, 18);
        HFONT section_font = fonts_.semibold(dpi_value, 9);
        HFONT body_font    = fonts_.regular(dpi_value, 9);

        // Title on the left of the toolbar, toggles on the right.
        const int right = client.right - outer;
        const int hc_w = dpi_.logical_to_pixels(160);
        const int btn_w = dpi_.logical_to_pixels(120);
        const int hc_x = right - hc_w;
        const int dark_x = hc_x - gap_v - btn_w;
        const int light_x = dark_x - gap_v - btn_w;
        const int toolbar_top = client.top + outer;

        RECT title{client.left + outer,
                   toolbar_top,
                   light_x - gap_v,
                   toolbar_top + toolbar_h};
        // CP35: shorten the painted headline from "NativeFrame UI
        // ThemeDemo" to "Theme Demo" so it matches the "Component
        // Gallery" / "Resource Gallery" / "Settings" surface vocabulary
        // and stops crowding the Light/Dark/HC toggle trio on the 980
        // default width. The window title bar still carries the full
        // "NativeFrame UI — Theme Demo" so the framework context is
        // preserved.
        nfui::draw_text(target,
                        title,
                        L"Theme Demo",
                        title_font,
                        palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Subtle hairline under the toolbar to separate it from the content.
        nfui::draw_line(target,
                        POINT{client.left + outer, toolbar_top + toolbar_h},
                        POINT{client.right - outer, toolbar_top + toolbar_h},
                        palette_.border,
                        1);

        // Section group headers.
        auto draw_section_header = [&](int group, const wchar_t* text) {
            const int y = group_header_y(group, client);
            RECT header{client.left + outer,
                        y,
                        client.left + outer + label_w,
                        y + header_h};
            nfui::draw_text(target,
                            header,
                            text,
                            section_font,
                            palette_.text,
                            DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
        };

        draw_section_header(0, kGroupButtons);
        draw_section_header(1, kGroupInputs);
        draw_section_header(2, kGroupFeedback);

        // Per-row labels to the left of each demonstrator control.
        auto draw_label = [&](int row, const wchar_t* text) {
            const int y = row_y(row, client);
            const int h = dpi_.logical_to_pixels(kRowHeights[row]);
            RECT label{client.left + outer,
                       y,
                       client.left + outer + label_w - gap_v,
                       y + h};
            nfui::draw_text(target,
                            label,
                            text,
                            section_font,
                            palette_.text_secondary,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        };

        draw_label(0,  kSectionButton);
        draw_label(1,  kSectionCheckBox);
        draw_label(2,  kSectionRadio);
        draw_label(3,  kSectionEdit);
        draw_label(4,  kSectionStatic);
        draw_label(5,  kSectionListBox);
        draw_label(6,  kSectionComboBox);
        draw_label(7,  kSectionListView);
        draw_label(8,  kSectionTreeView);
        draw_label(9,  kSectionIconView);
        draw_label(10, kSectionProgressBar);
        draw_label(11, kSectionTabControl);
        draw_label(12, kSectionSplitter);

        // Active mode footer anchored to the bottom-right, above the status bar.
        int status_height = 0;
        if (g_demo.status != nullptr) {
            RECT status_rect{};
            GetWindowRect(g_demo.status->hwnd(), &status_rect);
            status_height = status_rect.bottom - status_rect.top;
        }
        RECT footer{client.left + label_w + outer,
                    client.bottom - status_height - outer - row_h,
                    client.right - outer,
                    client.bottom - status_height - outer};
        const wchar_t* mode_text =
            (mode_ == nfui::ThemeMode::high_contrast) ? L"Active mode: High Contrast"
            : (mode_ == nfui::ThemeMode::dark)       ? L"Active mode: Dark"
                                                      : L"Active mode: Light";
        nfui::draw_text(target,
                        footer,
                        mode_text,
                        body_font,
                        palette_.text_secondary,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void switch_mode(nfui::ThemeMode new_mode) noexcept {
        if (new_mode == mode_) {
            return;
        }
        mode_ = new_mode;
        palette_ = nfui::theme_palette(mode_);

        // Refresh palette/font on the persistent toggle buttons so they match
        // the new theme before the demo rebuilds.
        theme_light_.set_palette(&palette_);
        theme_dark_.set_palette(&palette_);
        theme_hc_.set_palette(&palette_);

        build_demo();
        layout_demo();
        apply_native_fonts();
        InvalidateRect(hwnd(), nullptr, FALSE);
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
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG,   reinterpret_cast<LPARAM>(big));
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

    void clear_demo() noexcept {
        // OwnedHwnd handles DestroyWindow on destruction; just drop the
        // unique_ptrs and the demo controls release their HWNDs.
        g_demo = DemoControls{};
    }

    template <typename ControlT>
    [[nodiscard]] bool create_child(ControlT& control,
                                    int id,
                                    std::wstring_view text) noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id,
            text,
            0,
            0,
            100,
            28,
        };
        control.inject_theme(&palette_, &fonts_);
        return control.create(params);
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
    nfui::Button theme_light_;
    nfui::Button theme_dark_;
    nfui::Button theme_hc_;
    nfui::ThemeMode mode_;
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::DpiScale dpi_{96};
    HICON app_icon_{};
    DemoControls g_demo{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmd_line, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    // CP32: --theme seeds the mode before create_main so the first paint
    // already shows the requested palette.
    auto parse_theme = [](PCWSTR cl) noexcept {
        if (cl == nullptr) return nfui::ThemeMode::light;
        const wchar_t* tag = wcsstr(cl, L"--theme");
        if (tag == nullptr) return nfui::ThemeMode::light;
        tag += 7;
        while (*tag == L' ' || *tag == L'\t') ++tag;
        // CP32: visual audit's quoteArgument wraps the value as "--theme \"dark\"";
        // skip the leading quote so the comparison sees 'dark', not '"dark'.
        if (*tag == L'"') ++tag;
        if (wcsncmp(tag, L"dark", 4) == 0 && (tag[4] == L' ' || tag[4] == 0 || tag[4] == L'"')) return nfui::ThemeMode::dark;
        if (wcsncmp(tag, L"high_contrast", 13) == 0) return nfui::ThemeMode::high_contrast;
        return nfui::ThemeMode::light;
    };
    const nfui::ThemeMode initial_mode = parse_theme(cmd_line);

    ThemeDemoWindow window(instance);
    if (!window.set_initial_theme(initial_mode) || !window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}
