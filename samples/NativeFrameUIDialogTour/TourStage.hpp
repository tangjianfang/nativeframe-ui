#pragma once

// CP-B2: tour-stage model for NativeFrameUIDialogTour.
//
// The polished DialogTour replaces the older "3 buttons + debug string"
// layout with a real product tour: each user action moves the window
// to a new stage, the status card renders the current stage's human-
// readable label, and the modeless dialog's submitted payload (when
// present) is shown as a second-line readout. This header pins the
// stage enum + the two human-readable helpers in one place so the
// main WindowProc, the status card paint, and any future stage
// additions (e.g. an "About" demo with a confirmation step) all agree
// on the same vocabulary.

#include <string_view>

namespace dialog_tour {

// CP-B2: every user action drives a transition. The status card's
// eyebrow reads "Last action" and the body text uses stage_label()
// below. `ready` is the initial state (no clicks yet).
enum class TourStage {
    ready,            // initial — no user action
    about_opened,     // primary button clicked; modal about to show
    about_ok,         // modal About dismissed via IDOK
    about_cancel,     // modal About dismissed via IDCANCEL / WM_CLOSE
    prefs_opened,     // secondary button clicked; modeless is up
    prefs_submitted,  // modeless submitted via its IDOK button
    prefs_closed,     // tertiary button / dialog WM_CLOSE — modeless gone
};

// CP-B2: user-readable label for the stage. Strings are stable across
// themes so the screenshot-diff scroller in the visual audit can
// detect regressions programmatically.
[[nodiscard]] inline std::wstring_view stage_label(TourStage s) noexcept {
    switch (s) {
        case TourStage::ready:           return L"Welcome — try each action to see the dialog types.";
        case TourStage::about_opened:    return L"About dialog opened — choose an action to continue.";
        case TourStage::about_ok:        return L"About dialog closed with OK.";
        case TourStage::about_cancel:    return L"About dialog cancelled.";
        case TourStage::prefs_opened:    return L"Preferences opened — close via the dialog or the button below.";
        case TourStage::prefs_submitted: return L"Preferences submitted.";
        case TourStage::prefs_closed:    return L"Active dialog closed.";
    }
    return L"";
}

// CP-B2: the tertiary "Close active dialog" button is only sensible
// when the modeless is on screen. The modal About dialog closes
// itself, so the tertiary stay hidden until the user actually opens
// the modeless — this fixes the audit's "Close modeless still occupies
// the main action slot when no modeless is open" finding.
[[nodiscard]] inline bool show_close_action(TourStage s) noexcept {
    return s == TourStage::prefs_opened;
}

} // namespace dialog_tour

