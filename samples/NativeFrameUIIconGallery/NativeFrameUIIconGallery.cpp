// NativeFrameUIIconGallery -- CP18 showcase for the vector icon system.
//
// Renders every vector glyph in a card grid (custom WM_PAINT, paint-layer
// API) and a row of real icon Buttons (Button::set_icon) so the system is
// visible end-to-end. Light / Dark / High-Contrast toggle buttons re-palette
// both the custom-painted grid and the framework controls live.

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
    { nfui::IconKind::close,          L"Close" },
    { nfui::IconKind::plus,           L"Plus" },
    { nfui::IconKind::minus,          L"Minus" },
    { nfui::IconKind::search,         L"Search" },
    { nfui::IconKind::gear,           L"Settings" },
    { nfui::IconKind::info,           L"Info" },
    { nfui::IconKind::warning,        L"Warning" },
    { nfui::IconKind::dot,            L"Dot" },
    { nfui::IconKind::hamburger,      L"Menu" },
}};

constexpr int kColumns = 7;

constexpr int id_btn_light   = 2001;
constexpr int id_btn_dark    = 2002;
constexpr int id_btn_hc      = 2003;
constexpr int id_btn_search  = 2011;
constexpr int id_btn_settings = 2012;
constexpr int id_btn_add     = 2013;
constexpr int id_btn_close   = 2014;

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
            CW_USEDEFAULT, CW_USEDEFAULT, 1040, 560,
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
        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            const int code = HIWORD(wparam);
            if (code == BN_CLICKED) {
                if (id == id_btn_light)   set_theme(nfui::ThemeMode::light);
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
        if (!make_child(btn_hc_,       id_btn_hc,      L"High Contrast")) return false;

        // Icon buttons: each carries a leading vector glyph.
        if (!make_child(btn_search_,  id_btn_search,   L"Search"))  return false;
        if (!make_child(btn_settings_, id_btn_settings, L"Settings")) return false;
        if (!make_child(btn_add_,      id_btn_add,      L"Add"))     return false;
        if (!make_child(btn_close_,    id_btn_close,    L"Close"))   return false;

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

    void layout() noexcept {
        if (hwnd() == nullptr) return;
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        const int cw = client.right - client.left;
        const int pad = dpi_.logical_to_pixels(16);

        // Theme toggle row at the top-right.
        const int btn_w = dpi_.logical_to_pixels(96);
        const int btn_h = dpi_.logical_to_pixels(28);
        const int gap = dpi_.logical_to_pixels(8);
        int x = cw - pad - btn_w;
        move(btn_hc_, x, pad, btn_w, btn_h); x -= btn_w + gap;
        move(btn_dark_, x, pad, btn_w, btn_h); x -= btn_w + gap;
        move(btn_light_, x, pad, btn_w, btn_h);

        // Icon-button row near the bottom.
        const int ibtn_w = dpi_.logical_to_pixels(120);
        const int ibtn_y = (client.bottom - client.top) - pad - btn_h;
        int ix = pad;
        move(btn_search_, ix, ibtn_y, ibtn_w, btn_h); ix += ibtn_w + gap;
        move(btn_settings_, ix, ibtn_y, ibtn_w, btn_h); ix += ibtn_w + gap;
        move(btn_add_, ix, ibtn_y, ibtn_w, btn_h); ix += ibtn_w + gap;
        move(btn_close_, ix, ibtn_y, ibtn_w, btn_h);

        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void move(nfui::Control& c, int x, int y, int w, int h) noexcept {
        if (c.valid()) {
            SetWindowPos(c.hwnd(), nullptr, x, y, w, h,
                         SWP_NOACTIVATE | SWP_NOZORDER);
        }
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

        const int pad = dpi_.logical_to_pixels(16);
        const HFONT title_font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::ui);
        const HFONT label_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::ui);

        // Header.
        RECT header{ pad, pad, cw - pad, pad + dpi_.logical_to_pixels(28) };
        nfui::draw_text(target, header, L"Vector Icon Gallery", title_font,
                        palette_.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT sub{ pad, header.bottom, cw - pad, header.bottom + dpi_.logical_to_pixels(20) };
        nfui::draw_text(target, sub,
            L"GDI-primitive glyphs on a 16x16 grid — DPI-scaled, palette-coloured. Toggle the theme top-right.",
            label_font, palette_.text_secondary, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Card grid.
        // Reserve the icon-button band at the bottom and place the section
        // caption directly above it (derived from the button row so the gap
        // stays correct at any DPI — never overlaps the buttons).
        const int btn_h = dpi_.logical_to_pixels(28);
        const int ibtn_y = ch - pad - btn_h;
        const int caption_h = dpi_.logical_to_pixels(20);
        const int caption_gap = dpi_.logical_to_pixels(8);
        const RECT cap{ pad, ibtn_y - caption_gap - caption_h,
                        cw - pad, ibtn_y - caption_gap };
        const int top = sub.bottom + dpi_.logical_to_pixels(12);
        const int bottom_limit = cap.top - dpi_.logical_to_pixels(12);
        const int avail_w = cw - 2 * pad;
        const int card_w = avail_w / kColumns;
        const int rows = (static_cast<int>(kGlyphs.size()) + kColumns - 1) / kColumns;
        const int avail_h = bottom_limit - top;
        const int card_h = avail_h / rows;

        const int card_radius = dpi_.logical_to_pixels(8);
        const int glyph_box = dpi_.logical_to_pixels(36);
        const int label_h = dpi_.logical_to_pixels(20);
        const int glyph_label_gap = dpi_.logical_to_pixels(6);
        const int stroke = dpi_.logical_to_pixels(2);

        for (size_t i = 0; i < kGlyphs.size(); ++i) {
            const int col = static_cast<int>(i) % kColumns;
            const int row = static_cast<int>(i) / kColumns;
            const int cx = pad + col * card_w;
            const int cy = top + row * card_h;
            const int inner_pad = dpi_.logical_to_pixels(8);
            RECT card{ cx + inner_pad, cy + inner_pad,
                       cx + card_w - inner_pad, cy + card_h - inner_pad };
            if (card.right <= card.left || card.bottom <= card.top) continue;
            nfui::fill_rounded_rect(target, card, card_radius, palette_.surface, palette_.border);

            // Group the glyph + label as one unit and centre the unit in the
            // card so the two read as a paired "icon + caption" rather than a
            // top-pinned glyph with a floating label (精品: cards must not
            // look top-heavy and half-empty).
            const int card_h_inner = card.bottom - card.top;
            const int content_h = glyph_box + glyph_label_gap + label_h;
            const int gy = card.top + (card_h_inner - content_h) / 2;
            const int gx = card.left + (card.right - card.left - glyph_box) / 2;
            RECT glyph{ gx, gy, gx + glyph_box, gy + glyph_box };
            const nfui::Color glyph_color =
                (kGlyphs[i].kind == nfui::IconKind::warning) ? palette_.warning
                : (kGlyphs[i].kind == nfui::IconKind::check) ? palette_.success
                : (kGlyphs[i].kind == nfui::IconKind::info) ? palette_.accent
                : palette_.text;
            nfui::draw_vector_icon(target, kGlyphs[i].kind, glyph, glyph_color, stroke);

            // Label sits directly under the glyph (fixed height), so the
            // pair stays grouped and the gap never stretches.
            RECT label{ card.left, glyph.bottom + glyph_label_gap,
                        card.right, glyph.bottom + glyph_label_gap + label_h };
            nfui::draw_text(target, label, kGlyphs[i].label, label_font,
                            palette_.text_secondary,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        // Section caption for the icon-button row.
        nfui::draw_text(target, cap, L"Icon Buttons (Button::set_icon)", title_font,
                        palette_.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
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