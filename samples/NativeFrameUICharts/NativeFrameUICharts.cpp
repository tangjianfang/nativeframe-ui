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
#include <string_view>
#include <vector>
#include <windowsx.h>

namespace {

// Chart-series name strings. The ChartSeries::name field is borrowed
// (std::wstring_view); the underlying storage MUST outlive the chart view
// the series is attached to. Keep these as static-storage string literals so
// each set_series() call can hand the renderer a stable pointer.
constexpr std::wstring_view kBarName         = L"Monthly revenue (USDk)";
constexpr std::wstring_view kAreaName        = L"Active users (k)";
constexpr std::wstring_view kHbarSeries2022  = L"FY 2022";
constexpr std::wstring_view kHbarSeries2023  = L"FY 2023";
constexpr std::wstring_view kHbarSeries2024  = L"FY 2024";
constexpr std::wstring_view kLineSeriesRev   = L"Revenue";
constexpr std::wstring_view kLineSeriesCost  = L"Costs";
constexpr std::wstring_view kSplineWave      = L"Sensor A (Hz)";

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

class ChartsWindow final : public nfui::Window {
public:
    explicit ChartsWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)) {
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
            1200,
            720,
        };

        if (!create(params)) {
            return false;
        }

        apply_window_icon();
        if (!create_charts()) {
            return false;
        }

        populate_data();
        layout_charts();

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
        bar_.inject_theme(&palette_, &fonts_);
        hbar_.inject_theme(&palette_, &fonts_);
        line_.inject_theme(&palette_, &fonts_);
        spline_.inject_theme(&palette_, &fonts_);
        area_.inject_theme(&palette_, &fonts_);

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
            bar_.set_series({nfui::ChartSeries{kBarName,
                                               nfui::chart_series_color(nfui::ThemeMode::light, 0),
                                               std::move(points)}});
            bar_.set_axis_x(nfui::ChartAxisRange{1.0, 12.0, L"{:.0f}"});
            bar_.set_axis_y(nfui::ChartAxisRange{0.0, 100.0, L"{:.0f}"});
        }

        // CP14: filled-area chart. Reuses the same monthly revenue values
        // as the bar chart so the two views read as siblings when shown
        // side-by-side. Outline on for a clean upper-edge stroke;
        // fill_alpha defaults to 1.0 (opaque).
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
            area_.set_series({nfui::ChartSeries{kAreaName,
                                                nfui::chart_series_color(nfui::ThemeMode::light, 1),
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
            const std::array<nfui::Color, 3> series_colors{
                nfui::chart_series_color(nfui::ThemeMode::light, 2),
                nfui::chart_series_color(nfui::ThemeMode::light, 3),
                nfui::chart_series_color(nfui::ThemeMode::light, 4),
            };
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
                series.push_back(nfui::ChartSeries{series_names[s], series_colors[s], std::move(points)});
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
            series.push_back(nfui::ChartSeries{kLineSeriesRev,
                                               nfui::chart_series_color(nfui::ThemeMode::light, 5),
                                               std::move(revenue_points)});

            std::vector<nfui::ChartPoint> costs_points;
            costs_points.reserve(kLineCostsValues.size());
            for (std::size_t i = 0; i < kLineCostsValues.size(); ++i) {
                costs_points.push_back(nfui::ChartPoint{
                    static_cast<double>(i + 1),
                    kLineCostsValues[i],
                });
            }
            series.push_back(nfui::ChartSeries{kLineSeriesCost,
                                               nfui::chart_series_color(nfui::ThemeMode::light, 6),
                                               std::move(costs_points)});

            line_.set_kind(nfui::ChartKind::line);
            line_.set_series(std::move(series));
            line_.set_point_radius(3);
            line_.set_axis_x(nfui::ChartAxisRange{1.0, 12.0, L"{:.0f}"});
            line_.set_axis_y(nfui::ChartAxisRange{0.0, 100.0, L"{:.0f}"});
        }

        // Spline: 30-point sine wave. The smooth Catmull-Rom curve plus a
        // default tension (0.5) gives a clean undulating line; small markers
        // identify the sample anchors without cluttering the curve (CP30).
        {
            spline_.set_kind(nfui::ChartKind::spline);
            spline_.set_series({nfui::ChartSeries{kSplineWave,
                                                  nfui::chart_series_color(nfui::ThemeMode::light, 7),
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

        const int outer = dpi.logical_to_pixels(20);
        const int gap = dpi.logical_to_pixels(16);
        const int header_h = dpi.logical_to_pixels(80);

        const int area_left = client.left + outer;
        const int area_top = client.top + outer + header_h + gap;
        const int area_right = client.right - outer;
        const int area_bottom = client.bottom - outer;
        const int area_w = std::max(area_right - area_left, dpi.logical_to_pixels(360));
        const int area_h = std::max(area_bottom - area_top, dpi.logical_to_pixels(240));

        const int cell_w = std::max((area_w - gap * 2) / 3, dpi.logical_to_pixels(200));
        const int cell_h = std::max(static_cast<int>((area_h - gap) / 2), dpi.logical_to_pixels(140));

        // CP14: 3x2 grid. Top row: bar / hbar / line. Bottom row: spline /
        // area. CP22: drop the empty placeholder — every cell carries a
        // caption above its chart view, so the grid now reads as 5
        // identified charts rather than 2 titled + 3 mystery cells. The
        // caption reserve (`caption_reserve`) shortens each chart HWND so
        // the caption sits in the freed band instead of being painted over
        // by the chart. 18 logical px is the minimum height for a 9-pt
        // semibold label with vertical-centre padding; further reductions
        // clip the descenders of the caption glyphs.
        const int caption_reserve = dpi.logical_to_pixels(18);

        const RECT bar_rect    = make_rect(area_left,                                 area_top + caption_reserve,                  cell_w, cell_h - caption_reserve);
        const RECT hbar_rect   = make_rect(area_left + cell_w + gap,                  area_top + caption_reserve,                  cell_w, cell_h - caption_reserve);
        const RECT line_rect   = make_rect(area_left + (cell_w + gap) * 2,            area_top + caption_reserve,                  cell_w, cell_h - caption_reserve);
        const RECT spline_rect = make_rect(area_left,                                 area_top + cell_h + gap + caption_reserve,   cell_w, cell_h - caption_reserve);
        const RECT area_rect   = make_rect(area_left + cell_w + gap,                  area_top + cell_h + gap + caption_reserve,   cell_w, cell_h - caption_reserve);

        MoveWindow(bar_.hwnd(),    bar_rect.left,    bar_rect.top,    rect_width(bar_rect),    rect_height(bar_rect),    TRUE);
        MoveWindow(hbar_.hwnd(),   hbar_rect.left,   hbar_rect.top,   rect_width(hbar_rect),   rect_height(hbar_rect),   TRUE);
        MoveWindow(line_.hwnd(),   line_rect.left,   line_rect.top,   rect_width(line_rect),   rect_height(line_rect),   TRUE);
        MoveWindow(spline_.hwnd(), spline_rect.left, spline_rect.top, rect_width(spline_rect), rect_height(spline_rect), TRUE);
        MoveWindow(area_.hwnd(),   area_rect.left,   area_rect.top,   rect_width(area_rect),   rect_height(area_rect),   TRUE);
    }

    void paint_chrome(HDC target, const RECT& client) noexcept {
        const nfui::ThemePalette& p = palette_;
        const nfui::DpiScale dpi(GetDpiForWindow(hwnd()));
        const int dpi_value = dpi.dpi();

        nfui::fill_rect(target, client, p.background);

        // Header band — coral hairline at the bottom matches the SettingsDemo
        // + Showcase brand treatment so all four samples read as siblings.
        RECT banner = client;
        banner.bottom = banner.top + dpi.logical_to_pixels(80);
        nfui::fill_rect(target, banner, p.background);
        const RECT hairline{banner.left, banner.bottom - 1, banner.right, banner.bottom};
        nfui::fill_rect(target, hairline, p.accent);

        HFONT title_font = fonts_.semibold(dpi_value, 16);
        HFONT body_font = fonts_.regular(dpi_value, 9);
        HFONT label_font = fonts_.semibold(dpi_value, 9);

        RECT title = banner;
        title.left += dpi.logical_to_pixels(20);
        title.top += dpi.logical_to_pixels(16);
        title.right -= dpi.logical_to_pixels(20);
        nfui::draw_text(target,
                        title,
                        L"NativeFrame UI Charts",
                        title_font,
                        p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        title.top += dpi.logical_to_pixels(36);
        nfui::draw_text(target,
                        title,
                        L"Bar / HBar / Line / Spline / Area chart kinds rendered by nfui::ChartView subclasses using the CP31 categorical palette + FontCache.",
                        body_font,
                        p.text_secondary,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        // CP22: per-cell caption strip. The previous version captioned only
        // bar + hbar in a header band, leaving line / spline / area as
        // mystery cells. Each cell now carries its own small caption above
        // the chart view so a reviewer (or screenshot reader) can identify
        // every kind at a glance. Coordinates mirror the layout_chrome()
        // cell rectangles so captions stay aligned across DPI swaps.
        const int outer = dpi.logical_to_pixels(20);
        const int gap = dpi.logical_to_pixels(16);
        const int header_h = dpi.logical_to_pixels(80);
        const int area_left = client.left + outer;
        const int area_top = client.top + outer + header_h + gap;
        const int area_right = client.right - outer;
        const int area_w = std::max(area_right - area_left, dpi.logical_to_pixels(360));
        const int cell_w = std::max((area_w - gap * 2) / 3, dpi.logical_to_pixels(200));
        const int cell_h = std::max(static_cast<int>(((client.bottom - outer) - area_top - gap) / 2), dpi.logical_to_pixels(140));

        struct CaptionSlot { RECT cell; const wchar_t* text; };
        const CaptionSlot slots[] = {
            { make_rect(area_left,                        area_top,                    cell_w, cell_h), L"Bar — monthly revenue"      },
            { make_rect(area_left + cell_w + gap,         area_top,                    cell_w, cell_h), L"HBar — platform mix"        },
            { make_rect(area_left + (cell_w + gap) * 2,   area_top,                    cell_w, cell_h), L"Line — revenue vs costs"    },
            { make_rect(area_left,                        area_top + cell_h + gap,     cell_w, cell_h), L"Spline — sensor A (Hz)"     },
            { make_rect(area_left + cell_w + gap,         area_top + cell_h + gap,     cell_w, cell_h), L"Area — monthly revenue"     },
        };

        const int caption_h = dpi.logical_to_pixels(16);
        const int caption_pad = dpi.logical_to_pixels(2);
        for (const auto& slot : slots) {
            RECT caption = slot.cell;
            caption.top += caption_pad;
            caption.bottom = caption.top + caption_h;
            nfui::draw_text(target,
                            caption,
                            slot.text,
                            label_font,
                            p.text_secondary,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
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

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::BarChartView bar_;
    nfui::HBarChartView hbar_;
    nfui::LineChartView line_;
    nfui::SplineChartView spline_;
    nfui::AreaChartView area_;
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