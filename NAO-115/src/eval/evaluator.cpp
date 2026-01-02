/*
 * A part of this evaluator is derived from "Cactus Kev's Poker Library".
 * * Original Source: http://suffe.cool/poker/code/
 * Original Author: Kevin L. Suffecool
 * License: GNU General Public License v3.0 (GPL-3.0)
 * * MODIFICATIONS (by Vndrvs, 2025-2026):
 * - refactored into C++ namespace 'Eval'.
 * - kept 5 card evaluator logic, while implementing my own 6-7 card eval steps on top
 */

#include "evaluator.hpp"
#include "tables.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>

namespace Eval {

// standard 52-card poker deck
static int deck[52];
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
        return 7463 - flushRanks[q];
    }

    // 2. check for unique ranks (straight or high card)
    if ((s = uniqueRanks[q])) {
        return 7463 - s;
    }

    // 3. check for all other combinations
    int product = (c1 & 0xFF) * (c2 & 0xFF) * (c3 & 0xFF) * (c4 & 0xFF) * (c5 & 0xFF);
    return 7463 - hashRanks[find_fast(product)];
}

inline int get_straight_flush_rank(int mask) {

    int tmp = mask & (mask << 1) & (mask << 2) & (mask << 3) & (mask << 4);
    
    if (tmp != 0) {
        int bit = 31 - __builtin_clz(tmp);
        return bit;
    }
    
    if ((mask & 0x100F) == 0x100F) return 3;
    
    return -1;
}

int evaluate7(const std::vector<int>& cards) {
    int h[7];
    for(int i=0; i<cards.size(); ++i) h[i] = deck[cards[i]];
    
    int suit_counts[4] = {0};
    int suit_masks[4] = {0};
    int full_rank_mask = 0;
    
    for(int i=0; i<cards.size(); ++i) {
        int rank_bit = (h[i] >> 16);
        full_rank_mask |= rank_bit;
        
        if (h[i] & 0x1000) { suit_counts[0]++; suit_masks[0] |= rank_bit; }
        else if (h[i] & 0x2000) { suit_counts[1]++; suit_masks[1] |= rank_bit; }
        else if (h[i] & 0x4000) { suit_counts[2]++; suit_masks[2] |= rank_bit; }
        else { suit_counts[3]++; suit_masks[3] |= rank_bit; }
    }
    
    for(int i=0; i<4; i++) {
        if (suit_counts[i] >= 5) {
            int mask = suit_masks[i];
            
            int sf_top = get_straight_flush_rank(mask);
            if (sf_top != -1) {

                int sf_mask = 0;
                if (sf_top == 3) sf_mask = 0x100F;
                else sf_mask = (0x1F << (sf_top - 4));
                
                return 7463 - flushRanks[sf_mask];
            }
            
            int final_mask = 0;
            int count = 0;
            for(int r=12; r>=0; r--) {
                if ((mask >> r) & 1) {
                    final_mask |= (1 << r);
                    count++;
                    if (count == 5) break;
                }
            }
            return 7463 - flushRanks[final_mask];
        }
    }
    
    if (__builtin_popcount(full_rank_mask) == cards.size()) {
        int mask = 0;
        int bits_found = 0;
        for (int r=12; r>=0; r--) {
            if ((full_rank_mask >> r) & 1) {
                mask |= (1 << r);
                bits_found++;
                if (bits_found == 5) break;
            }
        }
        return 7463 - uniqueRanks[mask];
    }
    
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
    int ri = 0;
    if (isdigit(r)) ri = r - '2';
    else if (r == 'T') ri = 8;
    else if (r == 'J') ri = 9;
    else if (r == 'Q') ri = 10;
    else if (r == 'K') ri = 11;
    else if (r == 'A') ri = 12;
    int si = 0;
    if (s == 'c') si = 0;
    else if (s == 'd') si = 1;
    else if (s == 'h') si = 2;
    else if (s == 's') si = 3;
    return ri * 4 + si;
}

}
