#pragma once

#include <string_view>
#include <windows.h>

namespace nfui {

struct ApplicationConfig {
    HINSTANCE instance{};
    int show_command{SW_SHOWNORMAL};
};

class Application {
public:
    explicit Application(ApplicationConfig config) noexcept;

    [[nodiscard]] HINSTANCE instance() const noexcept;
    [[nodiscard]] int show_command() const noexcept;

    [[nodiscard]] static bool initialize_process_dpi() noexcept;
    [[nodiscard]] static bool initialize_common_controls() noexcept;

    int run() noexcept;

private:
    ApplicationConfig config_{};
};

[[nodiscard]] std::wstring_view version() noexcept;

} // namespace nfui
