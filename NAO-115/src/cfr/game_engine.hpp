#pragma once

#include "mccfr_state.hpp"
#include "bet-abstraction/bet_sequence.hpp"

namespace GameEngine {

/*
Apply an action to a state and return the resulting state.
Transitions handled internally:
 - chip counting
 - player switches
 - street transitioning
 - detecting terminal states
 
 Returned state is always from the new current player's perspective.
 (hero/villain fields are swapped after each action)
 
 @param state - current game state (by value — copied, not modified)
 @param action - action to apply
 @param deck - remaining deck for dealing board cards (used on street transitions)
 @return - new state after action is applied
*/

MCCFRState applyAction(MCCFRState state, BetAbstraction::AbstractAction action);

/*
 Returns true if the state is terminal.
 -> no more actions can be taken and Nao is able to compute payoff
 */
bool isGamestateTerminal(const MCCFRState& state);
 
/*
Returns the payoff for the ORIGINAL hero (player 0) at a terminal node.
 Positive = player 0 wins, negative = player 0 loses.
 
 @param state - terminal state
 @param player0hand - player 0's hole cards
 @param player1hand - player 1's hole cards
 @param board - community cards
 */
int getPayoff(const MCCFRState& state,
              const std::array<int,2>& player0hand,
              const std::array<int,2>& player1hand,
              const std::array<int, 5>& board);

/*
Switch the current player — swap hero/villain fields.
-> called internally by applyAction but exposed for testing
 */
void switchPlayer(MCCFRState& state);

/*
Transition to the next street.
 - moves heroStreetBet + villainStreetBet into potBase
 - resets heroStreetBet, villainStreetBet, raiseCount to 0
 - resets previousRaiseTotal, betBeforeRaise to 0
 - increments street
 - sets currentPlayer to 0 (BB is first to act postflop)
 */
void transitionStreet(MCCFRState& state);

}
