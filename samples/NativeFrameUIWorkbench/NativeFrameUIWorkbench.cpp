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
          resources_(instance) {
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
            WS_OVERLAPPEDWINDOW,
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
        case WM_CONTEXTMENU:
            show_context_menu(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
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
    nfui::Edit search_;
    nfui::TreeView tree_;
    nfui::TabControl tabs_;
    nfui::ListView list_;
    nfui::StaticText inspector_;
    nfui::StatusBar status_;
    nfui::ProgressBar progress_;
    nfui::Splitter left_splitter_;
    nfui::Splitter right_splitter_;
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
