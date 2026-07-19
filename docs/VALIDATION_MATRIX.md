# NativeFrame UI Validation Matrix

This matrix tracks the V1 validation path for the current implementation baseline and the product-growth sample portfolio.

The product-growth validation extension is defined in [PRODUCT_GROWTH_ROADMAP.md](PRODUCT_GROWTH_ROADMAP.md). Phase 9 adds clean-checkout/CI checks, Phase 10 adds visual Showcase checks, and Phase 11 adds per-demo build and walkthrough checks.

## Automated Validation

Run both configurations:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
```

CTest currently includes:

- `NativeFrameUISmokeTest`: initialization, resources, diagnostics, handles, windows, command routing, controls, DPI, layout, theme, persistence, and basic USER/GDI stability.
- `NativeFrameUISmokeTest`: also verifies that `NativeFrameUIShowcase`, `NativeFrameUIDarkStudio`, `NativeFrameUISettingsDemo`, and `NativeFrameUIResourceGallery` are emitted by generated build artifacts.
- `NativeFrameUIBoundaryCheck`: source boundary scan for forbidden BCG/MFC/ATL/WTL dependency markers in implementation surfaces.

## Manual Visual Validation

Use `NativeFrameUIWorkbench.exe`, not `NativeFrameUISmokeTest.exe`, for visual checks.

| Area | Windows 10 22H2 | Windows 11 23H2/24H2 | Notes |
| --- | --- | --- | --- |
| Debug startup | Pending manual run | Pending manual run | Workbench opens visible main window. |
| Release startup | Pending manual run | Pending manual run | Workbench opens visible main window. |
| DPI 100/125/150/200% | Pending manual run | Pending manual run | Verify readable panes/status/tabs. |
| Mixed-DPI monitors | Pending manual run | Pending manual run | Move window across monitors. |
| Light theme | Pending manual run | Pending manual run | Verify native controls remain readable. |
| Dark theme baseline | Pending manual run | Pending manual run | Current tokens exist; full dark rendering remains future work. |
| High contrast | Pending manual run | Pending manual run | Verify text remains visible. |
| Chinese resources | Pending manual run | Pending manual run | Add localized resources before final V1. |
| English resources | Pending manual run | Pending manual run | Current baseline resources are English. |
| Mouse workflow | Pending manual run | Pending manual run | Menu, context menu, tabs, selection. |
| Keyboard workflow | Pending manual run | Pending manual run | Menu accelerators and tab order need future expansion. |

## Product Growth Sample Checks

### Automated

| Target | Configure/build | CTest evidence | Boundary/resource note |
| --- | --- | --- | --- |
| `NativeFrameUIShowcase` | Required in Debug and Release | Smoke test confirms generated build artifact registration | Linked with `nfui_add_resources`; no new framework API surface |
| `NativeFrameUIDarkStudio` | Required in Debug and Release | Smoke test confirms generated build artifact registration | Uses shared icon resource only |
| `NativeFrameUISettingsDemo` | Required in Debug and Release | Smoke test confirms generated build artifact registration | Uses native controls and shared icon resource |
| `NativeFrameUIResourceGallery` | Required in Debug and Release | Smoke test confirms generated build artifact registration | Explicitly loads string/menu/dialog/icon/bitmap/toolbar resources |

### Manual

| Target | Startup | DPI | Theme | Keyboard | Resource | Screenshot | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `NativeFrameUIShowcase` | Pending manual run | Pending manual run | Pending manual run | Pending manual run | Shared icon only | Pending manual capture | Verify command-bar light/dark toggle and hover states |
| `NativeFrameUIDarkStudio` | Pending manual run | Pending manual run | Pending manual run | Pending manual run | Shared icon only | Pending manual capture | Verify navigation status updates and dark canvas readability |
| `NativeFrameUISettingsDemo` | Pending manual run | Pending manual run | Native-control readability pending | Pending manual run | Shared icon only | Pending manual capture | Verify save-state label/status bar stay in sync |
| `NativeFrameUIResourceGallery` | Pending manual run | Pending manual run | Light shell pending | Pending manual run | Pending manual run | Pending manual capture | Verify menu, dialog, icon, bitmap, and toolbar marker paths |

## Boundary Requirements

- No MFC dependency.
- No ATL/WTL dependency.
- No BCG dependency, source, resources, branding, class names, or compatibility claim.
- Static-library resources must be integrated explicitly through `nfui_add_resources`.

## Product Growth Validation

| Phase | Automated evidence | Manual evidence |
| --- | --- | --- |
| 9 | Clean configure/build/CTest, boundary scan, CI parity | README Quick Start and contributor workflow review |
| 10 | Showcase target build, smoke build-artifact registration, theme/layout compile path | Light/dark screenshots, DPI, keyboard, high-contrast review |
| 11 | Every demo target builds, smoke build-artifact registration remains green, boundary scan stays green | Demo walkthroughs, copyability, resource and screenshot observations |
