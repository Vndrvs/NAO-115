#include "strategy_io.hpp"
#include <zlib.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

namespace StrategyIO {
/*
These structs define the exact binary layout used for serialization
 & are written and read via memcpy, so their layout must remain stable

 Properties:
    - packed (no padding)
    - little-endian (host-dependent; must have compatible architecture)
    - float = IEEE-754 32-bit
    - changing field order, sizes, max-actions breaks functionality
*/
#pragma pack(push, 1)

struct Header {
    uint32_t magic; // file identifier
    uint32_t version; // format version
    uint64_t numEntries; // number of infosets
    uint32_t flags; // mode + compression flags
};
static_assert(sizeof(Header) == 20, "Header must be 20 bytes");

struct FullEntry {
    uint64_t historyHash;
    int32_t  bucketId;
    float    regretSum[MCCFR::MAX_ACTIONS];
    float    strategySum[MCCFR::MAX_ACTIONS];
    uint8_t  numActions;
};
static_assert(sizeof(FullEntry) == 8 + 4 + 24 + 24 + 1, "Full Entry must be 61 bytes");

struct PlayEntry {
    uint64_t historyHash;
    int32_t  bucketId;
    float    strategySum[MCCFR::MAX_ACTIONS];
    uint8_t  numActions;
};
static_assert(sizeof(PlayEntry) == 8 + 4 + 24 + 1, "Play Entry must be 37 bytes");

#pragma pack(pop)


/* 
Internal: Compress & Write (compresses raw serialized buffer, writes it to disk)
 
 File layout:
 [uint64_t originalSize]
 [compressed payload]
 
 - the size prefix is required for zlib decompression
 - this function does not understand poker logic
 - used by both full and play-only saves

 */

static bool writeZlibInfosets(const std::string& path, const void* buffer, size_t size) {
    uLongf compressedSize = compressBound(static_cast<uLong>(size));
    std::vector<Bytef> compressed(compressedSize);
    
    int ret = compress2(
                        compressed.data(), &compressedSize,
                        static_cast<const Bytef*>(buffer), static_cast<uLong>(size),
                        Z_BEST_SPEED
                        );
    
    if (ret != Z_OK) {
        fprintf(stderr, "StrategyIO: zlib compress failed (%d)\n", ret);
        return false;
    }
    
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        fprintf(stderr, "StrategyIO: cannot open %s\n", path.c_str());
        return false;
    }
    
    uint64_t originalSize = static_cast<uint64_t>(size);
    
    if (fwrite(&originalSize, sizeof(originalSize), 1, f) != 1) {
        fprintf(stderr, "StrategyIO: failed to write size header\n");
        fclose(f);
        return false;
    }
    
    if (fwrite(compressed.data(), 1, compressedSize, f) != compressedSize) {
        fprintf(stderr, "StrategyIO: failed to write compressed data\n");
        fclose(f);
        return false;
    }
    
    fclose(f);
    return true;
}

/*
 Internal: Reads and decompresses a saved Nao strategy file, buffer is later parsed into infosets by load()

 File layout:
 [ uint64_t originalSize ]
 [ zlib compressed blob ]
 */
static bool readZlibInfosets(const std::string& path, std::vector<uint8_t>& outData) {
    
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "StrategyIO: cannot open %s for reading\n", path.c_str());
        return false;
    }
    
    uint64_t uncompressedSize;
    if (fread(&uncompressedSize, sizeof(uncompressedSize), 1, f) != 1) {
        fprintf(stderr, "StrategyIO: failed to read size header from %s\n", path.c_str());
        fclose(f);
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    if (fileSize < 0) {
        fprintf(stderr, "StrategyIO: ftell failed for %s\n", path.c_str());
        fclose(f);
        return false;
    }
    
    fseek(f, sizeof(uint64_t), SEEK_SET);
    size_t compressedSize = fileSize - sizeof(uint64_t);
    
    std::vector<uint8_t> compressedBlob(compressedSize);
    if (fread(compressedBlob.data(), 1, compressedSize, f) != compressedSize) {
        fprintf(stderr, "StrategyIO: read error in %s\n", path.c_str());
        fclose(f);
        return false;
    }
    
    fclose(f);
    
    outData.resize(uncompressedSize);
    uLongf destLen = static_cast<uLongf>(uncompressedSize);
    
    int ret = uncompress(outData.data(), &destLen, compressedBlob.data(), static_cast<uLong>(compressedSize));
    
    if (ret != Z_OK || destLen != uncompressedSize) {
        fprintf(stderr, "StrategyIO: zlib decompress failed or size mismatch\n");
        return false;
    }
    
    return true;
}

