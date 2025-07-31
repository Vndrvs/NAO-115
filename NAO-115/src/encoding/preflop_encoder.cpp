#include "preflop_encoder.hpp"
#include "card_utils.hpp"
#include <cctype>

namespace Preflop {

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
    
    constexpr int NUM_RANKS = 13;
    
    uint8_t hi = hand.hiRank;
    uint8_t lo = hand.loRank;
    
    bool suited = hand.suited;
    
    // if it's a pocket pair (like '33' or 'AA'), just return the index of the specific rank
    if (hi == lo) {
        return hi - 1;
    }
    
    // in case if suited (like 'Ks7s'), we loop through suited combos
    else if (suited) {
        
        int suitedIndex = 0;
        
        for (int i = NUM_RANKS; i >= 1; --i) {
            for (int j = i - 1; j >= 1; --j) {
                
                if (i == hi && j == lo) {
                    return 13 + suitedIndex;
                }
                
                ++suitedIndex;
            }
        }
    }
    
    // else it's offsuit (like 'Jd8h'), we use different section of the grid
    else {
        
        int offsuitIndex = 0;
        
        for (int i = NUM_RANKS; i >= 1; --i) {
            for (int j = i - 1; j >= 1; --j) {
                
                if (i == hi && j == lo) {
                    return 91 + offsuitIndex;
                }
                
                ++offsuitIndex;
            }
        }
    }
    
    // hopefully nothing breaks, but if it does, this notifies us
    return -1;
}

}
