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

}
