#include "preflop_encoder.hpp"
#include "card_utils.hpp"
#include <cctype>

namespace Preflop {

static constexpr int PREFLOP_LOOKUP[13][13] = {
    {0, 90, 89, 87, 84, 80, 75, 69, 62, 54, 45, 35, 24},   // 2s
    {168, 1, 88, 86, 83, 79, 74, 68, 61, 53, 44, 34, 23},  // 3s
    {167, 166, 2, 85, 82, 78, 73, 67, 60, 52, 43, 33, 22}, // 4s
    {165, 164, 163, 3, 81, 77, 72, 66, 59, 51, 42, 32, 21}, // 5s
    {162, 161, 160, 159, 4, 76, 71, 65, 58, 50, 41, 31, 20}, // 6s
    {158, 157, 156, 155, 154, 5, 70, 64, 57, 49, 40, 30, 19}, // 7s
    {153, 152, 151, 150, 149, 148, 6, 63, 56, 48, 39, 29, 18}, // 8s
    {147, 146, 145, 144, 143, 142, 141, 7, 55, 47, 38, 28, 17}, // 9s
    {140, 139, 138, 137, 136, 135, 134, 133, 8, 46, 37, 27, 16}, // Ts
    {132, 131, 130, 129, 128, 127, 126, 125, 124, 9, 36, 26, 15}, // Js
    {123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 10, 25, 14}, // Qs
    {113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 11, 13}, // Ks
    {102, 101, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 12}          // As
};

std::optional<EncodedHand> convertHandFormat(const std::string& input) {
    
    // checks if input is exactly 4 letters like 'AsKs'
    if (input.size() != 4) {
        return std::nullopt;
    }
    
    // get first card rank and suit
    char r1 = CardUtils::normalizeRank(input[0]);
    char s1 = CardUtils::normalizeSuit(input[1]);
    
    // get second card rank and suit
    char r2 = CardUtils::normalizeRank(input[2]);
    char s2 = CardUtils::normalizeSuit(input[3]);
    
    // turn ranks into numbers of the linear index ordering (A=13, K=12...)
    int val1 = CardUtils::getRankValue(r1);
    int val2 = CardUtils::getRankValue(r2);
    
    // check if suits are off
    if (!CardUtils::isValidSuit(s1) || !CardUtils::isValidSuit(s2)) {
        return std::nullopt;
    }
    
    // check if ranks are off
    if (val1 == 0 || val2 == 0) {
        return std::nullopt;
    }
    
    // two exactly matching cards cannot occur inside one deck of cards
    if (r1 == r2 && s1 == s2) {
        return std::nullopt;
    }
    
    int hi = val1;
    int lo = val2;
    
    // put the bigger rank first
    if (lo > hi) {
        std::swap(hi, lo);
    }
    
    // create the hand object to hold stuff
    EncodedHand hand;
    hand.hiRank = static_cast<uint8_t>(hi);
    hand.loRank = static_cast<uint8_t>(lo);
    hand.suited = (s1 == s2); // throws true if suits match
    
    return hand;
}


int handToIndex(const EncodedHand& hand) {
    // Rank values are 1-13, but array indices are 0-12
    int hi = hand.hiRank - 1;
    int lo = hand.loRank - 1;

    // FIX: Suited hands/Pairs use the UPPER triangle (lo, hi)
    if (hand.hiRank == hand.loRank || hand.suited) {
        return PREFLOP_LOOKUP[lo][hi];
    }
    
    // FIX: Offsuit hands use the LOWER triangle (hi, lo)
    return PREFLOP_LOOKUP[hi][lo];
}

}
