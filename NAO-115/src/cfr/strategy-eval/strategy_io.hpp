#pragma once

/*
 This module is responsible for saving and loading Nao's strategies.
 Provides:
    - saving trained MCCFR strategies of game infosets, using the mapping utilised by Nao.
    - loading them back back for evaluation or to continue training
 
 Has 2 save modes:
 
 1. Full mode (save)
    - stores regretSums AND strategySums
    - preserves MCCFR state
    - used for checkpoints, continued training runs
 
 2. Play mode (saveForPlay)
    - stores strategySums, discards regretSums
    - is used for evals and simulated heads up matches
 
Files written:
    - they have fixed binary layout (header + entries)
    - compressed with zlib

 Format includes:
    - magic number (identity check)
    - versioning
    - flags (mode)
 
 load() reconstructs a usable infosetMap and detects the storage mode via flags
 
 Formatting:
 
    Header:
    [uint32_t magic]         — identification - 0x4E414F54 ("NAOT")
    [uint32_t version]       - format version
    [uint64_t numEntries]    — infoset count
    [uint32_t flags]         — bit 0: play-only mode
                             - bit 1: zlib compression (always set)

    Per entry (full mode):
    [uint64_t historyHash]
    [int32_t  bucketId]
    [float    regretSum[6]]
    [float    strategySum[6]]
    [uint8_t  numActions]
 
    Per entry (play-only mode):
    [uint64_t historyHash]
    [int32_t  bucketId]
    [float    strategySum[6]]
    [uint8_t  numActions]
 
 Notes:
    - format: little-endian
    - structure must match MCCFR::Infoset and InfosetKey definitions
    - link with: -lz
 */

#include "cfr/cfr-core/infoset.hpp"
#include "cfr/external/robin_hood.h"
#include <string>
#include <cstdint>

namespace StrategyIO {
// defines the container used in IO
// key: InfosetKey (hash of history + bucket)
// value: Infoset (regrets + strategy + numActions)
// hasher: boost-combine style hasher used in MCCFR module
using InfosetMap = robin_hood::unordered_flat_map<
    MCCFR::InfosetKey,
    MCCFR::Infoset,
    MCCFR::InfosetKeyHasher>;

static constexpr uint32_t MAGIC = 0x4E414F54; // "NAOT" (v2)
static constexpr uint32_t VERSION = 2;

static constexpr uint32_t FLAG_PLAY_ONLY = 0x1; // strategySum only (no regret values needed for play simulations)
static constexpr uint32_t FLAG_COMPRESSED_ZLIB = 0x2; // payload is zlib-compressed

/*
Writes a full infoset map (including BOTH regretSums and strategySums), compressed with zlib
 - used for continuing training later (basically a checkpoint)
 - it returns true if succeeded
 */
bool save(const InfosetMap& map, const std::string& path);

/*
Writes a reduced infoset map (including strategySums, NOT INCLUDING regretSums), compressed with zlib
 - used for evaluations / simulated matchplaying only, NOT reversible into training version
 - loaded map will have regretSum = 0
 - it returns true if succeeded
 */
bool saveForPlay(const InfosetMap& map, const std::string& path);

/*
Loads any infoset correctly saved by the previous 2 save function versions from file
 1. decompresses zlib payload
 2. validates header (magic, version, flags)
 3. reconstructs infoset map
 
 - automatically detects full map vs play-only map via header flags
 - it returns true if succeeded, false if invalid, not supported or corrupted
 */
bool load(InfosetMap& outInfosets, const std::string& path);

/*
Can read and print file metadata without reconstructing the complete map
(like version, number of infosets, storage mode)
 - is useful for me to help with validation and quick debugs
 */
void inspect(const std::string& path);
}
