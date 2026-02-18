#include "bucketer.hpp"
#include "eval/evaluator.hpp"
#include "abstraction.hpp"
#include "analyze_process.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <iostream>
#include <numeric>
#include <atomic>
#include <stdexcept>
#include <limits>
#include <random>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <filesystem>

// I have to compile omp later because I'm building on macOS
#ifdef _OPENMP
#include <omp.h>
#endif

// bucketing logic:

namespace Bucketer {

// CONFIGURATION

const int FLOP_BUCKETS  = 2000;
const int TURN_BUCKETS  = 2000;
const int RIVER_BUCKETS = 1500;

const int SAMPLES_FLOP  = 200000;
const int SAMPLES_TURN  = 200000;
const int SAMPLES_RIVER = 150000;

std::vector<std::array<float,4>> centroids[3];
std::vector<std::array<float,2>> feature_stats[3];

bool initialized = false;

int get_thread_id() {
    #ifdef _OPENMP
    return omp_get_thread_num();
    #else
    return 0;
    #endif
}

// initialize file directory for output files
void prepare_filesystem() {
    try {

        std::filesystem::path root_path = std::filesystem::current_path();
        std::filesystem::path output_path = root_path / "output";
        std::filesystem::path data_path = output_path / "data";
        std::filesystem::path logs_path = output_path / "logs";

        if (std::filesystem::create_directories(data_path)) {
            std::cout << "[Filesystem] Directory added:: " << data_path << std::endl;
        }
        
        if (std::filesystem::create_directories(logs_path)) {
            std::cout << "[Filesystem] Directory added:: " << logs_path << std::endl;
        }
        
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[Filesystem] Error creating directories: " << e.what() << std::endl;
    }
}

// PREFLOP
int get_preflop_bucket(const std::vector<int>& h) {
    int r1 = h[0] / 4, r2 = h[1] / 4;
    int s1 = h[0] % 4, s2 = h[1] % 4;
    int hi = std::max(r1, r2);
    int lo = std::min(r1, r2);
    
    if (hi == lo) {
        return hi;
    }
    
    int idx = hi * (hi - 1) / 2 + lo;
    
    if (s1 == s2) {
        return 13 + idx;
    } else {
        return 91 + idx;
    }
}

// CALCULATE FLOP AND TURN FEATURES
std::vector<float> get_features_dynamic(
    const std::vector<int>& hand,
    const std::vector<int>& board
) {
    std::array<int, 2> hand_arr = { hand[0], hand[1] };

    if (board.size() == 3) {
        std::array<int, 3> board_arr = { board[0], board[1], board[2] };
        
        Eval::BaseFeatures f = Eval::calculateFlopFeaturesFast(hand_arr, board_arr);
        
        return { f.e, f.e2, f.ppot, f.npot };
    }
    
    else if (board.size() == 4) {
        std::array<int, 4> board_arr = { board[0], board[1], board[2], board[3] };

        Eval::BaseFeatures f = Eval::calculateTurnFeaturesFast(hand_arr, board_arr);
        
        return { f.e, f.e2, f.ppot, f.npot };
    }

    return { 0, 0, 0, 0 };
}

// NORMALIZATION
void compute_stats(const std::vector<std::array<float,4>>& data,
                   std::vector<std::array<float,2>>& stats)
{
    const size_t n = data.size();
    const size_t dim = 4;

    stats.assign(dim, {0.f, 0.f});

    // compute means
    for (const auto& point : data) {
        for (size_t i = 0; i < dim; ++i) {
            stats[i][0] += point[i];
        }
    }
        
    for (size_t i = 0; i < dim; ++i) {
        stats[i][0] /= static_cast<float>(n);
    }

    // compute standard deviation
    for (const auto& point : data) {
        for (size_t i = 0; i < dim; ++i) {
            float diff = point[i] - stats[i][0];
            stats[i][1] += diff * diff;
        }
    }
    
    for (size_t i = 0; i < dim; ++i) {
        // population standard deviation (used for normalization)
        stats[i][1] = std::sqrt(stats[i][1] / static_cast<float>(n));
    }
    
}

/*
 Z-score normalization using population statistics
 to standardize features for k-means algorithm
 */

void apply_z(std::vector<std::array<float,4>>& data,
             const std::vector<std::array<float,2>>& stats)
{
    const size_t dim = 4;

    for (std::array<float,4>& point : data) {
        for (size_t i = 0; i < dim; ++i) {
            if (stats[i][1] > 1e-9f) {
                // works on means and stdev of each feature
                point[i] = (point[i] - stats[i][0]) / stats[i][1];
            }
        }
    }
}

// KMEANS

std::vector<std::array<float,4>> kmeans(
    const std::vector<std::array<float,4>>& data,
    int k,
    int max_iters
) {
    const size_t n = data.size();
    const size_t dim = 4;
    int num_threads = omp_get_max_threads();

    if (data.empty()) {
        throw std::invalid_argument("K-means: Data is empty");
    }
    if (k <= 0) {
        throw std::invalid_argument("K-means: k must be positive");
    }
    if (k > static_cast<int>(n)) {
        throw std::invalid_argument("K-means: k cannot exceed number of points");
    }

    std::vector<size_t> assignments(n, 0);
    std::vector<std::array<float,4>> centroids(k);
    std::vector<std::array<float,4>> old_centroids(k);

    std::vector<std::array<float,4>> sum(k, {0.f,0.f,0.f,0.f});
    std::vector<int> count(k, 0);

    std::mt19937 rng(123);
    std::uniform_int_distribution<size_t> U(0, n - 1);

    // logger
    KMeansLogger logger("output/logs/kmeans_log.txt");

    // random initialization
    for (int i = 0; i < k; ++i) {
        centroids[i] = data[U(rng)];
    }

    old_centroids = centroids; // initialize old centroids

    float initial_inertia = 0.f;
    float final_inertia = 0.f;
    int reseed_count = 0;
    
    std::vector<std::vector<std::array<double, 4>>> local_sums(num_threads, std::vector<std::array<double, 4>>(k, {0.0, 0.0, 0.0, 0.0}));
    std::vector<std::vector<int>> local_counts(num_threads, std::vector<int>(k, 0));

    int iterations_completed = 0;

    for (int it = 0; it < max_iters; ++it) {
        float iter_inertia = 0.f;

        iterations_completed = it + 1;
        
        // reset local buffers to prevent carry-over
        for (int t = 0; t < num_threads; ++t) {
            for (int i = 0; i < k; ++i) {
                local_counts[t][i] = 0;
                local_sums[t][i] = {0.0, 0.0, 0.0, 0.0};
            }
        }

        // assign points
        #pragma omp parallel for reduction(+:iter_inertia)
        for (size_t i = 0; i < n; ++i) {
            float best_dist = std::numeric_limits<float>::max();
            int best_idx = 0;

            for (int j = 0; j < k; ++j) {
                float d = 0.f;
                for (size_t l = 0; l < dim; ++l) {
                    float diff = data[i][l] - centroids[j][l];
                    d += diff * diff;
                }
                if (d < best_dist) {
                    best_dist = d;
                    best_idx = j;
                }
            }

            assignments[i] = best_idx;
            iter_inertia += best_dist;
        }

        if (it == 0) {
            initial_inertia = iter_inertia;
        }

        // reset sum & count
        for (int i = 0; i < k; ++i) {
            sum[i] = {0.f,0.f,0.f,0.f};
            count[i] = 0;
        }

        // accumulate sums for centroid update
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();

            #pragma omp for
            for (size_t i = 0; i < n; ++i) {
                size_t cluster = assignments[i];
                local_counts[tid][cluster]++;
                for (size_t j = 0; j < dim; ++j) {
                    local_sums[tid][cluster][j] += static_cast<double>(data[i][j]);
                }
            }
        }

        // merge thread-local data into the main sum/count
        for (int t = 0; t < num_threads; ++t) {
            for (int i = 0; i < k; ++i) {
                count[i] += local_counts[t][i];
                for (int j = 0; j < dim; ++j) {
                    sum[i][j] += static_cast<float>(local_sums[t][i][j]);
                }
            }
        }

        // update centroids
        for (int i = 0; i < k; ++i) {
            if (count[i] == 0) {
                centroids[i] = data[U(rng)];
                reseed_count++;
            } else {
                for (size_t j = 0; j < dim; ++j)
                    centroids[i][j] = sum[i][j] / count[i];
            }
        }

        // compute centroid delta
        float total_delta = 0.f;
        for (int i = 0; i < k; ++i) {
            float dist_sq = 0.f;
            for (size_t j = 0; j < dim; ++j) {
                float diff = centroids[i][j] - old_centroids[i][j];
                dist_sq += diff * diff;
            }
            total_delta += std::sqrt(dist_sq); // actual distance moved
        }
        
        float avg_delta = total_delta / k;

        // log iteration
        logger.logIteration(it, iter_inertia, avg_delta, count);

        // check convergence
        if (avg_delta < 1e-6f) {
            final_inertia = iter_inertia;
            break;
        }

        old_centroids = centroids; // store for next iteration
        final_inertia = iter_inertia;
    }

