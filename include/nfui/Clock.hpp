#pragma once

#include <windows.h>

namespace nfui {

// CP17: injectable monotonic millisecond clock. Animation logic reads "now"
// through this interface so the pure state machines (ColorAnimation, palette
// cross-fade) can be driven by a fake clock in unit tests without a message
// loop. The production clock is Win32Clock (GetTickCount64).
//
// The default clock used when no clock is injected is a stateless function-local
// Win32Clock — a utility, not a mutable singleton (cf. theme_palette / dpi_of).

class Clock {
public:
    virtual ~Clock() = default;
    [[nodiscard]] virtual unsigned long long now_ms() const noexcept = 0;
};

class Win32Clock : public Clock {
public:
    [[nodiscard]] unsigned long long now_ms() const noexcept override {
        return GetTickCount64();
    }
};

} // namespace nfui