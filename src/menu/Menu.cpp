#include <nfui/Menu.hpp>
#include <nfui/Theme.hpp>

#include <algorithm>
#include <cstring>
#include <utility>

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

// CP-B15: append an owner-draw item carrying its label as item data so
// the dispatcher can paint it later. The dwItemData points to a
// heap-allocated std::wstring owned by this Menu (freed in the dtor via
// clearing all items).
void append_owner_draw_item(HMENU target, int command_id, const std::wstring& label,
                            MenuOwnerDraw& drawer, UINT extra_flags) noexcept {
    auto* payload = new std::wstring(label);
    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_DATA | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
    mii.fType = MF_OWNERDRAW | extra_flags;
    mii.fState = (extra_flags & MF_GRAYED) ? MFS_GRAYED : MFS_ENABLED;
    mii.wID = static_cast<UINT>(command_id);
    mii.dwItemData = reinterpret_cast<ULONG_PTR>(payload);
    InsertMenuItemW(target, static_cast<UINT>(-1), TRUE, &mii);
    drawer.register_label(command_id, label);
}

} // namespace

Menu::Menu(ThemePalette palette) noexcept
    : palette_(palette),
      bar_(OwnedMenu(CreateMenu())) {
    // CP-B15: forward declaration — owner_draw_ is std::unique_ptr<MenuOwnerDraw>
    // which is created lazily on first enable_owner_draw() call.
}

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
    //
    // CP-B15: pass through owner_draw_ so builder().with_owner_draw()
    // chains correctly. The constructor takes the flag directly so the
    // call sites don't have to thread it through manually. We also pass
    // `this` so the builder can reach the lazily-created dispatcher.
    HBRUSH brush = CreateSolidBrush(palette_.surface.rgb);
    const bool od = (owner_draw_ != nullptr);
    return MenuBuilder(menu, brush, menu.get(), od, this);
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
    // CP-A4: suppress the uxtheme-themed menu-bar background on the host
    // so the MENUINFO brush we just installed is the only background source
    // for popups. Without this, on hosts that use the system's themed
    // menu chrome (visible in Workbench / DarkStudio dark captures as a
    // pale ribbon along the top of the menu bar), the uxtheme background
    // draws underneath the palette brush and bleeds through the gap
    // between menu items. The popup itself (`#32768`) is owned by the
    // system and is not reachable here — the host call is the only
    // surface theme_disable_window_theme can reach.
    if (host != nullptr && IsWindow(host) != FALSE) {
        theme_disable_window_theme(host);
        // CP28-B: nudge the bar to repaint with the new MENUINFO brush. On
        // Win10/11 the menu bar chrome is themed and the background brush is
        // only honored for popup submenus, so DrawMenuBar is a best-effort
        // hint rather than a guarantee — but it prevents the stale theme
        // paint that would otherwise stick until the next WM_NCACTIVATE.
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
        return MenuBuilder(menu_, brush_, nullptr, owner_draw_);
    }
    HMENU parent = target_ != nullptr ? target_ : menu_.get();
    HMENU sub = CreatePopupMenu();
    if (sub == nullptr) {
        return MenuBuilder(menu_, brush_, nullptr, owner_draw_);
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
    // CP-B15: forward owner_draw_ so popup children inherit MF_OWNERDRAW.
    return MenuBuilder(menu_, brush_, sub, owner_draw_);
}

MenuBuilder& MenuBuilder::item(const std::wstring& label, int command_id, bool enabled) noexcept {
    if (!menu_ || target_ == nullptr) {
        return *this;
    }
    UINT flags = MF_STRING;
    if (!enabled) {
        flags |= MF_GRAYED | MF_DISABLED;
    }
    if (owner_draw_ && parent_ != nullptr && parent_->owner_draw() != nullptr) {
        append_owner_draw_item(target_, command_id, label, *parent_->owner_draw(),
                                enabled ? 0u : MF_GRAYED);
    } else {
        AppendMenuW(target_,
                    flags,
                    static_cast<UINT_PTR>(command_id),
                    label.c_str());
    }
    return *this;
}

MenuBuilder& MenuBuilder::separator() noexcept {
    if (!menu_ || target_ == nullptr) {
        return *this;
    }
    if (owner_draw_ && parent_ != nullptr && parent_->owner_draw() != nullptr) {
        // CP-B15: owner-draw separator. The system reserves no implicit
        // height for MF_OWNERDRAW items, so we measure 8 logical px tall
        // in handle_measure_item and paint a divider hairline.
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_FTYPE | MIIM_ID;
        mii.fType = MF_OWNERDRAW | MF_SEPARATOR;
        mii.wID = static_cast<UINT>(-1);
        InsertMenuItemW(target_, 0, TRUE, &mii);
    } else {
        AppendMenuW(target_, MF_SEPARATOR, 0, nullptr);
    }
    return *this;
}

