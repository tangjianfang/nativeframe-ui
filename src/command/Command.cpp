#include <nfui/Command.hpp>

#include <utility>

namespace nfui {

void CommandRouter::set_handler(CommandId command, Handler handler) {
    if (handler) {
        handlers_.insert_or_assign(command, std::move(handler));
    } else {
        handlers_.erase(command);
    }
}

void CommandRouter::set_state(CommandId command, CommandState state) {
    states_.insert_or_assign(command, std::move(state));
}

CommandState CommandRouter::state(CommandId command) const {
    if (auto found = states_.find(command); found != states_.end()) {
        return found->second;
    }
    return {};
}

bool CommandRouter::route(CommandId command) const {
    if (!state(command).enabled) {
        return false;
    }

    auto found = handlers_.find(command);
    if (found == handlers_.end()) {
        return false;
    }

    return found->second(command);
}

} // namespace nfui
