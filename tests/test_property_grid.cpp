#include <nfui/Controls/PropertyGrid.hpp>
#include "test_helpers.hpp"

using nfui_test::expect;

namespace {

nfui::PropertyDef make_string(const wchar_t* name, const wchar_t* value) {
    nfui::PropertyDef def;
    def.name = name;
    def.type = nfui::PropertyType::string;
    def.value = value;
    return def;
}

nfui::PropertyDef make_integer(const wchar_t* name, const wchar_t* value) {
    nfui::PropertyDef def;
    def.name = name;
    def.type = nfui::PropertyType::integer;
    def.value = value;
    return def;
}

nfui::PropertyDef make_boolean(const wchar_t* name, const wchar_t* value) {
    nfui::PropertyDef def;
    def.name = name;
    def.type = nfui::PropertyType::boolean;
    def.value = value;
    return def;
}

nfui::PropertyDef make_choice(const wchar_t* name, const wchar_t* value,
                              std::initializer_list<const wchar_t*> choices) {
    nfui::PropertyDef def;
    def.name = name;
    def.type = nfui::PropertyType::choice;
    def.value = value;
    for (const wchar_t* c : choices) {
        def.choices.push_back(c);
    }
    return def;
}

} // namespace

int wmain() {
    bool ok = true;

    // --- parse_integer_property ---
    {
        long long out = 0;
        ok = expect(nfui::parse_integer_property(L"42", out) && out == 42,
                    L"integer: plain digits parse") && ok;
        ok = expect(nfui::parse_integer_property(L"-7", out) && out == -7,
                    L"integer: leading minus parses") && ok;
        ok = expect(!nfui::parse_integer_property(L"", out),
                    L"integer: empty rejects") && ok;
        ok = expect(!nfui::parse_integer_property(L"12px", out),
                    L"integer: trailing garbage rejects") && ok;
        ok = expect(!nfui::parse_integer_property(L"1.5", out),
                    L"integer: decimal point rejects") && ok;
        ok = expect(!nfui::parse_integer_property(L"99999999999999999999999", out),
                    L"integer: overflow rejects") && ok;
        ok = expect(!nfui::parse_integer_property(L" 5", out),
                    L"integer: leading whitespace rejects") && ok;
    }

    // --- validate_property_value per type ---
    {
        ok = expect(nfui::validate_property_value(make_string(L"s", L""), L"anything"),
                    L"validate: string accepts any text") && ok;
        ok = expect(nfui::validate_property_value(make_integer(L"i", L"0"), L"10"),
                    L"validate: integer accepts digits") && ok;
        ok = expect(!nfui::validate_property_value(make_integer(L"i", L"0"), L"ten"),
                    L"validate: integer rejects words") && ok;
        ok = expect(nfui::validate_property_value(make_boolean(L"b", L"true"), L"false"),
                    L"validate: boolean accepts false") && ok;
        ok = expect(!nfui::validate_property_value(make_boolean(L"b", L"true"), L"yes"),
                    L"validate: boolean rejects yes/no") && ok;
        ok = expect(nfui::validate_property_value(
                        make_choice(L"c", L"a", {L"a", L"b"}), L"b"),
                    L"validate: choice accepts member") && ok;
        ok = expect(!nfui::validate_property_value(
                        make_choice(L"c", L"a", {L"a", L"b"}), L"z"),
                    L"validate: choice rejects non-member") && ok;
        ok = expect(!nfui::validate_property_value(
                        make_choice(L"c", L"", {}), L"x"),
                    L"validate: choice with empty list rejects everything") && ok;
    }

    // --- custom per-item validator combines with the type check ---
    {
        nfui::PropertyDef def = make_string(L"name", L"unit");
        def.validate = [](const std::wstring& candidate) { return !candidate.empty(); };
        ok = expect(nfui::validate_property_value(def, L"ok"),
                    L"validate: custom rule accepts") && ok;
        ok = expect(!nfui::validate_property_value(def, L""),
                    L"validate: custom rule rejects") && ok;

        nfui::PropertyDef both = make_integer(L"range", L"5");
        both.validate = [](const std::wstring& candidate) {
            long long parsed = 0;
            return nfui::parse_integer_property(candidate, parsed) && parsed <= 10;
        };
        ok = expect(!nfui::validate_property_value(both, L"50"),
                    L"validate: type AND custom must both accept") && ok;
        ok = expect(!nfui::validate_property_value(both, L"big"),
                    L"validate: type check runs before custom rule") && ok;
    }

    // --- model: pending / dirty / apply / revert round-trip ---
    {
        nfui::PropertyGridModel model;
        const size_t name_index = model.add(make_string(L"Name", L"unit_test"));
        const size_t size_index = model.add(make_integer(L"Size", L"64"));

        ok = expect(model.count() == 2, L"model: add returns sequential count") && ok;
        ok = expect(model.display_value(name_index) == L"unit_test",
                    L"model: display_value starts at committed") && ok;
        ok = expect(!model.dirty(), L"model: fresh model is not dirty") && ok;

        ok = expect(model.set_pending(name_index, L"renamed"),
                    L"model: set_pending accepts valid string") && ok;
        ok = expect(model.display_value(name_index) == L"renamed",
                    L"model: pending wins in display_value") && ok;
        ok = expect(model.committed_value(name_index) == L"unit_test",
                    L"model: committed stays until apply") && ok;
        ok = expect(model.is_dirty(name_index) && model.dirty(),
                    L"model: pending edit marks dirty") && ok;
        ok = expect(model.dirty_indices().size() == 1
                        && model.dirty_indices()[0] == name_index,
                    L"model: dirty_indices reports the edited row") && ok;

        model.revert(name_index);
        ok = expect(model.display_value(name_index) == L"unit_test" && !model.dirty(),
                    L"model: revert discards pending") && ok;

        ok = expect(model.set_pending(size_index, L"128"),
                    L"model: integer edit accepts digits") && ok;
        ok = expect(!model.set_pending(size_index, L"big"),
                    L"model: invalid edit rejects and keeps state") && ok;
        ok = expect(model.display_value(size_index) == L"128",
                    L"model: rejected edit leaves previous pending intact") && ok;

        model.apply_all();
        ok = expect(model.committed_value(size_index) == L"128" && !model.dirty(),
                    L"model: apply_all promotes pending to committed") && ok;
    }

    // --- model: same-value pending is not dirty ---
    {
        nfui::PropertyGridModel model;
        const size_t index = model.add(make_string(L"Same", L"value"));
        ok = expect(model.set_pending(index, L"value"),
                    L"model: same-value pending stores") && ok;
        ok = expect(model.has_pending(index) && !model.is_dirty(index),
                    L"model: pending equal to committed is not dirty") && ok;
    }

    // --- model: read-only rows refuse edits ---
    {
        nfui::PropertyGridModel model;
        nfui::PropertyDef def = make_string(L"Locked", L"fixed");
        def.editable = false;
        const size_t index = model.add(def);
        ok = expect(!model.set_pending(index, L"hacked"),
                    L"model: read-only property rejects set_pending") && ok;
        ok = expect(model.display_value(index) == L"fixed",
                    L"model: read-only value unchanged") && ok;
    }

    // --- model: boolean canonical form ---
    {
        nfui::PropertyGridModel model;
        const size_t index = model.add(make_boolean(L"Visible", L"true"));
        ok = expect(model.set_pending(index, L"false"),
                    L"model: boolean toggle accepts false") && ok;
        ok = expect(!model.set_pending(index, L"TRUE"),
                    L"model: boolean is case-sensitive canonical") && ok;
    }

    return ok ? 0 : 1;
}
