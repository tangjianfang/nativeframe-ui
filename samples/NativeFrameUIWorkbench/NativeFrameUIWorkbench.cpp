#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

#include <cwchar>

namespace {

constexpr int command_about = 40002;
constexpr int context_refresh = 41001;

constexpr int id_search = 101;
constexpr int id_tree = 102;
constexpr int id_tabs = 103;
constexpr int id_list = 104;
constexpr int id_inspector = 105;
constexpr int id_status = 106;
constexpr int id_progress = 107;
constexpr int id_splitter_left = 108;
constexpr int id_splitter_right = 109;
constexpr int id_slider = 110;
constexpr int id_zoom_label = 111;
constexpr int id_inspector_panel = 112;

// CP31: logical layout constants in DIPs. All on-screen geometry is derived
// from these values through DpiScale::logical_to_pixels() so the workbench
// keeps the same proportions at 96, 144, and 192 DPI.
constexpr int base_margin = 8;
constexpr int base_top = 8;
constexpr int base_left_width = 250;
constexpr int base_right_width = 240;      // narrowed from 280; inspector content is light
constexpr int base_splitter_width = 4;
constexpr int base_search_height = 24;
constexpr int base_tabs_height = 28;
constexpr int base_progress_height = 20;
constexpr int base_zoom_label_height = 20;
constexpr int base_slider_height = 28;

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
        commands_.set_handler(nfui::CommandId{command_about}, [this](nfui::CommandId) {
            // CP15: route the About dialog through the framework's
            // nfui::Dialog wrapper over IDD_NFUI_ABOUT instead of the
            // system MessageBoxW. Native MessageBox chrome bypasses every
            // visual contract in the framework and reads as 1995-era
            // USER32 chrome on Win11; nfui::Dialog honours the same
            // palette and font cache the rest of the workbench uses.
            static_cast<void>(about_.show_modal(instance_,
                                                MAKEINTRESOURCEW(IDD_NFUI_ABOUT),
                                                hwnd(),
                                                &WorkbenchWindow::about_dlg_proc));
            return true;
        });
    }

    static INT_PTR CALLBACK about_dlg_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
        (void)wp;
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

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIWorkbenchWindow",
            L"NativeFrame UI Workbench",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1180,
            760,
        };

        if (!create(params)) {
            return false;
        }

        // CP24-A: build the menu bar through nfui::Menu so the surface
        // chrome inherits palette.surface via MENUINFO instead of the
        // default COLOR_MENU. The fluent builder keeps the construction
        // readable and frees us from the raw CreateMenu / AppendMenuW
        // chain. Each popup() returns a builder that targets the new
        // submenu, so chained .item() calls populate that submenu.
        // CP28-B: apply_to_bar() now takes the host HWND so it can call
        // DrawMenuBar and refresh the menu bar chrome with the freshly
        // installed brush (best-effort nudge on Win10/11 — popup
        // submenus always honor MIM_BACKGROUND; menu bar background is
        // a known Win32 theme limitation).
        menu_.apply_to_bar(hwnd());
        (void)menu_.builder(menu_.bar()).popup(L"&File")
            .item(L"E&xit", IDM_NFUI_EXIT);
        (void)menu_.builder(menu_.bar()).popup(L"&Help")
            .item(L"&About", command_about);
        SetMenu(hwnd(), menu_.bar().get());

        // create_children() -> apply_native_fonts() needs a valid DPI before
        // the HFONT cache lookup. Seed it from the live HWND so the initial
        // WM_SETFONT pass uses the correct scaled face (matches layout()).
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        create_children();
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
            // The cached HFONT handles are DPI-keyed, so re-apply WM_SETFONT
            // after a DPI bump so the native chrome tracks the new face.
            apply_native_fonts();
            layout();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_HSCROLL:
            // CP31: keep the zoom label and status bar in sync with the slider.
            if (reinterpret_cast<HWND>(lparam) == slider_.hwnd()) {
                update_zoom_display();
            }
            return 0;
        case WM_CONTEXTMENU:
            show_context_menu(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        case WM_ERASEBKGND:
            // We paint the cream background in WM_PAINT; suppress the default
            // COLOR_WINDOW erase so the area between native panes is ours.
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC hdc = BeginPaint(hwnd(), &paint);
            RECT client{};
            GetClientRect(hwnd(), &client);
            // Flicker-free offscreen buffer over the full client area. The
            // MemoryDC destructor BitBlts back to the target rect origin while
            // the BeginPaint DC is still valid, so the buffer flush MUST
            // happen before EndPaint (R6 fix from SettingsDemo).
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
        if (notification_code == 0 && commands_.route(nfui::CommandId{command_id})) {
            return true;
        }
        if (command_id == context_refresh) {
            set_status(L"Context refresh command routed.");
            return true;
        }
        return false;
    }

