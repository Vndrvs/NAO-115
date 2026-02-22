#include "eval/evaluator.hpp"
#include "hand_abstraction.hpp"

namespace Eval {

struct EvalContext {
    uint64_t deckMask;
    int h0, h1, b0, b1, b2;
    int selfRank;
};

enum HandState : int {
    AHEAD  = 0,
    TIED   = 1,
    BEHIND = 2
};

// helper to build the bitmask neccessary for flop and turn computations
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
inline int computeTopRangeCutoff(
    uint64_t availableDeckMask,   // bitmask of unused cards
    int boardCard0,
    int boardCard1,
    int boardCard2,
    float topPercentage,          // size of range
    int& outTopComboCount)        // number of villain combos considered top
{
    constexpr int MAX_EVAL_SCORE = 7463;

    // histogram: how many villain combos achieve each 5-card score
    int scoreHistogram[MAX_EVAL_SCORE] = {};

    int totalVillainCombos = 0;

    // enumerate all unordered villain hole card combinations
    uint64_t firstCardMask = availableDeckMask;
    
    while (firstCardMask) {
        int firstIndex = __builtin_ctzll(firstCardMask);
        firstCardMask &= (firstCardMask - 1);

        uint64_t secondCardMask = firstCardMask;
        while (secondCardMask) {
            int secondIndex = __builtin_ctzll(secondCardMask);
            secondCardMask &= (secondCardMask - 1);

            int villainScore = eval_5(
                boardCard0,
                boardCard1,
                boardCard2,
                deck[firstIndex],
                deck[secondIndex]
            );

            scoreHistogram[villainScore]++;
            totalVillainCombos++;
        }
    }

    // how many combos constitute the top 'x' percent of villain hands
    outTopComboCount = int(totalVillainCombos * topPercentage + 0.5f);

    // accumulate scores from weakest to strongest
    int accumulatedCombos = 0;
    for (int score = 0; score < MAX_EVAL_SCORE; ++score) {
        accumulatedCombos += scoreHistogram[score];

        // we've included enough combos, this score is the cutoff
        if (accumulatedCombos >= outTopComboCount) {
            return score;
        }
    }

    // safety fallback
    return MAX_EVAL_SCORE - 1;
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

FlopFeatures calculateFlopTransitionsTwoAhead(
    const std::array<int, 2>& hand,
    const std::array<int, 3>& board)
{
    
    // call environment initializer helper
    auto context = createContext(hand, board);

    int topCount;
    int cutoff = computeTopRangeCutoff(
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
    float betterThan = 0.f;
    float worseThan = 0.f;
    float equal = 0.f;

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
    
    float eVsTop = 0.f;
    if (topCount > 0) {
        eVsTop = (topWins + 0.5f * topTies) / (topCount * 990.f);
    }

    return FlopFeatures{ handStrength, Ppot, Npot, volatility, eVsTop };
}

// function to calculate strength variables for turn
TurnFeatures calculateTurnStrengthTransitions(const std::array<int, 2>& hand,
                           const std::array<int, 4>& board) {
    
    uint64_t usedCards = 0;

    usedCards |= (1ULL << hand[0]);
    usedCards |= (1ULL << hand[1]);
    usedCards |= (1ULL << board[0]);
    usedCards |= (1ULL << board[1]);
    usedCards |= (1ULL << board[2]);
    usedCards |= (1ULL << board[3]);
        
    uint64_t mask = (~usedCards) & ((uint64_t(1) << 52) - 1);
    
    // get the encoded card formats for evaluation
    int h0 = deck[hand[0]];
    int h1 = deck[hand[1]];
    int b0 = deck[board[0]];
    int b1 = deck[board[1]];
    int b2 = deck[board[2]];
    int b3 = deck[board[3]];
    
    int selfRank = eval_6(h0, h1, b0, b1, b2, b3);
    
    int heroRiverEvals[52] = {};
    uint64_t preCalcMask = mask;
    
    // we pre-calculate the hand strength against all-turns
    while(preCalcMask) {
        int cardIndex = __builtin_ctzll(preCalcMask);
        preCalcMask &= (preCalcMask - 1);
        heroRiverEvals[cardIndex] = eval_7(h0, h1, b0, b1, b2, b3, deck[cardIndex]);
    }
    
    int betterThan = 0; // opponent hand is weaker
    int worseThan = 0; // opponent hand is stronger
    int equal = 0; // opponent hand is equal
    
    int HP[3][3] = {}; // [turnState][finalState]
    int handPotentialTotal[3] = {};
    
    // volatility accumulators (HS on river)
    int hsWin[52] = {};
    int hsTie[52] = {};
    int hsTotal[52] = {};
    
    // HS calculation
    while (mask) {
        int villainIndex1 = __builtin_ctzll(mask);
        int villainCard1 = deck[villainIndex1];
        mask &= (mask - 1);
        
        uint64_t mask2 = mask;
        while(mask2) {
            int villainIndex2 = __builtin_ctzll(mask2);
            int villainCard2 = deck[villainIndex2];
            mask2 &= (mask2 - 1);
            
            int villainScore = eval_6(b0, b1, b2, b3, villainCard1, villainCard2); // evaluate opponent on turn
            
            int turnState;
            
            if(selfRank < villainScore) {
                worseThan ++;
                turnState = BEHIND;
            } else if(selfRank > villainScore) {
                betterThan ++;
                turnState = AHEAD;
            } else {
                equal ++;
                turnState = TIED;
            }
            
            handPotentialTotal[turnState]++;
            
            // EHS calculation
            
            uint64_t riverMask = (~usedCards) & ((1ULL << 52) - 1); // init mask of available cards to draw river from
            riverMask &= ~(1ULL << villainIndex1);
            riverMask &= ~(1ULL << villainIndex2); // remove both villain cards
            
            while(riverMask) {
                int riverCardIndex = __builtin_ctzll(riverMask); // draw river card
                int riverCard = deck[riverCardIndex]; // draw turn card
                riverMask &= (riverMask - 1); // remove used turn card from mask
                
                int selfBest = heroRiverEvals[riverCardIndex];
                int villainBest = eval_7(villainCard1, villainCard2, b0, b1, b2, b3, riverCard); // calculate opponent score
                
                int finalState;
                
                if(selfBest < villainBest) {
                    finalState = 2;
                } else if(selfBest > villainBest) {
                    hsWin[riverCardIndex]++;
                    finalState = 0;
                } else {
                    hsTie[riverCardIndex]++;
                    finalState = 1;
                }
                
                hsTotal[riverCardIndex]++;
                HP[turnState][finalState] ++;
            }
        }
    }
    
    // compute HS per turn
    float hsRiver[52];
    int hsCount = 0;
    
    for (int i = 0; i < 52; ++i) {
        if (hsTotal[i] > 0) {
            hsRiver[hsCount++] =
                (hsWin[i] + 0.5f * hsTie[i]) / hsTotal[i];
        }
    }
    
    // compute volatility
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
        
    // hand strength computation
    float handStrength = (betterThan + 0.5f * equal) / (betterThan + worseThan + equal);
    
    float PpotDenominator = handPotentialTotal[2] + handPotentialTotal[1];
    float NpotDenominator = handPotentialTotal[0] + handPotentialTotal[1];

    float Ppot = 0.f;
    float Npot = 0.f;
    const float remainingTurnCards = 44.0f;

    // compute potentials if denominators are non-zero
    if (PpotDenominator > 0.f) {
        Ppot = (HP[2][0] + HP[2][1]/2.f + HP[1][0]/2.f) / (PpotDenominator * remainingTurnCards);
    }

    if (NpotDenominator > 0.f) {
        Npot = (HP[0][2] + HP[0][1]/2.f + HP[1][2]/2.f) / (NpotDenominator * remainingTurnCards);
    }

    return TurnFeatures{ handStrength, Ppot, Npot, volatility };
}

// EHS calculators
// flop overload
float calculateEHS(const std::array<int,2>& hand,
                   const std::array<int,3>& board)
{
    StrengthTransitions sp = calculateFlopStrengthTransitions(hand, board);

    float winNow = sp.hs;
    float improve = (1.f - sp.hs) * sp.ppot;
    float deteriorate = sp.hs * sp.npot;

    return winNow + improve - deteriorate;
}

// turn overload
float calculateEHS(const std::array<int,2>& hand,
                   const std::array<int,4>& board)
{
    StrengthTransitions sp = calculateTurnStrengthTransitions(hand, board);

    float winNow = sp.hs;
    float improve = (1.f - sp.hs) * sp.ppot;
    float deteriorate = sp.hs * sp.npot;

    return winNow + improve - deteriorate;
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
