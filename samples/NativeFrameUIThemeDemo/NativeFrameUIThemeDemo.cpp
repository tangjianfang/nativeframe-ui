// NativeFrameUIThemeDemo
//
// CP32 Theme Gallery: a card-grid layout that showcases every themed control
// under Light / Dark / High Contrast palettes. Each card is a rounded panel
// with an in-card title; the tab row and layout row span the full width so no
// control is stranded in an orphan card.

#include <nfui/NativeFrameUI.hpp>

#include "NativeFrameUIResource.h"

#include <commctrl.h>
#include <fstream>
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

// CP32: every demonstrator id lives in the 201+ range.
constexpr int id_card_form_title     = 201;
constexpr int id_card_lists_title    = 202;
constexpr int id_card_feedback_title = 203;
constexpr int id_card_tabs_title     = 204;
constexpr int id_card_layout_title   = 205;

constexpr int id_lbl_button      = 210;
constexpr int id_btn_primary     = 211;
constexpr int id_btn_secondary   = 212;
constexpr int id_btn_disabled    = 213;
constexpr int id_lbl_checkbox    = 214;
constexpr int id_chk_unchecked   = 215;
constexpr int id_chk_checked     = 216;
constexpr int id_chk_indeterminate = 217;
constexpr int id_lbl_radio       = 218;
constexpr int id_rad_first       = 219;
constexpr int id_rad_second      = 220;
constexpr int id_rad_third       = 221;
constexpr int id_lbl_edit        = 222;
constexpr int id_edit            = 223;

constexpr int id_lbl_listbox     = 224;
constexpr int id_listbox         = 225;
constexpr int id_lbl_combobox    = 226;
constexpr int id_combobox        = 227;
constexpr int id_lbl_listview    = 228;
constexpr int id_listview        = 229;
constexpr int id_lbl_treeview    = 230;
constexpr int id_treeview        = 231;

constexpr int id_lbl_iconview    = 232;
constexpr int id_icon_home       = 233;
constexpr int id_icon_gear       = 234;
constexpr int id_icon_folder     = 235;
constexpr int id_icon_user       = 236;
constexpr int id_lbl_progress    = 237;
constexpr int id_progress        = 238;

constexpr int id_tabs            = 239;

constexpr int id_lbl_layout_static = 240;
constexpr int id_static_left     = 241;
constexpr int id_static_center   = 242;
constexpr int id_static_right    = 243;
constexpr int id_lbl_panel_splitter = 244;
constexpr int id_panel           = 245;
constexpr int id_splitter        = 246;
constexpr int id_status          = 247;

// 4/8/12/16 logical grid.
constexpr int kOuterMargin   = 16;
constexpr int kGap           = 12;
constexpr int kCardPadding   = 16;
constexpr int kTitleHeight   = 32;
constexpr int kLabelHeight   = 20;
constexpr int kControlHeight = 28;
constexpr int kEditHeight    = 28;
constexpr int kListBoxHeight = 80;
constexpr int kListViewHeight = 100;
constexpr int kTreeViewHeight = 80;
constexpr int kIconSize      = 40;
constexpr int kProgressHeight = 24;
constexpr int kTabStripHeight = 36;

constexpr int kTopBarHeight   = 56;
constexpr int kMinTopCardH    = 500;
constexpr int kTabCardHeight  = 72;
constexpr int kLayoutCardH    = 240;

// ---------------------------------------------------------------------------
// CardPanel: a rounded panel whose interior and 1px border track the injected
// ThemePalette. Radius is specified in logical pixels and scaled per-window.
// ---------------------------------------------------------------------------
class CardPanel : public nfui::Panel {
public:
    void set_dpi(nfui::DpiScale* dpi) noexcept { dpi_ = dpi; }
    void set_corner_radius(int logical_radius) noexcept { radius_logical_ = logical_radius; }

protected:
    void on_paint(HDC dc, const nfui::Control::PaintState& state) noexcept override {
        const nfui::ThemePalette* pal = palette();
        const nfui::ThemePalette& p = pal ? *pal : nfui::theme_palette(nfui::ThemeMode::light);
        const int radius = (dpi_ != nullptr) ? dpi_->logical_to_pixels(radius_logical_) : radius_logical_;

        nfui::MemoryDC mem(dc, state.bounds);
        HDC target = mem.valid() ? mem.dc() : dc;
        const RECT paint_bounds = mem.valid()
            ? RECT{0, 0, state.bounds.right - state.bounds.left, state.bounds.bottom - state.bounds.top}
            : state.bounds;

        // The MemoryDC already holds the parent background in the corner pixels,
        // so the rounded shape blends cleanly without a square halo.
        nfui::fill_rounded_rect(target, paint_bounds, radius, p.surface, p.border);
    }

private:
    nfui::DpiScale* dpi_{nullptr};
    int radius_logical_{10};
};

// ---------------------------------------------------------------------------
// TabControl post-paint helper: draws a 2px coral bottom bar on the selected
// tab after the native/common-control paint pass has finished.
// ---------------------------------------------------------------------------
struct TabCoralContext {
    nfui::TabControl* tabs{nullptr};
    nfui::DpiScale* dpi{nullptr};
    const nfui::ThemePalette* palette{nullptr};
};

