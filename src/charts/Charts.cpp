#include <nfui/Charts.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace nfui {

namespace {

constexpr int kAxisGutter = 40;      // px reserved for the y-axis tick labels (must exceed BarChartView::kAxisLabelGutter=28 + 8 px tick mark so the rightmost glyph doesn't sit flush against the plot frame)
constexpr int kTopGutter = 24;       // px reserved above the plot for value labels above the tallest bar
constexpr int kBottomGutter = 28;    // px reserved below the plot for category-axis tick labels (must cover kAxisLabelGutter so 1..N digits aren't clipped)

[[nodiscard]] int normalize_axis(double v, double axis_min, double axis_max, int span) noexcept {
    if (span <= 0) return 0;
    if (!(axis_max > axis_min)) return 0; // degenerate range; clamp to origin
    double t = (v - axis_min) / (axis_max - axis_min);
    if (t <= 0.0) return 0;
    if (t >= 1.0) return span;
    return static_cast<int>(t * static_cast<double>(span) + 0.5);
}

} // namespace

ChartLayout compute_chart_layout(RECT content_bounds,
                                 ChartKind kind,
                                 std::size_t series_count) noexcept {
    ChartLayout layout{};
    const int width = content_bounds.right - content_bounds.left;
    const int height = content_bounds.bottom - content_bounds.top;

    // Reserve a legend column when there is more than one series. A single
    // series does not need a legend box; the title names it.
    layout.legend_width_px = (series_count > 1) ? 120 : 0;

    // Single-series vertical charts need extra right margin so the last
    // x-axis tick label (e.g. "12") does not spill past the client edge.
    // Multi-series charts already reserve that space for the legend column;
    // horizontal bars get their width from the height budget via the swap.
    const bool right_gutter_needed = (series_count <= 1) &
                                     (kind != ChartKind::bar_horizontal);
    const int reserved_w = layout.legend_width_px + kAxisGutter +
                           (right_gutter_needed ? kAxisGutter : 0);
    const int reserved_h = kTopGutter + kBottomGutter + kAxisGutter;

    int plot_w = width - reserved_w;
    int plot_h = height - reserved_h;
    if (plot_w < 0) plot_w = 0;
    if (plot_h < 0) plot_h = 0;

    // Horizontal bars swap the natural aspect so bars stack as tall columns.
    if (kind == ChartKind::bar_horizontal) {
        std::swap(plot_w, plot_h);
    }

    layout.plot_bounds = RECT{
        content_bounds.left + kAxisGutter,
        content_bounds.top + kTopGutter,
        content_bounds.left + kAxisGutter + plot_w,
        content_bounds.top + kTopGutter + plot_h,
    };
    return layout;
}

std::vector<POINT> normalize_points(const std::vector<ChartPoint>& points,
                                    const ChartLayout& layout,
                                    const ChartAxisRange& x,
                                    const ChartAxisRange& y) noexcept {
    std::vector<POINT> out;
    out.reserve(points.size());
    const int plot_w = layout.plot_bounds.right - layout.plot_bounds.left;
    const int plot_h = layout.plot_bounds.bottom - layout.plot_bounds.top;
    for (const auto& p : points) {
        const int px = layout.plot_bounds.left + normalize_axis(p.x, x.min, x.max, plot_w);
        // Screen y axis is inverted: top of plot == max y value.
        const int plot_y = normalize_axis(p.y, y.min, y.max, plot_h);
        const int py = layout.plot_bounds.top + (plot_h - plot_y);
        out.push_back(POINT{ px, py });
    }
    return out;
}

std::vector<RECT> compute_bar_geometry(const ChartLayout& layout,
                                       std::size_t series_count,
                                       std::size_t bar_count,
                                       double gap_ratio) noexcept {
    std::vector<RECT> out;
    if (series_count == 0 || bar_count == 0) return out;
    out.resize(bar_count);

    const int plot_w = layout.plot_bounds.right - layout.plot_bounds.left;
    const int plot_h = layout.plot_bounds.bottom - layout.plot_bounds.top;
    if (plot_w <= 0 || plot_h <= 0) return out;

    // Clamp gap ratio into a sane band so degenerate inputs don't collapse the bar.
    const double clamped_gap = std::clamp(gap_ratio, 0.0, 0.9);

    const int total_band_w = plot_w / static_cast<int>(bar_count);
    if (total_band_w <= 0) return out;

    const int gap = std::max(1, static_cast<int>(total_band_w * clamped_gap + 0.5));
    const int cluster_w = total_band_w - gap;
    // Single series: bar spans the full cluster minus a single gap; otherwise split.
    const int bar_w = std::max(1, cluster_w / static_cast<int>(series_count));

    for (std::size_t i = 0; i < bar_count; ++i) {
        const int band_x = layout.plot_bounds.left + total_band_w * static_cast<int>(i);
        const int x = band_x + gap / 2;
        out[i] = RECT{
            x,
            layout.plot_bounds.top,
            x + bar_w,
            layout.plot_bounds.bottom,
        };
    }
    return out;
}

