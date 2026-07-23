// NativeFrameUIThemeDemo
//
// CP32 polish pass: renders every supported control inside a card-grid
// shell that matches the AI-generated reference (Theme Gallery, three
// columns of grouped controls, rounded card borders, small section
// chips, xs/sm/base/md/lg/xl font tokens, neutral cream surface).
// Public APIs and control behaviour are unchanged — this file only
// rearranges the layout, the labels, and the surrounding chrome.

#include <nfui/NativeFrameUI.hpp>
#include <nfui/VectorIcon.hpp>

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

constexpr int id_ok_button      = 201;
constexpr int id_cancel_button  = 202;
constexpr int id_skip_button    = 203;
constexpr int id_disabled_button = 204;
constexpr int id_check_a        = 205;
constexpr int id_check_b        = 206;
constexpr int id_check_c        = 207;
constexpr int id_radio_a        = 208;
constexpr int id_radio_b        = 209;
constexpr int id_radio_c        = 210;
constexpr int id_edit           = 211;
constexpr int id_static_left    = 212;
constexpr int id_static_center  = 213;
constexpr int id_static_right   = 214;
constexpr int id_listbox        = 215;
constexpr int id_combobox       = 216;
constexpr int id_listview       = 217;
constexpr int id_treeview       = 218;
constexpr int id_iconview       = 219;
constexpr int id_progress       = 220;
constexpr int id_status         = 221;
constexpr int id_tabs           = 222;
constexpr int id_panel          = 223;
constexpr int id_splitter       = 224;

// Layout rhythm — logical pixels, scaled to device pixels via DpiScale.
constexpr int kOuterMargin        = 16;
constexpr int kTitleBarHeight     = 64;
constexpr int kCardRadius         = 10;
constexpr int kChipHeight         = 22;
constexpr int kCardPadding        = 16;
constexpr int kRowGap             = 16;
constexpr int kCardBorderWidth    = 1;

// Card grid: 3 columns × 2 rows. Top row spans the full grid; bottom-left
// holds the Layout card; bottom-middle/right remain empty.
constexpr int kColumns            = 3;

constexpr const wchar_t* kChipForm     = L"Form Controls";
constexpr const wchar_t* kChipLists    = L"Lists";
constexpr const wchar_t* kChipFeedback = L"Feedback";
constexpr const wchar_t* kChipLayout   = L"Layout";

constexpr const wchar_t* kLabelButton      = L"Button";
constexpr const wchar_t* kLabelCheckBox    = L"CheckBox";
constexpr const wchar_t* kLabelRadio       = L"RadioButton";
constexpr const wchar_t* kLabelEdit        = L"Edit";
constexpr const wchar_t* kLabelStatic      = L"StaticText";
constexpr const wchar_t* kLabelListBox     = L"ListBox";
constexpr const wchar_t* kLabelComboBox    = L"ComboBox";
constexpr const wchar_t* kLabelListView    = L"ListView";
constexpr const wchar_t* kLabelTreeView    = L"TreeView";
constexpr const wchar_t* kLabelIconView    = L"IconView";
constexpr const wchar_t* kLabelProgressBar = L"ProgressBar";
constexpr const wchar_t* kLabelTabControl  = L"TabControl";
constexpr const wchar_t* kLabelSegmented   = L"Segmented";
constexpr const wchar_t* kLabelSplitter    = L"Panel + Splitter";

struct DemoControls {
    std::unique_ptr<nfui::Button>      ok;
    std::unique_ptr<nfui::Button>      cancel;
    std::unique_ptr<nfui::Button>      skip;
    std::unique_ptr<nfui::Button>      disabled;
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
    std::array<std::unique_ptr<nfui::IconView>, 4> iconviews;
    std::unique_ptr<nfui::ProgressBar> progress;
    std::unique_ptr<nfui::StatusBar>   status;
    std::unique_ptr<nfui::TabControl>  tabs;
    std::unique_ptr<nfui::Panel>       panel;
    std::unique_ptr<nfui::Splitter>    splitter;
};

struct CardLayout {
    RECT form{};
    RECT lists{};
    RECT feedback{};
    RECT layout{};
    int  row_h{};
};

