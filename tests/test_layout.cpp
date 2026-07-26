#include <nfui/Layout.hpp>
#include "test_helpers.hpp"

using nfui_test::expect;

int wmain() {
    bool ok = true;

    // --- split_horizontally ---
    {
        nfui::Rect bounds{0, 0, 1000, 500};
        nfui::SplitterLayout s = nfui::split_horizontally(bounds, 0.25, 4);
        ok = expect(s.first.width == 250, L"split_h: first width == 250") && ok;
        ok = expect(s.splitter.x == 250 && s.splitter.width == 4, L"split_h: splitter at 250 w=4") && ok;
        ok = expect(s.second.x == 254 && s.second.width == 746, L"split_h: second starts at 254") && ok;
        ok = expect(s.first.height == 500 && s.second.height == 500, L"split_h: heights preserved") && ok;
    }

    // --- split_horizontally edge cases ---
    {
        nfui::Rect bounds{0, 0, 100, 100};
        nfui::SplitterLayout s = nfui::split_horizontally(bounds, -1.0, 4);
        ok = expect(s.first.width == 0, L"split_h: negative ratio clamps to 0") && ok;

        s = nfui::split_horizontally(bounds, 2.0, 4);
        ok = expect(s.first.width == 96, L"split_h: ratio > 1 clamps to max") && ok;

        s = nfui::split_horizontally(bounds, 0.5, -2);
        ok = expect(s.splitter.width == 0, L"split_h: negative splitter width clamps to 0") && ok;
    }

    // --- split_vertically ---
    {
        nfui::Rect bounds{0, 0, 800, 600};
        nfui::SplitterLayout s = nfui::split_vertically(bounds, 0.5, 6);
        ok = expect(s.first.height == 300, L"split_v: first height == 300") && ok;
        ok = expect(s.splitter.y == 300 && s.splitter.height == 6, L"split_v: splitter at y=300 h=6") && ok;
        ok = expect(s.second.y == 306 && s.second.height == 294, L"split_v: second starts at 306") && ok;
        ok = expect(s.first.width == 800 && s.second.width == 800, L"split_v: widths preserved") && ok;
    }

    // --- split_vertically edge cases ---
    {
        nfui::Rect bounds{10, 20, 400, 200};
        nfui::SplitterLayout s = nfui::split_vertically(bounds, 0.0, 4);
        ok = expect(s.first.height == 0, L"split_v: ratio 0 gives zero first") && ok;
        ok = expect(s.first.y == 20, L"split_v: offset y preserved") && ok;

        s = nfui::split_vertically(bounds, 1.0, 4);
        ok = expect(s.first.height == 196, L"split_v: ratio 1 clamps to max") && ok;
        ok = expect(s.second.height == 0, L"split_v: ratio 1 gives zero second") && ok;
    }

    // --- layout_horizontal ---
    {
        nfui::Rect bounds{0, 0, 1000, 500};
        auto rows = nfui::layout_horizontal(bounds, {100, 200, 300}, 10);
        ok = expect(rows.size() == 3, L"layout_h: 3 items") && ok;
        ok = expect(rows[0].x == 0 && rows[0].width == 100, L"layout_h: first at x=0 w=100") && ok;
        ok = expect(rows[1].x == 110 && rows[1].width == 200, L"layout_h: second at x=110 w=200") && ok;
        ok = expect(rows[2].x == 320 && rows[2].width == 300, L"layout_h: third at x=320 w=300") && ok;
        ok = expect(rows[0].height == 500, L"layout_h: height fills bounds") && ok;
    }

    // --- layout_vertical ---
    {
        nfui::Rect bounds{0, 0, 800, 600};
        auto cols = nfui::layout_vertical(bounds, {100, 150}, 8);
        ok = expect(cols.size() == 2, L"layout_v: 2 items") && ok;
        ok = expect(cols[0].y == 0 && cols[0].height == 100, L"layout_v: first at y=0 h=100") && ok;
        ok = expect(cols[1].y == 108 && cols[1].height == 150, L"layout_v: second at y=108 h=150") && ok;
        ok = expect(cols[0].width == 800, L"layout_v: width fills bounds") && ok;
    }

    // --- layout_vertical with offset origin ---
    {
        nfui::Rect bounds{10, 20, 400, 300};
        auto cols = nfui::layout_vertical(bounds, {50}, 0);
        ok = expect(cols.size() == 1, L"layout_v offset: 1 item") && ok;
        ok = expect(cols[0].x == 10 && cols[0].y == 20, L"layout_v offset: origin preserved") && ok;
        ok = expect(cols[0].width == 400 && cols[0].height == 50, L"layout_v offset: dimensions correct") && ok;
    }

    // --- anchor_layout: left+top only (default, no resize) ---
    {
        nfui::Rect ctrl{10, 10, 100, 30};
        nfui::Rect orig_parent{0, 0, 800, 600};
        nfui::Rect new_parent{0, 0, 1000, 700};
        nfui::Rect r = nfui::anchor_layout(ctrl, orig_parent, new_parent, nfui::anchor_left | nfui::anchor_top);
        ok = expect(r.x == 10 && r.y == 10 && r.width == 100 && r.height == 30,
                    L"anchor left+top: position and size unchanged") && ok;
    }

    // --- anchor_layout: right only (shifts x) ---
    {
        nfui::Rect ctrl{700, 10, 80, 30};
        nfui::Rect orig_parent{0, 0, 800, 600};
        nfui::Rect new_parent{0, 0, 1000, 600};
        nfui::Rect r = nfui::anchor_layout(ctrl, orig_parent, new_parent, nfui::anchor_right | nfui::anchor_top);
        ok = expect(r.x == 900, L"anchor right: x shifts by delta_w") && ok;
        ok = expect(r.width == 80, L"anchor right: width unchanged") && ok;
    }

    // --- anchor_layout: left+right (stretches width) ---
    {
        nfui::Rect ctrl{10, 10, 780, 30};
        nfui::Rect orig_parent{0, 0, 800, 600};
        nfui::Rect new_parent{0, 0, 1000, 600};
        nfui::Rect r = nfui::anchor_layout(ctrl, orig_parent, new_parent, nfui::anchor_left | nfui::anchor_right | nfui::anchor_top);
        ok = expect(r.x == 10, L"anchor left+right: x stays") && ok;
        ok = expect(r.width == 980, L"anchor left+right: width grows by delta_w") && ok;
    }

    // --- anchor_layout: top+bottom (stretches height) ---
    {
        nfui::Rect ctrl{10, 10, 100, 580};
        nfui::Rect orig_parent{0, 0, 800, 600};
        nfui::Rect new_parent{0, 0, 800, 800};
        nfui::Rect r = nfui::anchor_layout(ctrl, orig_parent, new_parent, nfui::anchor_left | nfui::anchor_top | nfui::anchor_bottom);
        ok = expect(r.y == 10, L"anchor top+bottom: y stays") && ok;
        ok = expect(r.height == 780, L"anchor top+bottom: height grows by delta_h") && ok;
    }

    // --- anchor_layout: bottom only (shifts y) ---
    {
        nfui::Rect ctrl{10, 560, 100, 30};
        nfui::Rect orig_parent{0, 0, 800, 600};
        nfui::Rect new_parent{0, 0, 800, 700};
        nfui::Rect r = nfui::anchor_layout(ctrl, orig_parent, new_parent, nfui::anchor_left | nfui::anchor_bottom);
        ok = expect(r.y == 660, L"anchor bottom: y shifts by delta_h") && ok;
        ok = expect(r.height == 30, L"anchor bottom: height unchanged") && ok;
    }

    // --- anchor_layout: all four (stretches both) ---
    {
        nfui::Rect ctrl{10, 10, 780, 580};
        nfui::Rect orig_parent{0, 0, 800, 600};
        nfui::Rect new_parent{0, 0, 1000, 800};
        nfui::Rect r = nfui::anchor_layout(ctrl, orig_parent, new_parent, nfui::anchor_all);
        ok = expect(r.width == 980 && r.height == 780, L"anchor all: both dimensions grow") && ok;
    }

    // --- anchor_layout: shrink (negative delta) ---
    {
        nfui::Rect ctrl{10, 10, 780, 580};
        nfui::Rect orig_parent{0, 0, 800, 600};
        nfui::Rect new_parent{0, 0, 400, 300};
        nfui::Rect r = nfui::anchor_layout(ctrl, orig_parent, new_parent, nfui::anchor_all);
        ok = expect(r.width == 380 && r.height == 280, L"anchor all shrink: both dimensions shrink") && ok;
    }

    // --- anchor_layout: shrink below zero clamps ---
    {
        nfui::Rect ctrl{10, 10, 100, 100};
        nfui::Rect orig_parent{0, 0, 800, 600};
        nfui::Rect new_parent{0, 0, 50, 50};
        nfui::Rect r = nfui::anchor_layout(ctrl, orig_parent, new_parent, nfui::anchor_all);
        ok = expect(r.width == 0 && r.height == 0, L"anchor all extreme shrink: clamps to 0") && ok;
    }

    return ok ? 0 : 1;
}
