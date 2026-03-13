#pragma once

#include "hand-bucketing/generate_luts.hpp"
#include "hand-bucketing/lut_manager.hpp"

namespace Bucketer {

/**
 * core bucket lookup logic using raw pointers.
 * @param mappingEngine - isomorphism tool
 * @param hand pointer to start of 2 card hand array
 * @param board pointer to start of the board array (size determined by boardSize)
 * @param boardSize number of cards currently on the board (can be 0 / 3 / 4 / 5)
 * @return The bucket ID (0-999)
 */
int lookup_bucket(IsomorphismEngine& mappingEngine,
                  const int* hand,
                  const int* board,
                  int boardSize);

// Wrapper for the Preflop logic to handle the pointer-to-array conversion
int get_preflop_bucket_raw(const int* hand);

// Wrapper for the River logic to handle the pointer-to-array conversion
int get_river_bucket_raw(const int* hand, const int* board);

}
