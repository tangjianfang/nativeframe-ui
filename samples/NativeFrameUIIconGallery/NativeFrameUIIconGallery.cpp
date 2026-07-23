// NativeFrameUIIconGallery -- CP31 polished showcase for the vector icon system.
//
// Two clear sections: a neutral-coloured "Icon Gallery" grid and a real
// "Icon Buttons" row. Semantic colouring (warning/info/danger/success) is
// isolated to a small dedicated row. Light / Dark / High-Contrast toggles live
// in a top toolbar separated from the gallery by a divider. Hovering a gallery
// card highlights it with surface_hover and shows a tooltip-like caption.

#include <nfui/NativeFrameUI.hpp>
#include <nfui/VectorIcon.hpp>

#include "NativeFrameUIResource.h"

#include <windowsx.h>

#include <array>
#include <string_view>

namespace {

struct GlyphEntry {
    nfui::IconKind kind;
    std::wstring_view label;
};

constexpr std::array<GlyphEntry, 14> kGlyphs{{
    { nfui::IconKind::chevron_down,  L"Chevron Down" },
    { nfui::IconKind::chevron_up,    L"Chevron Up" },
    { nfui::IconKind::chevron_left,  L"Chevron Left" },
    { nfui::IconKind::chevron_right, L"Chevron Right" },
    { nfui::IconKind::check,         L"Check" },
    { nfui::IconKind::close,         L"Close" },
    { nfui::IconKind::plus,          L"Plus" },
    { nfui::IconKind::minus,         L"Minus" },
    { nfui::IconKind::search,        L"Search" },
    { nfui::IconKind::gear,          L"Settings" },
    { nfui::IconKind::info,          L"Info" },
    { nfui::IconKind::warning,       L"Warning" },
    { nfui::IconKind::dot,           L"Dot" },
    { nfui::IconKind::hamburger,     L"Menu" },
}};

constexpr int kColumns = 7;

struct SemanticEntry {
    nfui::IconKind kind;
    std::wstring_view label;
};

// Re-use existing glyphs for the semantic states row.  Danger has no dedicated
// glyph, so the close (X) glyph is coloured with palette.danger to signal the
// semantic intent without adding new icon assets.
constexpr std::array<SemanticEntry, 4> kSemantic{{
    { nfui::IconKind::warning, L"Warning" },
    { nfui::IconKind::info,    L"Info" },
    { nfui::IconKind::close,   L"Danger" },
    { nfui::IconKind::check,   L"Success" },
}};

[[nodiscard]] nfui::Color semantic_color(const nfui::ThemePalette& p,
                                         nfui::IconKind k) noexcept {
    switch (k) {
    case nfui::IconKind::warning: return p.warning;
    case nfui::IconKind::info:    return p.info;
    case nfui::IconKind::close:   return p.danger;
    case nfui::IconKind::check:   return p.success;
    default:                      return p.text;
    }
}

constexpr int id_btn_light    = 2001;
constexpr int id_btn_dark     = 2002;
constexpr int id_btn_hc       = 2003;
constexpr int id_btn_search   = 2011;
constexpr int id_btn_settings = 2012;
constexpr int id_btn_add      = 2013;
constexpr int id_btn_close    = 2014;

class IconGalleryWindow final : public nfui::Window {
public:
    explicit IconGalleryWindow(HINSTANCE instance)
        : instance_(instance), resources_(instance),
          palette_(nfui::theme_palette(nfui::ThemeMode::light)) {}

