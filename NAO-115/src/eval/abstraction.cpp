#pragma once

#include "abstraction.hpp"
#include "evaluator.hpp"

/*
 
Effective Hand Strength algorithm derived from research paper: Opponent Modeling in Poker (1998)
by computer scientists Darse Billings, Denis Papp, Jonathan Schaeffer and Duane Szafron
 
My quick pseudo recap:
 
CalculateHandStrength(pocketCards[2], boardCards[3]) {

     flopBoard1 = boardCards[0]
     flopBoard2 = boardCards[1]
     flopBoard3 = boardCards[2]
     pocketCard1 = pocketCards[0]
     pocketCard2 = pocketCards[1]

     selfRank = eval_5(flopBoard1..., pocketCard2)

     var betterThan = 0 // opponent hand is weaker
     var worseThan = 0 // opponent hand is stronger
     var tied = 0 // opponent hand has equal strength
     handStrength = 0
     
     for[choose 2 of 47] {
         
         opponentCard1 = opponentCards[i][0]
         opponentCard2 = opponentCards[i][1]

         opponentRank = eval_5(flopCard1..., opponentCard2)
         
         if(selfRank > opponentRank) {
             betterThan++
         } else if(opponentRank > selfRank) {
             worseThan++
         } else {
             tied++
         }
     }

     handStrength = (better + 0.5 * tied) / float(total)
     return handStrength
}

*/

namespace Eval {

double calculateHandStrength(const std::array<int, 2>& hand,
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
    int betterThan = 0; // opponent hand is weaker
    int worseThan = 0; // opponent hand is stronger
    int equal = 0; // opponent hand is equal
    int handStrength = 0;
        
    uint64_t mask2;

    while (mask) {
        int villainCard1 = deck[__builtin_ctzll(mask)]; // set lowest available card to villain 1
        mask &= (mask - 1); // clear first opponent card
        mask2 = mask; // form new mask with the remaining available cards
        while(mask2) {
            int villainCard2 = deck[__builtin_ctzll(mask2)]; // set lowest available card to villain 2
            int villainScore = eval_5(b0, b1, b2, villainCard1, villainCard2); // now we can eval opponent score
            
            if(selfRank < villainScore) {
                worseThan += 1;
            } else if(selfRank > villainScore) {
                betterThan += 1;
            } else {
                equal += 1;
            }
            
            mask2 &= (mask2 - 1); // clear second opponent card
        }
    }
    
    int total = betterThan + worseThan + equal;
    
    // hand strength computation
    handStrength = (betterThan + 0.5 * equal) / static_cast<double>(betterThan + worseThan + equal);
    return handStrength;
}










double calculateFlopFeatures(const std::array<int, 2>& hand,
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
    int betterThan = 0; // opponent hand is weaker
    int worseThan = 0; // opponent hand is stronger
    int equal = 0; // opponent hand is equal
    
    int handStrength = 0;
    int HP[3][3];
        
    uint64_t mask2;
    uint64_t remainingMask = (~usedCards) & ((1ULL << 52) - 1);
    uint64_t riverMask;

    while (mask) {
        int villainCard1 = deck[__builtin_ctzll(mask)]; // set lowest available card to villain 1
        mask &= (mask - 1); // clear first opponent card
        mask2 = mask; // form new mask with the remaining available cards
        remainingMask &= ~(1ULL << villainCard1);
        
        while(mask2) {
            int villainCard2 = deck[__builtin_ctzll(mask2)]; // set lowest available card to villain 2
            remainingMask &= ~(1ULL << villainCard2);
            int villainScore = eval_5(b0, b1, b2, villainCard1, villainCard2); // now we can eval opponent score
            
            if(selfRank < villainScore) {
                worseThan += 1;
            } else if(selfRank > villainScore) {
                betterThan += 1;
            } else {
                equal += 1;
            }
            
            uint64_t turnMask = remainingMask;
            while(turnMask) {
                int turnCard = deck[__builtin_ctzll(turnMask)];
                turnMask &= (turnMask - 1);
                
                uint64_t riverMask = turnMask;
                while(riverMask) {
                    int riverCard = deck[__builtin_ctzll(riverMask)];
                    
                 
                    riverMask &= (riverMask - 1);
                }
            }

            
            mask2 &= (mask2 - 1); // clear second opponent card
        }
    }
    
    int total = betterThan + worseThan + equal;
    
    // hand strength computation
    handStrength = (betterThan + 0.5 * equal) / static_cast<double>(betterThan + worseThan + equal);
    return handStrength;
}


}
