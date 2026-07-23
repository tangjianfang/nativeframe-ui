#include <nfui/Menu.hpp>
#include <nfui/Theme.hpp>

#include <algorithm>

namespace nfui {

namespace {

// CP24-A: build a solid brush from a palette colour. Caller owns the
// returned HBRUSH until DeleteObject.
[[nodiscard]] HBRUSH make_brush(Color c) noexcept {
    return CreateSolidBrush(c.rgb);
}

// CP24-A: apply MENUINFO with palette-driven chrome to an HMENU.
// MENUINFO fields used:
//   - dwStyle:        MIM_BACKGROUND (the brush) | MIM_APPLYTOSUBMENUS
//   - hbrBack:        owned by the OwnedMenu (DestroyMenu does NOT free
//                    it, so we cache it inside OwnedMenu::brush_ and free
//                    it on reset())
//   - brBorder:       unused on Win10/11 (the system paints the border)
//                    so we leave it zero and rely on the brush to carry
//                    the surface colour.
// CP24-A scope: we don't theme item text colour. SetMenuItemInfo does not
// expose item-level text colour without owner-draw; the system text colour
// tracks the user's high-contrast theme. Sufficient for the round.
[[nodiscard]] bool apply_menubar_palette(HMENU menu, HBRUSH brush) noexcept {
    if (menu == nullptr || brush == nullptr) {
        return false;
    }
    MENUINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    info.hbrBack = brush;
    return SetMenuInfo(menu, &info) != FALSE;
}

} // namespace

Menu::Menu(ThemePalette palette) noexcept
    : palette_(palette),
      bar_(OwnedMenu(CreateMenu())) {}

OwnedMenu Menu::make_popup() noexcept {
    OwnedMenu popup(CreatePopupMenu());
    apply_palette(popup);
    return popup;
}

MenuBuilder Menu::builder(OwnedMenu& menu) noexcept {
    // CP24-A: the builder needs a brush to apply MENUINFO to sub-popups it
    // creates via popup(). We synthesise one from the palette; the brush
    // outlives the builder because Menu holds the palette. The builder
    // targets the menu passed in directly (not a sub-popup).
    HBRUSH brush = CreateSolidBrush(palette_.surface.rgb);
    return MenuBuilder(menu, brush, menu.get());
}

void Menu::apply_palette(OwnedMenu& menu) noexcept {
    if (!menu) {
        return;
    }
    // Replace the previous brush if we owned one — the MENUINFO references
    // the brush by HBRUSH, so a palette swap must rebuild it.
    if (menu.brush_ != nullptr) {
        DeleteObject(menu.brush_);
        menu.brush_ = nullptr;
    }
    HBRUSH brush = make_brush(palette_.surface);
    if (brush == nullptr) {
        return;
    }
    if (apply_menubar_palette(menu.get(), brush)) {
        menu.brush_ = brush;
    } else {
        DeleteObject(brush);
    }
}

void Menu::apply_to_bar(HWND host) noexcept {
    apply_palette(bar_);
    // CP28-B: nudge the bar to repaint with the new MENUINFO brush. On
    // Win10/11 the menu bar chrome is themed and the background brush is
    // only honored for popup submenus, so DrawMenuBar is a best-effort
    // hint rather than a guarantee — but it prevents the stale theme
    // paint that would otherwise stick until the next WM_NCACTIVATE.
    if (host != nullptr && IsWindow(host) != FALSE) {
        DrawMenuBar(host);
    }
}

std::wstring Menu::escape_mnemonic(const std::wstring& text) noexcept {
    // Win32 menu labels use '&' as the mnemonic marker. If the input
    // already contains '&&' (a literal '&'), MenuBuilder treats the pair
    // as one literal; this helper is a pass-through today but provides a
    // stable hook for future escaping (e.g., stripping stray '&' from
    // labels that came from untrusted sources).
    return text;
}

MenuBuilder MenuBuilder::popup(const std::wstring& label) noexcept {
    if (!menu_) {
        return MenuBuilder(menu_, brush_, nullptr);
    }
    HMENU parent = target_ != nullptr ? target_ : menu_.get();
    HMENU sub = CreatePopupMenu();
    if (sub == nullptr) {
        return MenuBuilder(menu_, brush_, nullptr);
    }
    AppendMenuW(parent,
                 MF_POPUP | MF_STRING,
                 reinterpret_cast<UINT_PTR>(sub),
                 label.c_str());
    // CP24-A: sub-menus get the same themed brush so nested menus read as
    // a continuation of the bar. The brush is owned by the parent builder;
    // the returned child shares the same HBRUSH reference (it must outlive
    // the OS menu, which it does because Menu owns the palette).
    if (brush_ != nullptr) {
        MENUINFO info{};
        info.cbSize = sizeof(info);
        info.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
        info.hbrBack = brush_;
        SetMenuInfo(sub, &info);
    }
    return MenuBuilder(menu_, brush_, sub);
}

MenuBuilder& MenuBuilder::item(const std::wstring& label, int command_id, bool enabled) noexcept {
    if (!menu_ || target_ == nullptr) {
        return *this;
    }
    UINT flags = MF_STRING;
    if (!enabled) {
        flags |= MF_GRAYED | MF_DISABLED;
    }
    AppendMenuW(target_,
                 flags,
                 static_cast<UINT_PTR>(command_id),
                 label.c_str());
    return *this;
}

MenuBuilder& MenuBuilder::separator() noexcept {
    if (!menu_ || target_ == nullptr) {
        return *this;
    }
    AppendMenuW(target_, MF_SEPARATOR, 0, nullptr);
    return *this;
}

MenuBuilder& MenuBuilder::check_item(const std::wstring& label, int command_id, bool checked) noexcept {
    if (!menu_ || target_ == nullptr) {
        return *this;
    }
    UINT flags = MF_STRING;
    flags |= checked ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(target_,
                 flags,
                 static_cast<UINT_PTR>(command_id),
                 label.c_str());
    return *this;
}

} // namespace nfui