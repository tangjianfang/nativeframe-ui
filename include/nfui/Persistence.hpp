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

} // namespace nfui
