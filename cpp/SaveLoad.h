#pragma once

#include "Types.h"
#include <string>
#include <vector>
#include <optional>
#include <array>

class Game;

// ---------------------------------------------------------------------------
// Save / Load
//
// The JS save format (Game.WriteSave / Game.LoadSave) is a pipe-'|' /
// semicolon-';' delimited text string, UTF-8 -> Base64 encoded, terminated
// with '!END!', then JS escape()'d for storage (localStorage / cookie / file).
//
// pack3() is the identity function, so bitfields (prefs, upgrades,
// achievements) are stored as raw '0'/'1' strings. A C++ save can be imported
// into the browser and vice versa.
//
// Parsing is deliberately separated from side effects:
//   * parseSave()  – platform-free: raw string -> SaveData (no Game, no UI).
//   * applySave()  – Game + side effects (stubs where a subsystem is not yet
//                    built; raw values are still stored and re-emitted).
//   * loadSave()   – parseSave() followed by applySave().
//   * writeSave()  – Game state -> raw string, mirroring JS Game.WriteSave.
//
// Sections (in order), mirroring JS Game.WriteSave (v2.058):
//   0  version
//   1  (reserved, empty)
//   2  startDate;fullDate;lastDate;bakeryName;seed;appearance   (6 fields)
//   3  prefs bitstring                                           (27 bits)
//   4  55 misc fields (0..54): cookies, cookiesEarned, cookieClicks,
//       goldenClicks, handmadeCookies, missedGoldenClicks, bgType, milkType,
//       cookiesReset, elderWrath, pledges, pledgeT, nextResearch, researchT,
//       resets, goldenClicksLocal, cookiesSucked, wrinklersPopped, santaLevel,
//       reindeerClicked, seasonT, seasonUses, season, wrinkleAmount,
//       wrinkleNumber, prestige, heavenlyChips, heavenlyChipsSpent,
//       heavenlyCookies, ascensionMode, permanentUpgrades[0..4],
//       dragonLevel, dragonAura, dragonAura2, chimeType, volume,
//       wrinkleShinies, wrinkleAmountShinies, lumps, lumpsTotal, lumpT,
//       lumpRefill, lumpCurrentType, vault, heralds, fortuneGC, fortuneCPS,
//       cookiesPsRawHighest, volumeMusic, cookiesSent, cookiesReceived
//   5  buildings: amount,bought,floor(totalCookies),level,minigameSave,muted,highest;
//   6  upgrades bitstring (unlocked,bought pairs)
//   7  achievements bitstring (won)
//   8  buffs: typeId,maxTime,time,arg1,arg2,arg3;
//   9  mod data (modId:value entries)
//
// Port-only data (not in the real string): `ascensions` (Game.ascensions)
// collides with real field 51 (cookiesPsRawHighest), so it is kept as
// sidecar state only and never serialized.
//
// Storage form: escape( utf8_to_b64(raw) + "!END!" )
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Fully parsed save data – platform-free, no Game / UI dependency.
// ---------------------------------------------------------------------------
struct SaveData {
    f64 version = 0.0;

    // section 2
    f64 startDate = 0.0;
    f64 fullDate = 0.0;
    f64 lastDate = 0.0;
    std::string bakeryName;
    std::string seed;
    std::string appearance;   // YouCustomizer gene string

    // section 3: 27 prefs bits (0..26)
    std::array<bool, 27> prefs{};  // value-init to false

