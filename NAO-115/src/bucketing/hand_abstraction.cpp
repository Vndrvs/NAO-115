#include "eval/evaluator.hpp"
#include "hand_abstraction.hpp"
#include <iostream>

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

// helper to create flop environment
inline FlopContext createFlopContext(
    const std::array<int,2>& hand,
    const std::array<int,3>& board)
{
    // build mask
    uint64_t deckMask = buildDeckMask({ hand[0], hand[1], board[0], board[1], board[2] });
    
    int selfRank = eval_5(deck[hand[0]], deck[hand[1]], deck[board[0]], deck[board[1]], deck[board[2]]);

    return {
        deckMask,
        deck[hand[0]], deck[hand[1]], deck[board[0]], deck[board[1]], deck[board[2]],
        selfRank
    };
}

inline TurnContext createTurnContext(
    const std::array<int,2>& hand,
    const std::array<int,4>& board)
{
    uint64_t deckMask = buildDeckMask({ hand[0], hand[1], board[0], board[1], board[2], board[3] });

    return {
        deckMask,
        deck[hand[0]], deck[hand[1]], deck[board[0]], deck[board[1]], deck[board[2]], deck[board[3]],
        eval_6(deck[hand[0]], deck[hand[1]], deck[board[0]], deck[board[1]], deck[board[2]], deck[board[3]])
    };
}

// helper to run histogram logic involved in top range feature calculation
template <typename EvalFn>
inline int computeTopRangeCutoff(
    uint64_t availableDeckMask,
    float topPercentage,
    int& outTopComboCount,
    EvalFn evalVillain)
{
    constexpr int MAX_EVAL_SCORE = 7463;
    int scoreHistogram[MAX_EVAL_SCORE] = {};
    int totalVillainCombos = 0;

    uint64_t firstMask = availableDeckMask;
    while (firstMask) {
        int i = __builtin_ctzll(firstMask);
        firstMask &= firstMask - 1;

        uint64_t secondMask = firstMask;
        while (secondMask) {
            int j = __builtin_ctzll(secondMask);
            secondMask &= secondMask - 1;

            int score = evalVillain(i, j);
            scoreHistogram[score]++;
            totalVillainCombos++;
        }
    }

    outTopComboCount = int(totalVillainCombos * topPercentage + 0.5f);

    int accumulated = 0;
    for (int s = MAX_EVAL_SCORE - 1; s >= 0; --s) {
        accumulated += scoreHistogram[s];
        if (accumulated >= outTopComboCount)
            return s;
    }

    return MAX_EVAL_SCORE - 1;
}

inline int computeTopRangeCutoffFlop(
    uint64_t deckMask,
    int b0, int b1, int b2,
    float topPct,
    int& outTopCount)
{
    return computeTopRangeCutoff(
        deckMask,
        topPct,
        outTopCount,
        [&](int i, int j) {
            return eval_5(b0, b1, b2, deck[i], deck[j]);
        }
    );
}

