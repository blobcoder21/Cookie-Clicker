#include "SaveLoad.h"
#include "Game.h"
#include "Building.h"
#include "jsnum.h"

#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <cstring>

// ---------------------------------------------------------------------------
// Base64 (RFC 4648) - matches base64.js alphabet.
// ---------------------------------------------------------------------------
std::string SaveLoad::base64Encode(const std::string& in) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    u32 i = 0;
    while (i < in.size()) {
        u32 chr1 = static_cast<u8>(in[i++]);
        bool have2 = (i < in.size());
        u32 chr2 = have2 ? static_cast<u8>(in[i++]) : 0;
        bool have3 = (i < in.size());
        u32 chr3 = have3 ? static_cast<u8>(in[i++]) : 0;

        u32 enc1 = chr1 >> 2;
        u32 enc2 = ((chr1 & 3) << 4) | (chr2 >> 4);
        u32 enc3 = ((chr2 & 15) << 2) | (chr3 >> 6);
        u32 enc4 = chr3 & 63;

        if (!have2) enc3 = enc4 = 64;
        else if (!have3) enc4 = 64;

        out.push_back(alphabet[enc1]);
        out.push_back(alphabet[enc2]);
        out.push_back(alphabet[enc3]);
        out.push_back(alphabet[enc4]);
    }
    return out;
}

std::string SaveLoad::base64Decode(const std::string& in) {
    std::string filtered;
    filtered.reserve(in.size());
    for (char c : in) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=') {
            filtered.push_back(c);
        }
    }

    auto val = [](char c) -> i32 {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        if (c == '=') return 64;
        return -1;
    };

    std::string out;
    u32 i = 0;
    while (i < filtered.size()) {
        i32 enc1 = val(filtered[i++]);
        i32 enc2 = val(filtered[i++]);
        i32 enc3 = val(filtered[i++]);
        i32 enc4 = val(filtered[i++]);

        u32 chr1 = (enc1 << 2) | (enc2 >> 4);
        u32 chr2 = ((enc2 & 15) << 4) | (enc3 >> 2);
        u32 chr3 = ((enc3 & 3) << 6) | enc4;

        out.push_back(static_cast<char>(chr1));
        if (enc3 != 64) out.push_back(static_cast<char>(chr2));
        if (enc4 != 64) out.push_back(static_cast<char>(chr3));
    }
    return out;
}

std::string SaveLoad::utf8ToBytes(const std::string& s) { return s; }
std::string SaveLoad::bytesToUtf8(const std::string& b) { return b; }

// ---------------------------------------------------------------------------
// Outer storage encoding/decoding.
//
// JS:  save  = escape( utf8_to_b64(raw) + "!END!" )
// JS:  load  = b64_to_utf8( unescape(stored).split('!END!')[0] )
//
// The objective requires stripping the terminator and any whitespace/newlines
// before decoding.
// ---------------------------------------------------------------------------
std::string SaveLoad::encodeForStorage(const std::string& raw) {
    std::string b64 = base64Encode(utf8ToBytes(raw)) + "!END!";
    std::string out;
    out.reserve(b64.size() * 2);
    for (char c : b64) {
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '@' || c == '*' || c == '-' || c == '_' || c == '+' || c == '.') {
            out.push_back(c);
        } else if (c == ' ') {
            out += "%20";
        } else {
            u8 b = static_cast<u8>(c);
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%%%02X", b);
            out += buf;
        }
    }
    return out;
}

std::string SaveLoad::decodeFromStorage(const std::string& stored) {
    // JS unescape(): %xx -> byte, %uXXXX -> char; rest unchanged.
    std::string unescaped;
    unescaped.reserve(stored.size());
    for (u32 i = 0; i < stored.size(); ++i) {
        char c = stored[i];
        if (c == '%' && i + 2 < stored.size()) {
            char h[3] = {stored[i + 1], stored[i + 2], 0};
            u8 b = static_cast<u8>(std::strtoul(h, nullptr, 16));
            unescaped.push_back(static_cast<char>(b));
            i += 2;
        } else if (c == '%' && i + 5 < stored.size() && stored[i + 1] == 'u') {
            char h[5] = {stored[i + 2], stored[i + 3], stored[i + 4],
                         stored[i + 5], 0};
            u32 cp = std::strtoul(h, nullptr, 16);
            if (cp < 0x80) {
                unescaped.push_back(static_cast<char>(cp));
            } else if (cp < 0x800) {
                unescaped.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                unescaped.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
                unescaped.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                unescaped.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                unescaped.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            i += 5;
        } else {
            unescaped.push_back(c);
        }
    }

    // Strip whitespace/newlines, then split off the "!END!" terminator.
    std::string compact;
    compact.reserve(unescaped.size());
    for (char c : unescaped) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v')
            compact.push_back(c);
    }
    size_t endPos = compact.find("!END!");
    if (endPos != std::string::npos) compact.resize(endPos);

    return bytesToUtf8(base64Decode(compact));
}