    // final summary log
    logger.logSummary(iterations_completed, initial_inertia, final_inertia, reseed_count);

    return centroids;
}

// RANDOM HAND AND BOARD GENERATORS

void drawFlop(std::mt19937& rng, std::uniform_int_distribution<int>& dist,
              std::array<int,2>& hand, std::array<int,3>& board)
{
    uint64_t usedCards = 0;
    int fill = 0;

    while (fill < 5) {
        int card = dist(rng);
        
        if (!(usedCards & (1ULL << card))) {
            usedCards |= (1ULL << card);
            
            if (fill < 2) {
                hand[fill] = card;
            } else {
                board[fill - 2] = card;
            }
            fill++;
        }
    }
}

void drawTurn(std::mt19937& rng, std::uniform_int_distribution<int>& dist,
              std::array<int,2>& hand, std::array<int,4>& board)
{
    uint64_t usedCards = 0;
    int fill = 0;

    while (fill < 6) {
        int card = dist(rng);
        
        if (!(usedCards & (1ULL << card))) {
            usedCards |= (1ULL << card);
            
            if (fill < 2) {
                hand[fill] = card;
            } else {
                board[fill - 2] = card;
            }
            fill++;
        }
    }
}

void drawRiver(std::mt19937& rng, std::uniform_int_distribution<int>& dist,
               std::array<int,2>& hand, std::array<int,5>& board)
{
    uint64_t usedCards = 0;
    int fill = 0;

    while (fill < 7) {
        int card = dist(rng);
        
        if (!(usedCards & (1ULL << card))) {
            usedCards |= (1ULL << card);
            
            if (fill < 2) {
                hand[fill] = card;
            }
            else {
                board[fill - 2] = card;
            }
            fill++;
        }
    }
}

