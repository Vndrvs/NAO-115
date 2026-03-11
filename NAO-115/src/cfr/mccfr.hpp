#pragma once

#include <cstdint>
#include <array>
#include "mccfr_state.hpp"
#include "infoset.hpp"
#include "external/robin_hood.h"

namespace MCCFR {

class Trainer {
private:
    // main strategy table - key: (HistoryHash ^ (BucketId << 32))
    robin_hood::unordered_flat_map<InfosetKey, Infoset, InfosetKeyHasher> infosetMap;

    // get abstraction bucket for current player, current street
    int32_t getBucketId(const MCCFRState& state,
                        const std::array<int, 2>& p0_hand,
                        const std::array<int, 2>& p1_hand,
                        const std::array<int, 5>& board);

    // recursive tree walk
    float traverseExternalSampling(MCCFRState state,
                                   int updatePlayer,
                                   const std::array<int, 2>& p0_hand,
                                   const std::array<int, 2>& p1_hand,
                                   const std::array<int, 5>& board);

public:
    Trainer() = default;

    // main training loop
    void train(int iterations);

    // extract final table size after training
    size_t getNumInfosets() const {
        return infosetMap.size();
    }
};

}