private:
    void create_children() {
        // CP31: all geometry used at creation time is scaled by the live DPI.
        const int margin = dpi_.logical_to_pixels(base_margin);
        const int top = dpi_.logical_to_pixels(base_top);
        const int left_width = dpi_.logical_to_pixels(base_left_width);
        const int right_width = dpi_.logical_to_pixels(base_right_width);

        RECT client{};
        GetClientRect(hwnd(), &client);
        const int width = client.right - client.left;
        const int right_left = width - right_width;

        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id_search,
            L"Search",
            margin,
            top,
            left_width - margin * 2,
            dpi_.logical_to_pixels(base_search_height),
        };

        // Bind shared visual dependencies before creating any HWND so every
        // wrapper starts with the same palette and font cache. Native controls
        // still keep their system chrome; self-painted controls consume these
        // pointers directly.
        search_.inject_theme(&palette_, &fonts_);
        tree_.inject_theme(&palette_, &fonts_);
        tabs_.inject_theme(&palette_, &fonts_);
        list_.inject_theme(&palette_, &fonts_);
        inspector_panel_.inject_theme(&palette_, &fonts_);
        inspector_label_.inject_theme(&palette_, &fonts_);
        status_.inject_theme(&palette_, &fonts_);
        progress_.inject_theme(&palette_, &fonts_);
        slider_.inject_theme(&palette_, &fonts_);
        zoom_label_.inject_theme(&palette_, &fonts_);
        left_splitter_.inject_theme(&palette_, &fonts_);
        right_splitter_.inject_theme(&palette_, &fonts_);

        static_cast<void>(search_.create(params));

        params.control_id = id_tree;
        params.text = L"";
        static_cast<void>(tree_.create(params));
        TVINSERTSTRUCTW project_item = tree_item(L"Project");
        HTREEITEM h_project = TreeView_InsertItem(tree_.hwnd(), &project_item);
        TVINSERTSTRUCTW resources_item = tree_item(L"Resources");
        TreeView_InsertItem(tree_.hwnd(), &resources_item);
        if (h_project != nullptr) {
            const wchar_t* project_files[] = {
                L"src/core",
                L"src/theme",
                L"src/controls",
                L"include/nfui",
                L"CMakeLists.txt",
                L"README.md",
            };
            for (const wchar_t* file : project_files) {
                TVINSERTSTRUCTW child = tree_item(file, h_project);
                static_cast<void>(TreeView_InsertItem(tree_.hwnd(), &child));
            }
            TreeView_Expand(tree_.hwnd(), h_project, TVE_EXPAND);
        }

        params.control_id = id_tabs;
        static_cast<void>(tabs_.create(params));
        // CP8A: explicitly widen the horizontal padding so the tabs read as
        // deliberate segments (matches the 12-DIP gallery rhythm). Native v6
        // default would be (6, 3); we use ~ (12, 4) post-DPI scaling.
        static_cast<void>(tabs_.set_padding(dpi_.logical_to_pixels(12), dpi_.logical_to_pixels(4)));
        insert_tab(0, L"Workspace");
        insert_tab(1, L"Output");

        params.control_id = id_list;
        // The ListView custom-draw path reads the shared palette injected
        // above, so it is ready before its first paint.
        static_cast<void>(list_.create(params));
        ListView_SetExtendedListViewStyle(list_.hwnd(), LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.pszText = const_cast<wchar_t*>(L"Name");
        column.cx = dpi_.logical_to_pixels(180);
        static_cast<void>(ListView_InsertColumn(list_.hwnd(), 0, &column));
        column.pszText = const_cast<wchar_t*>(L"Type");
        column.cx = dpi_.logical_to_pixels(80);
        static_cast<void>(ListView_InsertColumn(list_.hwnd(), 1, &column));
        column.pszText = const_cast<wchar_t*>(L"Status");
        column.cx = dpi_.logical_to_pixels(90);
        static_cast<void>(ListView_InsertColumn(list_.hwnd(), 2, &column));
        add_list_row(0, L"NativeFrameUI.hpp", L"Header", L"Included");
        add_list_row(1, L"Application.cpp", L"Source", L"Modified");
        add_list_row(2, L"CMakeLists.txt", L"Build", L"Tracked");
        add_list_row(3, L"SettingsDemo.exe", L"Sample", L"Generated");
        add_list_row(4, L"Workbench-light.png", L"Asset", L"Current");
        list_item_count_ = 5;

        params.control_id = id_zoom_label;
        params.text = L"Zoom: 100%";
        params.x = right_left + margin;
        params.y = top;
        params.width = right_width - margin * 2;
        params.height = dpi_.logical_to_pixels(base_zoom_label_height);
        static_cast<void>(zoom_label_.create(params));
        {
            nfui::TextStyle style{};
            style.foreground = palette_.text;
            style.align_h = nfui::StaticTextAlignH::left;
            style.align_v = nfui::StaticTextAlignV::middle;
            zoom_label_.set_style(style);
        }

        params.control_id = id_slider;
        params.text = L"";
        params.y = top + dpi_.logical_to_pixels(base_zoom_label_height) + margin;
        params.width = right_width - margin * 2;
        params.height = dpi_.logical_to_pixels(base_slider_height);
        static_cast<void>(slider_.create(params));
        slider_.set_range(50, 200);
        slider_.set_pos(100);

        // CP31: inspector card is created before its caption label so the
        // caption sits on top of the panel in the default z-order.
        params.control_id = id_inspector_panel;
        params.text = L"";
        params.y = top + dpi_.logical_to_pixels(base_zoom_label_height)
                       + dpi_.logical_to_pixels(base_slider_height) + margin * 2;
        params.width = right_width - margin * 2;
        params.height = dpi_.logical_to_pixels(120);
        static_cast<void>(inspector_panel_.create(params));
        {
            nfui::FrameStyle style{};
            style.surface_brush = palette_.surface;
            style.accent = palette_.border;
            style.elevation = 2;
            inspector_panel_.set_style(style);
        }

        params.control_id = id_inspector;
        params.text = L"Select a file to inspect its properties.";
        static_cast<void>(inspector_label_.create(params));
        {
            nfui::TextStyle style{};
            style.background = palette_.surface;
            style.foreground = palette_.text_secondary;
            style.align_h = nfui::StaticTextAlignH::center;
            style.align_v = nfui::StaticTextAlignV::middle;
            style.single_line = true;
            inspector_label_.set_style(style);
        }

        params.control_id = id_progress;
        static_cast<void>(progress_.create(params));
        progress_.set_show_percentage(true);
        SendMessageW(progress_.hwnd(), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        progress_value_ = 40;
        SendMessageW(progress_.hwnd(), PBM_SETPOS, progress_value_, 0);

        params.control_id = id_splitter_left;
        static_cast<void>(left_splitter_.create(params));

        params.control_id = id_splitter_right;
        static_cast<void>(right_splitter_.create(params));

        params.control_id = id_status;
        static_cast<void>(status_.create(params));
        refresh_status();

        // The self-painting inspector and ListView consume the shared palette
        // and font cache injected before creation.

        // Native controls keep their Win32 chrome but adopt Segoe UI so the
        // text matches the shared paint. lParam=TRUE forces an immediate
        // redraw with the new font. Re-applied on DPI changes via
        // apply_native_fonts() so the HFONT tracks the cached face.
        apply_native_fonts();
    }

    void add_list_row(int row, const wchar_t* name, const wchar_t* type, const wchar_t* status) noexcept {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = const_cast<wchar_t*>(name);
        static_cast<void>(ListView_InsertItem(list_.hwnd(), &item));
        ListView_SetItemText(list_.hwnd(), row, 1, const_cast<wchar_t*>(type));
        ListView_SetItemText(list_.hwnd(), row, 2, const_cast<wchar_t*>(status));
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font = fonts_.regular(dpi_value, 9);
        SendMessageW(search_.hwnd(),       WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(tree_.hwnd(),         WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(tabs_.hwnd(),         WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(list_.hwnd(),         WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(progress_.hwnd(),     WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(status_.hwnd(),       WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(zoom_label_.hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        // CP22: inspector_label_ is a self-painting StaticText so it does not need
        // WM_SETFONT for the text itself, but forcing the cached Segoe UI face
        // keeps any fallback native pass in the same family as the siblings.
        SendMessageW(inspector_label_.hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
    }

    static TVINSERTSTRUCTW tree_item(const wchar_t* text, HTREEITEM parent = TVI_ROOT) {
        TVINSERTSTRUCTW item{};
        item.hParent = parent;
        item.hInsertAfter = TVI_LAST;
        item.item.mask = TVIF_TEXT;
        item.item.pszText = const_cast<wchar_t*>(text);
        return item;
    }

    void insert_tab(int index, const wchar_t* text) {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<wchar_t*>(text);
        static_cast<void>(TabCtrl_InsertItem(tabs_.hwnd(), index, &item));
    }

    void layout() noexcept {
        if (hwnd() == nullptr || status_.hwnd() == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        SendMessageW(status_.hwnd(), WM_SIZE, 0, 0);

        RECT status_rect{};
        GetWindowRect(status_.hwnd(), &status_rect);
        int status_height = status_rect.bottom - status_rect.top;

        const int margin = dpi_.logical_to_pixels(base_margin);
        const int top = dpi_.logical_to_pixels(base_top);
        const int left_width = dpi_.logical_to_pixels(base_left_width);
        const int right_width = dpi_.logical_to_pixels(base_right_width);
        const int splitter_width = dpi_.logical_to_pixels(base_splitter_width);
        const int search_height = dpi_.logical_to_pixels(base_search_height);
        const int tabs_height = dpi_.logical_to_pixels(base_tabs_height);
        const int progress_height = dpi_.logical_to_pixels(base_progress_height);
        const int zoom_label_height = dpi_.logical_to_pixels(base_zoom_label_height);
        const int slider_height = dpi_.logical_to_pixels(base_slider_height);

        int width = client.right - client.left;
        int height = client.bottom - client.top - status_height;

        MoveWindow(search_.hwnd(), margin, top, left_width - margin * 2, search_height, TRUE);
        MoveWindow(tree_.hwnd(), margin, top + search_height + margin, left_width - margin * 2,
                   height - search_height - margin * 3, TRUE);
        MoveWindow(left_splitter_.hwnd(), left_width, 0, splitter_width, height, TRUE);

        int center_left = left_width + splitter_width + margin;
        int center_width = width - left_width - right_width - splitter_width * 2 - margin * 2;
        int center_bottom = height - margin;
        int progress_y = center_bottom - progress_height;
        int list_y = top + tabs_height + margin;
        MoveWindow(tabs_.hwnd(), center_left, top, center_width, tabs_height, TRUE);
        MoveWindow(list_.hwnd(), center_left, list_y, center_width,
                   std::max(0, progress_y - list_y - margin), TRUE);
        MoveWindow(progress_.hwnd(), center_left, progress_y, center_width, progress_height, TRUE);

        int right_left = width - right_width;
        MoveWindow(right_splitter_.hwnd(), right_left - splitter_width, 0, splitter_width, height, TRUE);

        int zoom_label_y = top;
        int slider_y = zoom_label_y + zoom_label_height + margin;
        MoveWindow(zoom_label_.hwnd(), right_left + margin, zoom_label_y,
                   right_width - margin * 2, zoom_label_height, TRUE);
        MoveWindow(slider_.hwnd(), right_left + margin, slider_y,
                   right_width - margin * 2, slider_height, TRUE);

        // CP31: inspector card — rounded panel (elevation 2) with a centred
        // caption overlaid on top. The label is inset by one margin so the
        // panel's rounded corners and hairline border remain visible.
        int inspector_y = slider_y + slider_height + margin;
        int inspector_x = right_left + margin;
        int inspector_w = right_width - margin * 2;
        int inspector_h = std::max(0, height - margin - inspector_y);
        MoveWindow(inspector_panel_.hwnd(), inspector_x, inspector_y, inspector_w, inspector_h, TRUE);
        int label_inset = margin;
        MoveWindow(inspector_label_.hwnd(), inspector_x + label_inset, inspector_y + label_inset,
                   std::max(0, inspector_w - label_inset * 2),
                   std::max(0, inspector_h - label_inset * 2), TRUE);
    }

    void show_context_menu(int x, int y) {
        // CP24-A: build a themed context popup through nfui::Menu so the
        // menu surface carries the host palette. The popup is local — the
        // OS dismisses it on click or escape and we destroy it after
        // TrackPopupMenu returns.
        nfui::OwnedMenu popup = menu_.make_popup();
        (void)menu_.builder(popup).item(L"&Refresh", context_refresh);
        TrackPopupMenu(popup.get(), TPM_RIGHTBUTTON, x, y, 0, hwnd(), nullptr);
    }

    void set_status(const wchar_t* text) noexcept {
        if (status_.hwnd() != nullptr) {
            SendMessageW(status_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text));
        }
    }

    void refresh_status() noexcept {
        wchar_t text[128]{};
        (void)swprintf_s(text, sizeof(text) / sizeof(text[0]),
                         L"Ready — %d items    Zoom %d%%    Indexing... %d%%",
                         list_item_count_, slider_.pos(), progress_value_);
        set_status(text);
    }

    void update_zoom_display() noexcept {
        wchar_t label[32]{};
        (void)swprintf_s(label, sizeof(label) / sizeof(label[0]), L"Zoom: %d%%", slider_.pos());
        if (zoom_label_.hwnd() != nullptr) {
            SetWindowTextW(zoom_label_.hwnd(), label);
        }
        refresh_status();
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::CommandRouter commands_;
    nfui::Dialog about_{};
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::Menu menu_;
    nfui::Edit search_;
    nfui::TreeView tree_;
    nfui::TabControl tabs_;
    nfui::ListView list_;
    nfui::StaticText inspector_label_;
    nfui::Panel inspector_panel_;
    nfui::StatusBar status_;
    nfui::ProgressBar progress_;
    nfui::Slider slider_;
    nfui::StaticText zoom_label_;
    nfui::Splitter left_splitter_;
    nfui::Splitter right_splitter_;
    nfui::DpiScale dpi_{96};
    int list_item_count_{0};
    int progress_value_{0};
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
