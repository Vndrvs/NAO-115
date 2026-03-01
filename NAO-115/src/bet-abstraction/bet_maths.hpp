#pragma once
#include <vector>

/*
 Generic cases to be handled:
 
 1. minimum raise
 previous raise size -> valid list of raise sizings
 example: Villain raises by 100% of pot -> 33% and 75% raise options become invalid
 
 2. raise geometry
 pot size, previous raise size -> amount of chips to be raised
 example: 10BB pot, 5BB villain raise -> Pot after call: 10BB + 5BB + 5BB = 20BB -> 100% raise is 20BB

 + pot fraction bet computation (villain raise: 0)
 pot size -> amount of chips to be bet
 example: 10BB pot -> 50% raise = 5BB; 2x raise = 20BB raise
 
 3. (preflop) re-raise multiplier
 previous raise size, re-raise size -> amount to be re-raised
 example: 10BB villain raise, 2,5x 3bet size -> 25BB re-raise on 3bet
 NOTE: strictly preflop only, post-flop uses raise geometry (case 3)
 
 4. stack limits
 pot size, current stack size -> what amount can Nao use for raising
 example: 10BB pot, 8BB current stack -> only 33% and 75% are possible actions, we can't bet/raise 100% or more
 
 5. all-in detection
 raise size, current stack size -> can Nao call / raise without going all-in
 example: 15BB villain raise, 10BB current stack -> to call, Nao has to go all-in
 
 6. Preflop open
 default preflop bet amounts -> 2x, 3x
 example: valid preflop open bets = 2BB; 3BB
 
 7. remove duplicate action entries
 current stack, computed raise sizes after all-in capping -> list of legal amounts
 example: 15BB stack, 10BB pot -> 150% raise = 15BB, all-in = 15BB - should remove duplicate from list
 
 */

namespace BetAbstraction {
/*
Computes bet/raise amount as a fraction of the pot (valid in flop/turn/river).
initial bets (no villain bet): pass villainBet = 0, heroCurrentBet = 0
raises facing a bet: uses call-first geometry
pot after calling = pot + villainBet + call
raise amount = pot after calling * fraction
total bet = heroCurrentBet + villainBet + raise amount (amount on top of a call)
result capped at currentStack (all-in)
*/
int computePostflopAmount(float fraction, int pot, int villainBet, int heroCurrentBet, int currentStack);

/*
Computes a preflop open size in chips.
multiplier is relative to the big blind (e.g. 2.0 = 2BB, 3.0 = 3BB)
result capped at currentStack
*/
int computePreflopOpen(float multiplier, int bigBlind, int currentStack);

/*
Computes a preflop re-raise amount.
multiplier is relative to the previous raise total (e.g. 2.5x)
result capped at currentStack
*/
int computePreflopReraise(float multiplier, int previousRaiseTotal, int currentStack);

/*
Returns the minimum legal raise total.
in NLHE, a raise must be at least as large as the previous raise increment.
minRaise = previousRaiseTotal + previousRaiseIncrement
where previousRaiseIncrement = previousRaiseTotal - betBeforeRaise
*/
int computeMinRaise(int previousRaiseTotal, int betBeforeRaise);

/*
Returns true if amount >= currentStack (player would be all-in).
*/
bool isAllIn(int amount, int currentStack);

/*
Filters out any amounts below minRaise, then removes duplicates and sorts.
amounts >= currentStack are collapsed to currentStack (all-in)
(all-in is always appended if not already present)
 */
std::vector<int> filterAndDeduplicate(std::vector<int> amounts, int minRaise, int currentStack);

}

