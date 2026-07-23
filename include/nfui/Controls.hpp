#pragma once

// Pure forwarding umbrella. `Control`, `ControlCreateParams`, and `PaintState`
// live in <nfui/Controls/Control.hpp>; each leaf class lives in its own header
// (one per per-component lib). This umbrella re-exports everything so existing
// `#include <nfui/Controls.hpp>` consumers continue to work unchanged.

#include <nfui/Controls/Control.hpp>

#include <nfui/Controls/Button.hpp>
#include <nfui/Controls/CheckBox.hpp>
#include <nfui/Controls/RadioButton.hpp>
#include <nfui/Controls/StaticText.hpp>
#include <nfui/Controls/Edit.hpp>
#include <nfui/Controls/ListBox.hpp>
#include <nfui/Controls/ComboBox.hpp>
#include <nfui/Controls/ListView.hpp>
#include <nfui/Controls/TreeView.hpp>
#include <nfui/Controls/IconView.hpp>
#include <nfui/Controls/StatusBar.hpp>
#include <nfui/Controls/TabControl.hpp>
#include <nfui/Controls/Tooltip.hpp>
#include <nfui/Controls/ProgressBar.hpp>
#include <nfui/Controls/Panel.hpp>
#include <nfui/Controls/Slider.hpp>
#include <nfui/Controls/Splitter.hpp>