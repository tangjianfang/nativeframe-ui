#include <nfui/Persistence.hpp>

#include <cwchar>
#include <sstream>

namespace nfui {
namespace {

int theme_to_int(ThemeMode mode) noexcept {
    return static_cast<int>(mode);
}

ThemeMode int_to_theme(int value) noexcept {
    if (value < 0 || value > static_cast<int>(ThemeMode::high_contrast)) {
        return ThemeMode::system;
    }
    return static_cast<ThemeMode>(value);
}

bool valid_ratio(double ratio) noexcept {
    return ratio >= 0.0 && ratio <= 1.0;
}

} // namespace

std::wstring encode_workbench_state(const WorkbenchState& state) {
    std::wostringstream stream;
    stream << L"NFUI1 "
           << state.main_x << L' '
           << state.main_y << L' '
           << state.main_width << L' '
           << state.main_height << L' '
           << (state.maximized ? 1 : 0) << L' '
           << state.left_splitter_ratio << L' '
           << state.right_splitter_ratio << L' '
           << state.active_tab << L' '
           << theme_to_int(state.theme);
    return stream.str();
}

Result<WorkbenchState> decode_workbench_state(std::wstring_view text) {
    std::wstring owned(text);
    std::wistringstream stream(owned);
    std::wstring magic;
    int maximized = 0;
    int theme = 0;
    WorkbenchState state{};

    if (!(stream >> magic) || magic != L"NFUI1") {
        return Result<WorkbenchState>::failure(Error(ErrorCode::invalid_argument, L"Invalid workbench state header"));
    }

    if (!(stream >> state.main_x >> state.main_y >> state.main_width >> state.main_height >> maximized >>
          state.left_splitter_ratio >> state.right_splitter_ratio >> state.active_tab >> theme)) {
        return Result<WorkbenchState>::failure(Error(ErrorCode::invalid_argument, L"Invalid workbench state fields"));
    }

    if (state.main_width <= 0 || state.main_height <= 0 ||
        !valid_ratio(state.left_splitter_ratio) || !valid_ratio(state.right_splitter_ratio) ||
        state.active_tab < 0) {
        return Result<WorkbenchState>::failure(Error(ErrorCode::invalid_argument, L"Invalid workbench state values"));
    }

    state.maximized = maximized != 0;
    state.theme = int_to_theme(theme);
    return Result<WorkbenchState>::success(state);
}

} // namespace nfui
