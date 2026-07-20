#include <nfui/Font.hpp>

namespace nfui {

int font_pixel_height(int point_size, int dpi) noexcept {
    if (point_size <= 0 || dpi <= 0) return 0;
    return MulDiv(point_size, dpi, 72);
}

FontCache::~FontCache() noexcept {
    if (regular_12_) DeleteObject(regular_12_);
    if (semibold_12_) DeleteObject(semibold_12_);
}

HFONT FontCache::make(int dpi, int point_size, int weight) noexcept {
    const int px = font_pixel_height(point_size, dpi);
    return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

HFONT FontCache::regular(int dpi, int point_size) noexcept {
    if (regular_12_ && cached_dpi_ == dpi) return regular_12_;
    if (regular_12_) { DeleteObject(regular_12_); regular_12_ = nullptr; }
    regular_12_ = make(dpi, point_size, FW_NORMAL);
    cached_dpi_ = dpi;
    return regular_12_;
}

HFONT FontCache::semibold(int dpi, int point_size) noexcept {
    if (semibold_12_ && cached_dpi_ == dpi) return semibold_12_;
    if (semibold_12_) { DeleteObject(semibold_12_); semibold_12_ = nullptr; }
    semibold_12_ = make(dpi, point_size, FW_SEMIBOLD);
    cached_dpi_ = dpi;
    return semibold_12_;
}

} // namespace nfui