[[nodiscard]] RECT make_rect(int left, int top, int width, int height) noexcept {
    RECT rect{};
    rect.left = left;
    rect.top = top;
    rect.right = left + (width > 0 ? width : 0);
    rect.bottom = top + (height > 0 ? height : 0);
    return rect;
}

[[nodiscard]] RECT inset_rect(const RECT& rect, int left, int top, int right, int bottom) noexcept {
    RECT inset = rect;
    inset.left += left;
    inset.top += top;
    inset.right -= right;
    inset.bottom -= bottom;
    return inset;
}

[[nodiscard]] int rect_width(const RECT& r) noexcept { return r.right - r.left; }
[[nodiscard]] int rect_height(const RECT& r) noexcept { return r.bottom - r.top; }

struct RowRect {
    RECT label{};
    RECT area{};
};

// CP32: a tiny chrome subclass stacked on top of the existing TabControl
// subclass. SetWindowSubclass returns TRUE because each install uses a
// unique subclass_id; chaining is done through DefSubclassProc. The
// subclass only draws the 2 px coral bottom highlight under the selected
// tab after the OS + nfui paint the strip.
LRESULT CALLBACK tab_highlight_proc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam, UINT_PTR subclass_id,
                                    DWORD_PTR ref_data) noexcept {
    (void)subclass_id;
    (void)ref_data;
    LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
    if (message == WM_PAINT) {
        // Read the current tab + selected tab's rect and the palette the
        // demo window exposed via the window's GWLP_USERDATA slot. The
        // subclass stays palette-agnostic; we only know the highlight
        // colour through ref_data which the demo passes in.
        const nfui::ThemePalette* palette =
            reinterpret_cast<const nfui::ThemePalette*>(ref_data);
        if (palette == nullptr) return result;
        const int sel = static_cast<int>(TabCtrl_GetCurSel(hwnd));
        if (sel < 0) return result;
        RECT item{};
        if (TabCtrl_GetItemRect(hwnd, sel, &item) == FALSE) return result;
        HDC dc = GetDC(hwnd);
        if (dc == nullptr) return result;
        const RECT strip = make_rect(item.left, item.bottom - 2,
                                     item.right - item.left, 2);
        if (HBRUSH brush = CreateSolidBrush(palette->accent.rgb)) {
            FillRect(dc, &strip, brush);
            DeleteObject(brush);
        }
        ReleaseDC(hwnd, dc);
    }
    return result;
}

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

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIThemeDemoWindow",
            L"NativeFrame UI ThemeDemo",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1100,
            820,
        };

        if (!create(params)) {
            return false;
        }

        apply_window_icon();
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));

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
            InvalidateRect(hwnd(), nullptr, FALSE);
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

