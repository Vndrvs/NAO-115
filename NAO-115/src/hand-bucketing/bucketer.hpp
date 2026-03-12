#pragma once
#include <vector>
#include <string>

namespace Bucketer {

// centroid generation
extern std::vector<std::vector<float>> centroids[3];
extern std::vector<std::array<float,2>> feature_stats[3];
extern const int FLOP_BUCKETS;
extern const int TURN_BUCKETS;
extern const int RIVER_BUCKETS;

struct BucketData {
    float centroids[3][2000 * 4];
    float means[3][4];
    float stddevs[3][4];
    int   numCentroids[3];
    int   numFeatures[3];
};

extern BucketData bucketData;

void compute_stats(const std::vector<std::vector<float>>& data, std::vector<std::array<float,2>>& stats);
void apply_z(std::vector<std::vector<float>>& data, const std::vector<std::array<float,2>>& stats);
std::vector<std::vector<float>> kmeans(const std::vector<std::vector<float>>& data, int k, const std::string& logFilename, int max_iters);
void prepare_filesystem();
void generate_centroids();


// runtime lookups
void initialize();
int get_preflop_bucket(const std::vector<int>& h);
std::vector<float> get_features_dynamic(const std::vector<int>& hand, const std::vector<int>& board);
// LUT table helpers
int get_flop_bucket(const std::array<int, 2>& hand, const std::array<int, 3>& board);
int get_turn_bucket(const std::array<int, 2>& hand, const std::array<int, 4>& board);
int get_river_bucket(const std::array<int, 2>& hand, const std::array<int, 5>& board);

// analysis
class DataDistributionLogger;
class KMeansLogger;
}

