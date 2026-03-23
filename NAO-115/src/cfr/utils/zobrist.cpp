#include "zobrist.hpp"
#include <random>

uint64_t Zobrist::TABLE[4][2][5][7];

void Zobrist::init() {
    std::mt19937_64 rng(ZOBRIST_SEED);
    for (auto& street : TABLE) {
        for (auto& player : street) {
            for (auto& raiseCount : player) {
                for (auto& actionIndex : raiseCount) {
                    actionIndex = rng();
                }
            }
        }
    }
}
