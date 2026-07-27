// NativeFrameUIChartsInteractive: CP40 dashboard demo.
//
// Layout (1180 x 900):
//   ┌─ title strip (32px) ─────────────────────────────────────────────┐
//   │  Interactive Charts — NativeFrame UI             [Export][Reset][⚙]│
//   ├─ KPI row (132px) ─────────────────────────────────────┬─ InfoPanel ┤
//   │ ┌──────────┐ ┌──────────┐ ┌──────────┐                │  (320px)  │
//   │ │  Temp    │ │  Humid   │ │  Light   │                │ series    │
//   │ └──────────┘ └──────────┘ └──────────┘                │ overview  │
//   ├─ Primary chart (≈55% remaining chart height) ──────── ┤ + live    │
//   ├─ Comparison chart (remaining) ─────────────────────── ┤ selection │
//   ├─ status strip (28px) ─────────────────────────────────┴───────────┤
//
// Behaviour:
//   - Three KPI tiles, each with its own sparkline + delta (% vs. previous).
//   - Primary line chart hosts three series, controlled by the right-rail
//     InfoPanel checkboxes (drag-to-edit / wheel zoom / pan / dbl-click reset).
//   - Comparison line chart shows the temperature series on an independent
//     35..85 y-range.
//   - The two charts are linked via a ChartGroup: x-axis range and cursor
//     synchronise; y-axis ranges remain independent.
//   - Toolbar: Export PNG (GetSaveFileNameW -> primary chart export),
//     Reset (resets both linked views), Settings (opens the dialog).
//   - Status strip shows the latest hover/selection/edit feedback.

#include <nfui/Application.hpp>
#include <nfui/ChartGroup.hpp>
#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/KpiTile.hpp>
#include <nfui/Layout.hpp>
#include <nfui/NativeFrameUI.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include "NativeFrameUIResource.h"

#include <commdlg.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace nfui;

constexpr int kWindowWidth = 1180;
constexpr int kWindowHeight = 900;
constexpr int kTitleStripHeight = 32;
constexpr int kStatusStripHeight = 28;
constexpr int kInfoPanelWidth = 320;
constexpr int kOuterPadding = 12;
constexpr int kPanelHeaderHeight = 28;

// --- Demo data ---------------------------------------------------------------
//
// Three distinct curves that exercise the multi-series path: a noisy sine
// (Temperature), a smooth cosine (Humidity), and a narrow sawtooth (Light).
// The dataset has 30 samples so the demo can show x ticks 0..29.

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

// --- InfoPanel ---------------------------------------------------------------
//
// Same role as before: per-series visibility checkboxes, live selection
// stats, and an overview block. Now sized to the dashboard's right rail.

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
            return 1;
        case WM_COMMAND: {
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
            layout_series_controls();
            return 0;
        default:
            break;
        }
        return Window::handle_message(message, wparam, lparam);
    }