    // section 4: 55 misc fields (0..54)
    f64 cookies = 0.0;
    f64 cookiesEarned = 0.0;
    i32 cookieClicks = 0;
    i32 goldenClicks = 0;
    f64 handmadeCookies = 0.0;
    i32 missedGoldenClicks = 0;
    i32 bgType = 0;
    i32 milkType = 0;
    f64 cookiesReset = 0.0;
    i32 elderWrath = 0;
    i32 pledges = 0;
    f64 pledgeT = 0.0;
    i32 nextResearch = 0;
    f64 researchT = 0.0;
    i32 resets = 0;          // JS field 14 ("ascensions" count)
    i32 goldenClicksLocal = 0;
    f64 cookiesSucked = 0.0;
    i32 wrinklersPopped = 0;
    i32 santaLevel = 0;
    i32 reindeerClicked = 0;
    f64 seasonT = 0.0;
    i32 seasonUses = 0;
    std::string season;
    // wrinklers (SaveWrinklers)
    f64 wrinkleAmount = 0.0;
    i32 wrinkleNumber = 0;
    f64 prestige = 0.0;
    f64 heavenlyChips = 0.0;
    f64 heavenlyChipsSpent = 0.0;
    f64 heavenlyCookies = 0.0;
    i32 ascensionMode = 0;   // JS field 29
    i32 permanentUpgrades[5] = {-1, -1, -1, -1, -1};
    i32 dragonLevel = 0;
    i32 dragonAura = 0;
    i32 dragonAura2 = 0;
    i32 chimeType = 0;
    i32 volume = 0;
    i32 wrinkleShinies = 0;
    f64 wrinkleAmountShinies = 0.0;
    f64 lumps = 0.0;
    f64 lumpsTotal = 0.0;
    f64 lumpT = 0.0;
    f64 lumpRefill = 0.0;
    i32 lumpCurrentType = 0;
    std::vector<i32> vault;
    i32 heralds = 0;         // parseInt (no floor)
    i32 fortuneGC = 0;       // parseInt (no floor)
    i32 fortuneCPS = 0;      // parseInt (no floor)
    f64 cookiesPsRawHighest = 0.0;  // JS field 51 (NOT ascensions)
    i32 volumeMusic = 0;
    i32 cookiesSent = 0;
    i32 cookiesReceived = 0;

    // section 5: buildings
    struct BuildingSave {
        i32 amount = 0;
        i32 bought = 0;
        f64 totalCookies = 0.0;
        i32 level = 0;
        std::string minigameSave;
        i32 muted = 0;
        i32 highest = 0;
    };
    std::vector<BuildingSave> buildings;

    // section 6: upgrades bitstring (unlocked,bought pairs)
    std::string upgradeBits;
    // section 7: achievements bitstring (won)
    std::string achievementBits;

    // section 8: buffs
    struct BuffSave {
        i32 typeId = 0;
        f64 maxTime = 0.0;
        f64 time = 0.0;
        f64 arg1 = 0.0;
        f64 arg2 = 0.0;
        f64 arg3 = 0.0;
    };
    std::vector<BuffSave> buffs;

    // section 9: mod data (modId -> value)
    std::vector<std::pair<std::string, std::string>> modData;
};

class SaveLoad {
public:
    // Serialize the current game to the exact JS-compatible raw string.
    static std::string writeSave(Game& game, i32 type = 0);

    // Platform-free parse: raw string -> SaveData. Returns nullopt on
    // format/version error (truncated, garbage, wrong version). Game state is
    // NOT touched on failure.
    static std::optional<SaveData> parseSave(const std::string& raw);

    // Apply parsed data to Game (side effects; stubs where subsystems are not
    // built). Returns false only on unrecoverable application error.
    static bool applySave(Game& game, const SaveData& data);

    // Parse a raw save string into game. Returns false on format/version
    // error; game state is untouched on failure.
    static bool loadSave(Game& game, const std::string& raw);

    // Convenience: full storage pipeline.
    //   encodeForStorage(raw) -> escape(b64(raw + "!END!"))
    //   decodeFromStorage(stored) -> unescape(...) then b64 decode, strip "!END!"
    static std::string encodeForStorage(const std::string& raw);
    static std::string decodeFromStorage(const std::string& stored);

private:
    // Base64 (RFC 4648) - minimal implementation matching base64.js alphabet
    static std::string base64Encode(const std::string& in);
    static std::string base64Decode(const std::string& in);

    // UTF-8 helpers. JS: utf8_to_b64 = Base64.encode(unescape(encodeURIComponent(s)))
    //                             b64_to_utf8  = decodeURIComponent(escape(Base64.decode(s)))
    // In C++ we operate on raw bytes; these are no-ops for ASCII but keep the
    // pipeline correct for non-ASCII bakery names / seeds.
    static std::string utf8ToBytes(const std::string& s);   // identity in practice
    static std::string bytesToUtf8(const std::string& b);   // identity in practice

    // Legacy packing helpers (ported from main.js): unpack, unpack2,
    // UncompressBin, UncompressLargeBin. pack3 is the identity.
    static std::string unpack2(const std::string& packed);
    static std::vector<i32> uncompressLargeBin(const std::string& arr);
};
