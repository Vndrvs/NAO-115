#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include "hand-bucketing/mapping_engine.hpp"
#include "hand-bucketing/bucketer.hpp"
#include "eval/evaluator.hpp"

// Helper function to auto-find the file based on common execution directories
std::string find_lut_file() {
    std::vector<std::string> possible_paths = {
        "output/data/luts/flop_buckets.lut",             // If run from NAO-115 root
        "../../output/data/luts/flop_buckets.lut",       // If run from NAO-115_tests/bucketer/
        "../output/data/luts/flop_buckets.lut"           // If run from a top-level build/ folder
    };

    for (const auto& path : possible_paths) {
        std::ifstream file(path, std::ios::binary);
        if (file.good()) {
            return path; // File found successfully
        }
    }
    return ""; // Not found
}

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <random>
#include "hand-bucketing/bucketer.hpp"

int main() {
    std::cout << "--- STARTING PIPELINE TEST ---\n";

    // 1. Initialize EVERYTHING
    Bucketer::IsomorphismEngine isoEngine;
    isoEngine.initialize();
    
    Bucketer::initialize();
    
    Eval::initialize();

    const size_t FLOP_COMBOS = 1287000;
    std::vector<uint16_t> mock_lut(FLOP_COMBOS, 0);

    // 2. Pick 1,000 random indices
    std::cout << "Picking 1000 random hands...\n";
    std::vector<uint64_t> test_indices;
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<uint64_t> dist(0, FLOP_COMBOS - 1);
    
    for (int i = 0; i < 1000; ++i) {
        test_indices.push_back(dist(rng));
    }

    // 3. GENERATION PHASE (What the Droplet does)
    std::cout << "Calculating buckets for the 1000 hands...\n";
    for (uint64_t idx : test_indices) {
        std::array<uint8_t, 5> cards = isoEngine.unindexFlop(idx);
        std::array<int, 2> hand = { cards[0], cards[1] };
        std::array<int, 3> board = { cards[2], cards[3], cards[4] };
        
        mock_lut[idx] = Bucketer::get_flop_bucket(hand, board);
    }

    // 4. FILE WRITE PHASE (What the Droplet does)
    std::string test_file_path = "test_flop_mock.lut";
    std::cout << "Writing file to disk (" << test_file_path << ")...\n";
    std::ofstream outfile(test_file_path, std::ios::binary);
    outfile.write(reinterpret_cast<const char*>(mock_lut.data()), FLOP_COMBOS * sizeof(uint16_t));
    outfile.close();

    // 5. FILE READ PHASE (What your MCCFR / Local test does)
    std::cout << "Reading file back from disk...\n";
    std::vector<uint16_t> loaded_lut(FLOP_COMBOS, 0);
    std::ifstream infile(test_file_path, std::ios::binary);
    infile.read(reinterpret_cast<char*>(loaded_lut.data()), FLOP_COMBOS * sizeof(uint16_t));
    infile.close();

    // 6. VERIFICATION PHASE (Cross-referencing)
    std::cout << "Cross-validating data integrity...\n";
    int failures = 0;
    for (uint64_t idx : test_indices) {
        std::array<uint8_t, 5> cards = isoEngine.unindexFlop(idx);
        std::array<int, 2> hand = { cards[0], cards[1] };
        std::array<int, 3> board = { cards[2], cards[3], cards[4] };

        uint16_t file_bucket = loaded_lut[idx];
        int dynamic_bucket = Bucketer::get_flop_bucket(hand, board);

        if (file_bucket != dynamic_bucket) {
            failures++;
            if (failures <= 5) { // Only print the first few failures to avoid console spam
                std::cout << "FAIL at Index " << idx
                          << " | File says: " << file_bucket
                          << " | Dynamic says: " << dynamic_bucket << "\n";
            }
        }
    }

    if (failures == 0) {
        std::cout << "\nSUCCESS: All 1,000 hands perfectly matched. The C++ read/write architecture is bulletproof.\n";
    } else {
        std::cout << "\nCRITICAL ERROR: " << failures << " mismatches detected. The file saving/loading is corrupt.\n";
    }

    return 0;
}