// CENTROID ARITHMETIC

void generate_centroids() {
    Eval::initialize();
    DataDistributionLogger distributionLogger("output/logs/data_distribution_report.txt");
    std::cout << "Training bucketer..." << std::endl;

    for(int street = 0; street < 3; street++) {
        int N;
        if (street == 0) {
            N = SAMPLES_FLOP;
        } else if (street == 1) {
            N = SAMPLES_TURN;
        } else {
            N = SAMPLES_RIVER;
        }
        
        std::vector<std::array<float, 4>> data(N);
        
        #pragma omp parallel
        {
            std::mt19937 rng(100 + get_thread_id());
            std::uniform_int_distribution<int> dist(0, 51);
            
            #pragma omp for
            for (int i = 0; i < N; ++i) {
                if (street == 0) {
                    std::array<int,2> hand; std::array<int,3> board;
                    drawFlop(rng, dist, hand, board);
                    Eval::BaseFeatures f = Eval::calculateFlopFeaturesFast(hand, board);
                    data[i] = { f.e, f.e2, f.ppot, f.npot };
                } else if (street == 1) {
                    std::array<int,2> hand; std::array<int,4> board;
                    drawTurn(rng, dist, hand, board);
                    Eval::BaseFeatures f = Eval::calculateTurnFeaturesFast(hand, board);
                    data[i] = { f.e, f.e2, f.ppot, f.npot };
                } else {
                    std::array<int,2> hand; std::array<int,5> board;
                    drawRiver(rng, dist, hand, board);
                    Eval::RiverFeatures f = Eval::calculateRiverFeatures(hand, board);
                    data[i] = { f.eVsRandom, f.eVsTop, f.eVsMid, f.eVsBot };
                }
            }
        }
        
        distributionLogger.logDistribution(street, data);
        
        compute_stats(data, feature_stats[street]);
        apply_z(data, feature_stats[street]);
        
        int k;
        if (street == 0) {
            k = FLOP_BUCKETS;
        } else if (street == 1) {
            k = TURN_BUCKETS;
        } else {
            k = RIVER_BUCKETS;
        }
        
        if (k > N) {
            k = N;
        }
        
        centroids[street] = kmeans(data, k);
    }

    std::ofstream out("output/data/centroids.dat", std::ios::binary);
    
    if (!out.is_open()) {
        std::cerr << "Error: Could not open centroids.dat for writing!" << std::endl;
    } else {
        for(int street = 0; street < 3; street++){
            int numCentroids = static_cast<int>(centroids[street].size());
            int numFeatures = 4;
            
            out.write((char*)&numCentroids, sizeof(int));
            out.write((char*)&numFeatures, sizeof(int));
            
            for(int i = 0 ; i < numFeatures; i++) {
                out.write((char*)&feature_stats[street][i][0], sizeof(float));
            }
            for(int i = 0; i < numFeatures; i++) {
                out.write((char*)&feature_stats[street][i][1], sizeof(float));
            }
            
            for(auto& c : centroids[street]) {
                out.write((char*)c.data(), numFeatures * sizeof(float));
            }
        }
        out.close();
        std::cout << "Bucketer training finished." << std::endl;
    }
}