private:
    static constexpr int kSeriesCheckboxBase = 8000;

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
        while (checkbox_hwnds_.size() > series_.size()) {
            HWND stale = checkbox_hwnds_.back();
            checkbox_hwnds_.pop_back();
            if (stale != nullptr) DestroyWindow(stale);
        }
        if (checked_.size() > series_.size()) {
            checked_.resize(series_.size(), true);
        }
    }

    void layout_series_controls() noexcept {
        if (hwnd() == nullptr) return;
        RECT bounds{};
        GetClientRect(hwnd(), &bounds);
        int y = kPanelHeaderHeight + 14;
        y += 14 + 6;  // skip SECTION label band
        for (std::size_t i = 0; i < checkbox_hwnds_.size(); ++i) {
            const int chk_x = kOuterPadding;
            const int chk_y = y + 2;
            HWND checkbox = checkbox_hwnds_[i];
            if (checkbox == nullptr) continue;
            SendMessageW(checkbox, BM_SETCHECK,
                         (i < checked_.size() && checked_[i])
                             ? BST_CHECKED
                             : BST_UNCHECKED,
                         0);
            SetWindowPos(checkbox, nullptr, chk_x, chk_y, 14, 14,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            y += 16;  // name row
            y += 13;  // stats row
            y += 8;   // gap
        }
    }

    void on_paint(HDC hdc, RECT bounds) noexcept {
        fill_rect(hdc, bounds, palette_.background);

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

        RECT stripe{0, kPanelHeaderHeight, 3, bounds.bottom};
        fill_rect(hdc, stripe, palette_.accent);

        int y = kPanelHeaderHeight + 14;

        HFONT section_font = fonts_.mono(dpi, font_pt::xs);
        HFONT name_font = fonts_.regular(dpi, font_pt::base);
        HFONT stat_font = fonts_.mono(dpi, font_pt::xs);

        RECT series_label{kOuterPadding, y, bounds.right - kOuterPadding, y + 14};
        draw_text(hdc, series_label, L"SERIES", section_font, palette_.text_secondary,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y = series_label.bottom + 6;

        for (std::size_t i = 0; i < series_.size(); ++i) {
            const SeriesOverview& s = series_[i];
            const int chk_x = kOuterPadding;
            const int dot_r = 4;
            const int dot_cx = chk_x + 20 + dot_r;
            const int dot_cy = y + 9;
            HBRUSH dot_brush = CreateSolidBrush(s.color.rgb);
            HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, dot_brush));
            HPEN old_pen_local = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            Ellipse(hdc, dot_cx - dot_r, dot_cy - dot_r,
                       dot_cx + dot_r, dot_cy + dot_r);
            SelectObject(hdc, old_pen_local);
            SelectObject(hdc, old_brush);
            DeleteObject(dot_brush);

            RECT name_rc{dot_cx + dot_r + 4, y,
                         bounds.right - kOuterPadding, y + 16};
            draw_text(hdc, name_rc, s.name, name_font, palette_.text,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

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

        y += 4;
        HPEN sep_pen = CreatePen(PS_SOLID, 1, palette_.border.rgb);
        HPEN old_sep = static_cast<HPEN>(SelectObject(hdc, sep_pen));
        MoveToEx(hdc, kOuterPadding, y, nullptr);
        LineTo(hdc, bounds.right - kOuterPadding, y);
        SelectObject(hdc, old_sep);
        DeleteObject(sep_pen);
        y += 12;

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

        for (const auto& s : selection_->series_stats) {
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
            HPEN old_pen_local = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            Ellipse(hdc, dot_cx - dot_r, dot_cy - dot_r,
                       dot_cx + dot_r, dot_cy + dot_r);
            SelectObject(hdc, old_pen_local);
            SelectObject(hdc, old_brush);
            DeleteObject(dot_brush);

            RECT name_rc{kOuterPadding + dot_r * 2 + 6, y,
                         bounds.right - kOuterPadding, y + 14};
            wchar_t row_header[96]{};
            std::swprintf(row_header, std::size(row_header), L"%.*s   n=%zu",
                          static_cast<int>(name_text.size()), name_text.data(), s.count);
            draw_text(hdc, name_rc, row_header, name_font, palette_.text,
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
    std::vector<HWND> checkbox_hwnds_{};
    std::vector<bool> checked_{};
};

// --- SettingsDialog ----------------------------------------------------------
//
// Same model as CP39: owner-drawn modal child window. We re-create it on
// each open so the host can dispose of it cleanly via DestroyWindow on OK.

class MainWindow;  // forward decl

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

    void populate_controls(nfui::ChartSettings s) noexcept {
        RECT rc{};
        GetClientRect(hwnd(), &rc);

        CreateWindowExW(0, L"STATIC", L"Theme:",
                        WS_CHILD | WS_VISIBLE,
                        16, 20, 80, 18,
                        hwnd(), nullptr, instance_, nullptr);
        theme_combo_ = CreateWindowExW(0, L"COMBOBOX", nullptr,
            CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            110, 18, 230, 80,
            hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(kThemeId)),
            instance_, nullptr);
        SendMessageW(theme_combo_, CB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(L"Light"));
        SendMessageW(theme_combo_, CB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(L"Dark"));
        SendMessageW(theme_combo_, CB_SETCURSEL,
                     s.theme == nfui::ChartSettings::ThemeMode::dark ? 1 : 0, 0);

        CreateWindowExW(0, L"STATIC", L"Chart kind:",
                        WS_CHILD | WS_VISIBLE,
                        16, 56, 80, 18,
                        hwnd(), nullptr, instance_, nullptr);
        kind_combo_ = CreateWindowExW(0, L"COMBOBOX", nullptr,
            CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            110, 54, 230, 120,
            hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(kKindId)),
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

        cross_chk_ = CreateWindowExW(0, L"BUTTON", L"Show crosshair",
            BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            16, 100, 320, 20,
            hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCrossId)),
            instance_, nullptr);
        SendMessageW(cross_chk_, BM_SETCHECK,
                     s.show_crosshair ? BST_CHECKED : BST_UNCHECKED, 0);

        tip_chk_ = CreateWindowExW(0, L"BUTTON", L"Show tooltip",
            BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            16, 128, 320, 20,
            hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTooltipId)),
            instance_, nullptr);
        SendMessageW(tip_chk_, BM_SETCHECK,
                     s.show_tooltip ? BST_CHECKED : BST_UNCHECKED, 0);

        legend_chk_ = CreateWindowExW(0, L"BUTTON", L"Show legend",
            BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            16, 156, 320, 20,
            hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(kLegendId)),
            instance_, nullptr);
        SendMessageW(legend_chk_, BM_SETCHECK,
                     s.show_legend ? BST_CHECKED : BST_UNCHECKED, 0);

        CreateWindowExW(0, L"BUTTON", L"Apply",
            BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            110, 220, 100, 28,
            hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(kApplyId)),
            instance_, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            230, 220, 100, 28,
            hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCancelId)),
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

        // Resolve the palette up front so every child binds to the final
        // value. ChartView::create() resets its internal palette_ pointer
        // to nullptr, so inject_theme MUST run after create() (and the
        // same applies to InfoPanel's ThemePalette by-value member).
        apply_theme();
        create_charts();      // creates primary_chart_ first so apply_theme() can read its theme
        create_kpi_tiles();
        create_info_panel();
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
            switch (hit) {
            case ToolbarHit::export_png:
                export_primary_to_png();
                return 0;
            case ToolbarHit::reset:
                reset_views();
                return 0;
            case ToolbarHit::settings:
                open_settings_dialog();
                return 0;
            case ToolbarHit::none:
            default:
                return 0;
            }
        }
        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            const int code = HIWORD(wparam);
            HWND source = reinterpret_cast<HWND>(lparam);
            if (code == BN_CLICKED && source != nullptr &&
                info_.owns_hwnd(source) &&
                id >= kSeriesCheckboxBase &&
                id < kSeriesCheckboxBase +
                     static_cast<int>(primary_chart_.series_count())) {
                const std::size_t series_idx =
                    static_cast<std::size_t>(id - kSeriesCheckboxBase);
                const bool visible = (SendMessageW(source, BM_GETCHECK, 0, 0) ==
                                       BST_CHECKED);
                primary_chart_.set_series_visible(series_idx, visible);
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
    enum class ToolbarHit { none, export_png, reset, settings };

    static constexpr int kSeriesCheckboxBase = 8000;

    // CP40: the host's chosen theme. Persists across create_main calls and
    // is the source of truth for apply_theme() — do NOT derive it from
    // primary_chart_.settings(), which only exists after create_charts().
    nfui::ChartSettings::ThemeMode theme_mode_{
        nfui::ChartSettings::ThemeMode::dark};

    std::unique_ptr<SettingsDialog> settings_dialog_{};

    void apply_theme() noexcept {
        // CP40: read the host's chosen theme from our own member, NOT from
        // primary_chart_.settings(). apply_theme() runs before the chart
        // exists on first launch, so reading the chart would always yield
        // the default ChartSettings{} (theme=light) and the dashboard
        // would boot into a white surface. The Settings dialog writes
        // theme_mode_ directly when the user toggles dark/light.
        const nfui::ThemeMode mode = (theme_mode_ ==
                                      nfui::ChartSettings::ThemeMode::dark)
                                      ? nfui::ThemeMode::dark
                                      : nfui::ThemeMode::light;
        palette_ = nfui::theme_palette(mode);
    }

    [[nodiscard]] ToolbarHit hit_test_toolbar(POINT pt) const noexcept {
        RECT rc{};
        GetClientRect(hwnd(), &rc);
        const int button_size = 22;
        const int y = (kTitleStripHeight - button_size) / 2;
        const int settings_x = rc.right - kOuterPadding - button_size;
        const int reset_x = settings_x - 8 - button_size;
        const int export_x = reset_x - 8 - button_size;
        const RECT export_btn{export_x, y, export_x + button_size, y + button_size};
        const RECT reset_btn{reset_x, y, reset_x + button_size, y + button_size};
        const RECT settings_btn{settings_x, y, settings_x + button_size, y + button_size};
        if (PtInRect(&export_btn, pt)) return ToolbarHit::export_png;
        if (PtInRect(&reset_btn, pt)) return ToolbarHit::reset;
        if (PtInRect(&settings_btn, pt)) return ToolbarHit::settings;
        return ToolbarHit::none;
    }

    [[nodiscard]] RECT toolbar_button_rect(ToolbarHit hit) const noexcept {
        RECT rc{};
        GetClientRect(hwnd(), &rc);
        const int button_size = 22;
        const int y = (kTitleStripHeight - button_size) / 2;
        const int settings_x = rc.right - kOuterPadding - button_size;
        const int reset_x = settings_x - 8 - button_size;
        const int export_x = reset_x - 8 - button_size;
        switch (hit) {
        case ToolbarHit::export_png:
            return RECT{export_x, y, export_x + button_size, y + button_size};
        case ToolbarHit::reset:
            return RECT{reset_x, y, reset_x + button_size, y + button_size};
        case ToolbarHit::settings:
            return RECT{settings_x, y, settings_x + button_size, y + button_size};
        case ToolbarHit::none:
        default:
            return RECT{};
        }
    }

    void export_primary_to_png() noexcept {
        wchar_t file_name[MAX_PATH] = L"chart.png";
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd();
        ofn.lpstrFile = file_name;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"PNG Image (*.png)\0*.png\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetSaveFileNameW(&ofn)) return;
        const std::wstring path(file_name);
        const bool ok = primary_chart_.export_to_png(path);
        wchar_t buf[200]{};
        std::swprintf(buf, std::size(buf),
                      L"%s  \x2014  %s",
                      ok ? L"Exported" : L"Export FAILED",
                      path.c_str());
        SetWindowTextW(hwnd(), buf);
    }

    void reset_views() noexcept {
        primary_chart_.reset_view();
        comparison_chart_.reset_view();
        SetWindowTextW(hwnd(), L"View Reset \x2014 Interactive Charts \x2014 NativeFrame UI");
    }

    void open_settings_dialog() noexcept {
        settings_dialog_ = std::make_unique<SettingsDialog>();
        settings_dialog_->open(hwnd(), primary_chart_.settings(),
            [this](nfui::ChartSettings s) {
                theme_mode_ = s.theme;
                primary_chart_.apply_settings(s);
                comparison_chart_.apply_settings(s);
                apply_theme();
                primary_chart_.inject_theme(&palette_, &fonts_);
                comparison_chart_.inject_theme(&palette_, &fonts_);
                info_.set_theme(palette_);
                temperature_kpi_.inject_theme(&palette_, &fonts_);
                humidity_kpi_.inject_theme(&palette_, &fonts_);
                light_kpi_.inject_theme(&palette_, &fonts_);
                if (primary_chart_.hwnd()) InvalidateRect(primary_chart_.hwnd(), nullptr, FALSE);
                if (comparison_chart_.hwnd()) InvalidateRect(comparison_chart_.hwnd(), nullptr, FALSE);
                if (hwnd()) InvalidateRect(hwnd(), nullptr, FALSE);
            });
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

    void create_kpi_tiles() {
        const auto tile_color = [this](int idx) {
            return nfui::chart_series_color(
                primary_chart_.settings().theme == nfui::ChartSettings::ThemeMode::dark
                    ? nfui::ThemeMode::dark : nfui::ThemeMode::light, idx);
        };

        auto make_tile = [&](nfui::KpiTile& tile, std::wstring label,
                             std::wstring value, double delta,
                             std::vector<nfui::ChartPoint> points,
                             nfui::Color color) {
            nfui::WindowCreateParams cp{
                instance_, L"", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                0, 0, 0, 10, 10,
                hwnd(),
            };
            (void)tile.create(cp);
            tile.inject_theme(&palette_, &fonts_);
            tile.set_label(std::move(label));
            tile.set_value(std::move(value));
            tile.set_delta_percent(delta);
            tile.set_sparkline(std::move(points), color);
        };

        const double temp_first = temperature_.front().y;
        const double temp_last = temperature_.back().y;
        const double hum_first = humidity_.front().y;
        const double hum_last = humidity_.back().y;
        const double light_first = light_.front().y;
        const double light_last = light_.back().y;

        make_tile(temperature_kpi_,
                  L"Temperature",
                  std::to_wstring(static_cast<int>(std::lround(temp_last))) + L" \xB0" + L"C",
                  (temp_last - temp_first),
                  temperature_,
                  tile_color(0));
        make_tile(humidity_kpi_,
                  L"Humidity",
                  std::to_wstring(static_cast<int>(std::lround(hum_last))) + L" %",
                  (hum_last - hum_first),
                  humidity_,
                  tile_color(1));
        make_tile(light_kpi_,
                  L"Light",
                  std::to_wstring(static_cast<int>(std::lround(light_last))) + L" lx",
                  (light_last - light_first),
                  light_,
                  tile_color(2));
    }

    void create_charts() {
        nfui::WindowCreateParams primary_cp{
            instance_, L"", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 0, 100, 100,
            hwnd(),
        };
        if (!primary_chart_.create(primary_cp)) return;
        // CP40: ChartView::create() nulls palette_/fonts_, so inject_theme
        // must run AFTER create() to take effect. Without this the chart
        // falls back to settings_.theme, which is light by default and
        // would render the comparison pane as a white surface.
        primary_chart_.inject_theme(&palette_, &fonts_);
        primary_chart_.set_kind(nfui::ChartKind::line);
        // CP40 demo polish: default to dark theme so the dashboard
        // opens with the originally-curated dark surface. The Settings
        // dialog toggles between dark/light at runtime.
        {
            nfui::ChartSettings init_s = primary_chart_.settings();
            init_s.theme = nfui::ChartSettings::ThemeMode::dark;
            init_s.kind_id = 2;  // line
            primary_chart_.apply_settings(init_s);
        }

        std::vector<nfui::ChartSeries> series;
        auto make_series = [&](std::wstring_view name, int color_idx,
                               std::vector<nfui::ChartPoint> pts) {
            nfui::ChartSeries s;
            s.name = name;
            s.color = nfui::chart_series_color(nfui::ThemeMode::dark, color_idx);
            s.points = std::move(pts);
            series.push_back(std::move(s));
        };
        make_series(L"Temperature", 0, temperature_);
        make_series(L"Humidity",    1, humidity_);
        make_series(L"Light",       2, light_);

        primary_chart_.set_series(std::move(series));
        primary_chart_.set_axis_x(nfui::ChartAxisRange{0.0, 29.0, L"{:.0f}"});
        primary_chart_.set_axis_y(nfui::ChartAxisRange{0.0, 100.0, L"{:.0f}"});

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
        primary_chart_.enable_interaction(opts);

        // Comparison chart: temperature series on its own y range, no
        // interaction (this keeps it from competing with the primary for
        // input events). The ChartGroup is what wires it to the primary.
        nfui::WindowCreateParams comparison_cp{
            instance_, L"", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 0, 100, 100,
            hwnd(),
        };
        if (!comparison_chart_.create(comparison_cp)) return;
        // Same rule as primary: bind theme AFTER create().
        comparison_chart_.inject_theme(&palette_, &fonts_);
        comparison_chart_.set_kind(nfui::ChartKind::spline);
        // Mirror the primary's theme so refresh_fallback_palette() also
        // caches a dark fallback — if the host later clears palette_ the
        // comparison chart still paints dark, matching the dashboard.
        {
            nfui::ChartSettings cmp_init = comparison_chart_.settings();
            cmp_init.theme = nfui::ChartSettings::ThemeMode::dark;
            comparison_chart_.apply_settings(cmp_init);
        }
        std::vector<nfui::ChartSeries> cmp_series;
        nfui::ChartSeries cmp;
        cmp.name = L"Temperature (zoomed)";
        cmp.color = nfui::chart_series_color(nfui::ThemeMode::dark, 0);
        cmp.points = temperature_;
        cmp_series.push_back(std::move(cmp));
        comparison_chart_.set_series(std::move(cmp_series));
        comparison_chart_.set_axis_x(nfui::ChartAxisRange{0.0, 29.0, L"{:.0f}"});
        comparison_chart_.set_axis_y(nfui::ChartAxisRange{35.0, 85.0, L"{:.1f}"});

        // Axis titles + theme plumbing.
        nfui::ChartSettings ps = primary_chart_.settings();
        ps.x_axis_label = L"Time";
        ps.y_axis_label = L"Value";
        primary_chart_.apply_settings(ps);

        nfui::ChartSettings cs = comparison_chart_.settings();
        cs.x_axis_label = L"Time";
        cs.y_axis_label = L"Percent";
        comparison_chart_.apply_settings(cs);

        // Wire the group. Primary's x range will propagate; cursor moves
        // on the primary will paint an external crosshair on the
        // comparison and vice versa.
        (void)chart_group_.add_chart(&primary_chart_, nfui::ChartGroupRole::primary);
        (void)chart_group_.add_chart(&comparison_chart_, nfui::ChartGroupRole::sub);
        chart_group_.link_x_axis(true);
        chart_group_.sync_cursor(true);

        // Push the series overview into the info panel up front.
        info_.set_series_overview(build_overview(get_chart_series()));

        // Wire callbacks. The same view/cursor/select callbacks are
        // installed on both charts; the cursor callback is what the
        // ChartGroup also fires so the group's broadcast shows up in
        // the status strip too.
        nfui::ChartCallbacks cbs{};
        cbs.on_point_edited = [this](std::size_t si, std::size_t pi, nfui::ChartPoint val) {
            wchar_t buf[160];
            std::swprintf(buf, std::size(buf),
                          L"Edited [%zu][%zu] -> (%.1f, %.2f)  |  Undo: %s  Redo: %s",
                          si, pi, val.x, val.y,
                          primary_chart_.can_undo() ? L"Yes" : L"No",
                          primary_chart_.can_redo() ? L"Yes" : L"No");
            SetWindowTextW(hwnd(), buf);
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
        cbs.on_cursor_x_changed = [this](std::optional<double> x) {
            if (!x.has_value()) return;  // quiet when the cursor leaves
            wchar_t buf[160];
            std::swprintf(buf, std::size(buf),
                          L"Cursor x = %.2f  \x2014  Interactive Charts", *x);
            SetWindowTextW(hwnd(), buf);
        };
        cbs.on_view_reset = [this]() {
            SetWindowTextW(hwnd(), L"View Reset \x2014 Interactive Charts \x2014 NativeFrame UI");
        };
        primary_chart_.set_callbacks(std::move(cbs));
    }

    void create_info_panel() {
        RECT placeholder{0, 0, 100, 100};
        info_.create(hwnd(), placeholder, palette_);
        info_.set_theme(palette_);
    }

    std::vector<nfui::ChartSeries> get_chart_series() const {
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

    // Compute the dashboard body geometry. Returns the rectangle for the
    // KPI tile row, the primary chart, the comparison chart, and the
    // info panel. Caller is responsible for any offsets below the body
    // (e.g. status strip).
    struct DashboardGeometry {
        RECT kpi{};
        RECT primary{};
        RECT comparison{};
        RECT info{};
        int body_height{};
    };

    DashboardGeometry compute_geometry() const noexcept {
        RECT rc{};
        GetClientRect(hwnd(), &rc);
        const int body_top = kTitleStripHeight;
        const int body_bottom = rc.bottom - kStatusStripHeight;
        const int body_height = body_bottom - body_top;
        const int info_left = rc.right - kOuterPadding - kInfoPanelWidth;
        const int body_left = kOuterPadding;
        const int body_right = info_left - kOuterPadding;
        const int kpi_h = 132;
        const int gap = kOuterPadding;
        const int remaining = body_height - kpi_h - gap * 3;
        const int primary_h = (remaining * 55) / 100;
        const int comparison_h = remaining - primary_h - gap;

        DashboardGeometry g{};
        g.body_height = body_height;
        g.kpi = RECT{body_left,
                     body_top + gap,
                     body_right,
                     body_top + gap + kpi_h};
        const int primary_top = g.kpi.bottom + gap;
        g.primary = RECT{body_left,
                         primary_top,
                         body_right,
                         primary_top + primary_h};
        const int comparison_top = g.primary.bottom + gap;
        g.comparison = RECT{body_left,
                            comparison_top,
                            body_right,
                            comparison_top + comparison_h};
        g.info = RECT{info_left,
                      body_top,
                      rc.right - kOuterPadding,
                      body_bottom};
        return g;
    }

    void layout_children() noexcept {
        const DashboardGeometry g = compute_geometry();

        if (primary_chart_.hwnd() != nullptr) {
            MoveWindow(primary_chart_.hwnd(),
                       g.primary.left, g.primary.top,
                       g.primary.right - g.primary.left,
                       g.primary.bottom - g.primary.top,
                       TRUE);
        }
        if (comparison_chart_.hwnd() != nullptr) {
            MoveWindow(comparison_chart_.hwnd(),
                       g.comparison.left, g.comparison.top,
                       g.comparison.right - g.comparison.left,
                       g.comparison.bottom - g.comparison.top,
                       TRUE);
        }
        if (info_.hwnd() != nullptr) {
            MoveWindow(info_.hwnd(),
                       g.info.left, g.info.top,
                       g.info.right - g.info.left,
                       g.info.bottom - g.info.top,
                       TRUE);
            InvalidateRect(info_.hwnd(), nullptr, FALSE);
        }

        layout_kpi_tiles(g.kpi);
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void layout_kpi_tiles(RECT row) noexcept {
        const int gap = 12;
        const int total_gap = gap * 2;
        const int tile_w = ((row.right - row.left) - total_gap) / 3;
        if (tile_w <= 0) return;
        const int y = row.top;
        const int h = row.bottom - row.top;

        if (temperature_kpi_.hwnd() != nullptr) {
            MoveWindow(temperature_kpi_.hwnd(),
                       row.left, y, tile_w, h, TRUE);
        }
        if (humidity_kpi_.hwnd() != nullptr) {
            MoveWindow(humidity_kpi_.hwnd(),
                       row.left + tile_w + gap, y, tile_w, h, TRUE);
        }
        if (light_kpi_.hwnd() != nullptr) {
            MoveWindow(light_kpi_.hwnd(),
                       row.left + (tile_w + gap) * 2, y, tile_w, h, TRUE);
        }
    }

    void paint_strips(HDC hdc, RECT bounds) noexcept {
        const int w = bounds.right - bounds.left;
        const int h = bounds.bottom - bounds.top;

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

        RECT title_text{kOuterPadding, 0, w / 2, kTitleStripHeight};
        draw_text(hdc, title_text,
                  L"Interactive Charts \x2014 NativeFrame UI",
                  title_font, palette_.text,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        draw_toolbar_buttons(hdc);

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
                  L"Drag to range-select  \x2022  Wheel zoom  \x2022  Space+drag = pan  \x2022  Dbl-click = reset  \x2022  Ctrl+Z/Y = undo/redo  \x2022  Cursor x sync across linked charts",
                  hint_font, palette_.text_secondary,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }

    void draw_toolbar_buttons(HDC hdc) noexcept {
        const int button_size = 22;
        (void)button_size;

        auto draw_round_button = [&](RECT btn) {
            const POINT probe{btn.left + 2, btn.top + 2};
            const ToolbarHit probe_hit = hit_test_toolbar(probe);
            const nfui::Color bg = (probe_hit != ToolbarHit::none)
                ? nfui::Color{palette_.surface_hover.rgb}
                : nfui::Color{palette_.surface.rgb};
            fill_rounded_rect(hdc, btn, 4, bg, palette_.border);
        };

        const RECT export_btn = toolbar_button_rect(ToolbarHit::export_png);
        const RECT reset_btn = toolbar_button_rect(ToolbarHit::reset);
        const RECT settings_btn = toolbar_button_rect(ToolbarHit::settings);
        draw_round_button(export_btn);
        draw_round_button(reset_btn);
        draw_round_button(settings_btn);

        // Export glyph: downward arrow into a tray.
        {
            const int cx = (export_btn.left + export_btn.right) / 2;
            const int cy = (export_btn.top + export_btn.bottom) / 2;
            HPEN pen = CreatePen(PS_SOLID, 2, palette_.text.rgb);
            HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
            HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
            // Vertical stem.
            MoveToEx(hdc, cx, cy - 5, nullptr);
            LineTo(hdc, cx, cy + 3);
            // Tray.
            MoveToEx(hdc, cx - 5, cy + 3, nullptr);
            LineTo(hdc, cx + 5, cy + 3);
            // Arrowhead.
            MoveToEx(hdc, cx - 3, cy + 1, nullptr);
            LineTo(hdc, cx,     cy + 4);
            LineTo(hdc, cx + 3, cy + 1);
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(pen);
        }

        // Reset glyph: circular arrow.
        {
            const int cx = (reset_btn.left + reset_btn.right) / 2;
            const int cy = (reset_btn.top + reset_btn.bottom) / 2;
            const int r = 7;
            HPEN pen = CreatePen(PS_SOLID, 2, palette_.text.rgb);
            HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));
            HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
            Arc(hdc, cx - r, cy - r, cx + r, cy + r,
                cx + r, cy - r,
                cx,     cy + r);
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(pen);
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

        // Settings glyph: gear.
        {
            const int cx = (settings_btn.left + settings_btn.right) / 2;
            const int cy = (settings_btn.top + settings_btn.bottom) / 2;
            HBRUSH brush = CreateSolidBrush(palette_.text.rgb);
            HBRUSH old = static_cast<HBRUSH>(SelectObject(hdc, brush));
            HPEN oldp = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            Ellipse(hdc, cx - 2, cy - 2, cx + 2, cy + 2);
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

    // CP40: KPI tiles (row 1) -> primary chart (row 2) -> comparison
    // chart (row 3) -> group. Reverse destruction drops the group
    // first so its observer hooks detach from the still-living charts.
    nfui::KpiTile temperature_kpi_{};
    nfui::KpiTile humidity_kpi_{};
    nfui::KpiTile light_kpi_{};
    nfui::LineChartView primary_chart_{};
    nfui::LineChartView comparison_chart_{};
    nfui::ChartGroup chart_group_{};

    InfoPanel info_{};

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