// ---------------------------------------------------------------------------
// Legacy packing helpers (ported from main.js). pack3 is the identity.
//
// unpack2(packed): reverse of pack2. Each char holds 14 bits (char code =
// 0b1 + 14 bits). Returns the concatenated bitstring.
// ---------------------------------------------------------------------------
std::string SaveLoad::unpack2(const std::string& packed) {
    std::string values;
    values.reserve(packed.size() * 14);
    for (u32 i = 0; i < packed.size(); ++i) {
        // charCodeAt(i).toString(2).substring(1) -> 14 bits (strip leading '1')
        unsigned int code = static_cast<unsigned char>(packed[i]);
        std::string bits;
        // produce exactly 14 bits
        for (int b = 13; b >= 0; --b) bits.push_back((code >> b) & 1 ? '1' : '0');
        values += bits;
    }
    return values;
}

// UncompressBin(num): binary of num, reversed, strip leading & trailing char.
// Returns real 0/1 integer values (JS uses parseInt on the resulting strings,
// so '0' must be the integer 0, not ASCII 48).
static std::vector<i32> uncompressBin(i32 num) {
    std::string bin;
    if (num == 0) bin = "0";
    else {
        unsigned int n = static_cast<unsigned int>(num);
        while (n) { bin.push_back((n & 1) ? '1' : '0'); n >>= 1; }
        std::reverse(bin.begin(), bin.end());
    }
    std::reverse(bin.begin(), bin.end());
    if (bin.size() > 2) bin = bin.substr(1, bin.size() - 2);
    else bin.clear();
    std::vector<i32> out;
    out.reserve(bin.size());
    for (char c : bin) out.push_back(c == '1' ? 1 : 0);
    return out;
}

std::vector<i32> SaveLoad::uncompressLargeBin(const std::string& arr) {
    // arr is a ';'-separated list of CompressBin numbers.
    std::vector<i32> bits;
    std::string cur;
    for (char c : arr) {
        if (c == ';') {
            if (!cur.empty()) {
                i32 num = static_cast<i32>(std::strtoul(cur.c_str(), nullptr, 10));
                auto b = uncompressBin(num);
                bits.insert(bits.end(), b.begin(), b.end());
                cur.clear();
            }
        } else cur.push_back(c);
    }
    if (!cur.empty()) {
        i32 num = static_cast<i32>(std::strtoul(cur.c_str(), nullptr, 10));
        auto b = uncompressBin(num);
        bits.insert(bits.end(), b.begin(), b.end());
    }
    return bits;
}

// ---------------------------------------------------------------------------
// Helpers for building the save string.
// ---------------------------------------------------------------------------
namespace {
std::string joinBits(const std::string& bits) { return bits; }

// JS Number.prototype.toString() (shortest round-trip, JS fixed/exponential
// rules). Used for every parseFloat(...).toString() field.
inline void numField(std::string& s, f64 v) { s += jsNumberToString(v); s += ';'; }

// parseInt(Math.floor(x)) -> integer string.
inline void floorIntField(std::string& s, f64 v) {
    s += std::to_string(static_cast<i64>(std::floor(v))); s += ';';
}

// parseInt(x) -> integer string (truncate toward zero; values are non-negative).
inline void intField(std::string& s, i64 v) { s += std::to_string(v); s += ';'; }

// Raw string field (no trailing ';' - caller adds it or the section terminator).
inline void strField(std::string& s, const std::string& v) { s += v; }
} // namespace

