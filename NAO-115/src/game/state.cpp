#include "state.hpp"
#include <cctype> // using this to access isdigit()
#include <iostream>

// goal: if we receive an action string like "b200c//kk//kf"
void applyAction(GameState& state, const std::string& actionStr) {
    state.actionHistory = actionStr; // store Slumbot-styl action string
    
    size_t i = 0;
    int player = state.currentPlayer; // get whether the SB or the BB is acting
    
    while (i < actionStr.size()) {
        char c = actionStr[i];
        
        // if Nao sees '/', it should update the street to the next one (eg. Preflop -> Flop)
        if (c == '/') {
            if (state.street < 3) {
                state.street++;
            }
            // street-level bets should be reset, all bets are fresh if we start a new street
            state.players[0].currentBet = 0;
            state.players[1].currentBet = 0;
            i++;
            continue;
        }
        
        // 'c' call action logic
        if (c == 'c') {
            int opponent = 1 - player;
            int requiredCall = state.players[opponent].currentBet - state.players[player].currentBet;
            
            // player can't call more than they have
            int callAmount = std::min(state.players[player].stack, requiredCall);
            
            
            // subtract chips from stack
            state.players[player].stack -= callAmount;
            // update their current bet to match opponent
            state.players[player].currentBet += callAmount;
            // increase pot
            state.pot += callAmount;
            
            i++;
        }
        
        // 'k' check action logic
        else if (c == 'k') {
            i++;
        }
        
        // 'f' fold action logic (we should terminate the game)
        else if (c == 'f') {
            state.isTerminal = true;
            return;
        }
        
        // 'b' bet or raise action logic
        else if (c == 'b') {
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
                return;
            }
            
            // invalid move — can't bet more than you have
            if (diff > state.players[player].stack) {
                std::cerr << "Invalid bet: player tried to bet more than their stack.\n";
                state.isTerminal = true;
                return;
            }
            
            // subtract difference from player stack
            state.players[player].stack -= diff;
            // set their bet to the full amount
            state.players[player].currentBet = amount;
            // add difference to the pot
            state.pot += diff;
        }
        // catching the weird things
        else {
            std::cerr << "Unknown action: " << c << std::endl;
            i++;
        }
        
        // switch turns
        player = 1 - player;
    }
    
    // storing which player acts next
    state.currentPlayer = player;
    
    // smarter terminal state evaluation
    bool bothPlayersAllIn = (state.players[0].stack == 0 && state.players[1].stack == 0);
    
    // if someone has 0 chips left (went all-in), and action is done, it's terminal
    if (bothPlayersAllIn) {
        state.isTerminal = true;
        return;
    }
    
    // if we're on river and the last action was a call — hand is over
    // terminal detection — last valid actions on river (check-check or call)
    if (state.street == 3) {
        size_t lastSlash = state.actionHistory.rfind('/');
        if (lastSlash != std::string::npos) {
            // extract only action letters from the final street (ignore numbers like b200)
            std::string filtered;
            for (size_t j = lastSlash + 1; j < state.actionHistory.size(); ++j) {
                char ch = state.actionHistory[j];
                if (std::isalpha(ch)) {
                    filtered += ch;
                }
            }
            
            // check last 2 letters (could be like "b50ckk" → "ckk" → we grab "kk")
            if (filtered.size() >= 2) {
                std::string lastTwo = filtered.substr(filtered.size() - 2);
                if (lastTwo == "kk" || lastTwo == "cc") {
                    state.isTerminal = true;
                    return;
                }
            }
            
            // bet followed by a call ends river (e.g. "b200c")
            if (filtered.size() >= 2) {
                char last = filtered.back();
                char secondLast = filtered[filtered.size() - 2];
                if ((secondLast == 'b' || secondLast == 'k') && last == 'c') {
                    state.isTerminal = true;
                    return;
                }
            }
        }
    }
    
    // final catch: if someone called an all-in
    for (int i = 0; i < 2; ++i) {
        if (state.players[i].stack == 0) {
            state.isTerminal = true;
            return;
        }
    }
}
