#pragma once

#include <cctype>
#include <cstdint>

namespace CardUtils {

    // how many letters we can check (ASCII)
    constexpr int MAX_CHAR_LIMIT = 128;

    // numbers and face cards get turned into point values
    // no entry means zero, therefore things like 'x' or '1' won't do
    constexpr int rankValues[MAX_CHAR_LIMIT] = {
        ['2'] = 1,
        ['3'] = 2,
        ['4'] = 3,
        ['5'] = 4,
        ['6'] = 5,
        ['7'] = 6,
        ['8'] = 7,
        ['9'] = 8,
        ['T'] = 9,
        ['J'] = 10,
        ['Q'] = 11,
        ['K'] = 12,
        ['A'] = 13
    };

    // we only accept these suits (slumbot formatting)
    constexpr bool validSuit[MAX_CHAR_LIMIT] = {
        ['c'] = true,
        ['d'] = true,
        ['h'] = true,
        ['s'] = true
    };

    // if rank is lowercase, uppercase it
    inline char normalizeRank(char r) {
        return std::toupper(static_cast<unsigned char>(r));
    }

    // if suit is uppercase, lowercase it
    inline char normalizeSuit(char s) {
        return std::tolower(static_cast<unsigned char>(s));
    }

    // check if suit letter is okay or garbage
    inline bool isValidSuit(char s) {
        auto uc = static_cast<unsigned char>(s);
        return uc < MAX_CHAR_LIMIT && validSuit[uc];
    }

    // turn letter card into number value (or 0 if garbage)
    inline int getRankValue(char r) {
        auto uc = static_cast<unsigned char>(r);
        return (uc < MAX_CHAR_LIMIT) ? rankValues[uc] : 0;
    }

    // check whether full card is OK or something is off
    inline bool isValidCard(char rank, char suit) {
        return getRankValue(rank) != 0 && isValidSuit(suit);
    }
}
