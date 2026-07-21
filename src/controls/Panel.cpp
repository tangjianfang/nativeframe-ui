#include <nfui/Controls/Panel.hpp>

namespace nfui {
bool Panel::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams panel_params = params;
    panel_params.style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
    return create_native(L"STATIC", panel_params, SS_WHITERECT);
}
} // namespace nfui