#include <nfui/Theme.hpp>

namespace nfui {

ThemeTokens theme_tokens(ThemeMode mode) noexcept {
    switch (mode) {
    case ThemeMode::dark:
        return ThemeTokens{RGB(32, 32, 32), RGB(240, 240, 240), RGB(64, 156, 255)};
    case ThemeMode::high_contrast:
        return ThemeTokens{RGB(0, 0, 0), RGB(255, 255, 255), RGB(255, 255, 0)};
    case ThemeMode::system:
    case ThemeMode::light:
    default:
        return ThemeTokens{RGB(255, 255, 255), RGB(32, 32, 32), RGB(0, 120, 215)};
    }
}

} // namespace nfui
