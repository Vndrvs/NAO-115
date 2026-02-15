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

int get_bucket(const std::vector<int>& hand, const std::vector<int>& board);
void generate_centroids();
void analyze_centroids_full();
void bucketer_health_check();

}
