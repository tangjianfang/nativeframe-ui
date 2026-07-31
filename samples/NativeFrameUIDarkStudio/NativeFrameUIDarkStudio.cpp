// NativeFrameUIDarkStudio
//
// V1.0 capability: dark-mode-only product shell focused on a preview-heavy
// surface (think designer / creative tools). Pairs with NativeFrameUIShowcase
// to show that the framework does not force a single visual identity — both
// samples use the same core APIs but land in very different looks.
//
// Visual upgrade: workspace-themed IDE-like shell:
//   * italic serif "DarkStudio" brand (xl=28, accent) + sm=13 subtitle +
//     non-interactive theme toggle pill on the right.
//   * 240 logical px sidebar with 4 nav rows (Surface/Canvas/Tokens/Publish),
//     each with a custom monochrome vector icon and a 2px coral left bar
//     on the selected row.
//   * Two main cards: "Preview-first shell" description card on top, a
//     "Preview canvas" code block below with line numbers and simple
//     syntax highlighting (keywords → accent, strings → success, comments
//     → muted).
//   * Two stat cards ("Native shell 100%" / "DPI layout Per-monitor") with
//     the value rendered in accent coral at lg=20.
//   * Native status bar at the bottom showing a green dot + "Ready".

#include <nfui/NativeFrameUI.hpp>
#include <nfui/ThemeBroker.hpp>
#include <nfui/design_tokens.hpp>

#include "NativeFrameUIResource.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>
#include <windowsx.h>

