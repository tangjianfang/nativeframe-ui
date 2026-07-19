#pragma once

#include <utility>
#include <windows.h>

namespace nfui {

class BorrowedHwnd {
public:
    explicit BorrowedHwnd(HWND hwnd) noexcept
        : hwnd_(hwnd) {
    }

    [[nodiscard]] HWND hwnd() const noexcept {
        return hwnd_;
    }

    [[nodiscard]] bool valid() const noexcept {
        return hwnd_ != nullptr;
    }

private:
    HWND hwnd_{};
};

class OwnedHwnd {
public:
    OwnedHwnd() = default;

    explicit OwnedHwnd(HWND hwnd) noexcept
        : hwnd_(hwnd) {
    }

    ~OwnedHwnd() noexcept {
        reset();
    }

    OwnedHwnd(const OwnedHwnd&) = delete;
    OwnedHwnd& operator=(const OwnedHwnd&) = delete;

    OwnedHwnd(OwnedHwnd&& other) noexcept
        : hwnd_(std::exchange(other.hwnd_, nullptr)) {
    }

    OwnedHwnd& operator=(OwnedHwnd&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.hwnd_, nullptr));
        }
        return *this;
    }

    [[nodiscard]] HWND hwnd() const noexcept {
        return hwnd_;
    }

    [[nodiscard]] bool valid() const noexcept {
        return hwnd_ != nullptr;
    }

    [[nodiscard]] HWND release() noexcept {
        return std::exchange(hwnd_, nullptr);
    }

    void reset(HWND hwnd = nullptr) noexcept {
        if (hwnd_ != nullptr) {
            DestroyWindow(hwnd_);
        }
        hwnd_ = hwnd;
    }

private:
    HWND hwnd_{};
};

} // namespace nfui
