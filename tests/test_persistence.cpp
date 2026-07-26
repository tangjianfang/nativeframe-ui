// Pure-logic unit tests for the nfui persistence module (no HWND, no message loop).
#include "test_helpers.hpp"

#include <nfui/Persistence.hpp>

#include <string>

int main() {
    bool ok = true;

    // --- WorkbenchState round-trip ---
    {
        nfui::WorkbenchState state{};
        state.main_x = 100;
        state.main_y = 200;
        state.main_width = 1280;
        state.main_height = 800;
        state.maximized = true;
        state.left_splitter_ratio = 0.3;
        state.right_splitter_ratio = 0.7;
        state.active_tab = 2;
        state.theme = nfui::ThemeMode::dark;

        const std::wstring encoded = nfui::encode_workbench_state(state);
        auto decoded = nfui::decode_workbench_state(encoded);
        ok = nfui_test::expect(decoded.has_value(), L"WorkbenchState round-trip decodes") && ok;
        if (decoded.has_value()) {
            const auto& s = decoded.value();
            ok = nfui_test::expect(s.main_x == 100, L"WorkbenchState preserves main_x") && ok;
            ok = nfui_test::expect(s.main_width == 1280, L"WorkbenchState preserves main_width") && ok;
            ok = nfui_test::expect(s.maximized, L"WorkbenchState preserves maximized") && ok;
            ok = nfui_test::expect(s.active_tab == 2, L"WorkbenchState preserves active_tab") && ok;
            ok = nfui_test::expect(s.theme == nfui::ThemeMode::dark, L"WorkbenchState preserves theme") && ok;
        }
    }

    // --- WorkbenchState rejects corrupt input ---
    ok = nfui_test::expect(!nfui::decode_workbench_state(L"garbage").has_value(),
                           L"WorkbenchState rejects corrupt header") && ok;
    ok = nfui_test::expect(!nfui::decode_workbench_state(L"NFUI1 0 0 -1 0 0 0.5 0.5 0 0").has_value(),
                           L"WorkbenchState rejects negative width") && ok;

    // --- SettingsState round-trip ---
    {
        nfui::SettingsState state{};
        state.profile_name = L"My Profile";
        state.workspace_root = L"D:\\Projects\\test";
        state.theme_index = 1;
        state.auto_save = false;
        state.splash = true;
        state.verbose = true;
        state.selected_category = 3;

        const std::wstring encoded = nfui::encode_settings_state(state);
        auto decoded = nfui::decode_settings_state(encoded);
        ok = nfui_test::expect(decoded.has_value(), L"SettingsState round-trip decodes") && ok;
        if (decoded.has_value()) {
            const auto& s = decoded.value();
            ok = nfui_test::expect(s.profile_name == L"My Profile", L"SettingsState preserves profile_name") && ok;
            ok = nfui_test::expect(s.workspace_root == L"D:\\Projects\\test", L"SettingsState preserves workspace_root") && ok;
            ok = nfui_test::expect(s.theme_index == 1, L"SettingsState preserves theme_index") && ok;
            ok = nfui_test::expect(!s.auto_save, L"SettingsState preserves auto_save=false") && ok;
            ok = nfui_test::expect(s.splash, L"SettingsState preserves splash=true") && ok;
            ok = nfui_test::expect(s.verbose, L"SettingsState preserves verbose=true") && ok;
            ok = nfui_test::expect(s.selected_category == 3, L"SettingsState preserves selected_category") && ok;
        }
    }

    // --- SettingsState with Unicode strings ---
    {
        nfui::SettingsState state{};
        state.profile_name = L"\u6d4b\u8bd5\u914d\u7f6e"; // 测试配置
        state.workspace_root = L"C:\\\u7528\u6237\\\u6587\u6863"; // C:\用户\文档
        state.theme_index = 0;
        state.auto_save = true;
        state.splash = false;
        state.verbose = false;
        state.selected_category = 0;

        const std::wstring encoded = nfui::encode_settings_state(state);
        auto decoded = nfui::decode_settings_state(encoded);
        ok = nfui_test::expect(decoded.has_value(), L"SettingsState Unicode round-trip decodes") && ok;
        if (decoded.has_value()) {
            ok = nfui_test::expect(decoded.value().profile_name == L"\u6d4b\u8bd5\u914d\u7f6e",
                                   L"SettingsState preserves Chinese profile_name") && ok;
            ok = nfui_test::expect(decoded.value().workspace_root == L"C:\\\u7528\u6237\\\u6587\u6863",
                                   L"SettingsState preserves Chinese workspace_root") && ok;
        }
    }

    // --- SettingsState rejects corrupt input ---
    ok = nfui_test::expect(!nfui::decode_settings_state(L"bad").has_value(),
                           L"SettingsState rejects corrupt header") && ok;
    ok = nfui_test::expect(!nfui::decode_settings_state(L"NFUIS1 5 0 0 0 0 0: 0:").has_value(),
                           L"SettingsState rejects out-of-range theme_index") && ok;

    // --- appdata_path returns non-empty ---
    {
        const std::wstring path = nfui::appdata_path(L"test.dat");
        ok = nfui_test::expect(!path.empty(), L"appdata_path returns non-empty") && ok;
        ok = nfui_test::expect(path.find(L"NativeFrameUI") != std::wstring::npos,
                               L"appdata_path contains NativeFrameUI directory") && ok;
    }

    return ok ? 0 : 1;
}
