// NativeFrameUIIconGallery -- CP32 redesign of the vector-icon showcase.
//
// Layout matches the cp28 redesign reference (3 rows x 8 columns of cards):
//   - Title "Vector Icon Gallery" (xl=28 bold) + descriptive subtitle
//   - Theme toggle top-right (Light / Dark / High Contrast buttons)
//   - Search bar: rounded rect with magnifier icon + placeholder caption
//   - Filter chip row: 5 pills (All / Navigation / Status / Actions / Media);
//     clicking a chip updates the selected state and repaints
//   - 24-card icon grid laid out in 3 rows x 8 columns of 120x120 cards;
//     glyphs come from nfui::IconKind (the enum ships 14 glyphs so a few
//     repeat to fill the grid)
//   - "Icon Buttons (Button::set_icon)" section header + 4 coral Buttons
//
// All visual chrome is drawn via foundation helpers (fill_rounded_rect,
// fill_ellipse, draw_text, draw_vector_icon). No public APIs change.

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

// CP32: 24 cards in 3 rows x 8 columns. The IconKind enum currently ships
// 14 glyphs (chevrons, check/close/plus/minus, search/gear, info/warning,
// dot, hamburger); we cycle through them with mild repetition so the grid
// stays visually balanced without inventing new glyph assets.
constexpr std::array<GlyphEntry, 24> kGlyphs{{
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
    { nfui::IconKind::gear,          L"Settings" },
    { nfui::IconKind::hamburger,     L"Menu" },
    { nfui::IconKind::info,          L"Info" },
    { nfui::IconKind::warning,       L"Warning" },
    { nfui::IconKind::dot,           L"Dot" },
    { nfui::IconKind::chevron_down,  L"Chevron Down" },
    { nfui::IconKind::chevron_up,    L"Chevron Up" },
    { nfui::IconKind::chevron_left,  L"Chevron Left" },
    { nfui::IconKind::chevron_right, L"Chevron Right" },
    { nfui::IconKind::check,         L"Check" },
    { nfui::IconKind::close,         L"Close" },
    { nfui::IconKind::plus,          L"Plus" },
    { nfui::IconKind::minus,         L"Minus" },
}};

constexpr int kColumns = 8;
constexpr int kRows    = 3;

struct ChipEntry {
    std::wstring_view label;
};

