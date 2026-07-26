#include <nfui/Persistence.hpp>

#include <cwchar>
#include <sstream>
#include <shlobj.h>

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

// --- SettingsState encode/decode -------------------------------------------
// Format: "NFUIS1 <theme_index> <auto_save> <splash> <verbose> <category>
//          <profile_name_b64> <workspace_root_b64>"
// Strings are encoded as length-prefixed hex to avoid delimiter collisions.

namespace {

std::wstring encode_string_field(const std::wstring& value) {
    std::wstring out;
    out.reserve(value.size() * 5 + 16);
    wchar_t lenbuf[24]{};
    std::swprintf(lenbuf, std::size(lenbuf), L"%zu:", value.size());
    out.append(lenbuf);

    // Each wchar_t is encoded as exactly 4 uppercase hex digits. %04X formats
    // an unsigned int with zero-padded uppercase hex — matching the format
    // consumed by decode_string_field.
    wchar_t hexbuf[5]{};
    for (wchar_t ch : value) {
        std::swprintf(hexbuf, std::size(hexbuf), L"%04X", static_cast<unsigned>(ch));
        out.append(hexbuf, 4);
    }
    return out;
}

bool decode_string_field(std::wistringstream& stream, std::wstring& out) {
    std::size_t length = 0;
    wchar_t colon = 0;
    stream >> length >> colon;
    if (colon != L':' || length > 4096) return false;
    out.clear();
    out.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        wchar_t hex_buf[5]{};
        stream.read(hex_buf, 4);
        if (stream.gcount() != 4) return false;
        unsigned code = 0;
        for (int d = 0; d < 4; ++d) {
            code <<= 4;
            wchar_t c = hex_buf[d];
            if (c >= L'0' && c <= L'9') code += static_cast<unsigned>(c - L'0');
            else if (c >= L'A' && c <= L'F') code += static_cast<unsigned>(c - L'A' + 10);
            else if (c >= L'a' && c <= L'f') code += static_cast<unsigned>(c - L'a' + 10);
            else return false;
        }
        out.push_back(static_cast<wchar_t>(code));
    }
    return true;
}

} // namespace

std::wstring encode_settings_state(const SettingsState& state) {
    std::wostringstream stream;
    stream << L"NFUIS1 "
           << state.theme_index << L' '
           << (state.auto_save ? 1 : 0) << L' '
           << (state.splash ? 1 : 0) << L' '
           << (state.verbose ? 1 : 0) << L' '
           << state.selected_category << L' '
           << encode_string_field(state.profile_name) << L' '
           << encode_string_field(state.workspace_root);
    return stream.str();
}

Result<SettingsState> decode_settings_state(std::wstring_view text) {
    std::wstring owned(text);
    std::wistringstream stream(owned);
    std::wstring magic;
    SettingsState state{};

    if (!(stream >> magic) || magic != L"NFUIS1") {
        return Result<SettingsState>::failure(Error(ErrorCode::invalid_argument, L"Invalid settings state header"));
    }

    int auto_save = 0, splash = 0, verbose = 0;
    if (!(stream >> state.theme_index >> auto_save >> splash >> verbose >> state.selected_category)) {
        return Result<SettingsState>::failure(Error(ErrorCode::invalid_argument, L"Invalid settings state fields"));
    }

    if (!decode_string_field(stream, state.profile_name) ||
        !decode_string_field(stream, state.workspace_root)) {
        return Result<SettingsState>::failure(Error(ErrorCode::invalid_argument, L"Invalid settings string fields"));
    }

    if (state.theme_index < 0 || state.theme_index > 2 || state.selected_category < 0) {
        return Result<SettingsState>::failure(Error(ErrorCode::invalid_argument, L"Invalid settings state values"));
    }

    state.auto_save = auto_save != 0;
    state.splash = splash != 0;
    state.verbose = verbose != 0;
    return Result<SettingsState>::success(state);
}

// --- File I/O helpers -------------------------------------------------------

std::wstring appdata_path(const wchar_t* filename) noexcept {
    wchar_t* appdata = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        return {};
    }
    std::wstring dir(appdata);
    CoTaskMemFree(appdata);
    dir += L"\\NativeFrameUI";
    // Ensure the directory exists.
    CreateDirectoryW(dir.c_str(), nullptr);
    dir += L'\\';
    dir += filename;
    return dir;
}

bool save_state_to_file(const std::wstring& path, const std::wstring& content) noexcept {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const DWORD size = static_cast<DWORD>(content.size() * sizeof(wchar_t));
    const bool ok = WriteFile(file, content.data(), size, &written, nullptr) && written == size;
    CloseHandle(file);
    return ok;
}

Result<std::wstring> load_state_from_file(const std::wstring& path) noexcept {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return Result<std::wstring>::failure(
            Error::from_win32(GetLastError(), L"Cannot open state file"));
    }
    const DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0 || size > 1024 * 1024) {
        CloseHandle(file);
        return Result<std::wstring>::failure(
            Error(ErrorCode::invalid_state, L"Invalid state file size"));
    }
    std::wstring buffer(size / sizeof(wchar_t), L'\0');
    DWORD bytes_read = 0;
    if (!ReadFile(file, buffer.data(), size, &bytes_read, nullptr) || bytes_read != size) {
        CloseHandle(file);
        return Result<std::wstring>::failure(
            Error::from_win32(GetLastError(), L"Cannot read state file"));
    }
    CloseHandle(file);
    buffer.resize(bytes_read / sizeof(wchar_t));
    return Result<std::wstring>::success(std::move(buffer));
}

} // namespace nfui
