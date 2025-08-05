#pragma once

#include <string>
#include <optional>
#include <cstdint>

namespace Game {

    enum class ActionType : uint8_t {
        Fold,
        Call,
        Check,
        Bet
    };

    struct Action {
        ActionType type;
        int amount; // only exist if action is bet/raise
    };

    // from string to action ('f, c, k, b(n)
    std::optional<Action> getAction(const std::string&);

    // action to bits
    // why only 32 bits? well the first 2 will tell us the action type
    // and the remaining 30 will be enough to cover the total stack size of Slumbot which is 20,000 in total
    // well if I eventually get an even stronger opponent, I'll optimize it further for that scenario, but it's OK now
    uint32_t encodeAction(const Action&);

    // decodes bits to readable strings again
    std::string readAction(uint32_t);

}
