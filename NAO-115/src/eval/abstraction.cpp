#include "abstraction.hpp"
#include "evaluator.hpp"

/*
 
Effective Hand Strength algorithm derived from research paper: Opponent Modeling in Poker (1998)
by computer scientists Darse Billings, Denis Papp, Jonathan Schaeffer and Duane Szafron

*/

namespace Eval {

float calculateFlopHandStrength(const std::array<int, 2>& hand,
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
    float betterThan = 0.0f; // opponent hand is weaker
    float worseThan = 0.0f; // opponent hand is stronger
    float equal = 0.0f; // opponent hand is equal
        
    uint64_t mask2;

    while (mask) {
        int villainCard1 = deck[__builtin_ctzll(mask)]; // set lowest available card to villain 1
        mask &= (mask - 1); // clear first opponent card
        mask2 = mask; // form new mask with the remaining available cards
        while(mask2) {
            int villainCard2 = deck[__builtin_ctzll(mask2)]; // set lowest available card to villain 2
            int villainScore = eval_5(b0, b1, b2, villainCard1, villainCard2); // now we can eval opponent score
            
            if(selfRank < villainScore) {
                worseThan += 1.0f;
            } else if(selfRank > villainScore) {
                betterThan += 1.0f;
            } else {
                equal += 1.0f;
            }
            
            mask2 &= (mask2 - 1); // clear second opponent card
        }
    }
        
    // hand strength computation
    return (betterThan + 0.5f * equal) / (betterThan + worseThan + equal);
}

float calculateTurnHandStrength(const std::array<int, 2>& hand,
                                const std::array<int, 4>& board) {
    
    uint64_t usedCards = 0;

    // flip corresponding used bits in the bitmask
    usedCards |= (1ULL << hand[0]);
    usedCards |= (1ULL << hand[1]);
    usedCards |= (1ULL << board[0]);
    usedCards |= (1ULL << board[1]);
    usedCards |= (1ULL << board[2]);
    usedCards |= (1ULL << board[3]);
        
    // 46 out of 52 bits will be flipped according to which cards are still available
    uint64_t mask = (~usedCards) & ((uint64_t(1) << 52) - 1);
    
    // get the encoded card formats for evaluation
    int h0 = deck[hand[0]];
    int h1 = deck[hand[1]];
    int b0 = deck[board[0]];
    int b1 = deck[board[1]];
    int b2 = deck[board[2]];
    int b3 = deck[board[3]];
    
    int selfRank = eval_6(h0, h1, b0, b1, b2, b3);
    float betterThan = 0.0f; // opponent hand is weaker
    float worseThan = 0.0f; // opponent hand is stronger
    float equal = 0.0f; // opponent hand is equal
    
    uint64_t mask2;
    
    while (mask) {
        int villainCard1 = deck[__builtin_ctzll(mask)]; // set lowest available card to villain 1
        mask &= (mask - 1); // clear first opponent card
        mask2 = mask; // form new mask with the remaining available cards
        while(mask2) {
            int villainCard2 = deck[__builtin_ctzll(mask2)]; // set lowest available card to villain 2
            int villainScore = eval_6(b0, b1, b2, b3, villainCard1, villainCard2); // now we can eval opponent score
            
            if(selfRank < villainScore) {
                worseThan += 1.0f;
            } else if(selfRank > villainScore) {
                betterThan += 1.0f;
            } else {
                equal += 1.0f;
            }
            
            mask2 &= (mask2 - 1); // clear second opponent card
        }
    }
        
    // hand strength computation
    return (betterThan + 0.5f * equal) / (betterThan + worseThan + equal);
}

// function to calculate Ppot2 and Npot2
BaseFeatures calculateFlopFeaturesTwoAhead(const std::array<int, 2>& hand, const std::array<int, 3>& board) {
    
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
    
    float betterThan = 0.f; // opponent hand is weaker
    float worseThan = 0.f; // opponent hand is stronger
    float equal = 0.f; // opponent hand is equal
    
   float HP[3][3] = {}; // [flopState][finalState]
    float handPotentialTotal[3] = {};
        
    while (mask) {
        int villainIndex1 = __builtin_ctzll(mask);       // 0..51 index for bitmask
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
                worseThan += 1.f;
                flopState = 2;
            } else if(selfRank > villainScore) {
                betterThan += 1.f;
                flopState = 0;
            } else {
                equal += 1.f;
                flopState = 1;
            }
            
            handPotentialTotal[flopState]+= 1.f;
            
            uint64_t turnMask = (~usedCards) & ((1ULL << 52) - 1); // init mask of available cards to draw turn from
            turnMask &= ~(1ULL << villainIndex1);
            turnMask &= ~(1ULL << villainIndex2); // remove both villain cards
            
            while(turnMask) {
                int turnCardIndex = __builtin_ctzll(turnMask); // draw turn card
                int turnCard = deck[turnCardIndex]; // draw turn card
                turnMask &= (turnMask - 1); // remove used turn card from mask
                
                uint64_t riverMask = turnMask; // init mask of available cards to draw river from
                while(riverMask) {
                    int riverCardIndex = __builtin_ctzll(riverMask); // draw river card
                    int riverCard = deck[riverCardIndex];
                    riverMask &= (riverMask - 1); // remove used river card from mask
                    
                    int selfBest = eval_7(h0, h1, b0, b1, b2, turnCard, riverCard); // calculate our score
                    int villainBest = eval_7(villainCard1, villainCard2, b0, b1, b2, turnCard, riverCard); // calculate opponent score
                    
                    int finalState;
                    
                    if(selfBest < villainBest) {
                        finalState = 2;
                    } else if(selfBest > villainBest) {
                        finalState = 0;
                    } else {
                        finalState = 1;
                    }
                    
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

    // compute potentials if denominators are non-zero
    if (PpotDenominator > 0.f) {
        Ppot = (HP[2][0] + HP[2][1]/2.f + HP[1][0]/2.f) / PpotDenominator;
    }

    if (NpotDenominator > 0.f) {
        Npot = (HP[0][2] + HP[0][1]/2.f + HP[1][2]/2.f) / NpotDenominator;
    }

    return BaseFeatures{ handStrength, handStrength * handStrength, Ppot, Npot };
}

// function to calculate Ppot1 and Npot1
BaseFeatures calculateFlopFeaturesFast(const std::array<int, 2>& hand,
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
    
    int heroTurnEvals[52];
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
                flopState = 2;
            } else if(selfRank > villainScore) {
                betterThan ++;
                flopState = 0;
            } else {
                equal ++;
                flopState = 1;
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
                    finalState = 2;
                } else if(selfBest > villainBest) {
                    finalState = 0;
                } else {
                    finalState = 1;
                }
                
                HP[flopState][finalState] ++;
            }
        }
    }
        
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

    return BaseFeatures{ handStrength, handStrength * handStrength, Ppot, Npot };
}

BaseFeatures calculateTurnFeaturesFast(const std::array<int, 2>& hand,
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
    
    int heroRiverEvals[52];
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
                turnState = 2;
            } else if(selfRank > villainScore) {
                betterThan ++;
                turnState = 0;
            } else {
                equal ++;
                turnState = 1;
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
                    finalState = 0;
                } else {
                    finalState = 1;
                }
                
                HP[turnState][finalState] ++;
            }
        }
    }
        
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

    return BaseFeatures{ handStrength, handStrength * handStrength, Ppot, Npot };
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

    int topStart = int(0.8 * totalVillainCombos);
    int topEnd = totalVillainCombos;
            
    float eVsRandom = eq(0, totalVillainCombos);
    float eVsTop = eq(topStart, topEnd);
    float eVsMid = eq(midStart, midEnd);
    float eVsBot = eq(botStart, botEnd);
    
    return RiverFeatures { eVsRandom, eVsTop, eVsMid, eVsBot };
}

}
