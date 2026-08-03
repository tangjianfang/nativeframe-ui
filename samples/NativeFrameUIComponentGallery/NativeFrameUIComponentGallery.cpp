// NativeFrameUIComponentGallery
//
// V1.0 capability: the canonical "kitchen sink" surface. Every control class
// the framework ships, shown in its expected state, plus a per-control
// interaction-state matrix.

#ifndef NOMINMAX
// Compiler options already define NOMINMAX on the command line; guard the
// local definition so the two cannot collide (C4005).
#define NOMINMAX
#endif
#include <nfui/NativeFrameUI.hpp>
#include <nfui/ThemeBroker.hpp>
#include <nfui/design_tokens.hpp>

#include "NativeFrameUIResource.h"
#include "StateMatrix.hpp"

#include <algorithm>
#include <commctrl.h>
#include <cstddef>
#include <windows.h>
#include <windowsx.h>

namespace {

namespace tok = nfui::design;

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
constexpr int id_propertygrid    = 124;
// CP-B3: header theme chips. Mirror the ID_THEME_* tokens in
// <nfui/ThemeBroker.hpp> so the command handler is a one-liner.
constexpr int id_theme_light     = 130;
constexpr int id_theme_dark      = 131;
constexpr int id_theme_hc        = 132;

// CP-B3: logical layout constants, all derived from the design tokens so a
// token change re-flows the whole page. `_l` suffix = logical units.
constexpr int col_inputs_l   = 288;   // fixed left column
constexpr int col_matrix_l   = 560;   // fixed right column (6 state cells + label gutter)
constexpr int col_min_mid_l  = 240;   // middle column floor before it starts clipping
constexpr int header_band_l  = 56;    // title + subtitle band
constexpr int eyebrow_h_l    = tok::spacing_md;      // 16
constexpr int card_title_h_l = tok::control_height_sm; // 24
constexpr int chip_w_l       = 72;
constexpr int matrix_row_h_l = 44;
constexpr int matrix_label_l = 104;

// Painted label + its rect. Collected during layout so WM_PAINT and
// MoveWindow always agree on where a section header sits.
struct LabelSlot {
    RECT rect{};
    const wchar_t* text{nullptr};
};

// Full page geometry in device pixels, computed once per layout / paint pass.
struct GalleryLayout {
    RECT title{};
    RECT subtitle{};
    RECT chip[3]{};

    RECT card_rect[7]{};
    LabelSlot card_title[7]{};
    int  card_count{0};

    LabelSlot eyebrow[13]{};
    int  eyebrow_count{0};

    RECT button[3]{};
    RECT check[3]{};
    RECT radio[3]{};
    RECT statics[3]{};
    RECT edit{};
    RECT combo{};
    RECT progress{};
    RECT icon{};
    RECT listbox{};
    RECT listview{};
    RECT treeview{};
    RECT panel{};
    RECT splitter{};
    RECT tabs{};
    RECT propertygrid{};

    RECT matrix_grid{};   // area holding the column headers + 7 rows

    void push_card(const RECT& r, const wchar_t* text, const RECT& title_rect) noexcept {
        if (card_count >= 7) return;
        card_rect[card_count]  = r;
        card_title[card_count] = LabelSlot{title_rect, text};
        ++card_count;
    }
    void push_eyebrow(const RECT& r, const wchar_t* text) noexcept {
        if (eyebrow_count >= 13) return;
        eyebrow[eyebrow_count] = LabelSlot{r, text};
        ++eyebrow_count;
    }
};

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

    // Seeds the palette (and the process-wide broker) before create_main
    // wires the palette pointer into every wrapper, so `--theme dark` paints
    // dark on the very first frame instead of flashing light.
    void set_initial_theme(nfui::ThemeMode mode) noexcept {
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        nfui::ThemeBroker::instance().set_theme(mode);
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIComponentGalleryWindow",
            L"NativeFrame UI ComponentGallery",
            // CP-B3: WS_VSCROLL dropped. The old single tall column needed a
            // native scrollbar (which the audit flagged as "traditional
            // Win32" chrome) and still truncated the last section because no
            // WM_VSCROLL handler was ever wired up. The three-column card
            // layout below fits the default viewport, so there is nothing to
            // scroll and no scrollbar to theme.
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1240,
            1120,
        };

        if (!create(params)) {
            return false;
        }

        apply_window_icon();
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        rescale_window();

        nfui::ThemeBroker::instance().register_hwnd(
            hwnd(), [this](nfui::ThemeMode mode) { apply_theme(mode); });

