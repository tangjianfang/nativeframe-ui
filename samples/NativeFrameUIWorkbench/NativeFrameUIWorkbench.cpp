#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

#include <cwchar>
#include <string_view>

namespace {

constexpr int command_about = 40002;
constexpr int context_refresh = 41001;

// Menu commands routed by the CommandRouter. The toolbar buttons and the menu
// share these IDs so a click on "New file" and File > New both flow through the
// same handler. Keeping them in one place avoids command-id drift.
constexpr int idm_new        = 42001;
constexpr int idm_open       = 42002;
constexpr int idm_save       = 42003;
constexpr int idm_run        = 42004;
constexpr int idm_debug      = 42005;
constexpr int idm_search     = 42006;

constexpr int id_search          = 101;
constexpr int id_tree            = 102;
constexpr int id_tabs            = 103;
constexpr int id_list            = 104;
constexpr int id_status          = 105;
constexpr int id_splitter_left   = 106;
constexpr int id_splitter_right  = 107;
constexpr int id_slider          = 108;
constexpr int id_inspector_panel = 109;

// CP31: logical layout constants in DIPs. All on-screen geometry is derived
// from these values through DpiScale::logical_to_pixels() so the workbench
// keeps the same proportions at 96, 144, and 192 DPI.
//
// Layout (top → down):
//   menu bar (OS)   y = 0..~20
//   toolbar strip   y = base_toolbar_top..base_toolbar_top + base_toolbar_height
//   PROJECT label   y = base_top..base_top + base_project_label_h
//   search row      y = base_search_top..base_search_top + base_search_height
//   tree            y = base_search_top + base_search_height + base_margin..bottom
//   right pane      y = base_search_top..bottom (synchronized with the search row)
constexpr int base_margin           = 8;
constexpr int base_toolbar_top      = 8;    // top of icon strip (just under menu)
constexpr int base_toolbar_height   = 36;   // icon strip beneath the menu
constexpr int base_top              = 56;   // top of PROJECT label / left-pane content
constexpr int base_project_label_h  = 24;   // PROJECT header strip
constexpr int base_left_width       = 250;
constexpr int base_right_width      = 240;
constexpr int base_splitter_width   = 4;
constexpr int base_search_height    = 28;
constexpr int base_tabs_height      = 28;
constexpr int base_zoom_label_h     = 20;
constexpr int base_slider_height    = 28;
constexpr int base_build_panel_h    = 92;   // rounded card under the slider
constexpr int base_toolbar_button   = 28;
constexpr int base_toolbar_gap      = 4;
constexpr int base_toolbar_group    = 12;

// CP28: status dot text codes used to colour the dot rendered before each
// list row's status column. The leading "● " glyph is plain text — the colour
// comes from the NM_CUSTOMDRAW path that drives fill_ellipse for the dot.
constexpr std::wstring_view kStatusOk      = L"\x25CF ready";
constexpr std::wstring_view kStatusOkAlt   = L"\x25CF linked";
constexpr std::wstring_view kStatusOkDone  = L"\x25CF compiled";
constexpr std::wstring_view kStatusPending = L"\x25CF pending";

// Toolbar button indices for the icon strip beneath the menu bar. The index
// matches the IconKind assigned in toolbar_icon(); hit-testing and click
// handling both key off this enum.
enum ToolbarButton {
    TB_NEW = 0,
    TB_OPEN,
    TB_SAVE,
    TB_RUN,
    TB_DEBUG,
    TB_SEARCH,
    TB_COUNT,
};

class WorkbenchWindow final : public nfui::Window {
public:
    explicit WorkbenchWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)),
          menu_(palette_) {
        // CP28: route every menu + toolbar command through a single hub so a
        // click on the toolbar's "New" button and File > New both flow
        // through the same handler. status_ is null at construction time;
        // the handlers reach it through WorkbenchWindow* captured by-value.
        register_command_handlers();
    }

    ~WorkbenchWindow() noexcept override {
        // Remove the search-edit subclass so SetWindowSubclass's reference
        // count doesn't leak if the HWND outlives the wrapper.
        if (search_.hwnd() != nullptr) {
            RemoveWindowSubclass(search_.hwnd(),
                                 &WorkbenchWindow::search_subclass_proc,
                                 reinterpret_cast<UINT_PTR>(this));
        }
        if (tabs_.hwnd() != nullptr) {
            RemoveWindowSubclass(tabs_.hwnd(),
                                 &WorkbenchWindow::tabs_subclass_proc,
                                 reinterpret_cast<UINT_PTR>(this));
        }
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

    // CP28: search-edit subclass. The native Edit paints its own client area;
    // we let it run, then overlay a magnifier glyph on top so the search box
    // reads as a polished surface (matches the reference's left-aligned icon
    // inside the field). The subclass runs after DefSubclassProc so the
    // native paint is fully flushed by the time we issue GetDC.
    static LRESULT CALLBACK search_subclass_proc(HWND hwnd, UINT msg, WPARAM w,
                                                  LPARAM l, UINT_PTR id, DWORD_PTR ref) {
        auto* self = reinterpret_cast<WorkbenchWindow*>(ref);
        switch (msg) {
        case WM_PAINT: {
            const LRESULT res = DefSubclassProc(hwnd, msg, w, l);
            if (self != nullptr) {
                self->paint_search_overlay(hwnd);
            }
            return res;
        }
        case WM_NCDESTROY: {
            RemoveWindowSubclass(hwnd, &WorkbenchWindow::search_subclass_proc, id);
            return DefSubclassProc(hwnd, msg, w, l);
        }
        default:
            break;
        }
        return DefSubclassProc(hwnd, msg, w, l);
    }

    // CP32: inspector-panel subclass. The Panel paints its own surface, so
    // any overlay drawn on the parent DC is overwritten when the child
    // repaints. Subclass so we draw the magnifier + caption INSIDE the
    // panel's WM_PAINT cycle (after Panel::on_paint fills the rounded card).
    static LRESULT CALLBACK inspector_subclass_proc(HWND hwnd, UINT msg, WPARAM w,
                                                     LPARAM l, UINT_PTR id, DWORD_PTR ref) {
        auto* self = reinterpret_cast<WorkbenchWindow*>(ref);
        switch (msg) {
        case WM_PAINT: {
            const LRESULT res = DefSubclassProc(hwnd, msg, w, l);
            if (self != nullptr) {
                self->paint_inspector_overlay(hwnd);
            }
            return res;
        }
        case WM_NCDESTROY: {
            RemoveWindowSubclass(hwnd, &WorkbenchWindow::inspector_subclass_proc, id);
            return DefSubclassProc(hwnd, msg, w, l);
        }
        default:
            break;
        }
        return DefSubclassProc(hwnd, msg, w, l);
    }

    // CP32: tabs subclass. After the native TabControl paints, query the
    // currently selected tab and overlay a 2 device-px coral bar at the
    // bottom of its rect (matches the reference design — the coral stripe
    // signals the active tab). Coords from TabCtrl_GetItemRect are in the
    // tabs HWND's client space, so a GetDC on that HWND lines up directly.
    static LRESULT CALLBACK tabs_subclass_proc(HWND hwnd, UINT msg, WPARAM w,
                                                LPARAM l, UINT_PTR id, DWORD_PTR ref) {
        auto* self = reinterpret_cast<WorkbenchWindow*>(ref);
        switch (msg) {
        case WM_PAINT: {
            const LRESULT res = DefSubclassProc(hwnd, msg, w, l);
            if (self != nullptr) {
                self->paint_tabs_highlight(hwnd);
            }
            return res;
        }
        case WM_NCDESTROY: {
            RemoveWindowSubclass(hwnd, &WorkbenchWindow::tabs_subclass_proc, id);
            return DefSubclassProc(hwnd, msg, w, l);
        }
        default:
            break;
        }
        return DefSubclassProc(hwnd, msg, w, l);
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

        menu_.apply_to_bar(hwnd());
        // CP28: extend the menu to match the reference (File / Edit / View /
        // Run / Window / Help). Each popup() returns a builder targeting the
        // new submenu; the chained .item() calls populate that submenu.
        (void)menu_.builder(menu_.bar()).popup(L"&File")
            .item(L"&New",   idm_new)
            .item(L"&Open",  idm_open)
            .item(L"&Save",  idm_save)
            .separator()
            .item(L"E&xit",  IDM_NFUI_EXIT);
        (void)menu_.builder(menu_.bar()).popup(L"&Edit")
            .item(L"&Undo", idm_new)
            .item(L"&Redo", idm_open)
            .separator()
            .item(L"Cu&t",  idm_save)
            .item(L"&Copy", idm_run)
            .item(L"&Paste",idm_debug);
        (void)menu_.builder(menu_.bar()).popup(L"&View")
            .item(L"&Zoom In",  idm_search)
            .item(L"Zoom &Out", idm_new)
            .separator()
            .item(L"&Reset",    idm_open);
        (void)menu_.builder(menu_.bar()).popup(L"&Run")
            .item(L"&Run",   idm_run)
            .item(L"&Debug", idm_debug);
        (void)menu_.builder(menu_.bar()).popup(L"&Window")
            .item(L"&Cascade",      idm_new)
            .item(L"&Tile",         idm_open)
            .separator()
            .item(L"Close &All",    idm_save);
        (void)menu_.builder(menu_.bar()).popup(L"&Help")
            .item(L"&About", command_about);
        SetMenu(hwnd(), menu_.bar().get());

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        create_children();
        layout();
        refresh_status();

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
            apply_search_margins();
            layout();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(lparam) == slider_.hwnd()) {
                update_zoom_display();
                invalidate_right_pane();
            }
            return 0;
        case WM_CONTEXTMENU:
            show_context_menu(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            const int hit = hit_test_toolbar(x, y);
            if (hit != hovered_toolbar_) {
                hovered_toolbar_ = hit;
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            if (!tracking_toolbar_) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd(), 0};
                TrackMouseEvent(&tme);
                tracking_toolbar_ = true;
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_toolbar_ = false;
            if (hovered_toolbar_ != -1) {
                hovered_toolbar_ = -1;
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN: {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            const int hit = hit_test_toolbar(x, y);
            if (hit >= 0) {
                on_toolbar_click(hit);
            }
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
                paint_chrome(target, client);
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
            set_status(L"\x25CF Context refresh command routed.");
            return true;
        }
        return false;
    }

private:
    void register_command_handlers() noexcept {
        commands_.set_handler(nfui::CommandId{IDM_NFUI_EXIT}, [this](nfui::CommandId) {
            destroy();
            return true;
        });
        commands_.set_handler(nfui::CommandId{command_about}, [this](nfui::CommandId) {
            static_cast<void>(about_.show_modal(instance_,
                                                MAKEINTRESOURCEW(IDD_NFUI_ABOUT),
                                                hwnd(),
                                                &WorkbenchWindow::about_dlg_proc));
            return true;
        });
        auto route_toolbar = [this](std::wstring_view label) {
            set_status(label);
        };
        commands_.set_handler(nfui::CommandId{idm_new},
            [route_toolbar](nfui::CommandId) { route_toolbar(L"\x25CF New file command routed."); return true; });
        commands_.set_handler(nfui::CommandId{idm_open},
            [route_toolbar](nfui::CommandId) { route_toolbar(L"\x25CF Open command routed."); return true; });
        commands_.set_handler(nfui::CommandId{idm_save},
            [route_toolbar](nfui::CommandId) { route_toolbar(L"\x25CF Save command routed."); return true; });
        commands_.set_handler(nfui::CommandId{idm_run},
            [route_toolbar](nfui::CommandId) { route_toolbar(L"\x25CF Run command routed."); return true; });
        commands_.set_handler(nfui::CommandId{idm_debug},
            [route_toolbar](nfui::CommandId) { route_toolbar(L"\x25CF Debug command routed."); return true; });
        commands_.set_handler(nfui::CommandId{idm_search},
            [route_toolbar](nfui::CommandId) { route_toolbar(L"\x25CF Search command routed."); return true; });
    }

    void create_children() {
        // CP31: all geometry used at creation time is scaled by the live DPI.
        const int margin       = dpi_.logical_to_pixels(base_margin);
        const int top          = dpi_.logical_to_pixels(base_top);
        const int project_h    = dpi_.logical_to_pixels(base_project_label_h);
        // CP32: search/tree/tabs/slider all start below the PROJECT label.
        const int search_top   = top + project_h + margin;
        const int left_width   = dpi_.logical_to_pixels(base_left_width);
        const int right_width  = dpi_.logical_to_pixels(base_right_width);

        RECT client{};
        GetClientRect(hwnd(), &client);
        const int width = client.right - client.left;
        const int right_left = width - right_width;

        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id_search,
            L"",
            margin,
            search_top,
            left_width - margin * 2,
            dpi_.logical_to_pixels(base_search_height),
        };

        search_.inject_theme(&palette_, &fonts_);
        tree_.inject_theme(&palette_, &fonts_);
        tabs_.inject_theme(&palette_, &fonts_);
        list_.inject_theme(&palette_, &fonts_);
        inspector_panel_.inject_theme(&palette_, &fonts_);
        status_.inject_theme(&palette_, &fonts_);
        slider_.inject_theme(&palette_, &fonts_);
        left_splitter_.inject_theme(&palette_, &fonts_);
        right_splitter_.inject_theme(&palette_, &fonts_);

        static_cast<void>(search_.create(params));
        // CP28: install our subclass AFTER the framework's visual_subclass
        // proc so we run first on WM_PAINT (SetWindowSubclass dispatches in
        // reverse install order). The chain is base < framework < ours;
        // ours wins the first crack at WM_PAINT, hands off to DefSubclassProc
        // (framework), then draws the magnifier overlay on top.
        SetWindowSubclass(search_.hwnd(),
                          &WorkbenchWindow::search_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this));
        // Placeholder "Search files..." — wParam = TRUE so the cue draws with
        // focus-style colour even when the edit has focus. Combined with
        // EM_SETMARGINS below, text never overlaps the magnifier glyph.
        SendMessageW(search_.hwnd(), EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(L"Search files..."));
        apply_search_margins();

        params.control_id = id_tree;
        params.text = L"";
        static_cast<void>(tree_.create(params));
        // CP28: rebuild the tree contents to match the reference (PROJECT >
        // src/ > main.cpp / App.cpp / App.h + resources / tests / docs).
        populate_tree();

        params.control_id = id_tabs;
        static_cast<void>(tabs_.create(params));
        static_cast<void>(tabs_.set_padding(dpi_.logical_to_pixels(12), dpi_.logical_to_pixels(4)));
        insert_tab(0, L"Workspace");
        insert_tab(1, L"Output");
        // CP32: install a paint subclass so we can overlay a 2 device-px
        // coral bar at the bottom of the currently selected tab. Subclass
        // AFTER the framework's chrome subclass so we chain through
        // DefSubclassProc (which triggers the framework's tab paint) and
        // then draw the accent stripe on top.
        SetWindowSubclass(tabs_.hwnd(),
                          &WorkbenchWindow::tabs_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this));

        params.control_id = id_list;
        static_cast<void>(list_.create(params));
        ListView_SetExtendedListViewStyle(list_.hwnd(),
                                          LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        populate_list_columns();
        populate_list_rows();
        // CP28: install an item-paint subclass so we can draw a coloured
        // dot (green/amber) ahead of each status column's text. Subclass
        // AFTER the framework's chrome subclass; ours runs first and chains
        // through DefSubclassProc so the framework's row chrome (text colour,
        // selection, hover) keeps working.
        SetWindowSubclass(list_.hwnd(),
                          &WorkbenchWindow::list_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this));

        params.control_id = id_slider;
        params.text = L"";
        params.x = right_left + margin;
        params.y = search_top + dpi_.logical_to_pixels(base_zoom_label_h) + margin;
        params.width = right_width - margin * 2;
        params.height = dpi_.logical_to_pixels(base_slider_height);
        static_cast<void>(slider_.create(params));
        slider_.set_range(50, 200);
        slider_.set_pos(60);   // CP28: matches the reference's "60%" reading

        params.control_id = id_inspector_panel;
        params.text = L"";
        // CP28: place the inspector card under the build panel so the right
        // pane reads top-to-bottom as Zoom slider > Build progress > Inspector.
        const int build_y = search_top + dpi_.logical_to_pixels(base_zoom_label_h)
                             + dpi_.logical_to_pixels(base_slider_height) + margin * 2;
        const int inspector_y = build_y + dpi_.logical_to_pixels(base_build_panel_h) + margin;
        params.x = right_left + margin;
        params.y = inspector_y;
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
        // CP32: install an inspector subclass so we paint the magnifier
        // glyph + caption INSIDE the panel's own WM_PAINT cycle (after
        // Panel::on_paint fills the surface). Painting on the parent DC
        // leaves the overlay invisible — the child repaints its background
        // and erases the chrome-layer text. Subclass runs after the
        // framework's chrome subclass so it chains through DefSubclassProc
        // (which triggers on_paint via WM_PAINT), then overlays icon + text.
        SetWindowSubclass(inspector_panel_.hwnd(),
                          &WorkbenchWindow::inspector_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this));

        params.control_id = id_splitter_left;
        static_cast<void>(left_splitter_.create(params));

        params.control_id = id_splitter_right;
        static_cast<void>(right_splitter_.create(params));

        params.control_id = id_status;
        static_cast<void>(status_.create(params));

        apply_native_fonts();
    }

    void apply_search_margins() noexcept {
        if (search_.hwnd() == nullptr) {
            return;
        }
        // CP28: reserve horizontal padding inside the Edit so the user's
        // text never overlaps the magnifier overlay. The left margin has to
        // clear the icon + its inner gap; the right margin is a small cosmetic
        // pad matching the reference's inset.
        const int left_margin  = dpi_.logical_to_pixels(26);
        const int right_margin = dpi_.logical_to_pixels(8);
        SendMessageW(search_.hwnd(), EM_SETMARGINS,
                     EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(left_margin, right_margin));
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT sm_font    = fonts_.regular(dpi_value, nfui::font_pt::sm);
        const HFONT base_font  = fonts_.regular(dpi_value, nfui::font_pt::base);
        const HFONT header_font = fonts_.semibold(dpi_value, nfui::font_pt::lg);
        // CP28: nav/list rows use sm (13), search bar + inspector copy use
        // base (14), and the section headers ("PROJECT", "Build") use lg (20).
        // The framework's font cache is DPI-keyed, so re-apply after every
        // WM_DPICHANGED to keep the native chrome on the new face.
        SendMessageW(search_.hwnd(),       WM_SETFONT, reinterpret_cast<WPARAM>(base_font),  TRUE);
        SendMessageW(tree_.hwnd(),         WM_SETFONT, reinterpret_cast<WPARAM>(sm_font),    TRUE);
        SendMessageW(tabs_.hwnd(),         WM_SETFONT, reinterpret_cast<WPARAM>(header_font), TRUE);
        SendMessageW(list_.hwnd(),         WM_SETFONT, reinterpret_cast<WPARAM>(sm_font),    TRUE);
        SendMessageW(status_.hwnd(),       WM_SETFONT, reinterpret_cast<WPARAM>(sm_font),    TRUE);
        SendMessageW(slider_.hwnd(),       WM_SETFONT, reinterpret_cast<WPARAM>(sm_font),    TRUE);
    }

    void populate_tree() noexcept {
        // CP28: root folder name + one expanded folder with three child
        // files plus two collapsed folders at the bottom. The reference
        // shows the tree system image list is engaged on Vista+ via the
        // standard system ImageList, which renders folder glyphs out of
        // the box; we rely on that here so no NM_CUSTOMDRAW icon
        // bookkeeping is needed for the file-type icons.
        TVINSERTSTRUCTW root = tree_item(L"PROJECT", TVI_ROOT, true);
        HTREEITEM h_root = TreeView_InsertItem(tree_.hwnd(), &root);

        TVINSERTSTRUCTW src = tree_item(L"src", h_root, true);
        HTREEITEM h_src = TreeView_InsertItem(tree_.hwnd(), &src);
        TVINSERTSTRUCTW main_cpp = tree_item(L"main.cpp", h_src, false);
        TreeView_InsertItem(tree_.hwnd(), &main_cpp);
        TVINSERTSTRUCTW app_cpp = tree_item(L"App.cpp",  h_src, false);
        TreeView_InsertItem(tree_.hwnd(), &app_cpp);
        TVINSERTSTRUCTW app_h = tree_item(L"App.h",    h_src, false);
        TreeView_InsertItem(tree_.hwnd(), &app_h);

        TVINSERTSTRUCTW tests = tree_item(L"tests", h_root, true);
        TreeView_InsertItem(tree_.hwnd(), &tests);
        TVINSERTSTRUCTW docs = tree_item(L"docs",  h_root, true);
        TreeView_InsertItem(tree_.hwnd(), &docs);

        TreeView_Expand(tree_.hwnd(), h_root, TVE_EXPAND);
        TreeView_Expand(tree_.hwnd(), h_src,  TVE_EXPAND);
    }

    void populate_list_columns() noexcept {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.pszText = const_cast<wchar_t*>(L"Item");
        column.cx = dpi_.logical_to_pixels(220);
        static_cast<void>(ListView_InsertColumn(list_.hwnd(), 0, &column));
        column.pszText = const_cast<wchar_t*>(L"Status");
        column.cx = dpi_.logical_to_pixels(120);
        static_cast<void>(ListView_InsertColumn(list_.hwnd(), 1, &column));
    }

    void populate_list_rows() noexcept {
        // CP28: rows mirror the reference's build artefact table. The Status
        // column carries a Unicode bullet (●) prefix so the NM_CUSTOMDRAW
        // hook can detect it and replace the glyph with a coloured
        // fill_ellipse dot — see list_subclass_proc.
        struct Row { const wchar_t* name; std::wstring_view status; };
        const Row rows[] = {
            { L"NativeFrameUI.lib",  kStatusOk      },
            { L"NativeFrameUI.dll",  kStatusOkAlt   },
            { L"ChartView.obj",      kStatusOkDone  },
            { L"main.obj",           kStatusOkDone  },
            { L"tests.exe",          kStatusPending },
        };
        list_item_count_ = static_cast<int>(std::size(rows));
        for (int i = 0; i < list_item_count_; ++i) {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = i;
            item.iSubItem = 0;
            item.pszText = const_cast<wchar_t*>(rows[i].name);
            static_cast<void>(ListView_InsertItem(list_.hwnd(), &item));
            wchar_t text[32]{};
            (void)swprintf_s(text, L"%.*s",
                             static_cast<int>(rows[i].status.size()),
                             rows[i].status.data());
            ListView_SetItemText(list_.hwnd(), i, 1, text);
        }
    }

    void insert_tab(int index, const wchar_t* text) {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<wchar_t*>(text);
        static_cast<void>(TabCtrl_InsertItem(tabs_.hwnd(), index, &item));
    }

    static TVINSERTSTRUCTW tree_item(const wchar_t* text, HTREEITEM parent, bool is_folder) {
        TVINSERTSTRUCTW item{};
        item.hParent = parent;
        item.hInsertAfter = TVI_LAST;
        item.item.mask = TVIF_TEXT;
        item.item.pszText = const_cast<wchar_t*>(text);
        // CP28: tag folders so the toolbar's "New file" semantic can later
        // distinguish them from files. Reserved via cChildren — we don't
        // ship a fancy file-type icon overlay yet (see header for the
        // rationale).
        item.item.cChildren = is_folder ? 1 : 0;
        return item;
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

        const int margin          = dpi_.logical_to_pixels(base_margin);
        const int top             = dpi_.logical_to_pixels(base_top);
        const int project_h       = dpi_.logical_to_pixels(base_project_label_h);
        // CP32: search/tabs/slider sit directly under the PROJECT strip so
        // the toolbar above the strip and the search row below it never
        // overlap. `top + project_h + margin` is the new origin.
        const int search_top      = top + project_h + margin;
        const int left_width      = dpi_.logical_to_pixels(base_left_width);
        const int right_width     = dpi_.logical_to_pixels(base_right_width);
        const int splitter_width  = dpi_.logical_to_pixels(base_splitter_width);
        const int search_height   = dpi_.logical_to_pixels(base_search_height);
        const int tabs_height     = dpi_.logical_to_pixels(base_tabs_height);
        const int zoom_label_h    = dpi_.logical_to_pixels(base_zoom_label_h);
        const int slider_height   = dpi_.logical_to_pixels(base_slider_height);
        const int build_panel_h   = dpi_.logical_to_pixels(base_build_panel_h);

        int width = client.right - client.left;
        int height = client.bottom - client.top - status_height;

        // Left pane: search bar at top (below PROJECT), tree fills below.
        MoveWindow(search_.hwnd(), margin, search_top,
                   left_width - margin * 2, search_height, TRUE);
        MoveWindow(tree_.hwnd(), margin, search_top + search_height + margin,
                   left_width - margin * 2,
                   std::max(0, height - (search_top + search_height + margin) - margin), TRUE);
        MoveWindow(left_splitter_.hwnd(), left_width, 0, splitter_width, height, TRUE);

        // Centre pane: tabs across the top (aligned with search), list below.
        int center_left = left_width + splitter_width + margin;
        int center_width = width - left_width - right_width - splitter_width * 2 - margin * 2;
        MoveWindow(tabs_.hwnd(), center_left, search_top, center_width, tabs_height, TRUE);
        MoveWindow(list_.hwnd(), center_left, search_top + tabs_height + margin, center_width,
                   std::max(0, height - (search_top + tabs_height + margin) - margin), TRUE);

        int right_left = width - right_width;
        MoveWindow(right_splitter_.hwnd(), right_left - splitter_width, 0, splitter_width, height, TRUE);

        // Right pane: zoom label + slider on top row, build panel below,
        // inspector card at the bottom.
        MoveWindow(slider_.hwnd(),
                   right_left + margin,
                   search_top + zoom_label_h + margin,
                   right_width - margin * 2,
                   slider_height, TRUE);

        int inspector_y = search_top + zoom_label_h + margin * 2 + slider_height
                          + build_panel_h + margin;
        int inspector_x = right_left + margin;
        int inspector_w = right_width - margin * 2;
        int inspector_h = std::max(0, height - margin - inspector_y);
        MoveWindow(inspector_panel_.hwnd(), inspector_x, inspector_y, inspector_w, inspector_h, TRUE);
    }

    void invalidate_right_pane() noexcept {
        // CP28: when the slider moves, only the right pane needs to repaint
        // (the percentage label lives in the chrome, not in any HWND). Full
        // InvalidateRect is fine — the child controls re-paint themselves
        // from cached state and the chrome layer is cheap.
        InvalidateRect(hwnd(), nullptr, FALSE);
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

    void set_status(const std::wstring_view text) noexcept {
        if (status_.hwnd() != nullptr) {
            // SB_SETTEXTW requires a writable, null-terminated buffer. Copy
            // the view into a stack string so the Win32 API gets a stable
            // pointer.
            wchar_t buffer[256]{};
            const std::size_t len = std::min(text.size(),
                                             std::size(buffer) - std::size_t{1});
            for (std::size_t i = 0; i < len; ++i) {
                buffer[i] = text[i];
            }
            buffer[len] = L'\0';
            SendMessageW(status_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(buffer));
        }
    }

    void refresh_status() noexcept {
        wchar_t text[160]{};
        // CP28: status bar carries the "● Ready" indicator on the left and
        // the live DPI on the right, separated by a long run of non-breaking
        // spaces (the StatusBar chrome only paints part 0; tabs don't
        // survive DT_CALCRECT here so we use a string pad). The leading
        // bullet is just a glyph — colour is a separate concern resolved by
        // the chrome paint below (we can't recolour the part 0 prefix, so
        // the bullet ships in palette.text tone).
        const int dpi = dpi_.dpi();
        (void)swprintf_s(text, L"\x25CF Ready%*s%d DPI", 24, L"", dpi);
        set_status(text);
    }

    void update_zoom_display() noexcept {
        refresh_status();
    }

    void paint_chrome(HDC target, const RECT& client) noexcept {
        // CP28: paint order is window bg → toolbar → project header → right
        // pane overlays. The toolbar bg is the same as the window bg so the
        // chrome reads as one continuous surface; a single hairline beneath
        // the toolbar is the only separator. Children (search/tree/tabs/
        // list/slider/inspector_panel/status) repaint themselves on top.
        nfui::fill_rect(target, client, palette_.background);
        paint_toolbar(target, client);
        paint_project_header(target, client);
        paint_right_pane(target, client);
    }

    void paint_toolbar(HDC target, const RECT& client) noexcept {
        // CP32: toolbar lives at the top of the content area, directly under
        // the OS menu bar — it must be fully visible (the prior layout
        // overlapped it with the search bar so only an 8px sliver showed).
        const int toolbar_y      = dpi_.logical_to_pixels(base_toolbar_top);
        const int toolbar_height = dpi_.logical_to_pixels(base_toolbar_height);
        const int button_size    = dpi_.logical_to_pixels(base_toolbar_button);
        const int gap            = dpi_.logical_to_pixels(base_toolbar_gap);
        const int group_gap      = dpi_.logical_to_pixels(base_toolbar_group);
        const int radius         = dpi_.logical_to_pixels(6);
        const int margin         = dpi_.logical_to_pixels(base_margin);
        const int stroke         = dpi_.logical_to_pixels(2);

        // CP28: button positions are computed once here and reused by both
        // paint and hit-test. x_offsets[i] is the left edge of button i;
        // the separator gap (group_gap) lives between SAVE and RUN, i.e.
        // before x_offsets[TB_RUN]. Toolbar starts at the same left margin
        // as the search box so the icon strip aligns with the content below.
        int x = margin;
        for (int i = 0; i < TB_COUNT; ++i) {
            tb_x_[i] = x;
            tb_y_    = toolbar_y + (toolbar_height - button_size) / 2;
            x += button_size;
            if (i == TB_SAVE) {
                x += group_gap;
            } else if (i != TB_COUNT - 1) {
                x += gap;
            }
        }
        const int toolbar_right = x;

        for (int i = 0; i < TB_COUNT; ++i) {
            const RECT btn{tb_x_[i], tb_y_, tb_x_[i] + button_size, tb_y_ + button_size};
            const bool hovered = (i == hovered_toolbar_);
            const nfui::Color face = hovered ? palette_.surface_hover : palette_.background;
            if (hovered) {
                nfui::fill_rounded_rect(target, btn, radius, face, palette_.border);
            }
            // CP28: glyph sits inside the button with a small inset so the
            // hover background has breathing room around the icon.
            const int inset = dpi_.logical_to_pixels(2);
            RECT glyph{btn.left + inset, btn.top + inset,
                       btn.right - inset, btn.bottom - inset};
            const nfui::Color icon_col = hovered ? palette_.text : palette_.text_secondary;
            nfui::draw_vector_icon(target, toolbar_icon(i), glyph, icon_col, stroke);
        }

        // 1px hairline below the toolbar so the icon strip reads as its own
        // row without adding extra visual weight.
        const int hairline_y = toolbar_y + toolbar_height;
        if (hairline_y < client.bottom) {
            const RECT hairline{margin, hairline_y, client.right - margin, hairline_y + 1};
            nfui::fill_rect(target, hairline, palette_.border);
        }
        (void)toolbar_right;
    }

    [[nodiscard]] nfui::IconKind toolbar_icon(int index) const noexcept {
        // CP28: map each toolbar slot to the closest available IconKind.
        // The framework ships a fixed glyph vocabulary (chevron / plus /
        // search / gear / etc.); without a play or floppy glyph we lean on
        // chevron_right for "run" and chevron_down for "save" — both read
        // at the toolbar's small scale and stay monochrome.
        switch (index) {
        case TB_NEW:    return nfui::IconKind::plus;
        case TB_OPEN:   return nfui::IconKind::hamburger;
        case TB_SAVE:   return nfui::IconKind::chevron_down;
        case TB_RUN:    return nfui::IconKind::chevron_right;
        case TB_DEBUG:  return nfui::IconKind::warning;
        case TB_SEARCH: return nfui::IconKind::search;
        default:        return nfui::IconKind::none;
        }
    }

    [[nodiscard]] int hit_test_toolbar(int x, int y) const noexcept {
        const int button_size = dpi_.logical_to_pixels(base_toolbar_button);
        for (int i = 0; i < TB_COUNT; ++i) {
            const RECT btn{tb_x_[i], tb_y_, tb_x_[i] + button_size, tb_y_ + button_size};
            if (x >= btn.left && x < btn.right && y >= btn.top && y < btn.bottom) {
                return i;
            }
        }
        return -1;
    }

    void on_toolbar_click(int index) noexcept {
        // CP28: toolbar buttons share the menu's command IDs so they reuse
        // the same routed handlers. The CommandRouter is queried with the
        // matching idm_* constant and the routed handler updates the status
        // bar. on_command() is what the framework calls for menu items;
        // here we shortcut by routing through the router directly.
        int id = 0;
        switch (index) {
        case TB_NEW:    id = idm_new;    break;
        case TB_OPEN:   id = idm_open;   break;
        case TB_SAVE:   id = idm_save;   break;
        case TB_RUN:    id = idm_run;    break;
        case TB_DEBUG:  id = idm_debug;  break;
        case TB_SEARCH: id = idm_search; break;
        default: return;
        }
        if (commands_.route(nfui::CommandId{id})) {
            return;
        }
    }

    void paint_project_header(HDC target, const RECT& client) noexcept {
        // CP32: PROJECT sits in its own strip directly below the toolbar and
        // above the search bar, so neither the toolbar buttons nor the search
        // overlay can collide with the label. The chevron-down glyph signals
        // the tree is expanded; matching palette.text_secondary keeps it
        // muted against the surrounding chrome.
        const int margin = dpi_.logical_to_pixels(base_margin);
        const int project_y = dpi_.logical_to_pixels(base_top);
        const int project_h = dpi_.logical_to_pixels(base_project_label_h);
        HFONT header_font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::lg);

        RECT text{margin, project_y, margin + dpi_.logical_to_pixels(180),
                  project_y + project_h};
        nfui::draw_text(target, text, L"PROJECT", header_font, palette_.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Chevron next to the label: drawn at the right edge of the text
        // block so the icon never collides with subsequent layout (the
        // search box uses the same margin and starts 32 logical px below).
        const int chevron_size = dpi_.logical_to_pixels(12);
        const int chevron_x    = text.right + dpi_.logical_to_pixels(4);
        const int chevron_y    = text.top + (text.bottom - text.top - chevron_size) / 2;
        RECT chevron{chevron_x, chevron_y, chevron_x + chevron_size,
                     chevron_y + chevron_size};
        nfui::draw_vector_icon(target, nfui::IconKind::chevron_down, chevron,
                               palette_.text_secondary, dpi_.logical_to_pixels(2));
        (void)client;
    }

    void paint_right_pane(HDC target, const RECT& client) noexcept {
        const int margin       = dpi_.logical_to_pixels(base_margin);
        const int top          = dpi_.logical_to_pixels(base_top)
                                 + dpi_.logical_to_pixels(base_project_label_h)
                                 + dpi_.logical_to_pixels(base_margin);
        const int right_width  = dpi_.logical_to_pixels(base_right_width);
        const int zoom_label_h = dpi_.logical_to_pixels(base_zoom_label_h);
        const int slider_h     = dpi_.logical_to_pixels(base_slider_height);
        const int build_h      = dpi_.logical_to_pixels(base_build_panel_h);

        const int width    = client.right - client.left;
        const int right_left = width - right_width;

        // "Zoom" label on the left of the slider row, "##%" on the right.
        HFONT label_font  = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);
        HFONT header_font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::lg);

        // CP32: give the lg-font "Zoom" word as much room as the right pane
        // allows — cap at percent_label.left so the two never collide. The
        // prior 64-px width truncated "Zoom" to "Zoo" because Segoe UI
        // Semibold at 20pt measures wider than the legacy estimate.
        const int percent_w = dpi_.logical_to_pixels(40);
        const int zoom_label_w = (right_width - margin - percent_w - margin) - margin;
        RECT zoom_label{right_left + margin, top,
                        right_left + margin + zoom_label_w,
                        top + zoom_label_h};
        nfui::draw_text(target, zoom_label, L"Zoom", header_font, palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);

        wchar_t percent_text[16]{};
        (void)swprintf_s(percent_text, L"%d%%", slider_.pos());
        RECT percent_label{right_left + right_width - margin - percent_w, top,
                           right_left + right_width - margin, top + zoom_label_h};
        nfui::draw_text(target, percent_label, percent_text, label_font,
                        palette_.text_secondary,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Build panel: rounded card with "Build" + "40%" header row and a
        // coral progress bar with a "Building…" overlay.
        const int build_y = top + zoom_label_h + margin * 2 + slider_h;
        RECT build_card{right_left + margin, build_y,
                        right_left + right_width - margin, build_y + build_h};
        const int card_radius = dpi_.logical_to_pixels(nfui::theme_metrics().corner_radius_card);
        nfui::fill_rounded_rect(target, build_card, card_radius,
                                palette_.surface, palette_.border);

        HFONT sm_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);

        // Header row: "Build" on the left, "40%" on the right (palette.text).
        const int header_inset = dpi_.logical_to_pixels(12);
        RECT build_label{build_card.left + header_inset, build_card.top + header_inset,
                         build_card.right - header_inset,
                         build_card.top + header_inset + dpi_.logical_to_pixels(20)};
        nfui::draw_text(target, build_label, L"Build", sm_font, palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT build_percent{build_card.left + header_inset, build_card.top + header_inset,
                           build_card.right - header_inset,
                           build_card.top + header_inset + dpi_.logical_to_pixels(20)};
        nfui::draw_text(target, build_percent, percent_text, sm_font, palette_.text,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Progress bar: rounded track + coral fill, "Building…" overlay.
        const int bar_h      = dpi_.logical_to_pixels(20);
        const int bar_inset  = dpi_.logical_to_pixels(12);
        const int bar_top    = build_card.bottom - bar_inset - bar_h;
        RECT bar{build_card.left + bar_inset, bar_top,
                 build_card.right - bar_inset, bar_top + bar_h};
        const int bar_radius = dpi_.logical_to_pixels(6);
        nfui::fill_rounded_rect(target, bar, bar_radius, palette_.surface_hover, palette_.border);
        // CP28: the fill width tracks slider_.pos() so moving the slider
        // visibly animates the build progress; the overlay text always
        // paints on top.
        const int fill_w = (bar.right - bar.left) * slider_.pos()
                           / std::max(1, slider_.range_high() - slider_.range_low());
        if (fill_w > 0) {
            RECT fill{bar.left, bar.top, bar.left + fill_w, bar.bottom};
            nfui::fill_rounded_rect(target, fill, bar_radius, palette_.accent, palette_.accent);
        }
        // Overlay label: semibold text centered over the bar. Two-tone clip
        // — the text on the filled portion uses palette.accent_text (high
        // contrast on coral), the text on the empty portion uses
        // palette.text_secondary (muted on the surface_hover track).
        const std::wstring overlay = L"Building\x2026";
        HFONT bar_font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::sm);
        if (fill_w > 0) {
            RECT filled_clip{bar.left, bar.top, bar.left + fill_w, bar.bottom};
            const int saved = SaveDC(target);
            IntersectClipRect(target, filled_clip.left, filled_clip.top,
                              filled_clip.right, filled_clip.bottom);
            nfui::draw_text(target, bar, overlay, bar_font, palette_.accent_text,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            RestoreDC(target, saved);
        }
        if (fill_w < bar.right - bar.left) {
            const int saved = SaveDC(target);
            IntersectClipRect(target, bar.left + fill_w, bar.top, bar.right, bar.bottom);
            nfui::draw_text(target, bar, overlay, bar_font, palette_.text_secondary,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            RestoreDC(target, saved);
        }

        // Inspector overlay (magnifier glyph + caption) is drawn by
        // inspector_subclass_proc inside the panel's WM_PAINT cycle so the
        // panel's surface fill does not erase it. See paint_inspector_overlay.
    }

    void paint_search_overlay(HWND edit_hwnd) noexcept {
        // CP28: draw the magnifier glyph over the left padding of the Edit.
        // The native paint has already flushed; we paint via GetDC so the
        // glyph survives subsequent repaints until the Edit erases it
        // itself (typing, focus blink, etc.), at which point WM_PAINT fires
        // again and our subclass redraws the glyph.
        HDC dc = GetDC(edit_hwnd);
        if (dc == nullptr) {
            return;
        }
        RECT rc{};
        GetClientRect(edit_hwnd, &rc);
        const int icon_size = dpi_.logical_to_pixels(14);
        const int pad       = dpi_.logical_to_pixels(8);
        const int y         = rc.top + (rc.bottom - rc.top - icon_size) / 2;
        RECT glyph{rc.left + pad, y, rc.left + pad + icon_size, y + icon_size};
        nfui::draw_vector_icon(dc, nfui::IconKind::search, glyph,
                               palette_.text_secondary, dpi_.logical_to_pixels(2));
        ReleaseDC(edit_hwnd, dc);
    }

    void paint_inspector_overlay(HWND panel_hwnd) noexcept {
        // CP32: draw the inspector placeholder (magnifier + caption) inside
        // the panel's WM_PAINT cycle. GetDC on the panel returns a DC in
        // CLIENT coords, so icon + text are positioned relative to the
        // panel's client origin (no window-to-parent mapping needed here).
        HDC dc = GetDC(panel_hwnd);
        if (dc == nullptr) {
            return;
        }
        RECT rc{};
        GetClientRect(panel_hwnd, &rc);
        const int inspector_w = rc.right - rc.left;
        const int inspector_h = rc.bottom - rc.top;
        // Vertical centering: stack icon (40 logical) + 12 gap + two lines
        // of base-size caption (20 logical each) and centre the stack.
        const int icon_size   = dpi_.logical_to_pixels(40);
        const int cap_gap     = dpi_.logical_to_pixels(12);
        const int cap_line_h  = dpi_.logical_to_pixels(20);
        const int stack_h     = icon_size + cap_gap + cap_line_h * 2;
        const int stack_top   = rc.top + std::max(0, (inspector_h - stack_h) / 2);
        const int icon_x      = rc.left + (inspector_w - icon_size) / 2;
        RECT icon{icon_x, stack_top, icon_x + icon_size, stack_top + icon_size};
        nfui::draw_vector_icon(dc, nfui::IconKind::search, icon,
                               palette_.text_secondary, dpi_.logical_to_pixels(2));

        HFONT caption_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);
        const int cap_pad = dpi_.logical_to_pixels(10);
        const int cap_x = rc.left + cap_pad;
        const int cap_w = inspector_w - cap_pad * 2;
        const int cap_y = icon.bottom + cap_gap;
        RECT caption{cap_x, cap_y, cap_x + cap_w, cap_y + cap_line_h};
        // Two-line caption — the reference breaks "Inspector — select an
        // item to inspect its properties." into two lines.
        nfui::draw_text(dc, caption,
                        L"Inspector \x2014 select an item to",
                        caption_font, palette_.text_secondary,
                        DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
        RECT caption2 = caption;
        caption2.top += cap_line_h;
        caption2.bottom += cap_line_h;
        nfui::draw_text(dc, caption2,
                        L"inspect its properties.",
                        caption_font, palette_.text_secondary,
                        DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
        ReleaseDC(panel_hwnd, dc);
    }

    void paint_tabs_highlight(HWND tabs_hwnd) noexcept {
        // CP32: query the selected tab and overlay a 2 device-px coral bar
        // at the bottom of its rect. TabCtrl_GetItemRect returns coords in
        // the tab control's client space, which matches a GetDC on the
        // tabs HWND — no MapWindowPoints needed. A negative index (no
        // selection) or a failed rect query short-circuits cleanly.
        const int sel = TabCtrl_GetCurSel(tabs_hwnd);
        if (sel < 0) {
            return;
        }
        RECT item{};
        if (TabCtrl_GetItemRect(tabs_hwnd, sel, &item) == FALSE) {
            return;
        }
        HDC dc = GetDC(tabs_hwnd);
        if (dc == nullptr) {
            return;
        }
        RECT highlight{item.left, item.bottom - 2, item.right, item.bottom};
        nfui::fill_rect(dc, highlight, palette_.accent);
        ReleaseDC(tabs_hwnd, dc);
    }

    // CP28: ListView subclass. We install AFTER the framework's chrome
    // subclass; ours runs first on every WM_NOTIFY (NM_CUSTOMDRAW), chains
    // through DefSubclassProc so the framework's row chrome (text colour,
    // selection, hover) keeps working, and then overlays a coloured dot on
    // status-column rows whose text starts with the bullet glyph.
    static LRESULT CALLBACK list_subclass_proc(HWND hwnd, UINT msg, WPARAM w,
                                                LPARAM l, UINT_PTR id, DWORD_PTR ref) {
        auto* self = reinterpret_cast<WorkbenchWindow*>(ref);
        if (msg == WM_NCDESTROY) {
            RemoveWindowSubclass(hwnd, &WorkbenchWindow::list_subclass_proc, id);
            return DefSubclassProc(hwnd, msg, w, l);
        }
        if (msg == ocm_base + WM_NOTIFY) {
            auto* nmhdr = reinterpret_cast<NMHDR*>(l);
            if (nmhdr != nullptr && nmhdr->code == NM_CUSTOMDRAW) {
                auto* lvcd = reinterpret_cast<NMLVCUSTOMDRAW*>(l);
                // Chain through the framework first so row chrome applies.
                const LRESULT res = DefSubclassProc(hwnd, msg, w, l);
                if (self != nullptr) {
                    self->paint_list_status_dot(lvcd);
                }
                return res;
            }
        }
        return DefSubclassProc(hwnd, msg, w, l);
    }

    void paint_list_status_dot(NMLVCUSTOMDRAW* lvcd) noexcept {
        if (lvcd == nullptr || lvcd->nmcd.hdc == nullptr) {
            return;
        }
        // Only paint on item post-paint for sub-item 1 (Status column). The
        // bullet prefix in the column text drives the colour: green for
        // ready/linked/compiled, amber for pending. Reading the actual text
        // via ListView_GetItemText keeps this path robust to row reorder.
        if (lvcd->nmcd.dwDrawStage != CDDS_ITEMPOSTPAINT) {
            return;
        }
        if (lvcd->iSubItem != 1) {
            return;
        }
        const int row = static_cast<int>(lvcd->nmcd.dwItemSpec);
        wchar_t text[64]{};
        LVITEMW lvi{};
        lvi.iSubItem = 1;
        lvi.pszText = text;
        lvi.cchTextMax = static_cast<int>(sizeof(text) / sizeof(text[0]));
        // CP28: ListView_GetItemText is a statement-block macro that yields
        // void, so it can't appear in an `if (...)` expression. Drive
        // LVM_GETITEMTEXT directly and treat a 0-length copy as "skip".
        if (SendMessageW(list_.hwnd(), LVM_GETITEMTEXT, row,
                         reinterpret_cast<LPARAM>(&lvi)) == 0) {
            return;
        }
        if (text[0] != L'\x25CF') {
            return;   // row was repopulated without the bullet prefix
        }
        const std::wstring_view sv(text);
        const bool pending = (sv.find(L"pending") != std::wstring_view::npos);
        const nfui::Color dot_col = pending ? palette_.warning : palette_.success;

        const int dot_size = dpi_.logical_to_pixels(8);
        const int pad      = dpi_.logical_to_pixels(2);
        RECT rc = lvcd->nmcd.rc;
        RECT dot{rc.left + pad, rc.top + (rc.bottom - rc.top - dot_size) / 2,
                 rc.left + pad + dot_size,
                 rc.top + (rc.bottom - rc.top + dot_size) / 2};
        nfui::fill_ellipse(lvcd->nmcd.hdc, dot, dot_col);
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::CommandRouter commands_;
    nfui::Dialog about_;
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::Menu menu_;
    nfui::Edit search_;
    nfui::TreeView tree_;
    nfui::TabControl tabs_;
    nfui::ListView list_;
    nfui::Panel inspector_panel_;
    nfui::StatusBar status_;
    nfui::Slider slider_;
    nfui::Splitter left_splitter_;
    nfui::Splitter right_splitter_;
    nfui::DpiScale dpi_{96};
    int list_item_count_{0};

    // CP28: toolbar hit-test state. tb_x_[i] is the left edge of button i;
    // tb_y_ is the top edge (all buttons share a y so we cache once).
    int tb_x_[TB_COUNT]{};
    int tb_y_{};
    int hovered_toolbar_{-1};
    bool tracking_toolbar_{false};

    static constexpr UINT ocm_base = WM_USER + 0x1c00;
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