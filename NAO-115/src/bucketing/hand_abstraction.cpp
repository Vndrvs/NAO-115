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
    
    int h0 = deck[hand[0]];
    int h1 = deck[hand[1]];
    int b0 = deck[board[0]];
    int b1 = deck[board[1]];
    int b2 = deck[board[2]];
    
    // build mask
    uint64_t deckMask = buildDeckMask({ hand[0], hand[1], board[0], board[1], board[2] });
    
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

FlopFeatures calculateFlopFeaturesTwoAhead(const std::array<int, 2>& hand, const std::array<int, 3>& board) {
    // call environment initializer helper
    auto context = createFlopContext(hand, board);
    
    int heroEval[52][52] = {};
    
    // pre-compute hero ranks on possible boards
    precomputeHero7(
                    context.deckMask,
                    context.h0, context.h1,
                    context.b0, context.b1, context.b2,
                    heroEval
                    );
    
    // EHS / potential computation value set
    float betterThan = 0.f; // opponent hand is weaker
    float worseThan  = 0.f; // opponent hand is stronger
    float equal      = 0.f; // tie
    
    // initializing hand potential helper arrays for EHS calculation
    float HP[3][3]              = {};
    float handPotentialTotal[3] = {};
    
    // per-runout counters for volatility
    int   hsWin[52][52]         = {};
    int   hsTie[52][52]         = {};
    int   hsTotal[52][52]       = {};
    
    // initialize available cards mask to keep track of looping through villain cards
    uint64_t villainMask = context.deckMask;
    while (villainMask) {
        int villainIndex1 = __builtin_ctzll(villainMask);
        int villainCard1 = deck[villainIndex1];
        villainMask &= (villainMask - 1);
        
        uint64_t villainMask2 = villainMask;
        while (villainMask2) {
            int villainIndex2 = __builtin_ctzll(villainMask2);
            int villainCard2 = deck[villainIndex2];
            villainMask2 &= (villainMask2 - 1);
            
            // evaluate 5-card villain score with the given cards
            int villainScore = eval_5(context.b0, context.b1, context.b2, villainCard1, villainCard2);
            
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
            
            // mask with available cards for drawing turn
            uint64_t turnMask = context.deckMask;
            // remove villain cards from turn mask
            turnMask &= ~(1ULL << villainIndex1);
            turnMask &= ~(1ULL << villainIndex2);
            
            // loop through available cards of deck to get turn and river cards
            while (turnMask) {
                int turnCardIndex = __builtin_ctzll(turnMask);
                int turnCard = deck[turnCardIndex];
                turnMask &= (turnMask - 1);
                
                uint64_t riverMask = turnMask;
                while (riverMask) {
                    int riverCardIndex = __builtin_ctzll(riverMask);
                    int riverCard = deck[riverCardIndex];
                    riverMask &= (riverMask - 1);
                    
                    int selfBest = heroEval[turnCardIndex][riverCardIndex];
                    
                    int villainBest = eval_7(villainCard1, villainCard2, context.b0, context.b1, context.b2, turnCard, riverCard);
                    
                    int finalState;
                    if (selfBest < villainBest) {
                        finalState = BEHIND;
                    } else if (selfBest > villainBest) {
                        finalState = AHEAD;
                        hsWin[turnCardIndex][riverCardIndex]++;
                    } else {
                        finalState = TIED;
                        hsTie[turnCardIndex][riverCardIndex]++;
                    }
                    
                    hsTotal[turnCardIndex][riverCardIndex]++;
                    HP[flopState][finalState] += 1.f;
                }
            }
        }
    }
    
    // compute Effective Hand Strength
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
    
    float EHS = computeEHS(handStrength, Ppot, Npot);
    // compute asymmetry with EHS variables
    float asymmetry = computeAsymmetry(handStrength, Ppot, Npot);

    // compute 2-card flop volatility (weighted by probability mass)
    float totalWeight = 0.f;
    float weightedSum = 0.f;
    
    // 1. Calculate the Weighted Mean
    for (int t = 0; t < 52; t++) {
        for (int r = 0; r < 52; r++) {
            if (hsTotal[t][r] > 0) {
                float weight = hsTotal[t][r];
                float eq = (hsWin[t][r] + 0.5f * hsTie[t][r]) / weight;
                
                totalWeight += weight;
                weightedSum += weight * eq;
            }
        }
    }
    
    float weightedMean = 0.f;
    if (totalWeight > 0.f) {
        weightedMean = weightedSum / totalWeight;
    }
    
    float weightedVariance = 0.f;
    if (totalWeight > 0.f) {
        for (int t = 0; t < 52; t++) {
            for (int r = 0; r < 52; r++) {
                if (hsTotal[t][r] > 0) {
                    float weight = hsTotal[t][r];
                    float eq = (hsWin[t][r] + 0.5f * hsTie[t][r]) / weight;
                    
                    float d = eq - weightedMean;
                    weightedVariance += weight * (d * d);
                }
            }
        }
        weightedVariance /= totalWeight;
    }
    
    float volatility = sqrtf(weightedVariance);
    
    return FlopFeatures{ EHS, asymmetry, volatility };
}

