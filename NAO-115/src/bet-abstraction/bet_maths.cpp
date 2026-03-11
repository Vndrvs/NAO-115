#include "bet_maths.hpp"

namespace BetAbstraction {

// this function will get the raise multiplier and return the raise amount
// if the raise amount is higher than available chips, it will return the all-in amount
int computeAmount(float fraction, int pot, int villainBet, int heroCurrentBet, int currentStack) {
    // call-first geometry:
    // simulate calling the villain bet first, then compute the pot
    // handles any errors if heroCurrentBet is accidentally bigger than villainBet
    int callAmount = std::max(0, villainBet - heroCurrentBet);
    int potAfterCall = pot + villainBet + callAmount;
    
    // fraction of the pot after calling
    // raiseAmount is the additional chips beyond a call
    int raiseAmount = static_cast<int>(std::floor(potAfterCall * fraction));
    // total bet = what you already put in + call + raise
    int totalBet = heroCurrentBet + callAmount + raiseAmount;
    // cap at current stack (all-in)
    return std::min(totalBet, currentStack);
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

// this function will get all raise sizes
// filter out the ones below the minimum raise
// and the ones above our current stack
std::array<int, 3> filterAndDeduplicate(std::array<int, 3> amounts, int minRaise, int currentStack) {
    //
    for (int& amount : amounts) {
        if (amount >= currentStack) {
            amount = 0;
        }
    }
    
    int previous = -1;
    std::array<int, 3> output = {-1, -1, -1};
    for (int i = 0; i < MAX_RAISE_SIZES; i++) {
        int a = amounts[i];
        if (a >= minRaise && a != previous) {
            output[i] = a;
            previous = a;
        }
    }
    return output;
}

std::vector<int> filterAndDeduplicate(std::vector<int> amounts, int minRaise, int currentStack) {
    /*
    printf("  filterAndDeduplicate: in=[");
        for (int a : amounts) printf("%d ", a);
        printf("] minRaise=%d currentStack=%d\n", minRaise, currentStack);
    */
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

    /*
    printf("  filterAndDeduplicate: out=[");
       for (int a : amounts) printf("%d ", a);
       printf("]\n");
    */
    return amounts;
}

}
