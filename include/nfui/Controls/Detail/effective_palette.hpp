#pragma once

// CP-A final: shared chrome-paint helper. Previously duplicated verbatim in
// 8 control translation units (ComboBox, Edit, ListBox, ListView, Scrollbar,
// Slider, TabControl, TreeView). The body is identical: a magic-statics
// fallback to the light palette when the control has no injected palette
// pointer, so the chrome subclass procs — which all run with a const pointer
// — have a single source of truth for the "no palette injected" case.
//
// Lives in Detail/ rather than at the controls top level: this is a private
// implementation helper, not a public API. Consumers must continue to use
// Control::palette() / Control::set_palette(); this helper only resolves a
// raw pointer to a usable reference so subclass procs don't have to.
//
// Inline so multiple TUs including this header don't trigger ODR violations;
// the function is small enough that the compiler will fold it down.
#include <nfui/Theme.hpp>

namespace nfui::detail {

[[nodiscard]] inline const ThemePalette& effective_palette(const ThemePalette* injected) noexcept {
    static const ThemePalette fallback = theme_palette(ThemeMode::light);
    return injected != nullptr ? *injected : fallback;
}

} // namespace nfui::detail
