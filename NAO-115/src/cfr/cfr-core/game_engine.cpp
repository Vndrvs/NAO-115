#include "game_engine.hpp"
#include "eval/evaluator.hpp"
#include "mccfr_state.hpp"
#include "bet-abstraction/bet_sequence.hpp"
#include <cassert>
#include <algorithm>
#include <array>

namespace GameEngine {

void switchPlayer(MCCFRState& state) {
    // switch the stacks
    std::swap(state.heroStack, state.villainStack);
    // switch the bet counters for current street
    std::swap(state.heroStreetBet, state.villainStreetBet);
    // invert the current player tracker
    state.currentPlayer = 1 - state.currentPlayer;
}

void transitionStreet(MCCFRState& state) {
    // add current street bets to pot and null them
    state.potBase += state.heroStreetBet + state.villainStreetBet;
    state.heroStreetBet = 0;
    state.villainStreetBet = 0;
    // null raise counters and amount values related to raises
    state.raiseCount = 0;
    state.previousRaiseTotal = 0;
    state.betBeforeRaise = 0;
    // reset check boolean for new street
    state.streetHasCheck = false;
    // increment street number
    state.street++;
    // BB (player 0) is first to act postflop
    // if currentPlayer is not BB, we should switch
    if (state.currentPlayer != 0) {
        switchPlayer(state);
    }
}

MCCFRState applyAction(MCCFRState state, BetAbstraction::AbstractAction action) {
    
    // we need to handle each action type case
    switch (action.type) {
            
        case BetAbstraction::FOLD: {
            state.foldedPlayer = state.currentPlayer;
            state.isTerminal = true;
            return state;
        }
            
        case BetAbstraction::CHECK: { // check
            
            if (state.street == 0 &&
                state.currentPlayer == 0 &&
                state.raiseCount == 0 &&
                state.heroStreetBet == state.villainStreetBet) {
                
                transitionStreet(state);
                return state;
            }
            
            if (state.streetHasCheck) {
                // second check — street over
                if (state.street == 3) {
                    state.isTerminal = true;
                    return state;
                }
                transitionStreet(state);
                return state;
            } else {
                // first check — opponent still to act
                state.streetHasCheck = true;
                switchPlayer(state);
                return state;
            }
        }
            
        case BetAbstraction::CALL: { // call
            int callAmount = state.villainStreetBet - state.heroStreetBet;
            state.heroStack -= callAmount;
            state.heroStreetBet = state.villainStreetBet;
            
            // track contribution
            if (state.currentPlayer == 0) {
                state.player0Contribution += callAmount;
            } else {
                state.player1Contribution += callAmount;
            }
            
            if (state.heroStack == 0 || state.villainStack == 0) {
                state.isTerminal = true;
                return state;
            }
            
            if (state.street == 3) {
                state.isTerminal = true;
                return state;
            }
            
            // before moving on to next street, BB should get a chance to act
            if (state.street == 0 && state.currentPlayer == 1 && state.raiseCount == 0) {
                switchPlayer(state);
                return state;
            }
            
            transitionStreet(state);
            return state;
        }
            
        case BetAbstraction::RAISE: { // bet/raise
            int additional = action.amount - state.heroStreetBet;
            state.heroStack -= additional;
            state.heroStreetBet = action.amount;
            
            // track contribution
            if (state.currentPlayer == 0) {
                state.player0Contribution += additional;
            } else {
                state.player1Contribution += additional;
            }
            
            state.betBeforeRaise = state.villainStreetBet;
            state.previousRaiseTotal = action.amount;
            state.raiseCount++;
            
            if (state.heroStack == 0 && state.villainStack == 0) {
                state.isTerminal = true;
                return state;
            }
            
            switchPlayer(state);
            return state;
        }
    }
    
    return state;
}

// pretty trivial tbh, just read the flag on the state
bool isGamestateTerminal(const MCCFRState& state) {
    return state.isTerminal;
}

// calculate payoff of one game
int getPayoff(const MCCFRState& state,
              const std::array<int,2>& player0hand,
              const std::array<int,2>& player1hand,
              const std::array<int, 5>& board) {
    
    // pot is the sum of what the two players have already contributed
    int pot = state.player0Contribution + state.player1Contribution;
    
    // fold
    if (state.foldedPlayer != -1) {
        if (state.foldedPlayer == 0) {
            return -state.player0Contribution;
        } else {
            return pot - state.player0Contribution;
        }
    }
    
    // showdown, evaluate the winner
    int score0 = Eval::eval_7(
                              player0hand[0], player0hand[1],
                              board[0], board[1], board[2], board[3], board[4]
                              );
    
    int score1 = Eval::eval_7(
                              player1hand[0], player1hand[1],
                              board[0], board[1], board[2], board[3], board[4]
                              );
    
    if (score0 < score1) {
        return pot - state.player0Contribution;
    }
    
    if (score1 < score0) {
        return -state.player0Contribution;
    }
    
    // split pot (odd chip to player 0)
    return (pot / 2 + pot % 2) - state.player0Contribution;
}

}

