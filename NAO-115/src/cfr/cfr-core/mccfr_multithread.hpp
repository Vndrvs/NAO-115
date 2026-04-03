#pragma once

#include "mccfr.hpp"
#include <vector>
#include <thread>
#include <cstdint>

namespace MCCFR {

/*
Parallel MCCFR trainer:
 - threads runs 'N/numThreads' iterations in separation
 - threads build / hold their own infoset maps locally
 - after all threads finish, maps are merged into one final map (summing regretSum and strategySum across all threads)
*/

class ParallelTrainer {
public:
    void train(uint64_t totalNodesBudget, int numThreads, uint64_t baseSeed);

    // report how many unique infosets were learned across all threads
    size_t getNumInfosets() const;
    
    // exposes the merged infoset map for downstream evaluation
    auto& getInfosetMap() {
        return mergedMap;
    }
    // read-only ver.
    const auto& getInfosetMap() const {
        return mergedMap;
    }
    uint64_t getTotalNodesTouched() const;

    
    int32_t getBucketIdPublic(const MCCFRState& state,
                               const std::array<int,2>& hand,
                               const std::array<int,5>& board);

private:
    // global, merged infoset map
    robin_hood::unordered_flat_map<InfosetKey, Infoset, InfosetKeyHasher> mergedMap;
    uint64_t totalNodesTouched = 0;

    // merge srcMap into dstMap by adding regretSum and strategySum
    // numActions is taken from whichever map has the entry, if both have the entry, sums are added
    static void mergeMaps(
        robin_hood::unordered_flat_map<InfosetKey, Infoset, InfosetKeyHasher>& dst,
        const robin_hood::unordered_flat_map<InfosetKey, Infoset, InfosetKeyHasher>& src);
};

}
