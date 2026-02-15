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

std::ofstream file;
const int NUM_FEATURES = 4;

float calculateVariance(const std::vector<float>& dataset, float mean) {
    double sumOfSqDifferences = 0.0;
    size_t n = dataset.size();
    
    for (float value : dataset) {
        double difference = value - mean;
        sumOfSqDifferences += difference * difference;
    }
    
    // Bessel's Correction for sample size (n - 1 division)
    if (n > 1) {
        return static_cast<float>(sumOfSqDifferences / (n - 1));
    } else {
        return 0.0f;
    }
}

float findMedian(std::vector<float> dataset) {
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

std::pair<float, float> calculateMeanVariance(const std::vector<float>& inputVector) {
    
    if (inputVector.empty()) {
        return {0.0f, 0.0f};
    }
    
    double sum = 0;
    double sum_squared = 0;
    
    for (float value : inputVector) {
        sum += value;
        sum_squared += value * value;
    }
    
    float mean = sum / inputVector.size();
    float variance = calculateVariance(inputVector, mean);
    
    return { mean, variance };
}

void logMoments(const std::vector<std::vector<float>>& features,
                std::vector<double>& means,
                std::vector<double>& stds) {
    file << "1.1 Moments & Shape\n";
    file << "Feature, Mean, StdDev, Skew, Kurtosis\n";
    
    means.resize(NUM_FEATURES);
    stds.resize(NUM_FEATURES);
    
    for (int f = 0; f < NUM_FEATURES; ++f) {
        // calculate mean & variance through external helper
        std::pair<float, float> moments = calculateMeanVariance(features[f]);
        means[f] = moments.first;
        float variance = moments.second;
        // calculate standard deviation (square root of variance)
        stds[f] = std::sqrt(variance);
        
        // calculate higher moments for skew and kurtosis
        double m3_sum = 0.0;
        double m4_sum = 0.0;
        int N = features[f].size();
        
        for (float value : features[f]) {
            double deviation = value - means[f];
            m3_sum += deviation * deviation * deviation;
            m4_sum += deviation * deviation * deviation * deviation;
        }
        
        double m3 = m3_sum / N;
        double m4 = m4_sum / N;
        double skew = 0.0;
        double kurtosis = 0.0;
        
        if (variance > 1e-9) {
            skew = m3 / std::pow(stds[f], 3);
            kurtosis = (m4 / (variance * variance)) - 3.0;
        }
        
        file << "F" << f << ", " << means[f] << ", " << stds[f] << ", " << skew << ", " << kurtosis << "\n";
    }
}

int get_bin_index(float value, float min_v, float range, int num_bins) {
    if (range < 1e-9) return 0;
    int bin = static_cast<int>((value - min_v) / range * (num_bins - 0.001f));
    if (bin < 0) bin = 0;
    if (bin >= num_bins) bin = num_bins - 1;
    return bin;
}

void logOutliers(const std::vector<std::vector<float>>& features,
                 const std::vector<double>& means,
                 const std::vector<double>& stds,
                 const std::vector<std::string>& labels)
{
    if (!file.is_open()) return;
    
    file << ">>> 1.2 Extreme Values & Histograms (Statistical Analysis) <<<\n";
    
    size_t N = features[0].size();
    
    // calculate Rice's rule for binning (formula: k = 2 * N^(1/3))
    int rice_count = static_cast<int>(2.0 * std::pow(N, 1.0/3.0));
    
    file << "Binning Logic: \n";
    file << "  * Visual Bins: 50 (Fixed 2% resolution for readability)\n";
    file << "  * Rice's Rule: " << rice_count << " (Optimal for N = " << N << ")\n\n";
    
    for (int f = 0; f < NUM_FEATURES; ++f) {
        // statistics for this feature
        float min_v = *std::min_element(features[f].begin(), features[f].end());
        float max_v = *std::max_element(features[f].begin(), features[f].end());
        float range = (max_v - min_v > 1e-9) ? (max_v - min_v) : 1.0f;
        
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
        
        float mid_low = min_v + (range * 0.45f);
        float mid_high = min_v + (range * 0.55f);
        
        // data loop
        for (float v : features[f]) {
            // count extremes
            if (v < thresh_low) low_sigma_count++;
            if (v > thresh_high) high_sigma_count++;
            if (v >= mid_low && v <= mid_high) mid_band_count++;
            
            // populate visual histogram
            visualBins[get_bin_index(v, min_v, range, 50)]++;
            
            // populate Rice histogram
            riceBins[get_bin_index(v, min_v, range, rice_count)]++;
        }
        
        file << "--- Feature: " << labels[f] << " ---\n";
        file << "Range: [" << min_v << ", " << max_v << "]\n";
        file << "2-Sigma Low  (< " << thresh_low << "): " << low_sigma_count
        << " (" << std::fixed << std::setprecision(2) << (100.0 * low_sigma_count / N) << "%)\n";
        file << "2-Sigma High (> " << thresh_high << "): " << high_sigma_count
        << " (" << (100.0 * high_sigma_count / N) << "%)\n";
        file << "Middle 10%   (" << mid_low << "-" << mid_high << "): " << mid_band_count
        << " (" << (100.0 * mid_band_count / N) << "%)\n";
        
        // CSV block 1 (visual)
        file << "\n[CSV] Visual Histogram (50 Bins)\n";
        file << "BinStart, Count\n";
        for (int b = 0; b < 50; ++b) {
            float start = min_v + (range * b / 50.0f);
            file << start << ", " << visualBins[b] << "\n";
        }
        
        // CSV block 2 (Rice Rule based)
        file << "\n[CSV] Scientific Histogram (Rice Rule n=" << rice_count << ")\n";
        file << "BinStart, Count\n";
        for (int b = 0; b < rice_count; ++b) {
            float start = min_v + (range * b / static_cast<float>(rice_count));
            file << start << ", " << riceBins[b] << "\n";
        }
        file << "\n";
    }
    file.flush();
}

void logQuantiles(std::vector<std::vector<float>> features, const std::vector<std::string>& labels) {
    file << "1.3 Quantiles (Distribution Shape)\n";
    file << "Feature, Min, P1, P5, P25, Median, P75, P95, P99, Max\n";
    
    size_t N = features[0].size();
    
    for (int f = 0; f < NUM_FEATURES; ++f) {
        // sort the feature data locally
        std::sort(features[f].begin(), features[f].end());
        
        auto qidx = [&](double q) {
            return std::min(static_cast<size_t>(q * N), N - 1);
        };
        
        file << labels[f] << ", "
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
    file << "\n";
}

void logCorrelationAndPCA(const std::vector<std::array<float, 4>>& data,
                          const std::vector<double>& means,
                          const std::vector<double>& stds,
                          const std::vector<std::string>& labels)
{
    file << "1.4 Correlation & PCA\n";
    size_t N = data.size();
    int n = 4;
    
    // compute covariance matrix
    double cov[4][4] = {0};
    for (size_t i = 0; i < N; i++) {
        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                cov[r][c] += (static_cast<double>(data[i][r]) - means[r]) * (static_cast<double>(data[i][c]) - means[c]);
            }
        }
    }
    
    // finalize covariance and calculate correlation matrix
    file << "Correlation Matrix (Copy to Excel for Heatmap):\n ,";
    for(auto& l : labels) file << l << ",";
    file << "\n";
    
    for (int r = 0; r < n; r++) {
        file << labels[r] << ",";
        for (int c = 0; c < n; c++) {
            cov[r][c] /= (N - 1);
            double corr_val = 0.0;
            if (stds[r] > 1e-9 && stds[c] > 1e-9) {
                corr_val = cov[r][c] / (stds[r] * stds[c]);
            } else {
                corr_val = (r == c) ? 1.0 : 0.0;
            }
            file << std::fixed << std::setprecision(4) << corr_val << (c < 3 ? "," : "\n");
        }
    }
    file << "\n";
    
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

        file << "PCA Eigenvalues (Variance Explained):\n";
        for (int i = 0; i < n; i++) {
            double pct = (total_var > 0) ? (eigvals[i] / total_var * 100.0) : 0.0;
            file << "PC" << i << ": " << std::scientific << eigvals[i]
                 << " (" << std::fixed << std::setprecision(2) << pct << "%)\n";
        }
    } else {
        file << "Error: PCA failed to converge.\n";
    }
    
    // cleanup using library helpers
    matrix_alloc_jpd::Dealloc2D(&evecs);
    matrix_alloc_jpd::Dealloc2D(&M_input);
    delete[] evals;
    
    file.flush();
}

