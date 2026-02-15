#pragma once

#include <stdio.h>

namespace Bucketer {

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


