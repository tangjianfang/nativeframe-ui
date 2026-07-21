#include <nfui/Controls/ComboBox.hpp>

namespace nfui {
bool ComboBox::create(const ControlCreateParams& params) noexcept {
    return create_native(L"COMBOBOX", params, CBS_DROPDOWNLIST | WS_VSCROLL);
}
} // namespace nfui
