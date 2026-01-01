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

struct PlayerState {
        int64_t stack;               // chips left
        int64_t contributed_this_street;  // chips bet this street
        bool folded = false;
        bool all_in = false;
    };

struct GameState {
    Street street;
    PlayerState players[2];
    std::vector<std::vector<Action>> history;  // history per street
    std::vector<std::string> hole_cards;       // always 2 cards for our client
    std::vector<std::string> board;             // 0-5 cards for flop/turn/river
    std::string token;                         // unique ID for game state
    int client_pos;
};


}