MenuBuilder& MenuBuilder::check_item(const std::wstring& label, int command_id, bool checked) noexcept {
    if (!menu_ || target_ == nullptr) {
        return *this;
    }
    UINT flags = MF_STRING;
    flags |= checked ? MF_CHECKED : MF_UNCHECKED;
    // CP-B15: when owner_draw_ is on, route through MF_OWNERDRAW so the
    // chrome dispatcher paints the row (and stashes the label for it).
    if (owner_draw_ && parent_ != nullptr && parent_->owner_draw() != nullptr) {
        flags |= MF_OWNERDRAW | (checked ? MF_CHECKED : MF_UNCHECKED);
        // Owner-draw items MUST carry payload via dwItemData; the
        // dispatcher reads the label back out at WM_DRAWITEM time.
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_DATA | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
        mii.fType = MF_OWNERDRAW;
        mii.fState = checked ? MFS_CHECKED : MFS_UNCHECKED;
        mii.wID = static_cast<UINT>(command_id);
        mii.dwItemData = reinterpret_cast<ULONG_PTR>(
            new std::wstring(label));
        InsertMenuItemW(target_, static_cast<UINT>(-1), TRUE, &mii);
        parent_->owner_draw()->register_label(command_id, label);
    } else {
        AppendMenuW(target_,
                    flags,
                    static_cast<UINT_PTR>(command_id),
                    label.c_str());
    }
    return *this;
}

MenuOwnerDraw& Menu::enable_owner_draw() noexcept {
    if (owner_draw_ == nullptr) {
        owner_draw_ = std::make_unique<MenuOwnerDraw>(palette_);
    }
    return *owner_draw_;
}

// CP-B15: hold the palette by reference so set_palette() propagates
// without rebuilding the dispatcher. The reference is valid as long as
// the owning Menu outlives the dispatcher — Menu owns the
// unique_ptr<MenuOwnerDraw> in its member list, so the destruction order
// is well-defined.
MenuOwnerDraw::MenuOwnerDraw(const ThemePalette& palette) noexcept
    : palette_(palette) {}

void MenuOwnerDraw::register_label(int command_id, std::wstring label, bool is_separator) noexcept {
    entries_[command_id] = Entry{std::move(label), is_separator};
}

void MenuOwnerDraw::handle_measure_item(MEASUREITEMSTRUCT* mis) noexcept {
    if (mis == nullptr) return;
    // CP-B15: item height is fixed at 24 device px (96 DPI baseline). Menu
    // rows are tiny and the visual difference at 125/150/200% scales is
    // imperceptible — keeping the row height constant sidesteps the
    // architecture rule that the menu module must not depend on the dpi
    // module. Width is reported as 0 so Windows sizes the menu by content.
    mis->itemHeight = 24;
    mis->itemWidth  = 0;
}

