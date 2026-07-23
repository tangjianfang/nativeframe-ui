// NativeFrameUIIconGallery -- polished vector icon showcase (CP32-v2).
//
// Modern chrome: monochrome vector glyphs on white cards, clean header,
// search box, filter chips, and a row of owner-drawn icon buttons.

#include <nfui/NativeFrameUI.hpp>
#include <nfui/VectorIcon.hpp>

#include "NativeFrameUIResource.h"

#include <windowsx.h>

#include <array>
#include <string_view>
#include <vector>

namespace {

// --------------------------------------------------------------------------
// Layout tokens (logical pixels). The 4/8/12/16 grid keeps every sample
// aligned with the CP32 design system.
// --------------------------------------------------------------------------
constexpr int kOuterPad   = 24;
constexpr int kGap        = 16;
constexpr int kSmallGap   = 8;
constexpr int kTinyGap    = 4;
constexpr int kHeaderH    = 72;
constexpr int kSearchH    = 40;
constexpr int kChipH      = 32;
constexpr int kCardMinW   = 120;
constexpr int kCardH      = 112;
constexpr int kIconSize   = 36;
constexpr int kLabelH     = 18;
constexpr int kIBtnH      = 34;
constexpr int kIBtnW      = 120;
constexpr int kColumns    = 8;

// --------------------------------------------------------------------------
// Theme / action identifiers.
// --------------------------------------------------------------------------
constexpr int id_btn_light   = 2001;
constexpr int id_btn_dark    = 2002;
constexpr int id_btn_hc      = 2003;
constexpr int id_btn_search  = 2011;
constexpr int id_btn_settings = 2012;
constexpr int id_btn_add     = 2013;
constexpr int id_btn_close   = 2014;

// --------------------------------------------------------------------------
// Custom GDI icon helpers. These are simple geometric primitives drawn at
// the same stroke width as the framework vector icons so the whole grid reads
// as one consistent monochrome set.
// --------------------------------------------------------------------------

[[nodiscard]] RECT make_rect(int left, int top, int width, int height) noexcept {
    RECT r{};
    r.left   = left;
    r.top    = top;
    r.right  = left + std::max(width, 0);
    r.bottom = top + std::max(height, 0);
    return r;
}

[[nodiscard]] int rect_width(const RECT& r) noexcept { return r.right - r.left; }
[[nodiscard]] int rect_height(const RECT& r) noexcept { return r.bottom - r.top; }

void draw_mail_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke + 3;
    const int left  = r.left + pad;
    const int right = r.right - pad;
    const int top   = r.top + pad;
    const int bottom = r.bottom - pad;
    const int mid_y = (top + bottom) / 2;
    // Envelope body.
    nfui::draw_line(dc, {left, top},    {right, top},    color, stroke);
    nfui::draw_line(dc, {right, top},   {right, bottom}, color, stroke);
    nfui::draw_line(dc, {right, bottom},{left, bottom},  color, stroke);
    nfui::draw_line(dc, {left, bottom}, {left, top},     color, stroke);
    // Flap.
    nfui::draw_line(dc, {left, top}, {left + (right - left) / 2, mid_y}, color, stroke);
    nfui::draw_line(dc, {right, top}, {left + (right - left) / 2, mid_y}, color, stroke);
}

void draw_star_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top + r.bottom) / 2;
    const int s = std::max(3, std::min(rect_width(r), rect_height(r)) / 3);
    const std::array<POINT, 10> star{{
        {cx, cy - s},
        {cx + s / 4, cy - s / 4},
        {cx + s, cy - s / 4},
        {cx + s / 3, cy + s / 6},
        {cx + s / 2, cy + s},
        {cx, cy + s / 2},
        {cx - s / 2, cy + s},
        {cx - s / 3, cy + s / 6},
        {cx - s, cy - s / 4},
        {cx - s / 4, cy - s / 4},
    }};
    nfui::fill_polygon(dc, star.data(), static_cast<int>(star.size()), color, color);
    (void)stroke; // filled shape
}