namespace {

namespace tok = nfui::design;

[[nodiscard]] int rect_width(const RECT& rect) noexcept {
    return rect.right - rect.left;
}

[[nodiscard]] int rect_height(const RECT& rect) noexcept {
    return rect.bottom - rect.top;
}

[[nodiscard]] RECT make_rect(int left, int top, int width, int height) noexcept {
    RECT rect{};
    rect.left = left;
    rect.top = top;
    rect.right = left + std::max(width, 0);
    rect.bottom = top + std::max(height, 0);
    return rect;
}

[[nodiscard]] RECT inset_rect(const RECT& rect, int amount) noexcept {
    RECT inset = rect;
    inset.left += amount;
    inset.top += amount;
    inset.right -= amount;
    inset.bottom -= amount;
    return inset;
}

constexpr std::array<std::wstring_view, 4> navigation_items{
    L"Surface",
    L"Canvas",
    L"Tokens",
    L"Publish",
};

// Code snippet shown in the "Preview canvas" card.
constexpr std::array<std::wstring_view, 8> code_snippet{
    L"// NativeFrame UI DarkStudio Sample",
    L"import { fui } from 'native-frame-ui';",
    L"",
    L"function renderPreview() {",
    L"    const scale = fui.DpiScale;",
    L"    console.log('Rendering preview at scale:', scale);",
    L"    // ... more code ...",
    L"}",
};

// Code-token classification for syntax highlighting in the preview canvas.
enum class CodeTokenKind { Text, Keyword, String };

struct CodeToken {
    CodeTokenKind kind;
    std::wstring_view text;
};

[[nodiscard]] bool is_keyword(std::wstring_view word) noexcept {
    static const std::array<std::wstring_view, 16> keywords = {
        L"import", L"function", L"const", L"let",   L"var",
        L"return", L"if",      L"else",  L"for",   L"while",
        L"class",  L"new",     L"this",  L"true",  L"false",
        L"null",
    };
    for (auto kw : keywords) {
        if (word == kw) return true;
    }
    return false;
}

// Tokenize a single line of code into Text / Keyword / String segments. The
// caller passes the result through `draw_text` with measured widths to
// produce token-accurate coloring without needing a full lexer.
std::vector<CodeToken> tokenize_code_line(std::wstring_view line) noexcept {
    std::vector<CodeToken> tokens;
    std::size_t i = 0;
    while (i < line.size()) {
        const wchar_t c = line[i];

        // Whitespace run — render as text, never colored.
        if (c == L' ' || c == L'\t') {
            const std::size_t start = i;
            while (i < line.size() && (line[i] == L' ' || line[i] == L'\t')) ++i;
            tokens.push_back({CodeTokenKind::Text, line.substr(start, i - start)});
            continue;
        }

        // String literal — '...' / "..." / `...`. Greedy; backslash escapes
        // are honored so a quote inside doesn't terminate the string.
        if (c == L'\'' || c == L'"' || c == L'`') {
            const wchar_t quote = c;
            const std::size_t start = i;
            ++i;
            while (i < line.size() && line[i] != quote) {
                if (line[i] == L'\\' && i + 1 < line.size()) ++i;
                ++i;
            }
            if (i < line.size()) ++i; // closing quote
            tokens.push_back({CodeTokenKind::String, line.substr(start, i - start)});
            continue;
        }

        // Line / block comment — render the rest of the line in muted text.
        if (c == L'/' && i + 1 < line.size() &&
            (line[i + 1] == L'/' || line[i + 1] == L'*')) {
            tokens.push_back({CodeTokenKind::Text, line.substr(i)});
            i = line.size();
            continue;
        }

        // Identifier — keyword vs plain text.
        if (iswalnum(c) || c == L'_' || c == L'$') {
            const std::size_t start = i;
            while (i < line.size() &&
                   (iswalnum(line[i]) || line[i] == L'_' || line[i] == L'$')) {
                ++i;
            }
            const std::wstring_view word = line.substr(start, i - start);
            tokens.push_back({is_keyword(word) ? CodeTokenKind::Keyword : CodeTokenKind::Text, word});
            continue;
        }

        // Punctuation / operators — emit as text so spacing stays intact.
        const std::size_t start = i;
        while (i < line.size() && !iswalnum(line[i]) && line[i] != L'_' &&
               line[i] != L'$' && line[i] != L' ' && line[i] != L'\t' &&
               line[i] != L'\'' && line[i] != L'"' && line[i] != L'`') {
            ++i;
        }
        if (i > start) {
            tokens.push_back({CodeTokenKind::Text, line.substr(start, i - start)});
        } else {
            ++i; // safety: avoid infinite loop on unexpected chars
        }
    }
    return tokens;
}

// Render a single code line at (x, y). Each token is measured via
// GetTextExtentPoint32W and drawn individually so the spacing matches the
// underlying glyph run.
void render_code_line(HDC target, int x, int y, std::wstring_view line,
                      HFONT mono_font, const nfui::ThemePalette& p) noexcept {
    int cursor_x = x;
    const auto tokens = tokenize_code_line(line);
    for (const auto& tok : tokens) {
        nfui::Color color = p.text;
        switch (tok.kind) {
            case CodeTokenKind::Keyword: color = p.accent;  break;
            case CodeTokenKind::String:  color = p.success; break;
            default: break;
        }

        SIZE sz{};
        HFONT old = static_cast<HFONT>(SelectObject(target, mono_font));
        GetTextExtentPoint32W(target,
                              reinterpret_cast<LPCWSTR>(tok.text.data()),
                              static_cast<int>(tok.text.size()),
                              &sz);
        SelectObject(target, old);

        const RECT r = make_rect(cursor_x, y, sz.cx + 2, 20);
        nfui::draw_text(target, r, tok.text, mono_font, color,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        cursor_x += sz.cx;
    }
}

// Custom sidebar nav icons. VectorIcon.hpp doesn't carry surface/canvas/
// tokens/publish glyphs, so each is hand-drawn on a 16x16 logical grid and
// scaled into `bounds`. Stroke width is in device pixels (caller scales).
enum class NavIcon { Surface, Canvas, Tokens, Publish };

void draw_nav_icon(HDC dc, const RECT& bounds, NavIcon kind,
                   nfui::Color color, int stroke_width) noexcept {
    const int w = bounds.right - bounds.left;
    const int h = bounds.bottom - bounds.top;
    if (w <= 0 || h <= 0) return;
    const int side = (w < h) ? w : h;
    const float scale = static_cast<float>(side) / 16.0f;
    const int ox = bounds.left + (w - side) / 2;
    const int oy = bounds.top + (h - side) / 2;
    const int sw = stroke_width < 1 ? 1 : stroke_width;

    auto pt = [&](int lx, int ly) noexcept -> POINT {
        return POINT{ static_cast<LONG>(ox + static_cast<float>(lx) * scale),
                      static_cast<LONG>(oy + static_cast<float>(ly) * scale) };
    };
    auto px = [&](int lx) noexcept { return static_cast<int>(ox + static_cast<float>(lx) * scale); };
    auto py = [&](int ly) noexcept { return static_cast<int>(oy + static_cast<float>(ly) * scale); };

    switch (kind) {
    case NavIcon::Surface: {
        // Three horizontal slabs (suggests a stack / layers).
        nfui::draw_line(dc, pt(3, 5),  pt(13, 5),  color, sw);
        nfui::draw_line(dc, pt(3, 8),  pt(13, 8),  color, sw);
        nfui::draw_line(dc, pt(3, 11), pt(13, 11), color, sw);
        // End caps to read as closed slabs.
        const int cap = std::max(1, static_cast<int>(2 * scale));
        nfui::fill_ellipse(dc, make_rect(px(2) - cap / 2,  py(5)  - cap / 2, cap, cap), color);
        nfui::fill_ellipse(dc, make_rect(px(13) - cap / 2, py(5)  - cap / 2, cap, cap), color);
        nfui::fill_ellipse(dc, make_rect(px(13) - cap / 2, py(8)  - cap / 2, cap, cap), color);
        nfui::fill_ellipse(dc, make_rect(px(13) - cap / 2, py(11) - cap / 2, cap, cap), color);
        break;
    }
    case NavIcon::Canvas: {
        // Diagonal paintbrush — stick + handle nub + filled tip rectangle.
        nfui::draw_line(dc, pt(3, 13), pt(11, 5), color, sw);
        nfui::draw_line(dc, pt(11, 5), pt(13, 3), color, sw);
        const int tx0 = px(2);
        const int ty0 = py(11);
        const int tx1 = px(6);
        const int ty1 = py(14);
        nfui::fill_rounded_rect(dc, make_rect(tx0, ty0, tx1 - tx0, ty1 - ty0),
                                std::max(1, static_cast<int>(scale)), color, color);
        break;
    }
    case NavIcon::Tokens: {
        // </> code-brackets glyph.
        const POINT lt[3] = { pt(5, 5),  pt(2, 8),  pt(5, 11) };
        nfui::draw_polyline(dc, lt, 3, color, sw);
        const POINT gt[3] = { pt(11, 5), pt(14, 8), pt(11, 11) };
        nfui::draw_polyline(dc, gt, 3, color, sw);
        nfui::draw_line(dc, pt(10, 4), pt(6, 12), color, sw);
        break;
    }
    case NavIcon::Publish: {
        // Upward arrow with horizontal base (upload glyph).
        nfui::draw_line(dc, pt(3, 13), pt(13, 13), color, sw);
        nfui::draw_line(dc, pt(8, 4),  pt(8, 13),  color, sw);
        const POINT head[3] = { pt(8, 2), pt(12, 6), pt(4, 6) };
        nfui::fill_polygon(dc, head, 3, color, color);
        break;
    }
    }
}

// Subclass the native status bar to paint a green "ready" dot at its left
// edge, sitting alongside the "Ready" text. The dot lives in the bar chrome
// so the visual association is unambiguous — painting it from the parent
// would be covered by the status bar's own paint cycle. The status bar's
// chrome subclass uses pad_left = 6 logical px (see StatusBar::paint_chrome);
// we keep the dot inside that margin so it doesn't overlap the "R".
constexpr UINT_PTR kStatusBarDotSubclassId = 0xD57B12;

LRESULT CALLBACK StatusBarDotProc(HWND hwnd, UINT message,
                                  WPARAM wparam, LPARAM lparam,
                                  UINT_PTR subclass_id, DWORD_PTR ref_data) noexcept {
    if (message == WM_PAINT) {
        // Let the existing chrome subclass paint the bar (background,
        // hairline, "Ready" text, size grip).
        const LRESULT r = DefSubclassProc(hwnd, message, wparam, lparam);
        // The chrome subclass closed its BeginPaint/EndPaint above. Open a
        // fresh DC for the same client area and paint a green dot so the
        // status indicator is visible alongside "Ready".
        HDC dc = GetDC(hwnd);
        if (dc != nullptr) {
            const auto* palette = reinterpret_cast<const nfui::ThemePalette*>(ref_data);
            RECT bounds{};
            GetClientRect(hwnd, &bounds);
            // Status bar chrome reserves ~6 logical px of left padding for
            // the part text. Stay inside that reservation with a small dot
            // so we don't overpaint the "R" of "Ready".
            const int dot_d = std::max<int>(4, std::min<int>(bounds.bottom - 6, 8));
            const int pad = 2;
            const int dot_x = bounds.left + pad;
            const int dot_y = bounds.top + (bounds.bottom - dot_d) / 2;
            const nfui::Color dot_color = palette
                ? palette->success
                : nfui::Color{RGB(120, 184, 140)};
            nfui::fill_ellipse(dc, make_rect(dot_x, dot_y, dot_d, dot_d), dot_color);
            ReleaseDC(hwnd, dc);
        }
        return r;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, &StatusBarDotProc, subclass_id);
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

class DarkStudioWindow final : public nfui::Window {
public:
    explicit DarkStudioWindow(HINSTANCE instance)
        : instance_(instance),
          resources_(instance),
          mode_(nfui::ThemeMode::dark),
          palette_(nfui::theme_palette(nfui::ThemeMode::dark)) {
    }

    ~DarkStudioWindow() noexcept override {
        destroy_icons();
    }

    // CP-B10: seed the requested mode before create_main so the first paint
    // already shows the right palette. Also seeds ThemeBroker so a runtime
    // toggle has a stable baseline.
    [[nodiscard]] bool set_initial_theme(nfui::ThemeMode mode) noexcept {
        if (hwnd() != nullptr) return false;
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        nfui::ThemeBroker::instance().set_theme(mode);
        return true;
    }

    [[nodiscard]] bool create_main(int show_command) noexcept {
        nfui::WindowCreateParams params{
            instance_,
            L"NativeFrameUIDarkStudioWindow",
            L"NativeFrame UI DarkStudio",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            // CP36: shrink from 1320×840 → 940×700 so the demo sits in
            // a compact window consistent with the rest of the
            // compact-mode suite.
            940,
            700,
        };

        if (!create(params)) {
            return false;
        }

        dpi_ = nfui::DpiScale(nfui::dpi_of(hwnd()));
        apply_window_icon();
        if (!create_children()) {
            return false;
        }

        // CP-B10: register with ThemeBroker so runtime switches broadcast
        // back to this window. Unregister in WM_NCDESTROY.
        nfui::ThemeBroker::instance().register_hwnd(hwnd(),
            [this](nfui::ThemeMode mode) { apply_theme(mode); });

        refresh_layout();
        update_status();

        ShowWindow(hwnd(), show_command);
        UpdateWindow(hwnd());
        return true;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override {
        switch (message) {
        case WM_SIZE:
            refresh_layout();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            const POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            bool redraw = false;
            if (select_navigation(pt)) {
                update_status();
                redraw = true;
            }
            if (hit_theme_toggle(pt)) {
                cycle_theme();
                redraw = true;
            }
            if (redraw) {
                InvalidateRect(hwnd(), nullptr, FALSE);
            }
            return 0;
        }
        case WM_THEMECHANGED:
            // CP-B10: ThemeBroker broadcasts here after a runtime switch.
            apply_theme(nfui::ThemeBroker::instance().current());
            return 0;
        case WM_NCDESTROY:
            nfui::ThemeBroker::instance().unregister_hwnd(hwnd());
            destroy_icons();
            return nfui::Window::handle_message(message, wparam, lparam);
        case WM_DPICHANGED: {
            auto* suggested = reinterpret_cast<RECT*>(lparam);
            dpi_ = nfui::DpiScale(HIWORD(wparam));
            if (suggested != nullptr) {
                SetWindowPos(hwnd(),
                             nullptr,
                             suggested->left,
                             suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }
            apply_native_fonts();
            refresh_layout();
            InvalidateRect(hwnd(), nullptr, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC hdc = BeginPaint(hwnd(), &paint);
            paint_shell(hdc);
            EndPaint(hwnd(), &paint);
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
    [[nodiscard]] bool create_children() noexcept {
        nfui::ControlCreateParams params{
            instance_,
            hwnd(),
            101,
            L"",
            0,
            0,
            100,
            24,
        };
        status_.inject_theme(&palette_, &fonts_);
        if (!status_.create(params)) {
            return false;
        }
        // Install our dot subclass AFTER StatusBar's chrome subclass so
        // the dot proc runs first in the chain (last-installed-runs-first),
        // chains to the chrome subclass, then paints the dot on top.
        SetWindowSubclass(status_.hwnd(), &StatusBarDotProc,
                          kStatusBarDotSubclassId,
                          reinterpret_cast<DWORD_PTR>(&palette_));
        apply_native_fonts();
        return true;
    }

    void apply_native_fonts() noexcept {
        if (status_.hwnd() == nullptr || fonts() == nullptr) {
            return;
        }
        SendMessageW(status_.hwnd(),
                     WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts()->semibold(dpi_.dpi(), nfui::font_pt::ui)),
                     TRUE);
    }

    [[nodiscard]] nfui::FontCache* fonts() noexcept {
        return &fonts_;
    }

    void apply_window_icon() noexcept {
        if (!resources_.has_icon(IDI_NFUI_APP)) {
            return;
        }

        large_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXICON),
                                                    GetSystemMetrics(SM_CYICON),
                                                    LR_DEFAULTCOLOR));
        small_icon_ = static_cast<HICON>(LoadImageW(instance_,
                                                    MAKEINTRESOURCEW(IDI_NFUI_APP),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXSMICON),
                                                    GetSystemMetrics(SM_CYSMICON),
                                                    LR_DEFAULTCOLOR));
        if (large_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon_));
        }
        if (small_icon_ != nullptr) {
            SendMessageW(hwnd(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon_));
        }
    }

    void destroy_icons() noexcept {
        if (large_icon_ != nullptr) {
            DestroyIcon(large_icon_);
            large_icon_ = nullptr;
        }
        if (small_icon_ != nullptr) {
            DestroyIcon(small_icon_);
            small_icon_ = nullptr;
        }
        if (italic_brand_font_ != nullptr) {
            DeleteObject(italic_brand_font_);
            italic_brand_font_ = nullptr;
            italic_brand_font_dpi_ = 0;
            italic_brand_font_pt_ = 0;
        }
    }

    // Build (or return cached) italic serif font at the requested size.
    // DT_ITALIC isn't a real DrawTextW flag, so we synthesise italic by
    // requesting lfItalic=TRUE from CreateFontIndirectW. Keyed on (dpi, pt)
    // so a single instance survives across paints.
    [[nodiscard]] HFONT get_italic_serif(int dpi_value, int point_size) noexcept {
        if (italic_brand_font_ != nullptr &&
            italic_brand_font_dpi_ == dpi_value &&
            italic_brand_font_pt_ == point_size) {
            return italic_brand_font_;
        }
        if (italic_brand_font_ != nullptr) {
            DeleteObject(italic_brand_font_);
            italic_brand_font_ = nullptr;
        }
        // Match FontCache's serif (Georgia) and mono metrics so the brand
        // sits on the same baseline as the rest of the chrome.
        LOGFONTW lf{};
        lf.lfHeight = -MulDiv(point_size, dpi_value, 72);
        lf.lfWeight = FW_NORMAL;
        lf.lfItalic = TRUE;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = CLEARTYPE_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_ROMAN;
        wcsncpy_s(lf.lfFaceName, L"Georgia", _TRUNCATE);
        italic_brand_font_ = CreateFontIndirectW(&lf);
        italic_brand_font_dpi_ = dpi_value;
        italic_brand_font_pt_ = point_size;
        return italic_brand_font_;
    }

    void refresh_layout() noexcept {
        if (hwnd() == nullptr || status_.hwnd() == nullptr) {
            return;
        }

        dpi_ = nfui::DpiScale(GetDpiForWindow(hwnd()));
        GetClientRect(hwnd(), &client_rect_);
        SendMessageW(status_.hwnd(), WM_SIZE, 0, 0);
    }

    [[nodiscard]] int status_height() const noexcept {
        if (status_.hwnd() == nullptr) {
            return 0;
        }

        RECT status_rect{};
        GetWindowRect(status_.hwnd(), &status_rect);
        return status_rect.bottom - status_rect.top;
    }

    [[nodiscard]] RECT content_rect() const noexcept {
        RECT content = client_rect_;
        content.bottom -= status_height();
        return content;
    }

    [[nodiscard]] RECT navigation_rect(std::size_t index) const noexcept {
        const RECT content = content_rect();
        const auto px = [&](int logical) noexcept { return dpi_.logical_to_pixels(logical); };
        const int outer = px(tok::spacing_lg);
        const int rail_width = px(tok::spacing_xl * 7 + tok::spacing_md);
        const int nav_height = px(tok::control_height_lg + tok::spacing_sm);
        const int nav_gap = px(tok::spacing_xs);
        const int brand_height = px(tok::font_hero * 4 + tok::spacing_sm + tok::spacing_xs);
        const int brand_gap = px(tok::spacing_md);
        const int nav_inset = px(tok::spacing_xs);
        const int nav_left = content.left + outer + nav_inset;
        const int nav_width = rail_width - nav_inset * 2;
        const int nav_top = content.top + outer + brand_height + brand_gap
                          + static_cast<int>(index) * (nav_height + nav_gap);
        return make_rect(nav_left, nav_top, nav_width, nav_height);
    }

    [[nodiscard]] bool select_navigation(POINT point) noexcept {
        for (std::size_t index = 0; index < navigation_items.size(); ++index) {
            RECT rect = navigation_rect(index);
            if (PtInRect(&rect, point) != FALSE && selected_navigation_ != static_cast<int>(index)) {
                selected_navigation_ = static_cast<int>(index);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] RECT theme_toggle_rect() const noexcept {
        const RECT content = content_rect();
        const auto px = [&](int logical) noexcept { return dpi_.logical_to_pixels(logical); };
        const int outer = px(tok::spacing_lg);
        const int sidebar_w = px(tok::spacing_xl * 7 + tok::spacing_md);
        const int brand_h = px(tok::font_hero * 4 + tok::spacing_sm + tok::spacing_xs);
        const int toggle_w = px(tok::spacing_xl * 2 + tok::spacing_md);
        const int toggle_h = px(tok::control_height_sm);
        RECT brand_area = make_rect(content.left + outer,
                                    content.top + outer,
                                    sidebar_w, brand_h);
        return make_rect(brand_area.right - outer - toggle_w,
                         brand_area.top + px(tok::font_hero * 3 + tok::spacing_sm),
                         toggle_w, toggle_h);
    }

    [[nodiscard]] bool hit_theme_toggle(POINT point) const noexcept {
        RECT toggle = theme_toggle_rect();
        return PtInRect(&toggle, point) != FALSE;
    }

    void cycle_theme() noexcept {
        // CP-B10: cycle light -> dark -> high_contrast -> light.
        if (mode_ == nfui::ThemeMode::light) {
            mode_ = nfui::ThemeMode::dark;
        } else if (mode_ == nfui::ThemeMode::dark) {
            mode_ = nfui::ThemeMode::high_contrast;
        } else {
            mode_ = nfui::ThemeMode::light;
        }
        nfui::ThemeBroker::instance().set_theme(mode_);
    }

    void apply_theme(nfui::ThemeMode mode) noexcept {
        if (mode_ == mode) return;
        mode_ = mode;
        palette_ = nfui::theme_palette(mode);
        // Re-inject the new palette into the status bar so its chrome and our
        // dot subclass repaint with the new semantic colours.
        if (status_.hwnd() != nullptr) {
            status_.inject_theme(&palette_, &fonts_);
            apply_native_fonts();
            update_status();
        }
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    void update_status() noexcept {
        if (status_.hwnd() != nullptr) {
            // Static "Ready" indicator regardless of selected nav — the
            // spec wants a single, stable status message with the green dot.
            // Leading spaces give the dot room to live inside the bar's
            // 6 logical px left-padding reservation without colliding with
            // the "R" of "Ready".
            SendMessageW(status_.hwnd(),
                         SB_SETTEXTW,
                         0,
                         reinterpret_cast<LPARAM>(L"   Ready"));
        }
    }

    void paint_shell(HDC hdc) noexcept {
        const nfui::ThemePalette& p = palette_;
        const bool hc = nfui::is_high_contrast(p);
        const RECT content = content_rect();
        const auto px = [&](int logical) noexcept { return dpi_.logical_to_pixels(logical); };
        const int outer = px(tok::spacing_lg);
        const int gap = px(tok::spacing_md);
        const int sidebar_w = px(tok::spacing_xl * 7 + tok::spacing_md);
        const int card_radius = px(nfui::theme_metrics().corner_radius_card);
        const int pill_radius = px(nfui::theme_metrics().corner_radius_control);
        const int dpi_value = dpi_.dpi();

        // Flicker-free offscreen buffer scoped to the content area (above the
        // native status bar). The MemoryDC BitBlts back at the rect's actual
        // origin on destruction.
        nfui::MemoryDC mem(hdc, content);
        HDC target = mem.valid() ? mem.dc() : hdc;

        const nfui::Color rail_fill = nfui::darken(p.background, 0.15f);

        // ----- Geometry -----------------------------------------------------
        RECT sidebar = make_rect(content.left + outer,
                                 content.top + outer,
                                 sidebar_w,
                                 rect_height(content) - outer * 2);

        // Brand area height hosts the italic title, subtitle, and a functional
        // theme-toggle pill on its own row beneath them.
        const int brand_h = px(tok::font_hero * 4 + tok::spacing_sm + tok::spacing_xs);
        RECT brand_area = make_rect(sidebar.left, sidebar.top,
                                    sidebar.right - sidebar.left, brand_h);

        // Theme toggle pill — right-aligned beneath the subtitle. CP-B10: now
        // functional and cycles light -> dark -> high_contrast -> light.
        RECT theme_toggle = theme_toggle_rect();

        // Brand title (italic serif) and subtitle at full sidebar width.
        const int brand_pad_x = px(tok::spacing_lg);
        RECT brand_title = make_rect(brand_area.left + brand_pad_x,
                                     brand_area.top + px(tok::spacing_md),
                                     sidebar.right - sidebar.left - brand_pad_x * 2,
                                     px(tok::font_title + tok::spacing_sm));
        RECT brand_subtitle = make_rect(brand_area.left + brand_pad_x,
                                        brand_title.bottom + px(tok::spacing_xs),
                                        sidebar.right - sidebar.left - brand_pad_x * 2,
                                        px(tok::control_height_sm));

        // Main area geometry.
        const int main_x = sidebar.right + gap;
        const int main_w = rect_width(content) - sidebar_w - outer * 2 - gap;
        RECT main = make_rect(main_x, content.top + outer,
                              main_w, rect_height(content) - outer * 2);

        const int desc_h  = px(tok::control_height_lg * 3 + tok::spacing_lg);
        const int prev_h  = px(tok::control_height_lg * 7 + tok::spacing_lg);
        const int stat_h  = px(tok::control_height_lg * 4);

        RECT desc_card = make_rect(main.left, main.top, main_w, desc_h);
        RECT preview_card = make_rect(main.left, desc_card.bottom + gap, main_w, prev_h);
        const int stat_w = (main_w - gap) / 2;
        RECT stat_one = make_rect(main.left, preview_card.bottom + gap,
                                  stat_w, stat_h);
        RECT stat_two = make_rect(stat_one.right + gap, stat_one.top,
                                  main_w - stat_w - gap, stat_h);

        // ----- Background + sidebar -----------------------------------------
        nfui::fill_rect(target, content, p.background);
        nfui::fill_rect(target, sidebar, rail_fill);

        // ----- Theme toggle pill --------------------------------------------
        const int toggle_h = rect_height(theme_toggle);
        const int toggle_r = toggle_h / 2;
        nfui::fill_rounded_rect(target, theme_toggle, toggle_r,
                                p.surface_hover, p.border);
        // Small accent dot inside the pill, indicating the active state.
        const int dot_d = std::max<int>(4, px(tok::spacing_xs + tok::spacing_xs));
        RECT pill_dot = make_rect(theme_toggle.left + px(tok::spacing_md - tok::spacing_xs),
                                  theme_toggle.top + (toggle_h - dot_d) / 2,
                                  dot_d, dot_d);
        nfui::fill_ellipse(target, pill_dot, p.accent);
        // Dynamic label shows the currently-active mode; cycles on click.
        const wchar_t* mode_label =
            (mode_ == nfui::ThemeMode::light)        ? L"Light"
            : (mode_ == nfui::ThemeMode::dark)       ? L"Dark"
                                                    : L"High contrast";
        RECT pill_label = make_rect(pill_dot.right + px(tok::spacing_xs),
                                    theme_toggle.top,
                                    theme_toggle.right - pill_dot.right - px(tok::spacing_sm),
                                    toggle_h);
        nfui::draw_text(target, pill_label, mode_label,
                        fonts()->semibold(dpi_value, nfui::font_pt::sm), p.text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // ----- Brand title + subtitle ---------------------------------------
        // Italic serif "DarkStudio" in accent coral at xl=28. DT_ITALIC is not
        // a DrawTextW format flag, so we build a one-shot italic serif font
        // via LOGFONT and use it directly. The handle is cached in the
        // member and torn down in the destructor.
        HFONT italic_brand = get_italic_serif(dpi_value, nfui::font_pt::xl);
        nfui::draw_text(target, brand_title, L"DarkStudio",
                        italic_brand, p.accent,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        // "Preview-first shell" subtitle at sm=13 in muted text.
        nfui::draw_text(target, brand_subtitle, L"Preview-first shell",
                        fonts()->regular(dpi_value, nfui::font_pt::sm),
                        p.text_secondary,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        // ----- Sidebar nav rows ---------------------------------------------
        for (std::size_t index = 0; index < navigation_items.size(); ++index) {
            RECT item = navigation_rect(index);
            const bool selected = selected_navigation_ == static_cast<int>(index);
            if (selected) {
                nfui::fill_rounded_rect(target, item, pill_radius, p.surface_hover, p.surface_hover);
                // 2 px coral left bar per the spec.
                RECT bar = make_rect(item.left, item.top + px(tok::spacing_xs),
                                     px(tok::spacing_xs / 2),
                                     rect_height(item) - px(tok::spacing_xs) * 2);
                nfui::fill_rect(target, bar, p.accent);
            }

            // Custom monochrome vector icon at the left of the row.
            const int icon_side = px(tok::spacing_xl - tok::spacing_xs);
            RECT icon_bounds = make_rect(
                item.left + px(tok::spacing_md),
                item.top + (rect_height(item) - icon_side) / 2,
                icon_side, icon_side);
            const NavIcon nav_kind = (index == 0) ? NavIcon::Surface
                                    : (index == 1) ? NavIcon::Canvas
                                    : (index == 2) ? NavIcon::Tokens
                                    :                NavIcon::Publish;
            draw_nav_icon(target, icon_bounds, nav_kind,
                          selected ? p.accent : p.text_secondary,
                          px(tok::spacing_xs / 2));

            // Label.
            RECT label = make_rect(icon_bounds.right + px(tok::spacing_md),
                                   item.top,
                                   item.right - icon_bounds.right - px(tok::spacing_md),
                                   rect_height(item));
            nfui::draw_text(target, label, navigation_items[index],
                            selected ? fonts()->semibold(dpi_value, nfui::font_pt::base)
                                     : fonts()->regular(dpi_value, nfui::font_pt::base),
                            selected ? p.text : p.text_secondary,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // ----- Preview-first shell (description) card -----------------------
        // CP-B10: suppress decorative shadows in high contrast; cards read via
        // 1px borders there.
        if (!hc) {
            nfui::paint_drop_shadow(target, desc_card, card_radius, 1, p.shadow);
        }
        nfui::fill_rounded_rect(target, desc_card, card_radius, p.surface, p.border);

        // Title sits at the top of the card with a fixed height; the body rect is
        // a separate rect below it so DT_WORDBREAK can size to the remaining
        // vertical space without inheriting the title's reserved line.
        const int card_pad = px(tok::spacing_lg);
        const int card_title_h = px(tok::font_title + tok::spacing_sm);
        RECT desc_title = make_rect(desc_card.left + card_pad,
                                    desc_card.top + card_pad,
                                    desc_card.right - desc_card.left - card_pad * 2,
                                    card_title_h);
        nfui::draw_text(target, desc_title, L"Preview-first shell",
                        fonts()->semibold(dpi_value, nfui::font_pt::md), p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        RECT desc_body = make_rect(desc_title.left,
                                   desc_title.bottom + px(tok::spacing_sm),
                                   rect_width(desc_title),
                                   desc_card.bottom - desc_title.bottom - card_pad);
        nfui::draw_text(target, desc_body,
                        L"Every change to the theme shows up here instantly, without recompiling or restarting.",
                        fonts()->regular(dpi_value, nfui::font_pt::sm), p.text_secondary,
                        DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        // ----- Preview canvas card ------------------------------------------
        if (!hc) {
            nfui::paint_drop_shadow(target, preview_card, card_radius, 1, p.shadow);
        }
        nfui::fill_rounded_rect(target, preview_card, card_radius, p.surface, p.border);

        // Title sits at the top of the card with a fixed height so the code
        // block anchors to the title baseline instead of the card bottom.
        RECT preview_title = make_rect(preview_card.left + card_pad,
                                       preview_card.top + card_pad,
                                       preview_card.right - preview_card.left - card_pad * 2,
                                       card_title_h);
        nfui::draw_text(target, preview_title, L"Preview canvas",
                        fonts()->semibold(dpi_value, nfui::font_pt::md), p.text,
                        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        // Code block: a slightly darker rounded rect inside the card. The
        // block's height is sized to comfortably hold 8 lines at the mono
        // line height plus vertical padding.
        const int line_height = px(tok::font_title);
        const int code_pad_x  = px(tok::spacing_md);
        const int code_pad_y  = px(tok::font_subtitle);
        const int code_block_h = static_cast<int>(code_snippet.size()) * line_height + code_pad_y * 2;
        RECT code_block = make_rect(preview_card.left + card_pad,
                                    preview_title.bottom + px(tok::spacing_md),
                                    preview_card.right - preview_card.left - card_pad * 2,
                                    code_block_h);
        nfui::fill_rounded_rect(target, code_block, px(tok::radius_sm),
                                nfui::darken(p.surface, 0.18f), p.border);

        // Render each line: right-aligned line number in the left margin,
        // then the tokenised code content.
        const int line_num_w = px(tok::spacing_xl - tok::spacing_xs);
        HFONT mono_font  = fonts()->mono(dpi_value, nfui::font_pt::sm);
        HFONT mono_xs    = fonts()->mono(dpi_value, nfui::font_pt::xs);
        for (std::size_t i = 0; i < code_snippet.size(); ++i) {
            const int y = code_block.top + code_pad_y + static_cast<int>(i) * line_height;
            RECT ln_rect = make_rect(code_block.left + code_pad_x, y,
                                     line_num_w, line_height);
            wchar_t line_str[8]{};
            swprintf_s(line_str, L"%zu", i + 1);
            nfui::draw_text(target, ln_rect, line_str, mono_xs, p.text_secondary,
                            DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            const int code_x = code_block.left + code_pad_x + line_num_w + px(tok::spacing_sm);
            render_code_line(target, code_x, y, code_snippet[i], mono_font, p);
        }

        // ----- Stat cards ---------------------------------------------------
        auto draw_stat_card = [&](const RECT& card, const wchar_t* label_text,
                                  const wchar_t* value_text, const wchar_t* helper_text) {
            if (!hc) {
                nfui::paint_drop_shadow(target, card, card_radius, 1, p.shadow);
            }
            nfui::fill_rounded_rect(target, card, card_radius, p.surface, p.border);

            RECT inner = inset_rect(card, px(tok::spacing_lg));
            // Label (sm=13 muted).
            nfui::draw_text(target, inner, label_text,
                            fonts()->regular(dpi_value, nfui::font_pt::sm),
                            p.text_secondary,
                            DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            // Value (lg=20 bold accent coral).
            inner.top += px(tok::font_title + tok::spacing_sm);
            nfui::draw_text(target, inner, value_text,
                            fonts()->bold(dpi_value, nfui::font_pt::lg), p.accent,
                            DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            // Helper description (sm=13 muted, wrapped).
            inner.top += px(tok::font_title + tok::spacing_lg);
            nfui::draw_text(target, inner, helper_text,
                            fonts()->regular(dpi_value, nfui::font_pt::sm),
                            p.text_secondary,
                            DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        };
        draw_stat_card(stat_one, L"Native shell", L"100%",
                       L"Sample-local painting with a standard status bar.");
        draw_stat_card(stat_two, L"DPI layout", L"Per-monitor",
                       L"All shell measurements scale through nfui::DpiScale.");

        // FontCache owns the HFONT handles; they are released by fonts_' destructor.
    }

    HINSTANCE instance_{};
    nfui::ResourceContext resources_;
    nfui::ThemeMode mode_{nfui::ThemeMode::dark};
    nfui::ThemePalette palette_;
    nfui::FontCache fonts_;
    nfui::StatusBar status_;
    nfui::DpiScale dpi_{96};
    RECT client_rect_{};
    int selected_navigation_{};
    HICON large_icon_{};
    HICON small_icon_{};
    HFONT italic_brand_font_{};
    int italic_brand_font_dpi_{0};
    int italic_brand_font_pt_{0};
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmd_line, int show_command) {
    nfui::Application app({instance, show_command});
    if (!nfui::Application::initialize_process_dpi() ||
        !nfui::Application::initialize_common_controls()) {
        return 1;
    }

    // CP-B10: parse --theme so the audit can capture light / dark / HC. Default
    // remains dark to match the sample's identity.
    auto parse_theme = [](PCWSTR cl) noexcept {
        if (cl == nullptr) return nfui::ThemeMode::dark;
        const wchar_t* tag = wcsstr(cl, L"--theme");
        if (tag == nullptr) return nfui::ThemeMode::dark;
        tag += 7;
        while (*tag == L' ' || *tag == L'\t') ++tag;
        if (*tag == L'"') ++tag;
        if (wcsncmp(tag, L"light", 5) == 0) return nfui::ThemeMode::light;
        if (wcsncmp(tag, L"dark", 4) == 0) return nfui::ThemeMode::dark;
        if (wcsncmp(tag, L"high_contrast", 13) == 0) return nfui::ThemeMode::high_contrast;
        return nfui::ThemeMode::dark;
    };

    DarkStudioWindow window(instance);
    (void)window.set_initial_theme(parse_theme(cmd_line));
    if (!window.create_main(show_command)) {
        return 2;
    }

    return app.run();
}