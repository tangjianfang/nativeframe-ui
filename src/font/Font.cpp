#include <nfui/Font.hpp>

namespace nfui {

int font_pixel_height(int point_size, int dpi) noexcept {
    if (point_size <= 0 || dpi <= 0) return 0;
    return MulDiv(point_size, dpi, 72);
}

FontCache::~FontCache() noexcept {
    if (regular_)  DeleteObject(regular_);
    if (semibold_) DeleteObject(semibold_);
    if (serif_)    DeleteObject(serif_);
    if (mono_)     DeleteObject(mono_);
}

HFONT FontCache::make(int dpi, int point_size, int weight, DWORD pitch_and_family,
                      const wchar_t* family) noexcept {
    const int px = font_pixel_height(point_size, dpi);
    if (px <= 0) return nullptr;
    return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, pitch_and_family, family);
}

void FontCache::rebuild(HFONT& slot, int& slot_dpi, int& slot_pt,
                        int dpi, int pt, int weight, DWORD pitch_and_family,
                        const wchar_t* family) noexcept {
    if (slot && slot_dpi == dpi && slot_pt == pt) return;
    if (slot) { DeleteObject(slot); slot = nullptr; }
    slot = make(dpi, pt, weight, pitch_and_family, family);
    slot_dpi = dpi;
    slot_pt = pt;
}

HFONT FontCache::regular(int dpi, int point_size) noexcept {
    rebuild(regular_, regular_dpi_, regular_pt_, dpi, point_size, FW_NORMAL,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    return regular_;
}
HFONT FontCache::semibold(int dpi, int point_size) noexcept {
    rebuild(semibold_, semibold_dpi_, semibold_pt_, dpi, point_size, FW_SEMIBOLD,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    return semibold_;
}
HFONT FontCache::serif(int dpi, int point_size) noexcept {
    rebuild(serif_, serif_dpi_, serif_pt_, dpi, point_size, FW_NORMAL,
            DEFAULT_PITCH | FF_ROMAN, L"Georgia");
    return serif_;
}
HFONT FontCache::mono(int dpi, int point_size) noexcept {
    rebuild(mono_, mono_dpi_, mono_pt_, dpi, point_size, FW_NORMAL,
            DEFAULT_PITCH | FF_MODERN, L"Cascadia Code");
    if (mono_ == nullptr) {
        rebuild(mono_, mono_dpi_, mono_pt_, dpi, point_size, FW_NORMAL,
                DEFAULT_PITCH | FF_MODERN, L"Consolas");
    }
    return mono_;
}

} // namespace nfui
