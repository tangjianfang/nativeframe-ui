#pragma once

#include <nfui/Theme.hpp>

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

namespace nfui {

class Menu;
class MenuBuilder;

// CP24-A: RAII HMENU wrapper. Owns the menu handle and (optionally) the
// themed background brush attached via MENUINFO. DestroyMenu / DeleteObject
// fire on destruction; reset() does the same explicitly.
class OwnedMenu {
public:
    OwnedMenu() noexcept = default;
    explicit OwnedMenu(HMENU handle) noexcept : handle_(handle) {}

    OwnedMenu(const OwnedMenu&) = delete;
    OwnedMenu& operator=(const OwnedMenu&) = delete;

    OwnedMenu(OwnedMenu&& other) noexcept : handle_(other.handle_), brush_(other.brush_) {
        other.handle_ = nullptr;
        other.brush_ = nullptr;
    }
    OwnedMenu& operator=(OwnedMenu&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            brush_ = other.brush_;
            other.handle_ = nullptr;
            other.brush_ = nullptr;
        }
        return *this;
    }

    ~OwnedMenu() noexcept { reset(); }

    [[nodiscard]] HMENU get() const noexcept { return handle_; }
    [[nodiscard]] HMENU release() noexcept {
        HMENU h = handle_;
        handle_ = nullptr;
        // Intentionally keep `brush_` — the brush is owned by the menu, not
        // the HMENU lifetime. Caller is now responsible for the menu.
        return h;
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr;
    }

    void reset() noexcept {
        if (handle_ != nullptr) {
            DestroyMenu(handle_);
            handle_ = nullptr;
        }
        if (brush_ != nullptr) {
            DeleteObject(brush_);
            brush_ = nullptr;
        }
    }

private:
    HMENU handle_{};
    HBRUSH brush_{};

    friend class Menu;
    friend class MenuBuilder;
};

// CP24-A: fluent builder for OwnedMenu. Holds a reference to the menu and
// a brush (palette.surface) so sub-popups created via popup() inherit the
// same themed chrome. Builder lifetime is bound to the caller.
class MenuBuilder {
public:
    MenuBuilder(OwnedMenu& menu, HBRUSH brush) noexcept
        : menu_(menu), brush_(brush) {}

    // CP24-A: popup() appends a child HMENU to the current target and
    // returns a builder that targets the child. Subsequent .item/.separator
    // calls on the returned builder populate that submenu. Use this for
    // nested menus:
    //   menu.builder(bar).popup(L"&File")   // returns FileBuilder
    //       .item(L"E&xit", IDM_NFUI_EXIT)  // populates the &File popup
    //       .item(L"&Reload", id_reload);   // ditto
    [[nodiscard]] MenuBuilder popup(const std::wstring& label) noexcept;
    [[nodiscard]] MenuBuilder& item(const std::wstring& label, int command_id, bool enabled = true) noexcept;
    [[nodiscard]] MenuBuilder& separator() noexcept;
    [[nodiscard]] MenuBuilder& check_item(const std::wstring& label, int command_id, bool checked) noexcept;

private:
    explicit MenuBuilder(OwnedMenu& menu, HBRUSH brush, HMENU target) noexcept
        : menu_(menu), brush_(brush), target_(target) {}

    OwnedMenu& menu_;
    HBRUSH brush_{};
    HMENU target_{};

    friend class Menu;
};

// CP24-A: lightweight menu wrapper that pairs an OwnedMenu with a
// ThemePalette so callers can build theming into the menu surface via
// MENUINFO without juggling HMENU lifetimes by hand.
//
// Typical use:
//   nfui::Menu menu(nfui::theme_palette(nfui::ThemeMode::dark));
//   menu.builder(menu.bar())
//        .popup(L"&File")
//            .item(L"E&xit", IDM_NFUI_EXIT);
//   SetMenu(hwnd, menu.bar().get());
//
// For context menus:
//   OwnedMenu popup = menu.make_popup();
//   menu.builder(popup).item(L"&Refresh", id_refresh);
//   TrackPopupMenu(popup.get(), TPM_RIGHTBUTTON, x, y, 0, hwnd, nullptr);
class Menu {
public:
    explicit Menu(ThemePalette palette) noexcept;

    // The top-level menu bar (suitable for SetMenu). OwnedMenu releases on
    // destruction; call .release() if you want SetMenu to own it.
    [[nodiscard]] OwnedMenu& bar() noexcept { return bar_; }
    [[nodiscard]] const OwnedMenu& bar() const noexcept { return bar_; }

    // A fresh popup menu (suitable for TrackPopupMenu). Each call returns a
    // fresh menu; the caller is responsible for keeping it alive until the
    // popup closes (the OS does not take ownership).
    [[nodiscard]] OwnedMenu make_popup() noexcept;

    // Wrap an OwnedMenu in a fluent builder. The builder holds a reference
    // to the menu and an HBRUSH (palette.surface) so sub-popups created via
    // popup() inherit the same themed chrome.
    [[nodiscard]] MenuBuilder builder(OwnedMenu& menu) noexcept;

    // Apply palette-driven chrome to an OwnedMenu in-place: background
    // brush via MENUINFO. Idempotent — calling twice on the same menu
    // just rebuilds the brush.
    void apply_palette(OwnedMenu& menu) noexcept;

    // Convenience: same as apply_palette but for the bar(). When `host`
    // is non-null, also calls DrawMenuBar on it so the menu bar repaints
    // immediately with the freshly-installed brush. (Pass the HWND that
    // the menu has already been attached to with SetMenu; on Win10/11 the
    // MENUINFO background brush is only honored for popup submenus, so
    // this is a best-effort nudge, not a guarantee the bar surface will
    // visibly change — that's a known Win32 theme limitation.)
    void apply_to_bar(HWND host = nullptr) noexcept;

    // Read the palette that drives MENUINFO chrome.
    [[nodiscard]] const ThemePalette& palette() const noexcept { return palette_; }

    // CP32: lets a holder swap the palette after construction. The menu's
    // bar_ is move-only so callers can't reassign the whole Menu; this is
    // the supported way to retarget a built Menu (used by Workbench's
    // --theme seed). Does NOT re-attach the menu to a host HWND — the
    // caller still owns SetMenu.
    void set_palette(ThemePalette palette) noexcept { palette_ = palette; }

    // Item label helpers.
    [[nodiscard]] static std::wstring escape_mnemonic(const std::wstring& text) noexcept;

private:
    ThemePalette palette_;
    OwnedMenu bar_;
};

} // namespace nfui