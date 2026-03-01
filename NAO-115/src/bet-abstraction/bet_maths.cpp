#include "bet_maths.hpp"

namespace BetAbstraction {

int computePostflopAmount(float fraction, int pot, int villainBet, int heroCurrentBet, int currentStack) {
    
    // call-first geometry:
    // simulate calling the villain bet first, then compute the pot
    // handles any errors if heroCurrentBet is accidentally bigger than villainBet
    int callAmount = std::max(0, villainBet - heroCurrentBet);
    int potAfterCall = pot + callAmount;

    // fraction of the pot after calling
    // raiseAmount is the additional chips beyond a call
    int raiseAmount = static_cast<int>(std::floor(potAfterCall * fraction));

    // total bet = what you already put in + call + raise
    int totalBet = heroCurrentBet + callAmount + raiseAmount;

    // cap at current stack (all-in)
    return std::min(totalBet, currentStack);
}

int computePreflopOpen(float multiplier, int bigBlind, int currentStack) {
    int amount = static_cast<int>(std::floor(multiplier * bigBlind));
    return std::min(amount, currentStack);
}

int computePreflopReraise(float multiplier, int previousRaiseTotal, int currentStack) {
    int amount = static_cast<int>(std::floor(multiplier * previousRaiseTotal));
    return std::min(amount, currentStack);
}

int computeMinRaise(int previousRaiseTotal, int betBeforeRaise) {
    // the increment of the previous raise
    int previousIncrement = previousRaiseTotal - betBeforeRaise;
    // minimum legal re-raise total
    return previousRaiseTotal + previousIncrement;
}

bool isAllIn(int amount, int currentStack) {
    return amount >= currentStack;
}

std::vector<int> filterAndDeduplicate(std::vector<int> amounts, int minRaise, int currentStack) {
    // cap anything equal or above currentStack to currentStack
    for (int& a : amounts) {
        if (a >= currentStack) {
            a = currentStack;
        }
    }

    // filter out anything below minimum raise
    amounts.erase(
        std::remove_if(amounts.begin(), amounts.end(), [minRaise](int a) {
            return a < minRaise;
        }),
        amounts.end()
    );

    // sort ascending
    std::sort(amounts.begin(), amounts.end());

    // remove duplicates
    amounts.erase(std::unique(amounts.begin(), amounts.end()), amounts.end());

    // always ensure all-in is present
    if (amounts.empty() || amounts.back() != currentStack) {
        amounts.push_back(currentStack);
    }

    return amounts;
}

}