// ---------------------------------------------------------------------------
// Game.WriteSave (type 0 = store, 1 = string only, 2 = uncompressed, 3 =
// commented). Ported line-by-line from ref/main_2058.js.
// ---------------------------------------------------------------------------
std::string SaveLoad::writeSave(Game& game, i32 /*type*/) {
    std::string str;
    str.reserve(4096);

    // JS: Game.lastDate=parseInt(Game.time);  (injected-clock: tests set time==lastDate)
    game.lastDate = std::trunc(game.time);

    // Section 0: version
    str += jsNumberToString(game.VERSION);   // Game.version is a double
    str += '|';
    // Section 1: reserved (empty)
    str += '|';

    // --- section 2: run details ---
    // startDate;fullDate;lastDate;bakeryName;seed;appearance|
    floorIntField(str, game.startDate);
    floorIntField(str, game.fullDate);
    floorIntField(str, game.lastDate);
    str += game.bakeryName; str += ';';
    str += game.seed; str += ';';
    str += game.appearance;
    str += '|';

    // --- section 3: packed preferences bitfield (27 bits, pack3=identity) ---
    {
        std::string bits;
        bits.reserve(27);
        bits.push_back(game.prefs.particles ? '1' : '0');
        bits.push_back(game.prefs.numbers ? '1' : '0');
        bits.push_back(game.prefs.autosave ? '1' : '0');
        bits.push_back(game.prefs.autoupdate ? '1' : '0');
        bits.push_back(game.prefs.milk ? '1' : '0');
        bits.push_back(game.prefs.fancy ? '1' : '0');
        bits.push_back(game.prefs.warn ? '1' : '0');
        bits.push_back(game.prefs.cursors ? '1' : '0');
        bits.push_back(game.prefs.focus ? '1' : '0');
        bits.push_back(game.prefs.format ? '1' : '0');
        bits.push_back(game.prefs.notifs ? '1' : '0');
        bits.push_back(game.prefs.wobbly ? '1' : '0');
        bits.push_back(game.prefs.monospace ? '1' : '0');
        bits.push_back(game.prefs.filters ? '1' : '0');
        bits.push_back(game.prefs.cookiesound ? '1' : '0');
        bits.push_back(game.prefs.crates ? '1' : '0');
        bits.push_back(game.prefs.showBackupWarning ? '1' : '0');
        bits.push_back(game.prefs.extraButtons ? '1' : '0');
        bits.push_back(game.prefs.askLumps ? '1' : '0');
        bits.push_back(game.prefs.customGrandmas ? '1' : '0');
        bits.push_back(game.prefs.timeout ? '1' : '0');
        bits.push_back(game.prefs.cloudSave ? '1' : '0');
        bits.push_back(game.prefs.bgMusic ? '1' : '0');
        bits.push_back(game.prefs.notScary ? '1' : '0');
        bits.push_back(game.prefs.fullscreen ? '1' : '0');
        bits.push_back(game.prefs.screenreader ? '1' : '0');
        bits.push_back(game.prefs.discordPresence ? '1' : '0');
        str += bits; str += '|';
    }

    // --- section 4: misc game data (55 fields, 0..54) ---
    // .toString() fields (shortest round-trip):
    numField(str, game.cookies);              // 0
    numField(str, game.cookiesEarned);        // 1
    // parseInt(Math.floor(x)) fields:
    floorIntField(str, game.cookieClicks);    // 2
    floorIntField(str, game.goldenClicks);    // 3
    numField(str, game.handmadeCookies);      // 4
    floorIntField(str, game.missedGoldenClicks); // 5
    floorIntField(str, game.bgType);          // 6
    floorIntField(str, game.milkType);        // 7
    numField(str, game.cookiesReset);         // 8
    floorIntField(str, game.elderWrath);      // 9
    floorIntField(str, game.pledges);         // 10
    floorIntField(str, game.pledgeT);         // 11
    floorIntField(str, game.nextResearch);    // 12
    floorIntField(str, game.researchT);       // 13
    floorIntField(str, game.resets);          // 14
    floorIntField(str, game.goldenClicksLocal);// 15
    numField(str, game.cookiesSucked);        // 16
    floorIntField(str, game.wrinklersPopped); // 17
    floorIntField(str, game.santaLevel);      // 18
    floorIntField(str, game.reindeerClicked); // 19
    floorIntField(str, game.seasonT);         // 20
    floorIntField(str, game.seasonUses);      // 21
    str += game.season; str += ';';           // 22 (string)

    // Wrinklers: SaveWrinklers semantics.
    f64 wrinkleAmount = 0.0, wrinkleAmountShinies = 0.0;
    i32 wrinkleNumber = 0, wrinkleShinies = 0;
    for (auto& w : game.wrinklers) {
        if (w.sucked > 0.5) {
            ++wrinkleNumber;
            if (w.type == 1) { ++wrinkleShinies; wrinkleAmountShinies += w.sucked; }
            else wrinkleAmount += w.sucked;
        }
    }
    numField(str, std::floor(wrinkleAmount));      // 23  (parseFloat(Math.floor).toString)
    floorIntField(str, wrinkleNumber);             // 24
    numField(str, game.prestige);                  // 25
    numField(str, game.heavenlyChips);             // 26
    numField(str, game.heavenlyChipsSpent);        // 27
    numField(str, game.heavenlyCookies);           // 28
    floorIntField(str, game.ascensionMode);        // 29
    for (i32 i = 0; i < 5; ++i) floorIntField(str, game.permanentUpgrades[i]); // 30-34
    floorIntField(str, game.dragonLevel);          // 35
    floorIntField(str, game.dragonAura);           // 36
    floorIntField(str, game.dragonAura2);          // 37
    floorIntField(str, game.chimeType);            // 38
    floorIntField(str, game.volume);               // 39
    floorIntField(str, wrinkleShinies);            // 40
    numField(str, std::floor(wrinkleAmountShinies)); // 41
    numField(str, std::floor(game.lumps));         // 42
    numField(str, std::floor(game.lumpsTotal));    // 43
    numField(str, std::floor(game.lumpT));         // 44
    numField(str, std::floor(game.lumpRefill));    // 45
    floorIntField(str, game.lumpCurrentType);      // 46
    // 47: vault (comma-joined upgrade ids)
    {
        for (size_t i = 0; i < game.vault.size(); ++i) {
            if (i) str += ',';
            str += std::to_string(game.vault[i]);
        }
        str += ';';
    }
    intField(str, game.heralds);                   // 48  (parseInt, no floor)
    intField(str, game.fortuneGC);                 // 49
    intField(str, game.fortuneCPS);                // 50
    numField(str, game.cookiesPsRawHighest);       // 51  (NOT ascensions!)
    floorIntField(str, game.volumeMusic);          // 52
    floorIntField(str, game.cookiesSent);          // 53
    floorIntField(str, game.cookiesReceived);      // 54
    str += '|';// end section 4

    // --- section 5: buildings ---
    for (auto& b : game.objects) {
        if (!b || !b->vanilla) continue;
        str += std::to_string(b->amount); str += ',';
        str += std::to_string(b->bought); str += ',';
        str += jsNumberToString(std::floor(b->totalCookies)); str += ','; // comma-separated within entry
        str += std::to_string(b->level); str += ',';
        if (b->minigameLoaded && b->minigame) str += b->minigame->save();
        else str += b->minigameSave;                // raw stored minigame save
        str += ',';
        str += (b->muted ? '1' : '0'); str += ',';
        str += std::to_string(b->highest);
        str += ';';
    }
    str += '|';

    // --- section 6: packed upgrades bitfield (unlocked,bought pairs) ---
    {
        std::string bits;
        bits.reserve(game.upgradesById.size() * 2);
        for (auto* u : game.upgradesById) {
            if (!u) continue;
            if (!u->vanilla) continue;
            bits.push_back(u->unlocked ? '1' : '0');
            bits.push_back(u->bought ? '1' : '0');
        }
        str += bits; str += '|';
    }

    // --- section 7: packed achievements bitfield (won) ---
    {
        std::string bits;
        bits.reserve(game.achievementsById.size());
        for (auto* a : game.achievementsById) {
            if (!a) continue;
            if (!a->vanilla) continue;
            bits.push_back(a->won ? '1' : '0');
        }
        str += bits;
    }
    str += '|';

    // --- section 8: buffs ---
    f64 fps = static_cast<f64>(game.fps);
    for (auto& bf : game.buffs) {
        if (!bf || !bf->type) continue;
        str += std::to_string(bf->type->id); str += ',';
        numField(str, bf->maxTime * fps);
        numField(str, bf->time * fps);
        numField(str, bf->arg1);
        numField(str, bf->arg2);
        numField(str, bf->arg3);
        str += ';';
    }
    str += '|';

    // --- section 9: mod data ---
    // Game.saveModData() stub returns "" for now (mod system not built).
    str += ""; // Game.saveModData();

    return str;
}

