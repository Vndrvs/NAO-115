#include "analyze_process.hpp"
#include "external/jacobi_pd.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <string>
#include <limits>


namespace Bucketer {

// 1 - DATA DISTRIBUTION BEFORE CLUSTERING

DataDistributionLogger::DataDistributionLogger(const std::string& filename)
{
    file_.open(filename);
    if (!file_) throw std::runtime_error("Failed to open log file: " + filename);
}

DataDistributionLogger::~DataDistributionLogger() {
    if (file_.is_open()) file_.close();
}

// helpers

/*
using the two-pass algorithm for calculating variance to avoid roundoff errors
source: Higham, Nicholas J. (2002). "Problem 1.10".
https://epubs.siam.org/doi/book/10.1137/1.9780898718027
*/

float DataDistributionLogger::calculateVariance(const std::vector<float>& dataset, float mean) {
    double sumOfSqDifferences = 0.0;
    size_t n = dataset.size();
    
    for (float value : dataset) {
        double difference = value - mean;
        sumOfSqDifferences += difference * difference;
    }
    
    if (n > 1) {
        // Bessel's Correction for sample size (n - 1 division)
        return static_cast<float>(sumOfSqDifferences / (n - 1));
    } else {
        return 0.0f;
    }
}

float DataDistributionLogger::findMedian(std::vector<float> dataset) {
    if(dataset.empty()) {
        return 0.0f;
    }
    
    size_t n = dataset.size();
    std::sort(dataset.begin(), dataset.end());
    
    if (n % 2 != 0) {
        return dataset[n / 2];
    } else {
        return (dataset[(n - 1) / 2] + dataset[n / 2]) / 2.0f;
    }
}

int DataDistributionLogger::getBinIndex(float value, float min_v, float range, int num_bins) {
    if (range < 1e-9) return 0;
    int bin = static_cast<int>((value - min_v) / range * (num_bins - 0.001f));
    if (bin < 0) bin = 0;
    if (bin >= num_bins) bin = num_bins - 1;
    return bin;
}

std::pair<float, float> DataDistributionLogger::calculateMeanVariance(const std::vector<float>& inputVector) {
    
    if (inputVector.empty()) {
        return {0.0f, 0.0f};
    }
    
    double sum = 0;
    
    for (float value : inputVector) {
        sum += value;
    }
    
    float mean = sum / inputVector.size();
    float variance = calculateVariance(inputVector, mean);
    
    return { mean, variance };
}

// core loggers

void DataDistributionLogger::logMoments(const std::vector<std::vector<float>>& features,
                std::vector<double>& means,
                std::vector<double>& stds) {
    file_ << "1.1 Moments & Shape\n";
    file_ << "Feature, Mean, StdDev, Skew, Kurtosis\n";
    
    size_t num_features = features.size();
    
    means.resize(num_features);
    stds.resize(num_features);
    
    for (size_t f = 0; f < num_features; ++f) {
        // calculate mean & variance through external helper
        std::pair<float, float> moments = calculateMeanVariance(features[f]);
        means[f] = moments.first;
        float variance = moments.second;
        // calculate standard deviation (square root of variance)
        stds[f] = std::sqrt(variance);
        
        // calculate higher moments for skew and kurtosis
        double m3_sum = 0.0;
        double m4_sum = 0.0;
        int N = static_cast<int>(features[f].size());
        
        for (float value : features[f]) {
            double deviation = value - means[f];
            m3_sum += deviation * deviation * deviation;
            m4_sum += deviation * deviation * deviation * deviation;
        }
        
        double m3 = m3_sum / N;
        double m4 = m4_sum / N;
        double skew = 0.0;
        double kurtosis = 0.0;
        
        if (N > 0) {
            m3 = m3_sum / N;
            m4 = m4_sum / N;
        }
        
        if (variance > 1e-9) {
            skew = m3 / std::pow(stds[f], 3);
            kurtosis = (m4 / (variance * variance)) - 3.0;
        }
        
        file_ << "F" << f << ", " << means[f] << ", " << stds[f] << ", " << skew << ", " << kurtosis << "\n\n";
    }
}

void DataDistributionLogger::logOutliers(const std::vector<std::vector<float>>& features,
                 const std::vector<double>& means,
                 const std::vector<double>& stds,
                 const std::vector<std::string>& labels)
{
    if (!file_.is_open()) return;
    
    file_ << "1.2 Extreme Values & Histograms (Statistical Analysis)\n";
    
    size_t N = features[0].size();
    size_t num_features = features.size();
    
    // calculate Rice's rule for binning (formula: k = 2 * N^(1/3))
    int rice_count = static_cast<int>(2.0 * std::pow(N, 1.0/3.0));
    
    file_ << "Binning Logic: \n";
    file_ << "  * Visual Bins: 50 (Fixed 2% resolution for readability)\n";
    file_ << "  * Rice's Rule: " << rice_count << " (Optimal for N = " << N << ")\n\n";
    
    for (size_t f = 0; f < num_features; ++f) {
        float min_v = *std::min_element(features[f].begin(), features[f].end());
        float max_v = *std::max_element(features[f].begin(), features[f].end());
        float range = (max_v - min_v > 1e-9) ? (max_v - min_v) : 1.0f;
        
        float mid_low;
        float mid_high;
        
        if (labels[f] == "Asymmetry") {
            mid_low = -0.1f;
            mid_high = 0.1f;
        } else {
            mid_low = 0.45f;
            mid_high = 0.55f;
        }
        
        double mean = means[f];
        double std_dev = stds[f];
        
        // tresholds - over 2 sigma
        float thresh_low = mean - (2.0 * std_dev);
        float thresh_high = mean + (2.0 * std_dev);
        
        // prepare bins
        std::vector<int> visualBins(50, 0);       // for research visualisation
        std::vector<int> riceBins(rice_count, 0); // for true statistical analysis
        
        int low_sigma_count = 0; // over 2 sigma, lowest
        int high_sigma_count = 0; // over 2 sigma, highest
        int mid_band_count = 0; // middle 10%
        
        // data loop
        for (float v : features[f]) {
            // count extremes
            if (v < thresh_low) {
                low_sigma_count++;
            }
            if (v > thresh_high) {
                high_sigma_count++;
            }
            if (v >= mid_low && v <= mid_high) {
                mid_band_count++;
            }
            
            // populate visual histogram
            visualBins[getBinIndex(v, min_v, range, 50)]++;
            
            // populate Rice histogram
            riceBins[getBinIndex(v, min_v, range, rice_count)]++;
        }
        
        file_ << "--- Feature: " << labels[f] << " ---\n";
        file_ << "Range: [" << min_v << ", " << max_v << "]\n";
        file_ << "2-Sigma Low  (< " << thresh_low << "): " << low_sigma_count
        << " (" << std::fixed << std::setprecision(2) << (100.0 * low_sigma_count / N) << "%)\n";
        file_ << "2-Sigma High (> " << thresh_high << "): " << high_sigma_count
        << " (" << (100.0 * high_sigma_count / N) << "%)\n";
        file_ << "Middle 10%   (" << mid_low << "-" << mid_high << "): " << mid_band_count
        << " (" << (100.0 * mid_band_count / N) << "%)\n";
        
        // CSV block 1 (visual)
        file_ << "\n[CSV] Visual Histogram (50 Bins)\n";
        file_ << "BinStart, Count\n";
        for (int b = 0; b < 50; ++b) {
            float start = min_v + (range * b / 50.0f);
            file_ << start << ", " << visualBins[b] << "\n";
        }
        
        // CSV block 2 (Rice Rule based)
        file_ << "\n[CSV] Scientific Histogram (Rice Rule n=" << rice_count << ")\n";
        file_ << "BinStart, Count\n";
        for (int b = 0; b < rice_count; ++b) {
            float start = min_v + (range * b / static_cast<float>(rice_count));
            file_ << start << ", " << riceBins[b] << "\n";
        }
        file_ << "\n";
    }
    file_.flush();
}

void DataDistributionLogger::logQuantiles(std::vector<std::vector<float>> features, const std::vector<std::string>& labels) {
    file_ << "1.3 Quantiles (Distribution Shape)\n";
    file_ << "Feature, Min, P1, P5, P25, Median, P75, P95, P99, Max\n";
    
    size_t N = features[0].size();
    size_t num_features = features.size();
    
    for (size_t f = 0; f < num_features; ++f) {
        // sort the feature data locally
        std::sort(features[f].begin(), features[f].end());
        
        // quantiles computed as floor(q * N) on sorted data (empirical CDF)
        auto qidx = [&](double q) {
            return std::min(static_cast<size_t>(q * N), N - 1);
        };
        
        file_ << labels[f] << ", "
             << features[f][qidx(0.00)] << ", "
             << features[f][qidx(0.01)] << ", "
             << features[f][qidx(0.05)] << ", "
             << features[f][qidx(0.25)] << ", "
             << features[f][qidx(0.50)] << ", "
             << features[f][qidx(0.75)] << ", "
             << features[f][qidx(0.95)] << ", "
             << features[f][qidx(0.99)] << ", "
             << features[f][N - 1] << "\n";
    }
    file_ << "\n";
}

void DataDistributionLogger::logCorrelationAndPCA(const std::vector<std::vector<float>>& data,
                          const std::vector<double>& means,
                          const std::vector<double>& stds,
                          const std::vector<std::string>& labels)
{
    
    file_ << "1.4 Correlation & PCA\n";
    size_t N = data.size();
    int n = static_cast<int>(data[0].size());
    
    // verify label name input correctness
    if (labels.size() != static_cast<size_t>(n)) {
        file_ << "Error: label count mismatch\n";
        return;
    }
    
    // compute covariance matrix
    std::vector<std::vector<double>> cov(n, std::vector<double>(n, 0.0));
    
    // accumulate sums of products
    for (size_t i = 0; i < N; i++) {
        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                cov[r][c] += (static_cast<double>(data[i][r]) - means[r]) * (static_cast<double>(data[i][c]) - means[c]);
            }
        }
    }
    
    // divide once to finalize covariance matrix
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            cov[r][c] /= (N - 1);
        }
    }
    
    // finalize covariance and calculate correlation matrix
    file_ << "Correlation Matrix (copy-ready heatmap) :\n ,";
    for (size_t i = 0; i < labels.size(); ++i) {
        file_ << labels[i];
        if (i + 1 < labels.size()) {
            file_ << ",";
        }
    }
    
    file_ << "\n";

    for (int r = 0; r < n; r++) {
        file_ << labels[r] << ",";
        for (int c = 0; c < n; c++) {
            double corr_val = 0.0;
            if (stds[r] > 1e-9 && stds[c] > 1e-9) {
                corr_val = cov[r][c] / (stds[r] * stds[c]);
            } else {
                corr_val = (r == c) ? 1.0 : 0.0;
            }

            file_ << std::fixed << std::setprecision(4) << corr_val;
            
            if(c + 1 < n) {
                file_ << ",";
            }
        }
        file_ << "\n";
    }
    
    // 3. PCA using jacobi_pd
    double* evals = new double[n];
    double** evecs;
    double** M_input;
    
    // use library helpers for 2D allocation
    matrix_alloc_jpd::Alloc2D(n, n, &evecs);
    matrix_alloc_jpd::Alloc2D(n, n, &M_input);
    
    for(int i=0; i<n; i++)
        for(int j=0; j<n; j++)
            M_input[i][j] = cov[i][j];
    
    // solver instance
    jacobi_pd::Jacobi<double, double*, double**> eigen_calc(n);
    
    // run diagonalization
    int iterations = eigen_calc.Diagonalize(M_input, evals, evecs);
    
    if (iterations > 0) {
        std::vector<double> eigvals(evals, evals + n);
        std::sort(eigvals.begin(), eigvals.end(), std::greater<>());

        double total_var = 0;
        
        for (double v : eigvals) {
            if (v > 0) total_var += v;
        }

        file_ << "PCA Eigenvalues (Variance Explained):\n";
        for (int i = 0; i < n; i++) {
            double pct = (total_var > 0) ? (eigvals[i] / total_var * 100.0) : 0.0;
            file_ << "PC" << i << ": " << std::scientific << eigvals[i]
                 << " (" << std::fixed << std::setprecision(2) << pct << "%)\n";
        }
    } else {
        file_ << "Error: PCA failed to converge.\n";
    }
    file_ << "\n";
    
    // cleanup using library helpers
    matrix_alloc_jpd::Dealloc2D(&evecs);
    matrix_alloc_jpd::Dealloc2D(&M_input);
    delete[] evals;
    
    // reset formatting
    file_.unsetf(std::ios::floatfield);
    file_.precision(6);
    file_.flush();
}

