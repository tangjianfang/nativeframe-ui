#pragma once

#include <windows.h>

#include <utility>

namespace nfui {

[[nodiscard]] int icon_pixel_size(int logical_size, int dpi) noexcept;

class IconHandle {
public:
    IconHandle() noexcept = default;
    explicit IconHandle(HICON icon) noexcept : icon_(icon) {}
    ~IconHandle() noexcept { reset(); }
    IconHandle(const IconHandle&) = delete;
    IconHandle& operator=(const IconHandle&) = delete;
    IconHandle(IconHandle&& o) noexcept : icon_(o.release()) {}
    IconHandle& operator=(IconHandle&& o) noexcept { if (this != &o) reset(o.release()); return *this; }
    [[nodiscard]] HICON get() const noexcept { return icon_; }
    [[nodiscard]] bool valid() const noexcept { return icon_ != nullptr; }
    [[nodiscard]] HICON release() noexcept { return std::exchange(icon_, nullptr); }
    void reset(HICON icon = nullptr) noexcept { if (icon_) DestroyIcon(icon_); icon_ = icon; }
private:
    HICON icon_{};
};

// resource_id is a MAKEINTRESOURCEW-wrapped id. Returns null on failure (no throw).
[[nodiscard]] HICON load_scaled_icon(HINSTANCE instance, LPCWSTR resource_id, int logical_size, int dpi) noexcept;

} // namespace nfui