// runtime

void initialize() {
    if(initialized) {
        return;
    }
    
    std::ifstream in("centroids.dat", std::ios::binary);
    
    if(!in.is_open()) {
        std::cerr << "Error: Could not open centroids.dat. Run training first!\n";
        exit(1);
    }

    for(int s=0; s<3; s++){
        int numCentroids, numFeatures;
        in.read((char*)&numCentroids, sizeof(int));
        in.read((char*)&numFeatures, sizeof(int));
        
        feature_stats[s].resize(numFeatures, {0.f, 0.f});
        for(int i=0; i<numFeatures; i++) {
            in.read((char*)&feature_stats[s][i][0], sizeof(float));
        }
        
        for(int i=0; i<numFeatures; i++) {
            in.read((char*)&feature_stats[s][i][1], sizeof(float));
        }
        
        centroids[s].resize(numCentroids);
        for(int i=0; i<numCentroids; i++) {
            in.read((char*)centroids[s][i].data(), numFeatures * sizeof(float));
        }
    }
    
    initialized = true;
}

std::vector<float> get_features_river_runtime(
    const std::vector<int>& hand,
    const std::vector<int>& board
) {
    std::array<int, 2> h = { hand[0], hand[1] };
    std::array<int, 5> b = { board[0], board[1], board[2], board[3], board[4] };

    Eval::RiverFeatures f = Eval::calculateRiverFeatures(h, b);
    
    return { f.eVsRandom, f.eVsTop, f.eVsMid, f.eVsBot };
}

int get_bucket(const std::vector<int>& h, const std::vector<int>& b) {
    if(b.empty()) {
        return get_preflop_bucket(h);
    }
    if(!initialized) {
        initialize();
    }

    int st = b.size()==3 ? 0 : b.size()==4 ? 1 : 2;
    
    std::vector<float> f_vec;
    if (st == 2) {
        f_vec = get_features_river_runtime(h, b);
    } else {
        f_vec = get_features_dynamic(h, b);
    }

    std::vector<float> f_norm = f_vec;

    for(int i=0; i<4; i++){ // We know dim is 4
        float m = feature_stats[st][i][0];
        float s = feature_stats[st][i][1];
        if(s > 1e-9f) {
            f_norm[i] = (f_norm[i] - m) / s;
        }
    }

    int best = 0;
    float min_dist = std::numeric_limits<float>::max();
    
    for(int i=0; i < centroids[st].size(); i++){
        float d = 0.f;
        for(int k=0; k<4; k++) {
            float diff = f_norm[k] - centroids[st][i][k];
            d += diff * diff;
        }
        
        if(d < min_dist) {
            min_dist = d;
            best = i;
        }
    }
    return best;
}

}
