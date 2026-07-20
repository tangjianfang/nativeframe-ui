#include <nfui/Font.hpp>

namespace nfui {

int font_pixel_height(int point_size, int dpi) noexcept {
    if (point_size <= 0 || dpi <= 0) return 0;
    return MulDiv(point_size, dpi, 72);
}

FontCache::~FontCache() noexcept {
    if (regular_) DeleteObject(regular_);
    if (semibold_) DeleteObject(semibold_);
}

HFONT FontCache::make(int dpi, int point_size, int weight) noexcept {
    const int px = font_pixel_height(point_size, dpi);
    if (px <= 0) return nullptr;
    return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

HFONT FontCache::regular(int dpi, int point_size) noexcept {
    if (regular_ && regular_dpi_ == dpi && regular_pt_ == point_size) return regular_;
    if (regular_) { DeleteObject(regular_); regular_ = nullptr; }
    regular_ = make(dpi, point_size, FW_NORMAL);
    regular_dpi_ = dpi;
    regular_pt_ = point_size;
    return regular_;
}

HFONT FontCache::semibold(int dpi, int point_size) noexcept {
    if (semibold_ && semibold_dpi_ == dpi && semibold_pt_ == point_size) return semibold_;
    if (semibold_) { DeleteObject(semibold_); semibold_ = nullptr; }
    semibold_ = make(dpi, point_size, FW_SEMIBOLD);
    semibold_dpi_ = dpi;
    semibold_pt_ = point_size;
    return semibold_;
}

} // namespace nfui
