#include <nfui/Controls/StaticText.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Paint.hpp>

namespace nfui {

bool StaticText::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams static_params = params;
    static_params.style &= ~WS_TABSTOP;
    // CP15: SS_OWNERDRAW routes Control::subclass_proc's DRAWITEM dispatch
    // to on_paint. Without it the system default paints via
    // WM_CTLCOLORSTATIC (COLOR_BTNFACE brush, light grey in light mode and
    // dark grey in dark mode), which gives the static a hard gray box that
    // doesn't track the framework palette. SS_NOTIFY preserves the
    // pre-existing click forwarding contract.
    return create_native(L"STATIC", static_params, SS_OWNERDRAW | SS_NOTIFY);
}

void StaticText::set_caption(const std::wstring& text) noexcept {
    if (hwnd() != nullptr) {
        // SetWindowTextW routes through WM_SETTEXT → the base subclass_proc
        // updates the cached caption_ before DefSubclassProc triggers the
        // repaint, so by the time on_paint runs the new text is already in
        // place. We do not cache it ourselves to avoid a duplicate copy.
        SetWindowTextW(hwnd(), text.c_str());
    }
}

void StaticText::on_paint(HDC dc, const PaintState& state) noexcept {
    const ThemePalette* pal = palette();
    const ThemePalette& p = pal ? *pal : theme_palette(ThemeMode::light);
    const Color bg = style_.background.value_or(p.background);
    const Color fg = state.enabled
        ? style_.foreground.value_or(p.text)
        : p.text_secondary;
    const RECT& b = state.bounds;
    MemoryDC mem(dc, b);
    HDC target = mem.valid() ? mem.dc() : dc;
    const RECT paint_bounds = mem.valid()
        ? RECT{0, 0, b.right - b.left, b.bottom - b.top}
        : b;
    fill_rect(target, paint_bounds, bg);

    FontCache* cache = fonts();
    HFONT font = nullptr;
    if (cache != nullptr) {
        const int dpi = dpi_of(hwnd());
        const int pt = style_.font_size_pt.value_or(font_pt::ui);
        font = style_.use_semibold.value_or(false)
            ? cache->semibold(dpi, pt)
            : cache->regular(dpi, pt);
    }

    // Honour the explicit horizontal/vertical padding fields. They are declared
    // as logical-pixel paddings around the text rect, converted to device
    // pixels here so the inset stays constant across DPI changes.
    const DpiScale scale(dpi_of(hwnd()));
    const int pad_x = scale.logical_to_pixels(style_.horizontal_padding.value_or(0));
    const int pad_y = scale.logical_to_pixels(style_.vertical_padding.value_or(0));
    const RECT text_bounds{
        paint_bounds.left   + pad_x,
        paint_bounds.top    + pad_y,
        paint_bounds.right  - pad_x,
        paint_bounds.bottom - pad_y,
    };

    UINT format = 0;
    switch (style_.align_h.value_or(StaticTextAlignH::left)) {
    case StaticTextAlignH::center: format |= DT_CENTER; break;
    case StaticTextAlignH::right:  format |= DT_RIGHT;  break;
    case StaticTextAlignH::left:
    default:                       format |= DT_LEFT;   break;
    }
    switch (style_.align_v.value_or(StaticTextAlignV::middle)) {
    case StaticTextAlignV::top:    format |= DT_TOP;    break;
    case StaticTextAlignV::bottom: format |= DT_BOTTOM; break;
    case StaticTextAlignV::middle:
    default:                       format |= DT_VCENTER; break;
    }
    const bool single_line = style_.single_line.value_or(true);
    if (single_line) {
        format |= DT_SINGLELINE;
        if (style_.end_ellipsis.value_or(true)) {
            // DT_END_ELLIPSIS requires a null-terminated buffer; draw_text
            // hands text.data() to DrawTextW which may write back. caption_
            // is always null-terminated so this is safe.
            format |= DT_END_ELLIPSIS;
        }
    } else {
        // Multi-line: word-wrap. DrawTextW does not accept DT_VCENTER with
        // DT_WORDBREAK, so strip the vertical alignment for wrapped text and
        // let it stack from the top — Top is the conventional wrap behaviour.
        format |= DT_WORDBREAK;
        format &= ~DT_VCENTER;
        format &= ~DT_TOP;
        format &= ~DT_BOTTOM;
    }

    draw_text(target, text_bounds, caption(), font, fg, format);
}

} // namespace nfui