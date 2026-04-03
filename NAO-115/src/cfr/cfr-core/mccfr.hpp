#pragma once

#include <cstdint>
#include <array>
#include <random>
#include "mccfr_state.hpp"
#include "hand-bucketing/mapping_engine.hpp"
#include "infoset.hpp"
#include "cfr/external/robin_hood.h"

namespace MCCFR {

class Trainer {
private:
    Bucketer::IsomorphismEngine mappingEngine;
    
    // main strategy table - key: (HistoryHash ^ (BucketId << 32))
    robin_hood::unordered_flat_map<InfosetKey, Infoset, InfosetKeyHasher> infosetMap;
    
    uint64_t targetNodeBudget;
    uint64_t nodesTouched;
    
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist;
    bool traceMode;

    // get abstraction bucket for acting player
    int32_t getBucketId(const MCCFRState& state, const std::array<int, 2>& hand, const std::array<int, 5>& board);

    // recursive tree walk
    float traverseExternalSampling(const MCCFRState& state,
                                    int updatePlayer,
                                    const std::array<int, 2>& p0_hand,
                                    const std::array<int, 2>& p1_hand,
                                    const std::array<int, 5>& board);

public:
    int threadId;
    
    Trainer(uint64_t seed = 1337);
    uint64_t iterations = 0;

    // main training loop
    void train(uint64_t nodeBudget);

    // extract final table size after training
    size_t getNumInfosets() const {
        return infosetMap.size();
    }
    
    // testing
    void setTraceMode(bool enabled) { traceMode = enabled; }

    robin_hood::unordered_flat_map<InfosetKey, Infoset, InfosetKeyHasher>& getInfosetMap() {
        return infosetMap;
    }

    int32_t getBucketIdPublic(const MCCFRState& state,
                              const std::array<int, 2>& hand,
                              const std::array<int, 5>& board) {
        return getBucketId(state, hand, board);
    }
    
    uint64_t getNodesTouched() const {
        return nodesTouched;
    }
    
    void resetNodesTouched() {
        nodesTouched = 0;
    }
};

}