// function to calculate strength variables for turn
TurnFeatures calculateTurnFeatures(const std::array<int, 2>& hand, const std::array<int, 4>& board) {
    
    // call environment initializer helper
    auto context = createTurnContext(hand, board);

    // we pre-calculate hero ranks on possible river cards
    int heroRiverEvals[52] = {};
    uint64_t preRiverMask = context.deckMask;
    
    while(preRiverMask) {
        int cardIndex = __builtin_ctzll(preRiverMask);
        preRiverMask &= (preRiverMask - 1);
        heroRiverEvals[cardIndex] = eval_7(context.h0, context.h1, context.b0, context.b1, context.b2, context.b3, deck[cardIndex]);
    }
    
    // EHS / potential computation value set
    float betterThan = 0.f;
    float worseThan = 0.f;
    float equal = 0.f;
    
    // initializing hand potential helper arrays for EHS calculation
    float HP[3][3] = {};
    float handPotentialTotal[3] = {};
    
    // per-runout counters for volatility
    int hsWin[52] = {};
    int hsTie[52] = {};
    int hsTotal[52] = {};
    
    // create mask for computing villain hands
    uint64_t villainMask = context.deckMask;
    
    while (villainMask) {
        int villainIndex1 = __builtin_ctzll(villainMask);
        int villainCard1 = deck[villainIndex1];
        // found first villain card
        villainMask &= (villainMask - 1);
        
        uint64_t villainMask2 = villainMask;
        while(villainMask2) {
            int villainIndex2 = __builtin_ctzll(villainMask2);
            int villainCard2 = deck[villainIndex2];
            // found second villain card
            villainMask2 &= (villainMask2 - 1);
            
            int villainScore = eval_6(context.b0, context.b1, context.b2, context.b3, villainCard1, villainCard2); // evaluate villain on turn

            // initialize and set starting score comparison
            int turnState;
            
            if(context.selfRank < villainScore) {
                worseThan += 1.f;
                turnState = BEHIND;
            } else if(context.selfRank > villainScore) {
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
            riverMask &= ~(1ULL << villainIndex2);
            
            while(riverMask) {
                int riverCardIndex = __builtin_ctzll(riverMask); // draw river card
                int riverCard = deck[riverCardIndex]; // draw river card
                riverMask &= (riverMask - 1); // remove used river card from mask
                
                int selfBest = heroRiverEvals[riverCardIndex];
                int villainBest = eval_7(villainCard1, villainCard2, context.b0, context.b1, context.b2, context.b3, riverCard); // calculate opponent score
                
                // initialize and set score comparison after drawing the river card
                int finalState;
                
                if(selfBest < villainBest) {
                    finalState = BEHIND;
                } else if(selfBest > villainBest) {
                    hsWin[riverCardIndex]++;
                    finalState = AHEAD;
                } else {
                    hsTie[riverCardIndex]++;
                    finalState = TIED;
                }
                
                hsTotal[riverCardIndex]++;
                HP[turnState][finalState] += 1.f;
            }
        }
    }
    
    // hand strength computation
    float handStrength = (betterThan + 0.5f * equal) / (betterThan + worseThan + equal);
    
    float PpotDenominator = handPotentialTotal[BEHIND] + handPotentialTotal[TIED];
    float NpotDenominator = handPotentialTotal[AHEAD] + handPotentialTotal[TIED];

    float Ppot = 0.f;
    float Npot = 0.f;
    const float remainingRiverCards = 44.0f;

    // compute potentials if denominators are non-zero
    if (PpotDenominator > 0.f) {
        Ppot = (HP[BEHIND][AHEAD] + HP[BEHIND][TIED]/2.f + HP[TIED][AHEAD]/2.f) / (PpotDenominator * remainingRiverCards);
    }

    if (NpotDenominator > 0.f) {
        Npot = (HP[AHEAD][BEHIND] + HP[AHEAD][TIED]/2.f + HP[TIED][BEHIND]/2.f) / (NpotDenominator * remainingRiverCards);
    }
    
    // compute EHS and asymmetry from HS, Ppot and Npot
    float EHS = computeEHS(handStrength, Ppot, Npot);
    float asymmetry = computeAsymmetry(handStrength, Ppot, Npot);
    
    // compute 1-card turn volatility (weighted)
    float totalWeight = 0.f;
    float weightedSum = 0.f;

    // 1. weighted mean
    for (int i = 0; i < 52; ++i) {
        if (hsTotal[i] > 0) {
            float weight = hsTotal[i];
            float eq = (hsWin[i] + 0.5f * hsTie[i]) / weight;

            totalWeight += weight;
            weightedSum += weight * eq;
        }
    }

    float weightedMean = 0.f;
    if (totalWeight > 0.f) {
        weightedMean = weightedSum / totalWeight;
    }

    // 2. weighted variance
    float weightedVariance = 0.f;
    if (totalWeight > 0.f) {
        for (int i = 0; i < 52; ++i) {
            if (hsTotal[i] > 0) {
                float weight = hsTotal[i];
                float eq = (hsWin[i] + 0.5f * hsTie[i]) / weight;

                float d = eq - weightedMean;
                weightedVariance += weight * d * d;
            }
        }
        weightedVariance /= totalWeight;
    }

    float volatility = sqrtf(weightedVariance);

    return TurnFeatures{ EHS, asymmetry, volatility };
}

RiverFeatures calculateRiverFeatures(const std::array<int, 2>& hand,
                                     const std::array<int, 5>& board) {
    
    // before any computation, we need to know our own rank
    int h0 = deck[hand[0]];
    int h1 = deck[hand[1]];
    int b0 = deck[board[0]];
    int b1 = deck[board[1]];
    int b2 = deck[board[2]];
    int b3 = deck[board[3]];
    int b4 = deck[board[4]];
    int selfRank = eval_7(h0, h1, b0, b1, b2, b3, b4);
    
    static constexpr int MAX_SCORE = 7462;
    int scoreHistogram[MAX_SCORE + 1] = {};
    int totalCombos = 0;
    
    int scoreHistogramNoHero[MAX_SCORE + 1] = {};
    int totalCombosNoHero = 0;
    
    // build mask that excludes only board cards
    uint64_t usedBoard = 0;
    for (int b : board) {
        usedBoard |= 1ULL << b;
    }
    
    uint64_t villainMaskNoHero = (~usedBoard) & ((1ULL << 52) - 1);
    
    while (villainMaskNoHero) {
        int villainIndex1 = __builtin_ctzll(villainMaskNoHero);
        int villainCard1 = deck[villainIndex1];
        villainMaskNoHero &= (villainMaskNoHero - 1);
        
        uint64_t mask2 = villainMaskNoHero;
        while (mask2) {
            int villainIndex2 = __builtin_ctzll(mask2);
            int villainCard2 = deck[villainIndex2];
            mask2 &= (mask2 - 1);
            
            int vScore = eval_7(b0, b1, b2, b3, b4, villainCard1, villainCard2);
            scoreHistogramNoHero[vScore]++;
            totalCombosNoHero++;
        }
    }
    
    // build mask to get the available cards
    uint64_t usedCards = 0;
    usedCards |= 1ULL << hand[0];
    usedCards |= 1ULL << hand[1];
    for (int b : board) {
        usedCards |= 1ULL << b;
    }
    
    uint64_t villainMask = (~usedCards) & ((1ULL << 52) - 1);
    
    // now we enumerate all villain scores and add them into the histogram
    while (villainMask) {
        int villainIndex1 = __builtin_ctzll(villainMask);
        int villainCard1 = deck[villainIndex1];
        villainMask &= (villainMask - 1);
        
        uint64_t villainMask2 = villainMask;
        while (villainMask2) {
            int villainIndex2 = __builtin_ctzll(villainMask2);
            int villainCard2 = deck[villainIndex2];
            villainMask2 &= (villainMask2 - 1);
            
            int villainScore = eval_7(b0, b1, b2, b3, b4, villainCard1, villainCard2);
            scoreHistogram[villainScore]++;
            totalCombos++;
        }
    }
    
    // derive cutoff percentiles (relative to current cards)
    int topCut = -1;
    int midCut = -1;
    
    int cumulative = 0;
    const int topTarget = int(std::ceil(0.15f * totalCombos));
    const int midTarget = int(std::ceil(0.40f * totalCombos));
    
    // walk from strongest to weakest score
    for (int score = MAX_SCORE; score >= 0; --score) {
        int n = scoreHistogram[score];
        if (!n) {
            continue;
        }
        
        cumulative += n;
        
        if (topCut < 0 && cumulative >= topTarget) {
            topCut = score;
        }
        if (midCut < 0 && cumulative >= midTarget) {
            midCut = score;
            break;
        }
    }
    
    // derive cutoff percentiles (relative to current board only)
    int topCutNoHero = -1;
    int cumulativeNoHero = 0;
    const int topTargetNoHero = int(std::ceil(0.15f * totalCombosNoHero));
    
    for (int score = MAX_SCORE; score >= 0; --score) {
        int n = scoreHistogramNoHero[score];
        if (!n) continue;
        
        cumulativeNoHero += n;
        if (topCutNoHero < 0 && cumulativeNoHero >= topTargetNoHero) {
            topCutNoHero = score;
            break;
        }
    }
    
    // compute our equities against regions
    
    // win counters
    float winAll = 0.f;
    float winTop = 0.f;
    float winMid = 0.f;
    // tie counters
    float tieAll = 0.f;
    float tieTop = 0.f;
    float tieMid = 0.f;
    
    // combo counts for specific region
    int topCombos = 0;
    int midCombos = 0;
    
    for (int score = 0; score <= MAX_SCORE; ++score) {
        int n = scoreHistogram[score];
        if (!n) {
            continue;
        }
        // total equity
        if (selfRank > score) {
            winAll += n;
        } else if (selfRank == score) {
            tieAll += n;
        }
        // top 15% region
        if (score >= topCut) {
            topCombos += n;
            if (selfRank > score) {
                winTop += n;
            } else if (selfRank == score) {
                tieTop += n;
            }
        }
        // mid region (15%–40%)
        else if (score >= midCut) {
            midCombos += n;
            if (selfRank > score) {
                winMid += n;
            }
            else if (selfRank == score) {
                tieMid += n;
            }
        }
    }
    
    // compute blocker index
    int absoluteTopCombosNoHero = 0;
    for (int score = topCutNoHero; score <= MAX_SCORE; ++score) {
        absoluteTopCombosNoHero += scoreHistogramNoHero[score];
    }
    
    // 2. How many of those specific absolute Nut combos does Villain STILL hold?
    int actualTopCombos = 0;
    for (int score = topCutNoHero; score <= MAX_SCORE; ++score) {
        actualTopCombos += scoreHistogram[score];
    }
    
    // 3. Calculate blocker index
    float blockerIndex = 0.f;
    if (absoluteTopCombosNoHero > 0) {
        // Villain has 990 possible combos. The generic board has 1081.
        // Scale the absolute combos down so we compare apples to apples.
        float expectedTopCombos = float(absoluteTopCombosNoHero) * (990.f / 1081.f);
        
        blockerIndex = 1.f - (float(actualTopCombos) / expectedTopCombos);
        
        // Safety clamp to prevent floating point weirdness on extreme ties
        if (blockerIndex < -1.f) blockerIndex = -1.f;
        if (blockerIndex > 1.f)  blockerIndex = 1.f;
    }
    
    // calculate equity against regions
    float equityTotal = (winAll + 0.5f * tieAll) / totalCombos;
    float equityTop   = topCombos ? (winTop + 0.5f * tieTop) / topCombos : 0.f;
    float equityMid   = midCombos ? (winMid + 0.5f * tieMid) / midCombos : 0.f;
    
    return RiverFeatures { equityTotal, equityTop, equityMid, blockerIndex };
}

}
