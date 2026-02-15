#include "bucketer.hpp"
#include "eval/evaluator.hpp"
#include "abstraction.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <iostream>
#include <numeric>
#include <atomic>
#include <filesystem>

// I have to compile omp later because I'm building on macOS
#ifdef _OPENMP
#include <omp.h>
#endif

// bucketing logic:

namespace Bucketer {

// CONFIGURATION

const int FLOP_BUCKETS  = 15;
const int TURN_BUCKETS  = 20;
const int RIVER_BUCKETS = 50;

const int SAMPLES_FLOP  = 2000;
const int SAMPLES_TURN  = 3000;
const int SAMPLES_RIVER = 5000;

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

// PREFLOP

int get_preflop_bucket(const std::vector<int>& h) {
    int r1 = h[0] / 4, r2 = h[1] / 4;
    int s1 = h[0] % 4, s2 = h[1] % 4;
    int hi = std::max(r1, r2);
    int lo = std::min(r1, r2);
    if (hi == lo) return hi;
    int idx = hi * (hi - 1) / 2 + lo;
    return (s1 == s2) ? (13 + idx) : (91 + idx);
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
    for (const auto& point : data)
        for (size_t i = 0; i < dim; ++i)
            stats[i][0] += point[i];

    for (size_t i = 0; i < dim; ++i)
        stats[i][0] /= static_cast<float>(n);

    // compute standard deviation
    for (const auto& point : data)
        for (size_t i = 0; i < dim; ++i) {
            float diff = point[i] - stats[i][0];
            stats[i][1] += diff * diff;
        }

    for (size_t i = 0; i < dim; ++i)
        stats[i][1] = std::sqrt(stats[i][1] / static_cast<float>(n));
}

void apply_z(std::vector<std::array<float,4>>& data,
             const std::vector<std::array<float,2>>& stats)
{
    const size_t dim = 4;

    for (auto& point : data)
        for (size_t i = 0; i < dim; ++i)
            if (stats[i][1] > 1e-9f)
                point[i] = (point[i] - stats[i][0]) / stats[i][1];
}

// KMEANS

std::vector<std::array<float,4>> kmeans(
    const std::vector<std::array<float,4>>& data,
    int k,
    int max_iters
) {
    const size_t n = data.size();
    const size_t dim = 4;
    std::vector<size_t> assignments(n, 0);
    std::mt19937 rng(123);
    std::uniform_int_distribution<size_t> U(0, n - 1);

    std::vector<std::array<float,4>> centroids(k);
    
    if (data.empty())
        throw std::invalid_argument("kmeans: data is empty");

    if (k <= 0)
        throw std::invalid_argument("kmeans: k must be positive");

    // random init from data
    for (int i = 0; i < k; ++i)
        centroids[i] = data[U(rng)];
    
    if (k > static_cast<int>(data.size()))
        throw std::invalid_argument("kmeans: k cannot exceed number of points");

    // temporary storage
    std::vector<std::array<float,4>> sum(k, {0.f,0.f,0.f,0.f});
    std::vector<int> count(k, 0);

    for (int it = 0; it < max_iters; ++it) {
        #pragma omp parallel for
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
        }

        // reset sum and count
        for (int i = 0; i < k; ++i) {
            sum[i] = {0.f,0.f,0.f,0.f};
            count[i] = 0;
        }

        // accumulation for centroid update
        for (size_t i = 0; i < n; ++i) {
            size_t cluster = assignments[i];
            count[cluster]++;
            for (size_t j = 0; j < dim; ++j)
                sum[cluster][j] += data[i][j];
        }

        // update centroids
        for (int i = 0; i < k; ++i) {
            if (count[i] == 0) {
                centroids[i] = data[U(rng)];
            } else {
                for (size_t j = 0; j < dim; ++j)
                    centroids[i][j] = sum[i][j] / count[i];
            }
        }
    }

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
    std::cout << "Training bucketer..." << std::endl;

    for(int street = 0; street < 3; street++) {
        int N;
        if (street == 0) N = SAMPLES_FLOP;
        else if (street == 1) N = SAMPLES_TURN;
        else N = SAMPLES_RIVER;
        
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
        
        compute_stats(data, feature_stats[street]);
        apply_z(data, feature_stats[street]);
        
        int k = (street == 0) ? FLOP_BUCKETS : (street == 1) ? TURN_BUCKETS : RIVER_BUCKETS;
        
        if (k > N) k = N;
        
        centroids[street] = kmeans(data, k);
    }

    std::ofstream out("centroids.dat", std::ios::binary);
    
    if (!out.is_open()) {
        std::cerr << "Error: Could not open centroids.dat for writing!" << std::endl;
    } else {
        for(int street = 0; street < 3; street++){
            int numCentroids = static_cast<int>(centroids[street].size());
            int numFeatures = 4;
            
            out.write((char*)&numCentroids, sizeof(int));
            out.write((char*)&numFeatures, sizeof(int));
            
            for(int i = 0 ; i < numFeatures; i++) out.write((char*)&feature_stats[street][i][0], sizeof(float));
            for(int i = 0; i < numFeatures; i++) out.write((char*)&feature_stats[street][i][1], sizeof(float));
            
            for(auto& c : centroids[street])
                out.write((char*)c.data(), numFeatures * sizeof(float));
        }
        out.close();
        std::cout << "Bucketer training finished." << std::endl;
    }
}

void initialize() {
    if(initialized) return;
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
        
        for(int i=0; i<numFeatures; i++) in.read((char*)&feature_stats[s][i][0], sizeof(float));
        for(int i=0; i<numFeatures; i++) in.read((char*)&feature_stats[s][i][1], sizeof(float));
        
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
    if(b.empty()) return get_preflop_bucket(h);
    if(!initialized) initialize();

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
        if(s > 1e-9f) f_norm[i] = (f_norm[i] - m) / s;
    }

    int best = 0;
    float min_dist = std::numeric_limits<float>::max();
    
    for(int i=0; i < centroids[st].size(); i++){

        float d = 0.f;
        for(int k=0; k<4; k++) {
            float diff = f_norm[k] - centroids[st][i][k];
            d += diff * diff;
        }
        
        if(d < min_dist){
            min_dist = d;
            best = i;
        }
    }
    return best;
}

}
