#include "eval/evaluator.hpp"
#include "hand_abstraction.hpp"

namespace Eval {

struct FlopContext {
    uint64_t deckMask;
    int h0, h1;
    int b0, b1, b2;
    int selfRank;
};

struct TurnContext {
    uint64_t deckMask;
    int h0, h1;
    int b0, b1, b2, b3;
    int selfRank;
};

enum HandState : int {
    AHEAD  = 0,
    TIED   = 1,
    BEHIND = 2
};

// helper to build the bitmask neccessary for flop and turn computations
inline uint64_t buildDeckMask(const std::initializer_list<int>& usedCards)
{
    uint64_t used = 0;
    for (int c : usedCards)
        used |= 1ULL << c;

    return (~used) & ((1ULL << 52) - 1);
}

inline FlopContext createFlopContext(
    const std::array<int,2>& hand,
    const std::array<int,3>& board)
{
    uint64_t deckMask = buildDeckMask({ hand[0], hand[1], board[0], board[1], board[2] });

    return {
        deckMask,
        deck[hand[0]], deck[hand[1]], deck[board[0]], deck[board[1]], deck[board[2]],
        eval_5(deck[hand[0]], deck[hand[1]], deck[board[0]], deck[board[1]], deck[board[2]])
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

inline EvalContext createContext(
    const std::array<int,2>& hand,
    const std::array<int,3>& board)
{
    uint64_t used = 0;
    used |= 1ULL << hand[0];
    used |= 1ULL << hand[1];
    used |= 1ULL << board[0];
    used |= 1ULL << board[1];
    used |= 1ULL << board[2];

    return {
        (~used) & ((1ULL << 52) - 1),
        deck[hand[0]], deck[hand[1]],
        deck[board[0]], deck[board[1]], deck[board[2]],
        eval_5(deck[hand[0]], deck[hand[1]],
               deck[board[0]], deck[board[1]], deck[board[2]])
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

FlopFeatures calculateFlopTransitionsTwoAhead(
    const std::array<int, 2>& hand,
    const std::array<int, 3>& board)
{
    
    // call environment initializer helper
    auto context = createFlopContext(hand, board);

    // variables for EquityUnderPressure feature
    int topCount;
    int actualTopHandCount = 0;
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
            if (isTopRange) {
                actualTopHandCount++;
            }

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
    if (actualTopHandCount > 0) {
        equityUnderPressure = (topWins + 0.5f * topTies) / (actualTopHandCount * 990.f);
    }

    return FlopFeatures{ handStrength, asymmetry, volatility, equityUnderPressure };
}

// function to calculate strength variables for turn
TurnFeatures calculateTurnFeatures(const std::array<int, 2>& hand,
                           const std::array<int, 4>& board) {
    
    // call environment initializer helper
    auto context = createTurnContext(hand, board);
    
    // variables for EquityUnderPressure feature
    int topCount;
    int actualTopHandCount = 0;
    int cutoff = computeTopRangeCutoffTurn(
        context.deckMask, context.b0, context.b1, context.b2, context.b3,
        0.15f, topCount);
    float topWins = 0.f;
    float topTies = 0.f;
    
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
    
    uint64_t villainMask = context.deckMask;
    
    while (villainMask) {
        int villainIndex1 = __builtin_ctzll(villainMask);
        int villainCard1 = deck[villainIndex1];
        villainMask &= (villainMask - 1);
        
        uint64_t villainMask2 = villainMask;
        while(villainMask2) {
            int villainIndex2 = __builtin_ctzll(vilainMask2);
            int villainCard2 = deck[villainIndex2];
            villainMask2 &= (villainMask2 - 1);
            
            int villainScore = eval_6(context.b0, context.b1, context.b2, context.b3, villainCard1, villainCard2); // evaluate opponent on turn
            bool isTopRange = (villainScore >= cutoff);
            if (isTopRange) {
                actualTopHandCount++;
            }
            
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
                    if (selfBest > villainBest) {
                        topWins += 1.f;
                    } else if (selfBest == villainBest) {
                        topTies += 1.f;
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
        Ppot = (HP[2][0] + HP[2][1]/2.f + HP[1][0]/2.f) / (PpotDenominator * remainingRiverCards);
    }

    if (NpotDenominator > 0.f) {
        Npot = (HP[0][2] + HP[0][1]/2.f + HP[1][2]/2.f) / (NpotDenominator * remainingRiverCards);
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
    if (actualTopHandCount > 0) {
        equityUnderPressure = (topWins + 0.5f * topTies) / (actualTopHandCount * remainingRiverCards);
    }

    return TurnFeatures{ EHS, asymmetry, volatility, equityUnderPressure };
}

RiverFeatures calculateRiverFeatures(const std::array<int, 2>& hand,
                                     const std::array<int, 5>& board) {
    
    uint64_t usedCards = 0;
    int totalVillainCombos = 990;
    
    usedCards |= (1ULL << hand[0]);
    usedCards |= (1ULL << hand[1]);
    usedCards |= (1ULL << board[0]);
    usedCards |= (1ULL << board[1]);
    usedCards |= (1ULL << board[2]);
    usedCards |= (1ULL << board[3]);
    usedCards |= (1ULL << board[4]);
    
    uint64_t mask = (~usedCards) & ((uint64_t(1) << 52) - 1);
    
    int h0 = deck[hand[0]];
    int h1 = deck[hand[1]];
    int b0 = deck[board[0]];
    int b1 = deck[board[1]];
    int b2 = deck[board[2]];
    int b3 = deck[board[3]];
    int b4 = deck[board[4]];
    
    int selfRank = eval_7(h0, h1, b0, b1, b2, b3, b4);
    std::array<int, 990> villainCombos;
    
    int count = 0;
    while (mask) {
        int villainIndex1 = __builtin_ctzll(mask);
        int villainCard1 = deck[villainIndex1];
        mask &= (mask - 1);
        
        uint64_t mask2 = mask;
        
        while(mask2) {
            int villainIndex2 = __builtin_ctzll(mask2);
            int villainCard2 = deck[villainIndex2];
            mask2 &= (mask2 - 1);
            
            int villainScore = eval_7(b0, b1, b2, b3, b4, villainCard1, villainCard2);
            villainCombos[count] = villainScore;
            
            count++;
        }
    }
    
    std::sort(villainCombos.begin(), villainCombos.end());
    
    auto eq = [&](int start, int end) -> float {
        if (start >= end) return 0.0f;
        
        float wins = 0.0f;
        float ties = 0.0f;
        
        for(int i = start; i < end; i++) {
            if(selfRank > villainCombos[i]) {
                wins += 1.0f;
            } else if(selfRank == villainCombos[i]) {
                ties += 1.0f;
            }
        }
        
        return (wins + 0.5f * ties) / float(end - start);
    };
    
    int botStart = 0;
    int botEnd = int(0.2f * totalVillainCombos);
    
    int midStart = int(0.4 * totalVillainCombos);
    int midEnd   = int(0.6 * totalVillainCombos);

    int topStart = int(0.85 * totalVillainCombos);
    int topEnd = totalVillainCombos;
            
    float eVsTop = eq(topStart, topEnd);
    
    return RiverFeatures { eVsUniform, vsStrongDelta, vsWeakDelta, midCurvature };
}


/*
// 64bit bitmask to mark used cards inside the deck
uint64_t usedCards = 0;
// flip corresponding used bits in the bitmask
usedCards |= (1ULL << hand[0]);
usedCards |= (1ULL << hand[1]);
usedCards |= (1ULL << board[0]);
usedCards |= (1ULL << board[1]);
usedCards |= (1ULL << board[2]);

// 47 out of 52 bits will be flipped according to which cards are still available
uint64_t deckMask = (~usedCards) & ((1ULL << 52) - 1);

// get the encoded card formats for evaluation
int h0 = deck[hand[0]];
int h1 = deck[hand[1]];
int b0 = deck[board[0]];
int b1 = deck[board[1]];
int b2 = deck[board[2]];

// evaluate our 5-card rank with these specific cards
int selfRank = eval_5(h0, h1, b0, b1, b2);

// initializing histogram for 4th feature
constexpr int MAX_SCORE = 7463;
int scoreHistogram[MAX_SCORE] = {};
int villainComboCount = 0;
uint64_t vMask = deckMask;
while (vMask) {
    int i = __builtin_ctzll(vMask);
    vMask &= (vMask - 1);

    uint64_t vMask2 = vMask;
    while (vMask2) {
        int j = __builtin_ctzll(vMask2);
        vMask2 &= (vMask2 - 1);
        int villainScore = eval_5(b0, b1, b2, deck[i], deck[j]);
        
        scoreHistogram[villainScore]++;
        villainComboCount++;
    }
}

// declaring variables for assessing the 4th feature
constexpr float TOP_PCT = 0.15f;
int topTarget = int(villainComboCount * TOP_PCT + 0.5f);
float topWins = 0.f;
float topTies = 0.f;

int running = 0;
int cutoffScore = MAX_SCORE - 1;

for (int s = 0; s < MAX_SCORE; ++s) {
    running += scoreHistogram[s];
    if (running >= topTarget) {
        cutoffScore = s;
        break;
    }
}

// initializing 2d array for precomputed self hand ranks
int heroTurnEvals[52][52] = {};

uint64_t turnPreMask = deckMask;

// we pre-calculate self hand rank against all upcoming cards
while(turnPreMask) {
    int cardIndex = __builtin_ctzll(turnPreMask);
    turnPreMask &= (turnPreMask - 1);
    uint64_t riverPreMask = turnPreMask;
    while(riverPreMask) {
        int secondCardIndex = __builtin_ctzll(riverPreMask);
        riverPreMask &= (riverPreMask - 1);
        // populate 2d hand rank array with 7-card evaluations
        heroTurnEvals[cardIndex][secondCardIndex] = eval_7(h0, h1, b0, b1, b2, deck[cardIndex], deck[secondCardIndex]);
        heroTurnEvals[secondCardIndex][cardIndex] = heroTurnEvals[cardIndex][secondCardIndex];
    }
}
 */

/*
function to calculate strength variables for flop, uses npot1-ppot1 calculation
StrengthTransitions calculateFlopStrengthTransitions(
    const std::array<int, 2>& hand,
    const std::array<int, 3>& board)
{
    
    // 64bit bitmask to mark used cards inside the deck
    uint64_t usedCards = 0;

    // flip corresponding used bits in the bitmask
    usedCards |= (1ULL << hand[0]);
    usedCards |= (1ULL << hand[1]);
    usedCards |= (1ULL << board[0]);
    usedCards |= (1ULL << board[1]);
    usedCards |= (1ULL << board[2]);
        
    // 47 out of 52 bits will be flipped according to which cards are still available
    uint64_t mask = (~usedCards) & ((uint64_t(1) << 52) - 1);
    
    // get the encoded card formats for evaluation
    int h0 = deck[hand[0]];
    int h1 = deck[hand[1]];
    int b0 = deck[board[0]];
    int b1 = deck[board[1]];
    int b2 = deck[board[2]];
    
    int selfRank = eval_5(h0, h1, b0, b1, b2);
    
    int heroTurnEvals[52] = {};
    uint64_t preCalcMask = mask;
    
    // we pre-calculate the hand strength against all-turns
    while(preCalcMask) {
        int cardIndex = __builtin_ctzll(preCalcMask);
        preCalcMask &= (preCalcMask - 1);
        heroTurnEvals[cardIndex] = eval_6(h0, h1, b0, b1, b2, deck[cardIndex]);
    }
    
    int betterThan = 0; // opponent hand is weaker
    int worseThan = 0; // opponent hand is stronger
    int equal = 0; // opponent hand is equal
    
    int HP[3][3] = {}; // [flopState][finalState]
    int handPotentialTotal[3] = {};
    
    // volatility accumulators (HS on turn)
    int hsWin[52] = {};
    int hsTie[52] = {};
    int hsTotal[52] = {};
    
    // HS calculation
        
    while (mask) {
        int villainIndex1 = __builtin_ctzll(mask);       // 0-51 index for bitmask
        int villainCard1 = deck[villainIndex1];          // encoding for evaluation
        mask &= (mask - 1);                            // remove from mask
        
        uint64_t mask2 = mask;
        while(mask2) {
            int villainIndex2 = __builtin_ctzll(mask2);
            int villainCard2 = deck[villainIndex2];
            mask2 &= (mask2 - 1);
            
            int villainScore = eval_5(b0, b1, b2, villainCard1, villainCard2); // now we can eval opponent score on flop
            
            // compute counters
            int flopState;
            
            if(selfRank < villainScore) {
                worseThan ++;
                flopState = BEHIND;
            } else if(selfRank > villainScore) {
                betterThan ++;
                flopState = AHEAD;
            } else {
                equal ++;
                flopState = TIED;
            }
            
            handPotentialTotal[flopState]++;
            
            // EHS calculation
            
            uint64_t turnMask = (~usedCards) & ((1ULL << 52) - 1); // init mask of available cards to draw turn from
            turnMask &= ~(1ULL << villainIndex1);
            turnMask &= ~(1ULL << villainIndex2); // remove both villain cards
            
            while(turnMask) {
                int turnCardIndex = __builtin_ctzll(turnMask); // draw turn card
                int turnCard = deck[turnCardIndex]; // draw turn card
                turnMask &= (turnMask - 1); // remove used turn card from mask
                
                int selfBest = heroTurnEvals[turnCardIndex];
                int villainBest = eval_6(villainCard1, villainCard2, b0, b1, b2, turnCard); // calculate opponent score
                
                int finalState;

                if(selfBest < villainBest) {
                    finalState = BEHIND;
                } else if(selfBest > villainBest) {
                    hsWin[turnCardIndex]++;
                    finalState = AHEAD;
                } else {
                    hsTie[turnCardIndex]++;
                    finalState = TIED;
                }
                
                hsTotal[turnCardIndex]++;
                HP[flopState][finalState] ++;
            }
        }
    }
    
    // compute HS per turn
    float hsTurn[52];
    int hsCount = 0;
    
    for (int i = 0; i < 52; ++i) {
        if (hsTotal[i] > 0) {
            hsTurn[hsCount++] =
                (hsWin[i] + 0.5f * hsTie[i]) / hsTotal[i];
        }
    }
    
    // compute volatility
    float mean = 0.f;
    for (int i = 0; i < hsCount; ++i) {
        mean += hsTurn[i];
    }
    mean /= hsCount;

    float variance = 0.f;
    for (int i = 0; i < hsCount; ++i) {
        float d = hsTurn[i] - mean;
        variance += d * d;
    }
    
    variance /= hsCount;
    float volatility = sqrtf(variance);
    
    // hand strength computation
    float handStrength = (betterThan + 0.5f * equal) / (betterThan + worseThan + equal);
    
    float PpotDenominator = handPotentialTotal[2] + handPotentialTotal[1];
    float NpotDenominator = handPotentialTotal[0] + handPotentialTotal[1];

    float Ppot = 0.f;
    float Npot = 0.f;
    const float remainingTurnCards = 45.0f;

    // compute potentials if denominators are non-zero
    if (PpotDenominator > 0.f) {
        Ppot = (HP[2][0] + HP[2][1]/2.f + HP[1][0]/2.f) / (PpotDenominator * remainingTurnCards);
    }

    if (NpotDenominator > 0.f) {
        Npot = (HP[0][2] + HP[0][1]/2.f + HP[1][2]/2.f) / (NpotDenominator * remainingTurnCards);
    }

    return StrengthTransitions{ handStrength, Ppot, Npot, volatility };
}
*/

}
