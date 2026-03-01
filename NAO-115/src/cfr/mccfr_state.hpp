#pragma once

#include <cstdint>
#include <algorithm>

/*
 Complete game state for one MCCFR node.
 
speed considerations:
 - fits into single cache line (64 bytes)
 - derived values computed inline through helpers
 
players:
 player 0 - Big Blind (BB): first to act post-flop
 player 1 - Small Blind (SB / BTN): first to act pre-flop
 
streets (matching existing logic):
 0 = Preflop, 1 = Flop, 2 = Turn, 3 = River
 
chips:
 -chip amounts in integer units
 200BB stack: 20000 chips
 BB: 100 chips -> smallBlind = 50; bigBlind = 100
 */
struct MCCFRState {
    /*
    Current street
     0=Preflop, 1=Flop, 2=Turn, 3=River.
     Incremented when both players have equal bets and action is complete
    */
    uint8_t street;
    
    /*
    Raise count
     Shows how many raises have occured on current street.
     Reset to 0 on each new street
     Used to enforce the raise cap (max 4 raises per street, then call/fold only)
    */
    uint8_t raiseCount;
    
    /*
    The player who is currently acting.
     0 = BB, 1 = SB/BTN
     Pre-flop: SB (1) acts first.
     Post-flop: BB (0) acts first.
     Flips after every action except terminal ones.
    */
    uint8_t currentPlayer;
    
    /*
    Whether the current player is facing a bet to be responded to.
     Derived condition: villainStreetBet > heroStreetBet
     When true: legal actions are fold / call / raise
     When false: legal actions are check / bet
    */
    bool facingBet;
    
    /*
    True if the hand is over and no more actions can be taken.
     True when:
     - A player folds
     - A player calls an all-in bet
     - Both players check on the river
     - A bet is called on the river
     - Both players are all-in
    */
    bool isTerminal;
    
    /*
    Index of the player who folded, or -1 if no fold has occurred.
     Used during terminal evaluation to determine the winner
     -1 = no fold (hand went to showdown or is still in progress)
     0 = BB folded
     1 = SB folded
    */
    int8_t foldedPlayer;

    /*
    Chips in the pot from ALL COMPLETED STREETS only.
     Does NOT include heroStreetBet or villainStreetBet.
     Updated at street transition: potBase += heroStreetBet + villainStreetBet.
     Reset of heroStreetBet and villainStreetBet happens at the same time.
     * totalPot() can be used to get the full pot including current street bets.
    */
    int32_t potBase;
    
    /*
    Chips the hero (current player) has committed on the CURRENT STREET only.
     Reset to 0 at the start of each new street
     Used to calculate call amounts and raise geometry
     NOTE: "hero" here always means the player whose perspective we're
     calculating from — this will be currentPlayer in most contexts
    */
    int32_t heroStreetBet;
    
    /*
    Chips the villain (opponent player) has committed on the CURRENT STREET only.
     Reset to 0 at the start of each new street
     When villainStreetBet > heroStreetBet, facingBet = true
     
    */
    int32_t villainStreetBet;
    
    /*
    Remaining stack of the hero (current player).
     Decremented when the hero bets, calls, or raises
     When heroStack = 0, hero is all-in
    */
    int32_t heroStack;
    
    /*
    Remaining stack of the villain (opposing player).
     When villainStack == 0, villain is all-in
    */
    int32_t villainStack;
    /*
    The total amount of the last raise on this street.
     Example: villain raises to 400 total -> previousRaiseTotal = 400
     Used by computeMinRaise() to enforce minimum re-raise rules
     Reset to 0 at the start of each new street
    */
    int32_t previousRaiseTotal;
    
    /*
    The street bet level BEFORE the last raise occurred.
     Example: villain bets 200, hero raises to 600
     betBeforeRaise = 200 (the bet that was raised)
     previousRaiseTotal = 600 (the raise amount)
     increment = 600 - 200 = 400
     minReraise = 600 + 400 = 1000
     Reset to 0 at the start of each new street
    */
    int32_t betBeforeRaise;
    
    /*
    Size of the big blind in chips.
     Used for preflop open sizing (2BB = 2 * bigBlind, 3BB = 3 * bigBlind)
     Constant throughout the hand. With 200BB stacks: bigBlind = 100
    */
    int32_t bigBlind;
    
    /*
    Zobrist hash of the full action history up to this node.
     Updated incrementally via XOR when each action is applied:
     historyHash ^= Zobrist::TABLE[street][raiseCount][actionIndex]
     Combined with bucketId to form the information set key:
     infosetKey = historyHash ^ ((uint64_t)bucketId << 32)
     (never recomputed from scratch — always updated incrementally)
    */
    uint64_t historyHash;
    
    /*
    Card abstraction bucket ID for the current player's hand on this street.
     Range: [0, NUM_BUCKETS) where NUM_BUCKETS = 1000 (our chosen bucket count)
     Computed by the hand abstraction module at the start of each street
     Changes each street as new board cards are revealed
     Combined with historyHash to uniquely identify an information set
    */
    int32_t bucketId;

    // helper methods
    /*
    Total chips in the pot including current street bets from both players.
     This is what bet sizing fractions are applied against
     Example: potBase=500, heroStreetBet=100, villainStreetBet=300 -> totalPot=900
    */
    int32_t totalPot() const { return potBase + heroStreetBet + villainStreetBet; }
    
    /*
    The effective stack — the smaller of the two stacks.
     No bet can exceed this amount since the opponent can't call more than they have.
     All-in = betting exactly currentStack chips.
    */
    int32_t currentStack() const { return std::min(heroStack, villainStack); }
    
    /*
    True if we are in the preflop street.
     Used to switch between preflop (BB-relative) and
     post-flop (pot-fraction) bet sizing logic
     */
    bool isPreflop() const {
        return street == 0;
    }
    /*
    True if the hero is all-in (no chips remaining).
    */
    bool heroAllIn() const {
        return heroStack == 0;
    }

    /*
    True if the villain is all-in (no chips remaining).
    */
    bool villainAllIn() const {
        return villainStack == 0;
    }
    
    /*
    True if either player is all-in.
     When true, no further betting is possible — only call/fold
    */
    bool anyAllIn() const {
        return heroStack == 0 || villainStack == 0;
    }
};
