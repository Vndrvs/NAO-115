#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace Eval {

constexpr int HAND_RANKS = 7463;

void initialize();
inline int eval_5(int c1, int c2, int c3, int c4, int c5);
int evaluate5(const std::vector<int>& cards);
int evaluate6(const std::vector<int>& cards);
int evaluate7(const std::vector<int>& cards);
int parseCard(const std::string& cardStr);
int eval_7(const int* cards);

}

