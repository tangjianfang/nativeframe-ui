#pragma once

#include <nfui/Diagnostics.hpp>
#include <nfui/Theme.hpp>

#include <string>

namespace nfui {

struct WorkbenchState {
    int main_x{};
    int main_y{};
    int main_width{1024};
    int main_height{768};
    bool maximized{};
    double left_splitter_ratio{0.25};
    double right_splitter_ratio{0.75};
    int active_tab{};
    ThemeMode theme{ThemeMode::system};
};

[[nodiscard]] std::wstring encode_workbench_state(const WorkbenchState& state);
[[nodiscard]] Result<WorkbenchState> decode_workbench_state(std::wstring_view text);

// SettingsDemo persistence. Stores user preferences as a single-line text
// blob suitable for WriteFile / ReadFile round-tripping.
struct SettingsState {
    std::wstring profile_name{L"NativeFrame UI"};
    std::wstring workspace_root{L"C:\\nativeframeui\\workspace"};
    int theme_index{2}; // 0=Light, 1=Dark, 2=System
    bool auto_save{true};
    bool splash{true};
    bool verbose{false};
    int selected_category{0};
};

[[nodiscard]] std::wstring encode_settings_state(const SettingsState& state);
[[nodiscard]] Result<SettingsState> decode_settings_state(std::wstring_view text);

// File-based persistence helpers. The path is created (including parent
// directories) if it does not exist. Returns false on I/O failure.
bool save_state_to_file(const std::wstring& path, const std::wstring& content) noexcept;
[[nodiscard]] Result<std::wstring> load_state_from_file(const std::wstring& path) noexcept;

// Returns %APPDATA%\NativeFrameUI\<filename>. Empty string on failure.
[[nodiscard]] std::wstring appdata_path(const wchar_t* filename) noexcept;

} // namespace nfui
