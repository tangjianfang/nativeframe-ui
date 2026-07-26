// Pure-logic unit tests for the chart interaction module (no HWND, no message loop).
#include "test_helpers.hpp"

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

    if (ok) {
        std::printf("All chart interaction tests passed.\n");
        return 0;
    }
    std::printf("SOME TESTS FAILED.\n");
    return 1;
}