void draw_home_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke + 2;
    const int left   = r.left + pad;
    const int right  = r.right - pad;
    const int top    = r.top + pad;
    const int bottom = r.bottom - pad;
    const int roof_h = (bottom - top) * 2 / 5;
    const int mid_x  = (left + right) / 2;
    const POINT roof[] = {
        {left, top + roof_h},
        {mid_x, top},
        {right, top + roof_h},
    };
    nfui::draw_polyline(dc, roof, static_cast<int>(std::size(roof)), color, stroke);
    const RECT body = make_rect(left + stroke * 2, top + roof_h, right - left - stroke * 4, bottom - (top + roof_h) - stroke);
    nfui::draw_line(dc, {body.left, body.top}, {body.left, body.bottom}, color, stroke);
    nfui::draw_line(dc, {body.right, body.top}, {body.right, body.bottom}, color, stroke);
    nfui::draw_line(dc, {body.left, body.bottom}, {body.right, body.bottom}, color, stroke);
}

void draw_user_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int cx = (r.left + r.right) / 2;
    const int pad = stroke + 3;
    const int head_r = std::min(rect_width(r), rect_height(r)) / 5;
    // Head circle.
    nfui::draw_ellipse(dc, make_rect(cx - head_r, r.top + pad, head_r * 2 + 1, head_r * 2 + 1), color, stroke);
    // Shoulders: wide arc drawn as the top half of an ellipse.
    const int body_top = r.top + pad + head_r + kTinyGap;
    const int body_h = r.bottom - body_top - pad;
    const int body_w = std::max(body_h * 3 / 2, rect_width(r) * 2 / 3);
    const int body_cy = body_top + body_h / 2;
    // Clip to upper half manually by drawing an arc with GDI.
    {
        HDC hdc = dc;
        const RECT body = make_rect(cx - body_w / 2, body_top, body_w, body_h);
        HPEN pen = CreatePen(PS_SOLID, stroke, color.rgb);
        HGDIOBJ old = SelectObject(hdc, pen);
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        // Upper arc from 180 to 360 degrees.
        Arc(hdc, body.left, body.top, body.right, body.bottom,
            body.left, body_cy, body.right, body_cy);
        SelectObject(hdc, old_brush);
        SelectObject(hdc, old);
        DeleteObject(pen);
    }
}

void draw_bell_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int cx = (r.left + r.right) / 2;
    const int pad = stroke + 3;
    const int top    = r.top + pad;
    const int bottom = r.bottom - pad;
    const int width  = rect_width(r) / 2 - stroke;
    // Bell body: rounded-top rectangle.
    const int body_h = bottom - top - stroke * 2;
    const RECT body = make_rect(cx - width / 2, top, width, body_h * 4 / 5);
    nfui::draw_ellipse(dc, make_rect(body.left, body.top - body_h / 10,
                                      rect_width(body), rect_height(body) / 2 + body_h / 10),
                       color, stroke);
    nfui::draw_line(dc, {body.left, body.top + rect_height(body) / 4}, {body.left, body.bottom}, color, stroke);
    nfui::draw_line(dc, {body.right, body.top + rect_height(body) / 4}, {body.right, body.bottom}, color, stroke);
    nfui::draw_line(dc, {body.left, body.bottom}, {body.right, body.bottom}, color, stroke);
    // Clapper.
    const int clapper_r = std::max(3, width / 4);
    nfui::draw_ellipse(dc, make_rect(cx - clapper_r, body.bottom, clapper_r * 2 + 1, clapper_r * 2 + 1),
                       color, stroke);
}

