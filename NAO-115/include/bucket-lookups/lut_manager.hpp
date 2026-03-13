#pragma once

#include <cstdint>
#include <string>
#include <fstream>
#include <iostream>
#include ".../src/hand-bucketing/mapping_engine.hpp"

namespace Bucketer {

// pointers to the lookup tables - matches generator output: 2 bytes per bucket
extern uint16_t* flop_lut;
extern uint16_t* turn_lut;

// isomorphic combination counts
const uint64_t FLOP_COMBOS = 1287000;
const uint64_t TURN_COMBOS = 13915123;

// allocates memory and loads .lut files from disk into RAM - called once at the start
bool load_luts(const std::string& flop_path, const std::string& turn_path);

// frees the allocated memory for the LUTs - called once at the end of the program
void cleanup_luts();
}
