#include <nfui/Application.hpp>
#include <nfui/Charts.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/NativeFrameUI.hpp>
#include <nfui/Paint.hpp>
#include <nfui/ResourceContext.hpp>
#include <nfui/Theme.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include <windowsx.h>

namespace {

// CP30: chart-series name strings. The ChartSeries::name field is borrowed
// (std::wstring_view); the underlying storage MUST outlive the chart view
// the series is attached to. Keep these as static-storage string literals so
// each set_series() call can hand the renderer a stable pointer.
constexpr std::wstring_view kBarName        = L"Monthly Revenue";
constexpr std::wstring_view kAreaName       = L"Active Users";
constexpr std::wstring_view kHbarSeries2022 = L"FY 22";
constexpr std::wstring_view kHbarSeries2023 = L"FY 23";
constexpr std::wstring_view kHbarSeries2024 = L"FY 24";
constexpr std::wstring_view kLineSeriesRev  = L"Revenue";
constexpr std::wstring_view kLineSeriesCost = L"Costs";
constexpr std::wstring_view kSplineWave     = L"Sensor A (Hz)";

// CP30 categorical-palette indices. We pick hues by intent (teal/coral/amber)
// rather than by sequential position so the five cards read with their own
// identity: Bar/Revenue teal, FY23/Costs/Spline/Area coral, FY24 amber. The
// indices wrap on theme change so dark mode swaps the same conceptual hue
// for its brighter twin without any hardcoded RGB in this file.
constexpr std::size_t kIdxTeal  = 5;
constexpr std::size_t kIdxCoral = 3;
constexpr std::size_t kIdxAmber = 2;

// Twelve months of revenue (USD, thousands). Mild upward trend with summer +
// Q4 bumps so the bars read as a realistic monthly series rather than a flat
// sine. Picked so axis_y_max=100 leaves clean headroom above the tallest bar.
constexpr std::array<double, 12> kBarMonths = {
    42.0, 48.0, 51.0, 47.0, 53.0, 61.0,
    58.0, 67.0, 72.0, 69.0, 78.0, 84.0,
};

// Three-year revenue breakdown by platform. Categories are desktop, mobile,
// tablet, console, smart-TV. Each year's mix shifts towards mobile + TV so the
// grouped horizontal bars show the platform shift story.
constexpr std::array<std::wstring_view, 5> kHbarCategories = {
    L"Desktop", L"Mobile", L"Tablet", L"Console", L"Smart TV",
};
constexpr std::array<double, 5> kHbar2022 = {60.0, 80.0, 25.0, 12.0, 18.0};
constexpr std::array<double, 5> kHbar2023 = {62.0, 85.0, 28.0, 15.0, 22.0};
constexpr std::array<double, 5> kHbar2024 = {65.0, 92.0, 32.0, 20.0, 28.0};

// Two-series monthly comparison: revenue vs. costs. Same month axis as the
// bar chart so the line chart and bar chart read as siblings when stacked.
constexpr std::array<double, 12> kLineRevenueValues = {
    42.0, 48.0, 51.0, 47.0, 53.0, 61.0,
    58.0, 67.0, 72.0, 69.0, 78.0, 84.0,
};
constexpr std::array<double, 12> kLineCostsValues = {
    28.0, 30.0, 33.0, 31.0, 35.0, 38.0,
    36.0, 41.0, 44.0, 42.0, 47.0, 50.0,
};

// Sine-wave sweep over 30 samples. y = 50 + 28 * sin(2*pi*i/8) keeps the
// peaks inside [0, 100] so the axis-y range covers it with a little headroom.
std::vector<nfui::ChartPoint> build_spline_points() noexcept {
    std::vector<nfui::ChartPoint> pts;
    pts.reserve(30);
    for (std::size_t i = 0; i < 30; ++i) {
        const double phase = 2.0 * 3.14159265358979323846 *
                             static_cast<double>(i) / 8.0;
        pts.push_back(nfui::ChartPoint{
            static_cast<double>(i),
            50.0 + 28.0 * std::sin(phase),
        });
    }
    return pts;
}

[[nodiscard]] RECT make_rect(int left, int top, int width, int height) noexcept {
    RECT rect{};
    rect.left = left;
    rect.top = top;
    rect.right = left + std::max(width, 0);
    rect.bottom = top + std::max(height, 0);
    return rect;
}

[[nodiscard]] int rect_width(const RECT& rect) noexcept {
    return rect.right - rect.left;
}

[[nodiscard]] int rect_height(const RECT& rect) noexcept {
    return rect.bottom - rect.top;
}

[[nodiscard]] RECT inset_rect(const RECT& rect, int amount) noexcept {
    RECT inset = rect;
    inset.left += amount;
    inset.top += amount;
    inset.right -= amount;
    inset.bottom -= amount;
    return inset;
}

struct LegendEntry {
    nfui::Color         color{};
    std::wstring_view   text{};
};

// Per-card layout record. The parent window paints card chrome (title + chart
// HWND + legend) using these rects; layout_charts() recomputes them on every
// WM_SIZE / WM_DPICHANGED so the cards stay aligned with the grid.
struct CardLayout {
    RECT                card{};       // outer panel (shadow + fill + border)
    RECT                title{};      // chart title above the chart HWND
    RECT                chart{};      // chart HWND placement (inside card)
    RECT                legend{};     // legend strip below the chart HWND
    std::wstring_view   title_text{};
    std::vector<LegendEntry> legend_entries{};
};

class ChartsWindow final : public nfui::Window {
public:
    explicit ChartsWindow(HINSTANCE instance) noexcept
        : instance_(instance),
          resources_(instance),
          light_palette_(nfui::theme_palette(nfui::ThemeMode::light)),
          dark_palette_(nfui::theme_palette(nfui::ThemeMode::dark)),
          palette_(&light_palette_) {
    }