void draw_calendar_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke + 3;
    const int left   = r.left + pad;
    const int right  = r.right - pad;
    const int top    = r.top + pad + rect_height(r) / 6;
    const int bottom = r.bottom - pad;
    // Body.
    const POINT body[] = {
        {left, top},
        {right, top},
        {right, bottom},
        {left, bottom},
    };
    nfui::draw_polyline(dc, body, static_cast<int>(std::size(body)), color, stroke);
    // Header rings.
    const int ring_r = rect_height(r) / 10;
    const int ring_y = r.top + pad + ring_r;
    const int ring_span = (right - left) / 3;
    nfui::draw_ellipse(dc, make_rect(left + ring_span - ring_r, ring_y - ring_r, ring_r * 2 + 1, ring_r * 2 + 1), color, stroke);
    nfui::draw_ellipse(dc, make_rect(right - ring_span - ring_r, ring_y - ring_r, ring_r * 2 + 1, ring_r * 2 + 1), color, stroke);
    // Grid dots.
    const int cols = 3;
    const int rows = 3;
    const int cell_w = (right - left) / cols;
    const int cell_h = (bottom - top) / (rows + 1);
    const int dot = std::max(2, stroke);
    for (int row = 1; row <= rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const int x = left + col * cell_w + cell_w / 2 - dot / 2;
            const int y = top + row * cell_h + cell_h / 2 - dot / 2;
            nfui::fill_ellipse(dc, make_rect(x, y, dot, dot), color);
        }
    }
}

void draw_folder_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke + 2;
    const int tab_h = rect_height(r) / 4;
    const int tab_w = rect_width(r) / 3;
    const int left  = r.left + pad;
    const int right = r.right - pad;
    const int top   = r.top + pad;
    const int bottom = r.bottom - pad;
    // Outline folder with tab.
    nfui::draw_line(dc, {left, top + tab_h}, {left + tab_w, top + tab_h}, color, stroke);
    nfui::draw_line(dc, {left + tab_w, top + tab_h}, {left + tab_w + stroke * 2, top}, color, stroke);
    nfui::draw_line(dc, {left + tab_w + stroke * 2, top}, {right, top}, color, stroke);
    nfui::draw_line(dc, {right, top}, {right, bottom}, color, stroke);
    nfui::draw_line(dc, {right, bottom}, {left, bottom}, color, stroke);
    nfui::draw_line(dc, {left, bottom}, {left, top + tab_h}, color, stroke);
}

void draw_file_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke + 3;
    const int left   = r.left + pad;
    const int right  = r.right - pad;
    const int top    = r.top + pad;
    const int bottom = r.bottom - pad;
    const int fold   = (right - left) / 3;
    const POINT body[] = {
        {left, top},
        {right - fold, top},
        {right, top + fold},
        {right, bottom},
        {left, bottom},
    };
    nfui::draw_polyline(dc, body, static_cast<int>(std::size(body)), color, stroke);
    nfui::draw_line(dc, {right - fold, top}, {right - fold, top + fold}, color, stroke);
    nfui::draw_line(dc, {right - fold, top + fold}, {right, top + fold}, color, stroke);
}

void draw_download_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int cx = (r.left + r.right) / 2;
    const int pad = stroke + 4;
    const int top_y    = r.top + pad;
    const int bottom_y = r.bottom - pad;
    const int head_w   = rect_width(r) / 3;
    const POINT arrow[] = {
        {cx - head_w / 2, top_y + (bottom_y - top_y) / 3},
        {cx, bottom_y - stroke * 2},
        {cx + head_w / 2, top_y + (bottom_y - top_y) / 3},
    };
    nfui::draw_polyline(dc, arrow, static_cast<int>(std::size(arrow)), color, stroke);
    nfui::draw_line(dc, {cx, top_y}, {cx, bottom_y - stroke * 2}, color, stroke);
    nfui::draw_line(dc, {cx - head_w, bottom_y}, {cx + head_w, bottom_y}, color, stroke);
}

void draw_heart_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top + r.bottom) / 2 + rect_height(r) / 16;
    const int w  = rect_width(r) / 2 - stroke * 2;
    const int h  = rect_height(r) / 2 - stroke * 2;
    // Two upper circles form the lobes.
    const int lobe_r = std::max(3, w / 2);
    const int lobe_y = cy - h / 3;
    nfui::draw_ellipse(dc, make_rect(cx - w / 2 - lobe_r / 2, lobe_y - lobe_r,
                                      lobe_r * 2 + 1, lobe_r * 2 + 1), color, stroke);
    nfui::draw_ellipse(dc, make_rect(cx + w / 2 - lobe_r * 3 / 2, lobe_y - lobe_r,
                                      lobe_r * 2 + 1, lobe_r * 2 + 1), color, stroke);
    // Lower triangle point.
    const POINT heart[] = {
        {cx - w / 2, lobe_y},
        {cx, r.bottom - stroke - 2},
        {cx + w / 2, lobe_y},
    };
    nfui::fill_polygon(dc, heart, static_cast<int>(std::size(heart)), color, color);
}

