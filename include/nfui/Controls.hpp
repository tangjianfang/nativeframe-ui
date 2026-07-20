#pragma once

#include <nfui/Handle.hpp>
#include <nfui/Theme.hpp>
#include <nfui/Font.hpp>

#include <string>
#include <string_view>
#include <windows.h>

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
    void set_palette(const ThemePalette* palette) noexcept { palette_ = palette; }
    void set_font_cache(FontCache* fonts) noexcept { fonts_ = fonts; }

protected:
    [[nodiscard]] bool create_native(std::wstring_view class_name, const ControlCreateParams& params, DWORD extra_style) noexcept;
    [[nodiscard]] const std::wstring& caption() const noexcept { return caption_; }
    [[nodiscard]] const ThemePalette* palette() const noexcept { return palette_; }
    [[nodiscard]] FontCache* fonts() const noexcept { return fonts_; }
    virtual void on_paint(HDC dc, const PaintState& state) noexcept { (void)dc; (void)state; }
    [[nodiscard]] virtual bool wants_self_paint() const noexcept { return false; }

private:
    void detach_destroyed_hwnd(HWND hwnd) noexcept;
    static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR subclass_id, DWORD_PTR ref_data) noexcept;

    const ThemePalette* palette_{nullptr};
    FontCache* fonts_{nullptr};
    bool hover_{false};
    std::wstring caption_;
    OwnedHwnd hwnd_{};
};

class Button : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
};

class CheckBox : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class RadioButton : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class Edit : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class StaticText : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
    [[nodiscard]] bool wants_self_paint() const noexcept override { return true; }
};

class IconView : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_icon(HICON icon) noexcept;
protected:
    void on_paint(HDC dc, const PaintState& state) noexcept override;
private:
    HICON icon_{};
};

class ComboBox : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class ListBox : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class TreeView : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class ListView : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class StatusBar : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class TabControl : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class ProgressBar : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class Panel : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
};

class Splitter : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
    void set_ratio(double ratio) noexcept;
    [[nodiscard]] double ratio() const noexcept;

private:
    double ratio_{0.5};
};

} // namespace nfui
