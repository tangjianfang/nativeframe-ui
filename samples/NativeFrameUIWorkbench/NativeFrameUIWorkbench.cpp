#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

constexpr UINT ocm_base = WM_USER + 0x1c00;

constexpr int cmd_about = 40002;
constexpr int cmd_context_refresh = 41001;

constexpr int cmd_theme_light = 40101;
constexpr int cmd_theme_dark = 40102;
constexpr int cmd_theme_hc = 40103;

constexpr int cmd_toolbar_plus = 40201;
constexpr int cmd_toolbar_hamburger = 40202;
constexpr int cmd_toolbar_chevron = 40203;
constexpr int cmd_toolbar_play = 40204;
constexpr int cmd_toolbar_warning = 40205;
constexpr int cmd_toolbar_search = 40206;

constexpr int id_toolbar_panel = 200;
constexpr int id_left_panel = 201;
constexpr int id_center_panel = 202;
constexpr int id_right_panel = 203;
constexpr int id_theme_light = 204;
constexpr int id_theme_dark = 205;
constexpr int id_theme_hc = 206;
constexpr int id_project_header = 207;
constexpr int id_search = 208;
constexpr int id_tree = 209;
constexpr int id_tabs = 210;
constexpr int id_list = 211;
constexpr int id_list_footer = 212;
constexpr int id_zoom_label = 213;
constexpr int id_zoom_slider = 214;
constexpr int id_zoom_value = 215;
constexpr int id_build_label = 216;
constexpr int id_build_progress = 217;
constexpr int id_build_pct = 218;
constexpr int id_inspector_text = 219;
constexpr int id_left_splitter = 220;
constexpr int id_right_splitter = 221;
constexpr int id_status = 222;

class WorkbenchWindow final : public nfui::Window {
public:
    explicit WorkbenchWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)),
          menu_(palette_) {
        commands_.set_handler(nfui::CommandId{IDM_NFUI_EXIT}, [this](nfui::CommandId) {
            destroy();
            return true;
        });
        commands_.set_handler(nfui::CommandId{cmd_about}, [this](nfui::CommandId) {
            static_cast<void>(about_.show_modal(instance_,
                                                MAKEINTRESOURCEW(IDD_NFUI_ABOUT),
                                                hwnd(),
                                                &WorkbenchWindow::about_dlg_proc));
            return true;
        });

        toolbar_icons_[0] = ToolbarIcon{nfui::IconKind::plus,        false, {}, cmd_toolbar_plus};
        toolbar_icons_[1] = ToolbarIcon{nfui::IconKind::hamburger,     false, {}, cmd_toolbar_hamburger};
        toolbar_icons_[2] = ToolbarIcon{nfui::IconKind::chevron_right, false, {}, cmd_toolbar_chevron};
        toolbar_icons_[3] = ToolbarIcon{nfui::IconKind::none,          true,  {}, cmd_toolbar_play};
        toolbar_icons_[4] = ToolbarIcon{nfui::IconKind::warning,       false, {}, cmd_toolbar_warning};
        toolbar_icons_[5] = ToolbarIcon{nfui::IconKind::search,        false, {}, cmd_toolbar_search};
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIWorkbenchWindow",
            L"NativeFrame UI Workbench",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1320,
            860,
        };

        if (!create(params)) {
            return false;
        }

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        apply_window_icon();
        rebuild_menu();
        SetMenu(hwnd(), menu_.bar().get());

        if (!create_children()) {
            return false;
        }

        populate_tree();
        populate_list();
        apply_native_fonts();
        layout();

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            layout();
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
            layout();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_CONTEXTMENU:
            show_context_menu(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
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
                nfui::fill_rect(target, client, palette_.background);
            }
            EndPaint(hwnd(), &paint);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return nfui::Window::handle_message(message, wparam, lparam);
        }
    }

    bool on_command(int command_id, HWND, UINT notification_code) override {
        (void)notification_code;
        switch (command_id) {
        case IDM_NFUI_EXIT:
            destroy();
            return true;
        case cmd_about:
            static_cast<void>(about_.show_modal(instance_,
                                                MAKEINTRESOURCEW(IDD_NFUI_ABOUT),
                                                hwnd(),
                                                &WorkbenchWindow::about_dlg_proc));
            return true;
        case cmd_theme_light:
            set_theme(nfui::ThemeMode::light);
            return true;
        case cmd_theme_dark:
            set_theme(nfui::ThemeMode::dark);
            return true;
        case cmd_theme_hc:
            set_theme(nfui::ThemeMode::high_contrast);
            return true;
        case cmd_toolbar_plus:
        case cmd_toolbar_hamburger:
        case cmd_toolbar_chevron:
        case cmd_toolbar_play:
        case cmd_toolbar_warning:
        case cmd_toolbar_search:
            on_toolbar_command(command_id);
            return true;
        case cmd_context_refresh:
            set_status(L"Context refresh command routed.");
            return true;
        }
        if (notification_code == 0 && commands_.route(nfui::CommandId{command_id})) {
            return true;
        }
        return false;
    }