// ---------------------------------------------------------------------------
// Platform-free parsing helpers.
// ---------------------------------------------------------------------------
namespace {
std::vector<std::string> splitPipe(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '|') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    if (!cur.empty() || !out.empty()) out.push_back(cur);
    return out;
}

std::vector<std::string> splitChar(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

inline std::vector<std::string> splitSemi(const std::string& s) { return splitChar(s, ';'); }

f64 getfDef(const std::vector<std::string>& v, i32 idx, f64 def) {
    if (idx < 0 || idx >= static_cast<i32>(v.size()) || v[idx].empty()) return def;
    return jsParseFloat(v[idx]);
}
i64 getiDef(const std::vector<std::string>& v, i32 idx, i64 def) {
    if (idx < 0 || idx >= static_cast<i32>(v.size()) || v[idx].empty()) return def;
    // JS: parseInt(str) — NaN if no digits; here treat as def when empty, else parse.
    const char* p = v[idx].c_str();
    while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); ++p; }
    long long val = 0; bool any = false;
    while (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); any = true; ++p; }
    if (!any) return def; // no digits -> treat as missing/default
    return static_cast<i64>(neg ? -val : val);
}
std::string getsDef(const std::vector<std::string>& v, i32 idx, const std::string& def) {
    if (idx < 0 || idx >= static_cast<i32>(v.size())) return def;
    return v[idx];
}
} // namespace

