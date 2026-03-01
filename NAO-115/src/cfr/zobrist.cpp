#include "zobrist.hpp"
#include <random>

uint64_t Zobrist::TABLE[4][5][7];

void Zobrist::init() {
    std::mt19937_64 rng(ZOBRIST_SEED);
    for (auto& street : TABLE) {
        for (auto& raiseCount : street) {
            for (auto& actionIndex : raiseCount) {
                actionIndex = rng();
            }
        }
    }
}
