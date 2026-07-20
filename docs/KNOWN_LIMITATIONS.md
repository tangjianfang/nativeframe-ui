# NativeFrame UI Known Limitations

Current V1 baseline limitations:

- No Ribbon.
- No full docking or floating panes.
- No visual designer.
- No complete Property Grid.
- No complete Data Grid.
- No plugin system.
- No MFC, ATL/WTL, BCG, or BCG-compatible API surface.
- Dark theme support is a full `ThemePalette` (background, surface, surface hover, border, text, secondary text, accent, accent hover, accent text, selection, selection text, danger, success, warning) consumed by owner-draw and custom-draw controls. Per-row hover highlighting and listview column-header theming remain deferred.
- Showcase and demo visuals are sample-local evaluation surfaces that consume the shared theme/paint/font primitives (`theme_palette`, `fill_rounded_rect`, `draw_text`, `FontCache`); stable framework guarantees remain the documented `nfui` APIs, not sample pixel output.
- Owner-draw and custom-draw control painting is not double-buffered; large repaints may flicker. Memory-DC buffering per control is deferred to a follow-up.
- `FontCache` retains one regular and one semibold `HFONT`, rebuilt when queried at a new DPI/point size. Per-paint consumers are unaffected; a long-lived `HFONT` obtained from the cache (e.g. stored via `WM_SETFONT`) can be invalidated when the cache is next queried at a different DPI. The `NativeFrameUIControls` demo re-applies the ListView font on `WM_DPICHANGED` to avoid this.
- DarkStudio currently favors mouse-driven visual review over deeper keyboard command coverage.
- SettingsDemo exposes save-state intent but does not persist settings to disk yet.
- ResourceGallery reuses the shared framework asset file instead of demo-specific resources.
- UI automation coverage is documented for future expansion, not yet implemented.
- Manual compatibility matrix entries remain pending until run on the target OS/display combinations.
