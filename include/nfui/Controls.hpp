#pragma once

#include <nfui/Handle.hpp>

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

    [[nodiscard]] HWND hwnd() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

protected:
    [[nodiscard]] bool create_native(std::wstring_view class_name, const ControlCreateParams& params, DWORD extra_style) noexcept;

private:
    void detach_destroyed_hwnd(HWND hwnd) noexcept;
    static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR subclass_id, DWORD_PTR ref_data) noexcept;

    OwnedHwnd hwnd_{};
};

class Button : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;
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
