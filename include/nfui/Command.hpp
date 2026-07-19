#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace nfui {

struct CommandId {
    int value{};

    friend bool operator==(CommandId left, CommandId right) noexcept {
        return left.value == right.value;
    }
};

struct CommandIdHash {
    std::size_t operator()(CommandId id) const noexcept {
        return std::hash<int>{}(id.value);
    }
};

struct CommandState {
    bool enabled{true};
    bool checked{false};
    std::wstring text{};
};

class CommandRouter {
public:
    using Handler = std::function<bool(CommandId)>;

    void set_handler(CommandId command, Handler handler);
    void set_state(CommandId command, CommandState state);

    [[nodiscard]] CommandState state(CommandId command) const;
    [[nodiscard]] bool route(CommandId command) const;

private:
    std::unordered_map<CommandId, Handler, CommandIdHash> handlers_;
    std::unordered_map<CommandId, CommandState, CommandIdHash> states_;
};

} // namespace nfui
