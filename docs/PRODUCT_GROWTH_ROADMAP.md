# NativeFrame UI Product Growth Roadmap

This roadmap follows the completed engineering baseline (Phases 1-8) and defines the next product and community growth cycle.

## Strategic Position

NativeFrame UI is a pure Win32 C++20 static UI library for teams that need native Windows integration, explicit resource ownership, DPI-aware layouts, and a smaller dependency surface than a full application framework.

The commercial value is reduced integration risk, predictable native behavior, long-term Windows compatibility, and a credible migration path from raw Win32 or legacy UI frameworks. The open-source value is a transparent implementation, permissive MIT licensing, reproducible builds, clear contribution boundaries, and runnable examples.

## Phase 9: Product Credibility and Community Foundation

### Objectives

- Make the repository understandable in five minutes.
- Make a first build reproducible without private context.
- Establish contribution, security, support, and release expectations.
- Create evidence that the project is suitable for evaluation by commercial teams.

### Deliverables

- Accurate README with Quick Start, project status, product positioning, and demo matrix.
- CI workflow for x64 Debug/Release configure, build, CTest, and boundary checks.
- `CONTRIBUTING.md`, `SECURITY.md`, `SUPPORT.md`, and issue/PR templates.
- Release checklist covering API review, license scan, resource integration, Windows matrix, and sample verification.
- Public API stability policy and semantic versioning guidance.
- First-party integration example that builds against the static library.

### Commercial Acceptance

- A new engineer can configure, build, and run the sample from a clean Windows machine using documented commands.
- A technical evaluator can identify supported OS/toolchain/runtime and known limitations without contacting maintainers.
- A team can understand the support boundary and how to report security issues.

### Open-Source Acceptance

- Contributors can find coding, testing, review, and branching rules.
- Issues are categorized as bug, feature, documentation, or integration request.
- Every pull request runs the same local validation path in CI.

## Phase 10: Showcase Visual System

### Objectives

- Demonstrate that native Win32 can produce modern, branded, attractive desktop UI.
- Separate visual showcase code from framework-core APIs.
- Produce screenshots and a runnable executable suitable for README and release announcements.

### Deliverables

- `NativeFrameUIShowcase.exe` target.
- Modern dashboard layout with sidebar, command bar, cards, status badges, inspector, and data workspace.
- Light and dark visual modes using centralized theme tokens.
- GDI painting helpers for cards, separators, badges, focus, hover, and selection states.
- Logical-unit layout and DPI-aware font/icon sizing.
- Showcase walkthrough and screenshot capture instructions.

### Current Implementation Snapshot

- `samples/NativeFrameUIShowcase/ShowcaseView.*` keeps all showcase-only painting local to the sample.
- The showcase ships with a command-bar light/dark toggle and `WM_DPICHANGED` handling.
- `docs/SHOWCASE_GUIDE.md` records launch, screenshot, DPI, and boundary expectations.

### Design Constraints

- Do not copy BCG, MFC, Qt, or proprietary resources, names, branding, or source.
- Keep showcase-only painting and composition out of the stable core API unless independently useful.
- Preserve keyboard navigation, high-contrast fallback, and native HWND interoperability.

### Commercial Acceptance

- The showcase communicates product value without requiring a technical explanation.
- Visual behavior is stable enough for screenshots, demos, and customer evaluation.
- The sample demonstrates a realistic application shell rather than isolated controls.

### Open-Source Acceptance

- Showcase code is readable and independently buildable.
- Each visual technique is documented without hiding required dependencies.
- Contributors can add a new showcase screen without modifying core ownership rules.

## Phase 11: Demo Portfolio and Adoption

### Objectives

- Turn one showcase into a portfolio of focused, searchable examples.
- Give users copyable patterns for common desktop application surfaces.
- Create a sustainable path for external examples and commercial pilots.

### Deliverables

- `NativeFrameUIDarkStudio.exe`: dark-theme application shell and navigation.
- `NativeFrameUISettingsDemo.exe`: settings categories, forms, validation, and inspector layout.
- `NativeFrameUIResourceGallery.exe`: dialogs, menus, icons, strings, and explicit resource integration.
- Demo matrix describing purpose, source location, expected visual result, and relevant APIs.
- Migration examples from raw Win32 message handling to `nfui::Window`, controls, commands, layout, and persistence.
- Release screenshots, short recordings, and performance/resource observations.

### Current Implementation Snapshot

- DarkStudio focuses on a dark preview shell with a native status bar.
- SettingsDemo focuses on native form controls and save-state visibility.
- ResourceGallery focuses on explicit string/menu/dialog/icon/bitmap resource loading.
- `docs/DEMO_MATRIX.md` and the per-demo walkthroughs document launch expectations and known limits.

### Commercial Acceptance

- A prospective user can select a demo matching their application scenario.
- Integration pilots can start from a focused example rather than the full Workbench.
- Demo behavior and limitations are explicit enough for procurement and architecture review.

### Open-Source Acceptance

- Examples remain small, licensed, and independently understandable.
- New contributors can propose a demo through a documented template.
- Demo additions do not weaken automated boundary or compatibility checks.

## Execution Chain

1. Complete Phase 9 documentation and CI foundation.
2. Verify Phase 9 on a clean checkout.
3. Implement Phase 10 visual system and Showcase.
4. Capture automated and manual evidence for Phase 10.
5. Implement Phase 11 focused demos and adoption documentation.
6. Run the full validation matrix and publish a release candidate.

Each phase must pass Debug/Release CMake and CTest validation before the next phase begins.
