#include "DungeonGen.h"
#include <cstdint>
#include <vector>

// Simple deterministic PRNG for dungeon name generation (no Game dependency).
namespace {
    u32 rngState = 12345u;
    inline u32 xorshift32() {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return rngState;
    }
    inline void seed(u32 s) { rngState = s ? s : 1u; }
    inline i32 randint(i32 n) { return static_cast<i32>(xorshift32() % static_cast<u32>(n)); }
}

const char* const dungeonPrefixes[] = {
    "Mysterious", "Ancient", "Cookie", "Chocolate", "Dark", "Forgotten",
    "Molten", "Glazed", "Frosted", "Sugar", "Obsidian", "Ivory",
    "Caramel", "Midnight", "Golden", "Crimson", "Emerald", "Sapphire",
};
const char* const dungeonSuffixes[] = {
    "Dungeon", "Catacombs", "Vault", "Chamber", "Depths", "Sanctum",
    "Crypt", "Labyrinth", "Citadel", "Nest", "Hollow", "Rift",
};

std::string DungeonGen::generateDungeonName() {
    seed(0x9E3779B9u);
    std::string name = dungeonPrefixes[randint(sizeof(dungeonPrefixes) / sizeof(dungeonPrefixes[0]))];
    name += " ";
    name += dungeonSuffixes[randint(sizeof(dungeonSuffixes) / sizeof(dungeonSuffixes[0]))];
    return name;
}

std::string DungeonGen::generateFloorName(i32 floor) {
    seed(static_cast<u32>(floor) * 0x9E3779B9u);
    std::string name = "Floor ";
    name += std::to_string(floor);
    name += " - ";
    name += dungeonPrefixes[randint(sizeof(dungeonPrefixes) / sizeof(dungeonPrefixes[0]))];
    name += " ";
    name += dungeonSuffixes[randint(sizeof(dungeonSuffixes) / sizeof(dungeonSuffixes[0]))];
    return name;
}