// ---------------------------------------------------------------------------
// Game.LoadSave parse half (platform-free): raw string -> SaveData.
// Mirrors every version-gated branch of ref/main_2058.js Game.LoadSave.
// Returns nullopt on truncated/garbage/wrong-version input.
// ---------------------------------------------------------------------------
std::optional<SaveData> SaveLoad::parseSave(const std::string& raw) {
    std::string str = raw;

    auto sections = splitPipe(str);
    if (sections.size() < 5) return std::nullopt;

    f64 version = jsParseFloat(sections[0]);
    // JS: if (oldstr[0].length<1) return false; (empty version)
    if (sections[0].empty()) return std::nullopt;
    if (std::isnan(version) || sections.size() < 5) return std::nullopt;

    SaveData data;
    data.version = version;

    // JS version check: future version rejected.
    if (version >= 1.0 && version > Game::VERSION) return std::nullopt;
    if (version < 1.0) return std::nullopt; // classic saves rejected

    // --- section 2: run stats ---
    auto spl = splitSemi(sections[2]);
    data.startDate = getfDef(spl, 0, NAN);
    data.fullDate = getfDef(spl, 1, NAN);
    data.lastDate = getfDef(spl, 2, NAN);
    data.bakeryName = getsDef(spl, 3, "");
    data.seed = getsDef(spl, 4, "");
    data.appearance = getsDef(spl, 5, "");

    // --- section 3: prefs (version-gated unpacking) ---
    std::string prefsBits;
    if (version < 1.0503) {
        prefsBits = sections[3]; // split('') below
    } else if (version < 2.0046) {
        prefsBits = unpack2(sections[3]);
    } else {
        prefsBits = sections[3];
    }
    // prefsBits is a string of '0'/'1' chars
    for (i32 i = 0; i < 27 && i < static_cast<i32>(prefsBits.size()); ++i) {
        data.prefs[i] = (prefsBits[i] == '1');
    }

    // --- section 4: misc data ---
    spl = splitSemi(sections[4]);
    data.cookies = getfDef(spl, 0, 0.0);
    data.cookiesEarned = getfDef(spl, 1, 0.0);
    data.cookieClicks = static_cast<i32>(getiDef(spl, 2, 0));
    data.goldenClicks = static_cast<i32>(getiDef(spl, 3, 0));
    data.handmadeCookies = getfDef(spl, 4, 0.0);
    data.missedGoldenClicks = static_cast<i32>(getiDef(spl, 5, 0));
    data.bgType = static_cast<i32>(getiDef(spl, 6, 0));
    data.milkType = static_cast<i32>(getiDef(spl, 7, 0));
    data.cookiesReset = getfDef(spl, 8, 0.0);
    data.elderWrath = static_cast<i32>(getiDef(spl, 9, 0));
    data.pledges = static_cast<i32>(getiDef(spl, 10, 0));
    data.pledgeT = getfDef(spl, 11, 0.0);
    data.nextResearch = static_cast<i32>(getiDef(spl, 12, 0));
    data.researchT = getfDef(spl, 13, 0.0);
    data.resets = static_cast<i32>(getiDef(spl, 14, 0));
    data.goldenClicksLocal = static_cast<i32>(getiDef(spl, 15, 0));
    data.cookiesSucked = getfDef(spl, 16, 0.0);
    data.wrinklersPopped = static_cast<i32>(getiDef(spl, 17, 0));
    data.santaLevel = static_cast<i32>(getiDef(spl, 18, 0));
    data.reindeerClicked = static_cast<i32>(getiDef(spl, 19, 0));
    data.seasonT = getfDef(spl, 20, 0.0);
    data.seasonUses = static_cast<i32>(getiDef(spl, 21, 0));
    data.season = getsDef(spl, 22, "");

    data.wrinkleAmount = getfDef(spl, 23, 0.0);
    data.wrinkleNumber = static_cast<i32>(getiDef(spl, 24, 0));
    data.prestige = getfDef(spl, 25, 0.0);
    data.heavenlyChips = getfDef(spl, 26, 0.0);
    data.heavenlyChipsSpent = getfDef(spl, 27, 0.0);
    data.heavenlyCookies = getfDef(spl, 28, 0.0);
    data.ascensionMode = static_cast<i32>(getiDef(spl, 29, 0));
    for (i32 i = 0; i < 5; ++i)
        data.permanentUpgrades[i] = static_cast<i32>(getiDef(spl, 30 + i, -1));
    data.dragonLevel = static_cast<i32>(getiDef(spl, 35, 0));
    data.dragonAura = static_cast<i32>(getiDef(spl, 36, 0));
    data.dragonAura2 = static_cast<i32>(getiDef(spl, 37, 0));
    data.chimeType = static_cast<i32>(getiDef(spl, 38, 0));
    data.volume = static_cast<i32>(getiDef(spl, 39, 0));
    data.wrinkleShinies = static_cast<i32>(getiDef(spl, 40, 0));
    data.wrinkleAmountShinies = getfDef(spl, 41, 0.0);
    data.lumps = getfDef(spl, 42, 0.0);
    data.lumpsTotal = getfDef(spl, 43, 0.0);
    data.lumpT = getfDef(spl, 44, 0.0);
    data.lumpRefill = getfDef(spl, 45, 0.0);
    data.lumpCurrentType = static_cast<i32>(getiDef(spl, 46, 0));
    // 47: vault (comma-joined)
    {
        std::string vaultStr = getsDef(spl, 47, "");
        if (!vaultStr.empty()) {
            std::string cur;
            for (char c : vaultStr) {
                if (c == ',') { if (!cur.empty()) { data.vault.push_back(static_cast<i32>(getiDef(std::vector<std::string>{cur}, 0, 0))); cur.clear(); }
                                else cur.clear(); }
                else cur.push_back(c);
            }
            if (!cur.empty()) data.vault.push_back(static_cast<i32>(getiDef(std::vector<std::string>{cur}, 0, 0)));
        }
    }
    data.heralds = static_cast<i32>(getiDef(spl, 48, 0));
    data.fortuneGC = static_cast<i32>(getiDef(spl, 49, 0));
    data.fortuneCPS = static_cast<i32>(getiDef(spl, 50, 0));
    data.cookiesPsRawHighest = getfDef(spl, 51, 0.0);
    data.volumeMusic = static_cast<i32>(getiDef(spl, 52, 0));
    data.cookiesSent = static_cast<i32>(getiDef(spl, 53, 0));
    data.cookiesReceived = static_cast<i32>(getiDef(spl, 54, 0));

    // --- section 5: buildings ---
    if (sections.size() > 5 && !sections[5].empty()) {
        auto bents = splitSemi(sections[5]);
        for (const auto& bstr : bents) {
            SaveData::BuildingSave bs;
            if (bstr.empty()) { data.buildings.push_back(bs); continue; }
            auto bf = splitChar(bstr, ',');
            bs.amount = static_cast<i32>(getiDef(bf, 0, 0));
            bs.bought = static_cast<i32>(getiDef(bf, 1, 0));
            bs.totalCookies = getfDef(bf, 2, 0.0);
            bs.level = static_cast<i32>(getiDef(bf, 3, 0));
            bs.minigameSave = getsDef(bf, 4, "");
            bs.muted = static_cast<i32>(getiDef(bf, 5, 0));
            bs.highest = static_cast<i32>(getiDef(bf, 6, 0));
            data.buildings.push_back(bs);
        }
    }

    // --- section 6: upgrades (version-gated) ---
    if (sections.size() > 6) {
        if (version < 1.035) {
            // old non-binary: entries "unlocked,bought" separated by ';'
            auto ents = splitSemi(sections[6]);
            for (const auto& e : ents) {
                if (e.empty()) continue;
                auto pr = splitChar(e, ',');
                data.upgradeBits.push_back(getiDef(pr, 0, 0) ? '1' : '0');
                data.upgradeBits.push_back(getiDef(pr, 1, 0) ? '1' : '0');
            }
        } else if (version < 1.0502) {
            // old awful packing
            std::string packed = sections[6];
            std::vector<i32> bits;
            if (version < 1.05) bits = uncompressLargeBin(packed);
            else {
                // unpack(str) -> bytes array; here packed is raw chars
                // JS: unpack returns [char>>>8, char&0xFF ...]
                for (char c : packed) {
                    unsigned int code = static_cast<unsigned char>(c);
                    bits.push_back(static_cast<i32>((code >> 8) & 0xFF));
                    bits.push_back(static_cast<i32>(code & 0xFF));
                }
            }
            for (size_t i = 0; i + 1 < bits.size(); i += 2) {
                data.upgradeBits.push_back(bits[i] ? '1' : '0');
                data.upgradeBits.push_back(bits[i + 1] ? '1' : '0');
            }
        } else {
            std::string packed;
            if (version < 2.0046) packed = unpack2(sections[6]);
            else packed = sections[6];
            data.upgradeBits = packed; // raw '0'/'1' string
        }
    }

    // --- section 7: achievements (version-gated) ---
    if (sections.size() > 7) {
        if (version < 1.035) {
            auto ents = splitSemi(sections[7]);
            for (const auto& e : ents) {
                if (e.empty()) { data.achievementBits.push_back('0'); continue; }
                auto pr = splitChar(e, ',');
                data.achievementBits.push_back(getiDef(pr, 0, 0) ? '1' : '0');
            }
        } else if (version < 1.0502) {
            std::string packed = sections[7];
            std::vector<i32> bits;
            if (version < 1.05) bits = uncompressLargeBin(packed);
            else {
                for (char c : packed) {
                    unsigned int code = static_cast<unsigned char>(c);
                    bits.push_back(static_cast<i32>((code >> 8) & 0xFF));
                    bits.push_back(static_cast<i32>(code & 0xFF));
                }
            }
            for (i32 b : bits) data.achievementBits.push_back(b ? '1' : '0');
        } else {
            std::string packed;
            if (version < 2.0046) packed = unpack2(sections[7]);
            else packed = sections[7];
            data.achievementBits = packed;
        }
    }

    // --- section 8: buffs ---
    if (sections.size() > 8 && !sections[8].empty()) {
        auto bents = splitSemi(sections[8]);
        for (const auto& bstr : bents) {
            if (bstr.empty()) continue;
            auto bf = splitChar(bstr, ',');
            if (bf.size() < 3) continue;
            SaveData::BuffSave bs;
            bs.typeId = static_cast<i32>(getiDef(bf, 0, 0));
            bs.maxTime = getfDef(bf, 1, 0.0);
            bs.time = getfDef(bf, 2, 0.0);
            bs.arg1 = getfDef(bf, 3, 0.0);
            bs.arg2 = getfDef(bf, 4, 0.0);
            bs.arg3 = getfDef(bf, 5, 0.0);
            data.buffs.push_back(bs);
        }
    }

    // --- section 9: mod data ---
    if (sections.size() > 9 && !sections[9].empty()) {
        auto ments = splitSemi(sections[9]);
        for (const auto& m : ments) {
            if (m.empty()) continue;
            auto kv = splitChar(m, ':');
            // format: modId:value (but value may contain ':'? JS splits on ':' then
            // shifts modId off, rejoins with ':')
            if (kv.empty()) continue;
            std::string modId = kv[0];
            if (modId == "META") continue;
            std::string value;
            if (kv.size() > 1) {
                // rejoin kv[1..] with ':'
                for (size_t i = 1; i < kv.size(); ++i) {
                    if (i > 1) value += ':';
                    value += kv[i];
                }
            }
            data.modData.emplace_back(modId, value);
        }
    }

    return data;
}

