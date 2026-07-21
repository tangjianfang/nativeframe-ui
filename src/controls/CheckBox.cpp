#include <nfui/Controls/CheckBox.hpp>

namespace nfui {

bool CheckBox::create(const ControlCreateParams& params) noexcept {
    return create_native(L"BUTTON", params, BS_AUTOCHECKBOX);
}

} // namespace nfui