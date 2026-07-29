#include <nfui/ThemeBroker.hpp>

#include <unordered_map>
#include <utility>
#include <vector>
#include <mutex>

namespace nfui {

namespace {

// TU-local storage so this header doesn't drag <unordered_map> / <mutex>
// into every translation unit that includes ThemeBroker.hpp. The mutex
// guards against cross-thread register calls; broadcast() snapshots
// under the lock then invokes handlers outside it so a handler is free
// to call register_hwnd / unregister_hwnd without deadlocking.
struct Registry {
    std::unordered_map<HWND, ThemeChangeHandler> entries;
    std::mutex mutex;
};

Registry& registry() noexcept {
    static Registry r;
    return r;
}

} // namespace

ThemeBroker& ThemeBroker::instance() noexcept {
    static ThemeBroker broker;     // Magic Statics — thread-safe init.
    return broker;
}

void ThemeBroker::set_theme(ThemeMode mode) noexcept {
    if (mode == current_) return;  // idempotent — skip the broadcast.
    current_ = mode;
    broadcast();
}

void ThemeBroker::register_hwnd(HWND hwnd, ThemeChangeHandler handler) noexcept {
    if (hwnd == nullptr) return;
    auto& r = registry();
    std::lock_guard lock(r.mutex);
    r.entries[hwnd] = std::move(handler);
}

void ThemeBroker::unregister_hwnd(HWND hwnd) noexcept {
    if (hwnd == nullptr) return;
    auto& r = registry();
    std::lock_guard lock(r.mutex);
    r.entries.erase(hwnd);
}

void ThemeBroker::broadcast() noexcept {
    auto& r = registry();
    // Snapshot under lock; invoke outside the lock so handlers may register
    // / unregister without deadlocking.
    std::vector<std::pair<HWND, ThemeChangeHandler>> snapshot;
    {
        std::lock_guard lock(r.mutex);
        snapshot.reserve(r.entries.size());
        for (auto& entry : r.entries) {
            snapshot.emplace_back(entry.first, entry.second);
        }
    }
    const ThemeMode mode = current_;
    for (auto& entry : snapshot) {
        const HWND hwnd = entry.first;
        if (!IsWindow(hwnd)) {
            // The HWND was destroyed without an unregister() call (e.g. a
            // child window of an HWND we don't own). Drop it silently and
            // continue — the next register with the same handle will refresh.
            unregister_hwnd(hwnd);
            continue;
        }
        // Native control-level reflect: a control's own subclass_proc picks
        // this up (see Edit::visual_subclass_proc, the CP-A2/A3 controls
        // follow the same pattern) and invalidates itself. Custom Window
        // subclasses override on_theme_changed() instead.
        SendMessageW(hwnd, WM_THEMECHANGED, 0, 0);
        if (entry.second) entry.second(mode);
    }
}

} // namespace nfui
