#include <gtest/gtest.h>
#include "hand-bucketing/bucketer.hpp"
#include "hand-bucketing/hand_abstraction.hpp"
#include "eval/evaluator.hpp"
#include "eval/tables.hpp"

#include <cmath>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include <chrono>
#include <random>
#include <filesystem>
#include <limits>

static void draw_hand_board(
    std::mt19937& rng,
    std::uniform_int_distribution<int>& dist,
    std::vector<int>& hand,
    std::vector<int>& board,
    int boardSize)
{
    uint64_t used = 0;

    hand.clear();
    board.clear();

    while ((int)(hand.size() + board.size()) < 2 + boardSize) {
        int card = dist(rng);

        if (!(used & (1ULL << card))) {
            used |= (1ULL << card);

            if (hand.size() < 2)
                hand.push_back(card);
            else
                board.push_back(card);
        }
    }
}

int main() {

    const int TESTS = 10000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 51);
    std::uniform_int_distribution<int> streetDist(0, 3);

    std::vector<int> hand;
    std::vector<int> board;

    int flopCount = 0;
    int turnCount = 0;
    int riverCount = 0;

    double flopTime  = 0.0;
    double turnTime  = 0.0;
    double riverTime = 0.0;

    for (int i = 0; i < TESTS; i++) {

        int street = streetDist(rng);

        if (street == 0) {
            draw_hand_board(rng, dist, hand, board, 0);
        }
        else if (street == 1) {
            draw_hand_board(rng, dist, hand, board, 3);
        }
        else if (street == 2) {
            draw_hand_board(rng, dist, hand, board, 4);
        }
        else {
            draw_hand_board(rng, dist, hand, board, 5);
        }

        auto start = std::chrono::high_resolution_clock::now();

        int bucket = Bucketer::get_bucket(hand, board);

        auto end = std::chrono::high_resolution_clock::now();

        double dt =
            std::chrono::duration<double>(end - start).count();

        if (bucket < 0 || bucket >= 1000) {
            std::cerr << "Invalid bucket: " << bucket << std::endl;
            return 1;
        }

        if (board.size() == 3) {
            flopTime += dt;
            flopCount++;
        }
        else if (board.size() == 4) {
            turnTime += dt;
            turnCount++;
        }
        else if (board.size() == 5) {
            riverTime += dt;
            riverCount++;
        }
    }

    std::cout << "\n=== Bucket Lookup Timing ===\n\n";

    if (flopCount > 0) {
        double avg = flopTime / flopCount;
        std::cout << "Flop:\n";
        std::cout << "  Calls: " << flopCount << "\n";
        std::cout << "  Avg time: " << avg * 1000 << " ms ("
                  << avg * 1e6 << " us)\n";
        std::cout << "  Speed: " << (1.0 / avg) << " /sec\n\n";
    }

    if (turnCount > 0) {
        double avg = turnTime / turnCount;
        std::cout << "Turn:\n";
        std::cout << "  Calls: " << turnCount << "\n";
        std::cout << "  Avg time: " << avg * 1000 << " ms ("
                  << avg * 1e6 << " us)\n";
        std::cout << "  Speed: " << (1.0 / avg) << " /sec\n\n";
    }

    if (riverCount > 0) {
        double avg = riverTime / riverCount;
        std::cout << "River:\n";
        std::cout << "  Calls: " << riverCount << "\n";
        std::cout << "  Avg time: " << avg * 1000 << " ms ("
                  << avg * 1e6 << " us)\n";
        std::cout << "  Speed: " << (1.0 / avg) << " /sec\n\n";
    }

    return 0;
}
