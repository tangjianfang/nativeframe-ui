# Tutorial 01: Hello, Window

**Goal**: create a minimal NativeFrameUI app that opens a window
with one styled button.

**Time**: ~15 minutes
**Prerequisites**: VS 2022, CMake 3.25+, NativeFrameUI repo cloned.

## Step 1 — Set up the project

Create a new directory and CMakeLists.txt:

```cmake
cmake_minimum_required(VERSION 3.25)
project(HelloNFUI LANGUAGES CXX)
add_executable(HelloNFUI WIN32 main.cpp)
target_link_libraries(HelloNFUI PRIVATE NativeFrameUI::NativeFrameUI)
```

Add `main.cpp`:

```cpp
#include <nfui/NativeFrameUI.hpp>
#include <nfui/Controls/Button.hpp>

#include <windows.h>

namespace {
constexpr int ID_BUTTON = 1;
LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_COMMAND && LOWORD(w) == ID_BUTTON) {
        MessageBoxW(hwnd, L"Hello!", L"From NativeFrameUI", MB_OK);
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    nfui::init_common_controls();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"HelloNFUIWindow";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Hello, NativeFrameUI",
                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 320, 120,
                                 nullptr, nullptr, inst, nullptr);

    nfui::Button btn;
    nfui::ThemePalette palette = nfui::theme_palette(nfui::ThemeMode::light);
    nfui::FontCache fonts;
    btn.inject_theme(&palette, &fonts);

    nfui::ControlCreateParams p{};
    p.instance = inst;
    p.parent = hwnd;
    p.control_id = ID_BUTTON;
    p.text = L"Click me";
    p.x = 100; p.y = 40;
    p.width = 120; p.height = 32;
    btn.create(p);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
```

## Step 2 — Configure and build

```powershell
cd HelloNFUI
cmake -B build -DCMAKE_PREFIX_PATH=C:/path/to/NativeFrameUI/out/build/x64-release
cmake --build build --config Release
```

## Step 3 — Run

```powershell
./build/Release/HelloNFUI.exe
```

A window appears with one centered button. Clicking shows a
MessageBox. Press Escape or close the window to exit.

## What you learned

- `nfui::init_common_controls()` initializes ComCtl32 v6 (call once).
- A `nfui::Button` is just a Win32 HWND underneath — you create it
  with `ControlCreateParams` like any other.
- `inject_theme(palette, fonts)` gives the button the Claude palette
  colors and font caching — without it, the button uses defaults.
- WM_COMMAND routes button clicks via the standard Win32 command id
  mechanism.

## Next steps

- Tutorial 02: layout — add multiple controls with SplitterLayout.
- Tutorial 03: data binding — connect a ListBox to a model.
- Recipe: styled button with brand colors.