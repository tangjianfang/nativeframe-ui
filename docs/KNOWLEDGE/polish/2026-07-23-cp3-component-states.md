# CP3 component state audit

Date: 2026-07-23

## Findings and changes

- `Button` already self-paints all four state inputs. Pressed now uses a darker accent blend instead of sharing hover, and focused uses an accent border. Disabled retains the legible secondary text treatment.
- `StaticText` now applies `text_secondary` while disabled; its custom foreground remains used when enabled.
- `ProgressBar` now desaturates the fill while disabled, while retaining the configured bar color when enabled.
- `ListView` now enables `LVS_EX_TRACKSELECT` and consumes its custom-draw hook, so hot rows use `surface_hover` while selected rows retain selection colors. The base control dispatcher forwards item custom draw to the leaf override.
- Native `CheckBox`, `RadioButton`, `Edit`, and `ComboBox` remain native themed controls. Their common-controls renderer supplies hover, pressed/check, focus, and disabled states without duplicating platform behavior. Combo dropdown arrow and edit focus border are therefore OS/theme-owned.
- `Panel` and `Splitter` already self-paint border/surface and accent feedback during drag. `TabControl` and `TreeView` remain native themed controls; native selection, hover/focus, and disabled rendering is preferable to fragile owner drawing.
- `ListBox` already has selected, per-row hover, and disabled-row colors. `IconView` remains a simple owner-drawn icon surface; spacing/selection semantics are not exposed by its current API and were not expanded in this minimal polish pass.

## Scope note

The audit intentionally avoids adding palette tokens or new ownership/interaction APIs. Changes are limited to state visibility gaps that can be fixed with existing palette tokens and the existing custom-draw extension point.
