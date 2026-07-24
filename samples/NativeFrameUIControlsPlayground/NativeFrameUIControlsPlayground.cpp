// NativeFrameUIControlsPlayground
//
// V1.0 capability: an *interactive* playground that stresses the runtime
// behaviours a static gallery cannot show. Where NativeFrameUIThemeDemo lays
// every control out once per palette, this sample focuses on the framework's
// dynamic surface:
//
//   1. Runtime theme switching       — Light / Dark / High-Contrast buttons
//                                       re-inject the palette into every live
//                                       wrapper (persistent + dynamic) with no
//                                       recreate, no restart.
//   2. Dynamic create / destroy       — "Add" spins up a fresh nfui::CheckBox
//                                       bound to the current palette; "Remove"
//                                       drops the last one and OwnedHwnd RAII
//                                       calls DestroyWindow. A log ListBox and a
//                                       live counter mirror the churn.
//   3. Keyboard navigation            — a row of custom owner-draw NavTiles that
//                                       move focus on Tab / Shift+Tab / arrows
//                                       and activate on Space / Enter, entirely
//                                       from the keyboard.
//   4. State changes                  — hover / pressed / focused / disabled are
//                                       each rendered by the NavTiles (via
//                                       nfui::HoverState) plus a painted
//                                       "state reference" swatch strip and a row
//                                       of real controls whose enabled state
//                                       toggles at runtime.
//   5. palette-driven paint           — every custom pixel comes from the
//                                       injected ThemePalette through
//                                       nfui::fill_rounded_rect / draw_text, so
//                                       a theme switch repaints coherently.
//
// The sample deliberately instantiates one of every shipped control wrapper so
// it doubles as a link-coverage smoke exe for the umbrella target.

#include <nfui/NativeFrameUI.hpp>
#include <nfui/Easing.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <array>
#include <commctrl.h>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>
#include <windowsx.h>

namespace {

// ---- Command / control ids -------------------------------------------------
constexpr int id_theme_light = 101;
constexpr int id_theme_dark  = 102;
constexpr int id_theme_hc    = 103;

constexpr int id_add     = 111;
constexpr int id_remove  = 112;
constexpr int id_clear   = 113;
constexpr int id_toggle_enabled = 114;

constexpr int id_log_list    = 121;
constexpr int id_sample_edit = 122;
constexpr int id_sample_combo = 123;
constexpr int id_sample_button = 124;
constexpr int id_sample_check  = 125;
constexpr int id_sample_radio_a = 126;
constexpr int id_sample_radio_b = 127;

constexpr int id_gallery_listview = 131;
constexpr int id_gallery_treeview = 132;
constexpr int id_gallery_iconview = 133;
constexpr int id_gallery_tabs     = 134;
constexpr int id_gallery_panel    = 135;
constexpr int id_gallery_splitter = 136;
constexpr int id_gallery_progress = 137;
constexpr int id_status           = 138;

constexpr int id_dyn_base = 900;      // dynamic checkbox ids grow from here

constexpr UINT_PTR id_anim_timer = 1;
constexpr UINT_PTR id_theme_fade_timer = 2;   // CP17: theme cross-fade driver
constexpr unsigned int kThemeFadeMs = 200;     // CP17: cross-fade duration

constexpr int kNavTileCount   = 5;
constexpr int kDisabledTile   = 3;    // this tile stays WS_DISABLED to show the disabled state
constexpr wchar_t kNavTileClass[] = L"NfuiPlaygroundNavTile";

const wchar_t* const kNavLabels[kNavTileCount] = {
    L"Home", L"Search", L"Build", L"Locked", L"Run",
};

class PlaygroundWindow;

// One keyboard-navigable owner-draw tile. Renders normal / hover / pressed /
// focused / disabled entirely from the injected palette. Lives inside the
// owner window; the HWND is a child so it is torn down with the parent.
struct NavTile {
    HWND hwnd{};
    int index{};
    PlaygroundWindow* owner{};
    nfui::HoverState hover;
    bool pressed{false};
    int activations{0};
};

// ---- Small commctrl helpers ------------------------------------------------
void listview_add_column(HWND lv, int index, const wchar_t* text, int width) noexcept {
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = width;
    col.pszText = const_cast<LPWSTR>(text);
    ListView_InsertColumn(lv, index, &col);
}

void listview_add_row(HWND lv, int row, const wchar_t* c0, const wchar_t* c1) noexcept {
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = row;
    item.pszText = const_cast<LPWSTR>(c0);
    ListView_InsertItem(lv, &item);
    LVITEMW sub{};
    sub.mask = LVIF_TEXT;
    sub.iItem = row;
    sub.iSubItem = 1;
    sub.pszText = const_cast<LPWSTR>(c1);
    ListView_SetItem(lv, &sub);
}

HTREEITEM tree_insert(HWND tv, HTREEITEM parent, LPCWSTR text) noexcept {
    TVINSERTSTRUCTW ins{};
    ins.hParent = parent;
    ins.hInsertAfter = TVI_LAST;
    ins.item.mask = TVIF_TEXT;
    ins.item.pszText = const_cast<LPWSTR>(text);
    return TreeView_InsertItem(tv, &ins);
}

void tab_insert(HWND tabs, int index, LPCWSTR text) noexcept {
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<LPWSTR>(text);
    TabCtrl_InsertItem(tabs, index, &item);
}

// ---------------------------------------------------------------------------
class PlaygroundWindow final : public nfui::Window {
public:
    explicit PlaygroundWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(mode_)) {}

