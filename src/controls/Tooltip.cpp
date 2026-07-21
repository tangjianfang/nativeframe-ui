#include <nfui/Controls/Tooltip.hpp>
#include <commctrl.h>

namespace nfui {
bool Tooltip::create(const ControlCreateParams& params) noexcept {
    return create_native(TOOLTIPS_CLASSW, params, 0);
}
} // namespace nfui