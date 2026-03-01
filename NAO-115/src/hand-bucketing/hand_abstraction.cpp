/*
Effective Hand Strength algorithm derived from research paper: Opponent Modeling in Poker (1998)
by computer scientists Darse Billings, Denis Papp, Jonathan Schaeffer and Duane Szafron
*/

#include "eval/evaluator.hpp"
#include "hand_abstraction.hpp"
#include <iostream>
#include <iomanip>

namespace Eval {
/*
context structure for flop
contains: 2 self pocket cards, 3 board cards, rank of hero
 */
struct FlopContext {
    uint64_t deckMask;
    int h0, h1;
    int b0, b1, b2;
    int selfRank;
};

/*
context structure for turn
contains: 2 self pocket cards, 4 board cards, rank of hero
 */
struct TurnContext {
    uint64_t deckMask;
    int h0, h1;
    int b0, b1, b2, b3;
    int selfRank;
};

/*
context structure for river
contains: 2 self pocket cards, 5 board cards, rank of hero
 */
struct RiverContext {
    uint64_t deckMask;
    int h0, h1;
    int b0, b1, b2, b3, b4;
    int selfRank;
};

// enum for 3 possible states in EHS calculation
enum HandState : int {
    AHEAD  = 0,
    TIED   = 1,
    BEHIND = 2
};

// helper to build the bitmask neccessary for flop and turn computations
inline uint64_t buildDeckMask(const std::initializer_list<int>& usedCards)
{
    uint64_t used = 0;
    for (int c : usedCards) {
        used |= 1ULL << c;
    }

    return (~used) & ((1ULL << 52) - 1);
}

//

/*
helper to create flop environment
returns:
 - mask of available cards
 - 2 encoded self pocket cards, 3 encoded board cards
 - self rank
 */
inline FlopContext createFlopContext(
    const std::array<int,2>& hand,
    const std::array<int,3>& board)
{
    // identify cards using our encoded deck
    int h0 = deck[hand[0]];
    int h1 = deck[hand[1]];
    int b0 = deck[board[0]];
    int b1 = deck[board[1]];
    int b2 = deck[board[2]];
    
    // build mask
    uint64_t deckMask = buildDeckMask({ hand[0], hand[1], board[0], board[1], board[2] });
    
    // use Kev eval to calculate hand rank
    int selfRank = eval_5(h0, h1, b0, b1, b2);

    return {
        deckMask,
        h0, h1, b0, b1, b2,
        selfRank
    };
}

/*
helper to create turn environment
returns: 
- mask of available cards
- 2 encoded self pocket cards, 4 encoded board cards
- self rank
 */
inline TurnContext createTurnContext(
    const std::array<int,2>& hand,
    const std::array<int,4>& board)
{
    uint64_t deckMask = buildDeckMask({ hand[0], hand[1], board[0], board[1], board[2], board[3] });
    
    int h0 = deck[hand[0]];
    int h1 = deck[hand[1]];
    int b0 = deck[board[0]];
    int b1 = deck[board[1]];
    int b2 = deck[board[2]];
    int b3 = deck[board[3]];

    int selfRank = eval_6(h0, h1, b0, b1, b2, b3);
    
    return {
        deckMask,
        h0, h1, b0, b1, b2, b3,
        selfRank
    };
}

/*
helper to create river environment
returns:
- mask of available cards
- 2 encoded self pocket cards, 5 encoded board cards
- self rank
 */

inline RiverContext createRiverContext(
    const std::array<int,2>& hand,
    const std::array<int,5>& board)
{
    uint64_t deckMask = buildDeckMask({ hand[0], hand[1], board[0], board[1], board[2], board[3], board[4] });
    
    int h0 = deck[hand[0]];
    int h1 = deck[hand[1]];
    int b0 = deck[board[0]];
    int b1 = deck[board[1]];
    int b2 = deck[board[2]];
    int b3 = deck[board[3]];
    int b4 = deck[board[4]];

    int selfRank = eval_7(h0, h1, b0, b1, b2, b3, b4);
    
    return {
        deckMask,
        h0, h1, b0, b1, b2, b3, b4,
        selfRank
    };
}

inline void precomputeHero7(
    uint64_t availableDeckMask,
    int heroCard0,
    int heroCard1,
    int boardCard0,
    int boardCard1,
    int boardCard2,
    int heroSevenCardRank[52][52])   // symmetric lookup table [turn][river]
{
    // iterate over all unordered (turn, river) card pairs
    uint64_t firstCardMask = availableDeckMask;
    while (firstCardMask) {
        int firstCardIndex = __builtin_ctzll(firstCardMask);
        firstCardMask &= (firstCardMask - 1);

        uint64_t secondCardMask = firstCardMask;
        while (secondCardMask) {
            int secondCardIndex = __builtin_ctzll(secondCardMask);
            secondCardMask &= (secondCardMask - 1);

            // evaluate hero's best 7-card hand for this (turn, river) pair
            int heroRank = eval_7(
                heroCard0,
                heroCard1,
                boardCard0,
                boardCard1,
                boardCard2,
                deck[firstCardIndex],
                deck[secondCardIndex]
            );

            // store symmetrically: turn, river = river, turn
            heroSevenCardRank[firstCardIndex][secondCardIndex] = heroRank;
            heroSevenCardRank[secondCardIndex][firstCardIndex] = heroRank;
        }
    }
}

// compute asymmetry of Ppot and Npot variables
inline float computeAsymmetry(float handStrength, float Ppot, float Npot) {
    float upside   = (1.0f - handStrength) * Ppot;
    float downside = handStrength * Npot;

    return (upside - downside) / (upside + downside + 1e-6f);
}

// compute effective hand strength
inline float computeEHS(float handStrength, float Ppot, float Npot) {
    float winNow = handStrength;
    float improve = (1.f - handStrength) * Ppot;
    float deteriorate = handStrength * Npot;

    return winNow + improve - deteriorate;
}

// function that calculates feature set for flop
FlopFeatures calculateFlopFeaturesTwoAhead(const std::array<int, 2>& hand, const std::array<int, 3>& board) {
    
    // call environment initializer helper
    auto context = createFlopContext(hand, board);
    
    // pre-compute all hero ranks through (turn, river) card pairs
    int heroEval[52][52] = {};
    precomputeHero7(
        context.deckMask,
        context.h0, context.h1,
        context.b0, context.b1, context.b2,
        heroEval
    );
    
    // EHS / potential computation value set
    float betterThan = 0.f;
    float worseThan  = 0.f;
    float equal      = 0.f;
    
    // hand potential helper arrays for EHS calculation
    float HP[3][3]              = {};
    float handPotentialTotal[3] = {};
    
    // per-runout counters for volatility
    int hsWin[52][52]         = {};
    int hsTie[52][52]         = {};
    int hsTotal[52][52]       = {};

    // encoded rank of 'three of a kind' for feature computations
    constexpr int TRIPS_THRESHOLD = 4995;

    // create mask for computing villain card 1
    uint64_t villainMask = context.deckMask;
    while (villainMask) {
        int villainIndex1 = __builtin_ctzll(villainMask);
        int villainCard1 = deck[villainIndex1]; // found first villain card
        villainMask &= (villainMask - 1); // remove first villain card from mask
        
        // create mask for computing villain card 2
        uint64_t villainMask2 = villainMask;
        while (villainMask2) {
            int villainIndex2 = __builtin_ctzll(villainMask2);
            int villainCard2 = deck[villainIndex2]; // found second villain card
            villainMask2 &= (villainMask2 - 1); // remove second villain card from mask
            
            int villainScore = eval_5(context.b0, context.b1, context.b2, villainCard1, villainCard2); // evaluate villain on flop
            
            // starting score comparison
            int flopState;
            if (context.selfRank < villainScore) {
                worseThan += 1.f;
                flopState = BEHIND;
            } else if (context.selfRank > villainScore) {
                betterThan += 1.f;
                flopState = AHEAD;
            } else {
                equal += 1.f;
                flopState = TIED;
            }
            
            handPotentialTotal[flopState] += 1.f;
            
            // create mask with available cards for turn draw
            uint64_t turnMask = context.deckMask;
            turnMask &= ~(1ULL << villainIndex1);
            turnMask &= ~(1ULL << villainIndex2); // remove already used villain cards
            
            while (turnMask) {
                int turnCardIndex = __builtin_ctzll(turnMask);
                int turnCard = deck[turnCardIndex]; // found turn card
                turnMask &= (turnMask - 1); // remove turn card from mask
                
                // create mask with available cards for river draw
                uint64_t riverMask = turnMask;
                while (riverMask) {
                    int riverCardIndex = __builtin_ctzll(riverMask);
                    int riverCard = deck[riverCardIndex]; // found river card
                    riverMask &= (riverMask - 1); // remove river card from mask
                    
                    // get relevant hero rank from pre-computed array
                    int selfBest = heroEval[turnCardIndex][riverCardIndex];
                    int villainBest = eval_7(villainCard1, villainCard2,
                                            context.b0, context.b1, context.b2,
                                            turnCard, riverCard); // evaluate villain on river
                    
                    // final score comparison
                    int finalState;
                    if (selfBest < villainBest) {
                        finalState = BEHIND;
                    }
                    else if (selfBest > villainBest) {
                        hsWin[turnCardIndex][riverCardIndex]++;
                        finalState = AHEAD;
                    }
                    else {
                        hsTie[turnCardIndex][riverCardIndex]++;
                        finalState = TIED;
                    }
                    
                    HP[flopState][finalState] += 1.f;
                    hsTotal[turnCardIndex][riverCardIndex]++;
                }
            }
        }
    }
    
    // calculate simple HS
    float handStrength = (betterThan + 0.5f * equal) / (betterThan + worseThan + equal);
    
    float PpotDenominator = handPotentialTotal[BEHIND] + handPotentialTotal[TIED];
    float NpotDenominator = handPotentialTotal[AHEAD]  + handPotentialTotal[TIED];
    
    constexpr float remainingCombos = 990.f;
    float Ppot = 0.f;
    float Npot = 0.f;
    
    if (PpotDenominator > 0.f) {
        Ppot = (HP[BEHIND][AHEAD] +
                HP[BEHIND][TIED] * 0.5f +
                HP[TIED][AHEAD]  * 0.5f) /
               (PpotDenominator * remainingCombos);
    }
    
    if (NpotDenominator > 0.f) {
        Npot = (HP[AHEAD][BEHIND] +
                HP[AHEAD][TIED]   * 0.5f +
                HP[TIED][BEHIND]  * 0.5f) /
               (NpotDenominator * remainingCombos);
    }
    
    /*
    F1: EHS (Effective Hand Strength) — hand strength adjusted for drawing potential
    combines raw flop equity with Ppot (chance of improving from behind)
    and Npot (chance of being outdrawn from ahead) across all turn+river runouts
     */
    float EHS = computeEHS(handStrength, Ppot, Npot);
    
    
    /*
    F2: asymmetry — signed measure of draw character: Ppot - Npot
    positive = hero has more draw potential than villain (drawing hand)
    negative = hero is more likely to be outdrawn than to improve (made hand under threat)
    near zero = balanced / stable hand
    */
    float asymmetry = computeAsymmetry(handStrength, Ppot, Npot);
    
    /*
    F3: nut potential — fraction of (turn, river) runouts where hero makes trips or better
    high = hero has strong improvement ceiling (set draws, two pair with full house outs)
    low = hero's best possible river hand is one pair or two pair
    */
    int nutHits  = 0;
    int nutTotal = 0;
    for (int t = 0; t < 52; ++t) {
        for (int r = 0; r < 52; ++r) {
            if (hsTotal[t][r] > 0) {
                nutTotal++;
                if (heroEval[t][r] > TRIPS_THRESHOLD) {
                    nutHits++;
                }
            }
        }
    }

    float nutPotential = 0.f;
    if (nutTotal > 0) {
        nutPotential = float(nutHits) / nutTotal;
    }
    
    return FlopFeatures{ EHS, asymmetry, nutPotential };
}

// function to calculate feature set for turn
TurnFeatures calculateTurnFeatures(const std::array<int, 2>& hand, const std::array<int, 4>& board) {

    // call environment initializer helper
    auto context = createTurnContext(hand, board);

    // pre-compute all hero ranks through possible river cards
    int heroRiverEvals[52] = {};
    uint64_t preRiverMask = context.deckMask;

    while (preRiverMask) {
        int cardIndex = __builtin_ctzll(preRiverMask);
        preRiverMask &= (preRiverMask - 1);
        heroRiverEvals[cardIndex] = eval_7(
            context.h0, context.h1,
            context.b0, context.b1, context.b2, context.b3,
            deck[cardIndex]
        );
    }

    // EHS / potential computation value set
    float betterThan = 0.f;
    float worseThan  = 0.f;
    float equal      = 0.f;

    // hand potential helper arrays for EHS calculation
    float HP[3][3]              = {};
    float handPotentialTotal[3] = {};

    // per-runout counters
    int hsWin[52]   = {};
    int hsTie[52]   = {};
    int hsTotal[52] = {};

    // encoded rank thresholds for feature computations
    constexpr int TRIPS_THRESHOLD = 4995;
    
    float nutWinSum   = 0.f;
    float nutWinTotal = 0.f;

    // create mask for computing villain card 1
    uint64_t villainMask = context.deckMask;
    while (villainMask) {
        int villainIndex1 = __builtin_ctzll(villainMask);
        int villainCard1 = deck[villainIndex1]; // found first villain card
        villainMask &= (villainMask - 1); // remove first villain card from mask

        // create mask for computing villain card 2
        uint64_t villainMask2 = villainMask;
        while (villainMask2) {
            int villainIndex2 = __builtin_ctzll(villainMask2);
            int villainCard2 = deck[villainIndex2]; // found second villain card
            villainMask2 &= (villainMask2 - 1); // remove second villain card from mask

            int villainScore = eval_6(
                context.b0, context.b1, context.b2, context.b3,
                villainCard1, villainCard2
            ); // evaluate villain on turn

            // starting score comparison
            int turnState;
            if (context.selfRank < villainScore) {
                worseThan += 1.f;
                turnState = BEHIND;
            } else if (context.selfRank > villainScore) {
                betterThan += 1.f;
                turnState = AHEAD;
            } else {
                equal += 1.f;
                turnState = TIED;
            }

            handPotentialTotal[turnState] += 1.f;

            // create mask with available cards for river draw
            uint64_t riverMask = context.deckMask;
            riverMask &= ~(1ULL << villainIndex1);
            riverMask &= ~(1ULL << villainIndex2); // remove already used villain cards

            while (riverMask) {
                int riverCardIndex = __builtin_ctzll(riverMask);
                int riverCard = deck[riverCardIndex]; // found river card
                riverMask &= (riverMask - 1); // remove river card from mask

                // get relevant hero rank from pre-computed array
                int selfBest    = heroRiverEvals[riverCardIndex];
                int villainBest = eval_7(
                    villainCard1, villainCard2,
                    context.b0, context.b1, context.b2, context.b3,
                    riverCard
                ); // evaluate villain on river

                // final score comparison
                int finalState;
                if (selfBest < villainBest) {
                    finalState = BEHIND;
                } else if (selfBest > villainBest) {
                    hsWin[riverCardIndex]++;
                    finalState = AHEAD;
                } else {
                    hsTie[riverCardIndex]++;
                    finalState = TIED;
                }
                
                // evaluate nut potential for 3rd feature
                if (selfBest > TRIPS_THRESHOLD) {
                    // we are over the ceiling
                    nutWinTotal += 1.f;
                    // we actually win in this situation
                    if (selfBest > villainBest) {
                        nutWinSum += 1.f;
                    }
                }

                // increment counters of EHS computation
                hsTotal[riverCardIndex]++;
                HP[turnState][finalState] += 1.f;
            }
        }
    }

    // calculate simple hand strength
    float handStrength = (betterThan + 0.5f * equal) / (betterThan + worseThan + equal);

    float PpotDenominator = handPotentialTotal[BEHIND] + handPotentialTotal[TIED];
    float NpotDenominator = handPotentialTotal[AHEAD]  + handPotentialTotal[TIED];

    constexpr float remainingRiverCards = 44.f;
    float Ppot = 0.f;
    float Npot = 0.f;

    if (PpotDenominator > 0.f) {
        Ppot = (HP[BEHIND][AHEAD] +
                HP[BEHIND][TIED] * 0.5f +
                HP[TIED][AHEAD]  * 0.5f) /
               (PpotDenominator * remainingRiverCards);
    }

    if (NpotDenominator > 0.f) {
        Npot = (HP[AHEAD][BEHIND] +
                HP[AHEAD][TIED]  * 0.5f +
                HP[TIED][BEHIND] * 0.5f) /
               (NpotDenominator * remainingRiverCards);
    }

    // compute EHS using helper
    float EHS = computeEHS(handStrength, Ppot, Npot);
    // compute asymmetry using helper
    float asymmetry = computeAsymmetry(handStrength, Ppot, Npot);

    // compute nut potential
    float nutPotential = 0.f;
    if (nutWinTotal > 0.f) {
        nutPotential = nutWinSum / nutWinTotal;
    }

    return TurnFeatures{ EHS, asymmetry, nutPotential };
}

RiverFeatures calculateRiverFeatures(const std::array<int, 2>& hand, const std::array<int, 5>& board) {
    
    auto context = createRiverContext(hand, board);

    constexpr int TWO_PAIR_THRESHOLD = 4138;
    int strongCombosNoHero = 0;
    int totalCombosNoHero  = 0;

    uint64_t villainMaskNoHero = (~(
        (1ULL << board[0]) | (1ULL << board[1]) | (1ULL << board[2]) |
        (1ULL << board[3]) | (1ULL << board[4])
    )) & ((1ULL << 52) - 1);

    {
        uint64_t villainMask1 = villainMaskNoHero;
        while (villainMask1) {
            int villainIndex1 = __builtin_ctzll(villainMask1);
            int villainCard1 = deck[villainIndex1];
            villainMask1 &= villainMask1 - 1;

            uint64_t villainMask2 = villainMask1;
            while (villainMask2) {
                int villainIndex2 = __builtin_ctzll(villainMask2);
                int villainCard2 = deck[villainIndex2];
                villainMask2 &= villainMask2 - 1;

                int score = eval_7(villainCard1, villainCard2,
                    context.b0, context.b1, context.b2,
                    context.b3, context.b4);

                totalCombosNoHero++;
                
                if (score > TWO_PAIR_THRESHOLD) {
                    strongCombosNoHero++;
                }
            }
        }
    }

    // counters for equity computation
    int strongCombos = 0; // two pair or better
    int weakCombos   = 0; // one pair or worse
    int totalCombos  = 0; // sum

    // equity accumulators
    float winAll = 0.f;
    float winStrong = 0.f;
    float winWeak = 0.f;
    
    float tieAll = 0.f;
    float tieStrong = 0.f;
    float tieWeak = 0.f;

    uint64_t villainMask = context.deckMask;

    {
        uint64_t villainMask1 = villainMask;
        while (villainMask1) {
            int villainIndex1 = __builtin_ctzll(villainMask1);
            int villainCard1 = deck[villainIndex1];
            villainMask1 &= villainMask1 - 1;

            uint64_t villainMask2 = villainMask1;
            while (villainMask2) {
                int villainIndex2 = __builtin_ctzll(villainMask2);
                int villainCard2 = deck[villainIndex2];
                villainMask2 &= villainMask2 - 1;

                int villainScore = eval_7(villainCard1, villainCard2,
                    context.b0, context.b1, context.b2,
                    context.b3, context.b4);

                totalCombos++;

                // total equity
                if (context.selfRank > villainScore) {
                    winAll += 1.f;
                } else if (context.selfRank == villainScore) {
                    tieAll += 0.5f;
                }

                // bucket by hand strength category
                if (villainScore > TWO_PAIR_THRESHOLD) {
                    // strong: two pair or better
                    strongCombos++;
                    if (context.selfRank > villainScore) {
                        winStrong += 1.f;
                    } else if (context.selfRank == villainScore) {
                        tieStrong += 0.5f;
                    }
                } else {
                    // weak: one pair or worse
                    weakCombos++;
                    if (context.selfRank > villainScore) {
                        winWeak += 1.f;
                    } else if (context.selfRank == villainScore) {
                        tieWeak += 0.5f;
                    }
                }
            }
        }
    }

    // F1: overall equity — how often hero wins against a random villain hand
    float equityTotal;
    if (totalCombos > 0) {
        equityTotal = (winAll + tieAll) / totalCombos;
    } else {
        equityTotal = 0.f;
    }

    // F2: equity against strong hands — how often hero beats two pair or better
    float equityVsStrong;
    if (strongCombos > 0) {
        equityVsStrong = (winStrong + tieStrong) / strongCombos;
    } else {
        equityVsStrong = 0.f;
    }

    // F3: equity against weak hands — how often hero beats one pair or worse
    float equityVsWeak;
    if (weakCombos > 0) {
        equityVsWeak = (winWeak + tieWeak) / weakCombos;
    } else {
        equityVsWeak = 0.f;
    }

    // F4: blocker index — how much do hero's cards reduce villain's strong combos
    float blockerIndex = 0.f;
    if (strongCombosNoHero > 0) {
        // scale expected strong combos from no-hero universe (1081 combos)
        // down to hero universe (990 combos) for a fair comparison
        float expectedStrong = float(strongCombosNoHero) * (990.f / 1081.f);
        blockerIndex = 1.f - (float(strongCombos) / expectedStrong);
        blockerIndex = std::max(-1.f, std::min(1.f, blockerIndex));
    }

    return RiverFeatures{ equityTotal, equityVsStrong, equityVsWeak, blockerIndex };
}

}
