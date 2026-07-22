#pragma once

#include <nfui/Controls.hpp>

namespace nfui {

class CheckBox : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
private:
    // CP19: palette-accent focus ring. Suppresses the native dotted focus rect
    // (WM_UPDATEUISTATE UISF_HIDEFOCUS) and draws a 2px accent border around the
    // client on the post-paint pass when focused — a single, theme-aware focus
    // indicator instead of the system's low-contrast dotted outline.
    static LRESULT CALLBACK visual_subclass_proc(HWND hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR subclass_id,
                                                 DWORD_PTR ref_data) noexcept;
};

} // namespace nfui