#include <nfui/Controls/TabControl.hpp>
#include <commctrl.h>

namespace nfui {
bool TabControl::create(const ControlCreateParams& params) noexcept {
    return create_native(WC_TABCONTROLW, params, WS_CLIPSIBLINGS);
}
} // namespace nfui