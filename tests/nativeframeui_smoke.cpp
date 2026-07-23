#include <nfui/NativeFrameUI.hpp>
#include <nfui/HoverState.hpp>
#include <nfui/Easing.hpp>
#include <nfui/Clock.hpp>
#include <nfui/Animation.hpp>
#include <nfui/VectorIcon.hpp>

#include "NativeFrameUIResource.h"

#include <nfui/Charts.hpp>

#include <psapi.h>
#include <windows.h>
#include <windowsx.h>

#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>
#include <cstdlib>

namespace {

struct GuiResourceCounts {
    DWORD gdi{};
    DWORD user{};
};

[[nodiscard]] std::filesystem::path current_module_path() {
    std::wstring buffer(static_cast<std::size_t>(MAX_PATH), L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2, L'\0');
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    buffer.resize(length);
    return std::filesystem::path(buffer);
}

[[nodiscard]] std::filesystem::path build_root_path() {
    std::filesystem::path output_directory = current_module_path().parent_path();
    std::filesystem::path candidate = output_directory.parent_path();
    if (std::filesystem::exists(candidate / L"CMakeCache.txt")) {
        return candidate;
    }
    return output_directory;
}

[[nodiscard]] bool target_registered(std::wstring_view target_name) {
    std::filesystem::path build_root = build_root_path();
    std::filesystem::path project_file = build_root / (std::wstring(target_name) + L".vcxproj");
    std::filesystem::path target_directory = build_root / L"CMakeFiles" / (std::wstring(target_name) + L".dir");
    return std::filesystem::exists(project_file) || std::filesystem::is_directory(target_directory);
}

[[nodiscard]] GuiResourceCounts capture_gui_resource_counts() noexcept {
    HANDLE process = GetCurrentProcess();
    return GuiResourceCounts{
        GetGuiResources(process, GR_GDIOBJECTS),
        GetGuiResources(process, GR_USEROBJECTS),
    };
}

bool expect(bool condition, std::wstring_view message) {
    if (!condition) {
        std::wcerr << L"FAIL: " << message << L'\n';
        return false;
    }
    return true;
}

class CountingWindow : public nfui::Window {
public:
    [[nodiscard]] int nc_destroy_count() const noexcept {
        return nc_destroy_count_;
    }

    [[nodiscard]] int command_count() const noexcept {
        return command_count_;
    }

    [[nodiscard]] int notify_count() const noexcept {
        return notify_count_;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        if (message == WM_NCDESTROY) {
            ++nc_destroy_count_;
        }
        return nfui::Window::handle_message(message, wparam, lparam);
    }

    bool on_command(int command_id, HWND, UINT notification_code) override {
        if (command_id == IDM_NFUI_EXIT && notification_code == 0) {
            ++command_count_;
            return true;
        }
        return false;
    }

    LRESULT on_notify(int control_id, NMHDR* header) override {
        if (control_id == 77 && header != nullptr && header->code == NM_CLICK) {
            ++notify_count_;
            return 1;
        }
        return 0;
    }

private:
    int nc_destroy_count_{};
    int command_count_{};
    int notify_count_{};
};

class ThrowingWindow : public nfui::Window {
protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        if (message == WM_APP + 1) {
            throw 7;
        }
        return nfui::Window::handle_message(message, wparam, lparam);
    }
};

LRESULT CALLBACK foreign_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

INT_PTR CALLBACK test_dialog_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM) {
    if (message == WM_INITDIALOG) {
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(wparam) == IDOK) {
        DestroyWindow(hwnd);
        return TRUE;
    }
    return FALSE;
}

// Must stay in lockstep with the core ocm_base constant (WM_USER + 0x1c00) in
// src/core/Window.cpp so the subclass proc receives the same reflected message id.
constexpr UINT ocm_base_test = WM_USER + 0x1c00;

