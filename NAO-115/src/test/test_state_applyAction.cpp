#include "state.hpp"
#include <cassert>
#include <iostream>

void test_simple_call() {
    GameState state;
    state.players[0].stack = 1000;
    state.players[1].stack = 1000;
    state.players[0].currentBet = 50;
    state.players[1].currentBet = 100;
    state.currentPlayer = 0;

    applyAction(state, "c");

    assert(state.players[0].stack == 950); // paid 50 to match
    assert(state.players[0].currentBet == 100); // now matches opponent
    assert(state.pot == 50);
    assert(!state.isTerminal);
}

void test_fold() {
    GameState state;
    state.players[0].stack = 500;
    state.players[1].stack = 500;
    state.currentPlayer = 1;

    applyAction(state, "f");

    assert(state.isTerminal);
}

void test_bet_and_call_allin() {
    GameState state;
    state.players[0].stack = 200;
    state.players[1].stack = 200;
    state.currentPlayer = 0;

    applyAction(state, "b200c");

    assert(state.pot == 400);
    assert(state.players[0].stack == 0);
    assert(state.players[1].stack == 0);
    assert(state.isTerminal); // all-in by both
}

void runAllTests() {
    test_simple_call();
    test_fold();
    test_bet_and_call_allin();
    // add more stress cases here
    std::cout << "All tests passed!\n";
}

int main() {
    runAllTests();
    return 0;
}
