#pragma once

#include <cstdint>
#include <array>
#include <random>
#include "mccfr_state.hpp"
#include "hand-bucketing/mapping_engine.hpp"
#include "infoset.hpp"
#include "external/robin_hood.h"

namespace MCCFR {

class Trainer {
private:
    Bucketer::IsomorphismEngine mappingEngine;
    
    // main strategy table - key: (HistoryHash ^ (BucketId << 32))
    robin_hood::unordered_flat_map<InfosetKey, Infoset, InfosetKeyHasher> infosetMap;
    
    int totalIterations;
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist;

    // get abstraction bucket for acting player
    int32_t getBucketId(const MCCFRState& state, const std::array<int, 2>& hand, const std::array<int, 5>& board);

    // recursive tree walk
    float traverseExternalSampling(const MCCFRState& state,
                                    int updatePlayer,
                                    const std::array<int, 2>& p0_hand,
                                    const std::array<int, 2>& p1_hand,
                                    const std::array<int, 5>& board);

public:
    Trainer();

    // main training loop
    void train(int iterations);

    // extract final table size after training
    size_t getNumInfosets() const {
        return infosetMap.size();
    }
};

}