std::vector<POINT> catmull_rom_to_bezier(const std::vector<POINT>& points,
                                          double tension) noexcept {
    std::vector<POINT> out;
    if (points.size() < 2) return out;
    // CP28: Win32 PolyBezier and Gdiplus::Graphics::DrawBeziers expect
    // chained-cubic segments in a 1 + 3*N count (1 = first-segment start
    // anchor, 3*N = three points per subsequent segment: c1, c2, end). The
    // previous revision reserved 4*(N-1) and emitted four points per
    // segment, which yielded 3N-1 too many and made both backends fail
    // silently — exactly the empty spline-chart the CP27 audit caught.
    // Number of cubic segments is N-1, so the correct output count is
    // 1 + 3*(N-1) = 3N - 2.
    out.reserve(3 * points.size() - 2);

    const double t = std::clamp(tension, 0.0, 1.0);
    const double k = t / 3.0;

    auto lerp_x = [](const POINT& a, const POINT& b, double factor) {
        return static_cast<LONG>(static_cast<double>(a.x) +
                                 (static_cast<double>(b.x) - static_cast<double>(a.x)) * factor);
    };
    auto lerp_y = [](const POINT& a, const POINT& b, double factor) {
        return static_cast<LONG>(static_cast<double>(a.y) +
                                 (static_cast<double>(b.y) - static_cast<double>(a.y)) * factor);
    };

    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const POINT& p1 = points[i];
        const POINT& p2 = points[i + 1];
        // Mirror endpoints: p0 = p1, p3 = p2 for the first/last segments.
        const POINT& p0 = (i == 0) ? p1 : points[i - 1];
        const POINT& p3 = (i + 2 >= points.size()) ? p2 : points[i + 2];

        const POINT b1 = POINT{
            lerp_x(p1, p1, 0.0) + static_cast<LONG>(k * (p2.x - p0.x) + 0.5),
            lerp_y(p1, p1, 0.0) + static_cast<LONG>(k * (p2.y - p0.y) + 0.5),
        };
        const POINT b2 = POINT{
            lerp_x(p2, p2, 0.0) - static_cast<LONG>(k * (p3.x - p1.x) + 0.5),
            lerp_y(p2, p2, 0.0) - static_cast<LONG>(k * (p3.y - p1.y) + 0.5),
        };

        // First segment also needs the start anchor (subsequent segments
        // reuse the previous end as their start).
        if (i == 0) {
            out.push_back(p1);
        }
        out.push_back(b1);
        out.push_back(b2);
        out.push_back(p2);
    }
    return out;
}

std::wstring format_axis_tick(double value, std::wstring_view label_format) noexcept {
    // Recognize the placeholder shape `{:.Nf}` or `{:.N%}` and translate to a
    // matching printf format. Anything else falls back to `{:.0f}` so a malformed
    // label_format never propagates a broken string out of on_paint.
    wchar_t buf[64]{};
    int precision = 0;
    bool is_percent = false;
    bool recognized = false;

    if (label_format.size() >= 5 &&
        label_format[0] == L'{' && label_format[1] == L':' && label_format[2] == L'.' &&
        label_format.back() == L'}') {
        // Pull out the spec body between `:.` and the trailing `}`.
        const std::size_t close = label_format.size() - 1;
        std::size_t i = 3;
        int precision_acc = 0;
        bool saw_digit = false;
        for (; i < close; ++i) {
            const wchar_t c = label_format[i];
            if (c >= L'0' && c <= L'9') {
                precision_acc = precision_acc * 10 + (c - L'0');
                saw_digit = true;
            } else if (c == L'f' || c == L'%') {
                if (c == L'%') is_percent = true;
                // For the supported placeholder grammar, only the trailing
                // specifier (single f or %) is accepted; anything else (e.g.
                // intermediate text) is treated as unrecognized.
                if (i + 1 == close) {
                    precision = saw_digit ? precision_acc : 0;
                    // Cap precision so a hostile or accidental large digit run
                    // can't blow past buf.
                    if (precision < 0) precision = 0;
                    if (precision > 12) precision = 12;
                    recognized = true;
                }
                break;
            } else {
                break;
            }
        }
    }

    if (!recognized) {
        // Fallback matches the historical hardcoded `%.0f` so existing callers
        // that never set label_format (or set a malformed one) see no change.
        precision = 0;
        is_percent = false;
    }

    const double scaled = is_percent ? (value * 100.0) : value;
    if (is_percent) {
        std::swprintf(buf, std::size(buf), L"%.*f%%", precision, scaled);
    } else {
        std::swprintf(buf, std::size(buf), L"%.*f", precision, scaled);
    }
    buf[std::size(buf) - 1] = L'\0';
    return std::wstring(buf);
}

} // namespace nfui