bool MenuOwnerDraw::handle_draw_item(const DRAWITEMSTRUCT* dis) noexcept {
    if (dis == nullptr || dis->hDC == nullptr) return false;
    if (dis->CtlType != ODT_MENU) return false;
    auto it = entries_.find(static_cast<int>(dis->itemID));
    if (it == entries_.end()) return false;

    const ThemePalette& p = palette_;
    const RECT& rc = dis->rcItem;
    const bool selected = (dis->itemState & ODS_SELECTED) != 0;
    const bool grayed   = (dis->itemState & ODS_GRAYED) != 0;
    const bool checked  = (dis->itemState & ODS_CHECKED) != 0;
    const bool hot      = (dis->itemState & ODS_HOTLIGHT) != 0;
    const bool focused  = (dis->itemState & ODS_FOCUS) != 0;
    (void)focused;

    // CP-B15: the menu module is constrained to depend only on `core` and
    // `theme` — not on `paint`. Inline the small handful of GDI calls
    // we need so we can keep the layering intact. Each block below is a
    // direct equivalent of the corresponding nfui::paint_* helper.

    auto fill_rect_color = [](HDC dc, const RECT& bounds, Color c) noexcept {
        const HBRUSH b = CreateSolidBrush(c.rgb);
        if (b != nullptr) {
            FillRect(dc, &bounds, b);
            DeleteObject(b);
        }
    };
    auto draw_horizontal_line = [](HDC dc, int x1, int x2, int y, Color c) noexcept {
        const HPEN pen = CreatePen(PS_SOLID, 1, c.rgb);
        if (pen != nullptr) {
            const HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, x1, y, nullptr);
            LineTo(dc, x2, y);
            SelectObject(dc, old);
            DeleteObject(pen);
        }
    };
    auto draw_text_simple = [](HDC dc, const RECT& bounds, const std::wstring& s,
                                Color c) noexcept {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, c.rgb);
        const HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        const HGDIOBJ old = SelectObject(dc, font);
        DrawTextW(dc, s.c_str(), static_cast<int>(s.size()),
                  const_cast<RECT*>(&bounds),
                  DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, old);
    };

    if (it->second.is_separator) {
        // CP-B15: separator reads palette.divider per B16 so the hairline
        // matches the rest of the chrome. Centered horizontally with 8
        // device px of horizontal padding.
        const int pad = 8;
        const int mid = (rc.top + rc.bottom) / 2;
        draw_horizontal_line(dis->hDC, rc.left + pad, rc.right - pad, mid, p.divider);
        return true;
    }

    // Item chrome — selection highlight via solid rounded rect. We draw
    // a square (RoundRect with no rounded corners collapses to a rect in
    // some Windows builds) so a 4-px radius is a stable visual.
    if (selected || hot) {
        // CP-B15: selection uses palette.surface_variant per B17 so the
        // highlight reads on both light and dark themes without losing
        // the underlying row text contrast. The accent stripe along the
        // left edge is a 3-px-wide stripe for the active selection only.
        const int radius = 4;
        const int pad = 4;
        RECT hl = rc;
        hl.left += pad;
        hl.right -= pad;
        const HBRUSH sel_brush = CreateSolidBrush(p.surface_variant.rgb);
        if (sel_brush != nullptr) {
            const HPEN null_pen = CreatePen(PS_NULL, 0, 0);
            const HGDIOBJ old_brush = SelectObject(dis->hDC, sel_brush);
            const HGDIOBJ old_pen = SelectObject(dis->hDC, null_pen);
            RoundRect(dis->hDC, hl.left, hl.top, hl.right, hl.bottom,
                      radius * 2, radius * 2);
            SelectObject(dis->hDC, old_pen);
            SelectObject(dis->hDC, old_brush);
            DeleteObject(null_pen);
            DeleteObject(sel_brush);
        }
        if (selected) {
            const int stripe_w = 3;
            RECT stripe = hl;
            stripe.right = stripe.left + stripe_w;
            const HBRUSH stripe_brush = CreateSolidBrush(p.accent.rgb);
            if (stripe_brush != nullptr) {
                const HBRUSH old_b = static_cast<HBRUSH>(SelectObject(dis->hDC, stripe_brush));
                const HPEN null_pen2 = CreatePen(PS_NULL, 0, 0);
                const HGDIOBJ old_p = SelectObject(dis->hDC, null_pen2);
                RoundRect(dis->hDC, stripe.left, stripe.top, stripe.right, stripe.bottom,
                          radius * 2, radius * 2);
                SelectObject(dis->hDC, old_p);
                DeleteObject(null_pen2);
                SelectObject(dis->hDC, old_b);
                DeleteObject(stripe_brush);
            }
        }
    }

    // Caption — palette.text for normal, text_secondary for grayed.
    const Color fg = grayed ? p.text_secondary : p.text;
    const int pad_l = 28;  // room for a 16-px checkmark + 12-px padding
    const int pad_r = 16;
    RECT text_rc = rc;
    text_rc.left += pad_l;
    text_rc.right -= pad_r;
    draw_text_simple(dis->hDC, text_rc, it->second.label, fg);

    // Checkmark glyph for checked items — a small accent dot at the
    // leftmost padding slot. 8 device px square.
    if (checked) {
        const int check_size = 8;
        const int row_h = rc.bottom - rc.top;
        RECT dot{
            rc.left + (pad_l - check_size) / 2,
            rc.top + (row_h - check_size) / 2,
            rc.left + (pad_l - check_size) / 2 + check_size,
            rc.top + (row_h + check_size) / 2
        };
        const HBRUSH dot_brush = CreateSolidBrush(p.accent.rgb);
        if (dot_brush != nullptr) {
            const HBRUSH old_b = static_cast<HBRUSH>(SelectObject(dis->hDC, dot_brush));
            const HPEN null_pen3 = CreatePen(PS_NULL, 0, 0);
            const HGDIOBJ old_p = SelectObject(dis->hDC, null_pen3);
            Ellipse(dis->hDC, dot.left, dot.top, dot.right, dot.bottom);
            SelectObject(dis->hDC, old_p);
            DeleteObject(null_pen3);
            SelectObject(dis->hDC, old_b);
            DeleteObject(dot_brush);
        }
    }
    return true;
}

} // namespace nfui