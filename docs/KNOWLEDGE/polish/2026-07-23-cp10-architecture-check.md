# CP10D — Architecture dependency-direction check

Date: 2026-07-23
Branch: `polish/CP10-architecture-check`

## Problem

`docs/ARCHITECTURE.md` spells out a layered dependency direction (core stays
independent of controls/command/theme/layout; persistence never touches
HWND/Window; theme/resource never depend on business modules, etc.), but only
the *anti-BCG/MFC* rule was mechanically enforced (`verify_boundaries.ps1` →
`NativeFrameUIBoundaryCheck`). Nothing stopped a header from silently
introducing a wrong-direction `#include <nfui/...>` edge.

## Solution

Added `tools/verify_architecture.ps1`, registered as the CTest test
`NativeFrameUIArchitectureCheck`, running in parallel with the existing
`NativeFrameUIBoundaryCheck`.

The script:

1. Scans every public header under `include/nfui/**.hpp`.
2. Extracts each `#include <nfui/...>` edge with a regex.
3. Maps both the containing header and the include target to their owning
   module via `Get-ModuleName` (core, dpi, theme, font, icon, paint, resource,
   command, layout, persistence, controls, charts; `NativeFrameUI.hpp` is the
   exempt umbrella).
4. Enforces two complementary rule sets that mirror ARCHITECTURE.md's
   "Detailed dependency rules" and "Forbidden dependencies":
   - **Per-module allow-list** — a header may only include its own module or a
     lower module it is explicitly allowed to depend on. Catches
     `core -> controls/command/theme/layout`, etc.
   - **Named-forbidden patterns** — finer-grained than the module level.
     Notably `persistence -> Window.hpp/Dialog.hpp`: `Window` lives in the
     `core` module (which persistence *may* depend on), so the specific
     HWND/Window edge must be forbidden explicitly. Also reinforces
     `command -> TreeView`, `theme/resource -> business`, `layout -> controls`.

Exit code 1 with a per-edge diagnostic (`header:line : 'from' must not depend
on 'to'`) on any violation; unmapped headers also fail so the map stays honest
as new modules are added.

## Why allow-list *and* named-forbidden

Module-granularity alone can't express "persistence may use core but not the
Window part of core." Splitting core would be intrusive; a small explicit
blocklist per source module captures the doc's named rules exactly.

## Scope note

Only public headers (`include/nfui`) — the API contract surface — are scanned,
matching the task scope. Implementation `.cpp` / private headers under `src/`
are a possible future extension but carry more false-positive risk (private
per-module headers, legit cross-module impl includes).

## Verification

- Clean tree: `56 nfui edge(s) across 36 header(s)` → pass.
- Fault injection: temporarily adding `#include <nfui/Controls/Button.hpp>` to
  `Handle.hpp` (core) is correctly reported as a `core` violation.
- `cmake --build --preset x64-debug` succeeds.
- `NONFUI_SKIP_DIALOG=1 ctest --preset x64-debug` → **3/3 pass**
  (Smoke, Boundary, Architecture).