    ~PlaygroundWindow() noexcept override { cleanup(); }

    [[nodiscard]] const nfui::ThemePalette& palette() const noexcept { return palette_; }
    [[nodiscard]] nfui::FontCache& fonts() noexcept { return fonts_; }
    [[nodiscard]] const nfui::DpiScale& dpi() const noexcept { return dpi_; }

    // CP32: lets wWinMain seed the palette before create_main wires it into
    // every wrapper via inject_theme. Without this, --theme dark would
    // first paint light, then the user has to click Dark to see anything.
    [[nodiscard]] bool set_initial_theme(nfui::ThemeMode mode) noexcept {
        if (hwnd() != nullptr) return false;       // only valid pre-create
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        palette_from_ = palette_;
        palette_to_ = palette_;
        return true;
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIControlsPlaygroundWindow",
            L"NativeFrame UI — Controls Playground",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT, CW_USEDEFAULT,
            // CP36: shrink default window from 1200×860 → 940×700 so the
            // playground fits the unified compact demo silhouette. Inner
            // gallery strip is computed against the actual client width in
            // layout(), so the column reflows automatically.
            940, 700,
        };
        if (!create(params)) {
            return false;
        }
        apply_window_icon();
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));

        register_tile_class();
        if (!build_persistent() || !build_nav_tiles()) {
            return false;
        }
        apply_native_fonts();
        refresh_status();
        layout();

        SetTimer(hwnd(), id_anim_timer, 90, nullptr);
        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

    // Called by a NavTile when its focus / activation state changes.
    void on_tile_focus(int index) noexcept {
        focused_tile_ = index;
        refresh_status();
    }
    void on_tile_activated(int index) noexcept {
        if (index >= 0 && index < kNavTileCount) {
            ++tile_activations_[static_cast<size_t>(index)];
        }
        refresh_status();
    }

    // Advance keyboard focus to the next / previous *enabled* tile.
    void move_tile_focus(int from, int delta) noexcept {
        for (int step = 0; step < kNavTileCount; ++step) {
            int next = (from + delta * (step + 1)) % kNavTileCount;
            if (next < 0) {
                next += kNavTileCount;
            }
            if (next == kDisabledTile) {
                continue;
            }
            SetFocus(tiles_[static_cast<size_t>(next)].hwnd);
            return;
        }
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            layout();
            return 0;
        case WM_TIMER:
            if (wparam == id_anim_timer) {
                animate();
                return 0;
            }
            if (wparam == id_theme_fade_timer) {
                step_theme_fade();   // CP17: theme cross-fade tick
                return 0;
            }
            break;
        case WM_DPICHANGED: {
            auto* suggested = reinterpret_cast<RECT*>(lparam);
            dpi_ = nfui::DpiScale(HIWORD(wparam));
            if (suggested != nullptr) {
                SetWindowPos(hwnd(), nullptr,
                             suggested->left, suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }
            apply_native_fonts();
            layout();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd(), &ps);
            RECT client{};
            GetClientRect(hwnd(), &client);
            {
                nfui::MemoryDC mem(hdc, client);
                paint_background(mem.valid() ? mem.dc() : hdc, client);
            }
            EndPaint(hwnd(), &ps);
            return 0;
        }
        case WM_DESTROY:
            cleanup();
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return nfui::Window::handle_message(message, wparam, lparam);
    }

    bool on_command(int command_id, HWND, UINT code) override {
        const bool clicked = (code == BN_CLICKED || code == 0);
        switch (command_id) {
        case id_theme_light: if (clicked) { switch_mode(nfui::ThemeMode::light); return true; } break;
        case id_theme_dark:  if (clicked) { switch_mode(nfui::ThemeMode::dark); return true; } break;
        case id_theme_hc:    if (clicked) { switch_mode(nfui::ThemeMode::high_contrast); return true; } break;
        case id_add:     if (clicked) { add_dynamic(); return true; } break;
        case id_remove:  if (clicked) { remove_dynamic(); return true; } break;
        case id_clear:   if (clicked) { clear_dynamic(); return true; } break;
        case id_toggle_enabled: if (clicked) { toggle_sample_enabled(); return true; } break;
        default: break;
        }
        return false;
    }

