#include "action.hpp"

namespace Game {
    // this function gets the action character as a parameter and assigns an enum 'ActionType' for the corresponding letter
    std::optional<Action> getAction(const std::string& s) {
        if (s.empty()) return std::nullopt;
        
        // we always check the first character of the string, this might seem useless at first sight
        // because fold, call and check only come as one char, but bet / raise action always comes with bet sizing
        // so in those cases we need to tell Nao that the action is always the first character of the input string
        char c = s[0];
        
        // characters exactly match the API response received from Slumbot, quite straightforward
        // in the struct 'ActionType' created in the header we have type (enummed beforehand) and amount (int)
        // but we only use the amount variable if the action is a bet or a raise, otherwise it's always 0
        if (c == 'f') return Action{ActionType::Fold, 0};
        if (c == 'c') return Action{ActionType::Call, 0};
        // note: as 'c' is already taken for 'call', Slumbot uses 'k' for check, our enum is also set this way
        if (c == 'k') return Action{ActionType::Check, 0};
        // 'b' can mean bet or raise depending on the context,
        if (c == 'b') {
            try {
                // we get the substring after the first character which defines the amount on bet / raise
                // with stoi we convert it to an int, then we return the corresponding action
                int amount = std::stoi(s.substr(1));
                return Action{ActionType::Bet, amount};
            // catch ... means if we catch anything
            } catch (...) {
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    uint32_t encodeAction(const Action& a) {
        // first 2 bits action type, following 30 bits bet/raise amount
        // type refers back to the enum created in hpp (0:fold, 1:call, 2:check, 3:bet)
        
        uint32_t bits = static_cast<uint32_t>(a.type) & 0b11;

        if (a.type == ActionType::Bet) {
            uint32_t amt = static_cast<uint32_t>(a.amount);
            bits |= (amt << 2); // bitshift the amount to start from 3rd bit
        }

        return bits;
    }

    Action readAction(uint32_t bits) {
        Action a;
        // Unmask the first 2 bits
        a.type = static_cast<ActionType>(bits & 0b11);
        // Unshift to get the amount
        a.amount = (bits >> 2);
        return a;
    }

    // thought about creating a char based constexpr for the actions, but switch seems faster, will benchmark later to decide.
    std::string actionToString(const Action& a) {
        switch (a.type) {
            case ActionType::Fold: return "f";
            case ActionType::Call: return "c";
            case ActionType::Check: return "k";
            case ActionType::Bet:
                return "b" + std::to_string(a.amount);
            default: return "?";
        }
    }
}
