#include "hand_abstraction.hpp"

enum HandState : int {
    AHEAD  = 0,
    TIED   = 1,
    BEHIND = 2
};

namespace Eval {

// function to calculate EHS variables for flop
StrengthTransitions calculateFlopStrengthTransitions(const std::array<int, 2>& hand,
                           const std::array<int, 3>& board) {
    
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

    return StrengthPotentials{ handStrength, Ppot, Npot, volatility };
}

// function to calculate EHS variables for turn
StrengthTransitions calculateTurnStrengthTransitions(const std::array<int, 2>& hand,
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

    return StrengthTransitions{ handStrength, Ppot, Npot, volatility };
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

}