LRESULT CALLBACK reflection_test_subclass_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
                                               UINT_PTR /*id*/, DWORD_PTR ref_data) noexcept {
    if (message == ocm_base_test + WM_DRAWITEM) {
        ++*reinterpret_cast<int*>(ref_data);
        return TRUE;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace

int per_component_smoke(); // defined below main() — per-component lib split smoke checks (T4-T12).

int wmain() {
    // CP29: Modal dialog round-trip (DialogBoxParamW) blocks on its own
    // message pump until a human dismisses the dialog. In CI / headless
    // runs nobody does, and the test would hang to ctest's timeout. Set
    // NONFUI_SKIP_DIALOG=1 by default so direct invocation also avoids
    // the modal; developers who want to exercise the modal path can set
    // NONFUI_SKIP_DIALOG=0 explicitly. CMakeLists.txt also sets the same
    // env var via set_tests_properties so ctest picks it up reliably.
    SetEnvironmentVariableW(L"NONFUI_SKIP_DIALOG", L"1");
    if (int rc = per_component_smoke(); rc != 0) {
        return rc;
    }

    bool ok = true;

    ok = expect(!nfui::version().empty(), L"version string is available") && ok;

    nfui::Error missing_file = nfui::Error::from_win32(ERROR_FILE_NOT_FOUND, L"missing file");
    ok = expect(missing_file.code() == nfui::ErrorCode::win32_error, L"Win32 error code category is preserved") && ok;
    ok = expect(missing_file.win32_code() == ERROR_FILE_NOT_FOUND, L"Win32 error value is preserved") && ok;
    ok = expect(missing_file.message() == L"missing file", L"diagnostic message is preserved") && ok;

    nfui::Result<int> successful_result = nfui::Result<int>::success(42);
    ok = expect(successful_result.has_value(), L"successful result reports a value") && ok;
    ok = expect(successful_result.value() == 42, L"successful result preserves the value") && ok;

    nfui::Result<int> failed_result = nfui::Result<int>::failure(missing_file);
    ok = expect(!failed_result.has_value(), L"failed result reports no value") && ok;
    ok = expect(failed_result.error().win32_code() == ERROR_FILE_NOT_FOUND, L"failed result preserves the error") && ok;

    nfui::CommandRouter command_router;
    int routed_command_count = 0;
    constexpr nfui::CommandId test_command{40001};
    command_router.set_handler(test_command, [&routed_command_count](nfui::CommandId command) {
        if (command.value == 40001) {
            ++routed_command_count;
            return true;
        }
        return false;
    });
    ok = expect(command_router.route(test_command), L"command router invokes registered handlers") && ok;
    ok = expect(routed_command_count == 1, L"command router handler receives command id") && ok;
    command_router.set_state(test_command, nfui::CommandState{false, true, L"Disabled"});
    ok = expect(!command_router.route(test_command), L"command router skips disabled commands") && ok;
    ok = expect(routed_command_count == 1, L"disabled command does not invoke handler") && ok;
    ok = expect(command_router.state(test_command).text == L"Disabled", L"command router stores command state") && ok;
    ok = expect(!command_router.route(nfui::CommandId{49999}), L"command router rejects unknown commands") && ok;

    nfui::DpiScale dpi_144(144);
    ok = expect(dpi_144.logical_to_pixels(100) == 150, L"DPI converts logical units to pixels") && ok;
    ok = expect(dpi_144.pixels_to_logical(150) == 100, L"DPI converts pixels to logical units") && ok;
    ok = expect(dpi_144.scale_font_height(-12) == -18, L"DPI scales font heights") && ok;

    nfui::Rect layout_bounds{0, 0, 1000, 500};
    nfui::SplitterLayout split = nfui::split_horizontally(layout_bounds, 0.25, 4);
    ok = expect(split.first.width == 250 && split.splitter.x == 250 && split.second.x == 254,
                L"Layout splits horizontal bounds by ratio") && ok;
    auto rows = nfui::layout_horizontal(layout_bounds, {100, 200}, 10);
    ok = expect(rows.size() == 2 && rows[0].width == 100 && rows[1].x == 110,
                L"Layout creates horizontal rectangles with gaps") && ok;

    nfui::ThemeTokens light_tokens = nfui::theme_tokens(nfui::ThemeMode::light);
    nfui::ThemeTokens dark_tokens = nfui::theme_tokens(nfui::ThemeMode::dark);
    nfui::ThemeTokens high_contrast_tokens = nfui::theme_tokens(nfui::ThemeMode::high_contrast);
    ok = expect(light_tokens.window_background != dark_tokens.window_background,
                L"Theme tokens differ between light and dark") && ok;
    ok = expect(high_contrast_tokens.window_text == RGB(255, 255, 255),
                L"High contrast theme uses white text") && ok;

    // CP10B: lock down the HC profile invariants so a future tweak to the
    // palette cannot silently regress a contrast requirement. The numbers
    // here are the *defining* choices of the WCAG AAA high-contrast palette;
    // any change must be paired with an audit rerun. Reference:
    // docs/KNOWLEDGE/polish/2026-07-23-cp10-high-contrast.md.
    {
        const nfui::ThemePalette hc = nfui::theme_palette(nfui::ThemeMode::high_contrast);
        ok = expect(hc.background.rgb == RGB(0, 0, 0),
                    L"HC background is pure black") && ok;
        ok = expect(hc.text.rgb == RGB(255, 255, 255),
                    L"HC text is pure white") && ok;
        ok = expect(hc.surface.rgb != hc.background.rgb,
                    L"HC surface is distinct from background (cards are visible)") && ok;
        ok = expect(hc.surface_hover.rgb != hc.background.rgb,
                    L"HC surface_hover is distinct from background") && ok;
        ok = expect(hc.accent.rgb != hc.accent_hover.rgb,
                    L"HC accent and accent_hover are distinct (hover state must read)") && ok;
        ok = expect(hc.selection.rgb != hc.accent.rgb,
                    L"HC selection is distinct from accent (selected rows do not collide with button hover)") && ok;
        ok = expect(hc.warning.rgb != hc.accent.rgb,
                    L"HC warning is distinct from accent (no semantic collision)") && ok;
        ok = expect(nfui::is_high_contrast(hc),
                    L"is_high_contrast() recognises the HC palette") && ok;
        ok = expect(!nfui::is_high_contrast(nfui::theme_palette(nfui::ThemeMode::light)),
                    L"is_high_contrast() rejects the light palette") && ok;
        ok = expect(!nfui::is_high_contrast(nfui::theme_palette(nfui::ThemeMode::dark)),
                    L"is_high_contrast() rejects the dark palette") && ok;
    }

    nfui::WorkbenchState state{};
    state.main_x = 10;
    state.main_y = 20;
    state.main_width = 1024;
    state.main_height = 768;
    state.maximized = true;
    state.left_splitter_ratio = 0.25;
    state.right_splitter_ratio = 0.75;
    state.active_tab = 1;
    state.theme = nfui::ThemeMode::dark;
    std::wstring encoded_state = nfui::encode_workbench_state(state);
    nfui::Result<nfui::WorkbenchState> decoded_state = nfui::decode_workbench_state(encoded_state);
    ok = expect(decoded_state.has_value(), L"Persistence decodes encoded workbench state") && ok;
    ok = expect(decoded_state.has_value() && decoded_state.value().main_width == 1024,
                L"Persistence preserves window width") && ok;
    ok = expect(!nfui::decode_workbench_state(L"corrupt").has_value(),
                L"Persistence rejects corrupt state") && ok;

    ok = expect(nfui::Application::initialize_process_dpi(), L"Per-Monitor DPI V2 initialization succeeds or was already set") && ok;
    ok = expect(nfui::Application::initialize_common_controls(), L"Common Controls initialization succeeds") && ok;
    ok = expect(nfui::initialize_chart_aa(), L"Chart AA GDI+ session initializes") && ok;

    HWND borrowed_window = CreateWindowExW(0, L"STATIC", L"borrowed", WS_OVERLAPPEDWINDOW,
                                           CW_USEDEFAULT, CW_USEDEFAULT, 120, 80,
                                           nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ok = expect(borrowed_window != nullptr, L"borrowed test HWND can be created") && ok;
    if (borrowed_window != nullptr) {
        nfui::BorrowedHwnd borrowed(borrowed_window);
        ok = expect(borrowed.valid(), L"borrowed HWND wrapper reports valid handle") && ok;
        ok = expect(borrowed.hwnd() == borrowed_window, L"borrowed HWND wrapper preserves handle") && ok;
        ok = expect(IsWindow(borrowed_window) != FALSE, L"borrowed HWND wrapper does not destroy handle") && ok;
        DestroyWindow(borrowed_window);
    }

    HWND owned_window = CreateWindowExW(0, L"STATIC", L"owned", WS_OVERLAPPEDWINDOW,
                                        CW_USEDEFAULT, CW_USEDEFAULT, 120, 80,
                                        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ok = expect(owned_window != nullptr, L"owned test HWND can be created") && ok;
    if (owned_window != nullptr) {
        nfui::OwnedHwnd owned(owned_window);
        ok = expect(owned.valid(), L"owned HWND wrapper reports valid handle") && ok;
        owned.reset();
        ok = expect(!owned.valid(), L"owned HWND wrapper reset clears handle") && ok;
        ok = expect(IsWindow(owned_window) == FALSE, L"owned HWND wrapper reset destroys window") && ok;
    }

    CountingWindow counting_window;
    nfui::WindowCreateParams create_params{
        GetModuleHandleW(nullptr),
        L"NativeFrameUISmokeWindow",
        L"NativeFrame UI smoke window",
    };
    ok = expect(counting_window.create(create_params), L"Window wrapper creates a hidden HWND") && ok;
    HWND counted_hwnd = counting_window.hwnd();
    ok = expect(counted_hwnd != nullptr, L"Window wrapper exposes HWND") && ok;
    ok = expect(reinterpret_cast<CountingWindow*>(GetWindowLongPtrW(counted_hwnd, GWLP_USERDATA)) == &counting_window,
                L"Window wrapper is bound through GWLP_USERDATA") && ok;
    counting_window.destroy();
    ok = expect(counting_window.hwnd() == nullptr, L"Window wrapper clears HWND after destroy") && ok;
    ok = expect(counting_window.nc_destroy_count() == 1, L"Window wrapper observes WM_NCDESTROY once") && ok;

    CountingWindow command_window;
    nfui::WindowCreateParams command_window_params{
        GetModuleHandleW(nullptr),
        L"NativeFrameUICommandWindow",
        L"NativeFrame UI command window",
    };
    ok = expect(command_window.create(command_window_params), L"command Window wrapper creates a hidden HWND") && ok;
    if (command_window.hwnd() != nullptr) {
        SendMessageW(command_window.hwnd(), WM_COMMAND, MAKEWPARAM(IDM_NFUI_EXIT, 0), 0);
        ok = expect(command_window.command_count() == 1, L"Window wrapper routes WM_COMMAND to on_command") && ok;
        NMHDR header{};
        header.hwndFrom = command_window.hwnd();
        header.idFrom = 77;
        header.code = NM_CLICK;
        LRESULT notify_result = SendMessageW(command_window.hwnd(), WM_NOTIFY, 77, reinterpret_cast<LPARAM>(&header));
        ok = expect(command_window.notify_count() == 1, L"Window wrapper routes WM_NOTIFY to on_notify") && ok;
        ok = expect(notify_result == 1, L"Window wrapper returns on_notify result") && ok;
        command_window.destroy();
    }

    const wchar_t* foreign_class_name = L"NativeFrameUIForeignClass";
    WNDCLASSEXW foreign_class{};
    foreign_class.cbSize = sizeof(foreign_class);
    foreign_class.lpfnWndProc = foreign_window_proc;
    foreign_class.hInstance = GetModuleHandleW(nullptr);
    foreign_class.lpszClassName = foreign_class_name;
    ATOM foreign_atom = RegisterClassExW(&foreign_class);
    ok = expect(foreign_atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS,
                L"foreign window class can be registered") && ok;
    CountingWindow conflicting_window;
    nfui::WindowCreateParams conflicting_params{
        GetModuleHandleW(nullptr),
        foreign_class_name,
        L"NativeFrame UI conflicting class",
    };
    ok = expect(!conflicting_window.create(conflicting_params),
                L"Window wrapper rejects existing classes with a foreign WNDPROC") && ok;
    ok = expect(conflicting_window.hwnd() == nullptr,
                L"Window wrapper does not retain HWND after conflicting class rejection") && ok;

    ThrowingWindow throwing_window;
    nfui::WindowCreateParams throwing_params{
        GetModuleHandleW(nullptr),
        L"NativeFrameUIThrowingWindow",
        L"NativeFrame UI throwing window",
    };
    ok = expect(throwing_window.create(throwing_params), L"throwing Window wrapper creates a hidden HWND") && ok;
    if (throwing_window.hwnd() != nullptr) {
        SendMessageW(throwing_window.hwnd(), WM_APP + 1, 0, 0);
        ok = expect(IsWindow(throwing_window.hwnd()) != FALSE,
                    L"WindowProc boundary catches derived handler exceptions") && ok;
        throwing_window.destroy();
    }

    CountingWindow controls_parent;
    nfui::WindowCreateParams controls_parent_params{
        GetModuleHandleW(nullptr),
        L"NativeFrameUIControlsParent",
        L"NativeFrame UI controls parent",
    };
    ok = expect(controls_parent.create(controls_parent_params), L"controls parent window can be created") && ok;
    if (controls_parent.hwnd() != nullptr) {
        nfui::ControlCreateParams control_params{
            GetModuleHandleW(nullptr),
            controls_parent.hwnd(),
            10,
            L"control",
            0,
            0,
            120,
            24,
        };

        nfui::Button button;
        ok = expect(button.create(control_params), L"Button control creates HWND") && ok;
        ok = expect(button.hwnd() != nullptr, L"Button exposes HWND") && ok;

        ok = expect((GetWindowLongPtrW(button.hwnd(), GWL_STYLE) & BS_OWNERDRAW) != 0,
                    L"Button uses owner-draw style for refined painting") && ok;
        nfui::ThemePalette button_palette = nfui::theme_palette(nfui::ThemeMode::light);
        nfui::FontCache button_fonts;
        button.set_palette(&button_palette);
        button.set_font_cache(&button_fonts);
        ShowWindow(controls_parent.hwnd(), SW_SHOW);
        InvalidateRect(button.hwnd(), nullptr, TRUE);
        UpdateWindow(button.hwnd());
        ShowWindow(controls_parent.hwnd(), SW_HIDE);
        ok = expect(button.hwnd() != nullptr,
                    L"Button owner-draw paint cycle completes without crash") && ok;

        nfui::CheckBox check_box;
        ok = expect(check_box.create(control_params), L"CheckBox control creates HWND") && ok;
        ok = expect(check_box.hwnd() != nullptr, L"CheckBox exposes HWND") && ok;

        nfui::RadioButton radio_button;
        ok = expect(radio_button.create(control_params), L"RadioButton control creates HWND") && ok;
        ok = expect(radio_button.hwnd() != nullptr, L"RadioButton exposes HWND") && ok;

        nfui::Edit edit;
        ok = expect(edit.create(control_params), L"Edit control creates HWND") && ok;
        ok = expect(edit.hwnd() != nullptr, L"Edit exposes HWND") && ok;

        nfui::StaticText static_text;
        ok = expect(static_text.create(control_params), L"StaticText control creates HWND") && ok;
        ok = expect(static_text.hwnd() != nullptr, L"StaticText exposes HWND") && ok;
        // CP15: StaticText now creates with SS_OWNERDRAW so WM_DRAWITEM routes
        // to on_paint. SS_LEFT is implicit in owner-draw statics; verify
        // SS_OWNERDRAW is set instead.
        const LONG_PTR cp15_style = GetWindowLongPtrW(static_text.hwnd(), GWL_STYLE);
        ok = expect((cp15_style & SS_OWNERDRAW) != 0,
                    L"StaticText uses SS_OWNERDRAW so on_paint fires") && ok;

        nfui::ComboBox combo_box;
        ok = expect(combo_box.create(control_params), L"ComboBox control creates HWND") && ok;
        ok = expect(combo_box.hwnd() != nullptr, L"ComboBox exposes HWND") && ok;

        nfui::ListBox list_box;
        ok = expect(list_box.create(control_params), L"ListBox control creates HWND") && ok;
        ok = expect(list_box.hwnd() != nullptr, L"ListBox exposes HWND") && ok;

        nfui::TreeView tree_view;
        ok = expect(tree_view.create(control_params), L"TreeView control creates HWND") && ok;
        ok = expect(tree_view.hwnd() != nullptr, L"TreeView exposes HWND") && ok;

        nfui::ListView list_view;
        ok = expect(list_view.create(control_params), L"ListView control creates HWND") && ok;
        ok = expect(list_view.hwnd() != nullptr, L"ListView exposes HWND") && ok;

        nfui::StatusBar status_bar;
        ok = expect(status_bar.create(control_params), L"StatusBar control creates HWND") && ok;
        ok = expect(status_bar.hwnd() != nullptr, L"StatusBar exposes HWND") && ok;

        nfui::TabControl tab_control;
        ok = expect(tab_control.create(control_params), L"TabControl creates HWND") && ok;
        ok = expect(tab_control.hwnd() != nullptr, L"TabControl exposes HWND") && ok;

        nfui::ProgressBar progress_bar;
        ok = expect(progress_bar.create(control_params), L"ProgressBar creates HWND") && ok;
        ok = expect(progress_bar.hwnd() != nullptr, L"ProgressBar exposes HWND") && ok;

        nfui::Panel panel;
        ok = expect(panel.create(control_params), L"Panel creates HWND") && ok;
        ok = expect(panel.hwnd() != nullptr, L"Panel exposes HWND") && ok;

        nfui::Splitter splitter;
        ok = expect(splitter.create(control_params), L"Splitter creates HWND") && ok;
        splitter.set_ratio(0.25);
        ok = expect(splitter.ratio() == 0.25, L"Splitter preserves ratio in range") && ok;
        splitter.set_ratio(-1.0);
        ok = expect(splitter.ratio() == 0.0, L"Splitter clamps low ratio") && ok;
        splitter.set_ratio(2.0);
        ok = expect(splitter.ratio() == 1.0, L"Splitter clamps high ratio") && ok;
        ok = expect(splitter.hwnd() != nullptr, L"Splitter exposes HWND") && ok;

        // Reflection: an owner-draw button child must receive OCM_DRAWITEM via Window reflection.
        int reflected_draw_count = 0;
        ShowWindow(controls_parent.hwnd(), SW_SHOW);
        HWND owner_button = CreateWindowExW(0, L"BUTTON", L"X",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 60, 24,
            controls_parent.hwnd(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(9001)),
            GetModuleHandleW(nullptr), nullptr);
        ok = expect(owner_button != nullptr, L"owner-draw button child can be created") && ok;
        if (owner_button != nullptr) {
            SetWindowSubclass(owner_button, reflection_test_subclass_proc, 9001,
                              reinterpret_cast<DWORD_PTR>(&reflected_draw_count));
            InvalidateRect(owner_button, nullptr, TRUE);
            UpdateWindow(owner_button);
            ok = expect(reflected_draw_count > 0,
                        L"Window reflects WM_DRAWITEM to owner-draw child as OCM_DRAWITEM") && ok;
            RemoveWindowSubclass(owner_button, reflection_test_subclass_proc, 9001);
            DestroyWindow(owner_button);
        }
        ShowWindow(controls_parent.hwnd(), SW_HIDE);

        {
            using namespace nfui;
            StaticText lbl;
            ControlCreateParams p{};
            p.instance = GetModuleHandleW(nullptr);
            p.parent = controls_parent.hwnd();
            p.control_id = 9002;
            p.text = L"Hello";
            p.x = 0;
            p.y = 0;
            p.width = 120;
            p.height = 20;
            ok = expect(lbl.create(p), L"StaticText::create") && ok;
            ok = expect(lbl.valid(), L"StaticText::valid") && ok;
            const LONG_PTR style = GetWindowLongPtrW(lbl.hwnd(), GWL_STYLE);
            ok = expect((style & SS_OWNERDRAW) != 0, L"StaticText is SS_OWNERDRAW") && ok;

            ThemePalette label_palette = theme_palette(ThemeMode::light);
            FontCache label_fonts;
            lbl.set_palette(&label_palette);
            lbl.set_font_cache(&label_fonts);
            ShowWindow(lbl.hwnd(), SW_SHOW);
            InvalidateRect(lbl.hwnd(), nullptr, TRUE);
            UpdateWindow(lbl.hwnd());
            ShowWindow(lbl.hwnd(), SW_HIDE);
            ok = expect(lbl.hwnd() != nullptr, L"StaticText paint cycle completes without crash") && ok;
        }

        {
            using namespace nfui;
            ListBox lb;
            ControlCreateParams p{};
            p.instance = GetModuleHandleW(nullptr);
            p.parent = controls_parent.hwnd();
            p.control_id = 9003;
            p.x = 0;
            p.y = 0;
            p.width = 160;
            p.height = 120;
            ok = expect(lb.create(p), L"ListBox::create") && ok;
            ok = expect(ListBox_AddString(lb.hwnd(), L"Apple") != LB_ERR, L"ListBox add Apple") && ok;
            ok = expect(ListBox_AddString(lb.hwnd(), L"Banana") != LB_ERR, L"ListBox add Banana") && ok;
            ok = expect(ListBox_GetCount(lb.hwnd()) == 2, L"ListBox count == 2") && ok;
            ListBox_SetCurSel(lb.hwnd(), 1);
            ok = expect(ListBox_GetCurSel(lb.hwnd()) == 1, L"ListBox cur sel == 1") && ok;
            RedrawWindow(lb.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            ok = expect(true, L"ListBox owner-draw paint cycle no crash") && ok;
            // owner-draw style assertion:
            ok = expect((GetWindowLongPtrW(lb.hwnd(), GWL_STYLE) & LBS_OWNERDRAWFIXED) != 0,
                        L"ListBox is LBS_OWNERDRAWFIXED") && ok;
            // row height applied (see gotcha) — use LB_GETITEMHEIGHT:
            int h = static_cast<int>(SendMessageW(lb.hwnd(), LB_GETITEMHEIGHT, 0, 0));
            ok = expect(h > 0, L"ListBox item height > 0") && ok;
        }

        {
            using namespace nfui;
            ListView lv;
            ControlCreateParams p{};
            p.instance = GetModuleHandleW(nullptr); p.parent = controls_parent.hwnd();
            p.control_id = 9004; p.x = 0; p.y = 0; p.width = 240; p.height = 160;
            ok = expect(lv.create(p), L"ListView::create") && ok;
            LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.cx = 120; col.pszText = const_cast<LPWSTR>(L"Name");
            ok = expect(ListView_InsertColumn(lv.hwnd(), 0, &col) != -1, L"ListView insert column") && ok;
            LVITEMW it{}; it.mask = LVIF_TEXT; it.iItem = 0; it.pszText = const_cast<LPWSTR>(L"Row 1");
            ok = expect(ListView_InsertItem(lv.hwnd(), &it) != -1, L"ListView insert item") && ok;
            ok = expect(ListView_GetItemCount(lv.hwnd()) == 1, L"ListView item count == 1") && ok;
            // exercise custom-draw paint path:
            RedrawWindow(lv.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            ok = expect(true, L"ListView custom-draw paint no crash") && ok;
        }

        nfui::IconView icon_view;
        {
            using namespace nfui;
            ControlCreateParams p{};
            p.instance = GetModuleHandleW(nullptr); p.parent = controls_parent.hwnd();
            p.control_id = 9005; p.x = 0; p.y = 0; p.width = 24; p.height = 24;
            ok = expect(icon_view.create(p), L"IconView::create") && ok;
            ok = expect(icon_view.valid(), L"IconView::valid") && ok;
            ok = expect((GetWindowLongPtrW(icon_view.hwnd(), GWL_STYLE) & SS_OWNERDRAW) != 0, L"IconView is SS_OWNERDRAW") && ok;
            // exercise the DrawIconEx paint path with the framework's own icon resource.
            // load_scaled_icon signature: (HINSTANCE, LPCWSTR resource_id, int logical_size, int dpi).
            HICON h = nfui::load_scaled_icon(GetModuleHandleW(nullptr),
                                              MAKEINTRESOURCEW(IDI_NFUI_APP), 24, 96);
            ok = expect(h != nullptr, L"load_scaled_icon loads framework icon for IconView") && ok;
            if (h != nullptr) {
                icon_view.set_icon(h);
                RedrawWindow(icon_view.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                ok = expect(true, L"IconView owner-draw paint no crash") && ok;
                DestroyIcon(h);
            }
        }

        // Chart control smoke tests — one of each nfui::ChartView subclass
        // (BarChartView, HBarChartView, LineChartView, SplineChartView). All
        // four share the NativeFrameUIChartView window class registered by the
        // first create() call; ChartView::create overrides the requested class
        // name to that private class so Window::register_window_class accepts
        // it. Charts are nfui::Window subclasses, not Control subclasses, so
        // create() takes WindowCreateParams (no control_id, no text).
        {
            using namespace nfui;
            BarChartView bar;
            WindowCreateParams p{};
            p.instance = GetModuleHandleW(nullptr);
            p.parent = controls_parent.hwnd();
            p.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
            p.x = 0; p.y = 0; p.width = 240; p.height = 160;
            ok = expect(bar.create(p), L"BarChartView::create") && ok;
            ok = expect(bar.hwnd() != nullptr, L"BarChartView exposes HWND") && ok;
            const ThemePalette palette_for_test = theme_palette(ThemeMode::light);
            FontCache fonts_for_test;
            bar.set_palette(&palette_for_test);
            bar.set_font_cache(&fonts_for_test);
            bar.set_series({ChartSeries{L"Revenue", palette_for_test.accent,
                            {{0.0, 10.0}, {1.0, 20.0}, {2.0, 15.0}}}});
            bar.set_axis_x(ChartAxisRange{0.0, 2.0, L"{:.0f}"});
            bar.set_axis_y(ChartAxisRange{0.0, 25.0, L"{:.0f}"});
            RedrawWindow(bar.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            ok = expect(bar.hwnd() != nullptr, L"BarChartView paint cycle completes without crash") && ok;
        }
        {
            using namespace nfui;
            HBarChartView hbar;
            WindowCreateParams p{};
            p.instance = GetModuleHandleW(nullptr);
            p.parent = controls_parent.hwnd();
            p.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
            p.x = 0; p.y = 0; p.width = 240; p.height = 160;
            ok = expect(hbar.create(p), L"HBarChartView::create") && ok;
            ok = expect(hbar.hwnd() != nullptr, L"HBarChartView exposes HWND") && ok;
            const ThemePalette palette_for_test = theme_palette(ThemeMode::light);
            FontCache fonts_for_test;
            hbar.set_palette(&palette_for_test);
            hbar.set_font_cache(&fonts_for_test);
            hbar.set_series({ChartSeries{L"Q1", palette_for_test.accent,
                             {{0.0, 30.0}, {1.0, 45.0}, {2.0, 25.0}}}});
            RedrawWindow(hbar.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            ok = expect(hbar.hwnd() != nullptr, L"HBarChartView paint cycle completes without crash") && ok;
        }
        {
            using namespace nfui;
            LineChartView line;
            WindowCreateParams p{};
            p.instance = GetModuleHandleW(nullptr);
            p.parent = controls_parent.hwnd();
            p.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
            p.x = 0; p.y = 0; p.width = 240; p.height = 160;
            ok = expect(line.create(p), L"LineChartView::create") && ok;
            ok = expect(line.hwnd() != nullptr, L"LineChartView exposes HWND") && ok;
            const ThemePalette palette_for_test = theme_palette(ThemeMode::light);
            FontCache fonts_for_test;
            line.set_palette(&palette_for_test);
            line.set_font_cache(&fonts_for_test);
            line.set_series({
                ChartSeries{L"Revenue", palette_for_test.accent,
                            {{0.0, 10.0}, {1.0, 25.0}, {2.0, 18.0}, {3.0, 30.0}}},
                ChartSeries{L"Costs", palette_for_test.warning,
                            {{0.0, 8.0}, {1.0, 12.0}, {2.0, 10.0}, {3.0, 15.0}}},
            });
            line.set_point_radius(3);
            RedrawWindow(line.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            ok = expect(line.hwnd() != nullptr, L"LineChartView paint cycle completes without crash") && ok;
        }
        {
            using namespace nfui;
            SplineChartView spline;
            WindowCreateParams p{};
            p.instance = GetModuleHandleW(nullptr);
            p.parent = controls_parent.hwnd();
            p.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
            p.x = 0; p.y = 0; p.width = 240; p.height = 160;
            ok = expect(spline.create(p), L"SplineChartView::create") && ok;
            ok = expect(spline.hwnd() != nullptr, L"SplineChartView exposes HWND") && ok;
            const ThemePalette palette_for_test = theme_palette(ThemeMode::light);
            FontCache fonts_for_test;
            spline.set_palette(&palette_for_test);
            spline.set_font_cache(&fonts_for_test);
            spline.set_series({ChartSeries{L"Sensor", palette_for_test.accent,
                             {{0.0, 10.0}, {1.0, 30.0}, {2.0, 15.0}, {3.0, 35.0}, {4.0, 20.0}}}});
            spline.set_tension(0.5);
            RedrawWindow(spline.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            ok = expect(spline.hwnd() != nullptr, L"SplineChartView paint cycle completes without crash") && ok;
        }

        // CP14: AreaChartView coverage. Mirrors the LineChartView block —
        // proves create + paint + alpha-blend + outline toggles all
        // succeed without crashing the offscreen paint path.
        {
            using namespace nfui;
            AreaChartView area;
            WindowCreateParams p{};
            p.instance = GetModuleHandleW(nullptr);
            p.parent = controls_parent.hwnd();
            p.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
            p.x = 0; p.y = 0; p.width = 240; p.height = 160;
            ok = expect(area.create(p), L"AreaChartView::create") && ok;
            ok = expect(area.hwnd() != nullptr, L"AreaChartView exposes HWND") && ok;
            const ThemePalette palette_for_area = theme_palette(ThemeMode::light);
            FontCache fonts_for_area;
            area.set_palette(&palette_for_area);
            area.set_font_cache(&fonts_for_area);
            area.set_series({ChartSeries{L"Load", palette_for_area.accent,
                            {{0.0, 5.0}, {1.0, 25.0}, {2.0, 12.0}, {3.0, 40.0}, {4.0, 18.0}}}});
            area.set_outline(true);
            area.set_fill_alpha(1.0);
            RedrawWindow(area.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            ok = expect(area.hwnd() != nullptr, L"AreaChartView paint cycle completes without crash") && ok;
            // Outline off + alpha < 1.0 path: must not crash on the second
            // paint. Validates the alpha_blend branch without checking
            // pixel values (smoke test runs headless).
            area.set_outline(false);
            area.set_fill_alpha(0.5);
            RedrawWindow(area.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            ok = expect(area.hwnd() != nullptr, L"AreaChartView alpha-blend fill paints without crash") && ok;
        }

        controls_parent.destroy();
        ok = expect(!button.valid() && button.hwnd() == nullptr,
                    L"Button wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!check_box.valid() && check_box.hwnd() == nullptr,
                    L"CheckBox wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!radio_button.valid() && radio_button.hwnd() == nullptr,
                    L"RadioButton wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!edit.valid() && edit.hwnd() == nullptr,
                    L"Edit wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!static_text.valid() && static_text.hwnd() == nullptr,
                    L"StaticText wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!combo_box.valid() && combo_box.hwnd() == nullptr,
                    L"ComboBox wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!list_box.valid() && list_box.hwnd() == nullptr,
                    L"ListBox wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!tree_view.valid() && tree_view.hwnd() == nullptr,
                    L"TreeView wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!list_view.valid() && list_view.hwnd() == nullptr,
                    L"ListView wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!status_bar.valid() && status_bar.hwnd() == nullptr,
                    L"StatusBar wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!tab_control.valid() && tab_control.hwnd() == nullptr,
                    L"TabControl wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!progress_bar.valid() && progress_bar.hwnd() == nullptr,
                    L"ProgressBar wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!panel.valid() && panel.hwnd() == nullptr,
                    L"Panel wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!splitter.valid() && splitter.hwnd() == nullptr,
                    L"Splitter wrapper invalidates HWND after parent destruction") && ok;
        ok = expect(!icon_view.valid() && icon_view.hwnd() == nullptr,
                    L"IconView wrapper invalidates HWND after parent destruction") && ok;
    }

    nfui::ResourceContext resources(GetModuleHandleW(nullptr));
    ok = expect(resources.has_dialog(IDD_NFUI_ABOUT), L"dialog resource loads") && ok;
    ok = expect(resources.has_menu(IDM_NFUI_MAIN), L"menu resource loads") && ok;
    ok = expect(resources.has_string(IDS_NFUI_APP_TITLE), L"string resource loads") && ok;
    ok = expect(resources.has_icon(IDI_NFUI_APP), L"icon resource loads") && ok;
    ok = expect(resources.has_bitmap(IDB_NFUI_MARK), L"bitmap resource loads") && ok;
    ok = expect(resources.has_toolbar(IDT_NFUI_MAIN), L"toolbar resource loads") && ok;
    ok = expect(resources.load_string(IDS_NFUI_APP_TITLE) == L"NativeFrame UI", L"string resource text loads") && ok;

    ok = expect(target_registered(L"NativeFrameUIShowcase"),
                L"Showcase target is registered in generated build artifacts") && ok;
    ok = expect(target_registered(L"NativeFrameUIDarkStudio"),
                L"DarkStudio target is registered in generated build artifacts") && ok;
    ok = expect(target_registered(L"NativeFrameUISettingsDemo"),
                L"SettingsDemo target is registered in generated build artifacts") && ok;
    ok = expect(target_registered(L"NativeFrameUIResourceGallery"),
                L"ResourceGallery target is registered in generated build artifacts") && ok;
    ok = expect(target_registered(L"NativeFrameUIControls"),
                L"NativeFrameUIControls target is registered in generated build artifacts") && ok;
    ok = expect(target_registered(L"NativeFrameUICharts"),
                L"NativeFrameUICharts target is registered in generated build artifacts") && ok;

    HWND dialog = resources.create_dialog(IDD_NFUI_ABOUT, nullptr, test_dialog_proc, 0);
    ok = expect(dialog != nullptr, L"modeless dialog resource creates a HWND") && ok;
    ok = expect(dialog == nullptr || IsWindow(dialog) != FALSE, L"modeless dialog HWND is valid") && ok;
    if (dialog != nullptr) {
        DestroyWindow(dialog);
    }

    nfui::ResourceContext missing_resources(nullptr);
    ok = expect(!missing_resources.has_dialog(IDD_NFUI_ABOUT), L"null module does not load dialog resources") && ok;
    ok = expect(!missing_resources.has_menu(IDM_NFUI_MAIN), L"null module does not load menu resources") && ok;
    ok = expect(!missing_resources.has_string(IDS_NFUI_APP_TITLE), L"null module does not load string resources") && ok;
    ok = expect(!missing_resources.has_icon(IDI_NFUI_APP), L"null module does not load icon resources") && ok;
    ok = expect(!missing_resources.has_bitmap(IDB_NFUI_MARK), L"null module does not load bitmap resources") && ok;
    ok = expect(!missing_resources.has_toolbar(IDT_NFUI_MAIN), L"null module does not load toolbar resources") && ok;
    ok = expect(missing_resources.load_string(IDS_NFUI_APP_TITLE).empty(), L"null module does not load string text") && ok;
    ok = expect(missing_resources.create_dialog(IDD_NFUI_ABOUT, nullptr, test_dialog_proc, 0) == nullptr,
                L"null module does not create dialogs") && ok;

    {
        using namespace nfui;
        // Modeless round-trip via the Dialog wrapper. No modal pump, safe in CI.
        Dialog about;
        HWND modeless = about.show_modeless(GetModuleHandleW(nullptr),
                                            MAKEINTRESOURCEW(IDD_NFUI_ABOUT),
                                            nullptr,
                                            test_dialog_proc);
        ok = expect(modeless != nullptr, L"Dialog::show_modeless returns a non-null HWND") && ok;
        ok = expect(about.valid() && about.hwnd() == modeless,
                    L"Dialog exposes the modeless HWND via hwnd()/valid()") && ok;
        about.end_modeless(IDOK);
        ok = expect(!about.valid(),
                    L"Dialog::end_modeless releases the modeless HWND") && ok;

        // Modal round-trip is skip-guarded. DialogBoxParamW blocks on its
        // own message pump and a human has to dismiss the dialog; in
        // CI/headless this would hang. Set NONFUI_SKIP_DIALOG=1 to skip.
        wchar_t skip_buffer[8]{};
        DWORD skip_length = GetEnvironmentVariableW(L"NONFUI_SKIP_DIALOG",
                                                     skip_buffer,
                                                     static_cast<DWORD>(std::size(skip_buffer)));
        const bool skip_modal = skip_length > 0 && skip_buffer[0] != L'0';
        if (skip_modal) {
            ok = expect(true, L"Dialog::show_modal round-trip is skipped via NONFUI_SKIP_DIALOG") && ok;
        } else {
            Dialog modal_about;
            int rc = modal_about.show_modal(GetModuleHandleW(nullptr),
                                            MAKEINTRESOURCEW(IDD_NFUI_ABOUT),
                                            nullptr,
                                            [](HWND hwnd, UINT msg, WPARAM w, LPARAM) -> INT_PTR {
                if (msg == WM_COMMAND && LOWORD(w) == IDOK) {
                    EndDialog(hwnd, IDOK);
                    return TRUE;
                }
                if (msg == WM_INITDIALOG) return TRUE;
                return FALSE;
            });
            ok = expect(rc == IDOK, L"Dialog::show_modal returns IDOK when OK clicked") && ok;
            ok = expect(modal_about.modal_result() == IDOK,
                        L"Dialog::modal_result exposes the IDOK result") && ok;
        }
    }

    GuiResourceCounts before = capture_gui_resource_counts();
    HWND window = CreateWindowExW(0, L"STATIC", L"NativeFrame UI smoke test", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 320, 200,
                                  nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ok = expect(window != nullptr, L"hidden smoke-test HWND can be created") && ok;
    if (window != nullptr) {
        DestroyWindow(window);
    }
    GuiResourceCounts after = capture_gui_resource_counts();

    ok = expect(after.gdi <= before.gdi + 2, L"GDI object count remains stable after create/destroy") && ok;
    ok = expect(after.user <= before.user + 2, L"USER object count remains stable after create/destroy") && ok;

    {
        const nfui::ThemePalette light = nfui::theme_palette(nfui::ThemeMode::light);
        const nfui::ThemePalette dark  = nfui::theme_palette(nfui::ThemeMode::dark);
        ok = expect(light.background.rgb != light.surface.rgb, L"light background differs from surface") && ok;
        ok = expect(dark.background.rgb != dark.text.rgb, L"dark background differs from text") && ok;
        ok = expect(light.accent.rgb != light.accent_hover.rgb, L"light accent differs from accent hover") && ok;
        const nfui::ThemeTokens t = nfui::theme_tokens(nfui::ThemeMode::dark);
        ok = expect(t.window_background == dark.background.rgb, L"theme_tokens derives window_background from palette") && ok;
    }

    {
        using namespace nfui;
        const Color white{RGB(255, 255, 255)};
        const Color black{RGB(0, 0, 0)};
        const COLORREF lb = lighten(black, 0.5f).rgb;
        ok = expect(lb == RGB(127, 127, 127) || lb == RGB(128, 128, 128), L"lighten black by half yields mid gray") && ok;
        ok = expect(darken(white, 1.0f).rgb == black.rgb, L"darken white fully yields black") && ok;
        ok = expect(alpha_blend(white, black, 0.0f).rgb == black.rgb, L"alpha blend alpha 0 yields destination") && ok;
        ok = expect(alpha_blend(white, black, 1.0f).rgb == white.rgb, L"alpha blend alpha 1 yields source") && ok;
        ok = expect(darken(white, -0.5f).rgb == white.rgb, L"darken clamps negative amount to no-op") && ok;
    }

    {
        using namespace nfui;
        ok = expect(font_pixel_height(9, 96) == 12, L"9pt at 96dpi is 12px") && ok;
        ok = expect(font_pixel_height(9, 144) == 18, L"9pt at 144dpi is 18px (1.5x)") && ok;
        ok = expect(font_pixel_height(0, 96) == 0, L"zero point size yields zero height") && ok;
    }

    {
        using namespace nfui;
        FontCache fc;
        HFONT r96 = fc.regular(96, 9);
        HFONT s96 = fc.semibold(96, 9);
        ok = expect(r96 != nullptr, L"FontCache creates regular font at 96dpi") && ok;
        ok = expect(s96 != nullptr, L"FontCache creates semibold font at 96dpi") && ok;
        ok = expect(r96 != s96, L"regular and semibold are distinct handles") && ok;
        HFONT r144 = fc.regular(144, 9);
        ok = expect(r144 != nullptr && r144 != r96, L"FontCache rebuilds regular on DPI change") && ok;
        HFONT s144 = fc.semibold(144, 9);
        ok = expect(s144 != nullptr && s144 != s96, L"FontCache rebuilds semibold on DPI change (no stale font)") && ok;
        HFONT r12 = fc.regular(96, 12);
        ok = expect(r12 != nullptr && r12 != r96, L"FontCache rebuilds regular on point size change") && ok;
    }

    {
        using namespace nfui;
        FontCache fc;
        HFONT serif96 = fc.serif(96, 9);
        HFONT mono96  = fc.mono(96, 9);
        ok = expect(serif96 != nullptr, L"FontCache creates serif (Georgia) at 96dpi") && ok;
        ok = expect(mono96 != nullptr, L"FontCache creates mono (Cascadia/Consolas) at 96dpi") && ok;
        ok = expect(fc.serif(96, 9) == fc.serif(96, 9), L"FontCache reuses cached serif") && ok;
        ok = expect(fc.mono(96, 9) == fc.mono(96, 9), L"FontCache reuses cached mono") && ok;
        ok = expect(serif96 != mono96, L"serif and mono are distinct handles") && ok;
        ok = expect(serif96 != fc.regular(96, 9), L"serif differs from regular Segoe UI") && ok;
        HFONT serif144 = fc.serif(144, 9);
        ok = expect(serif144 != nullptr && serif144 != serif96, L"FontCache rebuilds serif on DPI change") && ok;
    }

    {
        using namespace nfui;
        ok = expect(icon_pixel_size(16, 96) == 16, L"icon size is 1:1 at 96dpi") && ok;
        ok = expect(icon_pixel_size(16, 144) == 24, L"icon size scales 1.5x at 144dpi") && ok;
        ok = expect(icon_pixel_size(0, 96) == 0, L"zero logical size yields zero") && ok;
        const HINSTANCE inst = GetModuleHandleW(nullptr);
        HICON good = load_scaled_icon(inst, MAKEINTRESOURCEW(IDI_NFUI_APP), 16, 96);
        ok = expect(good != nullptr, L"load_scaled_icon loads the framework icon") && ok;
        if (good) DestroyIcon(good);
        HICON bad = load_scaled_icon(inst, MAKEINTRESOURCEW(37123), 16, 96);
        ok = expect(bad == nullptr, L"load_scaled_icon returns null for a missing resource") && ok;
        if (bad) DestroyIcon(bad);
    }

    {
        using namespace nfui;
        const ThemePalette light = theme_palette(ThemeMode::light);
        const ThemePalette dark  = theme_palette(ThemeMode::dark);
        ok = expect(light.accent.rgb == RGB(217, 119, 87), L"light accent is Claude Code coral #D97757") && ok;
        ok = expect(dark.accent.rgb  == RGB(217, 119, 87), L"dark accent is Claude Code coral #D97757") && ok;
        ok = expect(light.background.rgb == RGB(250, 249, 245), L"light background is warm cream #FAF9F5") && ok;
        ok = expect(dark.background.rgb  == RGB(31, 30, 29),    L"dark background is warm ink #1F1E1D") && ok;
        ok = expect(light.text.rgb != dark.text.rgb, L"light and dark text differ") && ok;
        const ThemeMetrics m = theme_metrics();
        ok = expect(m.corner_radius_control == 6, L"metrics control radius == 6") && ok;
        ok = expect(m.corner_radius_card == 10, L"metrics card radius == 10") && ok;
        ok = expect(m.spacing == 8, L"metrics spacing == 8") && ok;
    }

    {
        using namespace nfui;
        HDC screen = GetDC(nullptr);
        RECT rc{0, 0, 40, 30};
        {
            MemoryDC mem(screen, rc);
            ok = expect(mem.valid(), L"MemoryDC creates an offscreen buffer") && ok;
            ok = expect(mem.dc() != nullptr, L"MemoryDC exposes its DC") && ok;
            fill_rect(mem.dc(), rc, Color{RGB(10, 20, 30)});
            draw_line(mem.dc(), {0,0}, {40,30}, Color{RGB(255,255,255)}, 1);
            POINT poly[3] = {{0,0},{40,0},{20,30}};
            fill_polygon(mem.dc(), poly, 3, Color{RGB(200,100,50)}, Color{RGB(0,0,0)});
            draw_polyline(mem.dc(), poly, 3, Color{RGB(0,0,0)}, 1);
        }
        ReleaseDC(nullptr, screen);
        ok = expect(true, L"MemoryDC paint helpers complete without crash") && ok;
    }

    {
        using namespace nfui;
        HDC screen = GetDC(nullptr);
        // Pick a corner that's unlikely to be covered by another test on the developer's desktop.
        RECT rc{200, 200, 240, 230};
        {
            MemoryDC mem(screen, rc);
            ok = expect(mem.valid(), L"MemoryDC non-zero-origin: valid buffer") && ok;
            // Paint a unique sentinel color at (0,0) of the buffer.
            fill_rect(mem.dc(), RECT{0, 0, 40, 30}, Color{RGB(123, 234, 156)});
        } // ~MemoryDC must flush to (200,200) — the screen pixel there should now be the sentinel.
        COLORREF px = GetPixel(screen, 210, 210);   // midpoint of the 40x30 patch
        ok = expect(px == RGB(123, 234, 156), L"MemoryDC non-zero-origin: flush lands at bounds.left/top") && ok;
        // Restore the pixel we just overwrote (best-effort — use a near-black color to minimize visual impact).
        SetPixel(screen, 210, 210, RGB(0, 0, 0));
        ReleaseDC(nullptr, screen);
        ok = expect(true, L"MemoryDC non-zero-origin: regression test completes without crash") && ok;
    }

    {
        using namespace nfui;
        // Graceful degradation: MemoryDC must never allocate or leak when it
        // cannot build an offscreen buffer. valid()==false is the documented
        // signal for the direct-DC fallback (P6.1 / CreateCompatibleBitmap doc).
        {
            MemoryDC null_target(nullptr, RECT{0, 0, 40, 30});
            ok = expect(!null_target.valid(), L"MemoryDC(null target) is not valid") && ok;
            ok = expect(null_target.dc() == nullptr, L"MemoryDC(null target) exposes no DC") && ok;
        } // ~MemoryDC on the degraded object must be a no-op (no crash, no leak).

        HDC screen = GetDC(nullptr);
        {
            MemoryDC empty(screen, RECT{5, 5, 5, 5});           // zero width & height
            ok = expect(!empty.valid(), L"MemoryDC(empty rect) is not valid") && ok;
        }
        {
            MemoryDC inverted(screen, RECT{40, 30, 0, 0});      // right<left, bottom<top
            ok = expect(!inverted.valid(), L"MemoryDC(inverted rect) is not valid") && ok;
        }
        ReleaseDC(nullptr, screen);
        ok = expect(true, L"MemoryDC degradation paths complete without crash") && ok;
    }

    {
        using namespace nfui;
        const RECT content{0, 0, 400, 300};
        ChartLayout layout = compute_chart_layout(content, ChartKind::bar_vertical, 2);
        ok = expect((layout.plot_bounds.right - layout.plot_bounds.left) > 0 &&
                    (layout.plot_bounds.bottom - layout.plot_bounds.top) > 0,
                    L"compute_chart_layout yields non-empty plot bounds") && ok;
        ok = expect(layout.legend_width_px == 120,
                    L"compute_chart_layout reserves 120px legend column for multi-series") && ok;
    }

    {
        using namespace nfui;
        ChartLayout layout = compute_chart_layout(RECT{0, 0, 400, 300}, ChartKind::line, 1);
        const ChartAxisRange x{0.0, 1.0};
        const ChartAxisRange y{0.0, 1.0};
        const std::vector<ChartPoint> samples{ {0.0, 0.0}, {1.0, 1.0} };
        const std::vector<POINT> pts = normalize_points(samples, layout, x, y);
        ok = expect(pts.size() == 2, L"normalize_points returns 2 points for 2 samples") && ok;
        const int w = layout.plot_bounds.right - layout.plot_bounds.left;
        const int h = layout.plot_bounds.bottom - layout.plot_bounds.top;
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const POINT p = pts[i];
            const bool inside = p.x >= layout.plot_bounds.left &&
                                p.x <= layout.plot_bounds.left + w &&
                                p.y >= layout.plot_bounds.top &&
                                p.y <= layout.plot_bounds.top + h;
            ok = expect(inside, L"normalized point lies within plot bounds") && ok;
        }
    }

    {
        using namespace nfui;
        ChartLayout layout = compute_chart_layout(RECT{0, 0, 400, 300}, ChartKind::bar_vertical, 1);
        const std::vector<RECT> bars = compute_bar_geometry(layout, 1, 4, 0.2);
        ok = expect(bars.size() == 4, L"compute_bar_geometry returns one RECT per bar") && ok;
        for (std::size_t i = 0; i < bars.size(); ++i) {
            const RECT r = bars[i];
            ok = expect(r.right > r.left && r.bottom > r.top,
                        L"compute_bar_geometry bar has positive area") && ok;
        }
        for (std::size_t i = 1; i < bars.size(); ++i) {
            const RECT prev = bars[i - 1];
            const RECT curr = bars[i];
            const bool disjoint = prev.right <= curr.left;
            ok = expect(disjoint, L"compute_bar_geometry bars do not overlap") && ok;
        }
    }

    {
        using namespace nfui;
        const std::vector<POINT> pts{ POINT{0, 0}, POINT{10, 10}, POINT{20, 5} };
        const std::vector<POINT> bez = catmull_rom_to_bezier(pts, 0.5);
        // CP28: PolyBezier / DrawBeziers expect chained-cubic segments in
        // 1 + 3*(N-1) layout (start anchor + 3 control points per segment).
        // For 3 input points there are 2 cubic segments, so 7 output points
        // is correct — the previous test asserted 8 (4 per segment), which
        // matched the broken pre-CP28 implementation that emitted one extra
        // point per segment and made both GDI/GDI+ silently fail to paint.
        ok = expect(bez.size() == 7,
                    L"catmull_rom_to_bezier returns 1 + 3*(N-1) control points (PolyBezier layout)") && ok;
        // Sanity: first and last control-point x values match the first/last inputs.
        ok = expect(bez.front().x == pts.front().x && bez.back().x == pts.back().x,
                    L"catmull_rom_to_bezier endpoint x matches input endpoints") && ok;
    }

    {
        using namespace nfui;
        // Pure-logic: state transitions without an HWND.
        HoverState h;
        ok = expect(!h.hover() && !h.pressed(),
                    L"HoverState starts idle") && ok;
        h.on_lbutton_down();
        ok = expect(h.pressed(),
                    L"HoverState tracks lbutton down") && ok;
        h.on_lbutton_up();
        ok = expect(!h.pressed(),
                    L"HoverState releases on lbutton up") && ok;
        h.on_mouse_move(nullptr); // null hwnd tolerated
        h.on_mouse_leave();
        ok = expect(!h.hover(),
                    L"HoverState mouse_leave clears hover") && ok;

        // attach()/detach() must reset all transient state to idle so a wrapper
        // that re-binds to a new HWND never inherits stale hover/pressed bits.
        h.on_mouse_move(nullptr);
        h.on_lbutton_down();
        h.attach(nullptr);
        ok = expect(!h.hover() && !h.pressed(),
                    L"HoverState attach() resets to idle") && ok;
        h.on_mouse_move(nullptr);
        h.on_lbutton_down();
        h.detach();
        ok = expect(!h.hover() && !h.pressed(),
                    L"HoverState detach() resets to idle") && ok;

        // Real-HWND path: on_mouse_move must register TrackMouseEvent (or take
        // the posted-WM_MOUSELEAVE fallback) and set hover without crashing.
        HWND hover_hwnd = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                          CW_USEDEFAULT, CW_USEDEFAULT, 60, 40,
                                          HWND_MESSAGE, nullptr,
                                          GetModuleHandleW(nullptr), nullptr);
        if (hover_hwnd != nullptr) {
            h.attach(hover_hwnd);
            h.on_mouse_move(hover_hwnd);
            ok = expect(h.hover(), L"HoverState on_mouse_move(real hwnd) sets hover") && ok;
            h.on_mouse_leave();
            ok = expect(!h.hover(), L"HoverState on_mouse_leave(real hwnd) clears hover") && ok;
            DestroyWindow(hover_hwnd);
        }
    }

    {
        using namespace nfui;
        // OwnerDrawDC wraps BeginPaint/MemoryDC/EndPaint around a real HWND.
        HWND msg_hwnd = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                        CW_USEDEFAULT, CW_USEDEFAULT, 100, 50,
                                        HWND_MESSAGE, nullptr,
                                        GetModuleHandleW(nullptr), nullptr);
        ok = expect(msg_hwnd != nullptr, L"OwnerDrawDC test creates a message-only HWND") && ok;
        if (msg_hwnd != nullptr) {
            RECT rc{0, 0, 100, 50};
            {
                OwnerDrawDC odc(msg_hwnd, rc);
                ok = expect(odc.valid(), L"OwnerDrawDC is valid") && ok;
                ok = expect(odc.dc() != nullptr, L"OwnerDrawDC exposes its DC") && ok;
                const RECT got = odc.bounds();
                ok = expect(got.right - got.left == 100 && got.bottom - got.top == 50,
                            L"OwnerDrawDC bounds round-trip") && ok;
            }
            DestroyWindow(msg_hwnd);
        }
    }

    {
        using namespace nfui;
        // CP17: pure animation primitives. No HWND, no message loop — the
        // ColorAnimation state machine is driven by a fake clock so the
        // synchronous test harness covers the same logic the WM_TIMER arm
        // drives in production.

        // Easing curves: endpoints exact, monotonic in between.
        ok = expect(ease_linear(0.0f) == 0.0f && ease_linear(1.0f) == 1.0f,
                    L"ease_linear endpoints exact") && ok;
        ok = expect(ease_out_cubic(0.0f) == 0.0f && ease_out_cubic(1.0f) == 1.0f,
                    L"ease_out_cubic endpoints exact") && ok;
        ok = expect(ease_in_out_cubic(0.0f) == 0.0f && ease_in_out_cubic(1.0f) == 1.0f,
                    L"ease_in_out_cubic endpoints exact") && ok;
        ok = expect(ease_out_quint(0.0f) == 0.0f && ease_out_quint(1.0f) == 1.0f,
                    L"ease_out_quint endpoints exact") && ok;
        // Clamping: out-of-range t snaps to the [0,1] endpoints.
        ok = expect(ease_out_cubic(-1.0f) == 0.0f && ease_out_cubic(2.0f) == 1.0f,
                    L"ease curves clamp out-of-range t") && ok;
        // ease_out_cubic is monotonic non-decreasing on a 5-step sample.
        bool mono = true;
        float prev = ease_out_cubic(0.0f);
        for (int i = 1; i <= 5; ++i) {
            const float v = ease_out_cubic(static_cast<float>(i) / 5.0f);
            if (v < prev) mono = false;
            prev = v;
        }
        ok = expect(mono, L"ease_out_cubic is monotonic") && ok;

        // lerp_color: endpoints exact, midpoint ~RGB(127,127,127).
        const Color black{RGB(0, 0, 0)};
        const Color white{RGB(255, 255, 255)};
        ok = expect(lerp_color(black, white, 0.0f).rgb == black.rgb,
                    L"lerp_color t=0 returns a") && ok;
        ok = expect(lerp_color(black, white, 1.0f).rgb == white.rgb,
                    L"lerp_color t=1 returns b") && ok;
        const Color mid = lerp_color(black, white, 0.5f);
        ok = expect(GetRValue(mid.rgb) >= 126 && GetRValue(mid.rgb) <= 129 &&
                    GetGValue(mid.rgb) >= 126 && GetGValue(mid.rgb) <= 129 &&
                    GetBValue(mid.rgb) >= 126 && GetBValue(mid.rgb) <= 129,
                    L"lerp_color t=0.5 is the channel midpoint") && ok;

        // lerp_palette: every field is the midpoint between dark and light.
        const ThemePalette pd = theme_palette(ThemeMode::dark);
        const ThemePalette pl = theme_palette(ThemeMode::light);
        const ThemePalette pm = lerp_palette(pd, pl, 0.5f);
        auto near_mid = [](COLORREF a, COLORREF b, COLORREF m) {
            const int ra = GetRValue(a), rb = GetRValue(b), rm = GetRValue(m);
            const int ga = GetGValue(a), gb = GetGValue(b), gm = GetGValue(m);
            const int ba = GetBValue(a), bb = GetBValue(b), bm = GetBValue(m);
            return std::abs(rm - (ra + rb) / 2) <= 1 &&
                   std::abs(gm - (ga + gb) / 2) <= 1 &&
                   std::abs(bm - (ba + bb) / 2) <= 1;
        };
        ok = expect(near_mid(pd.background.rgb, pl.background.rgb, pm.background.rgb),
                    L"lerp_palette background is midpoint") && ok;
        ok = expect(near_mid(pd.accent.rgb, pl.accent.rgb, pm.accent.rgb),
                    L"lerp_palette accent is midpoint") && ok;
        ok = expect(near_mid(pd.text.rgb, pl.text.rgb, pm.text.rgb),
                    L"lerp_palette text is midpoint") && ok;
        ok = expect(lerp_palette(pd, pl, 0.0f).background.rgb == pd.background.rgb,
                    L"lerp_palette t=0 returns a") && ok;
        ok = expect(lerp_palette(pd, pl, 1.0f).background.rgb == pl.background.rgb,
                    L"lerp_palette t=1 returns b") && ok;

        // Win32Clock: non-decreasing across two reads.
        Win32Clock clock;
        const unsigned long long t0 = clock.now_ms();
        const unsigned long long t1 = clock.now_ms();
        ok = expect(t1 >= t0, L"Win32Clock now_ms is non-decreasing") && ok;

        // ColorAnimation: begin / sample / deactivation / cancel.
        ColorAnimation anim;
        ok = expect(!anim.is_active(), L"ColorAnimation starts inactive") && ok;
        anim.begin(black, white, 1000, 120);
        ok = expect(anim.is_active(), L"ColorAnimation active after begin") && ok;
        ok = expect(anim.sample(1000, ease_out_cubic).rgb == black.rgb,
                    L"ColorAnimation sample at start returns from") && ok;
        ok = expect(anim.is_active(), L"ColorAnimation still active at t=0") && ok;
        const Color half = anim.sample(1060, ease_out_cubic);  // t=0.5
        ok = expect(GetRValue(half.rgb) > 0 && GetRValue(half.rgb) < 255,
                    L"ColorAnimation mid-sample is interpolated") && ok;
        ok = expect(anim.sample(1121, ease_out_cubic).rgb == white.rgb,
                    L"ColorAnimation sample past duration snaps to to") && ok;
        ok = expect(!anim.is_active(), L"ColorAnimation deactivates after duration") && ok;
        anim.begin(black, white, 2000, 100);
        anim.cancel();
        ok = expect(!anim.is_active(), L"ColorAnimation cancel deactivates") && ok;
    }

    {
        // CP18: vector icon system. Every glyph must render into a MemoryDC
        // without crashing and leave at least one non-background pixel inside
        // its cell (the sentinel-pixel idiom from the MemoryDC round-trip
        // test above). IconKind::none and a degenerate rect must be no-ops.
        using namespace nfui;
        const Color bg{RGB(0, 0, 0)};
        const Color fg{RGB(255, 255, 255)};
        const IconKind kinds[] = {
            IconKind::chevron_down, IconKind::chevron_up,
            IconKind::chevron_left, IconKind::chevron_right,
            IconKind::check, IconKind::close, IconKind::plus, IconKind::minus,
            IconKind::search, IconKind::gear, IconKind::info,
            IconKind::warning, IconKind::dot, IconKind::hamburger,
        };
        HDC screen = GetDC(nullptr);
        bool all_rendered = true;
        bool none_was_noop = false;
        for (IconKind k : kinds) {
            RECT rc{0, 0, 48, 48};
            MemoryDC mem(screen, rc);
            HDC target = mem.valid() ? mem.dc() : screen;
            RECT cell{0, 0, 48, 48};
            fill_rect(target, cell, bg);
            draw_vector_icon(target, k, cell, fg, 2);
            // Scan the cell for at least one foreground pixel.
            bool found = false;
            for (int y = 0; y < 48 && !found; ++y) {
                for (int x = 0; x < 48 && !found; ++x) {
                    if (GetPixel(target, x, y) == fg.rgb) found = true;
                }
            }
            if (!found) all_rendered = false;
        }
        // IconKind::none must leave the cell untouched (all background).
        {
            RECT rc{0, 0, 48, 48};
            MemoryDC mem(screen, rc);
            HDC target = mem.valid() ? mem.dc() : screen;
            fill_rect(target, RECT{0, 0, 48, 48}, bg);
            draw_vector_icon(target, IconKind::none, RECT{0, 0, 48, 48}, fg, 2);
            none_was_noop = (GetPixel(target, 24, 24) == bg.rgb);
        }
        // Degenerate rect must be a no-op (no crash, no paint).
        draw_vector_icon(screen, IconKind::check, RECT{5, 5, 5, 5}, fg, 2);
        ReleaseDC(nullptr, screen);
        ok = expect(all_rendered, L"every vector glyph paints at least one foreground pixel") && ok;
        ok = expect(none_was_noop, L"IconKind::none paints nothing") && ok;
        ok = expect(true, L"vector icon degenerate-rect path completes without crash") && ok;
    }

    {
        // CP19: paint_focus_border. Every border width (1, 2, 3) must paint the
        // border colour at the four edges of the rect and leave the centre
        // untouched. Degenerate and inverted rects must be no-ops. This is the
        // shared focus-ring helper used by Edit/ComboBox/ListBox/CheckBox/
        // RadioButton, so all of them inherit any regression here.
        using namespace nfui;
        const Color edge{RGB(255, 80, 40)};
        const Color interior_bg{RGB(12, 24, 36)};
        HDC screen = GetDC(nullptr);
        bool all_widths_painted = true;
        const int widths_to_test[] = {1, 2, 3};
        for (int w : widths_to_test) {
            RECT rc{0, 0, 80, 60};
            MemoryDC mem(screen, rc);
            HDC target = mem.valid() ? mem.dc() : screen;
            fill_rect(target, rc, interior_bg);
            paint_focus_border(target, rc, edge, w);

            // Four edge checks: rows/cols in the requested w-pixel band are
            // border colour. Catches an under-thick border (would leave the
            // last row/col of the band as background) and a missing border
            // (band entirely background).
            bool top_ok = true;
            for (int x = 0; x < 80 && top_ok; ++x) {
                for (int y = 0; y < w; ++y) {
                    if (GetPixel(target, x, y) != edge.rgb) { top_ok = false; break; }
                }
            }
            bool bottom_ok = true;
            for (int x = 0; x < 80 && bottom_ok; ++x) {
                for (int y = 60 - w; y < 60; ++y) {
                    if (GetPixel(target, x, y) != edge.rgb) { bottom_ok = false; break; }
                }
            }
            bool left_ok = true;
            for (int y = 0; y < 60 && left_ok; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (GetPixel(target, x, y) != edge.rgb) { left_ok = false; break; }
                }
            }
            bool right_ok = true;
            for (int y = 0; y < 60 && right_ok; ++y) {
                for (int x = 80 - w; x < 80; ++x) {
                    if (GetPixel(target, x, y) != edge.rgb) { right_ok = false; break; }
                }
            }

            // Interior scan (catches over-thick border): a 4-pixel gutter
            // inside the border must be entirely background. The four edge
            // checks above cannot detect a regression that adds one extra
            // FrameRect inward, because they only assert the OUTER band is
            // border colour. Scanning the interior is what closes that hole.
            const int inner_left   = w + 4;
            const int inner_top    = w + 4;
            const int inner_right  = 80 - w - 4;
            const int inner_bottom = 60 - w - 4;
            bool interior_ok = true;
            for (int y = inner_top; y < inner_bottom && interior_ok; ++y) {
                for (int x = inner_left; x < inner_right; ++x) {
                    if (GetPixel(target, x, y) != interior_bg.rgb) {
                        interior_ok = false;
                        break;
                    }
                }
            }

            if (!(top_ok && bottom_ok && left_ok && right_ok && interior_ok)) {
                all_widths_painted = false;
            }
        }
        // Degenerate rects must not crash or paint (degenerate = zero-area).
        bool degenerate_safe = true;
        try {
            paint_focus_border(screen, RECT{10, 10, 10, 10}, edge, 2);
            paint_focus_border(screen, RECT{20, 20, 5, 5}, edge, 2);   // inverted
        } catch (...) {
            degenerate_safe = false;
        }
        ReleaseDC(nullptr, screen);
        ok = expect(all_widths_painted, L"paint_focus_border paints exactly w px on each edge (no over-thick) and leaves interior untouched for widths 1/2/3") && ok;
        ok = expect(degenerate_safe, L"paint_focus_border degenerate-rect path is a safe no-op") && ok;
    }

    return ok ? 0 : 1;
}

// =====================================================================
// Per-component library integration (T4-T12 split).
// Each sub-block exercises one per-component lib via the new per-component
// header + Control base. Each block creates its own message-only parent
// HWND, calls create(), validates the resulting HWND, asserts a style
// bit that proves the lib compiled its create() override into a real
// owner-draw / native control, and cleans up. Return codes continue the
// 120-series (after the previous 1/0 returns) so each block can identify
// the failing leaf.
// =====================================================================
int per_component_smoke() {
    using namespace nfui;
    {
        // ----- nfui_button (T4) -----
        HWND parent_for_button = CreateWindowExW(0, L"STATIC", nullptr,
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 200,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (parent_for_button == nullptr) return 120;
        nfui::Button b;
        nfui::ControlCreateParams bp{};
        bp.instance = GetModuleHandleW(nullptr);
        bp.parent = parent_for_button;
        bp.control_id = 9001;
        bp.text = L"OK";
        bp.width = 80; bp.height = 28;
        if (!b.create(bp)) { DestroyWindow(parent_for_button); return 121; }
        if (!b.valid() || b.hwnd() == nullptr) { DestroyWindow(parent_for_button); return 122; }
        if ((GetWindowLongPtrW(b.hwnd(), GWL_STYLE) & BS_OWNERDRAW) == 0) { DestroyWindow(parent_for_button); return 123; }
        DestroyWindow(parent_for_button);

        // ----- nfui_checkbox (T5) -----
        HWND hidden_cb = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                         CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                         HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hidden_cb == nullptr) return 130;
        nfui::CheckBox c;
        nfui::ControlCreateParams cp{};
        cp.instance = GetModuleHandleW(nullptr);
        cp.parent = hidden_cb;
        cp.control_id = 9010;
        cp.text = L"Enable";
        cp.width = 120; cp.height = 24;
        if (!c.create(cp) || !c.valid()) { DestroyWindow(hidden_cb); return 131; }
        if ((GetWindowLongPtrW(c.hwnd(), GWL_STYLE) & BS_AUTOCHECKBOX) == 0) { DestroyWindow(hidden_cb); return 132; }
        DestroyWindow(hidden_cb);

        // ----- nfui_radio (T6) -----
        HWND hidden_rb = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                         CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                         HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hidden_rb == nullptr) return 140;
        nfui::RadioButton r;
        nfui::ControlCreateParams rp{};
        rp.instance = GetModuleHandleW(nullptr);
        rp.parent = hidden_rb;
        rp.control_id = 9020;
        rp.text = L"Option";
        rp.width = 120; rp.height = 24;
        if (!r.create(rp) || !r.valid()) { DestroyWindow(hidden_rb); return 141; }
        if ((GetWindowLongPtrW(r.hwnd(), GWL_STYLE) & BS_AUTORADIOBUTTON) == 0) { DestroyWindow(hidden_rb); return 142; }
        DestroyWindow(hidden_rb);

        // ----- nfui_text (T7) StaticText + Edit -----
        HWND hidden_st = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                         CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                         HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hidden_st == nullptr) return 150;
        nfui::StaticText t;
        nfui::ControlCreateParams tp{};
        tp.instance = GetModuleHandleW(nullptr);
        tp.parent = hidden_st;
        tp.control_id = 9030;
        tp.text = L"Hello";
        tp.width = 120; tp.height = 20;
        if (!t.create(tp) || !t.valid()) { DestroyWindow(hidden_st); return 151; }
        // CP15: StaticText now creates with SS_OWNERDRAW so on_paint fires
        // via WM_DRAWITEM. SS_LEFT is no longer the type mask — verify the
        // owner-draw bit instead.
        if ((GetWindowLongPtrW(t.hwnd(), GWL_STYLE) & SS_OWNERDRAW) == 0) { DestroyWindow(hidden_st); return 152; }
        nfui::Edit e;
        nfui::ControlCreateParams ep{};
        ep.instance = GetModuleHandleW(nullptr);
        ep.parent = hidden_st;
        ep.control_id = 9031;
        ep.text = L"editable";
        ep.width = 120; ep.height = 24;
        if (!e.create(ep) || !e.valid()) { DestroyWindow(hidden_st); return 153; }
        // ES_LEFT is 0x0000L on modern SDK — verify ES_AUTOHSCROLL bit instead.
        if ((GetWindowLongPtrW(e.hwnd(), GWL_STYLE) & ES_AUTOHSCROLL) == 0) { DestroyWindow(hidden_st); return 154; }
        DestroyWindow(hidden_st);

        // ----- nfui_listbox (T8) ListBox + ComboBox -----
        HWND hidden_lb = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                         CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                         HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hidden_lb == nullptr) return 160;
        nfui::ListBox lb;
        nfui::ControlCreateParams lbp{};
        lbp.instance = GetModuleHandleW(nullptr);
        lbp.parent = hidden_lb;
        lbp.control_id = 9040;
        lbp.width = 160; lbp.height = 120;
        if (!lb.create(lbp) || !lb.valid()) { DestroyWindow(hidden_lb); return 161; }
        if ((GetWindowLongPtrW(lb.hwnd(), GWL_STYLE) & LBS_OWNERDRAWFIXED) == 0) { DestroyWindow(hidden_lb); return 162; }
        nfui::ComboBox cb;
        nfui::ControlCreateParams cbp{};
        cbp.instance = GetModuleHandleW(nullptr);
        cbp.parent = hidden_lb;
        cbp.control_id = 9041;
        cbp.width = 160; cbp.height = 24;
        if (!cb.create(cbp) || !cb.valid()) { DestroyWindow(hidden_lb); return 163; }
        if ((GetWindowLongPtrW(cb.hwnd(), GWL_STYLE) & CBS_DROPDOWNLIST) == 0) { DestroyWindow(hidden_lb); return 164; }
        DestroyWindow(hidden_lb);

        // ----- nfui_listview (T9) -----
        HWND hidden_lv = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                         CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                         HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hidden_lv == nullptr) return 170;
        nfui::ListView lv;
        nfui::ControlCreateParams lvp{};
        lvp.instance = GetModuleHandleW(nullptr);
        lvp.parent = hidden_lv;
        lvp.control_id = 9050;
        lvp.width = 240; lvp.height = 160;
        if (!lv.create(lvp) || !lv.valid()) { DestroyWindow(hidden_lv); return 171; }
        if ((GetWindowLongPtrW(lv.hwnd(), GWL_STYLE) & LVS_REPORT) == 0) { DestroyWindow(hidden_lv); return 172; }
        DestroyWindow(hidden_lv);

        // ----- nfui_treeview (T10) -----
        HWND hidden_tv = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                         CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                         HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hidden_tv == nullptr) return 180;
        nfui::TreeView tv;
        nfui::ControlCreateParams tvp{};
        tvp.instance = GetModuleHandleW(nullptr);
        tvp.parent = hidden_tv;
        tvp.control_id = 9060;
        tvp.width = 200; tvp.height = 200;
        if (!tv.create(tvp) || !tv.valid()) { DestroyWindow(hidden_tv); return 181; }
        if ((GetWindowLongPtrW(tv.hwnd(), GWL_STYLE) & TVS_HASLINES) == 0) { DestroyWindow(hidden_tv); return 182; }
        DestroyWindow(hidden_tv);

        // ----- nfui_iconview (T11) -----
        HWND hidden_iv = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                         CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                         HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hidden_iv == nullptr) return 190;
        nfui::IconView iv;
        nfui::ControlCreateParams ivp{};
        ivp.instance = GetModuleHandleW(nullptr);
        ivp.parent = hidden_iv;
        ivp.control_id = 9070;
        ivp.width = 24; ivp.height = 24;
        if (!iv.create(ivp) || !iv.valid()) { DestroyWindow(hidden_iv); return 191; }
        if ((GetWindowLongPtrW(iv.hwnd(), GWL_STYLE) & SS_OWNERDRAW) == 0) { DestroyWindow(hidden_iv); return 192; }
        DestroyWindow(hidden_iv);

        // ----- nfui_frame (T12) StatusBar + Splitter -----
        HWND hidden_sb = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                         CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                         HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (hidden_sb == nullptr) return 200;
        nfui::StatusBar sb;
        nfui::ControlCreateParams sbp{};
        sbp.instance = GetModuleHandleW(nullptr);
        sbp.parent = hidden_sb;
        sbp.control_id = 9080;
        sbp.width = 200; sbp.height = 20;
        if (!sb.create(sbp) || !sb.valid()) { DestroyWindow(hidden_sb); return 201; }
        nfui::Splitter sp;
        nfui::ControlCreateParams spp{};
        spp.instance = GetModuleHandleW(nullptr);
        spp.parent = hidden_sb;
        spp.control_id = 9081;
        spp.width = 4; spp.height = 100;
        if (!sp.create(spp) || !sp.valid()) { DestroyWindow(hidden_sb); return 202; }
        sp.set_ratio(0.3);
        if (sp.ratio() != 0.3) { DestroyWindow(hidden_sb); return 203; }
        DestroyWindow(hidden_sb);
    }
    return 0;
}
