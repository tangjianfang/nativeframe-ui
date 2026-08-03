#pragma once

#include <nfui/Controls/Control.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace nfui {

// CP43: property value types understood by PropertyGrid. Every value travels as
// canonical text: integers as base-10 digits (optional leading minus),
// booleans as "true" / "false", choices as one of PropertyDef::choices.
enum class PropertyType {
    string,
    integer,
    boolean,
    choice,
};

struct PropertyDef {
    std::wstring name;
    PropertyType type{PropertyType::string};
    std::wstring value;                    // canonical text form
    std::vector<std::wstring> choices;     // choice type only
    bool editable{true};
    // Extra per-item rule on top of the type check; both must accept a
    // candidate for it to commit. Null means "type check only".
    std::function<bool(const std::wstring&)> validate;
};

// Pure validation helpers (HWND-free so the model layer is unit-testable).
[[nodiscard]] bool parse_integer_property(const std::wstring& text, long long& out) noexcept;
[[nodiscard]] bool validate_property_value(const PropertyDef& def,
                                           const std::wstring& candidate) noexcept;

// CP43: buffered value store backing PropertyGrid. Each property keeps a
// committed value plus an optional pending edit; `apply_*` promotes pending
// values to committed (the Apply button), `revert_*` discards them (Cancel).
// This is what lets a host dialog offer OK / Cancel / Apply semantics without
// the grid knowing anything about buttons.
class PropertyGridModel {
public:
    size_t add(PropertyDef def);
    void clear() noexcept { slots_.clear(); }
    [[nodiscard]] size_t count() const noexcept { return slots_.size(); }
    [[nodiscard]] const PropertyDef& at(size_t index) const { return slots_.at(index).def; }

    // Pending value wins when present, otherwise the committed value — the
    // text a grid row should display right now.
    [[nodiscard]] const std::wstring& display_value(size_t index) const;
    [[nodiscard]] const std::wstring& committed_value(size_t index) const;
    [[nodiscard]] bool has_pending(size_t index) const noexcept;
    // Dirty == a pending edit exists AND differs from the committed value.
    [[nodiscard]] bool is_dirty(size_t index) const;
    [[nodiscard]] bool dirty() const noexcept;
    [[nodiscard]] std::vector<size_t> dirty_indices() const;

    // Validates before storing; returns false (state unchanged) when the
    // candidate fails validate_property_value.
    bool set_pending(size_t index, const std::wstring& candidate);
    void apply(size_t index) noexcept;
    void apply_all() noexcept;
    void revert(size_t index) noexcept;
    void revert_all() noexcept;

private:
    struct Slot {
        PropertyDef def;
        std::wstring committed;
        std::wstring pending;
        bool has_pending{false};
    };
    std::vector<Slot> slots_;
};

// CP43: self-painted, editable property grid. Two-column name/value rows on
// the injected palette, in-place EDIT for string/integer values, click to
// toggle booleans and cycle choices, keyboard navigation (Up/Down, Space,
// Enter/F2). Edits land in the model's pending layer first; call apply_all()
// / revert_all() from the host's Apply / Cancel affordances. Invalid input
// keeps the editor open with a danger border instead of committing.
class PropertyGrid : public Control {
public:
    [[nodiscard]] bool create(const ControlCreateParams& params) noexcept;

    [[nodiscard]] PropertyGridModel& model() noexcept { return model_; }
    [[nodiscard]] const PropertyGridModel& model() const noexcept { return model_; }

    // Convenience: add + repaint. Returns the new property index.
    size_t add_property(PropertyDef def) noexcept;

    // Programmatic edit through the same validation path as the UI.
    bool set_value(size_t index, const std::wstring& candidate) noexcept;

    // Fires after every accepted edit (pending layer updated) with the
    // property index and its new display value.
    void set_on_value_changed(std::function<void(size_t, const std::wstring&)> callback) noexcept {
        on_value_changed_ = std::move(callback);
    }
    // Fires after apply_all() promotes at least one pending value.
    void set_on_applied(std::function<void()> callback) noexcept {
        on_applied_ = std::move(callback);
    }

    void apply_all() noexcept;
    void revert_all() noexcept;

    [[nodiscard]] int selected_index() const noexcept { return selected_; }
    void select(int index) noexcept;

protected:
    void on_palette_changed() noexcept override;

private:
    void paint(HDC dc, const RECT& bounds) noexcept;
    [[nodiscard]] int row_height() const noexcept;
    [[nodiscard]] int header_height() const noexcept;
    [[nodiscard]] int name_column_width(const RECT& bounds) const noexcept;
    [[nodiscard]] int row_at_point(int y) const noexcept;
    void activate(size_t index) noexcept;        // string/integer -> editor; bool/choice -> instant
    void toggle_or_cycle(size_t index) noexcept;
    void begin_edit(size_t index) noexcept;
    void commit_editor() noexcept;
    void end_edit(bool keep_focus_grid) noexcept;
    void notify_value_changed(size_t index) noexcept;

    static LRESULT CALLBACK grid_subclass_proc(HWND hwnd, UINT message, WPARAM wparam,
                                               LPARAM lparam, UINT_PTR subclass_id,
                                               DWORD_PTR ref_data) noexcept;
    static LRESULT CALLBACK editor_subclass_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                 LPARAM lparam, UINT_PTR subclass_id,
                                                 DWORD_PTR ref_data) noexcept;

    PropertyGridModel model_;
    std::function<void(size_t, const std::wstring&)> on_value_changed_;
    std::function<void()> on_applied_;
    int selected_{-1};
    HWND editor_{nullptr};
    size_t editing_index_{0};
    bool editor_error_{false};
};

} // namespace nfui