    ~IconGalleryWindow() noexcept override { destroy_icons(); }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIIconGalleryWindow",
            L"NativeFrame UI — Vector Icon Gallery",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT, CW_USEDEFAULT, 1040, 620,
        };
        if (!create(params)) return false;
        apply_window_icon();
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));

        if (!build_controls()) return false;
        layout();

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_ERASEBKGND:
            return 1;  // custom-painted; never let the system gray bg show
        case WM_SIZE:
            layout();
            return 0;
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            const int idx = hit_test_card(x, y);
            const bool moved = (idx >= 0) &&
                               (x != hover_pt_.x || y != hover_pt_.y);
            if (idx != hover_idx_ || moved) {
                hover_idx_ = idx;
                hover_pt_ = { x, y };
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            if (!tracking_mouse_) {
                TRACKMOUSEEVENT tme{
                    sizeof(TRACKMOUSEEVENT),
                    TME_LEAVE,
                    hwnd(),
                    HOVER_DEFAULT
                };
                if (TrackMouseEvent(&tme)) tracking_mouse_ = true;
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            if (hover_idx_ != -1) {
                hover_idx_ = -1;
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            const int code = HIWORD(wparam);
            if (code == BN_CLICKED) {
                if (id == id_btn_light)       set_theme(nfui::ThemeMode::light);
                else if (id == id_btn_dark)  set_theme(nfui::ThemeMode::dark);
                else if (id == id_btn_hc)    set_theme(nfui::ThemeMode::high_contrast);
            }
            return 0;
        }
        case WM_DPICHANGED: {
            dpi_ = nfui::DpiScale(HIWORD(wparam));
            auto* suggested = reinterpret_cast<RECT*>(lparam);
            if (suggested != nullptr) {
                SetWindowPos(hwnd(), nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }
            layout();
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd(), &ps);
            if (dc != nullptr) {
                paint_gallery(dc);
                EndPaint(hwnd(), &ps);
            }
            return 0;
        }
        case WM_DESTROY:
            destroy_icons();
            PostQuitMessage(0);
            return 0;
        default:
            return nfui::Window::handle_message(message, wparam, lparam);
        }
    }

