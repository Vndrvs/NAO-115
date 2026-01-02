#include "state.hpp"
#include <cctype> // using this to access std::isdigit
#include <iostream>
#include <algorithm> // for std::max

namespace Game {

// 'c' call action logic
void handleCall(GameState& state, int& player) {
    // player can't call more than they have
    int opponent = 1 - player;
    int requiredCall = state.players[opponent].currentBet - state.players[player].currentBet;
    
    int callAmount = std::min(state.players[player].stack, requiredCall);
    
    // subtract chips from stack
    state.players[player].stack -= callAmount;
    // update their current bet to match opponent
    state.players[player].currentBet += callAmount;
    // increase pot
    state.pot += callAmount;
}

// 'k' check action logic
void handleCheck(GameState& state, int& player) {
    // no chips move, just advance
    (void)state; // unused param (just for clarity)
    (void)player;
    // nothing to do here besides advancing index in main loop
}

// 'f' fold action logic (we should terminate the game)
void handleFold(GameState& state) {
    state.isTerminal = true;
}

// 'b' bet or raise action logic
// Returns false if invalid raise/bet detected, true otherwise
bool handleBet(GameState& state, int& player, const std::string& actionStr, size_t& i) {
    i++; // move past 'b'
    int amount = 0;
    
    // build the actual bet or raise number based on the read-in digits
    while (i < actionStr.size() && std::isdigit(actionStr[i])) {
        amount = amount * 10 + (actionStr[i] - '0'); // char to int conversion
        i++;
    }
    
    // figure out how much more they’re putting in compared to what they already bet
    int diff = amount - state.players[player].currentBet;
    
    if (diff <= 0) {
        std::cerr << "Invalid raise: raise amount must be greater than current bet.\n";
        state.isTerminal = true;
        return false;
    }
    
    // invalid move — can't bet more than you have
    if (diff > state.players[player].stack) {
        std::cerr << "Invalid bet: player tried to bet more than their stack.\n";
        state.isTerminal = true;
        return false;
    }
    
    // subtract difference from player stack
    state.players[player].stack -= diff;
    // set their bet to the full amount
    state.players[player].currentBet = amount;
    // add difference to the pot
    state.pot += diff;
    
    return true;
}

// if Nao sees '/', it should update the street to the next one (eg. Preflop -> Flop)
// street-level bets should be reset, all bets are fresh if we start a new street
void handleStreetChange(GameState& state) {
    if (state.street != Street::River) {
        state.street = static_cast<Street>(static_cast<int>(state.street) + 1);
    }
    
    state.players[0].currentBet = 0;
    state.players[1].currentBet = 0;
}

// smarter terminal state evaluation
void evaluateTerminal(GameState& state) {
    bool bothPlayersAllIn = (state.players[0].stack == 0 && state.players[1].stack == 0);
    
    // if someone has 0 chips left (went all-in), and action is done, it's terminal
    if (bothPlayersAllIn) {
        state.isTerminal = true;
        return;
    }
    
    // if we're on river and the last action was a call — hand is over
    // terminal detection — last valid actions on river (check-check or call)
    
    if (state.street == Street::River) {
        size_t lastSlash = state.actionHistory.rfind('/');
        if (lastSlash != std::string::npos) {
            std::string riverActions = state.actionHistory.substr(lastSlash + 1);
            
            std::string seq;
            for (char c : riverActions) {
                if (std::isalpha(c)) seq += c;
            }
            
            // terminals:
            // "kk" = Check-Check
            // "cc" = Call-Call (not sure if exists)
            // "bc" = Bet-Call (covers calling all-in on the river)
            if (seq.ends_with("kk") || seq.ends_with("cc") || seq.ends_with("bc")) {
                state.isTerminal = true;
                return;
            }
        }
    }
    
    // if one player is all-in and the other called
    if ((state.players[0].stack == 0 || state.players[1].stack == 0) && state.actionHistory.ends_with('c')) {
        state.isTerminal = true;
        return;
    }
}

void applyAction(GameState& state, const std::string& actionStr) {
    state.actionHistory = actionStr; // store Slumbot-style action string
    
    size_t i = 0;
    int player = state.currentPlayer; // get whether the SB or the BB is acting
    
    while (i < actionStr.size()) {
        char c = actionStr[i];
        
        if (c == '/') {
            handleStreetChange(state);
            i++;
            player = 0;
            continue;
        }
        
        if (c == 'c') {
            handleCall(state, player);
            i++;
        }
        
        else if (c == 'k') {
            handleCheck(state, player);
            i++;
        }
        
        else if (c == 'f') {
            handleFold(state);
            return;
        }
        
        else if (c == 'b') {
            if (!handleBet(state, player, actionStr, i)) {
                return;
            }
        }
        
        else {
            std::cerr << "Unknown action: " << c << std::endl;
            i++;
        }
        
        // switch turns
        player = 1 - player;
    }
    
    // storing which player acts next
    state.currentPlayer = player;
    
    evaluateTerminal(state);
}

}