void DataDistributionLogger::logDistribution(int street, const std::vector<std::vector<float>>& data) {
    if (!file_.is_open()) return;
    
    int NUM_FEATURES = static_cast<int>(data[0].size());
    
    file_ << ">>> Data Distribution: Street " << street << " <<<\n";
    file_ << "Sample size: " << data.size() << " samples\n\n";
    
    // transpose data
    std::vector<std::vector<float>> features(NUM_FEATURES, std::vector<float>(data.size()));
    for (int i = 0; i < data.size(); ++i) {
        for (int f = 0; f < NUM_FEATURES; ++f) features[f][i] = data[i][f];
    }
    
    // prepare stat containers
    std::vector<double> shared_means;
    std::vector<double> shared_stds;
    std::vector<std::string> currentLabels;
    if (street == 0 || street == 1 ) {
        currentLabels = {"EHS", "Asymmetry", "Nut Potential"};
    } else {
        currentLabels = {"EquityTotal", "EquityVsStrong", "EquityVsWeak", "BlockerIndex"};
    }
    
    // run loggers
    logMoments(features, shared_means, shared_stds);
    logOutliers(features, shared_means, shared_stds, currentLabels);
    logQuantiles(features, currentLabels);
    logCorrelationAndPCA(data, shared_means, shared_stds, currentLabels);
}

