#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

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

class WorkbenchWindow final : public nfui::Window {
public:
    explicit WorkbenchWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)) {
        commands_.set_handler(nfui::CommandId{IDM_NFUI_EXIT}, [this](nfui::CommandId) {
            destroy();
            return true;
        });
        commands_.set_handler(nfui::CommandId{command_about}, [this](nfui::CommandId) {
            MessageBoxW(hwnd(),
                        L"NativeFrame UI Workbench\nPure Win32 integration sample.",
                        L"About NativeFrame UI",
                        MB_OK | MB_ICONINFORMATION);
            return true;
        });
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

        HMENU menu = CreateMenu();
        HMENU file_menu = CreatePopupMenu();
        AppendMenuW(file_menu, MF_STRING, IDM_NFUI_EXIT, L"E&xit");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), L"&File");
        HMENU help_menu = CreatePopupMenu();
        AppendMenuW(help_menu, MF_STRING, command_about, L"&About");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help_menu), L"&Help");
        SetMenu(hwnd(), menu);

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
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id_search,
            L"Search",
            0,
            0,
            100,
            24,
        };

        // Bind shared visual dependencies before creating any HWND so every
        // wrapper starts with the same palette and font cache. Native controls
        // still keep their system chrome; self-painted controls consume these
        // pointers directly.
        search_.inject_theme(&palette_, &fonts_);
        tree_.inject_theme(&palette_, &fonts_);
        tabs_.inject_theme(&palette_, &fonts_);
        list_.inject_theme(&palette_, &fonts_);
        inspector_.inject_theme(&palette_, &fonts_);
        status_.inject_theme(&palette_, &fonts_);
        progress_.inject_theme(&palette_, &fonts_);
        left_splitter_.inject_theme(&palette_, &fonts_);
        right_splitter_.inject_theme(&palette_, &fonts_);

        static_cast<void>(search_.create(params));

        params.control_id = id_tree;
        params.text = L"";
        static_cast<void>(tree_.create(params));
        TVINSERTSTRUCTW project_item = tree_item(L"Project");
        TVINSERTSTRUCTW resources_item = tree_item(L"Resources");
        TreeView_InsertItem(tree_.hwnd(), &project_item);
        TreeView_InsertItem(tree_.hwnd(), &resources_item);

        params.control_id = id_tabs;
        static_cast<void>(tabs_.create(params));
        insert_tab(0, L"Workspace");
        insert_tab(1, L"Output");

        params.control_id = id_list;
        // The ListView custom-draw path reads the shared palette injected
        // above, so it is ready before its first paint.
        static_cast<void>(list_.create(params));
        ListView_SetExtendedListViewStyle(list_.hwnd(), LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.pszText = const_cast<wchar_t*>(L"Item");
        column.cx = 220;
        static_cast<void>(ListView_InsertColumn(list_.hwnd(), 0, &column));
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.pszText = const_cast<wchar_t*>(L"NativeFrameUI.lib linked");
        static_cast<void>(ListView_InsertItem(list_.hwnd(), &item));

        params.control_id = id_inspector;
        params.text = L"Inspector: select an item to inspect.";
        static_cast<void>(inspector_.create(params));

        params.control_id = id_progress;
        static_cast<void>(progress_.create(params));
        SendMessageW(progress_.hwnd(), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(progress_.hwnd(), PBM_SETPOS, 35, 0);

        params.control_id = id_splitter_left;
        static_cast<void>(left_splitter_.create(params));

        params.control_id = id_splitter_right;
        static_cast<void>(right_splitter_.create(params));

        params.control_id = id_status;
        static_cast<void>(status_.create(params));
        set_status(L"Ready - NativeFrame UI Workbench");

        // The self-painting inspector and ListView consume the shared palette
        // and font cache injected before creation.

        // Native controls keep their Win32 chrome but adopt Segoe UI so the
        // text matches the shared paint. lParam=TRUE forces an immediate
        // redraw with the new font. Re-applied on DPI changes via
        // apply_native_fonts() so the HFONT tracks the cached face.
        apply_native_fonts();
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
    }

    static TVINSERTSTRUCTW tree_item(const wchar_t* text) {
        TVINSERTSTRUCTW item{};
        item.hParent = TVI_ROOT;
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

        int width = client.right - client.left;
        int height = client.bottom - client.top - status_height;
        int left_width = 250;
        int right_width = 280;
        int splitter_width = 4;
        int top = 8;
        int search_height = 26;
        int margin = 8;

        MoveWindow(search_.hwnd(), margin, top, left_width - margin * 2, search_height, TRUE);
        MoveWindow(tree_.hwnd(), margin, top + search_height + margin, left_width - margin * 2,
                   height - search_height - margin * 3, TRUE);
        MoveWindow(left_splitter_.hwnd(), left_width, 0, splitter_width, height, TRUE);

        int center_left = left_width + splitter_width + margin;
        int center_width = width - left_width - right_width - splitter_width * 2 - margin * 2;
        MoveWindow(tabs_.hwnd(), center_left, top, center_width, 28, TRUE);
        MoveWindow(list_.hwnd(), center_left, top + 34, center_width, height - 74, TRUE);
        MoveWindow(progress_.hwnd(), center_left, height - 32, center_width, 20, TRUE);

        int right_left = width - right_width;
        MoveWindow(right_splitter_.hwnd(), right_left - splitter_width, 0, splitter_width, height, TRUE);
        MoveWindow(inspector_.hwnd(), right_left + margin, top, right_width - margin * 2, height - margin * 2, TRUE);
    }

    void show_context_menu(int x, int y) {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, context_refresh, L"&Refresh");
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, x, y, 0, hwnd(), nullptr);
        DestroyMenu(menu);
    }

    void set_status(const wchar_t* text) {
        if (status_.hwnd() != nullptr) {
            SendMessageW(status_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text));
        }
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::CommandRouter commands_;
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::Edit search_;
    nfui::TreeView tree_;
    nfui::TabControl tabs_;
    nfui::ListView list_;
    nfui::StaticText inspector_;
    nfui::StatusBar status_;
    nfui::ProgressBar progress_;
    nfui::Splitter left_splitter_;
    nfui::Splitter right_splitter_;
    nfui::DpiScale dpi_{96};
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