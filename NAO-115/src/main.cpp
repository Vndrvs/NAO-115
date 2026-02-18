// currently main is responsible for running the bucketer abstraction module
#include "bucketing/bucketer.hpp"

int main() {
    Bucketer::prepare_filesystem();
    Bucketer::generate_centroids();
    return 0;
}
