#pragma once
#include <string>
#include <optional>
#include <cstdint>

// created namespace for easier categorization for later
namespace Preflop {

// tiny box that holds info about a hand
struct EncodedHand {
    uint8_t hiRank;   // this is the bigger card number (like A = 13)
    uint8_t loRank;   // this is the smaller one
    bool suited;      // true if both cards got the same suit
    
    // checks if the 2 cards are the same rank (like 'JJ' or '77')
    bool isPair() const {
        return hiRank == loRank;
    }
    
    // checks if both cards are same suit (like both hearts)
    bool isSuited() const {
        return suited;
    }
    
    // checks if the ranks are right next to each other (like '98' or 'QJ')
    bool isConnector() const {
        return (hiRank - loRank) == 1;
    }
};

// this thing takes a string like 'AsKs' and tries to turn it into a hand
std::optional<EncodedHand> convertHandFormat(const std::string& input);

// this turns the hand into a number (so we can find it fast in a list)
int handToIndex(const EncodedHand& hand);

}
