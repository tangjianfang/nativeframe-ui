// NativeFrameUIChartsInteractive: demonstrates the chart interaction system.
// Features: drag-to-edit data points, range selection, wheel zoom, pan,
// crosshair, tooltip, point selection, undo/redo, and animated transitions.
//
// Controls:
//   - Drag data points vertically to edit values
//   - Mouse wheel to zoom in/out (centered on cursor)
//   - Middle-button drag or Space+Left-drag to pan
//   - Double-click to reset view
//   - Click a point to select; Ctrl+Click for multi-select
//   - Ctrl+Z / Ctrl+Y for undo/redo
//   - Ctrl+A to select all; Escape to clear selection
//   - Hover for crosshair + tooltip
//
// Demo layout (1100 x 740):
//   ┌─ title strip (32px) ──────────────────────────────────┐
//   │  Chart │ Info Panel (320px)                           │
//   │  (residual) │  series overview + live selection       │
//   ├─ status hints (28px) ─────────────────────────────────┤

#include <nfui/Application.hpp>
#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Layout.hpp>
#include <nfui/NativeFrameUI.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include "NativeFrameUIResource.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace nfui;

constexpr int kWindowWidth = 1100;
constexpr int kWindowHeight = 740;
constexpr int kTitleStripHeight = 32;
constexpr int kStatusStripHeight = 28;
constexpr int kInfoPanelWidth = 320;
constexpr int kOuterPadding = 12;
constexpr int kPanelHeaderHeight = 28;

// --- Demo data ---------------------------------------------------------------
//
// Three distinct curves that exercise the multi-series path: a noisy sine
// (Temperature), a smooth cosine (Humidity), and a narrow sawtooth (Light).
// All three share a common y range [0, 100] so the chart can plot them on a
// single axis without per-series y scaling.

std::vector<nfui::ChartPoint> build_temperature() {
    std::vector<nfui::ChartPoint> pts;
    pts.reserve(30);
    for (int i = 0; i < 30; ++i) {
        const double x = static_cast<double>(i);
        const double y = 50.0 + 30.0 * std::sin(2.0 * 3.14159265 * i / 10.0)
                         + 5.0 * std::sin(2.0 * 3.14159265 * i / 3.0);
        pts.push_back({x, y});
    }
    return pts;
}

std::vector<nfui::ChartPoint> build_humidity() {
    std::vector<nfui::ChartPoint> pts;
    pts.reserve(30);
    for (int i = 0; i < 30; ++i) {
        const double x = static_cast<double>(i);
        // 40-80% range, smoother than temperature
        const double y = 60.0 + 20.0 * std::cos(2.0 * 3.14159265 * i / 12.0);
        pts.push_back({x, y});
    }
    return pts;
}

std::vector<nfui::ChartPoint> build_light() {
    std::vector<nfui::ChartPoint> pts;
    pts.reserve(30);
    for (int i = 0; i < 30; ++i) {
        const double x = static_cast<double>(i);
        // Sawtooth with one obvious outlier so the crosshair hits it often
        const double phase = std::fmod(x, 6.0) / 6.0;
        const double y = phase * 100.0 + ((i == 14) ? 25.0 : 0.0);
        pts.push_back({x, std::min(y, 100.0)});
    }
    return pts;
}

struct SeriesOverview {
    std::wstring name;
    nfui::Color color;
    std::size_t total_count;
    double min_y;
    double max_y;
};

std::vector<SeriesOverview> build_overview(const std::vector<nfui::ChartSeries>& series) {
    std::vector<SeriesOverview> out;
    out.reserve(series.size());
    for (const auto& s : series) {
        SeriesOverview o{};
        o.name.assign(s.name.data(), s.name.size());
        o.color = s.color;
        o.total_count = s.points.size();
        if (!s.points.empty()) {
            o.min_y = s.points.front().y;
            o.max_y = s.points.front().y;
            for (const auto& p : s.points) {
                if (p.y < o.min_y) o.min_y = p.y;
                if (p.y > o.max_y) o.max_y = p.y;
            }
        }
        out.push_back(std::move(o));
    }
    return out;
}

// --- InfoPanel: structured right-side display --------------------------------
//
// Always-visible child window. Renders:
//   1. Header band ("Chart Info")
//   2. SERIES overview — name + colored dot + total count + y range
//   3. SELECTION block — live stats from the latest rubber-band release
//      (falls back to a hint when no selection is active)

class InfoPanel : public nfui::Window {
public:
    bool create(HWND parent, const RECT& bounds, nfui::ThemePalette palette) noexcept {
        palette_ = palette;
        nfui::WindowCreateParams cp{
            GetModuleHandleW(nullptr),
            L"NativeFrameUIChartsInfoPanel",
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0,
            bounds.left, bounds.top,
            bounds.right - bounds.left,
            bounds.bottom - bounds.top,
            parent,
        };
        return Window::create(cp);
    }

