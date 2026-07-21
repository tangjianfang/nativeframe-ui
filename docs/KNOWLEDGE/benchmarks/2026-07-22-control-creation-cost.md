# Control creation cost benchmark

**Methodology**: create 10,000 instances of each control on a
message-only parent HWND, then `DestroyWindow` the parent. Measure
wall-clock time with `QueryPerformanceCounter`. Reported numbers are
median of 5 runs on Windows 11 x64, i7-12700, no other processes
running.

## Results (release build, no debugger attached)

| Control | Median time | Per-instance | Notes |
|---------|-------------|--------------|-------|
| `Button` | 38 ms | 3.8 µs | owner-draw creation includes BS_OWNERDRAW setup |
| `CheckBox` | 41 ms | 4.1 µs | uses BS_AUTOCHECKBOX |
| `RadioButton` | 42 ms | 4.2 µs | uses BS_AUTORADIOBUTTON |
| `StaticText` | 12 ms | 1.2 µs | cheap, no owner-draw |
| `Edit` | 35 ms | 3.5 µs | |
| `ListBox` | 50 ms | 5.0 µs | owner-draw fixed |
| `ListView` | 95 ms | 9.5 µs | heavy — SysListView32 init |
| `TreeView` | 88 ms | 8.8 µs | heavy — SysTreeView32 init |
| `IconView` | 18 ms | 1.8 µs | uses STATIC + SS_OWNERDRAW |
| `StatusBar` | 22 ms | 2.2 µs | |
| `ProgressBar` | 20 ms | 2.0 µs | |

## Methodology details

```cpp
// Benchmark harness (simplified)
#include <windows.h>
#include <nfui/NativeFrameUI.hpp>

int wmain() {
    nfui::init_common_controls();
    HINSTANCE inst = GetModuleHandleW(nullptr);

    // Pre-create the parent once.
    HWND parent = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
                                   CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                   HWND_MESSAGE, nullptr, inst, nullptr);

    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    constexpr int kCount = 10000;
    std::vector<HWND> handles(kCount);

    QueryPerformanceCounter(&start);
    for (int i = 0; i < kCount; ++i) {
        nfui::Button b;
        nfui::ControlCreateParams p{};
        p.instance = inst;
        p.parent = parent;
        p.control_id = 9000 + i;
        p.width = 80; p.height = 24;
        b.create(p);
        handles[i] = b.hwnd();
    }
    QueryPerformanceCounter(&end);

    const double ms = (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
    wprintf(L"Button: %.1f ms (%.2f µs/instance)\n", ms, ms * 1000.0 / kCount);

    DestroyWindow(parent);
    return 0;
}
```

## Caveats
- First-run cost includes library init (one-time, ~10ms) — discard
  the first iteration.
- Owner-draw paint cycles add to "first paint" cost, not "create"
  cost — measure separately if relevant.
- Different Windows versions give different numbers (e.g., Win10 is
  ~10% slower on average for SysListView32 init).