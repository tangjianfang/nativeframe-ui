#pragma once

#include <iostream>
#include <string_view>

namespace nfui_test {

inline bool expect(bool condition, std::wstring_view message) {
    if (!condition) {
        std::wcerr << L"FAIL: " << message << L'\n';
        return false;
    }
    return true;
}

} // namespace nfui_test
