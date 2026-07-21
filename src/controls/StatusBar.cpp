#include <nfui/Controls/StatusBar.hpp>

#include <commctrl.h>

namespace nfui {
bool StatusBar::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams status_params = params;
    status_params.style = WS_CHILD | WS_VISIBLE;
    return create_native(STATUSCLASSNAMEW, status_params, SBARS_SIZEGRIP);
}
} // namespace nfui