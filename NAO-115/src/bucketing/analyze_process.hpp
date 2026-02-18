#pragma once
#include <vector>
#include <array>
#include <string>
#include <fstream>

namespace Bucketer {

// distribution
class DataDistributionLogger {
public:
    static constexpr int NUM_FEATURES = 4;

    explicit DataDistributionLogger(const std::string& filename);
    ~DataDistributionLogger();

    void logDistribution(int street, const std::vector<std::array<float, NUM_FEATURES>>& data);

private:
    std::ofstream file_;

    float calculateVariance(const std::vector<float>& dataset, float mean);
    float findMedian(std::vector<float> dataset);
    std::pair<float, float> calculateMeanVariance(const std::vector<float>& inputVector);
    int getBinIndex(float value, float min_v, float range, int num_bins);

    void logMoments(const std::vector<std::vector<float>>& features,
                    std::vector<double>& means,
                    std::vector<double>& stds);
    void logOutliers(const std::vector<std::vector<float>>& features,
                     const std::vector<double>& means,
                     const std::vector<double>& stds,
                     const std::vector<std::string>& labels);
    void logQuantiles(std::vector<std::vector<float>> features, const std::vector<std::string>& labels);
    void logCorrelationAndPCA(const std::vector<std::array<float, NUM_FEATURES>>& data,
                              const std::vector<double>& means,
                              const std::vector<double>& stds,
                              const std::vector<std::string>& labels);
};

// k-means
class KMeansLogger {
public:
    KMeansLogger(const std::string& filename);
    ~KMeansLogger();

    void logIteration(int iter, float inertia, float centroidDelta, const std::vector<int>& clusterCounts);
    void logSummary(int totalIters, float initialInertia, float finalInertia, int emptyClusterReseeds);

private:
    std::ofstream file;
};

}


