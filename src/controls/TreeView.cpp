#include <nfui/Controls/TreeView.hpp>
#include <commctrl.h>
namespace nfui {
bool TreeView::create(const ControlCreateParams& params) noexcept {
    return create_native(WC_TREEVIEWW, params, WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS);
}
}