private:
    [[nodiscard]] int outer_margin() const noexcept { return dpi_.logical_to_pixels(kOuterMargin); }
    [[nodiscard]] int title_bar_height() const noexcept { return dpi_.logical_to_pixels(kTitleBarHeight); }
    [[nodiscard]] int card_padding() const noexcept { return dpi_.logical_to_pixels(kCardPadding); }
    [[nodiscard]] int row_gap() const noexcept { return dpi_.logical_to_pixels(kRowGap); }
    [[nodiscard]] int card_radius() const noexcept { return dpi_.logical_to_pixels(kCardRadius); }
    [[nodiscard]] int chip_height() const noexcept { return dpi_.logical_to_pixels(kChipHeight); }
    [[nodiscard]] int card_border() const noexcept { return dpi_.logical_to_pixels(kCardBorderWidth); }

    [[nodiscard]] CardLayout compute_cards(const RECT& client) const noexcept {
        CardLayout cards{};
        const int outer = outer_margin();
        const int gap = row_gap();
        const int title_h = title_bar_height();
        const int usable_w = client.right - 2 * outer;
        const int col_w = (usable_w - gap * (kColumns - 1)) / kColumns;

        const int cards_top = client.top + outer + title_h + gap;
        const int usable_h = (client.bottom - outer) - cards_top;
        const int row_h = (usable_h - gap) / 2;
        cards.row_h = row_h;

        cards.form     = make_rect(client.left + outer, cards_top, col_w, row_h);
        cards.lists    = make_rect(cards.form.right + gap, cards_top, col_w, row_h);
        cards.feedback = make_rect(cards.lists.right + gap, cards_top, col_w, row_h);
        cards.layout   = make_rect(client.left + outer, cards.form.bottom + gap, col_w, row_h);
        return cards;
    }

    [[nodiscard]] RowRect row_within(const RECT& card, int index) const noexcept {
        RowRect row{};
        const int pad = card_padding();
        const int label_h = dpi_.logical_to_pixels(16);
        const int row_inner_h = dpi_.logical_to_pixels(36);
        const int row_y = card.top + pad + (label_h + dpi_.logical_to_pixels(8)) * index;
        row.label = make_rect(card.left + pad, row_y, rect_width(card) - 2 * pad, label_h);
        row.area = make_rect(card.left + pad, row_y + label_h, rect_width(card) - 2 * pad, row_inner_h);
        return row;
    }

    void build_demo() {
        clear_demo();

        // Buttons row.
        g_demo.ok = std::make_unique<nfui::Button>();
        {
            nfui::ButtonStyle style{};
            style.use_semibold = true;
            g_demo.ok->set_style(style);
        }
        g_demo.cancel = std::make_unique<nfui::Button>();
        {
            nfui::ButtonStyle style{};
            style.secondary = true;
            style.use_semibold = true;
            style.border_color = palette_.accent;
            g_demo.cancel->set_style(style);
        }
        g_demo.skip = std::make_unique<nfui::Button>();
        {
            nfui::ButtonStyle style{};
            style.secondary = true;
            style.use_semibold = true;
            style.border_color = palette_.background;
            g_demo.skip->set_style(style);
        }
        g_demo.disabled = std::make_unique<nfui::Button>();
        {
            nfui::ButtonStyle style{};
            style.secondary = true;
            style.use_semibold = true;
            g_demo.disabled->set_style(style);
        }
        if (!create_child(*g_demo.ok,       id_ok_button,       L"OK"))       return;
        if (!create_child(*g_demo.cancel,   id_cancel_button,   L"Cancel"))   return;
        if (!create_child(*g_demo.skip,     id_skip_button,     L"Skip"))     return;
        if (!create_child(*g_demo.disabled, id_disabled_button, L"Disabled")) return;
        EnableWindow(g_demo.disabled->hwnd(), FALSE);

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
        if (!create_child(*g_demo.radio_first,  id_radio_a, L"Option A")) return;
        if (!create_child(*g_demo.radio_second, id_radio_b, L"Option B")) return;
        if (!create_child(*g_demo.radio_third,  id_radio_c, L"Option C")) return;
        SendMessageW(g_demo.radio_first->hwnd(), BM_SETCHECK, BST_CHECKED, 0);

        // Edit.
        g_demo.edit = std::make_unique<nfui::Edit>();
        if (!create_child(*g_demo.edit, id_edit, L"editable sample")) return;

        // StaticText — three alignments.
        g_demo.static_left   = std::make_unique<nfui::StaticText>();
        g_demo.static_center = std::make_unique<nfui::StaticText>();
        g_demo.static_right  = std::make_unique<nfui::StaticText>();
        if (!create_child(*g_demo.static_left,   id_static_left,   L"Left aligned"))    return;
        if (!create_child(*g_demo.static_center, id_static_center, L"Center aligned"))  return;
        if (!create_child(*g_demo.static_right,  id_static_right,  L"Right aligned"))   return;
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
        ListBox_AddString(g_demo.listbox->hwnd(), L"Alice");
        ListBox_AddString(g_demo.listbox->hwnd(), L"Bob");
        ListBox_AddString(g_demo.listbox->hwnd(), L"Charlie");
        ListBox_AddString(g_demo.listbox->hwnd(), L"David");
        ListBox_AddString(g_demo.listbox->hwnd(), L"Emma");
        SendMessageW(g_demo.listbox->hwnd(), LB_SETCURSEL, 2, 0);

        // ComboBox.
        g_demo.combobox = std::make_unique<nfui::ComboBox>();
        if (!create_child(*g_demo.combobox, id_combobox, L"")) return;
        ComboBox_AddString(g_demo.combobox->hwnd(), L"Red");
        ComboBox_AddString(g_demo.combobox->hwnd(), L"Green");
        ComboBox_AddString(g_demo.combobox->hwnd(), L"Blue");
        ComboBox_AddString(g_demo.combobox->hwnd(), L"Yellow");
        ComboBox_AddString(g_demo.combobox->hwnd(), L"Purple");
        SendMessageW(g_demo.combobox->hwnd(), CB_SETCURSEL, 0, 0);

        // ListView (3 cols x 3 rows).
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
        insert_listview_column(g_demo.listview->hwnd(), 0, L"Name",  72);
        insert_listview_column(g_demo.listview->hwnd(), 1, L"Type",  60);
        insert_listview_column(g_demo.listview->hwnd(), 2, L"Owner", 60);
        insert_listview_row(g_demo.listview->hwnd(), 0, L"file.txt",   L"File",  L"User");
        insert_listview_row(g_demo.listview->hwnd(), 1, L"image.png",  L"Image", L"Admin");
        insert_listview_row(g_demo.listview->hwnd(), 2, L"document.pdf", L"PDF", L"User");

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
        tree_item_insert(g_demo.treeview->hwnd(), root, TVI_LAST, L"Docs");
        TreeView_Expand(g_demo.treeview->hwnd(), root, TVE_EXPAND);

        // IconView — 4 small vector icons in a single row.
        {
            nfui::IconViewStyle style{};
            style.pixel_size = 28;
            style.padding    = 4;
            style.background = nfui::Color{palette_.background.rgb};
            const nfui::IconKind kinds[4] = {
                nfui::IconKind::info,
                nfui::IconKind::gear,
                nfui::IconKind::search,
                nfui::IconKind::warning,
            };
            for (int i = 0; i < 4; ++i) {
                g_demo.iconviews[i] = std::make_unique<nfui::IconView>();
                g_demo.iconviews[i]->set_style(style);
                if (!create_child(*g_demo.iconviews[i],
                                  id_iconview + i,
                                  L"")) return;
                g_demo.iconviews[i]->set_glyph(kinds[i], palette_.accent);
            }
        }

        // ProgressBar.
        g_demo.progress = std::make_unique<nfui::ProgressBar>();
        g_demo.progress->set_style(nfui::FrameStyle{
            .surface_brush = palette_.background,
            .bar_color     = palette_.accent,
        });
        if (!create_child(*g_demo.progress, id_progress, L"")) return;
        SendMessageW(g_demo.progress->hwnd(), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(g_demo.progress->hwnd(), PBM_SETPOS, 65, 0);

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
        TabCtrl_SetCurSel(g_demo.tabs->hwnd(), 0);
        // CP32: stack a small chrome subclass on top of the existing
        // TabControl subclass to paint the 2 px coral bottom highlight under
        // the selected tab. The chained DefSubclassProc reaches the
        // nfui tab paint, then our subclass paints the highlight on top.
        SetWindowSubclass(g_demo.tabs->hwnd(),
                          &tab_highlight_proc,
                          0xA1B2,
                          reinterpret_cast<DWORD_PTR>(&palette_));

        // Panel + Splitter (chrome demonstration).
        g_demo.panel    = std::make_unique<nfui::Panel>();
        g_demo.splitter = std::make_unique<nfui::Splitter>();
        g_demo.panel->set_style(nfui::FrameStyle{
            .accent        = palette_.border,
            .surface_brush = palette_.background,
        });
        if (!create_child(*g_demo.panel,    id_panel,    L"")) return;
        if (!create_child(*g_demo.splitter, id_splitter, L"")) return;

        apply_native_fonts();
    }

    void layout_demo() {
        if (hwnd() == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        if (g_demo.status != nullptr) {
            SendMessageW(g_demo.status->hwnd(), WM_SIZE, 0, 0);
        }

        const CardLayout cards = compute_cards(client);

        // Form Controls card — Button, CheckBox, RadioButton, Edit rows.
        place_row_buttons(cards.form, 0);
        place_row_in_card(cards.form, 1, {
            {id_check_a, 100},
            {id_check_b, 100},
            {id_check_c, 110},
        });
        place_row_in_card(cards.form, 2, {
            {id_radio_a, 90},
            {id_radio_b, 90},
            {id_radio_c, 90},
        });
        place_row_in_card(cards.form, 3, {
            {id_edit,    0},
        });

        // Lists card — ListBox, ComboBox, ListView, TreeView rows.
        place_row_in_card(cards.lists, 0, {
            {id_listbox,  0},
            {id_combobox, 0},
        });
        place_row_in_card(cards.lists, 1, {
            {id_listview, 0},
            {id_treeview, 0},
        });

        // Feedback card — IconView, ProgressBar, TabControl, Segmented rows.
        place_row_in_card(cards.feedback, 0, {
            {id_iconview + 0, 0},
            {id_iconview + 1, 0},
            {id_iconview + 2, 0},
            {id_iconview + 3, 0},
        });
        place_row_in_card(cards.feedback, 1, {
            {id_progress, 0},
        });
        place_row_in_card(cards.feedback, 2, {
            {id_tabs,     0},
        });
        // Row 3 (segmented) is painted in paint_background — no HWND needed.

        // Layout card — StaticText, Panel + Splitter rows.
        place_row_in_card(cards.layout, 0, {
            {id_static_left,   90},
            {id_static_center, 90},
            {id_static_right,  90},
        });
        place_row_in_card(cards.layout, 1, {
            {id_panel,    0},
            {id_splitter, 0},
        });
    }

    // Place a row of equal-width buttons (Form Controls Button row).
    void place_row_buttons(const RECT& card, int row_index) noexcept {
        const RowRect row = row_within(card, row_index);
        const int n = 4;
        const int slot = rect_width(row.area) / n;
        const int y = row.area.top;
        const int h = rect_height(row.area);
        const int ids[4] = { id_ok_button, id_cancel_button, id_skip_button, id_disabled_button };
        for (int i = 0; i < n; ++i) {
            HWND child = GetDlgItem(hwnd(), ids[i]);
            if (child == nullptr) continue;
            const int x = row.area.left + i * slot;
            const int w = slot - dpi_.logical_to_pixels(8);
            MoveWindow(child, x, y, w, h, TRUE);
        }
    }

    // Place a row inside a card. `sizes` is the per-control width in
    // logical pixels; 0 means "split the remaining width evenly".
    void place_row_in_card(const RECT& card, int row_index,
                           std::initializer_list<std::pair<int, int>> sizes) noexcept {
        const RowRect row = row_within(card, row_index);
        const int total = static_cast<int>(sizes.size());
        if (total == 0) return;
        const int gap = dpi_.logical_to_pixels(12);

        // Count fixed + flex sizes first so we can compute each flex slot.
        int fixed = 0;
        int flex_count = 0;
        for (auto [id, w] : sizes) {
            (void)id;
            if (w > 0) fixed += w;
            else ++flex_count;
        }
        const int available = rect_width(row.area) - gap * (total - 1);
        const int flex_w = (flex_count > 0)
                               ? std::max((available - dpi_.logical_to_pixels(fixed)) / flex_count, dpi_.logical_to_pixels(60))
                               : 0;

        int x = row.area.left;
        const int y = row.area.top;
        const int h = rect_height(row.area);
        for (auto [id, w] : sizes) {
            HWND child = GetDlgItem(hwnd(), id);
            if (child == nullptr) continue;
            const int cw = (w > 0) ? dpi_.logical_to_pixels(w) : flex_w;
            MoveWindow(child, x, y, cw, h, TRUE);
            x += cw + gap;
        }
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font     = fonts_.regular(dpi_value, nfui::font_pt::ui);
        const HFONT body_font   = fonts_.regular(dpi_value, nfui::font_pt::sm);
        const auto set_font = [this](auto& control, HFONT font) {
            if (control != nullptr) {
                SendMessageW(control->hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            }
        };
        set_font(g_demo.ok,             ui_font);
        set_font(g_demo.cancel,         ui_font);
        set_font(g_demo.skip,           ui_font);
        set_font(g_demo.disabled,       ui_font);
        set_font(g_demo.check_unchecked, body_font);
        set_font(g_demo.check_checked,   body_font);
        set_font(g_demo.check_indeterminate, body_font);
        set_font(g_demo.radio_first,     body_font);
        set_font(g_demo.radio_second,    body_font);
        set_font(g_demo.radio_third,     body_font);
        set_font(g_demo.listbox,         body_font);
        set_font(g_demo.combobox,        body_font);
        set_font(g_demo.edit,            body_font);
        set_font(g_demo.listview,        body_font);
        set_font(g_demo.treeview,        body_font);
        for (auto& icon : g_demo.iconviews) {
            set_font(icon, body_font);
        }
        set_font(g_demo.tabs,            body_font);
        set_font(g_demo.status,          ui_font);
    }

    void draw_section_chip(HDC target, const RECT& card, const wchar_t* text) noexcept {
        const int radius = dpi_.logical_to_pixels(6);
        const RECT chip = make_rect(card.left + card_padding(),
                                    card.top + card_padding(),
                                    dpi_.logical_to_pixels(110),
                                    chip_height());
        const nfui::Color chip_bg = nfui::alpha_blend(palette_.border, palette_.surface, 0.45f);
        nfui::fill_rounded_rect(target, chip, radius, chip_bg, chip_bg);
        RECT chip_text = chip;
        chip_text.left  += dpi_.logical_to_pixels(10);
        chip_text.right -= dpi_.logical_to_pixels(10);
        nfui::draw_text(target,
                        chip_text,
                        text,
                        fonts_.semibold(dpi_.dpi(), nfui::font_pt::xs),
                        palette_.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void draw_row_label(HDC target, const RowRect& row, const wchar_t* text) noexcept {
        nfui::draw_text(target,
                        row.label,
                        text,
                        fonts_.semibold(dpi_.dpi(), nfui::font_pt::xs),
                        palette_.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void draw_card_chrome(HDC target, const RECT& card) noexcept {
        // Subtle drop shadow + rounded surface fill + 1 px border.
        nfui::paint_drop_shadow(target, card, card_radius(), 1, palette_.shadow);
        nfui::fill_rounded_rect(target, card, card_radius(), palette_.surface, palette_.border);
    }

    void draw_segmented(HDC target, const RECT& card, int row_index) noexcept {
        const RowRect row = row_within(card, row_index);
        const int segment_radius = dpi_.logical_to_pixels(6);
        const int n = 3;
        const int slot = rect_width(row.area) / n;
        const int h = rect_height(row.area);
        const wchar_t* labels[3] = { L"Day", L"Week", L"Month" };
        for (int i = 0; i < n; ++i) {
            const int x = row.area.left + i * slot;
            const int w = slot - dpi_.logical_to_pixels(8);
            const RECT seg = make_rect(x, row.area.top, w, h);
            const bool selected = (i == 1);  // "Week" pre-selected to mirror the reference
            const nfui::Color fill   = selected ? palette_.accent : palette_.background;
            const nfui::Color border = selected ? palette_.accent : palette_.border;
            const nfui::Color text_c = selected ? palette_.accent_text : palette_.text;
            nfui::fill_rounded_rect(target, seg, segment_radius, fill, border);
            nfui::draw_text(target,
                            seg,
                            labels[i],
                            fonts_.semibold(dpi_.dpi(), nfui::font_pt::sm),
                            text_c,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }

    void paint_background(HDC target, const RECT& client) {
        nfui::fill_rect(target, client, palette_.background);

        const CardLayout cards = compute_cards(client);

        // Title bar.
        const int title_h = title_bar_height();
        const RECT title_rect = make_rect(client.left + outer_margin(),
                                          client.top + outer_margin(),
                                          rect_width(client) - 2 * outer_margin(),
                                          title_h);
        nfui::draw_text(target,
                        title_rect,
                        L"Theme Gallery",
                        fonts_.bold(dpi_.dpi(), nfui::font_pt::xl),
                        palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Active mode footer on the right of the title bar.
        const wchar_t* mode_text =
            (mode_ == nfui::ThemeMode::high_contrast) ? L"Active mode: High Contrast"
            : (mode_ == nfui::ThemeMode::dark)       ? L"Active mode: Dark"
                                                      : L"Active mode: Light";
        RECT mode_rect = title_rect;
        mode_rect.left = title_rect.right - dpi_.logical_to_pixels(220);
        mode_rect.right = title_rect.right;
        // 8 px coral dot on the right of the active-mode caption.
        const int dot_size = dpi_.logical_to_pixels(8);
        const RECT dot_rect = make_rect(mode_rect.right - dot_size - dpi_.logical_to_pixels(2),
                                        mode_rect.top + (rect_height(mode_rect) - dot_size) / 2,
                                        dot_size, dot_size);
        nfui::fill_ellipse(target, dot_rect, palette_.accent);
        RECT mode_text_rect = mode_rect;
        mode_text_rect.right = dot_rect.left - dpi_.logical_to_pixels(8);
        nfui::draw_text(target,
                        mode_text_rect,
                        mode_text,
                        fonts_.regular(dpi_.dpi(), nfui::font_pt::sm),
                        palette_.text_secondary,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Hairline under the title bar.
        const int title_bottom = title_rect.bottom + dpi_.logical_to_pixels(4);
        nfui::draw_line(target,
                        POINT{client.left + outer_margin(), title_bottom},
                        POINT{client.right - outer_margin(), title_bottom},
                        palette_.border,
                        1);

        // Cards.
        draw_card_chrome(target, cards.form);
        draw_card_chrome(target, cards.lists);
        draw_card_chrome(target, cards.feedback);
        draw_card_chrome(target, cards.layout);

        // Section chips + row labels.
        draw_section_chip(target, cards.form,     kChipForm);
        draw_section_chip(target, cards.lists,    kChipLists);
        draw_section_chip(target, cards.feedback, kChipFeedback);
        draw_section_chip(target, cards.layout,   kChipLayout);

        draw_row_label(target, row_within(cards.form, 0), kLabelButton);
        draw_row_label(target, row_within(cards.form, 1), kLabelCheckBox);
        draw_row_label(target, row_within(cards.form, 2), kLabelRadio);
        draw_row_label(target, row_within(cards.form, 3), kLabelEdit);

        draw_row_label(target, row_within(cards.lists, 0), kLabelListBox);
        draw_row_label(target, row_within(cards.lists, 1), kLabelListView);

        draw_row_label(target, row_within(cards.feedback, 0), kLabelIconView);
        draw_row_label(target, row_within(cards.feedback, 1), kLabelProgressBar);
        draw_row_label(target, row_within(cards.feedback, 2), kLabelTabControl);
        draw_row_label(target, row_within(cards.feedback, 3), kLabelSegmented);

        draw_row_label(target, row_within(cards.layout, 0), kLabelStatic);
        draw_row_label(target, row_within(cards.layout, 1), kLabelSplitter);

        // Segmented control (custom-painted, no HWND). Icon strip is owned
        // by 4 IconView HWNDs placed in layout_demo.
        draw_segmented(target, cards.feedback, 3);

        // ProgressBar percent label — small caption aligned to the right
        // of the row label, mirroring the reference layout.
        const RowRect progress_row = row_within(cards.feedback, 1);
        const RECT pct_rect = make_rect(progress_row.label.right - dpi_.logical_to_pixels(48),
                                        progress_row.label.top,
                                        dpi_.logical_to_pixels(48),
                                        dpi_.logical_to_pixels(16));
        nfui::draw_text(target,
                        pct_rect,
                        L"65%",
                        fonts_.semibold(dpi_.dpi(), nfui::font_pt::sm),
                        palette_.text,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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
        // Drop the unique_ptrs first so the TabControl subclass is removed
        // before we tear down palette_ (the highlight subclass stores a
        // pointer to palette_ in its ref_data).
        if (g_demo.tabs != nullptr) {
            RemoveWindowSubclass(g_demo.tabs->hwnd(), &tab_highlight_proc, 0xA1B2);
        }
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
    nfui::ThemeMode mode_;
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::DpiScale dpi_{96};
    HICON app_icon_{};
    DemoControls g_demo{};
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