/*
 Internal: Serializes full MCCFR state (training snapshot)
 
 Each infoset stores:
  - regretSum   → used to update strategy during training
  - strategySum → accumulated strategy for averaging
 
Layout:
 [ Header ]
 [ FullEntry x N]

 Note:
  - preserves complete solver state
  - larger than play-only mode
 */
static std::vector<uint8_t> serializeFullInfosets(const InfosetMap& map) {
    
    if (map.size() > (SIZE_MAX - sizeof(Header)) / sizeof(FullEntry)) {
        throw std::runtime_error("StrategyIO: buffer size overflow");
    }
    
    const size_t totalSize = sizeof(Header) + map.size() * sizeof(FullEntry);
    std::vector<uint8_t> buffer(totalSize);
    
    uint8_t* writePointer = buffer.data();
    
    Header header{};
    header.magic = MAGIC;
    header.version = VERSION;
    header.numEntries = map.size();
    header.flags = FLAG_COMPRESSED_ZLIB; // full mode (no play-only flag)
    memcpy(writePointer, &header, sizeof(header));
    writePointer += sizeof(header);
    
    for (const auto& [key, infoset] : map) {
        FullEntry entry{};
        entry.historyHash = key.historyHash;
        entry.bucketId = key.bucketId;
        
        if (infoset.numActions < 2 || infoset.numActions > MCCFR::MAX_ACTIONS) {
            throw std::runtime_error("StrategyIO: invalid numActions");
        }
        
        entry.numActions = infoset.numActions;
        
        for (int i = 0; i < MCCFR::MAX_ACTIONS; ++i) {
            entry.regretSum[i] = infoset.regretSum[i];
            entry.strategySum[i] = infoset.strategySum[i];
        }
        
        memcpy(writePointer, &entry, sizeof(entry));
        writePointer += sizeof(entry);
    }
    
    return buffer;
}
/*
 Internal: Serializes play-only strategy

 Used for:
  - head-to-head matches
  - BO evaluation
  - exporting strategies for play
 
Layout:
 [ Header ]
 [ PlayEntry x N ]

 - NOT suitable for resuming training: only strategySum is stored (regretSum is not present)
 - returned buffer is later compressed and written to disk
*/
static std::vector<uint8_t> serializePlayInfosets(const InfosetMap& map) {
    if (map.size() > (SIZE_MAX - sizeof(Header)) / sizeof(PlayEntry)) {
        throw std::runtime_error("StrategyIO: buffer size overflow");
    }
    size_t totalSize = sizeof(Header) + map.size() * sizeof(PlayEntry);
    std::vector<uint8_t> buffer(totalSize);
    
    uint8_t* writePointer = buffer.data();
    
    Header header{};
    header.magic      = MAGIC;
    header.version    = VERSION;
    header.numEntries = map.size();
    header.flags = FLAG_PLAY_ONLY | FLAG_COMPRESSED_ZLIB; // play only flag set
    memcpy(writePointer, &header, sizeof(header));
    writePointer += sizeof(header);
    
    for (const auto& [key, infoset] : map) {
        PlayEntry entry{};
        entry.historyHash = key.historyHash;
        entry.bucketId = key.bucketId;
        if (infoset.numActions < 2 || infoset.numActions > MCCFR::MAX_ACTIONS) {
            throw std::runtime_error("StrategyIO: invalid numActions");
        }
        
        entry.numActions = infoset.numActions;
        for (int i = 0; i < MCCFR::MAX_ACTIONS; ++i) {
            entry.strategySum[i] = infoset.strategySum[i];
        }
        
        memcpy(writePointer, &entry, sizeof(entry));
        writePointer += sizeof(entry);
    }
    
    return buffer;
}

// PUBLIC API funcitons

bool save(const InfosetMap& map, const std::string& path) {
    fprintf(stderr, "StrategyIO: saving %zu infosets (full mode)...\n", map.size());
    auto serialized = serializeFullInfosets(map);
    return writeZlibInfosets(path, serialized.data(), serialized.size());
}

bool saveForPlay(const InfosetMap& map, const std::string& path) {
    fprintf(stderr, "StrategyIO: saving %zu infosets (strategy snapshot for evaluation)...\n", map.size());
    auto serialized = serializePlayInfosets(map);
    return writeZlibInfosets(path, serialized.data(), serialized.size());
}

