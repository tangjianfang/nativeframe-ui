#pragma once

#include <windows.h>

namespace nfui {

enum class ThemeMode {
    system,
    light,
    dark,
    high_contrast,
};

struct ThemeTokens {
    COLORREF window_background{};
    COLORREF window_text{};
    COLORREF accent{};
};

[[nodiscard]] ThemeTokens theme_tokens(ThemeMode mode) noexcept;

} // namespace nfui
