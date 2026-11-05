#pragma once

#include "Types.h"
#include <string>

class Game;

// ---------------------------------------------------------------------------
// Save / Load
//
// The JS save format (Game.WriteSave / Game.LoadSave) is a pipe-'|' /
// semicolon-';' delimited text string, UTF-8 -> Base64 encoded, terminated
// with '!END!', then JS escape()'d for storage (localStorage / cookie / file).
//
// pack3() is currently the identity function, so bitfields (prefs, upgrades,
// achievements) are stored as raw '0'/'1' strings. This makes a byte-compatible
// C++ port straightforward: a C++ save can be imported into the browser and
// vice versa.
//
// Sections (in order):
//   0  version
//   1  (reserved, empty)
//   2  runStart;legacyStart;lastDate;bakeryName;seed
//   3  prefs bitstring
//   4  cookies;cookiesEarned;cookieClicks;goldenClicks;handmadeCookies;
//       missedGoldenClicks;bgType;milkType;cookiesReset;elderWrath;pledges;
//       pledgeT;nextResearch;researchT;resets;goldenClicksLocal;cookiesSucked;
//       wrinklersAmount;wrinklersNumber;prestige;heavenlyChips;
//       heavenlyChipsSpent;heavenlyCookies;ascensionMode;
//       permanentUpgrades[0..4];dragonLevel;dragonAura;dragonAura2;chimeType;
//       volume;shinies;shiniesAmount;lumps;lumpsTotal;lumpT;lumpRefill;
//       lumpCurrentType;vault;heralds;fortuneGC;fortuneCPS
//   5  buildings: amount,bought,totalCookies,level,minigameSave,muted; ...
//   6  upgrades bitstring
//   7  achievements bitstring
//   8  buffs: typeId,maxTime,time,arg1,arg2,arg3; ...
//
// Storage form: escape( utf8_to_b64(raw) + "!END!" )
// ---------------------------------------------------------------------------
class SaveLoad {
public:
    // Serialize the current game to the exact JS-compatible raw string.
    static std::string writeSave(Game& game, i32 type = 0);

    // Parse a raw (already unescape'd + b64-decoded) save string into game.
    // Returns false on format/version error.
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
};
