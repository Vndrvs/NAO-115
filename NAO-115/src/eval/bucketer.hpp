#pragma once
#include <vector>
#include <string>

namespace Bucketer {

extern std::vector<std::array<float,4>> centroids[3];
extern std::vector<std::array<float,2>> feature_stats[3];
extern const int FLOP_BUCKETS;
extern const int TURN_BUCKETS;
extern const int RIVER_BUCKETS;

int get_preflop_bucket(const std::vector<int>& h);
std::vector<float> get_features_dynamic(const std::vector<int>& hand, const std::vector<int>& board);
void compute_stats(const std::vector<std::array<float,4>>& data, std::vector<std::array<float,2>>& stats);
void apply_z(std::vector<std::array<float,4>>& data, const std::vector<std::array<float,2>>& stats);
std::vector<std::array<float,4>> kmeans(const std::vector<std::array<float,4>>& data, int k, int max_iters = 100);
void initialize();

// The Main API: Returns Bucket ID
// 0-168: Preflop
// 0-999: Flop (Caller must manage offsets, e.g. flop_offset + bucket)
// 0-1999: Turn
// 0-1999: River
int get_bucket(const std::vector<int>& hand, const std::vector<int>& board);
void generate_centroids();
    
}