LRESULT CALLBACK tab_coral_subclass_proc(HWND hwnd,
                                         UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam,
                                         UINT_PTR subclass_id,
                                         DWORD_PTR ref_data) noexcept {
    auto* ctx = reinterpret_cast<TabCoralContext*>(ref_data);

    switch (message) {
    case WM_PAINT: {
        LRESULT r = DefSubclassProc(hwnd, message, wparam, lparam);
        if (ctx != nullptr && ctx->tabs != nullptr && ctx->dpi != nullptr) {
            HDC dc = GetDC(hwnd);
            if (dc != nullptr) {
                const nfui::ThemePalette& p = ctx->palette
                    ? *ctx->palette
                    : nfui::theme_palette(nfui::ThemeMode::light);
                const int sel = static_cast<int>(TabCtrl_GetCurSel(hwnd));
                RECT rc{};
                if (sel >= 0 && TabCtrl_GetItemRect(hwnd, sel, &rc) != FALSE) {
                    const int bar_h = ctx->dpi->logical_to_pixels(2);
                    const int y = rc.bottom - bar_h;
                    nfui::draw_line(dc, POINT{rc.left, y}, POINT{rc.right, y}, p.accent, bar_h);
                }
                ReleaseDC(hwnd, dc);
            }
        }
        return r;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &tab_coral_subclass_proc, subclass_id);
        break;
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

// ---------------------------------------------------------------------------
// ThemeDemoWindow
// ---------------------------------------------------------------------------
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
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"create_main start\n";
        }
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIThemeDemoWindow",
            L"Theme Gallery",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1400,
            1040,
        };

        if (!create(params)) {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"Window::create failed\n";
            return false;
        }
        {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"Window::create ok hwnd=" << static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(hwnd())) << L"\n";
        }

        apply_window_icon();
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));

        if (!create_toggle_buttons()) {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"toggle buttons failed\n";
            return false;
        }
        {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"toggle buttons ok\n";
        }

        if (!create_cards()) {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"cards failed\n";
            return false;
        }
        {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"cards ok\n";
        }

        if (!create_demo_controls()) {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"controls failed\n";
            return false;
        }
        {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"controls ok\n";
        }

        apply_palette_to_all();
        apply_native_fonts();
        perform_layout();

        {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"about to ShowWindow hwnd=" << reinterpret_cast<std::uintptr_t>(hwnd()) << L" show=" << show_command << L"\n";
        }
        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        {
            std::wofstream log(L"C:\\tmp\\themedemo_debug.log", std::ios::app);
            log << L"create_main done\n";
        }
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            perform_layout();
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
            perform_layout();
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
            if (hdc != nullptr) {
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

    bool on_command(int command_id, HWND, UINT notification_code) override {
        if (notification_code != BN_CLICKED && notification_code != 0) {
            return false;
        }
        switch (command_id) {
        case id_theme_light:
            switch_mode(nfui::ThemeMode::light);
            return true;
        case id_theme_dark:
            switch_mode(nfui::ThemeMode::dark);
            return true;
        case id_theme_hc:
            switch_mode(nfui::ThemeMode::high_contrast);
            return true;
        default:
            break;
        }
        return false;
    }

private:
    // -----------------------------------------------------------------------
    // Creation helpers
    // -----------------------------------------------------------------------
    template <typename ControlT>
    [[nodiscard]] bool create_child(ControlT& control,
                                    HWND parent,
                                    int id,
                                    std::wstring_view text) noexcept {
        nfui::ControlCreateParams params{
            instance_,
            parent,
            id,
            text,
            0, 0, 100, 24,
        };
        control.inject_theme(&palette_, &fonts_);
        return control.create(params);
    }

    template <typename ControlT>
    [[nodiscard]] bool create_child(ControlT& control,
                                    HWND parent,
                                    int id,
                                    std::wstring_view text,
                                    int width,
                                    int height) noexcept {
        nfui::ControlCreateParams params{
            instance_,
            parent,
            id,
            text,
            0, 0, width, height,
        };
        control.inject_theme(&palette_, &fonts_);
        return control.create(params);
    }

    [[nodiscard]] bool create_toggle_buttons() noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id_theme_light,
            L"Light",
            0, 0, 120, kControlHeight,
        };
        theme_light_.inject_theme(&palette_, &fonts_);
        if (!theme_light_.create(params)) return false;

        params.control_id = id_theme_dark;
        params.text       = L"Dark";
        theme_dark_.inject_theme(&palette_, &fonts_);
        if (!theme_dark_.create(params)) return false;

        params.control_id = id_theme_hc;
        params.text       = L"High Contrast";
        params.width      = 160;
        theme_hc_.inject_theme(&palette_, &fonts_);
        if (!theme_hc_.create(params)) return false;

        nfui::ButtonStyle active_style{};
        active_style.use_semibold = true;
        theme_light_.set_style(active_style);
        theme_dark_.set_style(nfui::ButtonStyle{});
        theme_hc_.set_style(nfui::ButtonStyle{});
        return true;
    }

    [[nodiscard]] bool create_cards() noexcept {
        auto setup_card = [&](CardPanel& card) {
            card.inject_theme(&palette_, &fonts_);
            card.set_dpi(&dpi_);
            card.set_corner_radius(10);
        };
        setup_card(card_form_);
        setup_card(card_lists_);
        setup_card(card_feedback_);
        setup_card(card_tabs_);
        setup_card(card_layout_);

        if (!create_child(card_form_,     hwnd(), 0, L"", 100, 100)) return false;
        if (!create_child(card_lists_,    hwnd(), 0, L"", 100, 100)) return false;
        if (!create_child(card_feedback_, hwnd(), 0, L"", 100, 100)) return false;
        if (!create_child(card_tabs_,     hwnd(), 0, L"", 100, 100)) return false;
        if (!create_child(card_layout_,   hwnd(), 0, L"", 100, 100)) return false;
        return true;
    }

    [[nodiscard]] bool create_demo_controls() noexcept {
        // ---- Form Controls card ----
        if (!create_title(title_form_, hwnd(), id_card_form_title, L"Form Controls")) return false;

        if (!create_label(lbl_button_, hwnd(), id_lbl_button, L"Button")) return false;
        if (!create_child(btn_primary_,   hwnd(), id_btn_primary,   L"OK",      72, kControlHeight)) return false;
        if (!create_child(btn_secondary_, hwnd(), id_btn_secondary, L"Cancel",  72, kControlHeight)) return false;
        if (!create_child(btn_disabled_,  hwnd(), id_btn_disabled,  L"Disabled", 72, kControlHeight)) return false;
        EnableWindow(btn_disabled_.hwnd(), FALSE);

        if (!create_label(lbl_checkbox_, hwnd(), id_lbl_checkbox, L"CheckBox")) return false;
        if (!create_child(chk_unchecked_,    hwnd(), id_chk_unchecked,    L"Unchecked",    100, kControlHeight)) return false;
        if (!create_child(chk_checked_,      hwnd(), id_chk_checked,      L"Checked",      100, kControlHeight)) return false;
        if (!create_child(chk_indeterminate_, hwnd(), id_chk_indeterminate, L"Indeterminate", 110, kControlHeight)) return false;
        SendMessageW(chk_checked_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(chk_indeterminate_.hwnd(), BM_SETCHECK, BST_INDETERMINATE, 0);

        if (!create_label(lbl_radio_, hwnd(), id_lbl_radio, L"RadioButton")) return false;
        if (!create_child(rad_first_,  hwnd(), id_rad_first,  L"First",  80, kControlHeight)) return false;
        if (!create_child(rad_second_, hwnd(), id_rad_second, L"Second", 80, kControlHeight)) return false;
        if (!create_child(rad_third_,  hwnd(), id_rad_third,  L"Third",  80, kControlHeight)) return false;
        SendMessageW(rad_first_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);

        if (!create_label(lbl_edit_, hwnd(), id_lbl_edit, L"Edit")) return false;
        if (!create_child(edit_, hwnd(), id_edit, L"editable sample", 200, kEditHeight)) return false;

        // ---- Lists card ----
        if (!create_title(title_lists_, hwnd(), id_card_lists_title, L"Lists")) return false;

        if (!create_label(lbl_listbox_, hwnd(), id_lbl_listbox, L"ListBox")) return false;
        if (!create_child(listbox_, hwnd(), id_listbox, L"", 200, kListBoxHeight)) return false;
        ListBox_AddString(listbox_.hwnd(), L"Alice");
        ListBox_AddString(listbox_.hwnd(), L"Bob");
        ListBox_AddString(listbox_.hwnd(), L"Charlie");
        ListBox_AddString(listbox_.hwnd(), L"David");
        ListBox_AddString(listbox_.hwnd(), L"Eve");
        SendMessageW(listbox_.hwnd(), LB_SETCURSEL, 2, 0);

        if (!create_label(lbl_combobox_, hwnd(), id_lbl_combobox, L"ComboBox")) return false;
        if (!create_child(combobox_, hwnd(), id_combobox, L"", 200, kControlHeight)) return false;
        ComboBox_AddString(combobox_.hwnd(), L"Red");
        ComboBox_AddString(combobox_.hwnd(), L"Green");
        ComboBox_AddString(combobox_.hwnd(), L"Blue");
        ComboBox_AddString(combobox_.hwnd(), L"Yellow");
        ComboBox_AddString(combobox_.hwnd(), L"Purple");
        SendMessageW(combobox_.hwnd(), CB_SETCURSEL, 0, 0);

        if (!create_label(lbl_listview_, hwnd(), id_lbl_listview, L"ListView")) return false;
        if (!create_child(listview_, hwnd(), id_listview, L"", 200, kListViewHeight)) return false;
        ListView_SetExtendedListViewStyle(listview_.hwnd(), LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        insert_listview_column(listview_.hwnd(), 0, L"Name",  120);
        insert_listview_column(listview_.hwnd(), 1, L"Type",  100);
        insert_listview_column(listview_.hwnd(), 2, L"Owner", 100);
        insert_listview_row(listview_.hwnd(), 0, L"file.txt",     L"File",  L"User");
        insert_listview_row(listview_.hwnd(), 1, L"image.png",    L"Image", L"Admin");
        insert_listview_row(listview_.hwnd(), 2, L"document.pdf", L"PDF",   L"User");
        insert_listview_row(listview_.hwnd(), 3, L"notes.md",      L"Text",  L"User");

        if (!create_label(lbl_treeview_, hwnd(), id_lbl_treeview, L"TreeView")) return false;
        if (!create_child(treeview_, hwnd(), id_treeview, L"", 200, kTreeViewHeight)) return false;
        HTREEITEM root = tree_item_insert(treeview_.hwnd(), TVI_ROOT, TVI_LAST, L"Project");
        HTREEITEM src = tree_item_insert(treeview_.hwnd(), root, TVI_LAST, L"Sources");
        tree_item_insert(treeview_.hwnd(), src, TVI_LAST, L"Main.cpp");
        tree_item_insert(treeview_.hwnd(), root, TVI_LAST, L"Resources");
        tree_item_insert(treeview_.hwnd(), root, TVI_LAST, L"Tests");
        TreeView_Expand(treeview_.hwnd(), root, TVE_EXPAND);
        TreeView_Expand(treeview_.hwnd(), src, TVE_EXPAND);

        // ---- Feedback card ----
        if (!create_title(title_feedback_, hwnd(), id_card_feedback_title, L"Feedback")) return false;

        if (!create_label(lbl_iconview_, hwnd(), id_lbl_iconview, L"IconView")) return false;
        if (!create_icon(icon_home_,   hwnd(), id_icon_home,   nfui::IconKind::search, 36)) return false;
        if (!create_icon(icon_gear_,   hwnd(), id_icon_gear,   nfui::IconKind::gear,   36)) return false;
        if (!create_icon(icon_folder_, hwnd(), id_icon_folder, nfui::IconKind::info,   36)) return false;
        if (!create_icon(icon_user_,   hwnd(), id_icon_user,   nfui::IconKind::warning, 36)) return false;

        if (!create_label(lbl_progress_, hwnd(), id_lbl_progress, L"ProgressBar")) return false;
        if (!create_child(progress_, hwnd(), id_progress, L"", 200, kProgressHeight)) return false;
        SendMessageW(progress_.hwnd(), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(progress_.hwnd(), PBM_SETPOS, 65, 0);
        nfui::FrameStyle progress_style{};
        progress_style.bar_color = palette_.accent;
        progress_.set_style(progress_style);

        // ---- Tab card ----
        if (!create_title(title_tabs_, hwnd(), id_card_tabs_title, L"TabControl")) return false;
        if (!create_child(tabs_, hwnd(), id_tabs, L"", 200, kTabStripHeight)) return false;
        static_cast<void>(tabs_.set_padding(dpi_.logical_to_pixels(12), dpi_.logical_to_pixels(4)));
        insert_tab(tabs_.hwnd(), 0, L"General");
        insert_tab(tabs_.hwnd(), 1, L"Layout");
        insert_tab(tabs_.hwnd(), 2, L"Output");
        tab_ctx_.tabs    = &tabs_;
        tab_ctx_.dpi     = &dpi_;
        tab_ctx_.palette = &palette_;
        if (SetWindowSubclass(tabs_.hwnd(), &tab_coral_subclass_proc, 1, reinterpret_cast<DWORD_PTR>(&tab_ctx_)) == FALSE) {
            return false;
        }

        // ---- Layout card ----
        if (!create_title(title_layout_, hwnd(), id_card_layout_title, L"Layout")) return false;

        if (!create_label(lbl_layout_static_, hwnd(), id_lbl_layout_static, L"StaticText")) return false;
        if (!create_child(static_left_,   hwnd(), id_static_left,   L"Left aligned",   100, kControlHeight)) return false;
        if (!create_child(static_center_, hwnd(), id_static_center, L"Center aligned", 100, kControlHeight)) return false;
        if (!create_child(static_right_,  hwnd(), id_static_right,  L"Right aligned",  100, kControlHeight)) return false;
        nfui::TextStyle left_style{};
        left_style.background = palette_.surface;
        left_style.foreground = palette_.text;
        left_style.align_h    = nfui::StaticTextAlignH::left;
        static_left_.set_style(left_style);
        nfui::TextStyle center_style = left_style;
        center_style.align_h = nfui::StaticTextAlignH::center;
        static_center_.set_style(center_style);
        nfui::TextStyle right_style = left_style;
        right_style.align_h = nfui::StaticTextAlignH::right;
        static_right_.set_style(right_style);

        if (!create_label(lbl_panel_splitter_, hwnd(), id_lbl_panel_splitter, L"Panel + Splitter")) return false;
        if (!create_child(layout_panel_,   hwnd(), id_panel,    L"", 200, 80)) return false;
        if (!create_child(layout_splitter_, hwnd(), id_splitter, L"", 6,   80)) return false;
        layout_splitter_.set_orientation(nfui::SplitterOrientation::Vertical);
        nfui::FrameStyle panel_style{};
        panel_style.surface_brush = palette_.surface;
        panel_style.accent      = palette_.border;
        layout_panel_.set_style(panel_style);

        // ---- Status bar ----
        if (!create_child(status_, hwnd(), id_status, L"", 100, 22)) return false;
        SendMessageW(status_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Theme Gallery: ready"));

        return true;
    }

    [[nodiscard]] bool create_title(nfui::StaticText& label, HWND parent, int id, std::wstring_view text) noexcept {
        nfui::TextStyle style{};
        style.background      = palette_.surface;
        style.foreground      = palette_.text;
        style.use_semibold    = true;
        style.font_size_pt    = nfui::font_pt::md;
        style.align_v         = nfui::StaticTextAlignV::middle;
        label.set_style(style);
        return create_child(label, parent, id, text);
    }

    [[nodiscard]] bool create_label(nfui::StaticText& label, HWND parent, int id, std::wstring_view text) noexcept {
        nfui::TextStyle style{};
        style.background      = palette_.surface;
        style.foreground      = palette_.text_secondary;
        style.use_semibold    = true;
        style.font_size_pt    = nfui::font_pt::sm;
        style.align_v         = nfui::StaticTextAlignV::middle;
        label.set_style(style);
        return create_child(label, parent, id, text);
    }

    [[nodiscard]] bool create_icon(nfui::IconView& icon, HWND parent, int id, nfui::IconKind kind, int logical_size) noexcept {
        if (!create_child(icon, parent, id, L"", logical_size, logical_size)) return false;
        nfui::IconViewStyle style{};
        style.pixel_size = logical_size;
        style.background = palette_.surface;
        style.padding    = 4;
        icon.set_style(style);
        icon.set_glyph(kind, palette_.accent);
        return true;
    }

    // -----------------------------------------------------------------------
    // Layout
    // -----------------------------------------------------------------------
    void perform_layout() {
        if (hwnd() == nullptr) return;

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);

        // Status bar auto-sizes and reports its height.
        int status_h = 0;
        if (status_.hwnd() != nullptr) {
            SendMessageW(status_.hwnd(), WM_SIZE, 0, 0);
            RECT sr{};
            GetWindowRect(status_.hwnd(), &sr);
            status_h = sr.bottom - sr.top;
        }

        const int outer   = dpi_.logical_to_pixels(kOuterMargin);
        const int gap     = dpi_.logical_to_pixels(kGap);
        const int pad     = dpi_.logical_to_pixels(kCardPadding);
        const int title_h = dpi_.logical_to_pixels(kTitleHeight);
        const int top_bar = dpi_.logical_to_pixels(kTopBarHeight);

        const int client_w = client.right - client.left;
        const int client_h = client.bottom - client.top;

        // Theme toggle buttons: top-right, right-aligned.
        const int btn_w = dpi_.logical_to_pixels(120);
        const int btn_h = dpi_.logical_to_pixels(kControlHeight);
        const int gap8  = dpi_.logical_to_pixels(8);
        const int total_toggle_w = 3 * btn_w + 2 * gap8;
        int tx = client.right - outer - total_toggle_w;
        int ty = client.top + outer + (top_bar - btn_h) / 2;
        MoveWindow(theme_light_.hwnd(), tx,                             ty, btn_w, btn_h, TRUE);
        MoveWindow(theme_dark_.hwnd(),  tx + (btn_w + gap8),          ty, btn_w, btn_h, TRUE);
        MoveWindow(theme_hc_.hwnd(),    tx + 2 * (btn_w + gap8),       ty, dpi_.logical_to_pixels(160), btn_h, TRUE);

        // Card geometry.
        const int y_cards = client.top + outer + top_bar + gap;
        const int card_w  = (client_w - 2 * outer - 2 * gap) / 3;
        const int x0      = client.left + outer;
        const int x1      = x0 + card_w + gap;
        const int x2      = x0 + 2 * (card_w + gap);

        int available_h = client_h - 2 * outer - top_bar - status_h - 4 * gap;
        int tab_h       = dpi_.logical_to_pixels(kTabCardHeight);
        int layout_h    = dpi_.logical_to_pixels(kLayoutCardH);
        int top_h       = available_h - tab_h - layout_h;
        const int min_top = dpi_.logical_to_pixels(kMinTopCardH);
        if (top_h < min_top) {
            top_h = min_top;
            int remaining = available_h - top_h;
            if (remaining > 0) {
                layout_h = remaining * 3 / 4;
                tab_h    = remaining - layout_h;
            }
        }

        const int y_tab    = y_cards + top_h + gap;
        const int y_layout = y_tab + tab_h + gap;

        SetRect(&rc_card_form_,     x0, y_cards,  x0 + card_w, y_cards + top_h);
        SetRect(&rc_card_lists_,    x1, y_cards,  x1 + card_w, y_cards + top_h);
        SetRect(&rc_card_feedback_, x2, y_cards,  x2 + card_w, y_cards + top_h);
        SetRect(&rc_card_tabs_,     x0, y_tab,    client.right - outer, y_tab + tab_h);
        SetRect(&rc_card_layout_,   x0, y_layout, client.right - outer, y_layout + layout_h);

        MoveWindow(hwnd(),     rc_card_form_.left,     rc_card_form_.top,
                   rc_card_form_.right     - rc_card_form_.left,     rc_card_form_.bottom     - rc_card_form_.top,     TRUE);
        MoveWindow(hwnd(),    rc_card_lists_.left,    rc_card_lists_.top,
                   rc_card_lists_.right    - rc_card_lists_.left,    rc_card_lists_.bottom    - rc_card_lists_.top,    TRUE);
        MoveWindow(hwnd(), rc_card_feedback_.left, rc_card_feedback_.top,
                   rc_card_feedback_.right - rc_card_feedback_.left, rc_card_feedback_.bottom - rc_card_feedback_.top, TRUE);
        MoveWindow(hwnd(),     rc_card_tabs_.left,     rc_card_tabs_.top,
                   rc_card_tabs_.right     - rc_card_tabs_.left,     rc_card_tabs_.bottom     - rc_card_tabs_.top,     TRUE);
        MoveWindow(hwnd(),   rc_card_layout_.left,   rc_card_layout_.top,
                   rc_card_layout_.right   - rc_card_layout_.left,   rc_card_layout_.bottom   - rc_card_layout_.top,   TRUE);

        // Place controls inside each card.
        layout_form_card(pad, title_h, gap);
        layout_lists_card(pad, title_h, gap);
        layout_feedback_card(pad, title_h, gap);
        layout_tab_card(pad, title_h);
        layout_layout_card(pad, title_h, gap);

        // Status bar across the bottom.
        if (status_.hwnd() != nullptr) {
            MoveWindow(status_.hwnd(), client.left, client.bottom - status_h,
                       client_w, status_h, TRUE);
        }
    }

    void move_to(HWND child, const RECT& rc) {
        if (child != nullptr) {
            MoveWindow(child, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
        }
    }

    void layout_form_card(int pad, int title_h, int gap) {
        const RECT& c = rc_card_form_;
        const int ctrl_h = dpi_.logical_to_pixels(kControlHeight);
        const int edit_h = dpi_.logical_to_pixels(kEditHeight);
        const int label_w = dpi_.logical_to_pixels(72);
        const int label_gap = dpi_.logical_to_pixels(8);
        const int btn_w = dpi_.logical_to_pixels(72);
        const int chk_w = dpi_.logical_to_pixels(100);
        const int rad_w = dpi_.logical_to_pixels(80);

        int y = c.top + pad;
        move_to(title_form_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + title_h});
        y += title_h + gap;

        // Button row.
        move_to(lbl_button_.hwnd(), RECT{c.left + pad, y, c.left + pad + label_w, y + ctrl_h});
        int x = c.left + pad + label_w + label_gap;
        move_to(btn_primary_.hwnd(),   RECT{x,                     y, x + btn_w,     y + ctrl_h});
        move_to(btn_secondary_.hwnd(), RECT{x + (btn_w + gap),      y, x + 2*btn_w + gap, y + ctrl_h});
        move_to(btn_disabled_.hwnd(),  RECT{x + 2*(btn_w + gap),   y, x + 3*btn_w + 2*gap, y + ctrl_h});
        y += ctrl_h + gap;

        // CheckBox row.
        move_to(lbl_checkbox_.hwnd(), RECT{c.left + pad, y, c.left + pad + label_w, y + ctrl_h});
        x = c.left + pad + label_w + label_gap;
        move_to(chk_unchecked_.hwnd(),    RECT{x, y, x + chk_w, y + ctrl_h});
        move_to(chk_checked_.hwnd(),      RECT{x + (chk_w + gap), y, x + 2*chk_w + gap, y + ctrl_h});
        move_to(chk_indeterminate_.hwnd(), RECT{x + 2*(chk_w + gap), y, x + 3*chk_w + 2*gap, y + ctrl_h});
        y += ctrl_h + gap;

        // Radio row.
        move_to(lbl_radio_.hwnd(), RECT{c.left + pad, y, c.left + pad + label_w, y + ctrl_h});
        x = c.left + pad + label_w + label_gap;
        move_to(rad_first_.hwnd(),  RECT{x, y, x + rad_w, y + ctrl_h});
        move_to(rad_second_.hwnd(), RECT{x + (rad_w + gap), y, x + 2*rad_w + gap, y + ctrl_h});
        move_to(rad_third_.hwnd(),  RECT{x + 2*(rad_w + gap), y, x + 3*rad_w + 2*gap, y + ctrl_h});
        y += ctrl_h + gap;

        // Edit row.
        move_to(lbl_edit_.hwnd(), RECT{c.left + pad, y, c.left + pad + label_w, y + edit_h});
        x = c.left + pad + label_w + label_gap;
        move_to(edit_.hwnd(), RECT{x, y, c.right - pad, y + edit_h});
    }

    void layout_lists_card(int pad, int title_h, int gap) {
        const RECT& c = rc_card_lists_;
        const int listbox_h = dpi_.logical_to_pixels(kListBoxHeight);
        const int combo_h   = dpi_.logical_to_pixels(kControlHeight);
        const int listview_h = dpi_.logical_to_pixels(kListViewHeight);
        const int treeview_h = dpi_.logical_to_pixels(kTreeViewHeight);
        const int label_h   = dpi_.logical_to_pixels(kLabelHeight);

        int y = c.top + pad;
        move_to(title_lists_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + title_h});
        y += title_h + gap;

        move_to(lbl_listbox_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + label_h});
        y += label_h;
        move_to(listbox_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + listbox_h});
        y += listbox_h + gap;

        move_to(lbl_combobox_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + label_h});
        y += label_h;
        move_to(combobox_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + combo_h});
        y += combo_h + gap;

        move_to(lbl_listview_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + label_h});
        y += label_h;
        move_to(listview_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + listview_h});
        y += listview_h + gap;

        move_to(lbl_treeview_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + label_h});
        y += label_h;
        move_to(treeview_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + treeview_h});
    }

    void layout_feedback_card(int pad, int title_h, int gap) {
        const RECT& c = rc_card_feedback_;
        const int icon_size = dpi_.logical_to_pixels(kIconSize);
        const int progress_h = dpi_.logical_to_pixels(kProgressHeight);
        const int label_h = dpi_.logical_to_pixels(kLabelHeight);

        int y = c.top + pad;
        move_to(title_feedback_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + title_h});
        y += title_h + gap;

        move_to(lbl_iconview_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + label_h});
        y += label_h;
        int x = c.left + pad;
        move_to(icon_home_.hwnd(),   RECT{x, y, x + icon_size, y + icon_size});
        x += icon_size + gap;
        move_to(icon_gear_.hwnd(),   RECT{x, y, x + icon_size, y + icon_size});
        x += icon_size + gap;
        move_to(icon_folder_.hwnd(), RECT{x, y, x + icon_size, y + icon_size});
        x += icon_size + gap;
        move_to(icon_user_.hwnd(),   RECT{x, y, x + icon_size, y + icon_size});
        y += icon_size + gap;

        move_to(lbl_progress_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + label_h});
        y += label_h;
        move_to(progress_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + progress_h});
    }

    void layout_tab_card(int pad, int title_h) {
        const RECT& c = rc_card_tabs_;
        const int tab_h = dpi_.logical_to_pixels(kTabStripHeight);

        int y = c.top + pad;
        move_to(title_tabs_.hwnd(), RECT{c.left + pad, y, c.left + pad + dpi_.logical_to_pixels(120), y + title_h});
        y += title_h;
        move_to(tabs_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + tab_h});
    }

    void layout_layout_card(int pad, int title_h, int gap) {
        const RECT& c = rc_card_layout_;
        const int ctrl_h = dpi_.logical_to_pixels(kControlHeight);
        const int label_h = dpi_.logical_to_pixels(kLabelHeight);
        const int splitter_w = dpi_.logical_to_pixels(6);

        int y = c.top + pad;
        move_to(title_layout_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + title_h});
        y += title_h + gap;

        // StaticText alignment demo.
        move_to(lbl_layout_static_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + label_h});
        y += label_h;
        const int static_w = dpi_.logical_to_pixels(100);
        int x = c.left + pad;
        move_to(static_left_.hwnd(),   RECT{x, y, x + static_w, y + ctrl_h});
        x += static_w + gap;
        move_to(static_center_.hwnd(), RECT{x, y, x + static_w, y + ctrl_h});
        x += static_w + gap;
        move_to(static_right_.hwnd(),  RECT{x, y, x + static_w, y + ctrl_h});
        y += ctrl_h + gap;

        // Panel + Splitter.
        move_to(lbl_panel_splitter_.hwnd(), RECT{c.left + pad, y, c.right - pad, y + label_h});
        y += label_h;
        const int panel_w = (c.right - c.left - 2 * pad - splitter_w - gap) / 2;
        const int section_h = c.bottom - y - pad;
        move_to(layout_panel_.hwnd(),   RECT{c.left + pad, y, c.left + pad + panel_w, y + section_h});
        move_to(layout_splitter_.hwnd(), RECT{c.left + pad + panel_w + gap, y,
                                              c.left + pad + panel_w + gap + splitter_w, y + section_h});
    }

    // -----------------------------------------------------------------------
    // Theme / font / paint helpers
    // -----------------------------------------------------------------------
    void apply_palette_to_all() noexcept {
        theme_light_.set_palette(&palette_);
        theme_dark_.set_palette(&palette_);
        theme_hc_.set_palette(&palette_);

        card_form_.set_palette(&palette_);
        card_lists_.set_palette(&palette_);
        card_feedback_.set_palette(&palette_);
        card_tabs_.set_palette(&palette_);
        card_layout_.set_palette(&palette_);

        auto refresh_static = [&](nfui::StaticText& st) {
            // Re-apply the stored style so the background tracks the new surface.
            nfui::TextStyle s = st.style();
            s.background = palette_.surface;
            st.set_style(s);
            st.set_palette(&palette_);
        };
        refresh_static(title_form_);
        refresh_static(title_lists_);
        refresh_static(title_feedback_);
        refresh_static(title_tabs_);
        refresh_static(title_layout_);
        refresh_static(lbl_button_);
        refresh_static(lbl_checkbox_);
        refresh_static(lbl_radio_);
        refresh_static(lbl_edit_);
        refresh_static(lbl_listbox_);
        refresh_static(lbl_combobox_);
        refresh_static(lbl_listview_);
        refresh_static(lbl_treeview_);
        refresh_static(lbl_iconview_);
        refresh_static(lbl_progress_);
        refresh_static(lbl_layout_static_);
        refresh_static(lbl_panel_splitter_);
        refresh_static(static_left_);
        refresh_static(static_center_);
        refresh_static(static_right_);

        btn_primary_.set_palette(&palette_);
        btn_secondary_.set_palette(&palette_);
        btn_disabled_.set_palette(&palette_);
        chk_unchecked_.set_palette(&palette_);
        chk_checked_.set_palette(&palette_);
        chk_indeterminate_.set_palette(&palette_);
        rad_first_.set_palette(&palette_);
        rad_second_.set_palette(&palette_);
        rad_third_.set_palette(&palette_);
        edit_.set_palette(&palette_);
        listbox_.set_palette(&palette_);
        combobox_.set_palette(&palette_);
        listview_.set_palette(&palette_);
        treeview_.set_palette(&palette_);
        icon_home_.set_palette(&palette_);
        icon_gear_.set_palette(&palette_);
        icon_folder_.set_palette(&palette_);
        icon_user_.set_palette(&palette_);
        progress_.set_palette(&palette_);
        tabs_.set_palette(&palette_);
        layout_panel_.set_palette(&palette_);
        layout_splitter_.set_palette(&palette_);
        status_.set_palette(&palette_);

        // ProgressBar accent fill and panel border need a palette-aware override.
        nfui::FrameStyle progress_style{};
        progress_style.bar_color = palette_.accent;
        progress_.set_style(progress_style);
        nfui::FrameStyle panel_style{};
        panel_style.surface_brush = palette_.surface;
        panel_style.accent        = palette_.border;
        layout_panel_.set_style(panel_style);

        // Re-tint vector glyphs to the new accent.
        icon_home_.set_glyph(nfui::IconKind::search,  palette_.accent);
        icon_gear_.set_glyph(nfui::IconKind::gear,    palette_.accent);
        icon_folder_.set_glyph(nfui::IconKind::info,   palette_.accent);
        icon_user_.set_glyph(nfui::IconKind::warning, palette_.accent);

        tab_ctx_.palette = &palette_;

        // Active toggle button gets semibold emphasis.
        nfui::ButtonStyle active_style{};
        active_style.use_semibold = true;
        nfui::ButtonStyle normal_style{};
        theme_light_.set_style(mode_ == nfui::ThemeMode::light       ? active_style : normal_style);
        theme_dark_.set_style(mode_ == nfui::ThemeMode::dark        ? active_style : normal_style);
        theme_hc_.set_style(mode_ == nfui::ThemeMode::high_contrast ? active_style : normal_style);

        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font = fonts_.regular(dpi_value, nfui::font_pt::base);

        auto set_font = [&](HWND h) {
            if (h != nullptr) {
                SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
            }
        };
        set_font(listbox_.hwnd());
        set_font(combobox_.hwnd());
        set_font(edit_.hwnd());
        set_font(listview_.hwnd());
        set_font(treeview_.hwnd());
        set_font(tabs_.hwnd());
        set_font(status_.hwnd());

        const HFONT btn_font = fonts_.semibold(dpi_value, nfui::font_pt::base);
        auto set_btn_font = [&](HWND h) {
            if (h != nullptr) {
                SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(btn_font), TRUE);
            }
        };
        set_btn_font(theme_light_.hwnd());
        set_btn_font(theme_dark_.hwnd());
        set_btn_font(theme_hc_.hwnd());
        set_btn_font(btn_primary_.hwnd());
        set_btn_font(btn_secondary_.hwnd());
        set_btn_font(btn_disabled_.hwnd());
    }

    void paint_background(HDC target, const RECT& client) {
        nfui::fill_rect(target, client, palette_.background);

        const int dpi_value = dpi_.dpi();
        HFONT title_font = fonts_.bold(dpi_value, nfui::font_pt::xl);
        HFONT mode_font  = fonts_.regular(dpi_value, nfui::font_pt::base);

        // Window title, left.
        RECT title_rc{client.left + dpi_.logical_to_pixels(kOuterMargin),
                      client.top + dpi_.logical_to_pixels(kOuterMargin),
                      client.right - dpi_.logical_to_pixels(360),
                      client.top + dpi_.logical_to_pixels(kTopBarHeight)};
        nfui::draw_text(target, title_rc, L"Theme Gallery", title_font,
                        palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Active mode, right (beside the toggle buttons).
        const wchar_t* mode_text =
            (mode_ == nfui::ThemeMode::high_contrast) ? L"Active mode: High Contrast"
            : (mode_ == nfui::ThemeMode::dark)       ? L"Active mode: Dark"
                                                     : L"Active mode: Light";
        RECT mode_rc{client.left + dpi_.logical_to_pixels(kOuterMargin) + dpi_.logical_to_pixels(360),
                     client.top + dpi_.logical_to_pixels(kOuterMargin),
                     client.right - dpi_.logical_to_pixels(kOuterMargin) - dpi_.logical_to_pixels(300),
                     client.top + dpi_.logical_to_pixels(kTopBarHeight)};
        nfui::draw_text(target, mode_rc, mode_text, mode_font,
                        palette_.text_secondary,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void switch_mode(nfui::ThemeMode new_mode) noexcept {
        if (new_mode == mode_) return;
        mode_    = new_mode;
        palette_ = nfui::theme_palette(mode_);
        apply_palette_to_all();
        apply_native_fonts();
    }

    void apply_window_icon() noexcept {
        if (!resources_.has_icon(IDI_NFUI_APP)) return;
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
        if (big != nullptr)   SendMessageW(hwnd(), WM_SETICON, ICON_BIG,   reinterpret_cast<LPARAM>(big));
        if (small != nullptr) SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));
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
    nfui::Button theme_light_;
    nfui::Button theme_dark_;
    nfui::Button theme_hc_;

    CardPanel card_form_;
    CardPanel card_lists_;
    CardPanel card_feedback_;
    CardPanel card_tabs_;
    CardPanel card_layout_;

    nfui::StaticText title_form_;
    nfui::StaticText title_lists_;
    nfui::StaticText title_feedback_;
    nfui::StaticText title_tabs_;
    nfui::StaticText title_layout_;

    nfui::StaticText lbl_button_;
    nfui::Button     btn_primary_;
    nfui::Button     btn_secondary_;
    nfui::Button     btn_disabled_;
    nfui::StaticText lbl_checkbox_;
    nfui::CheckBox   chk_unchecked_;
    nfui::CheckBox   chk_checked_;
    nfui::CheckBox   chk_indeterminate_;
    nfui::StaticText lbl_radio_;
    nfui::RadioButton rad_first_;
    nfui::RadioButton rad_second_;
    nfui::RadioButton rad_third_;
    nfui::StaticText lbl_edit_;
    nfui::Edit       edit_;

    nfui::StaticText lbl_listbox_;
    nfui::ListBox    listbox_;
    nfui::StaticText lbl_combobox_;
    nfui::ComboBox   combobox_;
    nfui::StaticText lbl_listview_;
    nfui::ListView   listview_;
    nfui::StaticText lbl_treeview_;
    nfui::TreeView   treeview_;

    nfui::StaticText lbl_iconview_;
    nfui::IconView   icon_home_;
    nfui::IconView   icon_gear_;
    nfui::IconView   icon_folder_;
    nfui::IconView   icon_user_;
    nfui::StaticText lbl_progress_;
    nfui::ProgressBar progress_;

    nfui::TabControl tabs_;
    TabCoralContext tab_ctx_{};

    nfui::StaticText lbl_layout_static_;
    nfui::StaticText static_left_;
    nfui::StaticText static_center_;
    nfui::StaticText static_right_;
    nfui::StaticText lbl_panel_splitter_;
    nfui::Panel      layout_panel_;
    nfui::Splitter   layout_splitter_;

    nfui::StatusBar  status_;

    nfui::ThemeMode  mode_;
    nfui::ThemePalette palette_;
    nfui::FontCache  fonts_;
    nfui::DpiScale   dpi_{96};
    HICON            app_icon_{};

    RECT rc_card_form_{};
    RECT rc_card_lists_{};
    RECT rc_card_feedback_{};
    RECT rc_card_tabs_{};
    RECT rc_card_layout_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    ThemeDemoWindow window(instance);
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}
