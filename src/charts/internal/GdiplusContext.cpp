// Implementation for GdiplusContext. Lives in src/charts/internal so the
// gdiplus dependency stays private to the nfui_charts library — exposing
// <gdiplus.h> here would leak GDI+ types through the public surface and
// force every consumer of NativeFrameUI::nfui_charts to link gdiplus too.

#include "GdiplusContext.hpp"

// gdiplus.h requires COM interface declarations (IStream, HRESULT, ...)
// which WIN32_LEAN_AND_MEAN strips from <windows.h>.
#include <objbase.h>
#include <gdiplus.h>

namespace nfui::charts_internal {

bool GdiplusContext::initialize() noexcept {
    if (token_ != 0) {
        // Already up; bump the ref count so paired shutdown() doesn't
        // tear the context out from under us.
        ++ref_count_;
        return true;
    }
    // Default GdiplusStartupInput spins a background thread that processes
    // graphics work asynchronously. We never feed it heavy work — chart
    // paints complete on the UI thread and let the foreground Graphics
    // finish before they hand control back — so the background thread is
    // pure overhead. The basic input struct is sufficient for that.
    Gdiplus::GdiplusStartupInput input{};
    input.GdiplusVersion = 1;
    ULONG_PTR token = 0;
    const Gdiplus::Status status = Gdiplus::GdiplusStartup(&token, &input, nullptr);
    if (status != Gdiplus::Status::Ok) {
        return false;
    }
    token_ = token;
    ref_count_ = 1;
    return true;
}

void GdiplusContext::shutdown() noexcept {
    if (token_ == 0) {
        // Either never started or already torn down; nothing to do.
        return;
    }
    if (ref_count_ > 1) {
        --ref_count_;
        return;
    }
    Gdiplus::GdiplusShutdown(token_);
    token_ = 0;
    ref_count_ = 0;
}

bool GdiplusContext::active() noexcept {
    return token_ != 0;
}

} // namespace nfui::charts_internal