constexpr std::array<ChipEntry, 5> kChips{{
    { L"All" },
    { L"Navigation" },
    { L"Status" },
    { L"Actions" },
    { L"Media" },
}};

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
            CW_USEDEFAULT, CW_USEDEFAULT, 1240, 720,
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
        case WM_LBUTTONDOWN: {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            RECT client{};
            GetClientRect(hwnd(), &client);
            const int hit = hit_test_chip(client.right - client.left,
                                          client.bottom - client.top, x, y);
            if (hit >= 0) {
                selected_chip_ = hit;
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        }
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
        if (!make_child(btn_light_,   id_btn_light,   L"Light"))          return false;
        if (!make_child(btn_dark_,    id_btn_dark,    L"Dark"))           return false;
        if (!make_child(btn_hc_,      id_btn_hc,      L"High Contrast"))  return false;

        // Bottom icon-button row: each carries a leading vector glyph via
        // ButtonStyle::icon, mirroring the "Button::set_icon" demo.
        if (!make_child(btn_search_,   id_btn_search,   L"Search"))   return false;
        if (!make_child(btn_settings_, id_btn_settings, L"Settings")) return false;
        if (!make_child(btn_add_,      id_btn_add,      L"Add"))      return false;
        if (!make_child(btn_close_,    id_btn_close,    L"Close"))    return false;

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
        int cw;
        int ch;
        // Vertical bands (top → bottom).
        int title_y;
        int subtitle_y;
        int search_y;
        int search_h;
        int chip_y;
        int chip_h;
        int grid_y;
        int grid_bottom;
        int section_y;
        int buttons_y;
        // Grid dimensions.
        int card_w;
        int card_h;
        int grid_x;
        // Chip dimensions.
        int chip_w;
    };

    [[nodiscard]] PageMetrics compute_page_metrics(int cw, int ch) const noexcept {
        PageMetrics m{};
        m.cw = cw;
        m.ch = ch;
        m.pad = dpi_.logical_to_pixels(24);
        m.gap = dpi_.logical_to_pixels(16);

        // Vertical placement.
        m.title_y    = m.pad;
        m.subtitle_y = m.title_y    + dpi_.logical_to_pixels(40);
        m.search_y   = m.subtitle_y + dpi_.logical_to_pixels(28);
        m.search_h   = dpi_.logical_to_pixels(44);
        m.chip_y     = m.search_y   + m.search_h + dpi_.logical_to_pixels(12);
        m.chip_h     = dpi_.logical_to_pixels(32);

        m.card_w = dpi_.logical_to_pixels(120);
        m.card_h = dpi_.logical_to_pixels(120);
        m.grid_y = m.chip_y + m.chip_h + dpi_.logical_to_pixels(20);
        m.grid_bottom = m.grid_y
                        + kRows * m.card_h
                        + (kRows - 1) * m.gap;
        m.section_y = m.grid_bottom + dpi_.logical_to_pixels(20);
        m.buttons_y = m.section_y  + dpi_.logical_to_pixels(36);

        // Centre the grid horizontally so narrow windows still look balanced.
        const int grid_w = kColumns * m.card_w + (kColumns - 1) * m.gap;
        m.grid_x = m.pad + ((cw - 2 * m.pad) - grid_w) / 2;
        if (m.grid_x < m.pad) m.grid_x = m.pad;

        // Chip width sized to fit 5 chips across the content area.
        const int chip_gap = dpi_.logical_to_pixels(8);
        m.chip_w = ((cw - 2 * m.pad) - 4 * chip_gap) / 5;
        return m;
    }

    [[nodiscard]] RECT card_rect(const PageMetrics& m, size_t i) const noexcept {
        const int col = static_cast<int>(i) % kColumns;
        const int row = static_cast<int>(i) / kColumns;
        const int cx = m.grid_x + col * (m.card_w + m.gap);
        const int cy = m.grid_y  + row * (m.card_h + m.gap);
        return { cx, cy, cx + m.card_w, cy + m.card_h };
    }

    [[nodiscard]] RECT chip_rect(const PageMetrics& m, size_t i) const noexcept {
        const int chip_gap = dpi_.logical_to_pixels(8);
        const int x = m.pad + static_cast<int>(i) * (m.chip_w + chip_gap);
        return { x, m.chip_y, x + m.chip_w, m.chip_y + m.chip_h };
    }

    [[nodiscard]] int hit_test_chip(int cw, int ch, int x, int y) const noexcept {
        if (hwnd() == nullptr) return -1;
        const PageMetrics m = compute_page_metrics(cw, ch);
        for (size_t i = 0; i < kChips.size(); ++i) {
            const RECT r = chip_rect(m, i);
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
                return static_cast<int>(i);
        }
        return -1;
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

        // Theme toggle: 3 segmented buttons at the top-right.
        const int btn_w = dpi_.logical_to_pixels(104);
        const int btn_h = dpi_.logical_to_pixels(32);
        int x = cw - m.pad - btn_w;
        move(btn_hc_,    x, m.title_y + dpi_.logical_to_pixels(4),
              btn_w, btn_h); x -= btn_w - dpi_.logical_to_pixels(1);
        move(btn_dark_,  x, m.title_y + dpi_.logical_to_pixels(4),
              btn_w, btn_h); x -= btn_w - dpi_.logical_to_pixels(1);
        move(btn_light_, x, m.title_y + dpi_.logical_to_pixels(4),
              btn_w, btn_h);

        // Bottom row of icon buttons.
        const int ibtn_w = dpi_.logical_to_pixels(120);
        int ix = m.pad;
        move(btn_search_,   ix, m.buttons_y, ibtn_w, btn_h);
        ix += ibtn_w + m.gap;
        move(btn_settings_, ix, m.buttons_y, ibtn_w, btn_h);
        ix += ibtn_w + m.gap;
        move(btn_add_,      ix, m.buttons_y, ibtn_w, btn_h);
        ix += ibtn_w + m.gap;
        move(btn_close_,    ix, m.buttons_y, ibtn_w, btn_h);

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
        const int offset  = dpi_.logical_to_pixels(12);
        int x = hover_pt_.x + offset;
        int y = hover_pt_.y - offset - th - 2 * tip_pad;
        int w = tw + 2 * tip_pad;
        int h = th + 2 * tip_pad;

        RECT client{};
        GetClientRect(hwnd(), &client);
        if (x + w > client.right)  x = client.right  - w - tip_pad;
        if (x < client.left)       x = client.left   + tip_pad;
        if (y < client.top)        y = client.top    + tip_pad;
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

        // CP32: page typography uses the new xs/sm/base/md/xl scale.
        const HFONT title_font    = fonts_.bold    (dpi_.dpi(), nfui::font_pt::xl);
        const HFONT subtitle_font = fonts_.regular (dpi_.dpi(), nfui::font_pt::sm);
        const HFONT chip_font     = fonts_.regular (dpi_.dpi(), nfui::font_pt::xs);
        const HFONT section_font  = fonts_.semibold(dpi_.dpi(), nfui::font_pt::md);
        const HFONT label_font    = fonts_.regular (dpi_.dpi(), nfui::font_pt::sm);

        // Title + subtitle.
        const int title_h = dpi_.logical_to_pixels(40);
        RECT title_rc{ m.pad, m.title_y, cw - m.pad, m.title_y + title_h };
        nfui::draw_text(target, title_rc, L"Vector Icon Gallery", title_font,
                        palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        const int subtitle_h = dpi_.logical_to_pixels(20);
        RECT subtitle_rc{ m.pad, m.subtitle_y, cw - m.pad, m.subtitle_y + subtitle_h };
        nfui::draw_text(target, subtitle_rc,
            L"GDI-primitive glyphs on a 16x16 grid — DPI-scaled, palette-coloured. "
            L"Toggle the theme top-right.",
            subtitle_font, palette_.text_secondary,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Search bar: pill-shaped surface with magnifier icon + placeholder.
        const RECT search_rc{ m.pad, m.search_y,
                              cw - m.pad, m.search_y + m.search_h };
        nfui::fill_rounded_rect(target, search_rc, m.search_h / 2,
                                palette_.surface, palette_.border);
        const int icon_box = dpi_.logical_to_pixels(20);
        const int pad_x    = dpi_.logical_to_pixels(16);
        RECT icon_rc{ search_rc.left + pad_x,
                      search_rc.top + (m.search_h - icon_box) / 2,
                      search_rc.left + pad_x + icon_box,
                      search_rc.top + (m.search_h - icon_box) / 2 + icon_box };
        nfui::draw_vector_icon(target, nfui::IconKind::search, icon_rc,
                               palette_.text_secondary,
                               dpi_.logical_to_pixels(2));
        RECT placeholder_rc{ icon_rc.right + dpi_.logical_to_pixels(8),
                             search_rc.top,
                             search_rc.right - pad_x,
                             search_rc.bottom };
        nfui::draw_text(target, placeholder_rc,
            L"Search icons by name or category", chip_font,
            palette_.text_secondary,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Filter chip row: active chip uses accent fill + accent_text; the
        // rest use surface fill + text colour.
        const int chip_radius = m.chip_h / 2;
        for (size_t i = 0; i < kChips.size(); ++i) {
            const RECT chip = chip_rect(m, i);
            if (chip.right <= chip.left || chip.bottom <= chip.top) continue;
            const bool active = (static_cast<int>(i) == selected_chip_);
            const nfui::Color fill = active ? palette_.accent : palette_.surface;
            const nfui::Color text_color = active ? palette_.accent_text : palette_.text;
            nfui::fill_rounded_rect(target, chip, chip_radius, fill, palette_.border);
            nfui::draw_text(target, chip, kChips[i].label, chip_font, text_color,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Icon grid: 3 rows x 8 columns of 120x120 cards.
        const int card_radius      = dpi_.logical_to_pixels(8);
        const int glyph_box        = dpi_.logical_to_pixels(40);
        const int label_h          = dpi_.logical_to_pixels(20);
        const int glyph_label_gap  = dpi_.logical_to_pixels(12);
        const int stroke           = dpi_.logical_to_pixels(2);
        for (size_t i = 0; i < kGlyphs.size(); ++i) {
            const RECT card = card_rect(m, i);
            if (card.right <= card.left || card.bottom <= card.top) continue;
            const bool hovered = (hover_idx_ == static_cast<int>(i));
            draw_icon_card(target, card, card_radius, glyph_box, label_h,
                           glyph_label_gap, stroke, kGlyphs[i].kind,
                           palette_.accent, kGlyphs[i].label, label_font,
                           hovered);
        }

        // "Icon Buttons (Button::set_icon)" section header (md=16 muted).
        const int section_h = dpi_.logical_to_pixels(24);
        RECT section_rc{ m.pad, m.section_y, cw - m.pad, m.section_y + section_h };
        nfui::draw_text(target, section_rc,
            L"Icon Buttons (Button::set_icon)", section_font,
            palette_.text_secondary,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

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

    int selected_chip_{ 0 };   // CP32: active filter chip index (0 = All).
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