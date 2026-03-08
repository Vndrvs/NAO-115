#include <gtest/gtest.h>
#include "encoding/state_encoder.hpp"
#include "eval/evaluator.hpp"


TEST(SampleTest, BasicAssertions) {
    
    Eval::initialize();
    
    assert(StateEncoder::cardToIndex(Eval::deck[0])  == 0);   // rank 0, suit 0 (2 of clubs)
    assert(StateEncoder::cardToIndex(Eval::deck[1])  == 1);   // rank 0, suit 1 (2 of diamonds)
    assert(StateEncoder::cardToIndex(Eval::deck[4])  == 4);   // rank 1, suit 0 (3 of clubs)
    assert(StateEncoder::cardToIndex(Eval::deck[51]) == 51);  // rank 12, suit 3 (Ace of spades)

    // Test 2: cardToIndex is inverse of deck indexing
    for (int i = 0; i < 52; i++) {
        assert(StateEncoder::cardToIndex(Eval::deck[i]) == i);
    }

    // Test 3: cardsToBitmask single card
    uint64_t mask = StateEncoder::cardsToBitmask(&Eval::deck[0], 1);
    assert(mask == 1ULL);  // bit 0 set

    // Test 4: cardsToBitmask three cards (flop)
    int flop[3] = { Eval::deck[0], Eval::deck[1], Eval::deck[2] };
    uint64_t flopMask = StateEncoder::cardsToBitmask(flop, 3);
    assert(flopMask == 0b111ULL);  // bits 0,1,2 set

    // Test 5: no overlap — each card sets a unique bit
    uint64_t fullDeck = StateEncoder::cardsToBitmask(Eval::deck, 52);
    assert(fullDeck == (1ULL << 52) - 1);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
