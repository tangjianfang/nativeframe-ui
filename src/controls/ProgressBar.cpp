#include <nfui/Controls/ProgressBar.hpp>
#include <commctrl.h>

namespace nfui {
bool ProgressBar::create(const ControlCreateParams& params) noexcept {
    return create_native(PROGRESS_CLASSW, params, PBS_SMOOTH);
}
} // namespace nfui