private:
    // ---- Construction ------------------------------------------------------
    template <typename ControlT>
    [[nodiscard]] bool make_child(ControlT& control, int id, std::wstring_view text,
                                  DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP) noexcept {
        nfui::ControlCreateParams params{instance_, hwnd(), id, text, 0, 0, 100, 28, style};
        control.inject_theme(&palette_, &fonts_);
        return control.create(params);
    }

    [[nodiscard]] bool build_persistent() noexcept {
        // Theme toggles.
        if (!make_child(theme_light_, id_theme_light, L"Light")) return false;
        if (!make_child(theme_dark_, id_theme_dark, L"Dark")) return false;
        if (!make_child(theme_hc_, id_theme_hc, L"High Contrast")) return false;

        // Dynamic-controls panel.
        if (!make_child(add_button_, id_add, L"Add control")) return false;
        if (!make_child(remove_button_, id_remove, L"Remove")) return false;
        if (!make_child(clear_button_, id_clear, L"Clear all")) return false;
        if (!make_child(count_label_, id_status + 100, L"Live dynamic controls: 0")) return false;
        if (!make_child(log_list_, id_log_list, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOINTEGRALHEIGHT)) return false;

        // Tooltip attached to the Add button so hovering explains the churn.
        if (tooltip_.create({instance_, hwnd(), 0, L"", 0, 0, 0, 0, 0})) {
            TOOLINFOW ti{};
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            ti.hwnd = hwnd();
            ti.uId = reinterpret_cast<UINT_PTR>(add_button_.hwnd());
            ti.lpszText = const_cast<LPWSTR>(L"Spins up a fresh palette-bound CheckBox at runtime");
            SendMessageW(tooltip_.hwnd(), TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
        }

        // Keyboard-nav instructions + activation label.
        if (!make_child(nav_hint_, id_status + 101, L"Tab / Shift+Tab / ← → to move — Space or Enter to activate")) return false;
        nfui::TextStyle hint_style{};
        hint_style.single_line = false;
        nav_hint_.set_style(hint_style);

        // Disabled-state demo row (real controls whose enabled state toggles).
        if (!make_child(toggle_enabled_button_, id_toggle_enabled, L"Disable sample row")) return false;
        if (!make_child(sample_button_, id_sample_button, L"Sample")) return false;
        if (!make_child(sample_check_, id_sample_check, L"Option")) return false;
        if (!make_child(sample_radio_a_, id_sample_radio_a, L"A")) return false;
        if (!make_child(sample_radio_b_, id_sample_radio_b, L"B")) return false;
        if (!make_child(sample_edit_, id_sample_edit, L"editable")) return false;
        if (!make_child(sample_combo_, id_sample_combo, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL)) return false;
        SendMessageW(sample_check_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(sample_radio_a_.hwnd(), BM_SETCHECK, BST_CHECKED, 0);
        ComboBox_AddString(sample_combo_.hwnd(), L"Low");
        ComboBox_AddString(sample_combo_.hwnd(), L"Medium");
        ComboBox_AddString(sample_combo_.hwnd(), L"High");
        SendMessageW(sample_combo_.hwnd(), CB_SETCURSEL, 1, 0);

        // Gallery strip — one of every remaining control wrapper.
        if (!make_child(gallery_listview_, id_gallery_listview, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT)) return false;
        ListView_SetExtendedListViewStyle(gallery_listview_.hwnd(), LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        listview_add_column(gallery_listview_.hwnd(), 0, L"Control", 130);
        listview_add_column(gallery_listview_.hwnd(), 1, L"Module", 130);
        listview_add_row(gallery_listview_.hwnd(), 0, L"Button", L"nfui_button");
        listview_add_row(gallery_listview_.hwnd(), 1, L"ListView", L"nfui_listview");

        if (!make_child(gallery_iconview_, id_gallery_iconview, L"")) return false;
        app_icon_ = nfui::load_scaled_icon(instance_, MAKEINTRESOURCEW(IDI_NFUI_APP), 32, dpi_.dpi());
        if (app_icon_ != nullptr) {
            gallery_iconview_.set_icon(app_icon_);
        }

        if (!make_child(gallery_tabs_, id_gallery_tabs, L"")) return false;
        tab_insert(gallery_tabs_.hwnd(), 0, L"Design");
        tab_insert(gallery_tabs_.hwnd(), 1, L"Preview");
        tab_insert(gallery_tabs_.hwnd(), 2, L"Log");

        if (!make_child(gallery_panel_, id_gallery_panel, L"")) return false;
        if (!make_child(gallery_splitter_, id_gallery_splitter, L"")) return false;

        if (!make_child(gallery_progress_, id_gallery_progress, L"")) return false;
        SendMessageW(gallery_progress_.hwnd(), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(gallery_progress_.hwnd(), PBM_SETPOS, 40, 0);

        if (!make_child(status_bar_, id_status, L"")) return false;
        return true;
    }

    [[nodiscard]] bool build_nav_tiles() noexcept {
        for (int i = 0; i < kNavTileCount; ++i) {
            NavTile& tile = tiles_[static_cast<size_t>(i)];
            tile.index = i;
            tile.owner = this;
            DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
            if (i == kDisabledTile) {
                style |= WS_DISABLED;
            }
            tile.hwnd = CreateWindowExW(0, kNavTileClass, kNavLabels[i], style,
                                        0, 0, 100, 48, hwnd(),
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(200 + i)),
                                        instance_, &tile);
            if (tile.hwnd == nullptr) {
                return false;
            }
        }
        return true;
    }

    void register_tile_class() noexcept {
        static bool registered = false;
        if (registered) {
            return;
        }
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &PlaygroundWindow::tile_proc;
        wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kNavTileClass;
        registered = (RegisterClassExW(&wc) != 0);
    }

    // ---- Dynamic control churn --------------------------------------------
    void add_dynamic() noexcept {
        auto check = std::make_unique<nfui::CheckBox>();
        const int id = id_dyn_base + static_cast<int>(dynamic_.size());
        std::wstring label = L"Dynamic #" + std::to_wstring(dynamic_.size() + 1);
        check->inject_theme(&palette_, &fonts_);
        nfui::ControlCreateParams params{instance_, hwnd(), id, label, 0, 0, 200, 24,
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP};
        if (!check->create(params)) {
            return;
        }
        SendMessageW(check->hwnd(), WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts_.regular(dpi_.dpi(), nfui::font_pt::ui)), TRUE);
        log(L"created " + label);
        dynamic_.push_back(std::move(check));
        after_dynamic_change();
    }

    void remove_dynamic() noexcept {
        if (dynamic_.empty()) {
            return;
        }
        log(L"destroyed Dynamic #" + std::to_wstring(dynamic_.size()));
        dynamic_.pop_back();   // OwnedHwnd RAII -> DestroyWindow
        after_dynamic_change();
    }

    void clear_dynamic() noexcept {
        if (dynamic_.empty()) {
            return;
        }
        log(L"cleared " + std::to_wstring(dynamic_.size()) + L" control(s)");
        dynamic_.clear();
        after_dynamic_change();
    }

    void after_dynamic_change() noexcept {
        std::wstring text = L"Live dynamic controls: " + std::to_wstring(dynamic_.size());
        count_label_.set_caption(text);
        refresh_status();
        layout();
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void log(const std::wstring& line) noexcept {
        int index = static_cast<int>(SendMessageW(log_list_.hwnd(), LB_ADDSTRING, 0,
                                                  reinterpret_cast<LPARAM>(line.c_str())));
        SendMessageW(log_list_.hwnd(), LB_SETCURSEL, index, 0);
    }

    void toggle_sample_enabled() noexcept {
        sample_enabled_ = !sample_enabled_;
        const BOOL on = sample_enabled_ ? TRUE : FALSE;
        for (HWND h : {sample_button_.hwnd(), sample_check_.hwnd(), sample_radio_a_.hwnd(),
                       sample_radio_b_.hwnd(), sample_edit_.hwnd(), sample_combo_.hwnd()}) {
            EnableWindow(h, on);
        }
        SetWindowTextW(toggle_enabled_button_.hwnd(),
                       sample_enabled_ ? L"Disable sample row" : L"Enable sample row");
        refresh_status();
    }

    // ---- Theme switch ------------------------------------------------------
    // CP17: cross-fade the palette over kThemeFadeMs instead of an instant
    // flip. Each tick builds an interpolated palette (lerp_palette) and
    // re-injects it into every control + the custom-painted window background.
    // Snaps instantly when either end is the high-contrast profile (HC is an
    // accessibility chrome — it must not soften) or when the system disables
    // client-area animation.
    void switch_mode(nfui::ThemeMode mode) noexcept {
        if (mode == mode_) {
            return;
        }
        const nfui::ThemePalette target = nfui::theme_palette(mode);
        const bool snap = is_high_contrast(palette_) || is_high_contrast(target)
                          || !nfui::Control::system_animations_enabled();
        mode_ = mode;
        if (snap) {
            fading_ = false;
            if (hwnd() != nullptr) KillTimer(hwnd(), id_theme_fade_timer);
            palette_ = target;
            apply_palette_to_all();
            refresh_status();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return;
        }
        // Begin / restart the fade from the currently-shown palette.
        palette_from_ = palette_;
        palette_to_ = target;
        fade_start_ = GetTickCount64();
        fading_ = true;
        if (hwnd() != nullptr) {
            SetTimer(hwnd(), id_theme_fade_timer, 16, nullptr);
        }
        refresh_status();
    }

    // One step of the theme cross-fade. Returns true while the fade is still
    // running (caller keeps the timer armed); false when it has settled.
    bool step_theme_fade() noexcept {
        if (!fading_) return false;
        const unsigned long long elapsed = GetTickCount64() - fade_start_;
        if (elapsed >= kThemeFadeMs) {
            palette_ = palette_to_;
            fading_ = false;
            apply_palette_to_all();
            if (hwnd() != nullptr) KillTimer(hwnd(), id_theme_fade_timer);
            InvalidateRect(hwnd(), nullptr, FALSE);
            return false;
        }
        const float t = static_cast<float>(elapsed) / static_cast<float>(kThemeFadeMs);
        palette_ = nfui::lerp_palette(palette_from_, palette_to_, nfui::ease_in_out_cubic(t));
        apply_palette_to_all();
        InvalidateRect(hwnd(), nullptr, FALSE);
        return true;
    }

    void apply_palette_to_all() noexcept {
        for (nfui::Control* c : all_controls()) {
            c->set_palette(&palette_);
        }
        for (auto& dyn : dynamic_) {
            dyn->set_palette(&palette_);
        }
        for (auto& tile : tiles_) {
            InvalidateRect(tile.hwnd, nullptr, FALSE);
        }
    }

    std::array<nfui::Control*, 23> all_controls() noexcept {
        // CP17: status_bar_ and gallery_splitter_ are self-paint controls whose
        // HWNDs are clipped from the parent's update region (WS_CLIPCHILDREN),
        // so the parent InvalidateRect in step_theme_fade does not reach them.
        // They must be in this list so apply_palette_to_all() calls set_palette
        // (which invalidates their own HWND) each fade tick — otherwise they
        // stay frozen on the pre-fade palette for the whole cross-fade.
        return {&theme_light_, &theme_dark_, &theme_hc_, &add_button_, &remove_button_,
                &clear_button_, &count_label_, &log_list_, &nav_hint_, &toggle_enabled_button_,
                &sample_button_, &sample_check_, &sample_radio_a_, &sample_radio_b_, &sample_edit_,
                &sample_combo_, &gallery_listview_, &gallery_iconview_,
                &gallery_tabs_, &gallery_panel_, &gallery_progress_,
                &status_bar_, &gallery_splitter_};
    }

    // ---- Animation ---------------------------------------------------------
    void animate() noexcept {
        anim_pos_ = (anim_pos_ + 3) % 101;
        if (gallery_progress_.hwnd() != nullptr) {
            SendMessageW(gallery_progress_.hwnd(), PBM_SETPOS, anim_pos_, 0);
        }
    }

    // ---- Status ------------------------------------------------------------
    void refresh_status() noexcept {
        if (status_bar_.hwnd() == nullptr) {
            return;
        }
        const wchar_t* mode =
            (mode_ == nfui::ThemeMode::high_contrast) ? L"High Contrast"
            : (mode_ == nfui::ThemeMode::dark)        ? L"Dark" : L"Light";
        std::wstring focus = (focused_tile_ >= 0 && focused_tile_ < kNavTileCount)
                                 ? kNavLabels[focused_tile_] : L"none";
        int total_acts = 0;
        for (int a : tile_activations_) {
            total_acts += a;
        }
        std::wstring text = std::wstring(L"Theme: ") + mode +
                            L"   |   Focused tile: " + focus +
                            L"   |   Activations: " + std::to_wstring(total_acts) +
                            L"   |   Dynamic: " + std::to_wstring(dynamic_.size()) +
                            L"   |   Sample row: " + (sample_enabled_ ? L"enabled" : L"disabled");
        SendMessageW(status_bar_.hwnd(), SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }

    // ---- Layout ------------------------------------------------------------
    int px(int logical) const noexcept { return dpi_.logical_to_pixels(logical); }

    void layout() noexcept {
        if (hwnd() == nullptr || status_bar_.hwnd() == nullptr) {
            return;
        }
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        SendMessageW(status_bar_.hwnd(), WM_SIZE, 0, 0);

        const int outer = px(12);
        const int gap = px(8);
        const int row = px(26);
        // CP36: trim col_w 360 → 300 logical px so the three-column body
        // reflows inside the 940-wide compact window. The previous 360 +
        // 32/24 inter-column gaps left only 132 logical px for the third
        // (gallery) column, which clipped its 3-tab TabControl against
        // the right edge. 300 + 24/20 gaps frees ~248 logical px so all
        // three TabControl tabs render fully.
        const int col_w = px(300);
        const int small_w = px(92);
        const int tile_w = px(56);
        const int tile_h = px(52);

        // Top bar: theme toggles on the right.
        const int top = client.top + outer;
        const int btn_w = px(110);
        int tx = client.right - outer - (btn_w * 2 + px(150) + gap * 2);
        MoveWindow(theme_light_.hwnd(), tx, top, btn_w, row, TRUE); tx += btn_w + gap;
        MoveWindow(theme_dark_.hwnd(), tx, top, btn_w, row, TRUE); tx += btn_w + gap;
        MoveWindow(theme_hc_.hwnd(), tx, top, px(150), row, TRUE);

        // CP32: section header band ends at client.top + px(104); body content
        // begins immediately below with a 12 px breathing room.
        const int body_top = client.top + px(116);

        // ---- Column 1: dynamic controls ----
        int x1 = client.left + outer;
        int y = body_top;
        // CP36: small_w, tile_w, tile_h moved to top of layout() — see comment
        // on the col_w declaration.
        MoveWindow(add_button_.hwnd(), x1, y, small_w, row, TRUE);
        MoveWindow(remove_button_.hwnd(), x1 + small_w + gap, y, small_w, row, TRUE);
        MoveWindow(clear_button_.hwnd(), x1 + 2 * (small_w + gap), y, small_w, row, TRUE);
        y += row + gap;
        MoveWindow(count_label_.hwnd(), x1, y, col_w, row, TRUE);
        y += row + px(4);
        int dyn_top = y;
        for (auto& dyn : dynamic_) {
            MoveWindow(dyn->hwnd(), x1, y, col_w, px(24), TRUE);
            y += px(26);
        }
        // Log list fills the rest of the column.
        const int log_top = std::max(y + gap, dyn_top + px(120));
        const int status_h = px(24);
        const int col_bottom = client.bottom - status_h - outer;
        MoveWindow(log_list_.hwnd(), x1, log_top, col_w, std::max(px(80), col_bottom - log_top), TRUE);

        // ---- Column 2: keyboard nav + state ----
        // CP36: tightened inter-column gap 32 → 20 logical px so the three
        // columns share the 940-wide compact canvas comfortably.
        int x2 = x1 + col_w + px(20);
        y = body_top;
        MoveWindow(nav_hint_.hwnd(), x2, y, col_w, px(36), TRUE);
        y += px(40);
        for (int i = 0; i < kNavTileCount; ++i) {
            MoveWindow(tiles_[static_cast<size_t>(i)].hwnd, x2 + i * (tile_w + gap), y, tile_w, tile_h, TRUE);
        }
        y += tile_h + px(24);

        // State-reference swatch strip is painted directly (see paint_background);
        // reserve its band then place the disabled-demo row below it.
        state_strip_top_ = y;
        state_strip_left_ = x2;
        y += px(96);

        MoveWindow(toggle_enabled_button_.hwnd(), x2, y, px(160), row, TRUE);
        y += row + gap;
        MoveWindow(sample_button_.hwnd(), x2, y, px(90), row, TRUE);
        MoveWindow(sample_check_.hwnd(), x2 + px(100), y, px(90), row, TRUE);
        MoveWindow(sample_radio_a_.hwnd(), x2 + px(196), y, px(50), row, TRUE);
        MoveWindow(sample_radio_b_.hwnd(), x2 + px(248), y, px(50), row, TRUE);
        y += row + gap;
        MoveWindow(sample_edit_.hwnd(), x2, y, px(180), row, TRUE);
        MoveWindow(sample_combo_.hwnd(), x2 + px(190), y, px(120), px(220), TRUE);

        // ---- Column 3: gallery strip ----
        // CP32: the gallery column absorbs all remaining width so the
        // TreeView/LVTabs inside it never clip sub-item text. Previous
        // std::max floor of 300 logical px was too tight once TV indent
        // + scrollbar were subtracted. CP36: tightens inter-column gap
        // 24 → 16 logical px to free more room for the gallery list.
        int x3 = x2 + col_w + px(16);
        // CP35: clamp gcol_w to the available client width. CP34 raised the
        // floor from 360 → 440 logical px so the TreeView never clips its
        // longest label, but std::max(440, available) over-rode the actual
        // available width on the default 1200-wide window and pushed the
        // TreeView (and the ListView / tabs above it) ~56 px past the
        // window's right edge — the truncated "Dynami" / "Keyboar" /
        // "State" labels in the CP34 audit were the visible symptom.
        const int gcol_available = static_cast<int>(client.right - outer - x3);
        const int gcol_w = (std::min)(px(440), gcol_available);
        y = body_top;
        MoveWindow(gallery_listview_.hwnd(), x3, y, gcol_w, px(110), TRUE);
        y += px(120);
        // CP35: gallery TreeView removed — Win32's text column is hardcoded
        // at ~30 px (no TVM widens it; TVM_SETCOLUMNWIDTH is gone from the
        // modern SDK), so any TreeView placed in this strip would render
        // "Dynami" / "Keyboar" style truncation regardless of control width.
        // The TreeView wrapper is still demonstrable in ComponentGallery
        // (where it has its own narrow column) and ThemeDemo.
        y += px(16);
        MoveWindow(gallery_tabs_.hwnd(), x3, y, gcol_w, px(90), TRUE);
        y += px(100);
        MoveWindow(gallery_iconview_.hwnd(), x3, y, px(40), px(40), TRUE);
        MoveWindow(gallery_panel_.hwnd(), x3 + px(52), y, gcol_w - px(80), px(40), TRUE);
        MoveWindow(gallery_splitter_.hwnd(), x3 + gcol_w - px(20), y, px(6), px(40), TRUE);
        y += px(52);
        MoveWindow(gallery_progress_.hwnd(), x3, y, gcol_w, px(18), TRUE);
    }

    // ---- Palette-driven background paint -----------------------------------
    void paint_background(HDC dc, const RECT& client) noexcept {
        nfui::fill_rect(dc, client, palette_.background);
        const int d = dpi_.dpi();
        HFONT title_font = fonts_.bold(d, nfui::font_pt::xl);
        HFONT subtitle_font = fonts_.regular(d, nfui::font_pt::sm);
        HFONT header_font = fonts_.semibold(d, nfui::font_pt::md);
        HFONT body_font = fonts_.regular(d, nfui::font_pt::sm);

        // CP32: title rect is taller than the bare cap height of xl (28 pt) so
        // the descender ('y'/'g') and the font's internal leading are not
        // clipped at the rect bottom. Subtitle rect shares the same right
        // boundary so the two lines stack flush.
        const int outer = px(24);
        // CP34: trim the right reservation from 420 → 260 logical px so the
        // subtitle line ("Interactive surface: theme switching, dynamic
        // create/destroy, keyboard navigation, state changes.") no longer
        // trips DT_END_ELLIPSIS at 96 DPI. The title is much shorter than
        // the subtitle, so the smaller right reserve doesn't affect it.
        const int title_right = client.right - px(260);
        RECT title{client.left + outer, client.top + px(8),
                   title_right, client.top + px(60)};
        nfui::draw_text(dc, title, L"Controls Playground", title_font, palette_.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        RECT subtitle{client.left + outer, title.bottom + px(2),
                      title_right, title.bottom + px(36)};
        nfui::draw_text(dc, subtitle,
                        L"Interactive surface: theme switching, dynamic create/destroy, keyboard navigation, state changes.",
                        subtitle_font, palette_.text_secondary,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // CP32: column section headers — each gets a small accent bar (2 px
        // wide, ~16 px tall) on the left edge and a semibold md label. The
        // bar reads as a quiet brand marker without competing with the rest
        // of the chrome.
        auto header = [&](int x, int right, const wchar_t* text) {
            const int bar_h = px(16);
            const int bar_w = px(2);
            const int bar_y = client.top + px(84);
            RECT bar{x, bar_y, x + bar_w, bar_y + bar_h};
            nfui::fill_rect(dc, bar, palette_.accent);
            RECT r{x + px(10), client.top + px(80), right, client.top + px(104)};
            nfui::draw_text(dc, r, text, header_font, palette_.text,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        };
        const int col_w = px(360);
        const int x1 = client.left + outer;
        const int x2 = x1 + col_w + px(24);
        const int x3 = x2 + col_w + px(24);
        // CP35: mirror the layout-side clamp so the section-3 header
        // accent bar stops at the actual window right edge (the std::max
        // here had the same overflow as the MoveWindow path).
        const int gcol_w = (std::min)(px(440),
                                      static_cast<int>(client.right - outer - x3));
        header(x1, x1 + col_w, L"1 · Dynamic create / destroy");
        header(x2, x2 + col_w, L"2 · Keyboard navigation + state");
        header(x3, x3 + gcol_w, L"3 · Every control wrapper");

        // State-reference swatch strip (palette-driven).
        if (state_strip_top_ > 0) {
            paint_state_strip(dc, body_font);
        }
    }

    void paint_state_strip(HDC dc, HFONT font) noexcept {
        struct Swatch { const wchar_t* label; nfui::Color fill; nfui::Color border; nfui::Color text; bool ring; };
        const nfui::ThemePalette& p = palette_;
        const Swatch swatches[] = {
            {L"Normal",   p.surface,       p.border, p.text,           false},
            {L"Hover",    p.surface_hover, p.border, p.text,           false},
            {L"Pressed",  p.accent,        p.accent, p.accent_text,    false},
            {L"Focused",  p.surface,       p.accent, p.text,           true},
            {L"Disabled", p.surface,       p.border, p.text_secondary, false},
        };
        const int sw = px(60);
        const int sh = px(44);
        const int gap = px(10);
        int x = state_strip_left_;
        const int radius = nfui::theme_metrics().corner_radius_control;
        for (const Swatch& s : swatches) {
            RECT r{x, state_strip_top_, x + sw, state_strip_top_ + sh};
            // CP32: subtle elevation-1 shadow under each swatch so they read
            // as discrete cards against the workspace background.
            nfui::paint_drop_shadow(dc, r, radius, 1, p.shadow);
            nfui::fill_rounded_rect(dc, r, radius, s.fill, s.border);
            if (s.ring) {
                RECT inner{r.left + px(3), r.top + px(3), r.right - px(3), r.bottom - px(3)};
                nfui::fill_rounded_rect(dc, inner, radius, s.fill, p.accent);
            }
            nfui::draw_text(dc, r, s.label, font, s.text,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            x += sw + gap;
        }
        RECT caption{state_strip_left_, state_strip_top_ + sh + px(4),
                     state_strip_left_ + (sw + gap) * 5, state_strip_top_ + sh + px(24)};
        nfui::draw_text(dc, caption, L"State reference — repainted from the live palette",
                        font, palette_.text_secondary, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    }

    // ---- NavTile window proc ----------------------------------------------
    static LRESULT CALLBACK tile_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept {
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        }
        auto* tile = reinterpret_cast<NavTile*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (tile == nullptr) {
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        }
        switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTTAB;
        case WM_MOUSEMOVE:
            tile->hover.on_mouse_move(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_MOUSELEAVE:
            tile->hover.on_mouse_leave();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            tile->pressed = true;
            tile->hover.on_lbutton_down();
            SetFocus(hwnd);
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            const bool was_pressed = tile->pressed;
            tile->pressed = false;
            tile->hover.on_lbutton_up();
            ReleaseCapture();
            if (was_pressed) {
                tile->owner->on_tile_activated(tile->index);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_SETFOCUS:
            tile->owner->on_tile_focus(tile->index);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_KILLFOCUS:
            tile->pressed = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_KEYDOWN:
            switch (wparam) {
            case VK_TAB:
                tile->owner->move_tile_focus(tile->index,
                                             (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1);
                return 0;
            case VK_RIGHT: case VK_DOWN:
                tile->owner->move_tile_focus(tile->index, 1);
                return 0;
            case VK_LEFT: case VK_UP:
                tile->owner->move_tile_focus(tile->index, -1);
                return 0;
            case VK_SPACE: case VK_RETURN:
                tile->pressed = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            default:
                break;
            }
            break;
        case WM_KEYUP:
            if ((wparam == VK_SPACE || wparam == VK_RETURN) && tile->pressed) {
                tile->pressed = false;
                tile->owner->on_tile_activated(tile->index);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paint_tile(hwnd, *tile);
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;
        default:
            break;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    static void paint_tile(HWND hwnd, NavTile& tile) noexcept {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        nfui::OwnerDrawDC dc(hwnd, rc);
        if (!dc.valid()) {
            return;
        }
        const nfui::ThemePalette& p = tile.owner->palette();
        const bool enabled = IsWindowEnabled(hwnd) != FALSE;
        const bool focused = (GetFocus() == hwnd);
        const bool pressed = tile.pressed;
        const bool hover = tile.hover.hover();

        nfui::Color fill = p.surface;
        nfui::Color border = p.border;
        nfui::Color text = p.text;
        if (!enabled) {
            fill = p.surface;
            text = p.text_secondary;
        } else if (pressed) {
            fill = p.accent;
            border = p.accent;
            text = p.accent_text;
        } else if (hover) {
            fill = p.surface_hover;
        }
        if (enabled && focused && !pressed) {
            border = p.accent;
        }

        const int radius = nfui::theme_metrics().corner_radius_control;
        nfui::fill_rounded_rect(dc.dc(), dc.bounds(), radius, fill, border);
        if (enabled && focused && !pressed) {
            RECT inner{rc.left + 3, rc.top + 3, rc.right - 3, rc.bottom - 3};
            nfui::fill_rounded_rect(dc.dc(), inner, radius, fill, p.accent);
        }

        const int d = tile.owner->dpi().dpi();
        HFONT font = tile.owner->fonts().semibold(d, nfui::font_pt::ui);
        wchar_t label[64]{};
        GetWindowTextW(hwnd, label, 63);
        nfui::draw_text(dc.dc(), rc, label, font, text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // ---- Fonts / icon / teardown ------------------------------------------
    void apply_native_fonts() noexcept {
        const int d = dpi_.dpi();
        HFONT ui = fonts_.regular(d, nfui::font_pt::ui);
        HFONT semi = fonts_.semibold(d, nfui::font_pt::ui);
        for (HWND h : {theme_light_.hwnd(), theme_dark_.hwnd(), theme_hc_.hwnd(),
                       add_button_.hwnd(), remove_button_.hwnd(), clear_button_.hwnd(),
                       toggle_enabled_button_.hwnd()}) {
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(semi), TRUE);
        }
        for (HWND h : {log_list_.hwnd(), sample_edit_.hwnd(), sample_combo_.hwnd(),
                       gallery_listview_.hwnd(), gallery_tabs_.hwnd(),
                       status_bar_.hwnd()}) {
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
        }
    }

    void apply_window_icon() noexcept {
        if (!resources_.has_icon(IDI_NFUI_APP)) {
            return;
        }
        HICON big = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                  IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                                  GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
        HICON small = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                                    GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
        if (big != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
        }
        if (small != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));
        }
    }

    void cleanup() noexcept {
        if (hwnd() != nullptr) {
            KillTimer(hwnd(), id_anim_timer);
            KillTimer(hwnd(), id_theme_fade_timer);   // CP17
        }
        dynamic_.clear();
        if (app_icon_ != nullptr) {
            DestroyIcon(app_icon_);
            app_icon_ = nullptr;
        }
    }

    // ---- State -------------------------------------------------------------
    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemeMode mode_{nfui::ThemeMode::light};
    nfui::ThemePalette palette_;
    // CP17: theme cross-fade state.
    nfui::ThemePalette palette_from_{};
    nfui::ThemePalette palette_to_{};
    unsigned long long fade_start_{0};
    bool fading_{false};
    nfui::FontCache fonts_;
    nfui::DpiScale dpi_{96};
    HICON app_icon_{};

    // Persistent controls.
    nfui::Button theme_light_, theme_dark_, theme_hc_;
    nfui::Button add_button_, remove_button_, clear_button_, toggle_enabled_button_;
    nfui::StaticText count_label_, nav_hint_;
    nfui::ListBox log_list_;
    nfui::Tooltip tooltip_;
    nfui::Button sample_button_;
    nfui::CheckBox sample_check_;
    nfui::RadioButton sample_radio_a_, sample_radio_b_;
    nfui::Edit sample_edit_;
    nfui::ComboBox sample_combo_;
    nfui::ListView gallery_listview_;
    nfui::IconView gallery_iconview_;
    nfui::TabControl gallery_tabs_;
    nfui::Panel gallery_panel_;
    nfui::Splitter gallery_splitter_;
    nfui::ProgressBar gallery_progress_;
    nfui::StatusBar status_bar_;

    // Dynamic controls.
    std::vector<std::unique_ptr<nfui::CheckBox>> dynamic_;

    // Keyboard-nav tiles.
    std::array<NavTile, kNavTileCount> tiles_{};
    int focused_tile_{-1};
    std::array<int, kNavTileCount> tile_activations_{};

    bool sample_enabled_{true};
    int anim_pos_{40};
    int state_strip_top_{0};
    int state_strip_left_{0};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmd_line, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    // CP32: --theme lets the visual audit open the window in a non-light mode
    // without relying on a synthetic button click. Default is light.
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

    PlaygroundWindow window(instance);
    if (!window.set_initial_theme(initial_mode) || !window.create_main(show_command)) {
        return 2;
    }
    return app.run();
}
