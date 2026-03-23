#pragma once

#include <cstdint>

namespace Zobrist {

void init();

extern uint64_t TABLE[4][2][5][7]; // [street (4)][player (2)][raiseCount (5)][actionIndex (6)]
static constexpr uint64_t ZOBRIST_SEED = 12345;
}
