#pragma once

namespace Zobrist {

void init();

extern uint64_t TABLE[4][5][7];  // [street][raiseCount][actionIndex]
uint64_t hash(uint8_t street, uint8_t raiseCount, int actionIndex);
static constexpr uint64_t ZOBRIST_SEED = 12345;

}
