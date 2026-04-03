#include "mccfr_multithread.hpp"
#include <thread>
#include <vector>
#include <cstdio>


namespace MCCFR {

uint64_t ParallelTrainer::getTotalNodesTouched() const {
    return totalNodesTouched;
}

void ParallelTrainer::mergeMaps(
                                robin_hood::unordered_flat_map<InfosetKey, Infoset, InfosetKeyHasher>& globalInfosets,
                                const robin_hood::unordered_flat_map<InfosetKey, Infoset, InfosetKeyHasher>& threadInfosets)
{
    // For every infoset explored by this thread
    for (const auto& [key, threadInfoset] : threadInfosets) {
        auto it = globalInfosets.find(key);
        if (it == globalInfosets.end()) {
            // Nao hasn’t seen this hand/street/bet sequence yet — add it directly
            globalInfosets[key] = threadInfoset;
        } else {
            // Nao has seen this situation before — merge thread’s regrets and strategy sums
            Infoset& globalInfoset = it->second;
            
            // if the thread explored more or fewer actions than global, take the max
            int numActions = std::max(
                                      static_cast<int>(globalInfoset.numActions),
                                      static_cast<int>(threadInfoset.numActions)
                                      );
            // compute how many actions we can actually merge (in case sizes differ)
            globalInfoset.numActions = static_cast<uint8_t>(numActions);
            
            // merge MCCFR regrets and strategy sums for each action in this infoset
            for (int i = 0; i < numActions; ++i) {
                globalInfoset.regretSum[i]   += threadInfoset.regretSum[i];   // update Nao’s regret for action i
                globalInfoset.strategySum[i] += threadInfoset.strategySum[i]; // update Nao’s accumulated strategy
            }
        }
    }
}

void ParallelTrainer::train(uint64_t totalNodesBudget, int numThreads, uint64_t baseSeed = 1337) {

    // get number of mccfr iterations each thread should perform in this run
    uint64_t nodesPerThread = totalNodesBudget / numThreads;
    
    // Kick off parallel MCCFR training for Nao
    printf("Starting Parallel MCCFR: %llu total nodes budget, %d threads (%llu nodes/thread)\n",
               totalNodesBudget, numThreads, nodesPerThread);
    
    // each thread gets its own Trainer with a unique seed
    std::vector<std::thread> threads; // hold all launched threads
    std::vector<Trainer*> trainers(numThreads); // trainer instance for each of them
    
    for (int t = 0; t < numThreads; ++t) {
        // unique seed for each thread to generate different hand sequences
        trainers[t] = new Trainer(baseSeed + t * 999983);
        trainers[t]->threadId = t;
    }
    
    // launch threads
    for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([t, nodesPerThread, &trainers]() {
        
                trainers[t]->train(nodesPerThread);
            });
        }
    
    // wait for all threads to finish game simulations
    for (auto& th : threads) {
        th.join();
    }
    
    totalNodesTouched = 0;
    for (int t = 0; t < numThreads; ++t) {
        totalNodesTouched += trainers[t]->getNodesTouched();
    }
    
    printf("All threads done. Actual total nodes evaluated: %llu\n", totalNodesTouched);
    
    // clear global map before merging
    mergedMap.clear();
    // for overlapping infosets (same hand / street / bet sequence), sum regrets and strategy counts
    for (int t = 0; t < numThreads; ++t) {
        mergeMaps(mergedMap, trainers[t]->getInfosetMap());
        delete trainers[t];
        trainers[t] = nullptr;
    }
    
    printf("Merge complete. Nao now has %zu infosets covering explored poker situations.\n", mergedMap.size());
}

size_t ParallelTrainer::getNumInfosets() const {
    return mergedMap.size();
}

int32_t ParallelTrainer::getBucketIdPublic(const MCCFRState& state, const std::array<int,2>& hand, const std::array<int,5>& board) {
    // delegate to a temporary trainer for bucket lookup
    // this is only used for testing — not called during training
    Trainer tmp;
    return tmp.getBucketIdPublic(state, hand, board);
}

}