void log_everything(int street, const std::vector<std::array<float, 4>>& data) {
    if (!file.is_open()) return;
    
    file << ">>> Data Distribution: Street " << street << " <<<\n";
    file << "Sample size: " << data.size() << " samples\n\n";
    
    // transpose data
    std::vector<std::vector<float>> features(NUM_FEATURES, std::vector<float>(data.size()));
    for (int i = 0; i < data.size(); ++i) {
        for (int f = 0; f < NUM_FEATURES; ++f) features[f][i] = data[i][f];
    }
    
    // prepare stat containers
    std::vector<double> shared_means;
    std::vector<double> shared_stds;
    std::vector<std::string> labels = {"Equity", "EqSquared", "PPot", "NPot"};
    
    // run loggers
    logMoments(features, shared_means, shared_stds);
    logOutliers(features, shared_means, shared_stds, labels);
    logQuantiles(features, labels);
    logCorrelationAndPCA(data, shared_means, shared_stds, labels);
}

// K-MEANS CONVERGENCE (class: KMeansLogger)

KMeansLogger(const std::string& filename) : file(filename) {}

~KMeansLogger() {
    if (file.is_open()) file.close();
}

// after each iteration
void logIteration(int iter, float inertia, float centroidDelta, const std::vector<int>& clusterCounts) {
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
void logSummary(int totalIters, float initialInertia, float finalInertia, int emptyClusterReseeds) {
    if (!file.is_open()) return;
    
    file << "\n--- K-Means Summary ---\n";
    file << "Initial Inertia: " << initialInertia << "\n";
    file << "Final Inertia:   " << finalInertia << "\n";
    file << "Iterations:      " << totalIters << "\n";
    file << "Empty Cluster Reseeds: " << emptyClusterReseeds << "\n";
    file << "----------------------\n\n";
}


}
