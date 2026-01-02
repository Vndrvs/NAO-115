#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace Eval {

    void initialize();
    int evaluate7(const std::vector<int>& cards);
    int parseCard(const std::string& cardStr);

}