// K-MEANS CONVERGENCE (class: KMeansLogger)

KMeansLogger::KMeansLogger(const std::string& filename)
: file(filename, std::ios::app) {}

KMeansLogger::~KMeansLogger() {
    if (file.is_open()) file.close();
}

// after each iteration
void KMeansLogger::logIteration(int iter, float inertia, float centroidDelta, const std::vector<int>& clusterCounts) {
    if (!file.is_open()) return;
    
    int minCount = *std::min_element(clusterCounts.begin(), clusterCounts.end());
    int maxCount = *std::max_element(clusterCounts.begin(), clusterCounts.end());
    
    file << "Iteration: " << iter
    << ", Inertia: " << inertia
    << ", CentroidDelta: " << centroidDelta
    << ", MinClusterSize: " << minCount
    << ", MaxClusterSize: " << maxCount << "\n";
}

// after K-means function completes
void KMeansLogger::logSummary(int totalIters, float initialInertia, float finalInertia, int emptyClusterReseeds) {
    if (!file.is_open()) return;
    
    file << "\n--- K-Means Summary ---\n";
    file << "Initial Inertia: " << initialInertia << "\n";
    file << "Final Inertia:   " << finalInertia << "\n";
    file << "Iterations:      " << totalIters << "\n";
    file << "Empty Cluster Reseeds: " << emptyClusterReseeds << "\n";
    file << "----------------------\n\n";
}

}
