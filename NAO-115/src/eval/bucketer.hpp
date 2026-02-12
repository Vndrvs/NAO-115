#pragma once
#include <vector>
#include <string>

namespace Bucketer {

    // Must be called once at startup to load "centroids.dat"
    void initialize();

    // The Main API: Returns Bucket ID
    // 0-168: Preflop
    // 0-999: Flop (Caller must manage offsets, e.g. flop_offset + bucket)
    // 0-1999: Turn
    // 0-1999: River
    int get_bucket(const std::vector<int>& hand, const std::vector<int>& board);

    // Run this offline to generate the centroids file
    void generate_centroids();
    
}
