#pragma once

#include <nfui/Theme.hpp>

#include <windows.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace nfui {

class Menu;
class MenuBuilder;
class MenuOwnerDraw;

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

// CP-B15: per-item menu chrome paint / measure helper. Host HWNDs that
// build a Menu with enable_owner_draw() must forward WM_DRAWITEM /
// WM_MEASUREITEM / WM_MENUCHAR to this class. The dispatcher reads the
// item payload (a pointer the builder stashed via SetMenuItemInfo at
// append time) and paints the themed item onto the DC.
//
// The helper owns a cache of item label strings keyed by item id so the
// paint path can resolve a label without keeping the host's Menu in
// scope. Each Menu with enable_owner_draw() owns its own dispatcher; the
// dispatcher survives as long as the Menu does.
class MenuOwnerDraw {
public:
    explicit MenuOwnerDraw(const ThemePalette& palette) noexcept;

    // WM_MEASUREITEM dispatcher: returns a 24 logical px tall item
    // (DPI-scaled). Width is reported as 0 so Windows sizes the menu by
    // content. The host HWND's WindowProc must call this from its
    // WM_MEASUREITEM arm.
    void handle_measure_item(MEASUREITEMSTRUCT* mis) noexcept;

    // WM_DRAWITEM dispatcher: paints the themed item chrome onto
    // mis->hDC inside the supplied item rect. Returns true if the item
    // was painted (caller must skip DefWindowProc for it), false if the
    // helper has no record for the item (the system draws it).
    bool handle_draw_item(const DRAWITEMSTRUCT* dis) noexcept;

    // Insert / update the label cache. The builder calls this implicitly
    // when emitting MF_OWNERDRAW items; consumers should not need to
    // touch the cache directly.
    void register_label(int command_id, std::wstring label, bool is_separator = false) noexcept;

private:
    const ThemePalette& palette_;
    // CP-B15: flat map keyed by command id. Owner-draw menus are small
    // (dozens, not thousands), so a std::unordered_map avoids the cost
    // of sorting and lets the dispatcher hit in O(1).
    struct Entry {
        std::wstring label;
        bool is_separator{false};
    };
    std::unordered_map<int, Entry> entries_;
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

    // CP-B15: opt this builder (and every popup child it creates) into
    // per-item owner-draw. Subsequent item / separator / check_item calls
    // emit MF_OWNERDRAW entries and stash the label as item data so the
    // host HWND's WM_DRAWITEM handler can paint themed chrome (rounded
    // selection, accent for checked, palette.divider hairline). Requires
    // Menu::enable_owner_draw() to have been called on the owning Menu.
    [[nodiscard]] MenuBuilder& with_owner_draw() noexcept {
        owner_draw_ = true;
        return *this;
    }

private:
    explicit MenuBuilder(OwnedMenu& menu, HBRUSH brush, HMENU target,
                         bool owner_draw, Menu* parent = nullptr) noexcept
        : menu_(menu), brush_(brush), target_(target), owner_draw_(owner_draw), parent_(parent) {}

    OwnedMenu& menu_;
    HBRUSH brush_{};
    HMENU target_{};
    // CP-B15: propagates to every popup() child so a with_owner_draw()
    // call on the bar builder enables MF_OWNERDRAW for every item the
    // builder produces (and every item produced by its descendants).
    bool owner_draw_{false};
    // CP-B15: pointer back to the owning Menu so the builder can reach
    // the lazily-created owner_draw_ dispatcher. nullptr for standalone
    // builders (the test harness uses one without a Menu parent).
    Menu* parent_{nullptr};

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
    //
    // CP-B15: a palette swap propagates to the MenuOwnerDraw dispatcher
    // by reference (the dispatcher holds a const pointer). Callers using
    // owner-draw chrome see the new palette on the next WM_DRAWITEM
    // cycle without any explicit refresh.
    void set_palette(ThemePalette palette) noexcept { palette_ = std::move(palette); }

    // CP-B15: enables MF_OWNERDRAW for every item the builder produces
    // and returns a reference to a per-menu MenuOwnerDraw dispatcher the
    // host HWND forwards WM_DRAWITEM / WM_MEASUREITEM to. Call this BEFORE
    // calling builder(); subsequent builder().with_owner_draw() calls
    // turn on MF_OWNERDRAW. The dispatcher survives as long as this Menu.
    [[nodiscard]] MenuOwnerDraw& enable_owner_draw() noexcept;

    // CP-B15: read-only accessor used by MenuBuilder to resolve the
    // lazily-created dispatcher when emitting MF_OWNERDRAW items.
    // Returns nullptr until enable_owner_draw() is called.
    [[nodiscard]] MenuOwnerDraw* owner_draw() const noexcept { return owner_draw_.get(); }

    // Item label helpers.
    [[nodiscard]] static std::wstring escape_mnemonic(const std::wstring& text) noexcept;

private:
    ThemePalette palette_;
    OwnedMenu bar_;
    // CP-B15: lazily created on first enable_owner_draw() call. Host
    // HWND forwards WM_DRAWITEM / WM_MEASUREITEM here. nullptr until
    // enable_owner_draw() is called, so existing callers that use
    // MENUINFO-only chrome pay no memory cost.
    std::unique_ptr<MenuOwnerDraw> owner_draw_;

    friend class MenuBuilder;
};

} // namespace nfui