#pragma once

#include <cstdint>

namespace Zobrist {

void init();

extern uint64_t TABLE[4][2][5][6]; // [street (4)][player (2)][raiseCount (5)][actionIndex (6)]
uint64_t hash(uint8_t street, uint8_t raiseCount, int actionIndex);
static constexpr uint64_t ZOBRIST_SEED = 12345;
}
