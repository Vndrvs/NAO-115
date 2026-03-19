#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace Bucketer {

// pointers to the lookup tables - matches generator output: 2 bytes per bucket
extern std::unique_ptr<uint16_t[]> flop_lut;
extern std::unique_ptr<uint16_t[]> turn_lut;

// isomorphic combination counts
const uint64_t FLOP_COMBOS = 1286792;
const uint64_t TURN_COMBOS = 13960050;

// allocates memory and loads .lut files from disk into RAM - called once at the start
bool load_luts(const std::string& flop_path, const std::string& turn_path);
}
