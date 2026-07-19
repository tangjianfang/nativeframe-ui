# NativeFrame UI Layout DPI Theme Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Phase 6 baseline services for logical/pixel DPI conversion, simple layout calculations, theme tokens, and resilient persistence state serialization.

**Architecture:** Keep Phase 6 testable without a real UI by implementing pure value services first. DPI, layout, theme, and persistence modules expose small deterministic APIs that Workbench can later consume without coupling persistence to `HWND` or concrete controls.

**Tech Stack:** C++20, Win32 types, CMake, CTest.

---

## File Structure

- Create `include/nfui/Dpi.hpp` and `src/dpi/Dpi.cpp`: logical/pixel conversion helpers.
- Create `include/nfui/Layout.hpp` and `src/layout/Layout.cpp`: anchor/linear/splitter rectangle calculations.
- Create `include/nfui/Theme.hpp` and `src/theme/Theme.cpp`: theme mode enum and color tokens with high-contrast fallback.
- Create `include/nfui/Persistence.hpp` and `src/persistence/Persistence.cpp`: simple validated workbench state encode/decode.
- Modify `include/nfui/NativeFrameUI.hpp`, `CMakeLists.txt`, `tests/nativeframeui_smoke.cpp`, `README.md`, and `AGENTS.md`.

## Tasks

1. DPI helpers: write failing smoke assertions for 96/144 DPI conversions and font scaling, implement `DpiScale`.
2. Layout helpers: write failing smoke assertions for splitter and horizontal layout, implement `Rect`, `Size`, and layout functions.
3. Theme tokens: write failing smoke assertions for light/dark/high-contrast token selection, implement `ThemeMode`, `ThemeTokens`, and `theme_tokens`.
4. Persistence: write failing smoke assertions for round-tripping validated workbench state and rejecting corrupt strings, implement encode/decode.
5. Documentation and full validation: update docs, then run Debug/Release configure/build/CTest.

## Self-Review

- Spec coverage: covers Phase 6 pure-logic baseline for DPI, layout, theme, and persistence.
- Placeholder scan: no unresolved placeholders remain.
- Type consistency: all APIs are public `nfui` headers and included by `NativeFrameUI.hpp`.
