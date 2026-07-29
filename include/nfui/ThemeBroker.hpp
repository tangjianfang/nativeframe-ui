#pragma once

#include <functional>

#include <windows.h>

#include <nfui/Theme.hpp>

namespace nfui {

// CP-A1: theme-change command IDs (sample wire-up). Mapped to
// ThemeBroker::instance().set_theme() from the demo's on_command handler.
// Mapped onto the existing IDM_NFUI_* range with a high prefix so they
// never collide with user-defined menu IDs.
inline constexpr int ID_THEME_SYSTEM      = 0xEF00;
inline constexpr int ID_THEME_LIGHT       = 0xEF01;
inline constexpr int ID_THEME_DARK        = 0xEF02;
inline constexpr int ID_THEME_HIGH_CONTRAST = 0xEF03;

// CP-A1: per-HWND theme-change callback. Receives the new mode.
// Implementations should re-resolve their palette, InvalidateRect, and
// re-paint non-client chrome. Must run on UI thread. set_theme broadcasts
// in process order (no order guarantees to the consumer).
using ThemeChangeHandler = std::function<void(ThemeMode)>;

// CP-A1: singleton broker that holds the process-wide current theme and
// fans WM_THEMECHANGED (plus the per-HWND callback) out to every registered
// HWND on a set_theme(). The companion test in
// NativeFrameUISmokeTest::test_theme_broadcast_propagates_to_children locks
// the contract: parent + children all receive the callback once per real
// (non-idempotent) change.
//
// ApplicationContext integration: ApplicationContext does not currently
// expose a service-registration slot (see include/nfui/Application.hpp —
// it only carries instance + show_command). When a service registry lands,
// prefer registering ThemeBroker through there; until then this singleton
// is the single source of truth and instance() is safe to call from any
// thread (Magic Statics init is thread-safe).
class ThemeBroker {
public:
    // Singleton accessor. First call constructs; subsequent calls return the
    // same instance. Construction is thread-safe (Magic Statics).
    static ThemeBroker& instance() noexcept;

    // Set the process-wide theme. UI thread only. Broadcasts WM_THEMECHANGED
    // to every registered HWND. Idempotent — same mode is a no-op.
    void set_theme(ThemeMode mode) noexcept;

    // Current process theme.
    [[nodiscard]] ThemeMode current() const noexcept { return current_; }

    // Register an HWND with a callback. The HWND must outlive the registration
    // (unregister in WM_NCDESTROY). Multiple registrations for the same HWND
    // are replaced by the latest callback.
    void register_hwnd(HWND hwnd, ThemeChangeHandler handler) noexcept;
    void unregister_hwnd(HWND hwnd) noexcept;

    // Internal: dispatch loop used by the broadcast. Sends WM_THEMECHANGED
    // to every registered HWND, then invokes each handler outside the
    // registry lock (handlers may register / unregister without deadlocking).
    void broadcast() noexcept;

private:
    ThemeBroker() = default;

    // Storage lives in the TU-local `Registry` struct in ThemeBroker.cpp
    // to avoid leaking <unordered_map> into this public header.
    ThemeMode current_{ThemeMode::light};
};

} // namespace nfui
