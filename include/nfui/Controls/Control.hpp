#pragma once

#include <nfui/Handle.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Font.hpp>
#include <nfui/HoverState.hpp>
#include <nfui/Clock.hpp>

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

    // CP17: honor the system "client-area animation" setting. Returns false
    // when the user has disabled UI animations (Performance / Visual Effects
    // → "Animate controls and elements inside windows"); callers should
    // resolve transitions instantly in that case. Public so Window subclasses
    // (samples) can query it too, not just Control subclasses. Stateless,
    // cheap, UI-thread only.
    [[nodiscard]] static bool system_animations_enabled() noexcept;

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

    // CP17: micro-animation driver. The base owns a per-HWND Win32 timer
    // (SetTimer/KillTimer) dispatched through a WM_TIMER arm in subclass_proc.
    // This sidesteps the scheduler-ownership problem (no singleton, no
    // ApplicationContext): each animating control owns its own OS timer, killed
    // in detach_destroyed_hwnd (WM_NCDESTROY). Leaves override on_animation_tick
    // to advance their animation state and InvalidateRect (async — never
    // UpdateWindow, to avoid re-entering on_paint inside the tick).
    //
    // `now_ms` is read through the injectable Clock so the pure animation
    // logic is testable without a message loop. Tests inject a fake clock;
    // production leaves the default (a stateless Win32Clock).
    void set_clock(const Clock* clock) noexcept { clock_ = clock; }
    [[nodiscard]] unsigned long long clock_now_ms() const noexcept;
    void start_anim_timer(unsigned int period_ms) noexcept;
    void stop_anim_timer() noexcept;
    [[nodiscard]] bool anim_timer_running() const noexcept { return anim_timer_ != 0; }
    virtual void on_animation_tick([[maybe_unused]] unsigned long long now_ms) noexcept {}

private:
    void detach_destroyed_hwnd(HWND hwnd) noexcept;
    static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR subclass_id, DWORD_PTR ref_data) noexcept;

    const ThemePalette* palette_{nullptr};
    FontCache* fonts_{nullptr};
    HoverState hover_state_;
    std::wstring caption_;
    OwnedHwnd hwnd_{};
    const Clock* clock_{nullptr};
    UINT_PTR anim_timer_{0};
};

} // namespace nfui
