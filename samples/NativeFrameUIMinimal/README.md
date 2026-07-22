# NativeFrameUIMinimal

The "smallest possible product" sample. Proves the per-component
library split (introduced in P2.1) delivers real value by linking
**only four** framework libraries to ship a working window with one
button — instead of the full 13-library umbrella.

## What it demonstrates

- **Standalone `WinMain`** — registers its own `WNDCLASSEXW` window
  class (`NFUI_MINIMAL`) and runs a manual `GetMessageW` loop. Does not
  use `nfui::Application` or `nfui::Window` wrappers.
- **One button** — `nfui::Button` created with the documented
  `inject_theme` → `create` path. A click pops a `MessageBoxW` with
  `Button clicked!` to prove the WM_COMMAND chain reaches user code.
- **Per-component link** — `CMakeLists.txt` links only the libraries
  needed to instantiate `nfui::Button`:
  - `NativeFrameUI::nfui_core`
  - `NativeFrameUI::nfui_theme`
  - `NativeFrameUI::nfui_control_base`
  - `NativeFrameUI::nfui_button`
  
  The full umbrella (`NativeFrameUI::NativeFrameUI`) is intentionally
  not linked. This is the value proposition for consumers who want a
  minimal dependency footprint.

## Key controls

- `nfui::Button` — the only control wrapper used.
- `nfui::ThemePalette` — single `nfui::theme_palette(light)` call.
- `nfui::FontCache` — instantiated but not strictly needed for a single
  button; included to mirror the standard consumer recipe.

## Theme support

None. This is a deliberate minimum — adding a theme toggle would
require code from the theme module's higher-level API surface, which
would obscure the "four libraries is enough" message.

## Build & run

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target NativeFrameUIMinimal
./out/build/x64-debug/Debug/NativeFrameUIMinimal.exe
```

The sample's `CMakeLists.txt` is also standalone-buildable:

```powershell
cmake -S samples/NativeFrameUIMinimal -B build/NativeFrameUIMinimal
cmake --build build/NativeFrameUIMinimal --config Debug
```

Same recipe works for `x64-release`.

## Known limitations

- Uses raw `WNDCLASSEXW` + `DefWindowProcW` rather than the
  `nfui::Application` + `nfui::Window` wrappers, so it does not
  exercise dispatcher routing, `OwnedHwnd`, or `CommandRouter`.
- Only one button, no layout, no resource script beyond the framework
  defaults. This is intentional — the demo exists to show the link
  surface, not to demonstrate features.
- No DPI handling. The button position is fixed at the same device
  pixels at every DPI; consumers wiring their own DPI helper should
  start from one of the larger samples.
- Does not load any custom resources; relies on `nfui_add_resources`
  to pull in `resources/NativeFrameUI.rc` only because the framework
  build system always attaches it.
