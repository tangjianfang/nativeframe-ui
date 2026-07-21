#include <nfui/Controls/IconView.hpp>
#include <nfui/Paint.hpp>

namespace nfui {

bool IconView::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams p = params;
    p.style = WS_CHILD | WS_VISIBLE;
    return create_native(L"STATIC", p, SS_OWNERDRAW);
}

void IconView::set_icon(HICON icon) noexcept {
    icon_ = icon;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void IconView::on_paint(HDC dc, const PaintState& state) noexcept {
    if (icon_ == nullptr) return;
    const int w = state.bounds.right - state.bounds.left;
    const int h = state.bounds.bottom - state.bounds.top;
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const int ox = mem.valid() ? 0 : b.left;
    const int oy = mem.valid() ? 0 : b.top;
    DrawIconEx(target, ox, oy, icon_, w, h, 0, nullptr, DI_NORMAL);
}

} // namespace nfui