void draw_share_icon(HDC dc, const RECT& r, nfui::Color color, int stroke) noexcept {
    const int pad = stroke + 4;
    const int left   = r.left + pad;
    const int right  = r.right - pad;
    const int top    = r.top + pad;
    const int bottom = r.bottom - pad;
    const int dot    = std::max(3, stroke * 2);
    const int mid_y  = (top + bottom) / 2;
    // Three dots.
    nfui::fill_ellipse(dc, make_rect(left - dot / 2, top - dot / 2, dot, dot), color);
    nfui::fill_ellipse(dc, make_rect(right - dot / 2, mid_y - dot / 2, dot, dot), color);
    nfui::fill_ellipse(dc, make_rect(left - dot / 2, bottom - dot / 2, dot, dot), color);
    // Connecting lines.
    nfui::draw_line(dc, {left + dot / 2, top + dot / 2}, {right - dot / 2, mid_y - dot / 2}, color, stroke);
    nfui::draw_line(dc, {left + dot / 2, bottom - dot / 2}, {right - dot / 2, mid_y + dot / 2}, color, stroke);
}

// --------------------------------------------------------------------------
// Icon entry. Either a framework vector glyph or a custom GDI primitive.
// --------------------------------------------------------------------------
enum class IconSource { vector, custom };

using CustomIconFn = void (*)(HDC, const RECT&, nfui::Color, int);

struct IconEntry {
    std::wstring_view label;
    IconSource source;
    nfui::IconKind kind;
    CustomIconFn custom;
};

constexpr std::array<IconEntry, 24> kIcons{{
    {L"Chevron Down", IconSource::vector, nfui::IconKind::chevron_down, nullptr},
    {L"Chevron Up",   IconSource::vector, nfui::IconKind::chevron_up,   nullptr},
    {L"Chevron Left", IconSource::vector, nfui::IconKind::chevron_left, nullptr},
    {L"Chevron Right",IconSource::vector, nfui::IconKind::chevron_right,nullptr},
    {L"Check",        IconSource::vector, nfui::IconKind::check,        nullptr},
    {L"Close",        IconSource::vector, nfui::IconKind::close,        nullptr},
    {L"Plus",         IconSource::vector, nfui::IconKind::plus,         nullptr},
    {L"Minus",        IconSource::vector, nfui::IconKind::minus,        nullptr},

    {L"Search",       IconSource::vector, nfui::IconKind::search,        nullptr},
    {L"Settings",     IconSource::vector, nfui::IconKind::gear,          nullptr},
    {L"Mail",         IconSource::custom, nfui::IconKind::none,          draw_mail_icon},
    {L"Menu",         IconSource::vector, nfui::IconKind::hamburger,     nullptr},
    {L"Info",         IconSource::vector, nfui::IconKind::info,          nullptr},
    {L"Warning",      IconSource::vector, nfui::IconKind::warning,       nullptr},
    {L"Star",         IconSource::custom, nfui::IconKind::none,          draw_star_icon},
    {L"Home",         IconSource::custom, nfui::IconKind::none,          draw_home_icon},

    {L"User",         IconSource::custom, nfui::IconKind::none,          draw_user_icon},
    {L"Bell",         IconSource::custom, nfui::IconKind::none,          draw_bell_icon},
    {L"Calendar",     IconSource::custom, nfui::IconKind::none,          draw_calendar_icon},
    {L"Folder",       IconSource::custom, nfui::IconKind::none,          draw_folder_icon},
    {L"File",         IconSource::custom, nfui::IconKind::none,          draw_file_icon},
    {L"Download",     IconSource::custom, nfui::IconKind::none,          draw_download_icon},
    {L"Heart",        IconSource::custom, nfui::IconKind::none,          draw_heart_icon},
    {L"Share",        IconSource::custom, nfui::IconKind::none,          draw_share_icon},
}};

