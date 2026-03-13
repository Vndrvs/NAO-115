#include "../include/bucket-lookups/lut_manager.hpp"
#include <fstream>
#include <iostream>

namespace Bucketer {

// global pointer definition
std::unique_ptr<uint16_t[]> flop_lut = nullptr;
std::unique_ptr<uint16_t[]> turn_lut = nullptr;

bool load_luts(const std::string& flop_path, const std::string& turn_path) {
    bool success = true;

    // load Flop LUT
    if (!flop_path.empty()) {
        std::cout << "Allocating memory for Flop LUT...\n";
        flop_lut = std::make_unique<uint16_t[]>(FLOP_COMBOS);

        std::ifstream flop_file(flop_path, std::ios::binary);
        if (flop_file) {
            flop_file.read(reinterpret_cast<char*>(flop_lut.get()), FLOP_COMBOS * sizeof(uint16_t));
            if (flop_file) {
                std::cout << "SUCCESS: Flop LUT loaded into RAM.\n";
            } else {
                std::cerr << "ERROR: Failed to read the entire Flop LUT file.\n";
                success = false;
            }
            flop_file.close();
        } else {
            std::cerr << "ERROR: Could not open Flop LUT file at " << flop_path << "\n";
            success = false;
        }
    }

    // load Turn LUT
    if (!turn_path.empty()) {
        std::cout << "Allocating memory for Turn LUT...\n";
        turn_lut = std::make_unique<uint16_t[]>(TURN_COMBOS);

        std::ifstream turn_file(turn_path, std::ios::binary);
        if (turn_file) {
            turn_file.read(reinterpret_cast<char*>(turn_lut.get()), TURN_COMBOS * sizeof(uint16_t));
            if (turn_file) {
                std::cout << "SUCCESS: Turn LUT loaded into RAM.\n";
            } else {
                std::cerr << "ERROR: Failed to read the entire Turn LUT file.\n";
                success = false;
            }
            turn_file.close();
        } else {
            std::cerr << "ERROR: Could not open Turn LUT file at " << turn_path << "\n";
            success = false;
        }
    }

    return success;
}

}
