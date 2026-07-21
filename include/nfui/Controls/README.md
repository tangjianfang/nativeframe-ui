# nfui Controls — per-component library index

The Controls umbrella re-exports 16 per-component static libraries.
Each is linkable independently for minimal-footprint consumers, or all
at once via the `NativeFrameUI::NativeFrameUI` umbrella target.

## Per-component libraries

| Library | Header(s) | Surface |
|---------|-----------|---------|
| `NativeFrameUI::nfui_button` | `<nfui/Controls/Button.hpp>` | `Button` |
| `NativeFrameUI::nfui_checkbox` | `<nfui/Controls/CheckBox.hpp>` | `CheckBox` |
| `NativeFrameUI::nfui_radio` | `<nfui/Controls/RadioButton.hpp>` | `RadioButton` |
| `NativeFrameUI::nfui_text` | `<nfui/Controls/StaticText.hpp>`, `<nfui/Controls/Edit.hpp>` | `StaticText`, `Edit` |
| `NativeFrameUI::nfui_listbox` | `<nfui/Controls/ListBox.hpp>`, `<nfui/Controls/ComboBox.hpp>` | `ListBox`, `ComboBox` |
| `NativeFrameUI::nfui_listview` | `<nfui/Controls/ListView.hpp>` | `ListView` |
| `NativeFrameUI::nfui_treeview` | `<nfui/Controls/TreeView.hpp>` | `TreeView` |
| `NativeFrameUI::nfui_iconview` | `<nfui/Controls/IconView.hpp>` | `IconView` |
| `NativeFrameUI::nfui_frame` | `<nfui/Controls/StatusBar.hpp>`, `<nfui/Controls/TabControl.hpp>`, `<nfui/Controls/Tooltip.hpp>`, `<nfui/Controls/ProgressBar.hpp>`, `<nfui/Controls/Panel.hpp>`, `<nfui/Controls/Splitter.hpp>` | `StatusBar`, `TabControl`, `Tooltip`, `ProgressBar`, `Panel`, `Splitter` |

All per-component libraries depend on `nfui_control_base`, which owns
the `Control` base class (`subclass_proc`, `create_native`,
owner-draw dispatch, hover-state plumbing).

## Picking the right library

Use **one** library per control your app needs. For example, a
settings dialog that has a button, two checkboxes, and a status bar
links:

```cmake
target_link_libraries(MyApp PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_control_base
    NativeFrameUI::nfui_button
    NativeFrameUI::nfui_checkbox
    NativeFrameUI::nfui_frame  # for StatusBar
)
```

## When to use the umbrella

If your app uses 5+ components, the convenience of linking
`NativeFrameUI::NativeFrameUI` outweighs the binary-size savings of
per-component linking. Use the umbrella for prototyping; switch to
per-component for release builds once surface is stable.