/*
Loads a saved Nao strategy into memory.

Steps:
 1. Decompress file into raw buffer
 2. Validate header (magic, version, flags) - automatically detects full vs play-only mode
 3. Reconstruct infoset map

Output:
 - outInfosets becomes a fully usable strategy for:
     - evaluation (always)
     - training (only if full mode)

Fails if:
 - file is corrupted
 - format/version mismatch
 - invalid entry data
*/
bool load(InfosetMap& outInfosets, const std::string& path) {
    std::vector<uint8_t> decompressed;
    if (!readZlibInfosets(path, decompressed)) return false;
    
    const uint8_t* readPointer = decompressed.data();
    const uint8_t* bufferEnd = decompressed.data() + decompressed.size();
    
    Header header{};
    
    if (readPointer + sizeof(Header) > bufferEnd) {
        fprintf(stderr, "StrategyIO: truncated file (header)\n");
        return false;
    }
    
    memcpy(&header, readPointer, sizeof(header));
    readPointer += sizeof(header);
    
    if (header.magic != MAGIC) {
        fprintf(stderr, "StrategyIO: bad magic in %s (got 0x%X expected 0x%X)\n",
                path.c_str(), header.magic, MAGIC);
        return false;
    }
    
    if (header.version != VERSION) {
        fprintf(stderr, "StrategyIO: version mismatch in %s (got %u expected %u)\n",
                path.c_str(), header.version, VERSION);
        return false;
    }
    
    const uint32_t allowedFlags = FLAG_PLAY_ONLY | FLAG_COMPRESSED_ZLIB;
    
    if (header.flags & ~allowedFlags) {
        fprintf(stderr, "StrategyIO: unknown flags 0x%X\n", header.flags);
        return false;
    }
    
    bool playOnly = (header.flags & FLAG_PLAY_ONLY) != 0;
    size_t numEntries = header.numEntries;
    
    outInfosets.clear();
    outInfosets.reserve(numEntries);
    
    if (!playOnly) {
        for (size_t i = 0; i < numEntries; ++i) {
            if (readPointer + sizeof(FullEntry) > bufferEnd) {
                fprintf(stderr, "StrategyIO: truncated file (full entry %zu)\n", i);
                return false;
            }
            
            FullEntry entry{};
            memcpy(&entry, readPointer, sizeof(entry));
            readPointer += sizeof(entry);
            
            MCCFR::InfosetKey key{entry.historyHash, entry.bucketId};
            MCCFR::Infoset infoset{};
            if (entry.numActions < 2 || entry.numActions > MCCFR::MAX_ACTIONS) {
                fprintf(stderr, "StrategyIO: invalid action count (%u) in infoset\n", entry.numActions);
                return false;
            }
            
            infoset.numActions = entry.numActions;
            for (int j = 0; j < MCCFR::MAX_ACTIONS; ++j) {
                infoset.regretSum[j]   = entry.regretSum[j];
                infoset.strategySum[j] = entry.strategySum[j];
            }
            
            outInfosets[key] = infoset;
        }
    } else {
        for (size_t i = 0; i < numEntries; ++i) {
            if (readPointer + sizeof(PlayEntry) > bufferEnd) {
                fprintf(stderr, "StrategyIO: truncated file (play entry %zu)\n", i);
                return false;
            }
            
            PlayEntry entry{};
            memcpy(&entry, readPointer, sizeof(entry));
            readPointer += sizeof(entry);
            
            MCCFR::InfosetKey key{entry.historyHash, entry.bucketId};
            MCCFR::Infoset infoset{};
            if (entry.numActions < 2 || entry.numActions > MCCFR::MAX_ACTIONS) {
                fprintf(stderr, "StrategyIO: invalid numActions (%u)\n", entry.numActions);
                return false;
            }
            infoset.numActions = entry.numActions;
            for (int j = 0; j < MCCFR::MAX_ACTIONS; ++j) {
                infoset.strategySum[j] = entry.strategySum[j];
                infoset.regretSum[j] = 0.0f; // not saved in play mode
            }
            outInfosets[key] = infoset;
        }
    }
    return true;
}

void inspect(const std::string& path) {
    std::vector<uint8_t> decompressed;
    if (!readZlibInfosets(path, decompressed)) return;
    
    if (decompressed.size() < sizeof(Header)) {
        fprintf(stderr, "StrategyIO: file too small to contain header\n");
        return;
    }
    
    Header header;
    memcpy(&header, decompressed.data(), sizeof(Header));
    
    fprintf(stdout, "File: %s\n", path.c_str());
    fprintf(stdout, "Version: %u\n", header.version);
    fprintf(stdout, "Infosets: %llu\n", (unsigned long long)header.numEntries);
    fprintf(stdout, "Mode: %s\n",
            (header.flags & FLAG_PLAY_ONLY) ? "play-only" : "full");
    fprintf(stdout, "Raw size:  %.1f MB\n", decompressed.size() / (1024.0 * 1024.0));
}

}
