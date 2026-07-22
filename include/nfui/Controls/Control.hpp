#pragma once

#include <nfui/Handle.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Font.hpp>
#include <nfui/HoverState.hpp>

#include <string>
#include <string_view>
#include <windows.h>
#include <commctrl.h>

namespace nfui {

struct ControlCreateParams {
    HINSTANCE instance{};
    HWND parent{};
    int control_id{};
    std::wstring_view text{};
    int x{};
    int y{};
    int width{100};
    int height{24};
    DWORD style{WS_CHILD | WS_VISIBLE | WS_TABSTOP};
    DWORD ex_style{};
};

class Control {
public:
    Control() = default;
    virtual ~Control() = default;

    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;

    Control(Control&&) = delete;
    Control& operator=(Control&&) = delete;

    struct PaintState {
        bool hover{false};
        bool pressed{false};
        bool focused{false};
        bool enabled{true};
        RECT bounds{};
    };

    [[nodiscard]] HWND hwnd() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

    // Visual dependencies injected by the owner before paint. Read-only pointers; not owned.
    // Re-injecting a palette is also the control-local palette-change notification:
    // native/custom chrome is refreshed by the leaf hook and the HWND is invalidated.
    void set_palette(const ThemePalette* palette) noexcept {
        palette_ = palette;
        if (hwnd() != nullptr) {
            on_palette_changed();
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
    }
    void set_font_cache(FontCache* fonts) noexcept {
        fonts_ = fonts;
        if (hwnd() != nullptr) {
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
    }

    // Bind both palette + FontCache in one call. Equivalent to:
    //   set_palette(palette);
    //   set_font_cache(fonts);
    void inject_theme(const ThemePalette* palette, FontCache* fonts) noexcept {
        set_palette(palette);
        set_font_cache(fonts);
    }

protected:
    [[nodiscard]] bool create_native(std::wstring_view class_name, const ControlCreateParams& params, DWORD extra_style) noexcept;
    [[nodiscard]] const std::wstring& caption() const noexcept { return caption_; }
    [[nodiscard]] const ThemePalette* palette() const noexcept { return palette_; }
    [[nodiscard]] FontCache* fonts() const noexcept { return fonts_; }
    virtual void on_palette_changed() noexcept {}
    virtual void on_paint(HDC dc, const PaintState& state) noexcept { (void)dc; (void)state; }
    [[nodiscard]] virtual bool wants_self_paint() const noexcept { return false; }

    // Per-component extension points. Default implementations are no-ops so
    // nfui_control_base has no transitive dependency on any leaf library
    // (e.g., nfui_listbox). Subclasses that need subclass-proc-level
    // dispatch override these. The subclass proc calls them with the raw
    // message arguments and lets the leaf perform any class-name-gated
    // work internally.
    virtual void on_reflected_draw_item(DRAWITEMSTRUCT*) noexcept {}
    virtual LRESULT on_custom_draw_item(NMLVCUSTOMDRAW*) noexcept { return 0; }
    virtual void on_subclass_mouse_move([[maybe_unused]] LPARAM lparam) noexcept {}
    virtual void on_subclass_mouse_leave() noexcept {}

private:
    void detach_destroyed_hwnd(HWND hwnd) noexcept;
    static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR subclass_id, DWORD_PTR ref_data) noexcept;

    const ThemePalette* palette_{nullptr};
    FontCache* fonts_{nullptr};
    HoverState hover_state_;
    std::wstring caption_;
    OwnedHwnd hwnd_{};
};

} // namespace nfui