// ---------------------------------------------------------------------------
// Game.LoadSave apply half (side effects). Parsing already done in parseSave.
// Subsystems not yet built are stubbed; raw values are still stored so nothing
// is dropped and everything re-emits unchanged.
// ---------------------------------------------------------------------------
bool SaveLoad::applySave(Game& game, const SaveData& data) {
    // --- section 2: run details ---
    // JS: Game.startDate=parseInt(spl[0])||parseInt(Date.now());
    //     NaN/undefined -> now. We mirror: if NaN, use now_ms().
    auto nowOrDef = [](f64 v) { return std::isnan(v) ? static_cast<f64>(now_ms()) : v; };
    game.startDate = nowOrDef(data.startDate);
    game.fullDate = nowOrDef(data.fullDate);
    game.lastDate = nowOrDef(data.lastDate);
    game.bakeryName = data.bakeryName.empty() ? game.getBakeryName() : data.bakeryName;
    game.seed = data.seed.empty() ? "stub-seed" : data.seed;
    game.appearance = data.appearance;

    // --- section 3: prefs ---
    game.prefs.particles = data.prefs[0];
    game.prefs.numbers = data.prefs[1];
    game.prefs.autosave = data.prefs[2];
    game.prefs.autoupdate = data.prefs[3];
    game.prefs.milk = data.prefs[4];
    game.prefs.fancy = data.prefs[5];
    game.prefs.warn = data.prefs[6];
    game.prefs.cursors = data.prefs[7];
    game.prefs.focus = data.prefs[8];
    game.prefs.format = data.prefs[9];
    game.prefs.notifs = data.prefs[10];
    game.prefs.wobbly = data.prefs[11];
    game.prefs.monospace = data.prefs[12];
    game.prefs.filters = data.prefs[13];
    game.prefs.cookiesound = data.prefs[14];
    game.prefs.crates = data.prefs[15];
    game.prefs.showBackupWarning = data.prefs[16];
    game.prefs.extraButtons = data.prefs[17];
    game.prefs.askLumps = data.prefs[18];
    game.prefs.customGrandmas = data.prefs[19];
    game.prefs.timeout = data.prefs[20];
    game.prefs.cloudSave = data.prefs[21];
    game.prefs.bgMusic = data.prefs[22];
    game.prefs.notScary = data.prefs[23];
    game.prefs.fullscreen = data.prefs[24];
    game.prefs.screenreader = data.prefs[25];
    game.prefs.discordPresence = data.prefs[26];

    // --- section 4: misc data ---
    game.cookies = data.cookies;
    game.cookiesEarned = data.cookiesEarned;
    game.cookieClicks = data.cookieClicks;
    game.goldenClicks = data.goldenClicks;
    game.handmadeCookies = data.handmadeCookies;
    game.missedGoldenClicks = data.missedGoldenClicks;
    game.bgType = data.bgType;
    game.milkType = data.milkType;
    game.cookiesReset = data.cookiesReset;
    game.elderWrath = data.elderWrath;
    game.pledges = data.pledges;
    game.pledgeT = data.pledgeT;
    game.nextResearch = data.nextResearch;
    game.researchT = data.researchT;
    game.resets = data.resets;
    game.goldenClicksLocal = data.goldenClicksLocal;
    game.cookiesSucked = data.cookiesSucked;
    game.wrinklersPopped = data.wrinklersPopped;
    game.santaLevel = data.santaLevel;
    game.reindeerClicked = data.reindeerClicked;
    game.seasonT = data.seasonT;
    game.seasonUses = data.seasonUses;
    game.season = data.season.empty() ? game.baseSeason : data.season;

    // wrinklers
    game.wrinklers.clear();
    game.wrinklers.resize(12);
    for (auto& w : game.wrinklers) { w.phase = 0; w.close = 0; w.sucked = 0.0; w.hp = 2.1; w.type = 0; }

    game.prestige = data.prestige;          // loaded value (overwritten below)
    game.heavenlyChips = data.heavenlyChips;
    game.heavenlyChipsSpent = data.heavenlyChipsSpent;
    game.heavenlyCookies = data.heavenlyCookies;
    game.ascensionMode = data.ascensionMode;
    for (i32 i = 0; i < 5; ++i) game.permanentUpgrades[i] = data.permanentUpgrades[i];
    game.dragonLevel = data.dragonLevel;
    game.dragonAura = data.dragonAura;
    game.dragonAura2 = data.dragonAura2;
    game.chimeType = data.chimeType;
    game.volume = data.volume;

    // wrinkler shinies/amountShinies stored for LoadWrinklers
    // (applied below via stub)

    game.lumps = data.lumps;
    game.lumpsTotal = data.lumpsTotal;
    game.lumpT = data.lumpT;
    game.lumpRefill = data.lumpRefill;
    game.lumpCurrentType = data.lumpCurrentType;
    game.vault = data.vault;
    game.heralds = data.heralds;
    game.fortuneGC = data.fortuneGC;
    game.fortuneCPS = data.fortuneCPS;
    game.cookiesPsRawHighest = data.cookiesPsRawHighest;
    game.volumeMusic = data.volumeMusic;
    game.cookiesSent = data.cookiesSent;
    game.cookiesReceived = data.cookiesReceived;

    // --- recompute prestige (JS always does this on load) ---
    // Game.prestige = Math.floor(Game.HowMuchPrestige(Game.cookiesReset));
    // HowMuchPrestige = pow(cookiesReset/1e12, 1/3) = cbrt(cookiesReset/1e12)
    {
        f64 p = std::cbrt(game.cookiesReset / 1000000000000.0);
        if (p < 0.0) p = 0.0;
        game.prestige = static_cast<f64>(static_cast<i32>(std::floor(p)));
    }

    // --- version-gated migrations ---
    if (data.version <= 1.0466) {
        game.heavenlyChips = game.prestige;
        game.heavenlyChipsSpent = 0;
    }
    if (data.version == 1.9) {
        // remove all heavenly upgrades, refund chips (stub: no upgrade data here)
        game.heavenlyChips = game.prestige;
        game.heavenlyChipsSpent = 0;
    }

    // --- season/milk clamping ---
    if (game.bgType == -1) game.bgType = 0;
    if (game.milkType == -1) game.milkType = 0;

    // --- wrinklers (LoadWrinklers stub: assign shinies first, then normals,
    //     split sucked evenly) ---
    {
        i32 number = data.wrinkleNumber;
        i32 shinies = data.wrinkleShinies;
        f64 amount = data.wrinkleAmount;
        f64 amountShinies = data.wrinkleAmountShinies;
        if (number > 0 && (amount > 0.0 || amountShinies > 0.0)) {
            i32 fullNumber = number - shinies;
            i32 fullShinies = shinies;
            i32 remaining = number;
            i32 shiniesLeft = shinies;
            for (auto& w : game.wrinklers) {
                if (remaining <= 0) break;
                w.phase = 2; w.close = 1; w.hp = 3.0;
                if (shiniesLeft > 0) {
                    w.type = 1;
                    w.sucked = fullShinies > 0 ? amountShinies / static_cast<f64>(fullShinies) : 0.0;
                    --shiniesLeft;
                } else {
                    w.type = 0;
                    w.sucked = fullNumber > 0 ? amount / static_cast<f64>(fullNumber) : 0.0;
                }
                --remaining;
            }
        }
    }

    // --- section 5: buildings ---
    {
        game.buildingsOwned = 0;
        i32 bi = 0;
        for (auto& b : game.objects) {
            if (!b || !b->vanilla) continue;
            if (bi >= static_cast<i32>(data.buildings.size())) break;
            const auto& bs = data.buildings[bi];
            ++bi;
            b->amount = bs.amount;
            b->bought = bs.bought;
            b->totalCookies = bs.totalCookies;
            b->level = bs.level;
            b->minigameSave = bs.minigameSave;
            b->muted = bs.muted != 0;
            b->highest = bs.highest;
            game.buildingsOwned += b->amount;
        }
        // any buildings not in the save are left at defaults (0)
    }

    // --- section 6/7: upgrades & achievements bits ---
    // Apply bits to game.upgradesById / game.achievementsById (vanilla only,
    // matching WriteSave which only writes vanilla).
    {
        i32 bi = 0;
        for (auto* u : game.upgradesById) {
            if (!u || !u->vanilla) continue;
            if (bi + 1 < static_cast<i32>(data.upgradeBits.size())) {
                u->unlocked = data.upgradeBits[bi] == '1';
                u->bought = data.upgradeBits[bi + 1] == '1';
                bi += 2;
            } else {
                u->unlocked = false; u->bought = false;
            }
        }
    }
    {
        i32 bi = 0;
        for (auto* a : game.achievementsById) {
            if (!a || !a->vanilla) continue;
            if (bi < static_cast<i32>(data.achievementBits.size())) {
                a->won = data.achievementBits[bi] == '1';
                ++bi;
            } else {
                a->won = false;
            }
        }
    }

    // --- section 8: buffs (stub: store into game.buffs) ---
    game.buffs.clear();
    f64 fps = static_cast<f64>(game.fps);
    for (const auto& bs : data.buffs) {
        // resolve type id -> BuffType
        auto it = game.buffTypes.find(bs.typeId);
        std::shared_ptr<BuffType> type;
        if (it != game.buffTypes.end()) type = it->second;
        if (!type) {
            type = std::make_shared<BuffType>();
            type->id = bs.typeId;
            type->name = "buff-" + std::to_string(bs.typeId);
            type->vanilla = true;
            game.buffTypes[bs.typeId] = type;
        }
        auto nb = std::make_unique<Buff>();
        nb->type = type;
        nb->maxTime = bs.maxTime / fps; // frames -> seconds
        nb->time = bs.time / fps;
        nb->arg1 = bs.arg1;
        nb->arg2 = bs.arg2;
        nb->arg3 = bs.arg3;
        game.buffs.push_back(std::move(nb));
    }

    // --- section 9: mod data ---
    game.modSaveData.clear();
    for (const auto& kv : data.modData) game.modSaveData[kv.first] = kv.second;

    // --- post-parse side effects (stubs; no tick between load & write in tests) ---
    // Game.T=0; Game.realT=0; (no frame timers in headless port)
    // Game.killBuffs(); Game.CalculateGains(); — stubbed, do not alter saved fields
    // Offline catch-up: timeOffline = (clock - lastDate)/1000. In tests clock==lastDate -> 0.
    // Game.loadLumps(0); — stubbed no-op.

    game.recalculateGainsFlag();
    return true;
}

// ---------------------------------------------------------------------------
// Game.LoadSave = parse + apply.
// ---------------------------------------------------------------------------
bool SaveLoad::loadSave(Game& game, const std::string& raw) {
    auto data = parseSave(raw);
    if (!data) return false;
    return applySave(game, *data);
}
