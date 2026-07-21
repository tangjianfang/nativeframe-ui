#include <nfui/Controls/Splitter.hpp>

namespace nfui {
bool Splitter::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams splitter_params = params;
    splitter_params.style = WS_CHILD | WS_VISIBLE;
    return create_native(L"STATIC", splitter_params, SS_GRAYRECT);
}
void Splitter::set_ratio(double ratio) noexcept {
    if (ratio < 0.0) ratio_ = 0.0;
    else if (ratio > 1.0) ratio_ = 1.0;
    else ratio_ = ratio;
}
double Splitter::ratio() const noexcept { return ratio_; }
} // namespace nfui