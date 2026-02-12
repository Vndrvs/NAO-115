/*
 * A part of this evaluator logic is derived from "Cactus Kev's Poker Library".
 * * Original Source: http://suffe.cool/poker/code/
 * Original Author: Kevin L. Suffecool
 * License: GNU General Public License v3.0 (GPL-3.0)
 * * MODIFICATIONS (by Vndrvs, 2025-2026):
 * - refactored into C++ namespace 'Eval'.
 * - kept 5 card evaluator logic, while implementing my own 6-7 card eval steps on top
 */

#include "evaluator.hpp"
#include "tables.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>

namespace Eval {

// standard 52-card poker deck
int deck[52];
static bool is_initialized = false;

// 1. we assign a prime number to each and every rank in the deck

// 2. we format each card in a 32 bit wide integer, filling up with the unused digits with zeros
// Format: 00000000 00000000 00000000 00000000
//         xxxbbbbb bbbbbbbb cdhsrrrr xxpppppp
// bits 0 - 5: prime number of rank (p)
// bits  8 - 11:  rank of card (r)
// bits 12 - 15: suit bitmask of card (cdhs - clubs, diamonds, hearts, spades)
// bits 16 - 28: rank bitmask of card (b)
// unused bits (x)

int make_card(int rank, int suit) {
    return typePrimes[rank] | (rank << 8) | ((1 << suit) << 12) | ((1 << 16) << rank);
}

void initialize() {
    if (is_initialized) return;
    for(int i=0; i<52; i++) {
        deck[i] = make_card(i/4, i%4);
    }
    is_initialized = true;
}

// perfect hash function, credit to Paul Senzee: https://senzee.blogspot.com/2006/06/some-perfect-hash.html

inline int find_fast(unsigned u) {
    unsigned a, b, r;
    u += 0xe91aaa35;
    u ^= u >> 16;
    u += u << 8;
    u ^= u >> 4;
    b  = (u >> 8) & 0x1ff;
    a  = (u + (u << 2)) >> 19;
    r  = a ^ hashAdjust[b];
    return (int)r;
}


// I kept Kev's awesome evaluator logic, which basically identifies 3 hand categories
// 1. Hand is a flush 
// -> Integer value of the rank bitmask is used as the index for a lookup in the flush ranks table
// 2. Hand contains 5 unique cards - so can be either of the following: Straight or no combo at all
// -> Integer value of the rank bitmask  is used as the index for a lookup in the unique ranks table
// 3. Hand doesn't belong to the first 2 buckets - so has a pair, two pairs, three of a kind, quads, or full house


inline int eval_5(int c1, int c2, int c3, int c4, int c5) {
    int q = (c1 | c2 | c3 | c4 | c5) >> 16;
    short s;

    // 1. check for flushes
    if (c1 & c2 & c3 & c4 & c5 & 0xF000) {
        return HAND_RANKS - flushRanks[q];
    }

    // 2. check for unique ranks (straight or high card)
    if ((s = uniqueRanks[q])) {
        return HAND_RANKS - s;
    }

    // 3. check for all other combinations
    int product = (c1 & 0xFF) * (c2 & 0xFF) * (c3 & 0xFF) * (c4 & 0xFF) * (c5 & 0xFF);
    return HAND_RANKS - hashRanks[find_fast(product)];
}

// here comes the 7-card evaluator
// pure bitwise logic function to find a straight flush fast

inline int find_straight_high(int mask) {
    // finds any sequence of 5 consecutive ranks
    int tmp = mask & (mask << 1) & (mask << 2) & (mask << 3) & (mask << 4);
    
    // if found
    if (tmp != 0) {
        // get the index of the highest set bit - which is the top rank of the straight
        // eg. case 'A-K-Q-J-T': returns 12
        int bit = 31 - __builtin_clz(tmp);
        return bit;
    }
    
    // special case A-2-3-4-5 ("ace-low straight"): returns 3 (rank of 5)
    if ((mask & 0x100F) == 0x100F) return 3;
    
    return -1;
}

int eval_7(int c1, int c2, int c3, int c4, int c5, int c6, int c7) {
    
    // array holding the 7 cards
    int h[7];
    
    // look up the Kev encoding per card and store it in the array
    h[0] = deck[c1];
    h[1] = deck[c2];
    h[2] = deck[c3];
    h[3] = deck[c4];
    h[4] = deck[c5];
    h[5] = deck[c6];
    h[6] = deck[c7];

    // count summary variables
    // eg. "As-Ks-Qs-Js-Ts-2d-9c"
    
    // how many cards of given suit
    // suit_counts[spades] = 5
    int suit_counts[4] = {0};
    // rank bits turned on for that suit
    // suit_masks[spades] = bits on for A,K,Q,J,T
    int suit_masks[4] = {0};
    // OR for all ranks
    // full_rank_mask = bits on for A,K,Q,J,T,9,2
    int full_rank_mask = 0;
    
    for(int i=0; i<7; ++i) {
        int rank_bit = (h[i] >> 16);
        full_rank_mask |= rank_bit;
        
        if (h[i] & 0x1000) { suit_counts[0]++; suit_masks[0] |= rank_bit; }
        else if (h[i] & 0x2000) { suit_counts[1]++; suit_masks[1] |= rank_bit; }
        else if (h[i] & 0x4000) { suit_counts[2]++; suit_masks[2] |= rank_bit; }
        else { suit_counts[3]++; suit_masks[3] |= rank_bit; }
    }
    
    // early exit optimization - does any suit have 5 or more cards?
    for(int i=0; i<4; i++) {
        if (suit_counts[i] >= 5) {
            // if yes,
            int mask = suit_masks[i];
            
            // we should find rank of the straight flush that we found
            int sf_top = find_straight_high(mask);
            
            // build the 5-bit rank mask and look it up in flushRanks, given by Kev's code previously
            if (sf_top != -1) {
                int sf_mask = 0;
                if (sf_top == 3) sf_mask = 0x100F;
                else sf_mask = (0x1F << (sf_top - 4));
                
                // return the result early
                return HAND_RANKS - flushRanks[sf_mask];
            }
            
            // walk ranks high to low, pick top 5 ranks from the flush suit
            int final_mask = 0;
            int count = 0;
            for(int r=12; r>=0; r--) {
                if ((mask >> r) & 1) {
                    final_mask |= (1 << r);
                    count++;
                    // break after found the top 5
                    if (count == 5) break;
                }
            }
            // lookup result in same flush array
            return HAND_RANKS - flushRanks[final_mask];
        }
    }
    
    // if all
    if (__builtin_popcount(full_rank_mask) == 7) {
      
        int str_top = find_straight_high(full_rank_mask);
        if (str_top != -1) {
            int str_mask = 0;
            if (str_top == 3) str_mask = 0x100F;
            else str_mask = (0x1F << (str_top - 4));
            
            return HAND_RANKS - uniqueRanks[str_mask];
        }
        
        int mask = 0;
        int bits_found = 0;
        for (int r=12; r>=0; r--) {
            if ((full_rank_mask >> r) & 1) {
                mask |= (1 << r);
                bits_found++;
                if (bits_found == 5) break;
            }
        }
        return HAND_RANKS - uniqueRanks[mask];
    }
    // if we fail to detect a flush, or a unique series of ranks, fallback to Kev's permutation-based calculator
    int max_score = 0;
    for (int i = 0; i < 21; i++) {
        int score = eval_5(
            h[permutations[i][0]], h[permutations[i][1]], h[permutations[i][2]],
            h[permutations[i][3]], h[permutations[i][4]]
        );
        if (score > max_score) max_score = score;
    }
    return max_score;
}

int parseCard(const std::string& cardStr) {
    if (cardStr.size() < 2) return -1;
    char r = cardStr[0];
    char s = cardStr[1];
    // rank index
    int ri = 0;
    if (isdigit(r)) ri = r - '2';
    else if (r == 'T') ri = 8;
    else if (r == 'J') ri = 9;
    else if (r == 'Q') ri = 10;
    else if (r == 'K') ri = 11;
    else if (r == 'A') ri = 12;
    // suit index
    int si = 0;
    if (s == 'c') si = 0;
    else if (s == 'd') si = 1;
    else if (s == 'h') si = 2;
    else if (s == 's') si = 3;
    return ri * 4 + si;
}

// BUCKETER WRAPPERS

int evaluate5(const std::vector<int>& cards) {
    return eval_5(
        deck[cards[0]], deck[cards[1]], deck[cards[2]],
        deck[cards[3]], deck[cards[4]]
    );
}

// 6 cards wrapper - brute force choose 5 (average speed probably faster than bitmasking)
int eval_6(int c1, int c2, int c3, int c4, int c5, int c6) {
    int k[6] = { deck[c1], deck[c2], deck[c3], deck[c4], deck[c5], deck[c6] };
    int best = 0;
    for (int drop = 0; drop < 6; ++drop) {
        int idx = 0, hand[5];
        for (int i = 0; i < 6; ++i) if (i != drop) hand[idx++] = k[i];
        int score = eval_5(hand[0], hand[1], hand[2], hand[3], hand[4]);
        if (score > best) best = score;
    }
    return best;
}

}


