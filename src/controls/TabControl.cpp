#include <nfui/Controls/TabControl.hpp>
#include <commctrl.h>

namespace nfui {
bool TabControl::create(const ControlCreateParams& params) noexcept {
    return create_native(WC_TABCONTROLW, params, WS_CLIPSIBLINGS);
}

bool TabControl::set_padding(int cx, int cy) noexcept {
    // CP8A additive: forward TCM_SETPADDING so consumers can tune the gap
    // between a tab's icon/text and the surrounding tab border. Native
    // TabControl default is approximately (cx=6, cy=3). The ComCtl32 v6
    // implementation accepts negative padding (treated as zero) so we
    // clamp on the consumer side instead of relying on undocumented
    // rounding. SendMessage returns void in the documented signature;
    // no failure mode other than "no HWND" — guarded by valid().
    if (!valid()) {
        return false;
    }
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    SendMessageW(hwnd(), TCM_SETPADDING, 0, MAKELPARAM(static_cast<WORD>(cx), static_cast<WORD>(cy)));
    return true;
}
} // namespace nfui