inline int computeTopRangeCutoffTurn(
    uint64_t deckMask,
    int b0, int b1, int b2, int b3,
    float topPct,
    int& outTopCount)
{
    return computeTopRangeCutoff(
        deckMask,
        topPct,
        outTopCount,
        [&](int i, int j) {
            return eval_6(b0, b1, b2, b3, deck[i], deck[j]);
        }
    );
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

FlopFeatures calculateFlopFeaturesTwoAhead(
    const std::array<int, 2>& hand,
    const std::array<int, 3>& board)
{
    
    // call environment initializer helper
    auto context = createFlopContext(hand, board);

    // variables for EquityUnderPressure feature
    int topCount;
    int cutoff = computeTopRangeCutoffFlop(
        context.deckMask, context.b0, context.b1, context.b2,
        0.15f, topCount);
    float topWins = 0.f;
    float topTies = 0.f;
    
    int heroEval[52][52] = {};
    
    precomputeHero7(
        context.deckMask,
        context.h0, context.h1,
        context.b0, context.b1, context.b2,
        heroEval
    );
    
    // initializing counters for EHS calculation
    float betterThan = 0.f; // opponent hand is weaker
    float worseThan = 0.f; // opponent hand is stronger
    float equal = 0.f; // tie

    // initializing 1d 2d hand potential arrays for EHS calculation
    float HP[3][3] = {};
    float handPotentialTotal[3] = {};

    // per-(turn,river) HS for volatility
    int hsWin[52][52] = {};
    int hsTie[52][52] = {};
    int hsTotal[52][52] = {};
    
    // initialize clear mask to keep track of looping through villain cards
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
            bool isTopRange = (villainScore >= cutoff);

            int flopState;
            // set flop starting state and count comparison results
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

                    // get the relevant score from our pre-computed 2d self score array
                    int selfBest = heroEval[turnCardIndex][riverCardIndex];
                    // compute villain's 7-card score
                    int villainBest = eval_7(villainCard1, villainCard2, context.b0, context.b1, context.b2, turnCard, riverCard);

                    // increment the 4th feature counters
                    if (isTopRange) {
                        if (selfBest > villainBest) {
                            topWins += 1.f;
                        } else if (selfBest == villainBest) {
                            topTies += 1.f;
                        }
                    }
                    
                    int finalState;
                    // compare self and villain score to set final state variables
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

    // hand strength computation
    float handStrength = (betterThan + 0.5f * equal) / (betterThan + worseThan + equal);

    float PpotDenominator = handPotentialTotal[2] + handPotentialTotal[1];
    float NpotDenominator = handPotentialTotal[0] + handPotentialTotal[1];

    float Ppot = 0.f;
    float Npot = 0.f;
    const float remainingCombinations = 990.f;

    if (PpotDenominator > 0.f) {
        Ppot = (HP[2][0] + HP[2][1]/2.f + HP[1][0]/2.f) / (PpotDenominator * remainingCombinations);
    }
    if (NpotDenominator > 0.f) {
        Npot = (HP[0][2] + HP[0][1]/2.f + HP[1][2]/2.f) / (NpotDenominator * remainingCombinations);
    }
    
    // compute EHS and asymmetry from HS, Ppot and Npot
    float EHS = computeEHS(handStrength, Ppot, Npot);
    float asymmetry = computeAsymmetry(handStrength, Ppot, Npot);

    // compute 2-card flop volatility
    std::vector<float> hsTR;
    hsTR.reserve(52*52);
    for (int t = 0; t < 52; t++) {
        for (int r = 0; r < 52; r++) {
            if (hsTotal[t][r] > 0) {
                hsTR.push_back((hsWin[t][r] + 0.5f * hsTie[t][r]) / hsTotal[t][r]);
            }
        }
    }

    float mean = 0.f;
    for (float v : hsTR) {
        mean += v;
    }
    mean /= hsTR.size();

    float variance = 0.f;
    for (float v : hsTR) {
        float d = v - mean;
        variance += d * d;
    }
    variance /= hsTR.size();

    float volatility = sqrtf(variance);
    
    // compute performance against top villain hands
    float equityUnderPressure = 0.f;
    if (topCount > 0) {
        equityUnderPressure = (topWins + 0.5f * topTies) / (topCount * 990.f);
    }
    
    if (equityUnderPressure > 0.8f) {
        std::cout
            << "EUP=" << equityUnderPressure
            << " HS=" << handStrength
            << " VOL=" << volatility
            << " topCount=" << topCount
            << "\n";
    }

    return FlopFeatures{ EHS, asymmetry, volatility, equityUnderPressure };
}

// function to calculate strength variables for turn
TurnFeatures calculateTurnFeatures(const std::array<int, 2>& hand,
                           const std::array<int, 4>& board) {
    
    // call environment initializer helper
    auto context = createTurnContext(hand, board);
    
    // variables for EquityUnderPressure feature
    int topCount;
    int cutoff = computeTopRangeCutoffTurn(
        context.deckMask, context.b0, context.b1, context.b2, context.b3,
        0.15f, topCount);
    float topWins = 0.f;
    float topTies = 0.f;
    float topCombosWithRiver = 0;

    
    int heroRiverEvals[52] = {};
    uint64_t preRiverMask = context.deckMask;
    
    // we pre-calculate the hand strength against all river card options
    while(preRiverMask) {
        int cardIndex = __builtin_ctzll(preRiverMask);
        preRiverMask &= (preRiverMask - 1);
        heroRiverEvals[cardIndex] = eval_7(context.h0, context.h1, context.b0, context.b1, context.b2, context.b3, deck[cardIndex]);
    }
    
    float betterThan = 0.f;
    float worseThan = 0.f;
    float equal = 0.f;
    
    float HP[3][3] = {};
    float handPotentialTotal[3] = {};
    
    // volatility accumulators (HS on river)
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
            bool isTopRange = (villainScore >= cutoff);
            
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
            
            uint64_t riverMask = context.deckMask;
            riverMask &= ~(1ULL << villainIndex1);
            riverMask &= ~(1ULL << villainIndex2);
            
            
            while(riverMask) {
                int riverCardIndex = __builtin_ctzll(riverMask); // draw river card
                int riverCard = deck[riverCardIndex]; // draw turn card
                riverMask &= (riverMask - 1); // remove used turn card from mask
                
                int selfBest = heroRiverEvals[riverCardIndex];
                int villainBest = eval_7(villainCard1, villainCard2, context.b0, context.b1, context.b2, context.b3, riverCard); // calculate opponent score
                
                if (isTopRange) {
                    topCombosWithRiver++;
                    if (selfBest > villainBest) {
                        topWins++;
                    } else if (selfBest == villainBest) {
                        topTies++;
                    }
                }
                
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
    
    float PpotDenominator = handPotentialTotal[2] + handPotentialTotal[1];
    float NpotDenominator = handPotentialTotal[0] + handPotentialTotal[1];

    float Ppot = 0.f;
    float Npot = 0.f;
    const float remainingRiverCards = 44.0f;

    // compute potentials if denominators are non-zero
    
    if (PpotDenominator > 0.f) {
        Ppot = (HP[2][0] + HP[2][1]/2.f + HP[1][0]/2.f)
               / (HP[2][0] + HP[2][1] + HP[2][2] +
                  HP[1][0] + HP[1][1] + HP[1][2]);
    }

    if (NpotDenominator > 0.f) {
        Npot = (HP[0][2] + HP[0][1]/2.f + HP[1][2]/2.f)
               / (HP[0][0] + HP[0][1] + HP[0][2] +
                  HP[1][0] + HP[1][1] + HP[1][2]);
    }
    
    // compute EHS and asymmetry from HS, Ppot and Npot
    float EHS = computeEHS(handStrength, Ppot, Npot);
    float asymmetry = computeAsymmetry(handStrength, Ppot, Npot);
    
    // compute 1-card turn volatility
    float hsRiver[52];
    int hsCount = 0;
    
    for (int i = 0; i < 52; ++i) {
        if (hsTotal[i] > 0) {
            hsRiver[hsCount++] =
                (hsWin[i] + 0.5f * hsTie[i]) / hsTotal[i];
        }
    }
    
    float mean = 0.f;
    for (int i = 0; i < hsCount; ++i) {
        mean += hsRiver[i];
    }
    mean /= hsCount;

    float variance = 0.f;
    for (int i = 0; i < hsCount; ++i) {
        float d = hsRiver[i] - mean;
        variance += d * d;
    }
    
    variance /= hsCount;
    float volatility = sqrtf(variance);
    
    // compute performance against top villain hands
    float equityUnderPressure = 0.f;
    if (topCombosWithRiver > 0.f) {
        equityUnderPressure = (topWins + 0.5f * topTies) / topCombosWithRiver;
    }
    
    if (equityUnderPressure > 0.8f) {
        std::cout
            << "EUP=" << equityUnderPressure
            << " HS=" << handStrength
            << " VOL=" << volatility
            << " topCount=" << topCount
            << "\n";
    }
    
    return TurnFeatures{ EHS, asymmetry, volatility, equityUnderPressure };
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
    // compute top combinations without self cards
    int topCombosNoHero = 0;
    for (int score = topCutNoHero; score <= MAX_SCORE; ++score) {
        topCombosNoHero += scoreHistogramNoHero[score];
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
    float blockerIndex = 0.f;
    if (topCombosNoHero > 0) {
        blockerIndex = 1.f - (float(topCombos) / float(topCombosNoHero));
    }
    
    float equityTotal = (winAll + 0.5f * tieAll) / totalCombos;
    float equityTop   = topCombos ? (winTop + 0.5f * tieTop) / topCombos : 0.f;
    float equityMid   = midCombos ? (winMid + 0.5f * tieMid) / midCombos : 0.f;
    
    return RiverFeatures {
        equityTotal,
        equityTop,
        equityMid,
        blockerIndex
    };
}

}
