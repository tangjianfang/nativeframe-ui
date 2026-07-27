// Pure-logic unit tests for the chart interaction module (no HWND, no message loop).
#include "test_helpers.hpp"

#include <nfui/ChartGroup.hpp>
#include <nfui/ChartInteraction.hpp>
#include <nfui/Charts.hpp>

// Include internal headers for direct testing.
#include "../src/charts/internal/ChartAnimator.hpp"
#include "../src/charts/internal/HitTest.hpp"
#include "../src/charts/internal/InteractionState.hpp"

#include <cmath>
#include <vector>

namespace {

// Helper: build a simple layout with a known plot area.
nfui::ChartLayout make_layout(int left, int top, int right, int bottom) {
    nfui::ChartLayout layout{};
    layout.plot_bounds = RECT{left, top, right, bottom};
    return layout;
}

bool approx(double a, double b, double eps = 0.01) {
    return std::fabs(a - b) < eps;
}

} // namespace

int main() {
    bool ok = true;

    // --- pixel_to_data / data_to_pixel round-trip ---
    {
        nfui::ChartLayout layout = make_layout(100, 50, 500, 350);  // 400x300 plot
        nfui::ChartAxisRange ax{0.0, 100.0};
        nfui::ChartAxisRange ay{0.0, 200.0};

        // Center of plot -> center of data range.
        POINT center_px{300, 200};
        nfui::ChartPoint data = nfui::charts_internal::pixel_to_data(center_px, layout, ax, ay);
        ok = nfui_test::expect(approx(data.x, 50.0) && approx(data.y, 100.0),
                               L"pixel_to_data center") && ok;

        // Round-trip: data -> pixel -> data.
        nfui::ChartPoint orig{25.0, 150.0};
        POINT px = nfui::charts_internal::data_to_pixel(orig, layout, ax, ay);
        nfui::ChartPoint back = nfui::charts_internal::pixel_to_data(px, layout, ax, ay);
        ok = nfui_test::expect(approx(back.x, orig.x, 0.5) && approx(back.y, orig.y, 0.7),
                               L"data->pixel->data round-trip") && ok;

        // Top-left of plot -> (min_x, max_y).
        POINT tl{100, 50};
        nfui::ChartPoint tl_data = nfui::charts_internal::pixel_to_data(tl, layout, ax, ay);
        ok = nfui_test::expect(approx(tl_data.x, 0.0) && approx(tl_data.y, 200.0),
                               L"pixel_to_data top-left") && ok;

        // Bottom-right of plot -> (max_x, min_y).
        POINT br{500, 350};
        nfui::ChartPoint br_data = nfui::charts_internal::pixel_to_data(br, layout, ax, ay);
        ok = nfui_test::expect(approx(br_data.x, 100.0) && approx(br_data.y, 0.0),
                               L"pixel_to_data bottom-right") && ok;
    }

    // --- hit_test_point ---
    {
        nfui::ChartLayout layout = make_layout(0, 0, 400, 300);
        nfui::ChartAxisRange ax{0.0, 10.0};
        nfui::ChartAxisRange ay{0.0, 10.0};

        std::vector<nfui::ChartPoint> pts = {{5.0, 5.0}, {2.0, 8.0}};
        std::vector<nfui::ChartSeries> series;
        series.push_back(nfui::ChartSeries{L"S1", nfui::Color{0}, pts});

        // Cursor exactly on the first point (center of plot).
        POINT on_point = nfui::charts_internal::data_to_pixel({5.0, 5.0}, layout, ax, ay);
        nfui::ChartHitResult hit = nfui::charts_internal::hit_test_point(on_point, series, layout, ax, ay, 8);
        ok = nfui_test::expect(hit.hit && hit.series_index == 0 && hit.point_index == 0,
                               L"hit_test_point exact") && ok;

        // Cursor far away -> no hit.
        POINT far_pt{0, 0};
        nfui::ChartHitResult miss = nfui::charts_internal::hit_test_point(far_pt, series, layout, ax, ay, 8);
        ok = nfui_test::expect(!miss.hit, L"hit_test_point miss") && ok;

        // Cursor within tolerance.
        POINT near_pt{on_point.x + 5, on_point.y + 3};
        nfui::ChartHitResult near_hit = nfui::charts_internal::hit_test_point(near_pt, series, layout, ax, ay, 8);
        ok = nfui_test::expect(near_hit.hit, L"hit_test_point within tolerance") && ok;
    }

    // --- point_in_plot ---
    {
        nfui::ChartLayout layout = make_layout(50, 50, 350, 250);
        ok = nfui_test::expect(nfui::charts_internal::point_in_plot({200, 150}, layout),
                               L"point_in_plot inside") && ok;
        ok = nfui_test::expect(!nfui::charts_internal::point_in_plot({10, 10}, layout),
                               L"point_in_plot outside") && ok;
    }

    // --- clamp_to_axis ---
    {
        nfui::ChartAxisRange ax{0.0, 100.0};
        ok = nfui_test::expect(approx(nfui::charts_internal::clamp_to_axis(50.0, ax), 50.0),
                               L"clamp mid") && ok;
        ok = nfui_test::expect(approx(nfui::charts_internal::clamp_to_axis(-10.0, ax), 0.0),
                               L"clamp below") && ok;
        ok = nfui_test::expect(approx(nfui::charts_internal::clamp_to_axis(150.0, ax), 100.0),
                               L"clamp above") && ok;
    }

    // --- zoom_axis ---
    {
        nfui::ChartAxisRange ax{0.0, 100.0};
        // Zoom in (factor < 1) centered at 50.
        auto zoomed = nfui::charts_internal::zoom_axis(ax, 50.0, 0.5);
        ok = nfui_test::expect(approx(zoomed.min, 25.0) && approx(zoomed.max, 75.0),
                               L"zoom_axis in") && ok;
        // Zoom out (factor > 1) centered at 50.
        auto zoomed_out = nfui::charts_internal::zoom_axis(ax, 50.0, 2.0);
        ok = nfui_test::expect(approx(zoomed_out.min, -50.0) && approx(zoomed_out.max, 150.0),
                               L"zoom_axis out") && ok;
    }

    // --- pan_axis ---
    {
        nfui::ChartAxisRange ax{0.0, 100.0};
        auto panned = nfui::charts_internal::pan_axis(ax, 20.0);
        ok = nfui_test::expect(approx(panned.min, 20.0) && approx(panned.max, 120.0),
                               L"pan_axis") && ok;
    }

    // --- InteractionState transitions ---
    {
        using nfui::charts_internal::InteractionPhase;
        using nfui::charts_internal::InteractionState;
        using nfui::charts_internal::transition;

        InteractionState st{};
        ok = nfui_test::expect(st.phase == InteractionPhase::idle, L"initial idle") && ok;

        // idle -> hover: valid.
        ok = nfui_test::expect(transition(st, InteractionPhase::hover), L"idle->hover") && ok;
        // hover -> drag_point: valid.
        ok = nfui_test::expect(transition(st, InteractionPhase::drag_point), L"hover->drag") && ok;
        // drag_point -> range_select: INVALID.
        ok = nfui_test::expect(!transition(st, InteractionPhase::range_select),
                               L"drag->range_select invalid") && ok;
        // drag_point -> idle: valid.
        ok = nfui_test::expect(transition(st, InteractionPhase::idle), L"drag->idle") && ok;
        // idle -> pan: valid.
        ok = nfui_test::expect(transition(st, InteractionPhase::pan), L"idle->pan") && ok;
        // pan -> idle: valid.
        ok = nfui_test::expect(transition(st, InteractionPhase::idle), L"pan->idle") && ok;
    }

    // --- begin_drag / begin_range_select / begin_pan ---
    {
        using nfui::charts_internal::InteractionPhase;
        using nfui::charts_internal::InteractionState;

        InteractionState st{};
        nfui::charts_internal::begin_drag(st, 1, 3, {5.0, 10.0});
        ok = nfui_test::expect(st.phase == InteractionPhase::drag_point &&
                               st.drag_series == 1 && st.drag_point == 3 &&
                               approx(st.drag_original.x, 5.0),
                               L"begin_drag state") && ok;

        nfui::charts_internal::reset_to_idle(st);
        nfui::charts_internal::begin_range_select(st, POINT{10, 20});
        ok = nfui_test::expect(st.phase == InteractionPhase::range_select &&
                               st.select_origin_px.x == 10,
                               L"begin_range_select state") && ok;

        nfui::charts_internal::reset_to_idle(st);
        nfui::ChartAxisRange rx{0, 100}, ry{0, 200};
        nfui::charts_internal::begin_pan(st, POINT{50, 60}, rx, ry);
        ok = nfui_test::expect(st.phase == InteractionPhase::pan &&
                               st.pan_origin_px.x == 50 &&
                               approx(st.pan_start_x.max, 100.0),
                               L"begin_pan state") && ok;
    }

    // --- AxisAnimation ---
    {
        nfui::charts_internal::AxisAnimation anim{};
        nfui::ChartAxisRange fx{0, 100}, tx{25, 75};
        nfui::ChartAxisRange fy{0, 200}, ty{50, 150};
        anim.begin(fx, tx, fy, ty, 1000, 200);

        ok = nfui_test::expect(anim.active, L"anim active after begin") && ok;

        // At start (t=0): should be at 'from'.
        nfui::ChartAxisRange ox{}, oy{};
        anim.sample(1000, ox, oy);
        ok = nfui_test::expect(approx(ox.min, 0.0, 1.0) && approx(oy.min, 0.0, 1.0),
                               L"anim at t=0") && ok;

        // After duration: should snap to 'to' and deactivate.
        anim.sample(1200, ox, oy);
        ok = nfui_test::expect(!anim.active && approx(ox.min, 25.0) && approx(oy.max, 150.0),
                               L"anim at t=end") && ok;
    }

    // --- ChartInteractionMode bitwise ops ---
    {
        using M = nfui::ChartInteractionMode;
        unsigned flags = M::drag_edit | M::zoom | M::pan;
        ok = nfui_test::expect(nfui::has_mode(flags, M::drag_edit), L"has drag_edit") && ok;
        ok = nfui_test::expect(nfui::has_mode(flags, M::zoom), L"has zoom") && ok;
        ok = nfui_test::expect(!nfui::has_mode(flags, M::select), L"no select") && ok;
    }

    // --- normalize_points (CP40 export-related) ---
    // Verifies that the public normalize_points helper maps a midpoint
    // to the centre of the plot rectangle and clamps out-of-range data.
    {
        nfui::ChartLayout layout = make_layout(100, 50, 500, 350);  // 400x300
        nfui::ChartAxisRange ax{0.0, 100.0};
        nfui::ChartAxisRange ay{0.0, 200.0};
        std::vector<nfui::ChartPoint> pts{
            {50.0, 100.0},   // midpoint -> plot centre (300, 200)
            {-10.0, -20.0},  // clamps below
            {200.0, 400.0},  // clamps above
        };
        auto out = nfui::normalize_points(pts, layout, ax, ay);
        ok = nfui_test::expect(out.size() == 3, L"normalize_points size") && ok;
        if (out.size() == 3) {
            // Midpoint.
            ok = nfui_test::expect(approx(static_cast<double>(out[0].x), 300.0) &&
                                   approx(static_cast<double>(out[0].y), 200.0),
                                   L"normalize midpoint") && ok;
            // Below clamps to plot's left edge / bottom edge (screen
            // y is inverted so y.min maps to plot_bounds.bottom).
            ok = nfui_test::expect(out[1].x == 100 && out[1].y == 350,
                                   L"normalize below clamps") && ok;
            // Above clamps to plot's right edge / top edge (y.max maps
            // to plot_bounds.top).
            ok = nfui_test::expect(out[2].x == 500 && out[2].y == 50,
                                   L"normalize above clamps") && ok;
        }
    }

    // --- Invisible hit-test ---
    // When a series has visible=false, hit_test_point must skip it even
    // when its data points fall inside the tolerance radius. This is the
    // same predicate that drives the renderer + range-select summary.
    {
        nfui::ChartLayout layout = make_layout(0, 0, 100, 100);
        nfui::ChartAxisRange ax{0.0, 100.0};
        nfui::ChartAxisRange ay{0.0, 100.0};
        std::vector<nfui::ChartSeries> series;
        nfui::ChartSeries s{};
        s.name = L"only";
        // Place a single point at (50, 50) -> pixel (50, 50).
        s.points = {{50.0, 50.0}};
        s.visible = true;
        series.push_back(s);
        // Same point in a hidden series must NOT be hit.
        nfui::ChartSeries hidden = s;
        hidden.visible = false;
        series.push_back(hidden);

        nfui::ChartHitResult hit = nfui::charts_internal::hit_test_point(
            POINT{50, 50}, series, layout, ax, ay, /*tol=*/10);
        ok = nfui_test::expect(hit.hit && hit.series_index == 0,
                               L"hit_test_point picks visible series") && ok;
    }

    // --- apply_settings clamping + unknown kind_id ---
    // apply_settings must clamp animation_ms / hit_tolerance_px into the
    // documented ranges and preserve an unknown kind_id through the
    // round-trip (the chart internally snaps it to ChartKind::line, but
    // the raw int stays in settings() so callers can detect the typo).
    // We construct a ChartView that never opens a HWND; every code path
    // in apply_settings that touches the window is gated on hwnd()!=null.
    {
        nfui::LineChartView view{};
        nfui::ChartSettings s{};
        s.animation_ms = 9000u;       // out of range -> 5000
        s.hit_tolerance_px = 0;       // out of range -> 1
        s.kind_id = 99;               // unknown; coerced internally to line
        s.show_legend = false;
        view.apply_settings(s);

        const nfui::ChartSettings round_trip = view.settings();
        ok = nfui_test::expect(round_trip.animation_ms == 5000u,
                               L"animation_ms clamped to 5000") && ok;
        ok = nfui_test::expect(round_trip.hit_tolerance_px == 1,
                               L"hit_tolerance_px clamped to 1") && ok;
        ok = nfui_test::expect(round_trip.kind_id == 99,
                               L"unknown kind_id preserved through round-trip") && ok;
        ok = nfui_test::expect(round_trip.show_legend == false,
                               L"show_legend propagated") && ok;

        // Second clamp pass: hit_tolerance_px = 100 -> 64.
        nfui::ChartSettings s2{};
        s2.hit_tolerance_px = 100;
        view.apply_settings(s2);
        ok = nfui_test::expect(view.settings().hit_tolerance_px == 64,
                               L"hit_tolerance_px clamped to 64") && ok;
    }

    // --- ChartGroup ---
    // Two HWND-less LineChartViews; enable interaction so the group can
    // mutate their visible ranges via set_visible_range. The group must
    // propagate x ranges to every linked chart while leaving y ranges
    // independent; null/duplicate/second-primary must be rejected.
    {
        nfui::LineChartView a{};
        nfui::LineChartView b{};
        nfui::ChartInteractionOptions opts{};
        opts.mode_flags = nfui::ChartInteractionMode::zoom
                        | nfui::ChartInteractionMode::pan;
        a.enable_interaction(opts);
        b.enable_interaction(opts);

        nfui::ChartGroup g{};
        ok = nfui_test::expect(g.add_chart(nullptr, nfui::ChartGroupRole::primary) == false,
                               L"null chart rejected") && ok;
        ok = nfui_test::expect(g.add_chart(&a, nfui::ChartGroupRole::primary),
                               L"add primary") && ok;
        ok = nfui_test::expect(g.size() == 1 && g.primary() == &a,
                               L"primary pointer + size after add") && ok;
        // Duplicate add rejected.
        ok = nfui_test::expect(!g.add_chart(&a, nfui::ChartGroupRole::sub),
                               L"duplicate chart rejected") && ok;
        // Second primary rejected.
        ok = nfui_test::expect(!g.add_chart(&b, nfui::ChartGroupRole::primary),
                               L"second primary rejected") && ok;
        ok = nfui_test::expect(g.add_chart(&b, nfui::ChartGroupRole::sub),
                               L"add sub chart") && ok;

        // Push a new x range into the primary; sub must follow, y ranges
        // stay independent (set_visible_range preserves the chart's
        // own y when we don't propagate it).
        g.set_primary_x_axis(nfui::ChartAxisRange{2.0, 8.0});
        ok = nfui_test::expect(approx(a.visible_x().min, 2.0) &&
                               approx(a.visible_x().max, 8.0) &&
                               approx(b.visible_x().min, 2.0) &&
                               approx(b.visible_x().max, 8.0),
                               L"linked x propagation") && ok;
        // Y ranges unchanged from initial interaction-enable defaults.
        ok = nfui_test::expect(a.visible_y().min == 0.0 && a.visible_y().max == 1.0 &&
                               b.visible_y().min == 0.0 && b.visible_y().max == 1.0,
                               L"y ranges stay independent") && ok;

        // Disable linking then change the primary again; sub must NOT
        // follow. We exercise the public link_x_axis setter.
        g.link_x_axis(false);
        ok = nfui_test::expect(!g.x_axis_linked(), L"link_x_axis(false) reported") && ok;
        g.set_primary_x_axis(nfui::ChartAxisRange{5.0, 6.0});
        ok = nfui_test::expect(approx(a.visible_x().min, 5.0),
                               L"primary x updated when unlinked") && ok;
        ok = nfui_test::expect(approx(b.visible_x().min, 2.0),
                               L"sub x preserved when unlinked") && ok;

        // remove_chart reduces size() and clears the role on the
        // remaining chart so primary_ no longer points to the
        // removed chart.
        g.remove_chart(&a);
        ok = nfui_test::expect(g.size() == 1 && g.primary() == &b,
                               L"primary promotes after remove") && ok;
        g.remove_chart(&b);
        ok = nfui_test::expect(g.size() == 0 && g.primary() == nullptr,
                               L"empty group after remove") && ok;
    }

    if (ok) {
        std::printf("All chart interaction tests passed.\n");
        return 0;
    }
    std::printf("SOME TESTS FAILED.\n");
    return 1;
}