private:
    struct ToolbarIcon {
        nfui::IconKind kind{nfui::IconKind::none};
        bool custom_play{};
        RECT rect{};
        int command{};
    };

    static INT_PTR CALLBACK about_dlg_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) noexcept {
        (void)lp;
        switch (msg) {
            case WM_INITDIALOG:
                return TRUE;
            case WM_COMMAND:
                if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
                    EndDialog(dlg, LOWORD(wp));
                    return TRUE;
                }
                return FALSE;
            case WM_CLOSE:
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            default:
                return FALSE;
        }
    }

    [[nodiscard]] bool create_children() noexcept {
        // Panels first so they can serve as parents / backgrounds.
        nfui::ControlCreateParams panel_params{
            instance_, hwnd(), id_toolbar_panel, L"", 0, 0, 100, 44};
        toolbar_panel_.inject_theme(&palette_, &fonts_);
        if (!toolbar_panel_.create(panel_params)) {
            return false;
        }

        panel_params.control_id = id_left_panel;
        left_panel_.inject_theme(&palette_, &fonts_);
        if (!left_panel_.create(panel_params)) {
            return false;
        }

        panel_params.control_id = id_center_panel;
        center_panel_.inject_theme(&palette_, &fonts_);
        if (!center_panel_.create(panel_params)) {
            return false;
        }

        panel_params.control_id = id_right_panel;
        right_panel_.inject_theme(&palette_, &fonts_);
        if (!right_panel_.create(panel_params)) {
            return false;
        }

        // Toolbar theme pills.
        nfui::ControlCreateParams button_params{
            instance_, hwnd(), id_theme_light, L"Light", 0, 0, 52, 26};
        theme_light_.inject_theme(&palette_, &fonts_);
        if (!theme_light_.create(button_params)) {
            return false;
        }

        button_params.control_id = id_theme_dark;
        button_params.text = L"Dark";
        theme_dark_.inject_theme(&palette_, &fonts_);
        if (!theme_dark_.create(button_params)) {
            return false;
        }

        button_params.control_id = id_theme_hc;
        button_params.text = L"HC";
        theme_hc_.inject_theme(&palette_, &fonts_);
        if (!theme_hc_.create(button_params)) {
            return false;
        }

        // Left sidebar controls.
        nfui::ControlCreateParams child_params{
            instance_, hwnd(), id_project_header, L"PROJECT", 0, 0, 100, 22};
        project_header_.inject_theme(&palette_, &fonts_);
        if (!project_header_.create(child_params)) {
            return false;
        }

        child_params.control_id = id_search;
        child_params.text = L"";
        child_params.height = 32;
        search_.inject_theme(&palette_, &fonts_);
        if (!search_.create(child_params)) {
            return false;
        }

        child_params.control_id = id_tree;
        child_params.text = L"";
        child_params.height = 100;
        tree_.inject_theme(&palette_, &fonts_);
        if (!tree_.create(child_params)) {
            return false;
        }

        // Center controls.
        child_params.parent = hwnd();
        child_params.control_id = id_tabs;
        child_params.text = L"";
        child_params.height = 36;
        tabs_.inject_theme(&palette_, &fonts_);
        if (!tabs_.create(child_params)) {
            return false;
        }
        static_cast<void>(tabs_.set_padding(dip(12), dip(4)));

        child_params.control_id = id_list;
        child_params.text = L"";
        child_params.height = 100;
        list_.inject_theme(&palette_, &fonts_);
        if (!list_.create(child_params)) {
            return false;
        }
        ListView_SetExtendedListViewStyle(list_.hwnd(), LVS_EX_FULLROWSELECT | LVS_EX_TRACKSELECT);

        child_params.control_id = id_list_footer;
        child_params.text = L"5 items";
        child_params.height = 24;
        list_footer_.inject_theme(&palette_, &fonts_);
        if (!list_footer_.create(child_params)) {
            return false;
        }

        // Right panel controls.
        child_params.parent = hwnd();
        child_params.control_id = id_zoom_label;
        child_params.text = L"Zoom";
        child_params.height = 20;
        zoom_label_.inject_theme(&palette_, &fonts_);
        if (!zoom_label_.create(child_params)) {
            return false;
        }

        child_params.control_id = id_zoom_slider;
        child_params.text = L"";
        child_params.height = 24;
        zoom_slider_.inject_theme(&palette_, &fonts_);
        if (!zoom_slider_.create(child_params)) {
            return false;
        }
        zoom_slider_.set_range(0, 100);
        zoom_slider_.set_pos(60);

        child_params.control_id = id_zoom_value;
        child_params.text = L"60%";
        child_params.height = 24;
        zoom_value_.inject_theme(&palette_, &fonts_);
        if (!zoom_value_.create(child_params)) {
            return false;
        }

        child_params.control_id = id_build_label;
        child_params.text = L"Build";
        child_params.height = 20;
        build_label_.inject_theme(&palette_, &fonts_);
        if (!build_label_.create(child_params)) {
            return false;
        }

        child_params.control_id = id_build_progress;
        child_params.text = L"";
        child_params.height = 24;
        build_progress_.inject_theme(&palette_, &fonts_);
        if (!build_progress_.create(child_params)) {
            return false;
        }
        SendMessageW(build_progress_.hwnd(), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(build_progress_.hwnd(), PBM_SETPOS, 40, 0);

        child_params.control_id = id_build_pct;
        child_params.text = L"40%";
        child_params.height = 24;
        build_pct_.inject_theme(&palette_, &fonts_);
        if (!build_pct_.create(child_params)) {
            return false;
        }

        child_params.control_id = id_inspector_text;
        child_params.text = L"Inspector — select an item to inspect its properties.";
        child_params.height = 80;
        inspector_text_.inject_theme(&palette_, &fonts_);
        if (!inspector_text_.create(child_params)) {
            return false;
        }

        // Splitters and status bar (children of main window).
        nfui::ControlCreateParams main_child{
            instance_, hwnd(), id_left_splitter, L"", 0, 0, 4, 100};
        left_splitter_.inject_theme(&palette_, &fonts_);
        left_splitter_.set_orientation(nfui::SplitterOrientation::Vertical);
        if (!left_splitter_.create(main_child)) {
            return false;
        }

        main_child.control_id = id_right_splitter;
        right_splitter_.inject_theme(&palette_, &fonts_);
        right_splitter_.set_orientation(nfui::SplitterOrientation::Vertical);
        if (!right_splitter_.create(main_child)) {
            return false;
        }

        main_child.control_id = id_status;
        main_child.text = L"";
        main_child.height = 22;
        status_.inject_theme(&palette_, &fonts_);
        if (!status_.create(main_child)) {
            return false;
        }

        // Install post-paint / custom-draw subclasses.
        if (SetWindowSubclass(search_.hwnd(), &search_subclass_proc,
                              reinterpret_cast<UINT_PTR>(this),
                              reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
            return false;
        }
        if (SetWindowSubclass(list_.hwnd(), &listview_subclass_proc,
                              reinterpret_cast<UINT_PTR>(this),
                              reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
            return false;
        }
        if (SetWindowSubclass(right_panel_.hwnd(), &inspector_subclass_proc,
                              reinterpret_cast<UINT_PTR>(this),
                              reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
            return false;
        }
        if (SetWindowSubclass(toolbar_panel_.hwnd(), &toolbar_subclass_proc,
                              reinterpret_cast<UINT_PTR>(this),
                              reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
            return false;
        }
        if (SetWindowSubclass(status_.hwnd(), &status_subclass_proc,
                              reinterpret_cast<UINT_PTR>(this),
                              reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
            return false;
        }
        if (SetWindowSubclass(build_progress_.hwnd(), &progress_subclass_proc,
                              reinterpret_cast<UINT_PTR>(this),
                              reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
            return false;
        }

        // Styling that depends on palette/fonts being injected.
        nfui::FrameStyle panel_style{};
        panel_style.surface_brush = palette_.surface;
        panel_style.accent = palette_.border;
        toolbar_panel_.set_style(panel_style);
        left_panel_.set_style(panel_style);
        center_panel_.set_style(panel_style);
        right_panel_.set_style(panel_style);

        nfui::FrameStyle tab_style{};
        tab_style.background = palette_.surface;
        tab_style.foreground = palette_.text;
        tab_style.chrome_text = palette_.text_secondary;
        tab_style.chrome_bg = palette_.surface;
        tabs_.set_style(tab_style);

        nfui::TreeViewStyle tree_style{};
        tree_style.row_background = palette_.surface;
        tree_style.row_foreground = palette_.text;
        tree_style.line_color = palette_.border;
        tree_style.selected_background = palette_.selection;
        tree_style.selected_foreground = palette_.selection_text;
        tree_.set_style(tree_style);

        nfui::ListViewStyle list_style{};
        list_style.row_background = palette_.surface;
        list_style.row_foreground = palette_.text;
        list_style.selected_background = palette_.selection;
        list_style.selected_foreground = palette_.selection_text;
        list_style.header_background = palette_.surface;
        list_style.header_caption = palette_.text;
        list_.set_style(list_style);

        nfui::FrameStyle progress_style{};
        progress_style.surface_brush = palette_.surface;
        progress_style.bar_color = palette_.accent;
        build_progress_.set_style(progress_style);

        nfui::SliderStyle slider_style{};
        slider_style.track_color = palette_.border;
        slider_style.filled_color = palette_.accent;
        slider_style.thumb_color = palette_.accent;
        zoom_slider_.set_style(slider_style);

        nfui::TextStyle header_style{};
        header_style.use_semibold = true;
        header_style.font_size_pt = nfui::font_pt::xs;
        header_style.foreground = palette_.text_secondary;
        header_style.align_v = nfui::StaticTextAlignV::middle;
        project_header_.set_style(header_style);

        nfui::TextStyle footer_style{};
        footer_style.font_size_pt = nfui::font_pt::xs;
        footer_style.foreground = palette_.text_secondary;
        footer_style.align_v = nfui::StaticTextAlignV::middle;
        list_footer_.set_style(footer_style);

        nfui::TextStyle label_style{};
        label_style.font_size_pt = nfui::font_pt::xs;
        label_style.foreground = palette_.text;
        label_style.align_v = nfui::StaticTextAlignV::middle;
        zoom_label_.set_style(label_style);
        build_label_.set_style(label_style);

        nfui::TextStyle value_style{};
        value_style.font_size_pt = nfui::font_pt::xs;
        value_style.foreground = palette_.text_secondary;
        value_style.align_h = nfui::StaticTextAlignH::right;
        value_style.align_v = nfui::StaticTextAlignV::middle;
        zoom_value_.set_style(value_style);
        build_pct_.set_style(value_style);

        nfui::TextStyle inspector_style{};
        inspector_style.font_size_pt = nfui::font_pt::sm;
        inspector_style.foreground = palette_.text_secondary;
        inspector_style.align_h = nfui::StaticTextAlignH::center;
        inspector_style.align_v = nfui::StaticTextAlignV::top;
        inspector_style.single_line = false;
        inspector_text_.set_style(inspector_style);

        return true;
    }

    void apply_native_fonts() noexcept {
        const int d = dpi_.dpi();
        SendMessageW(search_.hwnd(), WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts_.regular(d, nfui::font_pt::sm)), TRUE);
        SendMessageW(tree_.hwnd(), WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts_.regular(d, nfui::font_pt::sm)), TRUE);
        SendMessageW(list_.hwnd(), WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts_.regular(d, nfui::font_pt::sm)), TRUE);
        SendMessageW(tabs_.hwnd(), WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts_.regular(d, nfui::font_pt::sm)), TRUE);
        SendMessageW(zoom_value_.hwnd(), WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts_.regular(d, nfui::font_pt::xs)), TRUE);
        SendMessageW(build_pct_.hwnd(), WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts_.regular(d, nfui::font_pt::xs)), TRUE);

        SendMessageW(search_.hwnd(), EM_SETMARGINS, EC_LEFTMARGIN,
                     MAKELPARAM(dip(28), 0));

        update_button_styles();
    }

    void update_button_styles() noexcept {
        nfui::ButtonStyle pill_style{};
        pill_style.corner_radius = dip(14);
        pill_style.horizontal_padding = dip(10);
        pill_style.vertical_padding = dip(4);
        theme_light_.set_style(pill_style);
        theme_dark_.set_style(pill_style);
        theme_hc_.set_style(pill_style);
    }

    void populate_tree() noexcept {
        if (tree_.hwnd() == nullptr) {
            return;
        }
        auto insert = [this](HTREEITEM parent, HTREEITEM after, const wchar_t* text) -> HTREEITEM {
            TVINSERTSTRUCTW item{};
            item.hParent = parent;
            item.hInsertAfter = after;
            item.item.mask = TVIF_TEXT;
            item.item.pszText = const_cast<wchar_t*>(text);
            return reinterpret_cast<HTREEITEM>(SendMessageW(tree_.hwnd(), TVM_INSERTITEMW, 0,
                                                            reinterpret_cast<LPARAM>(&item)));
        };

        HTREEITEM src = insert(TVI_ROOT, TVI_LAST, L"src");
        insert(src, TVI_LAST, L"main.cpp");
        insert(src, TVI_LAST, L"App.cpp");
        insert(src, TVI_LAST, L"App.h");
        insert(src, TVI_LAST, L"resources");
        insert(TVI_ROOT, TVI_LAST, L"tests");
        insert(TVI_ROOT, TVI_LAST, L"docs");
        TreeView_Expand(tree_.hwnd(), src, TVE_EXPAND);
    }

    void populate_list() noexcept {
        if (list_.hwnd() == nullptr) {
            return;
        }
        ListView_DeleteAllItems(list_.hwnd());
        status_colors_.clear();

        RECT rc{};
        GetClientRect(list_.hwnd(), &rc);
        const int total_w = std::max(static_cast<int>(rc.right - rc.left), dip(200));
        const int item_w = static_cast<int>(total_w * 0.65);
        const int status_w = total_w - item_w;

        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<wchar_t*>(L"Item");
        col.cx = item_w;
        ListView_InsertColumn(list_.hwnd(), 0, &col);
        col.pszText = const_cast<wchar_t*>(L"Status");
        col.cx = status_w;
        ListView_InsertColumn(list_.hwnd(), 1, &col);

        struct Row {
            const wchar_t* name;
            const wchar_t* status;
            bool pending;
        };
        constexpr std::array<Row, 5> rows{{
            {L"NativeFrameUI.lib",  L"linked",   false},
            {L"NativeFrameUI.dll",  L"ready",    false},
            {L"ChartView.obj",      L"compiled", false},
            {L"main.obj",           L"compiled", false},
            {L"tests.exe",          L"pending",  true},
        }};

        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = i;
            item.pszText = const_cast<wchar_t*>(rows[static_cast<std::size_t>(i)].name);
            ListView_InsertItem(list_.hwnd(), &item);
            ListView_SetItemText(list_.hwnd(), i, 1,
                                 const_cast<wchar_t*>(rows[static_cast<std::size_t>(i)].status));
            status_colors_.push_back(rows[static_cast<std::size_t>(i)].pending
                                         ? palette_.warning
                                         : palette_.success);
        }
    }

    void layout() noexcept {
        if (hwnd() == nullptr || status_.hwnd() == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        SendMessageW(status_.hwnd(), WM_SIZE, 0, 0);

        RECT client{};
        GetClientRect(hwnd(), &client);
        const int status_h = [this]() noexcept {
            RECT rc{};
            GetWindowRect(status_.hwnd(), &rc);
            return rc.bottom - rc.top;
        }();

        const int margin = dip(12);
        const int gap = dip(8);
        const int toolbar_h = dip(44);
        const int left_w = dip(240);
        const int right_w = dip(280);
        const int splitter_w = dip(4);
        const int content_top = toolbar_h + margin;
        const int content_h = client.bottom - content_top - status_h - margin;
        const int center_w = client.right - left_w - right_w - splitter_w * 2 - margin * 2;
        const int left_x = margin;
        const int splitter_left_x = left_x + left_w;
        const int center_x = splitter_left_x + splitter_w;
        const int splitter_right_x = center_x + center_w;
        const int right_x = splitter_right_x + splitter_w;

        MoveWindow(toolbar_panel_.hwnd(), 0, 0, client.right, toolbar_h, TRUE);
        MoveWindow(left_panel_.hwnd(), left_x, content_top, left_w, content_h, TRUE);
        MoveWindow(left_splitter_.hwnd(), splitter_left_x, content_top, splitter_w, content_h, TRUE);
        MoveWindow(center_panel_.hwnd(), center_x, content_top, center_w, content_h, TRUE);
        MoveWindow(right_splitter_.hwnd(), splitter_right_x, content_top, splitter_w, content_h, TRUE);
        MoveWindow(right_panel_.hwnd(), right_x, content_top, right_w, content_h, TRUE);

        // Theme pills in the toolbar.
        const int pill_w = dip(50);
        const int pill_h = dip(26);
        const int pill_gap = dip(8);
        const int pill_y = (toolbar_h - pill_h) / 2;
        int pill_right = client.right - margin;
        MoveWindow(theme_hc_.hwnd(), pill_right - pill_w, pill_y, pill_w, pill_h, TRUE);
        pill_right -= pill_w + pill_gap;
        MoveWindow(theme_dark_.hwnd(), pill_right - pill_w, pill_y, pill_w, pill_h, TRUE);
        pill_right -= pill_w + pill_gap;
        MoveWindow(theme_light_.hwnd(), pill_right - pill_w, pill_y, pill_w, pill_h, TRUE);

        // Toolbar icon positions (panel-relative).
        const int icon_size = dip(20);
        const int icon_y = (toolbar_h - icon_size) / 2;
        const int icon_gap = dip(8);
        int icon_x = margin;
        for (auto& icon : toolbar_icons_) {
            icon.rect.left = icon_x;
            icon.rect.top = icon_y;
            icon.rect.right = icon_x + icon_size;
            icon.rect.bottom = icon_y + icon_size;
            icon_x += icon_size + icon_gap;
        }

        // Left sidebar controls (siblings of left_panel_, main-client coords).
        const int p_margin = dip(12);
        const int header_h = dip(22);
        const int search_h = dip(32);
        MoveWindow(project_header_.hwnd(), left_x + p_margin, content_top + p_margin,
                   left_w - 2 * p_margin, header_h, TRUE);
        MoveWindow(search_.hwnd(), left_x + p_margin,
                   content_top + p_margin + header_h + gap,
                   left_w - 2 * p_margin, search_h, TRUE);
        const int search_bottom = p_margin + header_h + gap + search_h;
        MoveWindow(tree_.hwnd(), left_x + p_margin,
                   content_top + search_bottom + gap,
                   left_w - 2 * p_margin,
                   content_h - (search_bottom + gap) - p_margin, TRUE);

        // Center controls.
        const int tab_h = dip(36);
        const int footer_h = dip(24);
        MoveWindow(tabs_.hwnd(), center_x + p_margin, content_top + p_margin,
                   center_w - 2 * p_margin, tab_h, TRUE);
        const int tabs_bottom = p_margin + tab_h;
        MoveWindow(list_.hwnd(), center_x + p_margin,
                   content_top + tabs_bottom + gap,
                   center_w - 2 * p_margin,
                   content_h - (tabs_bottom + gap) - footer_h - p_margin, TRUE);
        MoveWindow(list_footer_.hwnd(), center_x + p_margin,
                   content_top + content_h - footer_h - p_margin,
                   center_w - 2 * p_margin, footer_h, TRUE);

        // Right panel controls.
        const int label_h = dip(20);
        const int ctrl_h = dip(24);
        const int value_w = dip(48);
        const int slider_w = right_w - 2 * p_margin - value_w - gap;
        MoveWindow(zoom_label_.hwnd(), right_x + p_margin, content_top + p_margin,
                   right_w - 2 * p_margin, label_h, TRUE);
        MoveWindow(zoom_slider_.hwnd(), right_x + p_margin,
                   content_top + p_margin + label_h + gap,
                   slider_w, ctrl_h, TRUE);
        MoveWindow(zoom_value_.hwnd(), right_x + p_margin + slider_w + gap,
                   content_top + p_margin + label_h + gap, value_w, ctrl_h, TRUE);
        const int zoom_bottom = p_margin + label_h + gap + ctrl_h;

        MoveWindow(build_label_.hwnd(), right_x + p_margin,
                   content_top + zoom_bottom + gap * 2,
                   right_w - 2 * p_margin, label_h, TRUE);
        const int progress_w = right_w - 2 * p_margin - value_w - gap;
        MoveWindow(build_progress_.hwnd(), right_x + p_margin,
                   content_top + zoom_bottom + gap * 2 + label_h + gap,
                   progress_w, ctrl_h, TRUE);
        MoveWindow(build_pct_.hwnd(), right_x + p_margin + progress_w + gap,
                   content_top + zoom_bottom + gap * 2 + label_h + gap,
                   value_w, ctrl_h, TRUE);
        const int build_bottom = zoom_bottom + gap * 2 + label_h + gap + ctrl_h;

        right_separator_y_ = build_bottom + dip(16);
        const int mag_size = dip(48);
        const int mag_x = (right_w - mag_size) / 2;
        const int mag_y = right_separator_y_ + dip(16);
        right_magnifier_rect_ = RECT{mag_x, mag_y, mag_x + mag_size, mag_y + mag_size};

        const int text_top = mag_y + mag_size + gap;
        MoveWindow(inspector_text_.hwnd(), right_x + p_margin,
                   content_top + text_top,
                   right_w - 2 * p_margin,
                   content_h - text_top - p_margin, TRUE);

        // Refresh overlays that depend on computed rects.
        if (toolbar_panel_.hwnd() != nullptr) {
            InvalidateRect(toolbar_panel_.hwnd(), nullptr, FALSE);
        }
        if (right_panel_.hwnd() != nullptr) {
            InvalidateRect(right_panel_.hwnd(), nullptr, FALSE);
        }
    }

    void set_theme(nfui::ThemeMode mode) noexcept {
        theme_mode_ = mode;
        palette_ = nfui::theme_palette(mode);

        toolbar_panel_.set_palette(&palette_);
        left_panel_.set_palette(&palette_);
        center_panel_.set_palette(&palette_);
        right_panel_.set_palette(&palette_);
        theme_light_.set_palette(&palette_);
        theme_dark_.set_palette(&palette_);
        theme_hc_.set_palette(&palette_);
        project_header_.set_palette(&palette_);
        search_.set_palette(&palette_);
        tree_.set_palette(&palette_);
        tabs_.set_palette(&palette_);
        list_.set_palette(&palette_);
        list_footer_.set_palette(&palette_);
        zoom_label_.set_palette(&palette_);
        zoom_slider_.set_palette(&palette_);
        zoom_value_.set_palette(&palette_);
        build_label_.set_palette(&palette_);
        build_progress_.set_palette(&palette_);
        build_pct_.set_palette(&palette_);
        inspector_text_.set_palette(&palette_);
        left_splitter_.set_palette(&palette_);
        right_splitter_.set_palette(&palette_);
        status_.set_palette(&palette_);

        // Rebuild the menu so its background brush picks up the new palette.
        menu_ = nfui::Menu(palette_);
        rebuild_menu();
        SetMenu(hwnd(), menu_.bar().get());
        DrawMenuBar(hwnd());

        apply_native_fonts();
        populate_list(); // refresh status dot colours
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void rebuild_menu() noexcept {
        menu_.apply_to_bar(hwnd());
        (void)menu_.builder(menu_.bar()).popup(L"&File")
            .item(L"E&xit", IDM_NFUI_EXIT);
        (void)menu_.builder(menu_.bar()).popup(L"&Edit")
            .item(L"Cu&t", 50101)
            .item(L"&Copy", 50102)
            .item(L"&Paste", 50103);
        auto view_builder = menu_.builder(menu_.bar()).popup(L"&View");
        (void)view_builder.popup(L"&Theme")
            .item(L"&Light", cmd_theme_light)
            .item(L"&Dark", cmd_theme_dark)
            .item(L"&High contrast", cmd_theme_hc);
        (void)menu_.builder(menu_.bar()).popup(L"&Run")
            .item(L"&Start", 50201);
        (void)menu_.builder(menu_.bar()).popup(L"&Window")
            .item(L"&Refresh", cmd_context_refresh);
        (void)menu_.builder(menu_.bar()).popup(L"&Help")
            .item(L"&About", cmd_about);
    }

    void set_status(const wchar_t* text) noexcept {
        status_text_ = text;
        if (status_.hwnd() != nullptr) {
            InvalidateRect(status_.hwnd(), nullptr, FALSE);
        }
    }

    void on_toolbar_command(int command) noexcept {
        switch (command) {
        case cmd_toolbar_plus:
            set_status(L"Toolbar: add");
            break;
        case cmd_toolbar_hamburger:
            set_status(L"Toolbar: menu");
            break;
        case cmd_toolbar_chevron:
            set_status(L"Toolbar: forward");
            break;
        case cmd_toolbar_play:
            set_status(L"Toolbar: run");
            break;
        case cmd_toolbar_warning:
            set_status(L"Toolbar: warning");
            break;
        case cmd_toolbar_search:
            set_status(L"Toolbar: search");
            break;
        }
    }

    void show_context_menu(int x, int y) noexcept {
        nfui::OwnedMenu popup = menu_.make_popup();
        (void)menu_.builder(popup).item(L"&Refresh", cmd_context_refresh);
        TrackPopupMenu(popup.get(), TPM_RIGHTBUTTON, x, y, 0, hwnd(), nullptr);
    }

    void apply_window_icon() noexcept {
        if (!resources_.has_icon(IDI_NFUI_APP)) {
            return;
        }
        large_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXICON),
                                                    GetSystemMetrics(SM_CYICON),
                                                    LR_DEFAULTCOLOR));
        small_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXSMICON),
                                                    GetSystemMetrics(SM_CYSMICON),
                                                    LR_DEFAULTCOLOR));
        if (large_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon_));
        }
        if (small_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon_));
        }
    }

    void destroy_icons() noexcept {
        if (large_icon_ != nullptr) {
            DestroyIcon(large_icon_);
            large_icon_ = nullptr;
        }
        if (small_icon_ != nullptr) {
            DestroyIcon(small_icon_);
            small_icon_ = nullptr;
        }
    }

    [[nodiscard]] int dip(int logical) const noexcept {
        return dpi_.logical_to_pixels(logical);
    }

    // Overlay painters.
    void paint_search_overlay(HWND hwnd) noexcept {
        if (GetWindowTextLengthW(hwnd) > 0) {
            return;
        }
        HDC dc = GetDC(hwnd);
        if (dc == nullptr) {
            return;
        }
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const int icon_size = dip(16);
        const int pad = dip(8);
        const int gap = dip(6);
        const int icon_x = rc.left + pad;
        const int icon_y = (rc.top + rc.bottom - icon_size) / 2;
        RECT icon_rc{icon_x, icon_y, icon_x + icon_size, icon_y + icon_size};
        nfui::draw_vector_icon(dc, nfui::IconKind::search, icon_rc,
                               palette_.text_secondary, dip(2));
        RECT text_rc{rc.left + icon_size + gap + pad, rc.top, rc.right - pad, rc.bottom};
        HFONT font = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);
        nfui::draw_text(dc, text_rc, L"Search files...", font, palette_.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ReleaseDC(hwnd, dc);
    }

    void paint_inspector_overlay(HWND hwnd) noexcept {
        HDC dc = GetDC(hwnd);
        if (dc == nullptr) {
            return;
        }
        RECT rc{};
        GetClientRect(hwnd, &rc);
        if (right_separator_y_ > 0 && right_separator_y_ < rc.bottom) {
            nfui::draw_line(dc,
                            {rc.left + dip(12), right_separator_y_},
                            {rc.right - dip(12), right_separator_y_},
                            palette_.border, dip(1));
        }
        if (right_magnifier_rect_.right > right_magnifier_rect_.left) {
            nfui::draw_vector_icon(dc, nfui::IconKind::search, right_magnifier_rect_,
                                   palette_.text_secondary, dip(2));
        }
        ReleaseDC(hwnd, dc);
    }

    void paint_toolbar_overlay(HWND hwnd) noexcept {
        HDC dc = GetDC(hwnd);
        if (dc == nullptr) {
            return;
        }
        const int stroke = dip(2);
        for (const auto& icon : toolbar_icons_) {
            if (icon.rect.right <= icon.rect.left || icon.rect.bottom <= icon.rect.top) {
                continue;
            }
            if (icon.custom_play) {
                paint_play_icon(dc, icon.rect, palette_.text, stroke);
            } else {
                nfui::draw_vector_icon(dc, icon.kind, icon.rect, palette_.text, stroke);
            }
        }
        ReleaseDC(hwnd, dc);
    }

    void paint_play_icon(HDC dc, const RECT& bounds, nfui::Color color, int stroke_width) noexcept {
        (void)stroke_width;
        const int margin = dip(2);
        const int avail_w = bounds.right - bounds.left - margin * 2;
        const int avail_h = bounds.bottom - bounds.top - margin * 2;
        const int size = std::min(avail_w, avail_h);
        const int cx = (bounds.left + bounds.right) / 2;
        const int cy = (bounds.top + bounds.bottom) / 2;
        POINT tri[3]{
            {cx - size / 3, cy - size / 2},
            {cx + size / 2, cy},
            {cx - size / 3, cy + size / 2}};
        nfui::fill_polygon(dc, tri, 3, color, color);
    }

    void paint_status_overlay(HWND hwnd) noexcept {
        HDC dc = GetDC(hwnd);
        if (dc == nullptr) {
            return;
        }
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const int pad = dip(8);
        const int dot_size = dip(8);
        const int grip = dip(16);
        const int cy = (rc.top + rc.bottom) / 2;
        const int dot_cx = rc.left + pad + dot_size / 2;
        RECT dot_rc{dot_cx - dot_size / 2, cy - dot_size / 2,
                    dot_cx + dot_size / 2, cy + dot_size / 2};
        nfui::fill_ellipse(dc, dot_rc, palette_.success);

        HFONT font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::xs);
        RECT text_rc{rc.left + pad + dot_size + dip(4), rc.top, rc.right - grip, rc.bottom};
        nfui::draw_text(dc, text_rc, status_text_.c_str(), font, palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        std::wstring dpi_text = std::to_wstring(dpi_.dpi()) + L"% DPI";
        RECT right_rc{rc.left, rc.top, rc.right - grip - pad, rc.bottom};
        nfui::draw_text(dc, right_rc, dpi_text.c_str(), font, palette_.text,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ReleaseDC(hwnd, dc);
    }

    void paint_progress_overlay(HWND hwnd) noexcept {
        HDC dc = GetDC(hwnd);
        if (dc == nullptr) {
            return;
        }
        RECT rc{};
        GetClientRect(hwnd, &rc);
        HFONT font = fonts_.regular(dpi_.dpi(), nfui::font_pt::xs);
        nfui::draw_text(dc, rc, L"Building...", font, palette_.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ReleaseDC(hwnd, dc);
    }

    void paint_list_status_dot(HDC dc, int row, const RECT& text_rc, int dot_size) noexcept {
        if (row < 0 || row >= static_cast<int>(status_colors_.size())) {
            return;
        }
        const int dot_cx = text_rc.left - dot_size / 2;
        const int cy = (text_rc.top + text_rc.bottom) / 2;
        RECT dot{dot_cx - dot_size / 2, cy - dot_size / 2,
                 dot_cx + dot_size / 2, cy + dot_size / 2};
        nfui::fill_ellipse(dc, dot, status_colors_[static_cast<std::size_t>(row)]);
    }

    // Subclass procs.
    static LRESULT CALLBACK search_subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                 UINT_PTR id, DWORD_PTR ref) noexcept {
        auto* self = reinterpret_cast<WorkbenchWindow*>(ref);
        switch (msg) {
        case WM_PAINT: {
            LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
            if (self != nullptr) {
                self->paint_search_overlay(hwnd);
            }
            return result;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, &search_subclass_proc, id);
            break;
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK inspector_subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                    UINT_PTR id, DWORD_PTR ref) noexcept {
        auto* self = reinterpret_cast<WorkbenchWindow*>(ref);
        switch (msg) {
        case WM_PAINT: {
            LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
            if (self != nullptr) {
                self->paint_inspector_overlay(hwnd);
            }
            return result;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, &inspector_subclass_proc, id);
            break;
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK toolbar_subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                  UINT_PTR id, DWORD_PTR ref) noexcept {
        auto* self = reinterpret_cast<WorkbenchWindow*>(ref);
        switch (msg) {
        case WM_PAINT: {
            LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
            if (self != nullptr) {
                self->paint_toolbar_overlay(hwnd);
            }
            return result;
        }
        case WM_LBUTTONUP: {
            if (self != nullptr) {
                int x = GET_X_LPARAM(lp);
                int y = GET_Y_LPARAM(lp);
                for (const auto& icon : self->toolbar_icons_) {
                    if (x >= icon.rect.left && x < icon.rect.right &&
                        y >= icon.rect.top && y < icon.rect.bottom) {
                        self->on_toolbar_command(icon.command);
                        break;
                    }
                }
            }
            return 0;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, &toolbar_subclass_proc, id);
            break;
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK listview_subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                   UINT_PTR id, DWORD_PTR ref) noexcept {
        auto* self = reinterpret_cast<WorkbenchWindow*>(ref);
        if (msg == ocm_base + WM_NOTIFY) {
            auto* nmh = reinterpret_cast<NMHDR*>(lp);
            if (nmh != nullptr && nmh->hwndFrom == hwnd && nmh->code == NM_CUSTOMDRAW) {
                auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(lp);
                if (self == nullptr) {
                    return CDRF_DODEFAULT;
                }
                const nfui::ThemePalette& p = self->palette_;
                constexpr int status_col = 1;
                const int dot_size = self->dip(8);
                const int dot_gap = self->dip(6);
                switch (cd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    const bool selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
                    const bool hot = (cd->nmcd.uItemState & CDIS_HOT) != 0;
                    cd->clrText = selected ? p.selection_text.rgb : p.text.rgb;
                    cd->clrTextBk = selected ? p.selection.rgb
                                             : (hot ? p.surface_hover.rgb : p.surface.rgb);
                    return CDRF_NOTIFYSUBITEMDRAW;
                }
                case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
                    if (cd->iSubItem == status_col) {
                        cd->rcText.left += dot_size + dot_gap;
                        if (cd->rcText.left > cd->rcText.right) {
                            cd->rcText.left = cd->rcText.right;
                        }
                    }
                    return CDRF_DODEFAULT;
                }
                case CDDS_SUBITEM | CDDS_ITEMPOSTPAINT: {
                    if (cd->iSubItem == status_col) {
                        self->paint_list_status_dot(cd->nmcd.hdc,
                                                    static_cast<int>(cd->nmcd.dwItemSpec),
                                                    cd->rcText, dot_size);
                    }
                    return CDRF_DODEFAULT;
                }
                default:
                    return CDRF_DODEFAULT;
                }
            }
        }
        if (msg == WM_NCDESTROY) {
            RemoveWindowSubclass(hwnd, &listview_subclass_proc, id);
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK status_subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                 UINT_PTR id, DWORD_PTR ref) noexcept {
        auto* self = reinterpret_cast<WorkbenchWindow*>(ref);
        switch (msg) {
        case WM_PAINT: {
            LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
            if (self != nullptr) {
                self->paint_status_overlay(hwnd);
            }
            return result;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, &status_subclass_proc, id);
            break;
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK progress_subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                   UINT_PTR id, DWORD_PTR ref) noexcept {
        auto* self = reinterpret_cast<WorkbenchWindow*>(ref);
        switch (msg) {
        case WM_PAINT: {
            LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
            if (self != nullptr) {
                self->paint_progress_overlay(hwnd);
            }
            return result;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, &progress_subclass_proc, id);
            break;
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::CommandRouter commands_;
    nfui::Dialog about_;
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::DpiScale dpi_{96};
    nfui::Menu menu_;
    nfui::ThemeMode theme_mode_{nfui::ThemeMode::light};

    nfui::Panel toolbar_panel_;
    nfui::Panel left_panel_;
    nfui::Panel center_panel_;
    nfui::Panel right_panel_;

    nfui::Button theme_light_;
    nfui::Button theme_dark_;
    nfui::Button theme_hc_;

    nfui::StaticText project_header_;
    nfui::Edit search_;
    nfui::TreeView tree_;

    nfui::TabControl tabs_;
    nfui::ListView list_;
    nfui::StaticText list_footer_;

    nfui::StaticText zoom_label_;
    nfui::Slider zoom_slider_;
    nfui::StaticText zoom_value_;
    nfui::StaticText build_label_;
    nfui::ProgressBar build_progress_;
    nfui::StaticText build_pct_;
    nfui::StaticText inspector_text_;

    nfui::Splitter left_splitter_;
    nfui::Splitter right_splitter_;
    nfui::StatusBar status_;

    std::array<ToolbarIcon, 6> toolbar_icons_{};
    std::vector<nfui::Color> status_colors_;
    std::wstring status_text_{L"Ready"};

    int right_separator_y_{0};
    RECT right_magnifier_rect_{};

    HICON large_icon_{};
    HICON small_icon_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    nfui::Application app({instance, show_command});

    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    WorkbenchWindow window(instance);
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}
