# Learning: MemoryDC Scope-Before-EndPaint Audit

**Date:** 2026-07-21
**Triggered by:** Phase A1 polish audit
**Related commits:** `dd4768f` (R6), `15f25e6` (R8), `380a330` (R9), `4c3533b` (C2)

## Background

`nfui::MemoryDC` allocates an offscreen `HBITMAP` and a memory `HDC` in its
constructor and flushes via `BitBlt` to the original `HDC` in its destructor.
The destruction must happen **before** `EndPaint` is called — once
`EndPaint` releases the `BeginPaint` HDC, any `BitBlt` against it is a no-op
or worse.

## Symptom

If a sample's `case WM_PAINT` body declares `nfui::MemoryDC mem(hdc, client);`
at function scope, the destructor runs at the closing `}` of the WM_PAINT
block — **after** `EndPaint`. The flicker-free buffer never reaches the
window.

This was the root cause of the original R6 spec gap (commit `dd4768f`).

## Audit Result (2026-07-21)

All 8 `case WM_PAINT` bodies in the repository correctly observe the invariant:

| File | Status |
|---|---|
| `samples/NativeFrameUIControls/NativeFrameUIControls.cpp:114-135` | ✅ nested scope at L120-134 |
| `samples/NativeFrameUIDarkStudio/NativeFrameUIDarkStudio.cpp:125-129` | ✅ scope = `paint_shell()` function body, closes before `EndPaint` |
| `samples/NativeFrameUISettingsDemo/NativeFrameUISettingsDemo.cpp:91-102` | ✅ nested scope at L97-101 |
| `samples/NativeFrameUIResourceGallery/NativeFrameUIResourceGallery.cpp:114-129` | ✅ nested scope at L123-127 |
| `samples/NativeFrameUICharts/NativeFrameUICharts.cpp:166-181` | ✅ nested scope at L176-180 |
| `samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp:113-128` | ✅ nested scope at L122-126 |
| `src/charts/ChartView.cpp:117-131` | ✅ nested scope at L126-130 |
| `src/controls/Controls.cpp:179-193` | ✅ no MemoryDC (direct `on_paint` call) |

## Applies to

Any future `case WM_PAINT` body that uses `nfui::MemoryDC` for flicker-free
paint. The pattern must be:

```cpp
case WM_PAINT: {
    PAINTSTRUCT paint{};
    HDC hdc = BeginPaint(hwnd(), &paint);
    RECT client{};
    GetClientRect(hwnd(), &client);
    {
        nfui::MemoryDC mem(hdc, client);   // ← inner scope opens
        HDC target = mem.valid() ? mem.dc() : hdc;
        // ... paint body ...
    }                                       // ← MemoryDC destructor flushes HERE
    EndPaint(hwnd(), &paint);               // ← after flush
    return 0;
}
```

## Prevention check

- Code review: any new `case WM_PAINT` body using `nfui::MemoryDC` must have
  the inner `{ }` scope wrapping the MemoryDC declaration, with the
  closing brace BEFORE `EndPaint`.
- Mechanical: `grep -A30 "case WM_PAINT" path/to/file.cpp` and verify the
  brace structure matches the pattern above.