private:
    template <typename ControlT>
    [[nodiscard]] bool make_child(ControlT& control, int id, std::wstring_view text,
                                  DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP) noexcept {
        nfui::ControlCreateParams params{instance_, hwnd(), id, text, 0, 0, 100, 28, style};
        control.inject_theme(&palette_, &fonts_);
        return control.create(params);
    }

    [[nodiscard]] bool build_controls() noexcept {
        if (!make_child(btn_light_,   id_btn_light,   L"Light"))   return false;
        if (!make_child(btn_dark_,    id_btn_dark,    L"Dark"))    return false;
        if (!make_child(btn_hc_,      id_btn_hc,      L"High Contrast")) return false;

        // Icon buttons: each carries a leading vector glyph.
        if (!make_child(btn_search_,   id_btn_search,   L"Search"))  return false;
        if (!make_child(btn_settings_, id_btn_settings, L"Settings")) return false;
        if (!make_child(btn_add_,       id_btn_add,      L"Add"))     return false;
        if (!make_child(btn_close_,     id_btn_close,    L"Close"))   return false;

        auto apply_icon = [](nfui::Button& b, nfui::IconKind k) {
            nfui::ButtonStyle s = b.style();
            s.icon = k;
            b.set_style(s);
        };
        apply_icon(btn_search_,   nfui::IconKind::search);
        apply_icon(btn_settings_, nfui::IconKind::gear);
        apply_icon(btn_add_,      nfui::IconKind::plus);
        apply_icon(btn_close_,    nfui::IconKind::close);
        return true;
    }

    void set_theme(nfui::ThemeMode mode) noexcept {
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        for (nfui::Control* c : all_controls()) {
            if (c != nullptr) c->set_palette(&palette_);
        }
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    std::array<nfui::Control*, 7> all_controls() noexcept {
        return { &btn_light_, &btn_dark_, &btn_hc_,
                 &btn_search_, &btn_settings_, &btn_add_, &btn_close_ };
    }

    struct PageMetrics {
        int pad;
        int gap;
        int btn_h;
        int toolbar_bottom;
        int gallery_header_top;
        int gallery_top;
        int gallery_bottom;
        int card_w;
        int card_h;
        int semantic_header_y;
        int semantic_top;
        int semantic_bottom;
        int semantic_card_w;
        int semantic_left;
        int ibtn_y;
        int caption_y;
    };

    [[nodiscard]] PageMetrics compute_page_metrics(int cw, int ch) const noexcept {
        PageMetrics m{};
        m.pad = dpi_.logical_to_pixels(16);
        m.gap = dpi_.logical_to_pixels(8);
        m.btn_h = dpi_.logical_to_pixels(28);
        const int heading_h = dpi_.logical_to_pixels(24);
        const int subtitle_h = dpi_.logical_to_pixels(20);
        const int section_gap = dpi_.logical_to_pixels(12);
        const int semantic_h = dpi_.logical_to_pixels(64);
        const int semantic_header_h = dpi_.logical_to_pixels(18);
        const int caption_h = dpi_.logical_to_pixels(20);
        const int caption_gap = dpi_.logical_to_pixels(8);

        m.toolbar_bottom = m.pad + m.btn_h + m.gap;
        m.gallery_header_top = m.toolbar_bottom;
        m.gallery_top = m.gallery_header_top + heading_h + subtitle_h + m.gap;

        m.ibtn_y = ch - m.pad - m.btn_h;
        m.caption_y = m.ibtn_y - caption_gap - caption_h;
        m.semantic_bottom = m.caption_y - section_gap;
        m.semantic_top = m.semantic_bottom - semantic_h;
        m.semantic_header_y = m.semantic_top - semantic_header_h - m.gap;
        m.gallery_bottom = m.semantic_header_y - section_gap;

        const int rows =
            (static_cast<int>(kGlyphs.size()) + kColumns - 1) / kColumns;
        m.card_w = (cw - 2 * m.pad) / kColumns;
        m.card_h = (m.gallery_bottom > m.gallery_top)
                       ? (m.gallery_bottom - m.gallery_top) / rows
                       : 0;

        const int semantic_count = static_cast<int>(kSemantic.size());
        m.semantic_card_w = (cw - 2 * m.pad - (semantic_count - 1) * m.gap)
                            / semantic_count;
        const int semantic_total = semantic_count * m.semantic_card_w
                                   + (semantic_count - 1) * m.gap;
        m.semantic_left = m.pad + ((cw - 2 * m.pad) - semantic_total) / 2;

        return m;
    }

    [[nodiscard]] RECT card_rect(const PageMetrics& m, size_t i) const noexcept {
        const int inner_pad = dpi_.logical_to_pixels(8);
        const int col = static_cast<int>(i) % kColumns;
        const int row = static_cast<int>(i) / kColumns;
        const int cx = m.pad + col * m.card_w;
        const int cy = m.gallery_top + row * m.card_h;
        return { cx + inner_pad, cy + inner_pad,
                 cx + m.card_w - inner_pad, cy + m.card_h - inner_pad };
    }

    [[nodiscard]] int hit_test_card(int x, int y) const noexcept {
        if (hwnd() == nullptr) return -1;
        RECT client{};
        GetClientRect(hwnd(), &client);
        const PageMetrics m = compute_page_metrics(client.right, client.bottom);
        if (m.card_w <= 0 || m.card_h <= 0) return -1;
        for (size_t i = 0; i < kGlyphs.size(); ++i) {
            const RECT r = card_rect(m, i);
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
                return static_cast<int>(i);
        }
        return -1;
    }

    void layout() noexcept {
        if (hwnd() == nullptr) return;
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        const int cw = client.right - client.left;
        const PageMetrics m = compute_page_metrics(cw, client.bottom);

        const int btn_w = dpi_.logical_to_pixels(96);
        int x = cw - m.pad - btn_w;
        move(btn_hc_,    x, m.pad, btn_w, m.btn_h); x -= btn_w + m.gap;
        move(btn_dark_,  x, m.pad, btn_w, m.btn_h); x -= btn_w + m.gap;
        move(btn_light_, x, m.pad, btn_w, m.btn_h);

        const int ibtn_w = dpi_.logical_to_pixels(120);
        int ix = m.pad;
        move(btn_search_,   ix, m.ibtn_y, ibtn_w, m.btn_h); ix += ibtn_w + m.gap;
        move(btn_settings_, ix, m.ibtn_y, ibtn_w, m.btn_h); ix += ibtn_w + m.gap;
        move(btn_add_,      ix, m.ibtn_y, ibtn_w, m.btn_h); ix += ibtn_w + m.gap;
        move(btn_close_,    ix, m.ibtn_y, ibtn_w, m.btn_h);

        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void move(nfui::Control& c, int x, int y, int w, int h) noexcept {
        if (c.valid()) {
            SetWindowPos(c.hwnd(), nullptr, x, y, w, h,
                         SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }

    void draw_icon_card(HDC dc, const RECT& card, int radius, int glyph_box,
                        int label_h, int glyph_label_gap, int stroke,
                        nfui::IconKind kind, nfui::Color icon_color,
                        std::wstring_view label, HFONT label_font,
                        bool hovered) const noexcept {
        const nfui::Color fill = hovered ? palette_.surface_hover : palette_.surface;
        nfui::fill_rounded_rect(dc, card, radius, fill, palette_.border);
        if (card.right <= card.left || card.bottom <= card.top) return;

        const int content_h = glyph_box + glyph_label_gap + label_h;
        const int gy = card.top + ((card.bottom - card.top) - content_h) / 2;
        const int gx = card.left + ((card.right - card.left) - glyph_box) / 2;
        RECT glyph{ gx, gy, gx + glyph_box, gy + glyph_box };
        nfui::draw_vector_icon(dc, kind, glyph, icon_color, stroke);

        RECT label_rc{ card.left, glyph.bottom + glyph_label_gap,
                         card.right, glyph.bottom + glyph_label_gap + label_h };
        nfui::draw_text(dc, label_rc, label, label_font,
                        palette_.text_secondary,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    void paint_tooltip(HDC dc, HFONT font) const noexcept {
        if (hover_idx_ < 0 ||
            hover_idx_ >= static_cast<int>(kGlyphs.size())) return;

        const auto label = kGlyphs[hover_idx_].label;
        RECT text_rc{ 0, 0, 0, 0 };
        nfui::draw_text(dc, text_rc, label, font, palette_.text,
                        DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
        const int tw = text_rc.right - text_rc.left;
        const int th = text_rc.bottom - text_rc.top;
        if (tw <= 0 || th <= 0) return;

        const int tip_pad = dpi_.logical_to_pixels(6);
        const int offset = dpi_.logical_to_pixels(12);
        int x = hover_pt_.x + offset;
        int y = hover_pt_.y - offset - th - 2 * tip_pad;
        int w = tw + 2 * tip_pad;
        int h = th + 2 * tip_pad;

        RECT client{};
        GetClientRect(hwnd(), &client);
        if (x + w > client.right) x = client.right - w - tip_pad;
        if (x < client.left)      x = client.left + tip_pad;
        if (y < client.top)      y = client.top + tip_pad;
        if (y + h > client.bottom) y = client.bottom - h - tip_pad;

        const RECT tip{ x, y, x + w, y + h };
        nfui::fill_rounded_rect(dc, tip, dpi_.logical_to_pixels(4),
                                palette_.surface, palette_.border);
        const RECT text_draw{ tip.left + tip_pad, tip.top + tip_pad,
                              tip.right - tip_pad, tip.bottom - tip_pad };
        nfui::draw_text(dc, text_draw, label, font, palette_.text,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void paint_gallery(HDC dc) noexcept {
        RECT client{};
        GetClientRect(hwnd(), &client);
        const int cw = client.right - client.left;
        const int ch = client.bottom - client.top;
        if (cw <= 0 || ch <= 0) return;

        nfui::MemoryDC mem(dc, client);
        HDC target = mem.valid() ? mem.dc() : dc;
        const RECT bounds = mem.valid() ? RECT{0, 0, cw, ch} : client;

        nfui::fill_rect(target, bounds, palette_.background);

        const PageMetrics m = compute_page_metrics(cw, ch);
        const HFONT heading_font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::ui);
        const HFONT label_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::ui);

        // Toolbar divider under the theme toggle buttons.
        const int divider_y = m.toolbar_bottom - m.gap / 2;
        nfui::draw_line(target, POINT{ m.pad, divider_y },
                        POINT{ cw - m.pad, divider_y }, palette_.border,
                        dpi_.logical_to_pixels(1));

        // Icon Gallery section header.
        const int heading_h = dpi_.logical_to_pixels(24);
        const int subtitle_h = dpi_.logical_to_pixels(20);
        RECT header{ m.pad, m.gallery_header_top,
                     cw - m.pad, m.gallery_header_top + heading_h };
        nfui::draw_text(target, header, L"Icon Gallery", heading_font,
                        palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT sub{ m.pad, header.bottom,
                  cw - m.pad, header.bottom + subtitle_h };
        nfui::draw_text(target, sub,
            L"GDI-primitive glyphs on a 16x16 grid — DPI-scaled and neutral. "
            L"Semantic colours are shown separately below.",
            label_font, palette_.text_secondary,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Main gallery grid: every icon rendered in the neutral text colour.
        const int card_radius = dpi_.logical_to_pixels(8);
        const int glyph_box = dpi_.logical_to_pixels(36);
        const int label_h = dpi_.logical_to_pixels(20);
        const int glyph_label_gap = dpi_.logical_to_pixels(6);
        const int stroke = dpi_.logical_to_pixels(2);

        for (size_t i = 0; i < kGlyphs.size(); ++i) {
            const RECT card = card_rect(m, i);
            if (card.right <= card.left || card.bottom <= card.top) continue;
            const bool hovered = (hover_idx_ == static_cast<int>(i));
            draw_icon_card(target, card, card_radius, glyph_box, label_h,
                           glyph_label_gap, stroke, kGlyphs[i].kind,
                           palette_.text, kGlyphs[i].label, label_font,
                           hovered);
        }

        // Semantic states row.
        const int semantic_header_h = dpi_.logical_to_pixels(18);
        RECT sem_label{ m.pad, m.semantic_header_y,
                        cw - m.pad, m.semantic_header_y + semantic_header_h };
        nfui::draw_text(target, sem_label, L"Semantic states", heading_font,
                        palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const int inner_pad = dpi_.logical_to_pixels(8);
        const int sem_radius = dpi_.logical_to_pixels(6);
        const int sem_glyph = dpi_.logical_to_pixels(28);
        const int sem_label_h = dpi_.logical_to_pixels(16);
        for (size_t i = 0; i < kSemantic.size(); ++i) {
            const int col = static_cast<int>(i);
            const int left = m.semantic_left
                             + col * (m.semantic_card_w + m.gap)
                             + inner_pad;
            const int right = left + m.semantic_card_w - 2 * inner_pad;
            const RECT card{ left, m.semantic_top + inner_pad,
                             right, m.semantic_bottom - inner_pad };
            if (card.right <= card.left || card.bottom <= card.top) continue;
            draw_icon_card(target, card, sem_radius, sem_glyph, sem_label_h,
                           glyph_label_gap, stroke, kSemantic[i].kind,
                           semantic_color(palette_, kSemantic[i].kind),
                           kSemantic[i].label, label_font, false);
        }

        // Icon Buttons section header.
        const int caption_h = dpi_.logical_to_pixels(20);
        RECT cap{ m.pad, m.caption_y, cw - m.pad, m.caption_y + caption_h };
        nfui::draw_text(target, cap, L"Icon Buttons", heading_font,
                        palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Hover tooltip caption for the gallery card under the cursor.
        paint_tooltip(target, label_font);
    }

    void apply_window_icon() noexcept {
        if (!resources_.has_icon(IDI_NFUI_APP)) return;
        large_icon_ = static_cast<HICON>(LoadImageW(instance_,
            MAKEINTRESOURCEW(IDI_NFUI_APP), IMAGE_ICON,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
        small_icon_ = static_cast<HICON>(LoadImageW(instance_,
            MAKEINTRESOURCEW(IDI_NFUI_APP), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
        if (large_icon_) SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon_));
        if (small_icon_) SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon_));
    }

    void destroy_icons() noexcept {
        if (large_icon_) { DestroyIcon(large_icon_); large_icon_ = nullptr; }
        if (small_icon_) { DestroyIcon(small_icon_); small_icon_ = nullptr; }
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::FontCache fonts_{};
    nfui::ThemePalette palette_{};
    nfui::ThemeMode mode_{nfui::ThemeMode::light};
    nfui::DpiScale dpi_{};

    int hover_idx_{ -1 };
    POINT hover_pt_{};
    bool tracking_mouse_{ false };

    nfui::Button btn_light_, btn_dark_, btn_hc_;
    nfui::Button btn_search_, btn_settings_, btn_add_, btn_close_;
    HICON large_icon_{};
    HICON small_icon_{};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }
    IconGalleryWindow window(instance);
    if (!window.create_main(show_command)) return 2;
    return app.run();
}