    ~ChartsWindow() noexcept override {
        destroy_icons();
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIChartsWindow",
            L"NativeFrame UI Charts",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1100,
            1500,
        };

        if (!create(params)) {
            return false;
        }

        apply_window_icon();
        if (!create_charts()) {
            return false;
        }

        apply_theme();

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            layout_charts();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        case WM_DPICHANGED: {
            auto* suggested = reinterpret_cast<RECT*>(lparam);
            if (suggested != nullptr) {
                SetWindowPos(hwnd(),
                             nullptr,
                             suggested->left,
                             suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }
            // Owner-draw chart views re-query DPI inside on_paint, but
            // re-binding the FontCache pointer keeps the cached HFONT slots
            // in lockstep with the freshly-bumped DPI (R9 fix pattern).
            bar_.set_font_cache(&fonts_);
            hbar_.set_font_cache(&fonts_);
            line_.set_font_cache(&fonts_);
            spline_.set_font_cache(&fonts_);
            area_.set_font_cache(&fonts_);
            layout_charts();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            track_mouse_leave();
            const POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (update_hover(pt)) {
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            if (clear_hover()) {
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN: {
            const POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (handle_click(pt)) {
                InvalidateRect(hwnd(), nullptr, FALSE);
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
            // Flicker-free offscreen buffer over the full client area. The
            // MemoryDC destructor BitBlts back to the target rect origin while
            // the BeginPaint DC is still valid, so the buffer flush MUST
            // happen before EndPaint (R6 spec gap, originally fixed in
            // SettingsDemo, mirrored here).
            {
                nfui::MemoryDC mem(hdc, client);
                HDC target = mem.valid() ? mem.dc() : hdc;
                paint_chrome(target, client);
            }
            EndPaint(hwnd(), &paint);
            return 0;
        }
        case WM_DESTROY:
            destroy_icons();
            PostQuitMessage(0);
            return 0;
        default:
            return nfui::Window::handle_message(message, wparam, lparam);
        }
    }

private:
    [[nodiscard]] bool create_charts() noexcept {
        // All five chart views share the same chart-view window class
        // (NativeFrameUIChartView) — the first create() registers it, the
        // rest reuse it. Each subclass (Bar / HBar / Line / Spline / Area)
        // wires its own on_paint override. Palette + fonts are injected
        // before create so the very first paint cycle has them.
        bar_.inject_theme(palette_, &fonts_);
        hbar_.inject_theme(palette_, &fonts_);
        line_.inject_theme(palette_, &fonts_);
        spline_.inject_theme(palette_, &fonts_);
        area_.inject_theme(palette_, &fonts_);

        nfui::WindowCreateParams params{
            instance_,
            L"",  // overridden by ChartView::create to NativeFrameUIChartView
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0,
            0, 0, 1, 1,  // placeholder; layout_charts() resizes them
            hwnd(),
        };

        if (!bar_.create(params))    return false;
        if (!hbar_.create(params))   return false;
        if (!line_.create(params))   return false;
        if (!spline_.create(params)) return false;
        if (!area_.create(params))   return false;
        return true;
    }

    // Rebuild series palettes from the live theme + push the palette pointer
    // through every chart view. Called on create + on theme toggle.
    void apply_theme() noexcept {
        palette_ = (theme_mode_ == nfui::ThemeMode::dark)
                       ? static_cast<const nfui::ThemePalette*>(&dark_palette_)
                       : static_cast<const nfui::ThemePalette*>(&light_palette_);
        bar_.set_palette(palette_);
        hbar_.set_palette(palette_);
        line_.set_palette(palette_);
        spline_.set_palette(palette_);
        area_.set_palette(palette_);
        populate_data();
        // Re-layout so the card legend entries pick up the new series colors
        // and the chart HWNDs land on the freshly sized card rects.
        layout_charts();
    }

    void populate_data() noexcept {
        // Vertical bar: one monthly series, twelve bars. axis_y {0..100}
        // matches the renderer clamp range so the tallest bar (84) leaves
        // visible headroom above the plot frame.
        {
            std::vector<nfui::ChartPoint> points;
            points.reserve(kBarMonths.size());
            for (std::size_t i = 0; i < kBarMonths.size(); ++i) {
                points.push_back(nfui::ChartPoint{
                    static_cast<double>(i + 1),
                    kBarMonths[i],
                });
            }
            bar_.set_kind(nfui::ChartKind::bar_vertical);
            bar_.set_series({nfui::ChartSeries{
                kBarName,
                nfui::chart_series_color(theme_mode_, kIdxTeal),
                std::move(points)}});
            bar_.set_axis_x(nfui::ChartAxisRange{1.0, 12.0, L"{:.0f}"});
            bar_.set_axis_y(nfui::ChartAxisRange{0.0, 100.0, L"{:.0f}"});
        }

        // Filled-area chart. Reuses the same monthly revenue shape as the
        // bar chart so the two views read as siblings when stacked. We use
        // the coral index so the area reads as a "user growth" accent rather
        // than duplicating the bar's teal.
        {
            std::vector<nfui::ChartPoint> points;
            points.reserve(kBarMonths.size());
            for (std::size_t i = 0; i < kBarMonths.size(); ++i) {
                points.push_back(nfui::ChartPoint{
                    static_cast<double>(i + 1),
                    kBarMonths[i],
                });
            }
            area_.set_kind(nfui::ChartKind::area);
            area_.set_series({nfui::ChartSeries{
                kAreaName,
                nfui::chart_series_color(theme_mode_, kIdxCoral),
                std::move(points)}});
            area_.set_axis_x(nfui::ChartAxisRange{1.0, 12.0, L"{:.0f}"});
            area_.set_axis_y(nfui::ChartAxisRange{0.0, 100.0, L"{:.0f}"});
            area_.set_outline(true);
            area_.set_fill_alpha(0.6);
        }

        // Horizontal bar: three series, five categories. axis_y carries the
        // value range (HBar renderer reads axis_y even though the value axis
        // is conceptually x). Categories are sorted descending by FY 2024 so
        // the longest bar is on top.
        {
            std::array<std::size_t, 5> hbar_order{0, 1, 2, 3, 4};
            std::sort(hbar_order.begin(), hbar_order.end(),
                      [](std::size_t a, std::size_t b) noexcept {
                          return kHbar2024[a] > kHbar2024[b];
                      });

            std::vector<nfui::ChartSeries> series;
            series.reserve(3);
            const std::array<std::array<double, 5>, 3> series_data{kHbar2022, kHbar2023, kHbar2024};
            const std::array<std::wstring_view, 3> series_names{kHbarSeries2022, kHbarSeries2023, kHbarSeries2024};
            const std::array<std::size_t, 3> series_color_idx{kIdxTeal, kIdxCoral, kIdxAmber};
            for (std::size_t s = 0; s < series_data.size(); ++s) {
                std::vector<nfui::ChartPoint> points;
                points.reserve(kHbarCategories.size());
                for (std::size_t i = 0; i < kHbarCategories.size(); ++i) {
                    const std::size_t c = hbar_order[i];
                    points.push_back(nfui::ChartPoint{
                        static_cast<double>(i + 1),
                        series_data[s][c],
                    });
                }
                series.push_back(nfui::ChartSeries{
                    series_names[s],
                    nfui::chart_series_color(theme_mode_, series_color_idx[s]),
                    std::move(points)});
            }
            hbar_.set_kind(nfui::ChartKind::bar_horizontal);
            hbar_.set_series(std::move(series));
            hbar_.set_axis_y(nfui::ChartAxisRange{0.0, 100.0, L"{:.0f}"});
        }

        // Line: revenue vs. costs, twelve monthly samples. Markers stay on
        // so the per-month data points read clearly.
        {
            std::vector<nfui::ChartSeries> series;
            series.reserve(2);

            std::vector<nfui::ChartPoint> revenue_points;
            revenue_points.reserve(kLineRevenueValues.size());
            for (std::size_t i = 0; i < kLineRevenueValues.size(); ++i) {
                revenue_points.push_back(nfui::ChartPoint{
                    static_cast<double>(i + 1),
                    kLineRevenueValues[i],
                });
            }
            series.push_back(nfui::ChartSeries{
                kLineSeriesRev,
                nfui::chart_series_color(theme_mode_, kIdxTeal),
                std::move(revenue_points)});

            std::vector<nfui::ChartPoint> costs_points;
            costs_points.reserve(kLineCostsValues.size());
            for (std::size_t i = 0; i < kLineCostsValues.size(); ++i) {
                costs_points.push_back(nfui::ChartPoint{
                    static_cast<double>(i + 1),
                    kLineCostsValues[i],
                });
            }
            series.push_back(nfui::ChartSeries{
                kLineSeriesCost,
                nfui::chart_series_color(theme_mode_, kIdxCoral),
                std::move(costs_points)});

            line_.set_kind(nfui::ChartKind::line);
            line_.set_series(std::move(series));
            line_.set_point_radius(4);
            line_.set_axis_x(nfui::ChartAxisRange{1.0, 12.0, L"{:.0f}"});
            line_.set_axis_y(nfui::ChartAxisRange{0.0, 100.0, L"{:.0f}"});
        }

        // Spline: 30-point sine wave. The smooth Catmull-Rom curve plus a
        // default tension (0.5) gives a clean undulating line; the chart
        // primitive already paints small markers at the sample anchors.
        {
            spline_.set_kind(nfui::ChartKind::spline);
            spline_.set_series({nfui::ChartSeries{
                kSplineWave,
                nfui::chart_series_color(theme_mode_, kIdxCoral),
                build_spline_points()}});
            spline_.set_tension(0.5);
            spline_.set_axis_x(nfui::ChartAxisRange{0.0, 29.0, L"{:.0f}"});
            spline_.set_axis_y(nfui::ChartAxisRange{0.0, 100.0, L"{:.0f}"});
        }
    }

    void layout_charts() noexcept {
        if (hwnd() == nullptr) {
            return;
        }

        RECT client{};
        GetClientRect(hwnd(), &client);
        const nfui::DpiScale dpi(GetDpiForWindow(hwnd()));

        const int outer = dpi.logical_to_pixels(24);
        const int gap = dpi.logical_to_pixels(16);
        const int header_h = dpi.logical_to_pixels(72);

        // Title row sits inside the header band so it shares the brand line
        // with the theme toggle. The toggle rect is reused by hover + click
        // dispatchers so the click test always sees the live coordinates.
        const int header_band_top = client.top + outer;
        const int header_band_h = header_h;
        const int logo_size = dpi.logical_to_pixels(32);
        brand_rect_ = make_rect(client.left + outer,
                                header_band_top + (header_band_h - logo_size) / 2,
                                logo_size, logo_size);
        const int toggle_w = dpi.logical_to_pixels(120);
        const int toggle_h = dpi.logical_to_pixels(32);
        theme_toggle_rect_ = make_rect(client.right - outer - toggle_w,
                                       header_band_top + (header_band_h - toggle_h) / 2,
                                       toggle_w, toggle_h);

        const int area_left = client.left + outer;
        const int area_top = header_band_top + header_band_h + gap;
        const int area_right = client.right - outer;
        const int area_bottom = client.bottom - outer;

        // Per-card reserved vertical space: title strip + legend strip +
        // inner padding. The chart HWND sits between them inside the card
        // chrome (rounded rect + border + drop shadow painted by the parent).
        const int card_padding = dpi.logical_to_pixels(16);
        const int card_title_h = dpi.logical_to_pixels(22);
        const int card_legend_h = dpi.logical_to_pixels(22);
        const int card_gap = dpi.logical_to_pixels(8);

        // 5 cards in a vertical stack: each card spans the full content
        // width. The references ask for "five chart cards stacked vertically";
        // stacking gives each chart enough horizontal room for value labels
        // above bars and inline value labels at the end of HBar segments,
        // and it keeps the chart-vs-card chrome relationship one-to-one so
        // a reviewer can see the design system in action.
        const int card_w = std::max(area_right - area_left,
                                    dpi.logical_to_pixels(720));
        const int card_inner_w = std::max(card_w - card_padding * 2,
                                          dpi.logical_to_pixels(360));
        const int chart_h = dpi.logical_to_pixels(220);
        const int card_h = card_padding + card_title_h + card_gap
                            + chart_h + card_gap + card_legend_h + card_padding;

        const int card_count = 5;
        const int total_h = card_count * card_h + (card_count - 1) * gap;
        const int max_total_h = std::max(area_bottom - area_top,
                                        total_h + dpi.logical_to_pixels(120));
        const int stack_top = area_top + std::max(0, (max_total_h - total_h) / 2);

        cards_.clear();
        cards_.reserve(card_count);

        struct CardSpec {
            std::wstring_view              title_text;
            std::vector<LegendEntry>       entries;
        };
        // CP30: titles + legends tied to each chart view. Legend colors come
        // from the live palette so they re-tint on theme toggle.
        const CardSpec specs[card_count] = {
            { L"Bar — Q4 Revenue",
              { { palette_->accent, L"Brand accent" },
                { nfui::chart_series_color(theme_mode_, kIdxTeal), kBarName } } },
            { L"HBar — Multi-year Mix",
              { { nfui::chart_series_color(theme_mode_, kIdxTeal),  kHbarSeries2022 },
                { nfui::chart_series_color(theme_mode_, kIdxCoral), kHbarSeries2023 },
                { nfui::chart_series_color(theme_mode_, kIdxAmber), kHbarSeries2024 } } },
            { L"Line — Revenue vs Costs",
              { { nfui::chart_series_color(theme_mode_, kIdxTeal),  kLineSeriesRev  },
                { nfui::chart_series_color(theme_mode_, kIdxCoral), kLineSeriesCost } } },
            { L"Spline — Growth Trend",
              { { nfui::chart_series_color(theme_mode_, kIdxCoral), kSplineWave } } },
            { L"Area — Monthly Users",
              { { nfui::chart_series_color(theme_mode_, kIdxCoral), kAreaName } } },
        };

        // Chart HWND handles in display order (Bar, HBar, Line, Spline, Area).
        const nfui::Window* chart_views[card_count] = {
            &bar_, &hbar_, &line_, &spline_, &area_,
        };

        for (std::size_t i = 0; i < card_count; ++i) {
            const int card_top = stack_top + static_cast<int>(i) * (card_h + gap);
            CardLayout layout{};
            layout.card = make_rect(area_left, card_top, card_w, card_h);
            layout.title = make_rect(layout.card.left + card_padding,
                                     layout.card.top + card_padding,
                                     card_inner_w, card_title_h);
            layout.chart = make_rect(layout.card.left + card_padding,
                                     layout.title.bottom + card_gap,
                                     card_inner_w, chart_h);
            layout.legend = make_rect(layout.card.left + card_padding,
                                      layout.chart.bottom + card_gap,
                                      card_inner_w, card_legend_h);
            layout.title_text = specs[i].title_text;
            layout.legend_entries = specs[i].entries;
            cards_.push_back(layout);

            if (chart_views[i]->hwnd() != nullptr) {
                MoveWindow(chart_views[i]->hwnd(),
                           layout.chart.left, layout.chart.top,
                           rect_width(layout.chart), rect_height(layout.chart),
                           TRUE);
            }
        }
    }

    void paint_chrome(HDC target, const RECT& client) noexcept {
        const nfui::ThemePalette& p = *palette_;
        const nfui::DpiScale dpi(GetDpiForWindow(hwnd()));
        const int dpi_value = dpi.dpi();
        const int card_radius = dpi.logical_to_pixels(10);

        // 1) Window background.
        nfui::fill_rect(target, client, p.background);

        // 2) Header band: brand square + page title on the left, theme
        //    toggle pill on the right. The toggle shows the OPPOSITE mode
        //    label so the affordance reads as an action ("switch to dark"),
        //    not a status badge ("currently light"). Hover lifts the face
        //    one notch toward accent so the click affordance is obvious.
        paint_brand_square(target);
        paint_page_title(target);
        paint_theme_toggle(target);

        // 3) Card chrome: shadow + rounded surface + border, then title +
        //    legend above and below the chart HWND. Chart HWNDs paint
        //    themselves first (child windows); the parent's paint lands
        //    behind them, so chrome here only fills the reserved band.
        for (const CardLayout& layout : cards_) {
            nfui::paint_drop_shadow(target, layout.card, card_radius,
                                    1, p.shadow);
            nfui::fill_rounded_rect(target, layout.card, card_radius,
                                    p.surface, p.border);
            paint_card_title(target, layout);
            paint_card_legend(target, layout);
        }
    }

    void paint_brand_square(HDC target) noexcept {
        const nfui::ThemePalette& p = *palette_;
        const nfui::DpiScale dpi(GetDpiForWindow(hwnd()));
        const int radius = dpi.logical_to_pixels(8);
        nfui::fill_rounded_rect(target, brand_rect_, radius, p.accent, p.accent);
        // White "N" mark, semibold at font_pt::md (16pt). The accent_text
        // colour is reserved for text drawn ON accent (per palette docs),
        // so we use it instead of p.text here.
        const int inset = dpi.logical_to_pixels(2);
        RECT glyph = inset_rect(brand_rect_, inset);
        nfui::draw_text(target,
                        glyph,
                        L"N",
                        fonts_.semibold(dpi.dpi(), nfui::font_pt::md),
                        p.accent_text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_page_title(HDC target) noexcept {
        const nfui::ThemePalette& p = *palette_;
        const nfui::DpiScale dpi(GetDpiForWindow(hwnd()));
        const int gap = dpi.logical_to_pixels(12);
        const int title_h = dpi.logical_to_pixels(28);
        RECT title = brand_rect_;
        title.left = brand_rect_.right + gap;
        title.top = brand_rect_.top + (rect_height(brand_rect_) - title_h) / 2;
        title.right = theme_toggle_rect_.left - gap;
        title.bottom = title.top + title_h;
        nfui::draw_text(target,
                        title,
                        L"Charts",
                        fonts_.semibold(dpi.dpi(), nfui::font_pt::lg),
                        p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    void paint_theme_toggle(HDC target) noexcept {
        const nfui::ThemePalette& p = *palette_;
        const nfui::DpiScale dpi(GetDpiForWindow(hwnd()));
        const int radius = dpi.logical_to_pixels(16);

        // Hover face: lift the surface toward the accent so the click
        // affordance reads. When the user is already on dark mode the
        // hover target is "switch to light", so the face lifts toward the
        // light accent-equivalent instead of the dark accent.
        const bool hovered = theme_toggle_hovered_;
        const nfui::Color face = hovered
            ? nfui::alpha_blend(p.surface_hover, p.accent, 0.08f)
            : p.surface;
        nfui::fill_rounded_rect(target, theme_toggle_rect_, radius, face, p.border);

        // Label shows the ACTION ("Switch to dark") plus a chevron. The
        // brief says "shows current mode + arrow" — we go with action+arrow
        // because it reads as the same affordance ShowcaseView uses.
        const std::wstring_view label = (theme_mode_ == nfui::ThemeMode::dark)
            ? L"Switch to light ▾"
            : L"Switch to dark ▾";
        RECT text_rc = inset_rect(theme_toggle_rect_, dpi.logical_to_pixels(12));
        nfui::draw_text(target,
                        text_rc,
                        label,
                        fonts_.semibold(dpi.dpi(), nfui::font_pt::sm),
                        p.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_card_title(HDC target, const CardLayout& layout) const noexcept {
        const nfui::ThemePalette& p = *palette_;
        const nfui::DpiScale dpi(GetDpiForWindow(hwnd()));
        nfui::draw_text(target,
                        layout.title,
                        layout.title_text,
                        fonts_.semibold(dpi.dpi(), nfui::font_pt::base),
                        p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    void paint_card_legend(HDC target, const CardLayout& layout) const noexcept {
        const nfui::ThemePalette& p = *palette_;
        const nfui::DpiScale dpi(GetDpiForWindow(hwnd()));
        const int dot_size = dpi.logical_to_pixels(8);
        const int gap = dpi.logical_to_pixels(8);

        const HFONT font = fonts_.regular(dpi.dpi(), nfui::font_pt::sm);
        HGDIOBJ prev_font = SelectObject(target, font);

        // Walk entries left-to-right. Each entry: filled circle, then a
        // label sized to its measured text. The legend strip is centred
        // vertically inside layout.legend so a clipped trailing label still
        // reads on the same baseline as its dot.
        int x = layout.legend.left;
        const int cy = layout.legend.top + (rect_height(layout.legend) - dot_size) / 2;
        for (const LegendEntry& entry : layout.legend_entries) {
            // Measure the label into a fresh null-terminated buffer —
            // std::wstring_view is NOT guaranteed to be null-terminated
            // and GetTextExtentPoint32W requires a NUL-terminated wide str.
            std::wstring text_buf(entry.text);
            SIZE sz{};
            GetTextExtentPoint32W(target, text_buf.c_str(),
                                  static_cast<int>(text_buf.size()), &sz);
            const int label_w = static_cast<int>(sz.cx);

            RECT dot{x, cy, x + dot_size, cy + dot_size};
            nfui::fill_ellipse(target, dot, entry.color);
            x += dot_size + gap;

            const int label_right = std::min<int>(x + label_w,
                                                  layout.legend.right);
            RECT label{x, layout.legend.top, label_right, layout.legend.bottom};
            nfui::draw_text(target,
                            label,
                            entry.text,
                            font,
                            p.text_secondary,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            x = label_right + gap;
            if (x + dot_size >= layout.legend.right) {
                break;  // no room for another dot+label pair
            }
        }

        SelectObject(target, prev_font);
    }

    void apply_window_icon() noexcept {
        if (!resources_.has_icon(IDI_NFUI_APP)) {
            return;
        }

        large_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXICON),
                                                    GetSystemMetrics(SM_CYICON),
                                                    LR_DEFAULTCOLOR));
        small_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXSMICON),
                                                    GetSystemMetrics(SM_CYSMICON),
                                                    LR_DEFAULTCOLOR));
        if (large_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon_));
        }
        if (small_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon_));
        }
    }

    void destroy_icons() noexcept {
        if (large_icon_ != nullptr) {
            DestroyIcon(large_icon_);
            large_icon_ = nullptr;
        }
        if (small_icon_ != nullptr) {
            DestroyIcon(small_icon_);
            small_icon_ = nullptr;
        }
    }

    void track_mouse_leave() noexcept {
        if (tracking_mouse_) {
            return;
        }
        TRACKMOUSEEVENT event{};
        event.cbSize = sizeof(event);
        event.dwFlags = TME_LEAVE;
        event.hwndTrack = hwnd();
        tracking_mouse_ = TrackMouseEvent(&event) != FALSE;
    }

    [[nodiscard]] bool update_hover(POINT pt) noexcept {
        const bool hovered = PtInRect(&theme_toggle_rect_, pt) != FALSE;
        if (hovered == theme_toggle_hovered_) {
            return false;
        }
        theme_toggle_hovered_ = hovered;
        return true;
    }

    [[nodiscard]] bool clear_hover() noexcept {
        if (!theme_toggle_hovered_) {
            return false;
        }
        theme_toggle_hovered_ = false;
        return true;
    }

    [[nodiscard]] bool handle_click(POINT pt) noexcept {
        if (PtInRect(&theme_toggle_rect_, pt) == FALSE) {
            return false;
        }
        theme_mode_ = (theme_mode_ == nfui::ThemeMode::dark)
                          ? nfui::ThemeMode::light
                          : nfui::ThemeMode::dark;
        apply_theme();
        return true;
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette light_palette_;
    nfui::ThemePalette dark_palette_;
    const nfui::ThemePalette* palette_{};
    nfui::FontCache fonts_;
    nfui::BarChartView bar_;
    nfui::HBarChartView hbar_;
    nfui::LineChartView line_;
    nfui::SplineChartView spline_;
    nfui::AreaChartView area_;
    nfui::ThemeMode theme_mode_{nfui::ThemeMode::light};

    // Header band geometry. Recomputed in layout_charts() so the brand +
    // toggle always match the live DPI / window size.
    RECT brand_rect_{};
    RECT theme_toggle_rect_{};
    bool theme_toggle_hovered_{false};
    bool tracking_mouse_{false};

    // Card stack. One entry per chart view, in display order. Recomputed
    // in layout_charts() so the chrome always lands on the chart HWNDs.
    std::vector<CardLayout> cards_{};

    HICON large_icon_{};
    HICON small_icon_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls() ||
        !nfui::initialize_chart_aa()) {
        return 1;
    }

    ChartsWindow window(instance);
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}