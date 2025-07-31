// test_preflop_encoder.cpp

#include "preflop_encoder.hpp"
#include "card_utils.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm> // for std::max, std::min

// check if two ints r the same, print fail or pass with message
void assertEqual(int actual, int expected, const std::string& context) {
    if (actual != expected) {
        std::cout << "[fail] " << context << " — expected: " << expected << ", got: " << actual << "\n";
    } else {
        std::cout << "[pass] " << context << " == " << expected << "\n";
    }
}

// same as above but for bools
void assertEqual(bool actual, bool expected, const std::string& context) {
    if (actual != expected) {
        std::cout << "[fail] " << context << " — expected: " << (expected ? "true" : "false")
                  << ", got: " << (actual ? "true" : "false") << "\n";
    } else {
        std::cout << "[pass] " << context << " == " << (expected ? "true" : "false") << "\n";
    }
}

// test one hand string with expected hi, lo, suited, and index
void testHandEncoding(const std::string& handStr,
                      int expectedHi,
                      int expectedLo,
                      bool expectedSuited,
                      int expectedIndex) {
    auto encoded = Preflop::convertHandFormat(handStr);
    if (!encoded) {
        std::cout << "[fail] couldnt parse valid hand: " << handStr << "\n";
        return;
    }

    assertEqual(encoded->hiRank, expectedHi, handStr + " hiRank");
    assertEqual(encoded->loRank, expectedLo, handStr + " loRank");
    assertEqual(encoded->suited, expectedSuited, handStr + " suited");

    int actualIndex = Preflop::handToIndex(*encoded);
    assertEqual(actualIndex, expectedIndex, handStr + " index");
}

// test some busted hand strings, expect failure to parse
void testConvertHandFormatFailures() {
    using namespace Preflop;

    std::vector<std::string> badInputs = {
        "", "A", "AA", "As", "AsKsX", "1c7d", "7x7d", "7c7c7d",
        "7c7c ", "7c7c\n", "7c7c\0", "7c7c7", "7c7d7", "7c7d7x", "7c7d1x",
        "7c7c7c", "AcAc" // same card twice, no no
    };

    for (const auto& input : badInputs) {
        auto encoded = convertHandFormat(input);
        if (encoded) {
            std::cout << "[fail] expected fail for input: '" << input << "'\n";
        } else {
            std::cout << "[pass] correctly rejected input: '" << input << "'\n";
        }
    }
}

// test all 169 combos, make sure encode and index works
void testAll169Hands() {
    using namespace Preflop;

    const std::string ranks = "23456789TJQKA";
    const std::string suits = "cdhs";
    int failCount = 0;
    int testCount = 0;

    // loop all combos with no exact duplicates
    for (char r1 : ranks) {
        for (char r2 : ranks) {
            for (char s1 : suits) {
                for (char s2 : suits) {
                    if (r1 == r2 && s1 == s2) continue; // no same card twice

                    std::string handStr = {r1, s1, r2, s2};

                    auto encoded = convertHandFormat(handStr);
                    ++testCount;

                    if (!encoded) {
                        std::cout << "[fail] convertHandFormat gave null for valid hand: " << handStr << "\n";
                        ++failCount;
                        continue;
                    }

                    int v1 = CardUtils::getRankValue(r1);
                    int v2 = CardUtils::getRankValue(r2);
                    int expectedHi = std::max(v1, v2);
                    int expectedLo = std::min(v1, v2);
                    bool expectedSuited = (s1 == s2);

                    if (encoded->hiRank != expectedHi || encoded->loRank != expectedLo || encoded->suited != expectedSuited) {
                        std::cout << "[fail] mismatch for " << handStr
                                  << " | expected hi=" << expectedHi << ", lo=" << expectedLo
                                  << ", suited=" << expectedSuited
                                  << " | got hi=" << static_cast<int>(encoded->hiRank)
                                  << ", lo=" << static_cast<int>(encoded->loRank)
                                  << ", suited=" << encoded->suited << "\n";
                        ++failCount;
                        continue;
                    }

                    int index = handToIndex(*encoded);
                    if (index < 0 || index >= 169) {
                        std::cout << "[fail] bad index " << index << " for hand " << handStr << "\n";
                        ++failCount;
                    }
                }
            }
        }
    }

    std::cout << "[info] tested " << testCount << " two-card hands\n";
    if (failCount == 0) {
        std::cout << "[pass] all hands encoded and indexed right!\n";
    } else {
        std::cout << "[fail] " << failCount << " fails during 169-hand test\n";
    }
}

int main() {
    std::cout << "running hand encoding & index tests:\n";

    // quick checks for some hands
    testHandEncoding("AsKs", 13, 12, true, 13);
    testHandEncoding("AhKd", 13, 12, false, 91);
    testHandEncoding("7c7d", 6, 6, false, 5);
    testHandEncoding("2c3c", 2, 1, true, 90);
    testHandEncoding("asks", 13, 12, true, 13);
    testHandEncoding("KdAh", 13, 12, false, 91);

    std::cout << "\nrunning weird input tests:\n";
    testConvertHandFormatFailures();

    std::cout << "\nrunning big 169-hand tests:\n";
    testAll169Hands();

    return 0;
}
