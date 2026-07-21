#pragma once

// Process-lifetime RAII singleton for the GDI+ runtime used by the chart
// renderers. Lives under charts_internal so it never leaks to other modules
// or into the public nfui surface — only nfui::initialize_chart_aa() /
// nfui::shutdown_chart_aa() touch it, exposed via <nfui/Charts.hpp>.
//
// Idempotent: multiple initialize() calls reuse the same token; shutdown()
// without a prior initialize() is a no-op. This matches Microsoft guidance
// for GdiplusStartup (paired start/stop on the same thread is fine; multiple
// startups must be balanced with shutdowns — hence the reference count).

#include <windows.h>

namespace nfui::charts_internal {

class GdiplusContext {
public:
    // Brings the GDI+ runtime up if it isn't already. Returns true when the
    // context is active and AA primitives may be used, false if startup
    // failed (in which case callers MUST fall back to the GDI primitives).
    [[nodiscard]] static bool initialize() noexcept;

    // Pairs with initialize(). Safe to call even when the context was never
    // started (or has already been shut down) — implements the natural
    // counter pattern Microsoft recommends for stacked GdiplusStartup calls.
    static void shutdown() noexcept;

    // Cheap positive test for the AA dispatch path. Never throws.
    [[nodiscard]] static bool active() noexcept;

    GdiplusContext(const GdiplusContext&) = delete;
    GdiplusContext& operator=(const GdiplusContext&) = delete;

private:
    GdiplusContext() = default;
    ~GdiplusContext() = default;

    // Token returned by GdiplusStartup; non-zero when active.
    static inline ULONG_PTR token_{0};
    // Reference count of balanced initialize()/shutdown() calls so nested
    // consumers (e.g. test + app) don't tear the context out from under
    // each other. Active iff token_ != 0.
    static inline unsigned long ref_count_{0};
};

} // namespace nfui::charts_internal
