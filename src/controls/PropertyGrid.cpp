#include <nfui/Controls/PropertyGrid.hpp>

#include <nfui/Controls/Detail/effective_palette.hpp>
#include <nfui/Dpi.hpp>
#include <nfui/Font.hpp>
#include <nfui/Paint.hpp>
#include <nfui/Theme.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cwctype>

namespace nfui {
namespace {

constexpr int kRowPadLogical = 8;        // vertical slack added to the font height
constexpr int kCellPadLogical = 8;       // text inset inside a cell
constexpr int kNameColumnPercent = 45;   // name column share of the client width
constexpr int kHeaderCaptionLogical = 0; // header uses the same row height

// The in-place editor is a child of the grid; it needs no dialog id, but a
// distinctive constant keeps WM_COMMAND diagnostics readable.
constexpr int kEditorControlId = 0x7A43;

} // namespace

// ---------------------------------------------------------------------------
// Pure validation helpers
// ---------------------------------------------------------------------------

bool parse_integer_property(const std::wstring& text, long long& out) noexcept {
    if (text.empty()) {
        return false;
    }
    // wcstoll silently skips leading whitespace; the canonical form must
    // round-trip exactly, so any whitespace is rejected up front (trailing
    // whitespace falls out of the end-pointer check below).
    if (std::iswspace(static_cast<wint_t>(text.front())) != 0) {
        return false;
    }
    errno = 0;
    wchar_t* end = nullptr;
    const long long parsed = std::wcstoll(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str()) {
        return false;
    }
    // Trailing garbage ("12px") is rejected; whitespace is not tolerated
    // either so the canonical form round-trips exactly.
    if (end == nullptr || *end != L'\0') {
        return false;
    }
    out = parsed;
    return true;
}

bool validate_property_value(const PropertyDef& def, const std::wstring& candidate) noexcept {
    bool type_ok = true;
    switch (def.type) {
    case PropertyType::string:
        type_ok = true;
        break;
    case PropertyType::integer: {
        long long parsed = 0;
        type_ok = parse_integer_property(candidate, parsed);
        break;
    }
    case PropertyType::boolean:
        type_ok = candidate == L"true" || candidate == L"false";
        break;
    case PropertyType::choice:
        type_ok = !def.choices.empty()
            && std::find(def.choices.begin(), def.choices.end(), candidate) != def.choices.end();
        break;
    }
    if (!type_ok) {
        return false;
    }
    if (def.validate) {
        try {
            return def.validate(candidate);
        } catch (...) {
            // User validators must not break the noexcept subclass boundary;
            // a throwing validator reads as "candidate rejected".
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// PropertyGridModel
// ---------------------------------------------------------------------------

size_t PropertyGridModel::add(PropertyDef def) {
    Slot slot;
    slot.committed = def.value;
    slot.def = std::move(def);
    slots_.push_back(std::move(slot));
    return slots_.size() - 1;
}

const std::wstring& PropertyGridModel::display_value(size_t index) const {
    const Slot& slot = slots_.at(index);
    return slot.has_pending ? slot.pending : slot.committed;
}

const std::wstring& PropertyGridModel::committed_value(size_t index) const {
    return slots_.at(index).committed;
}

bool PropertyGridModel::has_pending(size_t index) const noexcept {
    return index < slots_.size() && slots_[index].has_pending;
}

bool PropertyGridModel::is_dirty(size_t index) const {
    const Slot& slot = slots_.at(index);
    return slot.has_pending && slot.pending != slot.committed;
}

bool PropertyGridModel::dirty() const noexcept {
    for (const Slot& slot : slots_) {
        if (slot.has_pending && slot.pending != slot.committed) {
            return true;
        }
    }
    return false;
}

std::vector<size_t> PropertyGridModel::dirty_indices() const {
    std::vector<size_t> result;
    for (size_t i = 0; i < slots_.size(); ++i) {
        const Slot& slot = slots_[i];
        if (slot.has_pending && slot.pending != slot.committed) {
            result.push_back(i);
        }
    }
    return result;
}

bool PropertyGridModel::set_pending(size_t index, const std::wstring& candidate) {
    if (index >= slots_.size()) {
        return false;
    }
    Slot& slot = slots_[index];
    if (!slot.def.editable) {
        return false;
    }
    if (!validate_property_value(slot.def, candidate)) {
        return false;
    }
    slot.pending = candidate;
    slot.has_pending = true;
    return true;
}

void PropertyGridModel::apply(size_t index) noexcept {
    if (index >= slots_.size()) {
        return;
    }
    Slot& slot = slots_[index];
    if (slot.has_pending) {
        slot.committed = slot.pending;
        slot.has_pending = false;
    }
}

void PropertyGridModel::apply_all() noexcept {
    for (size_t i = 0; i < slots_.size(); ++i) {
        apply(i);
    }
}

void PropertyGridModel::revert(size_t index) noexcept {
    if (index >= slots_.size()) {
        return;
    }
    slots_[index].has_pending = false;
}

void PropertyGridModel::revert_all() noexcept {
    for (Slot& slot : slots_) {
        slot.has_pending = false;
    }
}

// ---------------------------------------------------------------------------
// PropertyGrid control
// ---------------------------------------------------------------------------

bool PropertyGrid::create(const ControlCreateParams& params) noexcept {
    ControlCreateParams grid_params = params;
    // WS_CLIPCHILDREN keeps the in-place editor from smearing over grid rows
    // during repaints; WS_TABSTOP so keyboard navigation participates in the
    // host's focus chain.
    grid_params.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN;
    if (!create_native(L"STATIC", grid_params, 0)) {
        return false;
    }
    if (SetWindowSubclass(hwnd(), &PropertyGrid::grid_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(hwnd());
        return false;
    }
    return true;
}

size_t PropertyGrid::add_property(PropertyDef def) noexcept {
    try {
        const size_t index = model_.add(std::move(def));
        if (selected_ < 0) {
            selected_ = 0;
        }
        if (hwnd() != nullptr) {
            InvalidateRect(hwnd(), nullptr, FALSE);
        }
        return index;
    } catch (...) {
        return model_.count();
    }
}

bool PropertyGrid::set_value(size_t index, const std::wstring& candidate) noexcept {
    if (index >= model_.count()) {
        return false;
    }
    if (editor_ != nullptr && editing_index_ == index) {
        end_edit(false);
    }
    try {
        if (!model_.set_pending(index, candidate)) {
            return false;
        }
    } catch (...) {
        return false;
    }
    notify_value_changed(index);
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
    return true;
}

void PropertyGrid::apply_all() noexcept {
    if (editor_ != nullptr) {
        commit_editor();
    }
    const bool was_dirty = model_.dirty();
    model_.apply_all();
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
    if (was_dirty && on_applied_) {
        try {
            on_applied_();
        } catch (...) {
            // Host callbacks must not cross the Win32 boundary.
        }
    }
}

void PropertyGrid::revert_all() noexcept {
    if (editor_ != nullptr) {
        end_edit(false);
    }
    model_.revert_all();
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void PropertyGrid::select(int index) noexcept {
    if (index < 0 || index >= static_cast<int>(model_.count())) {
        return;
    }
    selected_ = index;
    if (hwnd() != nullptr) {
        InvalidateRect(hwnd(), nullptr, FALSE);
    }
}

void PropertyGrid::on_palette_changed() noexcept {
    RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

void PropertyGrid::notify_value_changed(size_t index) noexcept {
    if (!on_value_changed_) {
        return;
    }
    try {
        on_value_changed_(index, model_.display_value(index));
    } catch (...) {
        // Host callbacks must not cross the Win32 boundary.
    }
}

int PropertyGrid::row_height() const noexcept {
    return font_pixel_height(font_pt::base, dpi_of(hwnd())) + kRowPadLogical;
}

int PropertyGrid::header_height() const noexcept {
    return row_height();
}

int PropertyGrid::name_column_width(const RECT& bounds) const noexcept {
    const int width = bounds.right - bounds.left;
    return width * kNameColumnPercent / 100;
}

int PropertyGrid::row_at_point(int y) const noexcept {
    const int first = header_height();
    if (y < first) {
        return -1;
    }
    const int index = (y - first) / row_height();
    if (index < 0 || index >= static_cast<int>(model_.count())) {
        return -1;
    }
    return index;
}

// CP43: the whole grid is painted from the injected palette — header band,
// name/value rows, selection fill, dirty accents, hairlines, focus ring. No
// native STATIC pixel survives.
void PropertyGrid::paint(HDC dc, const RECT& bounds) noexcept {
    const ThemePalette& p = detail::effective_palette(palette());
    const UINT dpi = dpi_of(hwnd());
    const DpiScale scale(dpi);
    const int cell_pad = scale.logical_to_pixels(kCellPadLogical);
    const int rh = row_height();
    const int hh = header_height();
    const int name_w = name_column_width(bounds);
    const bool focused = (GetFocus() == hwnd());
    const bool enabled = IsWindowEnabled(hwnd()) != FALSE;

    HFONT header_font = fonts() ? fonts()->semibold(dpi, font_pt::sm) : nullptr;
    HFONT name_font = fonts() ? fonts()->regular(dpi, font_pt::sm) : nullptr;
    HFONT value_font = fonts() ? fonts()->regular(dpi, font_pt::base) : nullptr;

    // Body + header band.
    fill_rect(dc, bounds, enabled ? p.surface : p.surface_variant);
    const RECT header{bounds.left, bounds.top, bounds.right, bounds.top + hh};
    fill_rect(dc, header, p.surface_variant);
    RECT header_name{bounds.left + cell_pad, bounds.top,
                     bounds.left + name_w, bounds.top + hh};
    RECT header_value{bounds.left + name_w + cell_pad, bounds.top,
                      bounds.right - cell_pad, bounds.top + hh};
    draw_text(dc, header_name, L"Property", header_font, p.text_secondary,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    draw_text(dc, header_value, L"Value", header_font, p.text_secondary,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Rows.
    int y = bounds.top + hh;
    for (size_t i = 0; i < model_.count(); ++i) {
        const int row_bottom = y + rh;
        if (y >= bounds.bottom) {
            break; // rows below the fold are clipped (scrolling is post-V1)
        }
        const RECT row{bounds.left, y, bounds.right, row_bottom};
        const bool selected = static_cast<int>(i) == selected_;
        if (selected) {
            fill_rect(dc, row, p.selection);
        }
        // Editing hides the value text under the in-place editor; the name
        // column still paints normally.
        const bool editing_here = editor_ != nullptr && editing_index_ == i;

        const PropertyDef& def = model_.at(i);
        RECT name_rect{bounds.left + cell_pad, y,
                       bounds.left + name_w - cell_pad / 2, row_bottom};
        const Color name_color = selected ? p.selection_text : p.text_secondary;
        draw_text(dc, name_rect, def.name, name_font, name_color,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

        if (!editing_here) {
            RECT value_rect{bounds.left + name_w + cell_pad, y,
                            bounds.right - cell_pad, row_bottom};
            const bool dirty_row = model_.is_dirty(i);
            Color value_color;
            if (selected) {
                value_color = p.selection_text;
            } else if (!def.editable) {
                value_color = p.text_secondary;
            } else if (dirty_row) {
                value_color = p.accent; // pending edits read as "unapplied"
            } else {
                value_color = p.text;
            }
            draw_text(dc, value_rect, model_.display_value(i), value_font, value_color,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        }

        // Row hairline.
        const RECT divider{bounds.left, row_bottom - 1, bounds.right, row_bottom};
        fill_rect(dc, divider, p.divider);
        y = row_bottom;
    }

    // Column divider + header divider + outer border.
    const RECT column{bounds.left + name_w, bounds.top + hh,
                      bounds.left + name_w + 1, bounds.bottom};
    fill_rect(dc, column, p.divider);
    const RECT under_header{bounds.left, bounds.top + hh - 1,
                            bounds.right, bounds.top + hh};
    fill_rect(dc, under_header, p.border);
    paint_focus_border(dc, bounds, p.border, 1);

    // Focused grid gets a 1px accent ring inside the border so keyboard
    // navigation is visible without stealing the selection colour.
    if (focused) {
        RECT inner{bounds.left + 1, bounds.top + 1, bounds.right - 1, bounds.bottom - 1};
        paint_focus_border(dc, inner, p.accent, 1);
    }
}

void PropertyGrid::activate(size_t index) noexcept {
    if (index >= model_.count()) {
        return;
    }
    if (!model_.at(index).editable) {
        return;
    }
    const PropertyType type = model_.at(index).type;
    if (type == PropertyType::boolean || type == PropertyType::choice) {
        toggle_or_cycle(index);
        return;
    }
    begin_edit(index);
}

void PropertyGrid::toggle_or_cycle(size_t index) noexcept {
    if (index >= model_.count()) {
        return;
    }
    const PropertyDef& def = model_.at(index);
    std::wstring candidate;
    try {
        if (def.type == PropertyType::boolean) {
            candidate = model_.display_value(index) == L"true" ? L"false" : L"true";
        } else if (def.type == PropertyType::choice && !def.choices.empty()) {
            const std::wstring& current = model_.display_value(index);
            auto it = std::find(def.choices.begin(), def.choices.end(), current);
            const size_t next = (it == def.choices.end())
                ? 0
                : (static_cast<size_t>(it - def.choices.begin()) + 1) % def.choices.size();
            candidate = def.choices[next];
        } else {
            return;
        }
        if (!model_.set_pending(index, candidate)) {
            return;
        }
    } catch (...) {
        return;
    }
    notify_value_changed(index);
    InvalidateRect(hwnd(), nullptr, FALSE);
}

void PropertyGrid::begin_edit(size_t index) noexcept {
    if (index >= model_.count() || hwnd() == nullptr) {
        return;
    }
    if (editor_ != nullptr) {
        commit_editor(); // flush the previous editor before opening a new one
        if (editor_ != nullptr) {
            return;      // previous editor refused to commit (invalid input)
        }
    }

    RECT client{};
    GetClientRect(hwnd(), &client);
    const int rh = row_height();
    const int hh = header_height();
    const int name_w = name_column_width(client);
    const int top = client.top + hh + static_cast<int>(index) * rh;
    const RECT cell{client.left + name_w, top, client.right, top + rh};

    std::wstring text;
    try {
        text = model_.display_value(index);
    } catch (...) {
        return;
    }

    HWND editor = CreateWindowExW(0, L"EDIT", text.c_str(),
                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                  cell.left + 1, cell.top + 1,
                                  std::max(1, static_cast<int>(cell.right - cell.left - 2)),
                                  std::max(1, static_cast<int>(cell.bottom - cell.top - 2)),
                                  hwnd(),
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditorControlId)),
                                  reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd(), GWLP_HINSTANCE)),
                                  nullptr);
    if (editor == nullptr) {
        return;
    }
    if (SetWindowSubclass(editor, &PropertyGrid::editor_subclass_proc,
                          reinterpret_cast<UINT_PTR>(this),
                          reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        DestroyWindow(editor);
        return;
    }
    editor_ = editor;
    editing_index_ = index;
    editor_error_ = false;
    if (fonts() != nullptr) {
        SendMessageW(editor, WM_SETFONT,
                     reinterpret_cast<WPARAM>(fonts()->regular(dpi_of(hwnd()), font_pt::base)),
                     TRUE);
    }
    SetFocus(editor);
    SendMessageW(editor, EM_SETSEL, 0, -1);
    InvalidateRect(hwnd(), nullptr, FALSE);
}

void PropertyGrid::commit_editor() noexcept {
    if (editor_ == nullptr) {
        return;
    }
    const int length = GetWindowTextLengthW(editor_);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(editor_, text.data(), length + 1);
    text.resize(static_cast<size_t>(std::max(0, copied)));

    // Unchanged text commits as a no-op — closing the editor without
    // dirtying the pending layer.
    bool unchanged = false;
    try {
        unchanged = text == model_.display_value(editing_index_);
    } catch (...) {
        unchanged = false;
    }
    if (unchanged) {
        end_edit(true);
        return;
    }

    bool accepted = false;
    try {
        accepted = model_.set_pending(editing_index_, text);
    } catch (...) {
        accepted = false;
    }
    if (!accepted) {
        // Keep the editor open and mark it invalid; the editor's WM_PAINT
        // arm draws the danger border from editor_error_.
        editor_error_ = true;
        InvalidateRect(editor_, nullptr, FALSE);
        SetFocus(editor_);
        return;
    }
    const size_t changed = editing_index_;
    end_edit(true);
    notify_value_changed(changed);
}

void PropertyGrid::end_edit(bool keep_focus_grid) noexcept {
    if (editor_ == nullptr) {
        return;
    }
    HWND editor = editor_;
    editor_ = nullptr; // cleared first: DestroyWindow re-enters WM_NCDESTROY
    DestroyWindow(editor);
    if (keep_focus_grid && hwnd() != nullptr) {
        SetFocus(hwnd());
    }
    InvalidateRect(hwnd(), nullptr, FALSE);
}

LRESULT CALLBACK PropertyGrid::editor_subclass_proc(HWND hwnd,
                                                    UINT message,
                                                    WPARAM wparam,
                                                    LPARAM lparam,
                                                    UINT_PTR subclass_id,
                                                    DWORD_PTR ref_data) noexcept {
    auto* grid = reinterpret_cast<PropertyGrid*>(ref_data);
    if (grid == nullptr) {
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }

    switch (message) {
    case WM_KEYDOWN: {
        if (wparam == VK_RETURN) {
            grid->commit_editor();
            return 0;
        }
        if (wparam == VK_ESCAPE) {
            grid->end_edit(true);
            return 0;
        }
        break;
    }
    case WM_CHAR: {
        // Enter is consumed via WM_KEYDOWN; keep the default char handling.
        if (wparam == VK_RETURN) {
            return 0;
        }
        break;
    }
    case WM_PAINT: {
        const LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        // Themed frame around the native edit client: danger when the last
        // commit failed validation, accent otherwise. GetWindowDC reaches the
        // 1-px frame the borderless EDIT leaves unpainted.
        const ThemePalette& p = detail::effective_palette(grid->palette());
        RECT window_bounds{};
        GetWindowRect(hwnd, &window_bounds);
        OffsetRect(&window_bounds, -window_bounds.left, -window_bounds.top);
        HDC window_dc = GetWindowDC(hwnd);
        if (window_dc != nullptr) {
            paint_focus_border(window_dc, window_bounds,
                               grid->editor_error_ ? p.danger : p.accent, 1);
            ReleaseDC(hwnd, window_dc);
        }
        return result;
    }
    case WM_KILLFOCUS: {
        // Only steal the commit path when focus leaves the editor AND the
        // grid is not shutting the editor down itself (commit_editor /
        // end_edit null the pointer before DestroyWindow).
        if (grid->editor_ == hwnd) {
            if (grid->editor_error_) {
                grid->end_edit(false);
            } else {
                grid->commit_editor();
            }
        }
        break;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &PropertyGrid::editor_subclass_proc, subclass_id);
        if (grid->editor_ == hwnd) {
            grid->editor_ = nullptr;
        }
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }

    return DefSubclassProc(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK PropertyGrid::grid_subclass_proc(HWND hwnd,
                                                  UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam,
                                                  UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data) noexcept {
    auto* grid = reinterpret_cast<PropertyGrid*>(ref_data);
    if (grid == nullptr) {
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        if (dc != nullptr) {
            RECT bounds{};
            GetClientRect(hwnd, &bounds);
            grid->paint(dc, bounds);
            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        // The borderless in-place EDIT asks its parent (the grid) for
        // colours; serve the palette so the editor client matches the row.
        HDC dc = reinterpret_cast<HDC>(wparam);
        const ThemePalette& p = detail::effective_palette(grid->palette());
        SetTextColor(dc, p.text.rgb);
        SetBkColor(dc, p.surface.rgb);
        SetBkMode(dc, OPAQUE);
        SetDCBrushColor(dc, p.surface.rgb);
        return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
    }
    case WM_LBUTTONDOWN: {
        SetFocus(hwnd);
        const int y = static_cast<short>(HIWORD(lparam));
        const int x = static_cast<short>(LOWORD(lparam));
        const int row = grid->row_at_point(y);
        if (row >= 0) {
            RECT client{};
            GetClientRect(hwnd, &client);
            const bool on_value = x > grid->name_column_width(client);
            if (row == grid->selected_ && on_value) {
                grid->activate(static_cast<size_t>(row));
            } else {
                grid->select(row);
            }
        } else if (grid->editor_ != nullptr) {
            grid->commit_editor(); // click outside the rows flushes the editor
        }
        return 0;
    }
    case WM_KEYDOWN: {
        const int count = static_cast<int>(grid->model_.count());
        if (count == 0) {
            break;
        }
        switch (static_cast<int>(wparam)) {
        case VK_DOWN:
            grid->select(std::min(grid->selected_ + 1, count - 1));
            return 0;
        case VK_UP:
            grid->select(std::max(grid->selected_ - 1, 0));
            return 0;
        case VK_SPACE:
        case VK_RETURN:
        case VK_F2:
            if (grid->selected_ >= 0) {
                grid->activate(static_cast<size_t>(grid->selected_));
            }
            return 0;
        default:
            break;
        }
        break;
    }
    case WM_GETDLGCODE:
        // Arrow keys must reach the grid even when hosted inside a dialog.
        return DLGC_WANTARROWS;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE: {
        const LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return result;
    }
    case WM_SIZE: {
        const LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        // Keep an open editor glued to its cell across host relayouts.
        if (grid->editor_ != nullptr) {
            RECT client{};
            GetClientRect(hwnd, &client);
            const int rh = grid->row_height();
            const int hh = grid->header_height();
            const int name_w = grid->name_column_width(client);
            const int top = client.top + hh + static_cast<int>(grid->editing_index_) * rh;
            MoveWindow(grid->editor_, client.left + name_w + 1, top + 1,
                       std::max(1, static_cast<int>(client.right - client.left - name_w - 2)),
                       std::max(1, rh - 2), TRUE);
        }
        return result;
    }
    case WM_NCDESTROY:
        grid->editor_ = nullptr; // child dies with the parent; drop the pointer
        RemoveWindowSubclass(hwnd, &PropertyGrid::grid_subclass_proc, subclass_id);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    default:
        break;
    }

    return DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace nfui
