#include "match_engine.hpp"
#include "pseudo_harmonic.hpp"
#include "cfr/cfr-core/game_engine.hpp"
#include "cfr/cfr-core/infoset.hpp"
#include "cfr/utils/zobrist.hpp"
#include "bet-abstraction/bet_sequence.hpp"
#include "bet-abstraction/bet_utils.hpp"
#include "eval/evaluator.hpp"
#include "../include/bucket-lookups/lut_indexer.hpp"

#include <random>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <vector>
#include <algorithm>

namespace MatchEngine {

static void fractionToRatio(float fraction, int32_t& numerator, int32_t& denominator) {
    
    if (fraction <= 0.0f) {
        numerator = 0;
        denominator = 1;
        return;
    }
    
    constexpr int MAX_DENOMINATOR = 32;
    float bestError = std::numeric_limits<float>::max();
    int32_t bestNumerator = 0;
    int32_t bestDenominator = 1;
    
    for (int32_t candidateDenominator = 1; candidateDenominator <= MAX_DENOMINATOR; ++candidateDenominator) {
        int32_t candidateNumerator = static_cast<int32_t>(std::round(fraction * candidateDenominator));
        float candidateValue = static_cast<float>(candidateNumerator) / candidateDenominator;
        float error = std::fabs(candidateValue - fraction);
        
        if (candidateNumerator <= 0) {
            continue;
        }
        
        // early exit: perfect (or near-perfect) candidate bet found
        if (error < 1e-7f) {
            numerator = candidateNumerator;
            denominator = candidateDenominator;
            return;
        }

        if (error < bestError ||
           (std::fabs(error - bestError) < 1e-9f && candidateDenominator < bestDenominator)) {
            bestError = error;
            bestNumerator = candidateNumerator;
            bestDenominator = candidateDenominator;
        }
    }
    
    numerator = bestNumerator;
    denominator = bestDenominator;
}

StrategyProfile buildProfile(StrategyIO::InfosetMap map,
                             const float flopSizes[3],
                             const float turnSizes[3],
                             const float riverSizes[3])
{
    StrategyProfile profile;
    profile.map = std::move(map);
    profile.numSizes = 3;
    
    for (int i = 0; i < profile.numSizes; ++i) {
        profile.flop[i]  = flopSizes[i];
        profile.turn[i]  = turnSizes[i];
        profile.river[i] = riverSizes[i];
        
        fractionToRatio(flopSizes[i],  profile.flop_n[i],  profile.flop_d[i]);
        fractionToRatio(turnSizes[i],  profile.turn_n[i],  profile.turn_d[i]);
        fractionToRatio(riverSizes[i], profile.river_n[i], profile.river_d[i]);
    }
    
    return profile;
}

static void applyConfig(const StrategyProfile& profile) {
    auto& bc = BetAbstraction::g_betConfig;
    for (int i = 0; i < 3; ++i) {
        bc.flop_numerators[i]    = profile.flop_n[i];
        bc.flop_denominators[i]  = profile.flop_d[i];
        bc.turn_numerators[i]    = profile.turn_n[i];
        bc.turn_denominators[i]  = profile.turn_d[i];
        bc.river_numerators[i]   = profile.river_n[i];
        bc.river_denominators[i] = profile.river_d[i];
    }
}

static MCCFRState makeRoot() {
    MCCFRState state{};
    state.bigBlind            = 100;
    state.street              = 1;
    state.raiseCount          = 0;
    state.currentPlayer       = 0;
    state.isTerminal          = false;
    state.foldedPlayer        = -1;
    state.potBase             = 2000;
    state.heroStreetBet       = 0;
    state.villainStreetBet    = 0;
    state.heroStack           = 9000;
    state.villainStack        = 9000;
    state.previousRaiseTotal  = 0;
    state.betBeforeRaise      = 0;
    state.historyHash         = 0;
    state.bucketId            = 0;
    state.streetHasCheck      = false;
    state.player0Contribution = 1000;
    state.player1Contribution = 1000;
    return state;
}

static int32_t getBucket(const MCCFRState& state,
                          const std::array<int,2>& player0Hand,
                          const std::array<int,2>& player1Hand,
                          const std::array<int,5>& boardCards,
                          Bucketer::IsomorphismEngine& isoEngine) {
    const std::array<int,2>& actingHand = (state.currentPlayer == 0) ? player0Hand : player1Hand;
    int boardCardCount = 0;
    // decide on boardSize amount based on street
    // can be used in wider implementation, which would include preflop
    if (state.street == 0) {
        boardCardCount = 0;  // preflop
    } else if (state.street == 1) {
        boardCardCount = 3;  // flop
    } else if (state.street == 2) {
        boardCardCount = 4;  // turn
    } else {
        boardCardCount = 5;  // river
    }
                    
    // delegate to LUT-based bucketing module
    return Bucketer::lookup_bucket(isoEngine, actingHand.data(), boardCards.data(), boardCardCount);
}

static int sampleAction(const StrategyIO::InfosetMap& map, const MCCFR::InfosetKey& infosetKey, int actionCount, float random01) {
    float strategy[MCCFR::MAX_ACTIONS];
    
    auto infosetIt = map.find(infosetKey);
    if (infosetIt != map.end() && infosetIt->second.numActions == actionCount) {
        // use learned average strategy for current game situation
        infosetIt->second.getAverageStrategy(strategy);
    } else {
        // fallback: uniform random over available actions
        float u = 1.0f / actionCount;
        for (int i = 0; i < actionCount; ++i) {
            strategy[i] = u;
        }
    }
    
    // sample action using cumulative probability
    float cumulativeProb = 0.0f;
    for (int i = 0; i < actionCount - 1; ++i) {
        cumulativeProb += strategy[i];
        if (random01 < cumulativeProb) {
            return i;
        }
    }
    
    // fallback to last action (handles FP drift / sum < 1.0)
    return actionCount - 1;
}

static const float* getAbstractSizes(const StrategyProfile& profile, int street) {
    switch (street) {
        case 1: 
            return profile.flop;
        case 2: 
            return profile.turn;
        case 3: 
            return profile.river;
        default: 
            return profile.flop;
    }
}

inline int translateBetFraction(const float* abstractSizes,
                                int numSizes,
                                float x,
                                float rng)
{
    return PHM::translateBet(abstractSizes, numSizes, x, 1.0f, rng);
}

/*
Internal: Simulates one full poker hand and returns payoff for Player 0 in chips amount
 if the opponent's last bet was off-tree, PHM handles the case: search the bracketing abstract actions and sample one
 probabilistically before looking up the actor's strategy
 
 - returns P0 payoff
 
 Note: assumption made - P1 payoff = -result
 */
static float playHand(const StrategyProfile& p0Profile, // strategy + bet abstraction for p0
                      const StrategyProfile& p1Profile, // strategy + bet abstraction for p1
                      const std::array<int,2>& player0hand,
                      const std::array<int,2>& player1hand,
                      const std::array<int,5>& board,
                      Bucketer::IsomorphismEngine& isoEngine,
                      std::mt19937& random01) {
    // converts random to float for action and pseudo-harmonic mapping
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    MCCFRState state = makeRoot();
    
    // we keep playing until fold / showdown / all-in resolve
    while (!state.isTerminal) {
        const StrategyProfile* actorPointer;

        // pick which player is acting now
        if (state.currentPlayer == 0) {
            actorPointer = &p0Profile;
        } else {
            actorPointer = &p1Profile;
        }

        const StrategyProfile& actor = *actorPointer;
        
        // writes the bet sizes for current player into global bet configuration (used in bet sequencing)
        applyConfig(actor);
        // call bet sequencing module to get possible actions for this game moment
        auto legalActions = BetAbstraction::getLegalActions(state);
        const float* abstractBetSizes = getAbstractSizes(actor, state.street);
        
        // compute call amount, because if 0, we can skip pseudo-harmonic mapping
        int32_t chipsToCall = state.villainStreetBet - state.heroStreetBet;
        
        if (chipsToCall > 0 && !state.anyAllIn()) {
            int32_t lastBetChips = chipsToCall;
            // calculate opponent's bet as the fraction of the pot
            float potBeforeBet = static_cast<float>(state.totalPot() - lastBetChips);
            float opponentBetFraction;

            if (potBeforeBet > 0.0f) {
                opponentBetFraction = static_cast<float>(lastBetChips) / potBeforeBet;
            } else {
                opponentBetFraction = 1.0f;
            }
            // check if opponent bet is close to any exiting raise size in the available actions
            // NOTE: not exact mathematically
            bool isOnTree = false;
            for (int i = 0; i < actor.numSizes; ++i) {
                if (std::fabs(abstractBetSizes[i] - opponentBetFraction) < 0.05f) {
                    isOnTree = true;
                    break;
                }
            }
            
            if (!isOnTree) {
               // if bet is off-tree, we need to kick in pseudo-harmonic mapping (PHM) module
                float rngVal = dist(random01); // PHM needs random value to work
                int translatedIdx = PHM::translateBet(
                    abstractBetSizes,
                    actor.numSizes,
                    opponentBetFraction,
                    1.0f, // already normalized to fraction of pot
                    rngVal
                );
                
                // fold and call/check are always the first two entries in the current list
                // PLEASE MIND: layout will break if bet sequence logic is overwritten
                int raiseActionIdx = 2 + translatedIdx;
                if (raiseActionIdx >= legalActions.count) {
                    raiseActionIdx = legalActions.count - 1;
                }
                
                // here I construct a 'fake state', to be able to query the strategy as if the opponent bet was abstract
                MCCFRState phmState = state;
                phmState.villainStreetBet = legalActions.actions[raiseActionIdx].amount;
                
                // run hand bucket query through LUT module and get infoset from MCCFR map
                int32_t handBucket = getBucket(phmState, player0hand, player1hand, board, isoEngine);
                MCCFR::InfosetKey infosetKey{phmState.historyHash, handBucket};
                
                // need actions from the translated state to know the sizing of the strategy array
                auto translatedActions = BetAbstraction::getLegalActions(phmState);
                
                // get sample from average MCCFR strategy or uniform fallback case
                int chosenIdx = sampleAction(actor.map, infosetKey, translatedActions.count, dist(random01));
                
                // apply real action (devised by 'PHM-world' logic, executed in real-world)
                BetAbstraction::AbstractAction physicalAction = legalActions.actions[chosenIdx];
                
                // apply action and advance game
                state.historyHash ^= Zobrist::TABLE[state.street][state.currentPlayer][state.raiseCount][chosenIdx];
                state = GameEngine::applyAction(state, physicalAction);
                continue;
            }
        }
        
        // if no PHM is needed, we construct a real state and sample directly from relevant strategy map
        int32_t handBucket = getBucket(state, player0hand, player1hand, board, isoEngine);
        MCCFR::InfosetKey infosetKey{state.historyHash, handBucket};
        
        int chosenIdx = sampleAction(actor.map, infosetKey, legalActions.count, dist(random01));
        BetAbstraction::AbstractAction chosenAction = legalActions.actions[chosenIdx];
        
        state.historyHash ^= Zobrist::TABLE[state.street][state.currentPlayer][state.raiseCount][chosenIdx];
        state = GameEngine::applyAction(state, chosenAction);
    }
    // compute result
    int payoff = GameEngine::getPayoff(state, player0hand, player1hand, board);
    
    // return P0 payoff
    return static_cast<float>(payoff);
}

static float playDuplicatePair(const StrategyProfile& profileA,
                               const StrategyProfile& profileB,
                               const std::array<int,2>& player0hand,
                               const std::array<int,2>& player1hand,
                               const std::array<int,5>& board,
                               Bucketer::IsomorphismEngine& isoEngine,
                               std::mt19937& random01) {
    // Hand 1: A as P0, B as P1 — get A's payoff directly
    float ev1 = playHand(profileA, profileB, player0hand, player1hand, board, isoEngine, random01);
    
    // Hand 2: B as P0, A as P1
    // A's EV as P1 = -(P0's payoff) since zero-sum
    float ev2 = -playHand(profileB, profileA, player0hand, player1hand, board, isoEngine, random01);
    
    // duplicate score: average of A's EV in both positions
    return (ev1 + ev2) * 0.5f;
}

MatchResult runMatch(const StrategyProfile& profileA,
                     const StrategyProfile& profileB,
                     Bucketer::IsomorphismEngine& isoEngine,
                     int numPairs,
                     int bigBlind,
                     uint64_t seed) {
    
    std::mt19937 random01(seed);
    
    float sum = 0.0f;
    float sumSquared = 0.0f;
    
    for (int p = 0; p < numPairs; ++p) {
        int deck[52];
        for (int i = 0; i < 52; ++i) {
            deck[i] = i;
        }
        // partial Fisher-Yates: only shuffle first 9 cards
        for (int i = 0; i < 9; ++i) {
            std::uniform_int_distribution<int> dist(i, 51);
            int j = dist(random01);
            std::swap(deck[i], deck[j]);
        }
        
        std::array<int,2> player0hand = {deck[0], deck[1]};
        std::array<int,2> player1hand = {deck[2], deck[3]};
        std::array<int,5> board = {deck[4], deck[5], deck[6], deck[7], deck[8]};
        
        float score = playDuplicatePair(profileA, profileB, player0hand, player1hand, board, isoEngine, random01);
        
        sum += score;
        sumSquared += score * score;
    }
    
    fprintf(stderr, "\n");
    
    float mean = sum / numPairs;
    float variance = (sumSquared / numPairs) - (mean * mean);
    float stddev = std::sqrt(variance > 0.0f ? variance : 0.0f);
    float stderr_v = stddev / std::sqrt(static_cast<float>(numPairs));
    
    float chips_to_mbb = 1000.0f / static_cast<float>(bigBlind);
    
    MatchResult result;
    result.mean_ev_mbb = mean * chips_to_mbb;
    result.std_error_mbb = stderr_v * chips_to_mbb;
    result.confidence_95_mbb = 1.96f * stderr_v * chips_to_mbb;
    result.num_pairs = numPairs;
    result.total_hands = numPairs * 2;
    
    return result;
}

}