constexpr std::array<std::wstring_view, 5> kChips{{
    L"All",
    L"Navigation",
    L"Status",
    L"Actions",
    L"Media",
}};

// --------------------------------------------------------------------------
// Main window.
// --------------------------------------------------------------------------
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
            CW_USEDEFAULT, CW_USEDEFAULT, 1200, 760,
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
            return 1;
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
        apply_icon(btn_close_,     nfui::IconKind::close);
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

    [[nodiscard]] int scale(int logical) const noexcept {
        return dpi_.logical_to_pixels(logical);
    }

    void layout() noexcept {
        if (hwnd() == nullptr) return;
        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        RECT client{};
        GetClientRect(hwnd(), &client);
        const int cw = client.right - client.left;
        const int ch = client.bottom - client.top;
        const int pad = scale(kOuterPad);
        const int gap = scale(kGap);
        const int btn_h = scale(32);
        const int btn_w = scale(88);

        // Theme toggle row at the top-right.
        int x = cw - pad - btn_w;
        move(btn_hc_, x, pad, btn_w, btn_h); x -= btn_w + kTinyGap;
        move(btn_dark_, x, pad, btn_w, btn_h); x -= btn_w + kTinyGap;
        move(btn_light_, x, pad, btn_w, btn_h);

        // Icon-button row anchored to the bottom-left.
        const int ibtn_w = scale(kIBtnW);
        const int ibtn_h = scale(kIBtnH);
        const int ibtn_y = ch - pad - ibtn_h;
        int ix = pad;
        move(btn_search_,  ix, ibtn_y, ibtn_w, ibtn_h); ix += ibtn_w + gap;
        move(btn_settings_, ix, ibtn_y, ibtn_w, ibtn_h); ix += ibtn_w + gap;
        move(btn_add_,      ix, ibtn_y, ibtn_w, ibtn_h); ix += ibtn_w + gap;
        move(btn_close_,    ix, ibtn_y, ibtn_w, ibtn_h);

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

        const int pad = scale(kOuterPad);
        const int gap = scale(kGap);
        const int small_gap = scale(kSmallGap);
        const int tiny_gap = scale(kTinyGap);

        nfui::fill_rect(target, bounds, palette_.background);

        const HFONT title_font = fonts_.serif(dpi_.dpi(), nfui::font_pt::xl);
        const HFONT subtitle_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::sm);
        const HFONT chip_font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::base);
        const HFONT label_font = fonts_.regular(dpi_.dpi(), nfui::font_pt::xs);
        const HFONT section_font = fonts_.semibold(dpi_.dpi(), nfui::font_pt::md);

        // Header text (left).
        const int header_h = scale(kHeaderH);
        RECT header{ pad, pad, cw - pad - scale(3 * 88 + 2 * kTinyGap + kOuterPad), pad + header_h };
        RECT title_r = header;
        title_r.bottom = title_r.top + scale(38);
        nfui::draw_text(target, title_r, L"Vector Icon Gallery", title_font,
                        palette_.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RECT sub_r = header;
        sub_r.top = title_r.bottom + tiny_gap;
        nfui::draw_text(target, sub_r,
            L"GDI-primitive glyphs on a 16x16 grid — DPI-scaled, palette-coloured. Toggle the theme top-right.",
            subtitle_font, palette_.text_secondary,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

        // Search box.
        const int search_y = pad + header_h + gap;
        const int search_h = scale(kSearchH);
        RECT search_r{ pad, search_y, cw - pad, search_y + search_h };
        const int search_radius = scale(8);
        nfui::fill_rounded_rect(target, search_r, search_radius, palette_.surface, palette_.border);
        // Search glyph.
        const int search_icon_size = scale(18);
        RECT search_icon_r = make_rect(search_r.left + small_gap,
                                         search_r.top + (search_h - search_icon_size) / 2,
                                         search_icon_size, search_icon_size);
        nfui::draw_vector_icon(target, nfui::IconKind::search, search_icon_r,
                               palette_.text_secondary, scale(2));
        // Placeholder text.
        RECT search_text_r = make_rect(search_icon_r.right + small_gap,
                                       search_r.top,
                                       search_r.right - search_icon_r.right - small_gap * 2,
                                       search_h);
        nfui::draw_text(target, search_text_r, L"Search icons by name or category",
                        subtitle_font, palette_.text_secondary,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Filter chips.
        const int chip_y = search_y + search_h + gap;
        const int chip_h = scale(kChipH);
        int chip_x = pad;
        for (std::size_t i = 0; i < kChips.size(); ++i) {
            const bool selected = (i == 0);
            // Measure text to size the pill tightly.
            int text_w = 0;
            if (chip_font != nullptr) {
                HGDIOBJ old = SelectObject(target, chip_font);
                SIZE sz{};
                if (GetTextExtentPoint32W(target, kChips[i].data(), static_cast<int>(kChips[i].size()), &sz)) {
                    text_w = sz.cx;
                }
                if (old && old != HGDI_ERROR) SelectObject(target, old);
            }
            const int chip_w = text_w + scale(24);
            RECT chip_r = make_rect(chip_x, chip_y, chip_w, chip_h);
            const int chip_radius = chip_h / 2;
            if (selected) {
                nfui::fill_rounded_rect(target, chip_r, chip_radius, palette_.accent, palette_.accent);
                nfui::draw_text(target, chip_r, kChips[i], chip_font,
                                palette_.accent_text,
                                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            } else {
                nfui::fill_rounded_rect(target, chip_r, chip_radius, palette_.surface, palette_.border);
                nfui::draw_text(target, chip_r, kChips[i], chip_font,
                                palette_.text,
                                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            chip_x += chip_w + small_gap;
        }

        // Icon grid.
        const int grid_top = chip_y + chip_h + gap;
        const int grid_bottom = ch - pad - scale(kIBtnH) - scale(28) - gap;
        const int avail_w = cw - 2 * pad;
        const int avail_h = std::max(0, grid_bottom - grid_top);
        const int card_w = std::max(scale(kCardMinW), (avail_w - (kColumns - 1) * gap) / kColumns);
        const int rows = (static_cast<int>(kIcons.size()) + kColumns - 1) / kColumns;
        const int card_h = std::max(scale(kCardH), (avail_h - (rows - 1) * gap) / rows);
        const int card_radius = scale(10);
        const int icon_size = scale(kIconSize);
        const int label_h = scale(kLabelH);
        const int icon_label_gap = scale(8);
        const int stroke = scale(2);

        for (std::size_t i = 0; i < kIcons.size(); ++i) {
            const int col = static_cast<int>(i) % kColumns;
            const int row = static_cast<int>(i) / kColumns;
            const int cx = pad + col * (card_w + gap);
            const int cy = grid_top + row * (card_h + gap);
            RECT card = make_rect(cx, cy, card_w, card_h);
            if (card.right <= card.left || card.bottom <= card.top) continue;
            nfui::fill_rounded_rect(target, card, card_radius, palette_.surface, palette_.border);

            // Centre icon + label as a single unit.
            const int content_h = icon_size + icon_label_gap + label_h;
            const int gy = card.top + (card_h - content_h) / 2;
            const int gx = card.left + (card_w - icon_size) / 2;
            RECT icon_r = make_rect(gx, gy, icon_size, icon_size);

            const auto& entry = kIcons[i];
            if (entry.source == IconSource::vector) {
                nfui::draw_vector_icon(target, entry.kind, icon_r, palette_.text, stroke);
            } else if (entry.custom != nullptr) {
                entry.custom(target, icon_r, palette_.text, stroke);
            }

            RECT label_r = make_rect(card.left + small_gap,
                                     icon_r.bottom + icon_label_gap,
                                     card_w - small_gap * 2,
                                     label_h);
            nfui::draw_text(target, label_r, entry.label, label_font,
                            palette_.text_secondary,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        }

        // Icon-buttons section caption.
        const int caption_y = ch - pad - scale(kIBtnH) - scale(24);
        RECT caption_r = make_rect(pad, caption_y, cw - pad, scale(24));
        nfui::draw_text(target, caption_r, L"Icon Buttons (Button::set_icon)",
                        section_font, palette_.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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
