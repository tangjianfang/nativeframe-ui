#include <nfui/Controls/Edit.hpp>

namespace nfui {

bool Edit::create(const ControlCreateParams& params) noexcept {
    return create_native(L"EDIT", params, WS_BORDER | ES_LEFT | ES_AUTOHSCROLL);
}

} // namespace nfui