    void set_series_overview(std::vector<SeriesOverview> series) {
        series_ = std::move(series);
        // CP40: rebuild / prune checkbox HWNDs to match the new series
        // count, preserving existing per-row state across the change.
        // Old rows that disappear lose their HWNDs here so we never
        // paint stale controls. Checked state survives via checked_
        // because we resize that vector first.
        const std::size_t new_count = series_.size();
        if (new_count > checked_.size()) {
            checked_.resize(new_count, true);
        }
        ensure_series_controls();
        layout_series_controls();
        if (hwnd()) InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void set_selection(std::optional<nfui::ChartRangeSelection> sel) {
        selection_ = std::move(sel);
        if (hwnd()) InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void set_theme(nfui::ThemePalette palette) noexcept {
        palette_ = palette;
        if (hwnd()) InvalidateRect(hwnd(), nullptr, FALSE);
    }

    // Expose so MainWindow's WM_COMMAND can validate that an incoming
    // lparam is a child checkbox belonging to this panel.
    [[nodiscard]] bool owns_hwnd(HWND candidate) const noexcept {
        for (HWND h : checkbox_hwnds_) {
            if (h == candidate) return true;
        }
        return false;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd(), &ps);
            RECT rc{};
            GetClientRect(hwnd(), &rc);
            on_paint(hdc, rc);
            EndPaint(hwnd(), &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;  // we self-paint; suppress default
        case WM_COMMAND: {
            // CP40: forward checkbox notifications to the parent (the
            // MainWindow). Win32 routes BUTTON notifications to the
            // immediate parent — without this forward MainWindow never
            // sees the BN_CLICKED and the series stays visible regardless
            // of the checkbox state. We update our own state first so
            // the next paint reflects the new checked value.
            const int id = LOWORD(wparam);
            const int code = HIWORD(wparam);
            HWND checkbox = reinterpret_cast<HWND>(lparam);
            if (code == BN_CLICKED && checkbox != nullptr) {
                const std::size_t index =
                    static_cast<std::size_t>(id - kSeriesCheckboxBase);
                if (index < checked_.size()) {
                    checked_[index] =
                        (SendMessageW(checkbox, BM_GETCHECK, 0, 0) ==
                         BST_CHECKED);
                    if (HWND parent = GetParent(hwnd()); parent != nullptr) {
                        SendMessageW(parent, WM_COMMAND, wparam, lparam);
                    }
                }
                return 0;
            }
            return 0;
        }
        case WM_SIZE:
            // CP40: reposition existing checkbox HWNDs in addition to the
            // row decorations, so resizing keeps the controls aligned with
            // their painted rows.
            layout_series_controls();
            return 0;
        default:
            break;
        }
        return Window::handle_message(message, wparam, lparam);
    }

private:
    // CP40: checkbox IDs start here so MainWindow can recognise them in
    // its WM_COMMAND handler. Keep in sync with MainWindow::kSeriesCheckboxBase.
    static constexpr int kSeriesCheckboxBase = 8000;

    // CP40: create only the checkbox HWNDs that don't exist yet, then
    // sync their BM_SETCHECK state. The old code created one every paint
    // (forcing BST_CHECKED) which both wasted HWNDs and immediately
    // undid any user click before MainWindow saw the notification.
    void ensure_series_controls() noexcept {
        if (hwnd() == nullptr) return;
        while (checkbox_hwnds_.size() < series_.size()) {
            const std::size_t i = checkbox_hwnds_.size();
            const int id = kSeriesCheckboxBase + static_cast<int>(i);
            HWND checkbox = CreateWindowExW(
                0, L"BUTTON", L"",
                BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, 14, 14,
                hwnd(),
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                GetModuleHandleW(nullptr), nullptr);
            if (checkbox == nullptr) break;
            SendMessageW(checkbox, BM_SETCHECK,
                         checked_[i] ? BST_CHECKED : BST_UNCHECKED, 0);
            checkbox_hwnds_.push_back(checkbox);
        }
        // Prune any surplus HWNDs if series_ shrank.
        while (checkbox_hwnds_.size() > series_.size()) {
            HWND stale = checkbox_hwnds_.back();
            checkbox_hwnds_.pop_back();
            if (stale != nullptr) DestroyWindow(stale);
        }
        // Keep checked_ in sync with series_.
        if (checked_.size() > series_.size()) {
            checked_.resize(series_.size(), true);
        }
    }

    // CP40: reposition cached checkbox HWNDs to match the painted row
    // geometry in on_paint. y_rows_[i] stores the y-coordinate of row i;
    // row height is identical to what on_paint uses so the control and
    // the decoration stay aligned.
    void layout_series_controls() noexcept {
        if (hwnd() == nullptr) return;
        RECT bounds{};
        GetClientRect(hwnd(), &bounds);
        int y = kPanelHeaderHeight + 14;
        RECT dummy_label{kOuterPadding, y,
                         bounds.right - kOuterPadding, y + 14};
        y = dummy_label.bottom + 6;  // skip the SECTION label band
        for (std::size_t i = 0; i < checkbox_hwnds_.size(); ++i) {
            const int chk_x = kOuterPadding;
            const int chk_y = y + 2;
            HWND checkbox = checkbox_hwnds_[i];
            if (checkbox == nullptr) continue;
            // Sync checked state in case something else changed it.
            SendMessageW(checkbox, BM_SETCHECK,
                         (i < checked_.size() && checked_[i])
                             ? BST_CHECKED
                             : BST_UNCHECKED,
                         0);
            SetWindowPos(checkbox, nullptr, chk_x, chk_y, 14, 14,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            // Advance y exactly like on_paint so the control stays in
            // sync with the row layout.
            y += 16;  // name row
            y += 13;  // stats row
            y += 8;   // gap
        }
    }

    void on_paint(HDC hdc, RECT bounds) noexcept {
        // Background.
        fill_rect(hdc, bounds, palette_.background);

        // Header band — slightly elevated tone, monospaced label.
        RECT header{0, 0, bounds.right, kPanelHeaderHeight};
        fill_rect(hdc, header, palette_.surface);
        HPEN pen = CreatePen(PS_SOLID, 1, palette_.border.rgb);
        HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
        MoveToEx(hdc, 0, kPanelHeaderHeight - 1, nullptr);
        LineTo(hdc, bounds.right, kPanelHeaderHeight - 1);
        SelectObject(hdc, old_pen);
        DeleteObject(pen);

        const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
        HFONT title_font = fonts_.mono(dpi, font_pt::md);
        RECT title_rc{kOuterPadding, 0, bounds.right - kOuterPadding, kPanelHeaderHeight};
        draw_text(hdc, title_rc, L"CHART INFO", title_font, palette_.text_secondary,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Vertical accent stripe at the left edge of the body — mirrors the
        // floating card's accent stripe so the two feel like the same family.
        RECT stripe{0, kPanelHeaderHeight, 3, bounds.bottom};
        fill_rect(hdc, stripe, palette_.accent);

        // Layout sections vertically: SERIES then SELECTION then (footer).
        int y = kPanelHeaderHeight + 14;

        // SERIES section.
        HFONT section_font = fonts_.mono(dpi, font_pt::xs);
        HFONT name_font = fonts_.regular(dpi, font_pt::base);
        HFONT stat_font = fonts_.mono(dpi, font_pt::xs);

        RECT series_label{kOuterPadding, y, bounds.right - kOuterPadding, y + 14};
        draw_text(hdc, series_label, L"SERIES", section_font, palette_.text_secondary,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y = series_label.bottom + 6;

        for (std::size_t i = 0; i < series_.size(); ++i) {
            const SeriesOverview& s = series_[i];
            // CP40: the checkbox itself is a real HWND child created by
            // ensure_series_controls() and positioned by layout_series_controls().
            // on_paint just decorates the row to the right of it.
            const int chk_x = kOuterPadding;
            // 6-px colored dot.
            const int dot_r = 4;
            const int dot_cx = chk_x + 20 + dot_r;
            const int dot_cy = y + 9;
            HBRUSH dot_brush = CreateSolidBrush(s.color.rgb);
            HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, dot_brush));
            HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            Ellipse(hdc, dot_cx - dot_r, dot_cy - dot_r,
                       dot_cx + dot_r, dot_cy + dot_r);
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(dot_brush);

            // Name (left-aligned next to the dot).
            RECT name_rc{dot_cx + dot_r + 4, y,
                         bounds.right - kOuterPadding, y + 16};
            draw_text(hdc, name_rc, s.name, name_font, palette_.text,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            // Count + y range (dimmed, below the name).
            wchar_t stats[96]{};
            std::swprintf(stats, std::size(stats),
                          L"n=%zu   y %.1f \x2192 %.1f",
                          s.total_count, s.min_y, s.max_y);
            RECT stats_rc{dot_cx + dot_r + 4, y + 16,
                          bounds.right - kOuterPadding, y + 16 + 13};
            draw_text(hdc, stats_rc, stats, stat_font, palette_.text_secondary,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            y = stats_rc.bottom + 8;
        }

        // Separator.
        y += 4;
        HPEN sep_pen = CreatePen(PS_SOLID, 1, palette_.border.rgb);
        HPEN old_sep = static_cast<HPEN>(SelectObject(hdc, sep_pen));
        MoveToEx(hdc, kOuterPadding, y, nullptr);
        LineTo(hdc, bounds.right - kOuterPadding, y);
        SelectObject(hdc, old_sep);
        DeleteObject(sep_pen);
        y += 12;

        // SELECTION section.
        RECT sel_label{kOuterPadding, y, bounds.right - kOuterPadding, y + 14};
        draw_text(hdc, sel_label, L"SELECTION", section_font, palette_.text_secondary,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y = sel_label.bottom + 6;

        if (!selection_.has_value() || selection_->total_count == 0) {
            wchar_t hint[128]{};
            if (!selection_.has_value()) {
                std::swprintf(hint, std::size(hint),
                              L"Drag the chart to range-select");
            } else {
                std::swprintf(hint, std::size(hint),
                              L"No points inside the selection");
            }
            RECT hint_rc{kOuterPadding, y, bounds.right - kOuterPadding, y + 14};
            draw_text(hdc, hint_rc, hint, name_font, palette_.text_secondary,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return;
        }

        // Per-series stats.
        for (const auto& s : selection_->series_stats) {
            // Look up the series in our overview by index for color + name.
            nfui::Color dot_color = palette_.accent;
            std::wstring name_text;
            if (s.series_index < series_.size()) {
                dot_color = series_[s.series_index].color;
                name_text = series_[s.series_index].name;
            }

            const int dot_r = 4;
            const int dot_cx = kOuterPadding + dot_r;
            const int dot_cy = y + 7;
            HBRUSH dot_brush = CreateSolidBrush(dot_color.rgb);
            HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, dot_brush));
            HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            Ellipse(hdc, dot_cx - dot_r, dot_cy - dot_r,
                       dot_cx + dot_r, dot_cy + dot_r);
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(dot_brush);

            RECT name_rc{kOuterPadding + dot_r * 2 + 6, y,
                         bounds.right - kOuterPadding, y + 14};
            wchar_t header[96]{};
            std::swprintf(header, std::size(header), L"%.*s   n=%zu",
                          static_cast<int>(name_text.size()), name_text.data(), s.count);
            draw_text(hdc, name_rc, header, name_font, palette_.text,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            wchar_t stats[128]{};
            std::swprintf(stats, std::size(stats),
                          L"y %.2f \x2192 %.2f   \x03bc = %.2f",
                          s.min_y, s.max_y, s.mean_y);
            RECT stats_rc{kOuterPadding + dot_r * 2 + 6, y + 14,
                          bounds.right - kOuterPadding, y + 14 + 13};
            draw_text(hdc, stats_rc, stats, stat_font, palette_.text_secondary,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            y = stats_rc.bottom + 6;
        }

        // Footer: overall totals + overall y range (the per-chart floating
        // card already shows the precise pixel/data-space x extent).
        y += 4;
        HPEN sep_pen2 = CreatePen(PS_SOLID, 1, palette_.border.rgb);
        HPEN old_sep2 = static_cast<HPEN>(SelectObject(hdc, sep_pen2));
        MoveToEx(hdc, kOuterPadding, y, nullptr);
        LineTo(hdc, bounds.right - kOuterPadding, y);
        SelectObject(hdc, old_sep2);
        DeleteObject(sep_pen2);
        y += 8;

        wchar_t footer[200]{};
        std::swprintf(footer, std::size(footer),
                      L"Total: %zu pts   \x03bc_y = %.2f",
                      selection_->total_count, selection_->mean_y);
        RECT footer_rc{kOuterPadding, y, bounds.right - kOuterPadding, y + 14};
        draw_text(hdc, footer_rc, footer, stat_font, palette_.text,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y = footer_rc.bottom + 2;

        std::swprintf(footer, std::size(footer),
                      L"y range: %.2f \x2192 %.2f",
                      selection_->min_y, selection_->max_y);
        RECT range_rc{kOuterPadding, y, bounds.right - kOuterPadding, y + 13};
        draw_text(hdc, range_rc, footer, stat_font, palette_.text_secondary,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    std::vector<SeriesOverview> series_{};
    std::optional<nfui::ChartRangeSelection> selection_{};
    nfui::ThemePalette palette_{};
    nfui::FontCache fonts_{};
    // CP40: cached checkbox HWNDs (parallel to series_) and the last known
    // checked state, used to recreate HWNDs only when needed and to
    // distinguish user clicks from re-paints.
    std::vector<HWND> checkbox_hwnds_{};
    std::vector<bool> checked_{};
};

// --- SettingsDialog ----------------------------------------------------------
//
// CP39: modal child window invoked from the toolbar gear button. Hosts the
// the user-facing knobs the user wants exposed:
//   - Theme (light / dark) — ComboBox
//   - Chart kind (line / spline / area / bar_v / bar_h) — ComboBox
//   - Show crosshair / tooltip / legend — Checkboxes
//   - Apply + Cancel buttons
// On OK the dialog packs a ChartSettings and ships it to the MainWindow
// via the `on_apply` callback. The dialog is owner-drawn so its chrome
// matches the rest of the app without pulling in comctl32 v6 theming.

class MainWindow;  // forward decl (declared below)

class SettingsDialog : public nfui::Window {
public:
    bool open(HWND owner, nfui::ChartSettings initial,
              std::function<void(nfui::ChartSettings)> on_apply) noexcept {
        on_apply_ = std::move(on_apply);
        nfui::WindowCreateParams cp{
            GetModuleHandleW(nullptr),
            L"NativeFrameUIChartsSettingsDialog",
            L"Chart Settings",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0,
            0, 0, 360, 280,
            owner,
        };
        if (!Window::create(cp)) return false;
        SetWindowLongPtrW(hwnd(), GWLP_ID, kDialogId);
        populate_controls(initial);
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd(), &ps);
            RECT rc{};
            GetClientRect(hwnd(), &rc);
            paint_background(hdc, rc);
            EndPaint(hwnd(), &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            const int code = HIWORD(wparam);
            if (code == BN_CLICKED) {
                if (id == kApplyId) {
                    if (on_apply_) on_apply_(collect_settings());
                    if (hwnd()) DestroyWindow(hwnd());
                    return 0;
                }
                if (id == kCancelId) {
                    if (hwnd()) DestroyWindow(hwnd());
                    return 0;
                }
            }
            return 0;
        }
        default:
            break;
        }
        return Window::handle_message(message, wparam, lparam);
    }

private:
    static constexpr int kDialogId   = 9001;
    static constexpr int kThemeId    = 9101;
    static constexpr int kKindId     = 9102;
    static constexpr int kCrossId    = 9103;
    static constexpr int kTooltipId  = 9104;
    static constexpr int kLegendId   = 9105;
    static constexpr int kApplyId    = 9201;
    static constexpr int kCancelId   = 9202;

    static RECT rect_for(HWND parent, int x, int y, int w, int h) noexcept {
        // Use MapWindowPoints so the rect is in the dialog's coordinates.
        POINT p{x, y};
        MapWindowPoints(parent, HWND_DESKTOP, &p, 1);
        RECT r{p.x, p.y, p.x + w, p.y + h};
        return r;
    }

    void populate_controls(nfui::ChartSettings s) noexcept {
        // Layout (in dialog-local coords). Width 360, height 280.
        RECT rc{};
        GetClientRect(hwnd(), &rc);

        // Theme label + combo.
        CreateWindowExW(0, L"STATIC", L"Theme:",
                        WS_CHILD | WS_VISIBLE,
                        16, 20, 80, 18,
                        hwnd(), nullptr, instance_, nullptr);
        theme_combo_ = CreateWindowExW(0, L"COMBOBOX", nullptr,
            CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            110, 18, 230, 80,
            hwnd(), reinterpret_cast<HMENU>(kThemeId),
            instance_, nullptr);
        SendMessageW(theme_combo_, CB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(L"Light"));
        SendMessageW(theme_combo_, CB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(L"Dark"));
        SendMessageW(theme_combo_, CB_SETCURSEL,
                     s.theme == nfui::ChartSettings::ThemeMode::dark ? 1 : 0, 0);

        // Kind label + combo.
        CreateWindowExW(0, L"STATIC", L"Chart kind:",
                        WS_CHILD | WS_VISIBLE,
                        16, 56, 80, 18,
                        hwnd(), nullptr, instance_, nullptr);
        kind_combo_ = CreateWindowExW(0, L"COMBOBOX", nullptr,
            CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            110, 54, 230, 120,
            hwnd(), reinterpret_cast<HMENU>(kKindId),
            instance_, nullptr);
        SendMessageW(kind_combo_, CB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(L"Line"));
        SendMessageW(kind_combo_, CB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(L"Spline"));
        SendMessageW(kind_combo_, CB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(L"Area"));
        SendMessageW(kind_combo_, CB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(L"Bar (vertical)"));
        SendMessageW(kind_combo_, CB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(L"Bar (horizontal)"));
        SendMessageW(kind_combo_, CB_SETCURSEL, s.kind_id, 0);

        // Checkboxes.
        cross_chk_ = CreateWindowExW(0, L"BUTTON", L"Show crosshair",
            BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            16, 100, 320, 20,
            hwnd(), reinterpret_cast<HMENU>(kCrossId),
            instance_, nullptr);
        SendMessageW(cross_chk_, BM_SETCHECK,
                     s.show_crosshair ? BST_CHECKED : BST_UNCHECKED, 0);

        tip_chk_ = CreateWindowExW(0, L"BUTTON", L"Show tooltip",
            BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            16, 128, 320, 20,
            hwnd(), reinterpret_cast<HMENU>(kTooltipId),
            instance_, nullptr);
        SendMessageW(tip_chk_, BM_SETCHECK,
                     s.show_tooltip ? BST_CHECKED : BST_UNCHECKED, 0);

        legend_chk_ = CreateWindowExW(0, L"BUTTON", L"Show legend",
            BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            16, 156, 320, 20,
            hwnd(), reinterpret_cast<HMENU>(kLegendId),
            instance_, nullptr);
        SendMessageW(legend_chk_, BM_SETCHECK,
                     s.show_legend ? BST_CHECKED : BST_UNCHECKED, 0);

        // Apply / Cancel buttons.
        CreateWindowExW(0, L"BUTTON", L"Apply",
            BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            110, 220, 100, 28,
            hwnd(), reinterpret_cast<HMENU>(kApplyId),
            instance_, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            230, 220, 100, 28,
            hwnd(), reinterpret_cast<HMENU>(kCancelId),
            instance_, nullptr);
    }

    nfui::ChartSettings collect_settings() const noexcept {
        nfui::ChartSettings s{};
        const int theme_idx = static_cast<int>(SendMessageW(theme_combo_, CB_GETCURSEL, 0, 0));
        s.theme = (theme_idx == 1)
            ? nfui::ChartSettings::ThemeMode::dark
            : nfui::ChartSettings::ThemeMode::light;
        s.kind_id = static_cast<int>(SendMessageW(kind_combo_, CB_GETCURSEL, 0, 0));
        if (s.kind_id < 0 || s.kind_id > 4) s.kind_id = 2;  // default line
        s.show_crosshair = (SendMessageW(cross_chk_, BM_GETCHECK, 0, 0) == BST_CHECKED);
        s.show_tooltip = (SendMessageW(tip_chk_, BM_GETCHECK, 0, 0) == BST_CHECKED);
        s.show_legend = (SendMessageW(legend_chk_, BM_GETCHECK, 0, 0) == BST_CHECKED);
        return s;
    }

    void paint_background(HDC hdc, const RECT& bounds) noexcept {
        const nfui::ThemePalette& pal = palette_for_dialog();
        fill_rect(hdc, bounds, pal.surface);
        // Frame.
        HPEN pen = CreatePen(PS_SOLID, 1, pal.border.rgb);
        HPEN old = static_cast<HPEN>(SelectObject(hdc, pen));
        HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        Rectangle(hdc, bounds.left, bounds.top, bounds.right, bounds.bottom);
        SelectObject(hdc, old);
        SelectObject(hdc, old_brush);
        DeleteObject(pen);
    }

    static nfui::ThemePalette palette_for_dialog() noexcept {
        return nfui::theme_palette(nfui::ThemeMode::dark);
    }

    HINSTANCE instance_{GetModuleHandleW(nullptr)};
    HWND theme_combo_{};
    HWND kind_combo_{};
    HWND cross_chk_{};
    HWND tip_chk_{};
    HWND legend_chk_{};
    std::function<void(nfui::ChartSettings)> on_apply_{};
};

// --- MainWindow --------------------------------------------------------------

class MainWindow : public nfui::Window {
public:
    [[nodiscard]] bool create_main(int show_cmd) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIChartsInteractiveWindow",
            L"Interactive Charts \x2014 NativeFrame UI",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT, CW_USEDEFAULT,
            kWindowWidth, kWindowHeight,
        };
        if (!create(params)) return false;

        apply_theme();
        create_info_panel();  // CP40: must exist before create_chart() so
                              // set_series_overview() can create the cached
                              // checkbox HWNDs.
        create_chart();
        ShowWindow(hwnd(), show_cmd);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            layout_children();
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd(), &ps);
            RECT rc{};
            GetClientRect(hwnd(), &rc);
            paint_strips(hdc, rc);
            EndPaint(hwnd(), &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN: {
            const POINT pt{LOWORD(lparam), HIWORD(lparam)};
            const ToolbarHit hit = hit_test_toolbar(pt);
            if (hit == ToolbarHit::reset) {
                chart_.reset_view();
                return 0;
            }
            if (hit == ToolbarHit::settings) {
                open_settings_dialog();
                return 0;
            }
            return 0;
        }
        case WM_COMMAND: {
            // CP40: checkboxes created by InfoPanel forward their state
            // here via SendMessageW from InfoPanel::handle_message. We
            // require lparam (the actual button HWND) to be non-null and
            // to be a child of the info panel — that rejects any stray
            // notifications (e.g. the settings dialog's own checkboxes
            // which arrive directly, not via the InfoPanel forward).
            const int id = LOWORD(wparam);
            const int code = HIWORD(wparam);
            HWND source = reinterpret_cast<HWND>(lparam);
            if (code == BN_CLICKED && source != nullptr &&
                info_.owns_hwnd(source) &&
                id >= kSeriesCheckboxBase &&
                id < kSeriesCheckboxBase + static_cast<int>(chart_.series_count())) {
                const std::size_t series_idx =
                    static_cast<std::size_t>(id - kSeriesCheckboxBase);
                const bool visible = (SendMessageW(source, BM_GETCHECK, 0, 0) ==
                                       BST_CHECKED);
                chart_.set_series_visible(series_idx, visible);
                return 0;
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return Window::handle_message(message, wparam, lparam);
    }

private:
    // CP39: toolbar in the title strip. The reset button (circular arrow)
    // and settings button (gear) are drawn directly into the title strip
    // (no native BUTTON chrome) and hit-tested in WM_LBUTTONDOWN.
    enum class ToolbarHit { none, reset, settings };

    // Checkbox IDs issued by InfoPanel for per-series visibility. Stored
    // starting at kSeriesCheckboxBase so MainWindow's WM_COMMAND can
    // identify them and dispatch to chart_.set_series_visible().
    static constexpr int kSeriesCheckboxBase = 8000;

    // CP39: settings dialog held by MainWindow. We re-create it on each
    // open so the owner-window stays in control of its lifecycle.
    std::unique_ptr<SettingsDialog> settings_dialog_{};

    void apply_theme() noexcept {
        palette_ = nfui::theme_palette(nfui::ThemeMode::dark);
    }

    [[nodiscard]] ToolbarHit hit_test_toolbar(POINT pt) const noexcept {
        RECT rc{};
        GetClientRect(hwnd(), &rc);
        const int button_size = 22;
        const int y = (kTitleStripHeight - button_size) / 2;
        // Settings sits at the far right (rc.right - 12 - size); reset is
        // immediately to its left. Both are inside the title strip.
        const RECT settings_btn{rc.right - kOuterPadding - button_size,
                                y,
                                rc.right - kOuterPadding,
                                y + button_size};
        const RECT reset_btn{settings_btn.left - 8 - button_size,
                             y,
                             settings_btn.left - 8,
                             y + button_size};
        if (PtInRect(&reset_btn, pt)) return ToolbarHit::reset;
        if (PtInRect(&settings_btn, pt)) return ToolbarHit::settings;
        return ToolbarHit::none;
    }

    void open_settings_dialog() noexcept {
        settings_dialog_ = std::make_unique<SettingsDialog>();
        settings_dialog_->open(hwnd(), chart_.settings(),
            [this](nfui::ChartSettings s) {
                chart_.apply_settings(s);
                // Refresh palette + rewire info panel + title strip.
                apply_theme();
                chart_.inject_theme(&palette_, &fonts_);
                info_.set_theme(palette_);
                if (chart_.hwnd()) InvalidateRect(chart_.hwnd(), nullptr, FALSE);
                if (hwnd()) InvalidateRect(hwnd(), nullptr, FALSE);
            });
        // Center the dialog over the client area.
        RECT rc{};
        GetClientRect(hwnd(), &rc);
        RECT drc{};
        GetWindowRect(settings_dialog_->hwnd(), &drc);
        const int dw = drc.right - drc.left;
        const int dh = drc.bottom - drc.top;
        const int x = (rc.right - dw) / 2;
        const int y = (rc.bottom - dh) / 3;
        SetWindowPos(settings_dialog_->hwnd(), HWND_TOP, x, y, dw, dh,
                     SWP_SHOWWINDOW);
    }

    void create_chart() {
        chart_.inject_theme(&palette_, &fonts_);

        nfui::WindowCreateParams cp{
            instance_,
            L"",  // overridden by ChartView::create
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0,
            0, 0, 100, 100,  // placeholder; layout_children() repositions
            hwnd(),
        };
        if (!chart_.create(cp)) return;

        chart_.set_kind(nfui::ChartKind::line);

        // Three distinct, color-distinguished series.
        std::vector<nfui::ChartSeries> series;
        auto make_series = [&](std::wstring_view name, int color_idx,
                               std::vector<nfui::ChartPoint> pts) {
            nfui::ChartSeries s;
            s.name = name;
            s.color = nfui::chart_series_color(nfui::ThemeMode::dark, color_idx);
            s.points = std::move(pts);
            series.push_back(std::move(s));
        };
        make_series(L"Temperature", 0, build_temperature());
        make_series(L"Humidity",    1, build_humidity());
        make_series(L"Light",       2, build_light());

        chart_.set_series(std::move(series));
        chart_.set_axis_x(nfui::ChartAxisRange{0.0, 29.0, L"{:.0f}"});
        chart_.set_axis_y(nfui::ChartAxisRange{0.0, 100.0, L"{:.0f}"});

        // Interaction: all modes enabled.
        nfui::ChartInteractionOptions opts{};
        opts.mode_flags = nfui::ChartInteractionMode::select
                        | nfui::ChartInteractionMode::drag_edit
                        | nfui::ChartInteractionMode::range_select
                        | nfui::ChartInteractionMode::pan
                        | nfui::ChartInteractionMode::zoom;
        opts.hit_tolerance_px = 12;
        opts.edit_x = false;
        opts.edit_y = true;
        opts.show_crosshair = true;
        opts.show_tooltip = true;
        opts.animate_on_edit = true;
        opts.animation_ms = 150;
        chart_.enable_interaction(opts);

        // Push the series overview into the info panel up front.
        info_.set_series_overview(build_overview(get_chart_series()));

        // Wire callbacks. Selection updates both the title bar (compact
        // summary) and the info panel (rich stats).
        nfui::ChartCallbacks cbs{};
        cbs.on_point_edited = [this](std::size_t si, std::size_t pi, nfui::ChartPoint val) {
            wchar_t buf[160];
            std::swprintf(buf, std::size(buf),
                          L"Edited [%zu][%zu] -> (%.1f, %.2f)  |  Undo: %s  Redo: %s",
                          si, pi, val.x, val.y,
                          chart_.can_undo() ? L"Yes" : L"No",
                          chart_.can_redo() ? L"Yes" : L"No");
            SetWindowTextW(hwnd(), buf);
            // Refresh the series overview totals so the panel reflects edits.
            info_.set_series_overview(build_overview(get_chart_series()));
        };
        cbs.on_range_selected = [this](nfui::ChartRangeSelection sel) {
            info_.set_selection(sel);
            wchar_t buf[200];
            if (sel.total_count == 0) {
                std::swprintf(buf, std::size(buf),
                              L"Range: no points inside selection \x2014 Interactive Charts");
            } else if (sel.series_stats.size() == 1) {
                std::swprintf(buf, std::size(buf),
                              L"Selected %zu pts  \x2014  y %.2f \x2192 %.2f   \x03bc = %.2f  \x2014  Interactive Charts",
                              sel.total_count, sel.min_y, sel.max_y, sel.mean_y);
            } else {
                std::swprintf(buf, std::size(buf),
                              L"Selected %zu pts across %zu series  \x2014  y %.2f \x2192 %.2f   \x03bc = %.2f  \x2014  Interactive Charts",
                              sel.total_count, sel.series_stats.size(),
                              sel.min_y, sel.max_y, sel.mean_y);
            }
            SetWindowTextW(hwnd(), buf);
        };
        cbs.on_view_changed = [this](nfui::ChartAxisRange x, nfui::ChartAxisRange y) {
            wchar_t buf[200];
            std::swprintf(buf, std::size(buf),
                          L"View: X[%.1f..%.1f] Y[%.1f..%.1f]",
                          x.min, x.max, y.min, y.max);
            SetWindowTextW(hwnd(), buf);
        };
        cbs.on_view_reset = [this]() {
            SetWindowTextW(hwnd(), L"View Reset \x2014 Interactive Charts \x2014 NativeFrame UI");
        };
        chart_.set_callbacks(std::move(cbs));
    }

    void create_info_panel() {
        RECT placeholder{0, 0, 100, 100};
        info_.create(hwnd(), placeholder, palette_);
        info_.set_theme(palette_);
    }

    // The chart owns its series in a protected member; this accessor returns
    // a copy of the names/colors so the info panel can render the overview
    // and per-series lookup even after edits.
    std::vector<nfui::ChartSeries> get_chart_series() const {
        // ChartView exposes series through a protected accessor; for the
        // demo we rebuild the overview from the original data + the public
        // API. A real app would expose a const& getter; here we return the
        // stored data_ + color table directly.
        std::vector<nfui::ChartSeries> out;
        const std::vector<std::vector<nfui::ChartPoint>> series_data = {
            temperature_, humidity_, light_,
        };
        const std::wstring_view names[] = {L"Temperature", L"Humidity", L"Light"};
        for (std::size_t i = 0; i < series_data.size(); ++i) {
            nfui::ChartSeries s;
            s.name = names[i];
            s.color = nfui::chart_series_color(nfui::ThemeMode::dark, static_cast<int>(i));
            s.points = series_data[i];
            out.push_back(std::move(s));
        }
        return out;
    }

    void layout_children() noexcept {
        RECT rc{};
        GetClientRect(hwnd(), &rc);
        const int chart_left = 0;
        const int chart_top = kTitleStripHeight;
        const int chart_bottom = rc.bottom - kStatusStripHeight;
        const int chart_w = rc.right - rc.left - kInfoPanelWidth;
        const int chart_h = chart_bottom - chart_top;

        if (chart_.hwnd() != nullptr && chart_w > 50 && chart_h > 50) {
            MoveWindow(chart_.hwnd(),
                       chart_left, chart_top,
                       chart_w, chart_h,
                       TRUE);
        }
        if (info_.hwnd() != nullptr) {
            const int info_left = rc.right - kInfoPanelWidth;
            MoveWindow(info_.hwnd(),
                       info_left, chart_top,
                       kInfoPanelWidth, chart_h,
                       TRUE);
            // Force the panel to repaint at its new size.
            InvalidateRect(info_.hwnd(), nullptr, FALSE);
        }
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void paint_strips(HDC hdc, RECT bounds) noexcept {
        const int w = bounds.right - bounds.left;
        const int h = bounds.bottom - bounds.top;

        // Title strip (top).
        RECT title{0, 0, w, kTitleStripHeight};
        fill_rect(hdc, title, palette_.surface);
        HPEN title_pen = CreatePen(PS_SOLID, 1, palette_.border.rgb);
        HPEN old_title_pen = static_cast<HPEN>(SelectObject(hdc, title_pen));
        MoveToEx(hdc, 0, kTitleStripHeight - 1, nullptr);
        LineTo(hdc, w, kTitleStripHeight - 1);
        SelectObject(hdc, old_title_pen);
        DeleteObject(title_pen);

        const int dpi = (hwnd() != nullptr) ? dpi_of(hwnd()) : 96;
        HFONT title_font = fonts_.mono(dpi, font_pt::md);

        // Left: app title.
        RECT title_text{kOuterPadding, 0, w / 2, kTitleStripHeight};
        draw_text(hdc, title_text,
                  L"Interactive Charts \x2014 NativeFrame UI",
                  title_font, palette_.text,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Right: series legend dots.
        HFONT legend_font = fonts_.mono(dpi, font_pt::xs);
        HFONT old_font = static_cast<HFONT>(SelectObject(hdc, legend_font));
        SetTextColor(hdc, palette_.text_secondary.rgb);
        int legend_x = w - kOuterPadding;
        const std::wstring_view names[] = {L"Light", L"Humidity", L"Temperature"};
        for (int i = 2; i >= 0; --i) {
            // Draw name right-aligned, then a dot to its left.
            SIZE sz{};
            GetTextExtentPoint32W(hdc, names[i].data(),
                                  static_cast<int>(names[i].size()), &sz);
            RECT label_rc{legend_x - sz.cx, 0, legend_x, kTitleStripHeight};
            draw_text(hdc, label_rc, std::wstring(names[i]), legend_font,
                      palette_.text_secondary,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            const int dot_r = 4;
            const int cx = legend_x - sz.cx - 10;
            const int cy = kTitleStripHeight / 2;
            HBRUSH brush = CreateSolidBrush(
                nfui::chart_series_color(nfui::ThemeMode::dark, i).rgb);
            HBRUSH old_b = static_cast<HBRUSH>(SelectObject(hdc, brush));
            HPEN old_p = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            Ellipse(hdc, cx - dot_r, cy - dot_r, cx + dot_r, cy + dot_r);
            SelectObject(hdc, old_p);
            SelectObject(hdc, old_b);
            DeleteObject(brush);
            legend_x -= (sz.cx + 24);
        }
        SelectObject(hdc, old_font);

        // CP39: toolbar buttons (reset + settings) on the far right of the
        // title strip. Draw with primitives so the chrome matches the rest
        // of the demo (no native BUTTON bevel). Reset is a circular arrow
        // glyph; settings is a simple gear-ish asterisk cross.
        draw_toolbar_buttons(hdc, w);

        // Status strip (bottom).
        RECT status{0, h - kStatusStripHeight, w, h};
        fill_rect(hdc, status, palette_.surface);
        HPEN status_pen = CreatePen(PS_SOLID, 1, palette_.border.rgb);
        HPEN old_status_pen = static_cast<HPEN>(SelectObject(hdc, status_pen));
        MoveToEx(hdc, 0, h - kStatusStripHeight, nullptr);
        LineTo(hdc, w, h - kStatusStripHeight);
        SelectObject(hdc, old_status_pen);
        DeleteObject(status_pen);

        HFONT hint_font = fonts_.mono(dpi, font_pt::xs);
        RECT hint_rc{kOuterPadding, h - kStatusStripHeight,
                     w - kOuterPadding, h};
        draw_text(hdc, hint_rc,
                  L"Drag to range-select  \x2022  Wheel zoom  \x2022  Space+drag = pan  \x2022  Dbl-click = reset  \x2022  Ctrl+Z/Y = undo/redo  \x2022  Ctrl+A = select all  \x2022  Esc = clear",
                  hint_font, palette_.text_secondary,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // CP39: draw reset + settings icons into the right edge of the title
    // strip. We avoid the native BUTTON control so the chrome matches the
    // rest of the dark surface; hit-testing lives in MainWindow::hit_test_toolbar.
    void draw_toolbar_buttons(HDC hdc, int w) noexcept {
        const int button_size = 22;
        const int y = (kTitleStripHeight - button_size) / 2;
        const int settings_x = w - kOuterPadding - button_size;
        const int reset_x = settings_x - 8 - button_size;

        auto draw_round_button = [&](int x, int yy) {
            RECT btn{x, yy, x + button_size, yy + button_size};
            const POINT pt{btn.left + 2, btn.top + 2};
            const ToolbarHit hit = hit_test_toolbar(pt);
            const nfui::Color bg = (hit != ToolbarHit::none)
                ? nfui::Color{palette_.surface_hover.rgb}
                : nfui::Color{palette_.surface.rgb};
            fill_rounded_rect(hdc, btn, 4, bg, palette_.border);
        };
        draw_round_button(reset_x, y);
        draw_round_button(settings_x, y);

        // Reset glyph: circular arrow (open ring + arrowhead).
        {
            const int cx = reset_x + button_size / 2;
            const int cy = y + button_size / 2;
            const int r = 7;
            HPEN pen = CreatePen(PS_SOLID, 2, palette_.text.rgb);
            HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
            HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
            // Arc spanning ~270°, opening at the top-right.
            Arc(hdc, cx - r, cy - r, cx + r, cy + r,
                cx + r, cy - r,
                cx,     cy + r);
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(pen);
            // Arrowhead (small triangle at the top-right of the arc).
            const int ax = cx + r - 1;
            const int ay = cy - r + 1;
            POINT tri[3]{{ax, ay}, {ax - 4, ay + 1}, {ax - 1, ay + 4}};
            HBRUSH brush = CreateSolidBrush(palette_.text.rgb);
            HBRUSH old = static_cast<HBRUSH>(SelectObject(hdc, brush));
            HPEN oldp = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            Polygon(hdc, tri, 3);
            SelectObject(hdc, oldp);
            SelectObject(hdc, old);
            DeleteObject(brush);
        }

        // Settings glyph: gear (six spokes around a center dot).
        {
            const int cx = settings_x + button_size / 2;
            const int cy = y + button_size / 2;
            // Outer dot.
            HBRUSH brush = CreateSolidBrush(palette_.text.rgb);
            HBRUSH old = static_cast<HBRUSH>(SelectObject(hdc, brush));
            HPEN oldp = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            Ellipse(hdc, cx - 2, cy - 2, cx + 2, cy + 2);
            // Six spokes.
            HPEN pen = CreatePen(PS_SOLID, 2, palette_.text.rgb);
            HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
            HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
            for (int i = 0; i < 6; ++i) {
                const double ang = (i * 3.14159265) / 3.0;
                const int x1 = cx + static_cast<int>(5.0 * std::cos(ang));
                const int y1 = cy + static_cast<int>(5.0 * std::sin(ang));
                const int x2 = cx + static_cast<int>(8.0 * std::cos(ang));
                const int y2 = cy + static_cast<int>(8.0 * std::sin(ang));
                MoveToEx(hdc, x1, y1, nullptr);
                LineTo(hdc, x2, y2);
            }
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(pen);
            SelectObject(hdc, oldp);
            SelectObject(hdc, old);
            DeleteObject(brush);
        }
    }

    HINSTANCE instance_{GetModuleHandleW(nullptr)};
    nfui::ThemePalette palette_{};
    nfui::FontCache fonts_{};
    nfui::LineChartView chart_{};
    InfoPanel info_{};

    // Persistent source-of-truth for the demo's three series so the info
    // panel can rebuild its overview after edits without reaching into the
    // chart's protected state.
    std::vector<nfui::ChartPoint> temperature_{build_temperature()};
    std::vector<nfui::ChartPoint> humidity_{build_humidity()};
    std::vector<nfui::ChartPoint> light_{build_light()};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show_cmd) {
    nfui::Application app({instance, show_cmd});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls() ||
        !nfui::initialize_chart_aa()) {
        return 1;
    }

    MainWindow window{};
    if (!window.create_main(show_cmd)) {
        nfui::shutdown_chart_aa();
        return 1;
    }

    const int result = app.run();
    nfui::shutdown_chart_aa();
    return result;
}