        if (!create_children()) {
            return false;
        }
        apply_native_fonts();
        layout_children();

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    bool on_command(int command_id, HWND, UINT) override {
        switch (command_id) {
        case id_theme_light:
            nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::light);
            return true;
        case id_theme_dark:
            nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::dark);
            return true;
        case id_theme_hc:
            nfui::ThemeBroker::instance().set_theme(nfui::ThemeMode::high_contrast);
            return true;
        default:
            return false;
        }
    }

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
        // ThemeBroker broadcasts WM_THEMECHANGED to every registered HWND on
        // set_theme(). The registered callback already ran apply_theme(); this
        // arm keeps the window correct when an external broker targets it.
        case WM_THEMECHANGED:
            apply_theme(nfui::ThemeBroker::instance().current());
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
                paint_background(target, client);
            }
            EndPaint(hwnd(), &paint);
            return 0;
        }
        case WM_NCDESTROY:
            nfui::ThemeBroker::instance().unregister_hwnd(hwnd());
            return nfui::Window::handle_message(message, wparam, lparam);
        case WM_DESTROY:
            destroy_icon();
            PostQuitMessage(0);
            return 0;
        default:
            return nfui::Window::handle_message(message, wparam, lparam);
        }
    }

private:
    [[nodiscard]] int px(int logical) const noexcept { return dpi_.logical_to_pixels(logical); }

    // The create-time 1240x900 is expressed in logical units; re-apply it
    // once the real per-window DPI is known so a 150% monitor gets a
    // proportionally larger window instead of a cramped one.
    void rescale_window() noexcept {
        if (dpi_.dpi() == 96) {
            return;
        }
        RECT frame{0, 0, px(1240), px(900)};
        RECT current{};
        if (!GetWindowRect(hwnd(), &current)) {
            return;
        }
        SetWindowPos(hwnd(), nullptr, current.left, current.top,
                     frame.right, frame.bottom,
                     SWP_NOACTIVATE | SWP_NOZORDER);
    }

    template <typename Fn>
    void for_each_control(Fn&& fn) noexcept {
        nfui::Control* all[] = {
            &button_default_, &button_disabled_, &button_hover_,
            &check_unchecked_, &check_checked_, &check_indeterm_,
            &radio_a_, &radio_b_, &radio_c_,
            &edit_, &static_left_, &static_center_, &static_right_,
            &listbox_, &combobox_, &listview_, &treeview_, &iconview_,
            &status_, &tabs_, &progress_, &panel_, &splitter_,
            &property_grid_,
            &chip_light_, &chip_dark_, &chip_hc_,
        };
        for (nfui::Control* control : all) {
            fn(control);
        }
    }

    template <typename ControlT>
    [[nodiscard]] bool init(ControlT& control, int id, std::wstring_view text) noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            id,
            text,
            0, 0, px(100), px(tok::control_height_md),
        };
        control.inject_theme(&palette_, &fonts_);
        return control.create(params);
    }

    [[nodiscard]] bool create_children() noexcept {
        // Header theme chips — the audit's "dark/HC never reach the client
        // area" finding is only verifiable if the user can flip themes here.
        if (!init(chip_light_, id_theme_light, L"Light")) return false;
        if (!init(chip_dark_,  id_theme_dark,  L"Dark"))  return false;
        if (!init(chip_hc_,    id_theme_hc,    L"HC"))    return false;

        // Buttons — primary / disabled / secondary.
        if (!init(button_default_,  id_button_default,  L"Primary"))   return false;
        if (!init(button_disabled_, id_button_disabled, L"Disabled"))  return false;
        if (!init(button_hover_,    id_button_hover,    L"Secondary")) return false;
        EnableWindow(button_disabled_.hwnd(), FALSE);
        nfui::ButtonStyle secondary{};
        secondary.secondary = true;
        button_hover_.set_style(secondary);

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

        if (!init(edit_, id_edit, L"editable sample")) return false;

        // StaticText — three alignments, stacked at full column width so the
        // alignment is actually observable (side-by-side boxes made all three
        // look identical in the audit capture).
        if (!init(static_left_,   id_static_left,   L"Left aligned"))   return false;
        if (!init(static_center_, id_static_center, L"Center aligned")) return false;
        if (!init(static_right_,  id_static_right,  L"Right aligned"))  return false;
        nfui::TextStyle center_style{};
        center_style.align_h = nfui::StaticTextAlignH::center;
        static_center_.set_style(center_style);
        nfui::TextStyle right_style{};
        right_style.align_h = nfui::StaticTextAlignH::right;
        static_right_.set_style(right_style);

        if (!init(listbox_, id_listbox, L"")) return false;
        ListBox_AddString(listbox_.hwnd(), L"One");
        ListBox_AddString(listbox_.hwnd(), L"Two");
        ListBox_AddString(listbox_.hwnd(), L"Three");
        ListBox_AddString(listbox_.hwnd(), L"Four");
        ListBox_AddString(listbox_.hwnd(), L"Five");
        SendMessageW(listbox_.hwnd(), LB_SETCURSEL, 1, 0);

        if (!init(combobox_, id_combobox, L"")) return false;
        ComboBox_AddString(combobox_.hwnd(), L"Red");
        ComboBox_AddString(combobox_.hwnd(), L"Green");
        ComboBox_AddString(combobox_.hwnd(), L"Blue");
        ComboBox_AddString(combobox_.hwnd(), L"Yellow");
        ComboBox_AddString(combobox_.hwnd(), L"Purple");
        SendMessageW(combobox_.hwnd(), CB_SETCURSEL, 0, 0);

        if (!init(listview_, id_listview, L"")) return false;
        ListView_SetExtendedListViewStyle(listview_.hwnd(),
                                          LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        insert_listview_column(listview_.hwnd(), 0, L"Name",   px(96));
        insert_listview_column(listview_.hwnd(), 1, L"Type",   px(72));
        insert_listview_column(listview_.hwnd(), 2, L"Status", px(72));
        insert_listview_row(listview_.hwnd(), 0, L"Button",    L"widget", L"stable");
        insert_listview_row(listview_.hwnd(), 1, L"ListView",  L"view",   L"stable");
        insert_listview_row(listview_.hwnd(), 2, L"TreeView",  L"view",   L"stable");
        insert_listview_row(listview_.hwnd(), 3, L"Splitter",  L"chrome", L"stable");
        ListView_SetItemState(listview_.hwnd(), 0,
                              LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);

        if (!init(treeview_, id_treeview, L"")) return false;
        HTREEITEM root = tree_item_insert(treeview_.hwnd(), TVI_ROOT, TVI_LAST, L"nfui");
        HTREEITEM controls = tree_item_insert(treeview_.hwnd(), root, TVI_LAST, L"controls");
        tree_item_insert(treeview_.hwnd(), controls, TVI_LAST, L"Button");
        tree_item_insert(treeview_.hwnd(), controls, TVI_LAST, L"TreeView");
        tree_item_insert(treeview_.hwnd(), root, TVI_LAST, L"layout");
        tree_item_insert(treeview_.hwnd(), root, TVI_LAST, L"theme");
        TreeView_Expand(treeview_.hwnd(), root, TVE_EXPAND);
        TreeView_Expand(treeview_.hwnd(), controls, TVE_EXPAND);
        TreeView_SelectItem(treeview_.hwnd(), controls);

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

        if (!init(status_, id_status, L"")) return false;
        set_status_text();

        if (!init(tabs_, id_tabs, L"")) return false;
        static_cast<void>(tabs_.set_padding(px(tok::spacing_md), px(tok::spacing_xs)));
        insert_tab(tabs_.hwnd(), 0, L"Overview");
        insert_tab(tabs_.hwnd(), 1, L"States");
        insert_tab(tabs_.hwnd(), 2, L"Tokens");

        if (!init(progress_, id_progress, L"")) return false;
        SendMessageW(progress_.hwnd(), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(progress_.hwnd(), PBM_SETPOS, 60, 0);

        if (!init(panel_, id_panel, L"")) return false;
        nfui::FrameStyle panel_style{};
        panel_style.elevation = 2;
        panel_.set_style(panel_style);
        if (!init(splitter_, id_splitter, L"")) return false;

        // PropertyGrid — CP43. Four property types exercising the buffered
        // edit model: edits land in the pending layer (accent value text) and
        // are validated before they can commit. Integer Width rejects
        // non-digits; the status bar reports each accepted edit.
        if (!init(property_grid_, id_propertygrid, L"")) return false;
        {
            nfui::PropertyDef name;
            name.name = L"Name";
            name.type = nfui::PropertyType::string;
            name.value = L"Button";
            property_grid_.add_property(std::move(name));

            nfui::PropertyDef width;
            width.name = L"Width";
            width.type = nfui::PropertyType::integer;
            width.value = L"120";
            property_grid_.add_property(std::move(width));

            nfui::PropertyDef visible;
            visible.name = L"Visible";
            visible.type = nfui::PropertyType::boolean;
            visible.value = L"true";
            property_grid_.add_property(std::move(visible));

            nfui::PropertyDef anchor;
            anchor.name = L"Anchor";
            anchor.type = nfui::PropertyType::choice;
            anchor.value = L"Left";
            anchor.choices = {L"Left", L"Top", L"Right", L"Bottom", L"Fill"};
            property_grid_.add_property(std::move(anchor));

            property_grid_.select(0);
            property_grid_.set_on_value_changed(
                [this](size_t index, const std::wstring& value) noexcept {
                    update_grid_status(index, value);
                });
        }

        refresh_chip_styles();
        return true;
    }

    void set_status_text() noexcept {
        if (status_.hwnd() == nullptr) {
            return;
        }
        const wchar_t* text = L"Ready  \x2022  21 control classes  \x2022  7 \x00D7 6 interaction-state matrix";
        SendMessageW(status_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text));
    }

    // Reports the latest accepted PropertyGrid edit in the status bar so the
    // buffered-edit model is observable without an Apply button.
    void update_grid_status(size_t index, const std::wstring& value) noexcept {
        if (status_.hwnd() == nullptr) {
            return;
        }
        const std::wstring name = property_grid_.model().at(index).name;
        const std::wstring text = L"PropertyGrid  \x2022  " + name + L" = " + value + L"  (pending)";
        SendMessageW(status_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }

    // Active chip = accent face; the other two = secondary face. Re-applied on
    // every theme change so the header always reflects the broker's mode.
    void refresh_chip_styles() noexcept {
        auto apply = [](nfui::Button& chip, bool active) noexcept {
            nfui::ButtonStyle style{};
            style.corner_radius = tok::radius_sm;
            style.secondary     = !active;
            style.use_semibold  = active;
            chip.set_style(style);
            if (chip.hwnd() != nullptr) {
                InvalidateRect(chip.hwnd(), nullptr, FALSE);
            }
        };
        apply(chip_light_, mode_ == nfui::ThemeMode::light);
        apply(chip_dark_,  mode_ == nfui::ThemeMode::dark);
        apply(chip_hc_,    mode_ == nfui::ThemeMode::high_contrast);
    }

    void apply_theme(nfui::ThemeMode mode) noexcept {
        if (mode_ == mode) {
            return;
        }
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        // The palette pointer handed to each wrapper is stable (&palette_), so
        // re-injecting is what triggers each control's on_palette_changed +
        // InvalidateRect. Without this the wrappers would keep painting the
        // stale colours they cached at create time.
        for_each_control([this](nfui::Control* control) noexcept {
            control->set_palette(&palette_);
        });
        refresh_chip_styles();
        apply_native_fonts();
        set_status_text();
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void apply_native_fonts() noexcept {
        const int dpi_value = dpi_.dpi();
        const HFONT ui_font = fonts_.regular(dpi_value, nfui::font_pt::sm);
        SendMessageW(listbox_.hwnd(),   WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(combobox_.hwnd(),  WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(edit_.hwnd(),      WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(listview_.hwnd(),  WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(treeview_.hwnd(),  WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(tabs_.hwnd(),      WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        SendMessageW(status_.hwnd(),    WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
    }

    [[nodiscard]] int status_height() const noexcept {
        RECT status_rect{};
        if (status_.hwnd() != nullptr && GetWindowRect(status_.hwnd(), &status_rect)) {
            return status_rect.bottom - status_rect.top;
        }
        return px(tok::control_height_sm);
    }

    // Single source of truth for the page geometry. Called by both
    // layout_children() (MoveWindow) and paint_background() (cards + labels),
    // so painted chrome can never drift from the live HWNDs.
    [[nodiscard]] GalleryLayout compute_layout(const RECT& client) noexcept {
        GalleryLayout out{};

        const int outer    = px(tok::spacing_md);   // 16
        const int col_gap  = px(tok::spacing_lg);   // 24
        const int pad      = px(tok::spacing_md);   // 16
        const int gap_card = px(tok::spacing_md);   // 16
        const int gap_grp  = px(tok::spacing_md);   // 16
        const int gap_row  = px(tok::spacing_sm);   // 8
        const int gap_tiny = px(tok::spacing_xs);   // 4

        const int eyebrow_h = px(eyebrow_h_l);
        const int title_h   = px(card_title_h_l);
        const int row_md    = px(tok::control_height_md);
        const int row_sm    = px(tok::control_height_sm);

        const int left  = client.left + outer;
        const int right = client.right - outer;

        // ---- header band -------------------------------------------------
        const int band_top = client.top + outer;
        const int band_h   = px(header_band_l);
        out.title    = RECT{left, band_top, right, band_top + px(32)};
        out.subtitle = RECT{left, out.title.bottom + px(tok::spacing_xs) / 2,
                            right, band_top + band_h};

        const int chip_w = px(chip_w_l);
        const int chip_h = px(tok::control_height_sm);
        const int chip_y = band_top + (band_h - chip_h) / 2;
        for (int i = 0; i < 3; ++i) {
            const int x = right - (3 - i) * chip_w - (2 - i) * gap_row;
            out.chip[i] = RECT{x, chip_y, x + chip_w, chip_y + chip_h};
        }

        // ---- content band --------------------------------------------------
        const int content_top    = band_top + band_h + px(tok::spacing_md);
        const int content_bottom = client.bottom - status_height() - outer;

        int col1_w = px(col_inputs_l);
        int col3_w = px(col_matrix_l);
        int col2_w = (right - left) - col1_w - col3_w - col_gap * 2;
        // Degrade gracefully when the user shrinks the window: the middle
        // column gives up width first, then the matrix column.
        if (col2_w < px(col_min_mid_l)) {
            const int deficit = px(col_min_mid_l) - col2_w;
            col3_w = (std::max)(px(tok::spacing_lg), col3_w - deficit);
            col2_w = (right - left) - col1_w - col3_w - col_gap * 2;
        }
        if (col2_w < 0) col2_w = 0;

        const int col1_x = left;
        const int col2_x = col1_x + col1_w + col_gap;
        const int col3_x = col2_x + col2_w + col_gap;

        // ---- column 1: Inputs + Feedback ----------------------------------
        {
            const int inner_x = col1_x + pad;
            const int inner_w = col1_w - pad * 2;
            int y = content_top + pad;
            const int card_top = content_top;

            RECT ct{inner_x, y, inner_x + inner_w, y + title_h};
            y += title_h + gap_row;

            // BUTTON
            out.push_eyebrow(RECT{inner_x, y, inner_x + inner_w, y + eyebrow_h}, L"BUTTON");
            y += eyebrow_h + gap_tiny;
            {
                const int bw = (inner_w - gap_row * 2) / 3;
                for (int i = 0; i < 3; ++i) {
                    const int x = inner_x + i * (bw + gap_row);
                    out.button[i] = RECT{x, y, x + bw, y + row_md};
                }
            }
            y += row_md + gap_grp;

            // CHECKBOX — 2-up then full width so "Indeterminate" never clips.
            out.push_eyebrow(RECT{inner_x, y, inner_x + inner_w, y + eyebrow_h}, L"CHECKBOX");
            y += eyebrow_h + gap_tiny;
            {
                const int half = (inner_w - gap_row) / 2;
                out.check[0] = RECT{inner_x, y, inner_x + half, y + row_sm};
                out.check[1] = RECT{inner_x + half + gap_row, y, inner_x + inner_w, y + row_sm};
                out.check[2] = RECT{inner_x, y + row_sm + gap_tiny,
                                    inner_x + inner_w, y + row_sm * 2 + gap_tiny};
            }
            y += row_sm * 2 + gap_tiny + gap_grp;

            // RADIOBUTTON
            out.push_eyebrow(RECT{inner_x, y, inner_x + inner_w, y + eyebrow_h}, L"RADIOBUTTON");
            y += eyebrow_h + gap_tiny;
            {
                const int half = (inner_w - gap_row) / 2;
                out.radio[0] = RECT{inner_x, y, inner_x + half, y + row_sm};
                out.radio[1] = RECT{inner_x + half + gap_row, y, inner_x + inner_w, y + row_sm};
                out.radio[2] = RECT{inner_x, y + row_sm + gap_tiny,
                                    inner_x + inner_w, y + row_sm * 2 + gap_tiny};
            }
            y += row_sm * 2 + gap_tiny + gap_grp;

            // EDIT / COMBOBOX
            out.push_eyebrow(RECT{inner_x, y, inner_x + inner_w, y + eyebrow_h}, L"EDIT / COMBOBOX");
            y += eyebrow_h + gap_tiny;
            out.edit = RECT{inner_x, y, inner_x + inner_w, y + row_md};
            y += row_md + gap_row;
            out.combo = RECT{inner_x, y, inner_x + inner_w, y + row_md};
            y += row_md + gap_grp;

            // STATICTEXT
            out.push_eyebrow(RECT{inner_x, y, inner_x + inner_w, y + eyebrow_h}, L"STATICTEXT");
            y += eyebrow_h + gap_tiny;
            for (int i = 0; i < 3; ++i) {
                out.statics[i] = RECT{inner_x, y + i * row_sm, inner_x + inner_w, y + (i + 1) * row_sm};
            }
            y += row_sm * 3;

            const int card_bottom = y + pad;
            out.push_card(RECT{col1_x, card_top, col1_x + col1_w, card_bottom}, L"Inputs", ct);

            // Feedback card
            const int fb_top = card_bottom + gap_card;
            int fy = fb_top + pad;
            RECT fct{inner_x, fy, inner_x + inner_w, fy + title_h};
            fy += title_h + gap_row;

            out.push_eyebrow(RECT{inner_x, fy, inner_x + inner_w, fy + eyebrow_h}, L"PROGRESSBAR");
            fy += eyebrow_h + gap_tiny;
            out.progress = RECT{inner_x, fy, inner_x + inner_w, fy + row_sm};
            fy += row_sm + gap_grp;

            out.push_eyebrow(RECT{inner_x, fy, inner_x + inner_w, fy + eyebrow_h}, L"ICONVIEW");
            fy += eyebrow_h + gap_tiny;
            out.icon = RECT{inner_x, fy, inner_x + px(32), fy + px(32)};
            fy += px(32);

            out.push_card(RECT{col1_x, fb_top, col1_x + col1_w, (std::min)(fy + pad, content_bottom)},
                          L"Feedback", fct);
        }

        // ---- column 2: Collections + Containers ---------------------------
        {
            const int inner_x = col2_x + pad;
            const int inner_w = col2_w - pad * 2;
            const int card_top = content_top;
            int y = card_top + pad;

            RECT ct{inner_x, y, inner_x + inner_w, y + title_h};
            y += title_h + gap_row;

            out.push_eyebrow(RECT{inner_x, y, inner_x + inner_w, y + eyebrow_h}, L"LISTBOX");
            y += eyebrow_h + gap_tiny;
            out.listbox = RECT{inner_x, y, inner_x + inner_w, y + px(120)};
            y += px(120) + gap_grp;

            out.push_eyebrow(RECT{inner_x, y, inner_x + inner_w, y + eyebrow_h}, L"LISTVIEW");
            y += eyebrow_h + gap_tiny;
            out.listview = RECT{inner_x, y, inner_x + inner_w, y + px(116)};
            y += px(116) + gap_grp;

            out.push_eyebrow(RECT{inner_x, y, inner_x + inner_w, y + eyebrow_h}, L"TREEVIEW");
            y += eyebrow_h + gap_tiny;
            out.treeview = RECT{inner_x, y, inner_x + inner_w, y + px(116)};
            y += px(116);

            const int card_bottom = y + pad;
            out.push_card(RECT{col2_x, card_top, col2_x + col2_w, card_bottom}, L"Collections", ct);

            const int cn_top = card_bottom + gap_card;
            int cy = cn_top + pad;
            RECT cct{inner_x, cy, inner_x + inner_w, cy + title_h};
            cy += title_h + gap_row;

            out.push_eyebrow(RECT{inner_x, cy, inner_x + inner_w, cy + eyebrow_h}, L"PANEL + SPLITTER");
            cy += eyebrow_h + gap_tiny;
            {
                const int split_w = px(tok::spacing_sm) - px(2);
                const int body_h  = px(64);
                out.splitter = RECT{inner_x + inner_w - split_w, cy,
                                    inner_x + inner_w, cy + body_h};
                out.panel = RECT{inner_x, cy, out.splitter.left - gap_row, cy + body_h};
                cy += body_h;
            }
            const int cn_bottom = cy + pad;
            out.push_card(RECT{col2_x, cn_top, col2_x + col2_w, cn_bottom},
                          L"Containers", cct);

            // PropertyGrid card fills whatever vertical room is left so the
            // grid always has space for its header + rows.
            const int pg_top = cn_bottom + gap_card;
            int gy = pg_top + pad;
            RECT gct{inner_x, gy, inner_x + inner_w, gy + title_h};
            gy += title_h + gap_row;
            out.push_eyebrow(RECT{inner_x, gy, inner_x + inner_w, gy + eyebrow_h}, L"PROPERTYGRID");
            gy += eyebrow_h + gap_tiny;
            const int grid_bottom = (std::max)(gy + px(80), content_bottom - pad);
            out.propertygrid = RECT{inner_x, gy, inner_x + inner_w, grid_bottom};
            out.push_card(RECT{col2_x, pg_top, col2_x + col2_w,
                               (std::min)(grid_bottom + pad, content_bottom)},
                          L"PropertyGrid", gct);
        }

        // ---- column 3: Interaction states + TabControl --------------------
        {
            const int inner_x = col3_x + pad;
            const int inner_w = col3_w - pad * 2;
            const int card_top = content_top;
            int y = card_top + pad;

            RECT ct{inner_x, y, inner_x + inner_w, y + title_h};
            y += title_h + gap_row;

            const int grid_h = px(tok::spacing_md) + gap_row +
                               static_cast<int>(gallery::matrix_control_count) * px(matrix_row_h_l) +
                               (static_cast<int>(gallery::matrix_control_count) - 1) * gap_row;
            out.matrix_grid = RECT{inner_x, y, inner_x + inner_w, y + grid_h};
            y += grid_h;

            const int card_bottom = y + pad;
            out.push_card(RECT{col3_x, card_top, col3_x + col3_w, card_bottom},
                          L"Interaction states", ct);

            const int tb_top = card_bottom + gap_card;
            int ty = tb_top + pad;
            RECT tct{inner_x, ty, inner_x + inner_w, ty + title_h};
            ty += title_h + gap_row;
            out.push_eyebrow(RECT{inner_x, ty, inner_x + inner_w, ty + eyebrow_h}, L"TABCONTROL");
            ty += eyebrow_h + gap_tiny;
            const int tabs_h = (std::max)(px(tok::control_height_lg),
                                          content_bottom - pad - ty);
            out.tabs = RECT{inner_x, ty, inner_x + inner_w, ty + tabs_h};
            ty += tabs_h;
            out.push_card(RECT{col3_x, tb_top, col3_x + col3_w, (std::min)(ty + pad, content_bottom)},
                          L"Chrome", tct);
        }

        return out;
    }

    static void move_to(HWND h, const RECT& r) noexcept {
        if (h == nullptr) return;
        MoveWindow(h, r.left, r.top,
                   (std::max)(0, static_cast<int>(r.right - r.left)),
                   (std::max)(0, static_cast<int>(r.bottom - r.top)),
                   TRUE);
    }

    void layout_children() noexcept {
        if (hwnd() == nullptr || status_.hwnd() == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        // The StatusBar docks itself to the bottom edge of the client area at
        // full width when it is handed a WM_SIZE — the product-UI placement
        // the audit asked for. Do this first so status_height() is accurate
        // for the content band below.
        SendMessageW(status_.hwnd(), WM_SIZE, 0, 0);

        RECT client{};
        GetClientRect(hwnd(), &client);
        const GalleryLayout page = compute_layout(client);

        move_to(chip_light_.hwnd(), page.chip[0]);
        move_to(chip_dark_.hwnd(),  page.chip[1]);
        move_to(chip_hc_.hwnd(),    page.chip[2]);

        move_to(button_default_.hwnd(),  page.button[0]);
        move_to(button_disabled_.hwnd(), page.button[1]);
        move_to(button_hover_.hwnd(),    page.button[2]);

        move_to(check_unchecked_.hwnd(), page.check[0]);
        move_to(check_checked_.hwnd(),   page.check[1]);
        move_to(check_indeterm_.hwnd(),  page.check[2]);

        move_to(radio_a_.hwnd(), page.radio[0]);
        move_to(radio_b_.hwnd(), page.radio[1]);
        move_to(radio_c_.hwnd(), page.radio[2]);

        move_to(edit_.hwnd(),  page.edit);
        move_to(combobox_.hwnd(), page.combo);

        move_to(static_left_.hwnd(),   page.statics[0]);
        move_to(static_center_.hwnd(), page.statics[1]);
        move_to(static_right_.hwnd(),  page.statics[2]);

        move_to(progress_.hwnd(), page.progress);
        move_to(iconview_.hwnd(), page.icon);

        move_to(listbox_.hwnd(),  page.listbox);
        move_to(listview_.hwnd(), page.listview);
        move_to(treeview_.hwnd(), page.treeview);

        move_to(panel_.hwnd(),    page.panel);
        move_to(splitter_.hwnd(), page.splitter);
        move_to(tabs_.hwnd(),     page.tabs);
        move_to(property_grid_.hwnd(), page.propertygrid);

        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void paint_card(HDC dc, const RECT& r) const noexcept {
        if (r.right <= r.left || r.bottom <= r.top) return;
        nfui::fill_rounded_rect(dc, r, px(tok::radius_lg), palette_.surface, palette_.border);
    }

    void paint_matrix(HDC dc, const RECT& grid) noexcept {
        if (grid.right <= grid.left || grid.bottom <= grid.top) return;

        const int gap        = px(tok::spacing_sm);
        const int label_w    = px(matrix_label_l);
        const int header_h   = px(tok::spacing_md);
        const int row_h      = px(matrix_row_h_l);
        const int states     = static_cast<int>(gallery::matrix_state_count);
        const int cells_left = grid.left + label_w;
        const int cells_w    = grid.right - cells_left;
        if (cells_w <= states) return;
        const int cell_w = (cells_w - gap * (states - 1)) / states;
        if (cell_w <= 0) return;

        HFONT head_font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::xs);
        HFONT row_font  = fonts_.semibold(dpi_.dpi(), nfui::font_pt::sm);

        for (int s = 0; s < states; ++s) {
            const int x = cells_left + s * (cell_w + gap);
            RECT head{x, grid.top, x + cell_w, grid.top + header_h};
            nfui::draw_text(dc, head, gallery::matrix_state_labels[s], head_font,
                            palette_.text_secondary,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        int y = grid.top + header_h + gap;
        for (std::size_t r = 0; r < gallery::matrix_control_count; ++r) {
            RECT label{grid.left, y, cells_left - gap, y + row_h};
            nfui::draw_text(dc, label, gallery::matrix_control_labels[r], row_font,
                            palette_.text,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            for (int s = 0; s < states; ++s) {
                const int x = cells_left + s * (cell_w + gap);
                const RECT cell{x, y, x + cell_w, y + row_h};
                gallery::paint_matrix_cell(dc, cell,
                                           gallery::matrix_controls[r],
                                           gallery::matrix_states[static_cast<std::size_t>(s)],
                                           palette_,
                                           dpi_);
            }
            y += row_h + gap;
        }
    }

    void paint_background(HDC target, const RECT& client) noexcept {
        nfui::fill_rect(target, client, palette_.background);

        const int dpi_value = dpi_.dpi();
        HFONT hero_font    = fonts_.bold(dpi_value, nfui::font_pt::xl);
        HFONT card_font    = fonts_.semibold(dpi_value, nfui::font_pt::lg);
        HFONT eyebrow_font = fonts_.semibold(dpi_value, nfui::font_pt::xs);
        HFONT body_font    = fonts_.regular(dpi_value, nfui::font_pt::sm);

        const GalleryLayout page = compute_layout(client);

        for (int i = 0; i < page.card_count; ++i) {
            paint_card(target, page.card_rect[i]);
        }

        nfui::draw_text(target, page.title, L"Component Gallery", hero_font, palette_.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        nfui::draw_text(target, page.subtitle,
                        L"Live controls, containers, and a per-class interaction-state matrix \x2014 "
                        L"switch themes to verify every state in light, dark, and high contrast.",
                        body_font, palette_.text_secondary,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        for (int i = 0; i < page.card_count; ++i) {
            const LabelSlot& slot = page.card_title[i];
            if (slot.text == nullptr) continue;
            nfui::draw_text(target, slot.rect, slot.text, card_font, palette_.text,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        for (int i = 0; i < page.eyebrow_count; ++i) {
            const LabelSlot& slot = page.eyebrow[i];
            if (slot.text == nullptr) continue;
            nfui::draw_text(target, slot.rect, slot.text, eyebrow_font, palette_.text_secondary,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        paint_matrix(target, page.matrix_grid);
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

    nfui::Button      chip_light_;
    nfui::Button      chip_dark_;
    nfui::Button      chip_hc_;
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
    nfui::PropertyGrid property_grid_;
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmd_line, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    // --theme <light|dark|high_contrast> lets the visual audit capture each
    // variant without restarting the binary. Defaults to light.
    auto parse_theme = [](PCWSTR cl) noexcept {
        if (cl == nullptr) return nfui::ThemeMode::light;
        const wchar_t* tag = wcsstr(cl, L"--theme");
        if (tag == nullptr) return nfui::ThemeMode::light;
        tag += 7;  // skip past "--theme"
        while (*tag == L' ' || *tag == L'\t') ++tag;
        // The visual audit's quoteArgument wraps the value as "--theme \"dark\"";
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
