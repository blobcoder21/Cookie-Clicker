#include "SaveLoad.h"
#include "Game.h"
#include "Building.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdio>
#include <cstdlib>

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

        if (!have2) enc3 = enc4 = 64;   // only 1 byte remained
        else if (!have3) enc4 = 64;     // 2 bytes remained

        out.push_back(alphabet[enc1]);
        out.push_back(alphabet[enc2]);
        out.push_back(alphabet[enc3]);
        out.push_back(alphabet[enc4]);
    }
    return out;
}

std::string SaveLoad::base64Decode(const std::string& in) {
    std::string s = in;
    // strip characters outside the alphabet (JS does this in b64_to_utf8)
    std::string filtered;
    filtered.reserve(s.size());
    for (char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=') {
            filtered.push_back(c);
        }
    }

    std::string out;
    auto val = [](char c) -> i32 {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        if (c == '=') return 64; // padding marker, matches the encoder's "missing" value
        return -1;
    };

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

std::string SaveLoad::encodeForStorage(const std::string& raw) {
    std::string b64 = base64Encode(utf8ToBytes(raw)) + "!END!";
    // JS escape(): alphanumerics + @*-_+. unchanged; space -> %20; everything
    // else -> %xx (ASCII) or %uXXXX (non-ASCII). We implement the common subset.
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
            // store as UTF-8 (simplified BMP only)
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

    // split off "!END!" terminator and everything after it (JS: str.split('!END!')[0])
    size_t endPos = unescaped.find("!END!");
    if (endPos != std::string::npos) unescaped.resize(endPos);

    return bytesToUtf8(base64Decode(unescaped));
}

// ---------------------------------------------------------------------------
// Helpers for building the save string
// ---------------------------------------------------------------------------
namespace {
std::string joinBits(const std::vector<char>& bits) {
    std::string s;
    s.reserve(bits.size());
    for (char b : bits) s.push_back(b ? '1' : '0');
    return s;
}

std::string f64Str(f64 v) {
    std::ostringstream os;
    os << v;
    return os.str();
}
} // namespace

std::string SaveLoad::writeSave(Game& game, i32 /*type*/) {
    std::ostringstream s;
    auto field = [&](const std::string& v) { s << v << ';'; };
    // Print doubles with enough significant digits to round-trip any
    // IEEE-754 double exactly (17 digits). JS Number.toString() produces the
    // shortest such string; parseFloat/stod both accept exponent notation, so
    // values written here parse identically on both sides.
    auto num = [&](f64 v) {
        std::ostringstream os;
        os << std::setprecision(17) << v;
        s << os.str();
    };
    auto numField = [&](f64 v) { num(v); s << ';'; };
    auto intField = [&](i64 v) { s << v << ';'; };

    s << game.VERSION << '|';
    s << '|'; // reserved

    // --- run stats (section 2) ---
    // JS writes: start;full;last;name;seed|  (seed is NOT ';'-terminated;
    // the '|' is the section terminator).
    numField(game.startDate);
    numField(game.fullDate);
    numField(game.lastDate);
    field(game.bakeryName);
    s << game.seed << '|';

    // --- prefs bitstring (section 3) ---
    {
        std::vector<char> bits = {
            static_cast<char>(game.prefs.particles ? 1 : 0),
            static_cast<char>(game.prefs.numbers ? 1 : 0),
            static_cast<char>(game.prefs.autosave ? 1 : 0),
            static_cast<char>(game.prefs.autoupdate ? 1 : 0),
            static_cast<char>(game.prefs.milk ? 1 : 0),
            static_cast<char>(game.prefs.fancy ? 1 : 0),
            static_cast<char>(game.prefs.warn ? 1 : 0),
            static_cast<char>(game.prefs.cursors ? 1 : 0),
            static_cast<char>(game.prefs.focus ? 1 : 0),
            static_cast<char>(game.prefs.format ? 1 : 0),
            static_cast<char>(game.prefs.notifs ? 1 : 0),
            static_cast<char>(game.prefs.wobbly ? 1 : 0),
            static_cast<char>(game.prefs.monospace ? 1 : 0),
            static_cast<char>(game.prefs.filters ? 1 : 0),
            static_cast<char>(game.prefs.cookiesound ? 1 : 0),
            static_cast<char>(game.prefs.crates ? 1 : 0),
            static_cast<char>(game.prefs.showBackupWarning ? 1 : 0),
            static_cast<char>(game.prefs.extraButtons ? 1 : 0),
            static_cast<char>(game.prefs.askLumps ? 1 : 0),
            static_cast<char>(game.prefs.customGrandmas ? 1 : 0),
            static_cast<char>(game.prefs.timeout ? 1 : 0),
        };
        s << joinBits(bits) << '|';
    }

    // --- misc game data (section 4) ---
    // Field order mirrors JS Game.WriteSave exactly so browser-made saves
    // load into this build (and vice versa).
    numField(game.cookies);              // 0
    numField(game.cookiesEarned);        // 1
    intField(game.cookieClicks);         // 2
    intField(game.goldenClicks);         // 3
    numField(game.handmadeCookies);      // 4
    intField(game.missedGoldenClicks);   // 5
    intField(game.bgType);               // 6
    intField(game.milkType);             // 7
    numField(game.cookiesReset);         // 8
    intField(game.elderWrath);           // 9
    intField(game.pledges);              // 10
    numField(game.pledgeT);              // 11
    intField(game.nextResearch);         // 12
    numField(game.researchT);            // 13
    intField(game.resets);               // 14
    intField(game.goldenClicksLocal);    // 15
    numField(game.cookiesSucked);        // 16
    intField(game.wrinklersPopped);      // 17
    intField(game.santaLevel);           // 18
    intField(game.reindeerClicked);      // 19
    numField(game.seasonT);              // 20
    intField(game.seasonUses);           // 21
    field(game.season);                  // 22

    // wrinklers: amount/number (23,24) and shinies/amountShinies (40,41).
    // JS SaveWrinklers: count wrinklers with sucked > 0.5.
    f64 wrinkleAmount = 0.0, wrinkleAmountShinies = 0.0;
    i32 wrinkleNumber = 0, wrinkleShinies = 0;
    for (auto& w : game.wrinklers) {
        if (w.sucked > 0.5) {
            ++wrinkleNumber;
            if (w.type == 1) { ++wrinkleShinies; wrinkleAmountShinies += w.sucked; }
            else wrinkleAmount += w.sucked;
        }
    }
    numField(wrinkleAmount);   // 23
    intField(wrinkleNumber);   // 24
    numField(game.prestige);         // 25
    numField(game.heavenlyChips);    // 26
    numField(game.heavenlyChipsSpent); // 27
    numField(game.heavenlyCookies);  // 28
    intField(0);                     // 29 ascensionMode placeholder
    for (i32 i = 0; i < 5; ++i) intField(game.permanentUpgrades[i]); // 30-34
    intField(game.dragonLevel);      // 35
    intField(game.dragonAura);       // 36
    intField(game.dragonAura2);      // 37
    intField(game.chimeType);        // 38
    intField(game.volume);           // 39
    intField(wrinkleShinies);        // 40
    numField(wrinkleAmountShinies);  // 41
    numField(game.lumps);            // 42
    numField(game.lumpsTotal);       // 43
    numField(game.lumpT);            // 44
    numField(game.lumpRefill);       // 45
    intField(game.lumpCurrentType);  // 46
    // vault: comma-joined list of upgrade ids (47)
    {
        for (size_t i = 0; i < game.vault.size(); ++i) {
            if (i) s << ',';
            s << game.vault[i];
        }
        s << ';';
    }
    intField(game.heralds);          // 48
    intField(game.fortuneGC);        // 49
    intField(game.fortuneCPS);       // 50

    s << '|';

    // --- buildings (section 5) ---
    // amount,bought,totalCookies,level,minigameSave,muted;  (minigame save
    // left empty - minigames are stubbed in this skeleton)
    for (auto& b : game.objects) {
        if (!b->vanilla) continue;
        s << b->amount << ',';
        s << b->bought << ',';
        s << std::floor(b->totalCookies) << ',';
        s << b->level << ',';
        if (b->minigameLoaded && b->minigame) s << b->minigame->save();
        s << ','; // minigame save (between level and muted)
        s << (b->muted ? '1' : '0');
        s << ';';
    }
    s << '|';

    // --- upgrades bitstring (section 6) ---
    {
        std::vector<char> bits;
        bits.reserve(game.upgradesById.size());
        for (auto* u : game.upgradesById) {
            if (!u) continue;
            bits.push_back(u->unlocked ? 1 : 0);
            bits.push_back(u->bought ? 1 : 0);
        }
        s << joinBits(bits) << '|';
    }

    // --- achievements bitstring (section 7) ---
    {
        std::vector<char> bits;
        bits.reserve(game.achievementsById.size());
        for (auto* a : game.achievementsById) {
            if (!a) continue;
            bits.push_back(a->won ? 1 : 0);
        }
        s << joinBits(bits);
    }

    s << '|';

    // --- buffs (section 8) ---
    // type.id,maxTime,time,arg1,arg2,arg3;  (maxTime/time stored in frames:
    // internal seconds * fps, matching JS which saves frame counts)
    f64 fps = static_cast<f64>(game.fps);
    for (auto& bf : game.buffs) {
        if (!bf || !bf->type) continue;
        s << bf->type->id << ',';
        num(bf->maxTime * fps); s << ',';
        num(bf->time * fps); s << ',';
        num(bf->arg1); s << ',';
        num(bf->arg2); s << ',';
        num(bf->arg3);
        s << ';';
    }
    s << '\n';

    return s.str();
}

bool SaveLoad::loadSave(Game& game, const std::string& raw) {
    std::string str = raw;
    // JS: str = str.split('!END!')[0]  (already done in decodeFromStorage)

    // split into sections on '|'
    std::vector<std::string> sections;
    {
        std::string cur;
        for (char c : str) {
            if (c == '|') { sections.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        if (!cur.empty() || !sections.empty()) sections.push_back(cur);
    }

    if (sections.size() < 5) return false;

    f64 version = 0.0;
    try { version = std::stod(sections[0]); } catch (...) { return false; }
    if (version < 1.0 || version > Game::VERSION) return false;

    // --- section 2: run stats ---
    auto splitSemi = [](const std::string& sec) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : sec) {
            if (c == ';') { out.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        out.push_back(cur);
        return out;
    };

    auto spl = splitSemi(sections[2]);
    auto getf = [&](const std::vector<std::string>& v, i32 idx, f64 def = 0.0) -> f64 {
        if (idx < 0 || idx >= static_cast<i32>(v.size()) || v[idx].empty()) return def;
        try { return std::stod(v[idx]); } catch (...) { return def; }
    };
    auto geti = [&](const std::vector<std::string>& v, i32 idx, i64 def = 0) -> i64 {
        return static_cast<i64>(getf(v, idx, static_cast<f64>(def)));
    };
    auto gets = [&](const std::vector<std::string>& v, i32 idx,
                    const std::string& def = "") -> std::string {
        if (idx < 0 || idx >= static_cast<i32>(v.size())) return def;
        return v[idx];
    };

    game.startDate = getf(spl, 0);
    game.fullDate = getf(spl, 1);
    game.lastDate = getf(spl, 2);
    game.bakeryName = gets(spl, 3, game.getBakeryName());
    game.seed = gets(spl, 4);

    // --- section 3: prefs ---
    const std::string& prefsBits = sections[3];
    auto pbit = [&](i32 idx, bool def = false) -> bool {
        if (idx < 0 || idx >= static_cast<i32>(prefsBits.size())) return def;
        return prefsBits[idx] == '1';
    };
    game.prefs.particles = pbit(0, true);
    game.prefs.numbers = pbit(1, true);
    game.prefs.autosave = pbit(2, true);
    game.prefs.autoupdate = pbit(3, true);
    game.prefs.milk = pbit(4, true);
    game.prefs.fancy = pbit(5, true);
    game.prefs.warn = pbit(6, false);
    game.prefs.cursors = pbit(7, false);
    game.prefs.focus = pbit(8, false);
    game.prefs.format = pbit(9, false);
    game.prefs.notifs = pbit(10, true);
    game.prefs.wobbly = pbit(11, false);
    game.prefs.monospace = pbit(12, false);
    game.prefs.filters = pbit(13, true);
    game.prefs.cookiesound = pbit(14, true);
    game.prefs.crates = pbit(15, false);
    game.prefs.showBackupWarning = pbit(16, true);
    game.prefs.extraButtons = pbit(17, true);
    game.prefs.askLumps = pbit(18, false);
    game.prefs.customGrandmas = pbit(19, true);
    game.prefs.timeout = pbit(20, false);

    // --- section 4: misc data ---
    // Field order mirrors JS Game.WriteSave exactly.
    spl = splitSemi(sections[4]);
    game.cookies = getf(spl, 0);
    game.cookiesEarned = getf(spl, 1);
    game.cookieClicks = static_cast<i32>(geti(spl, 2));
    game.goldenClicks = static_cast<i32>(geti(spl, 3));
    game.handmadeCookies = getf(spl, 4);
    game.missedGoldenClicks = static_cast<i32>(geti(spl, 5));
    game.bgType = static_cast<i32>(geti(spl, 6));
    game.milkType = static_cast<i32>(geti(spl, 7));
    game.cookiesReset = getf(spl, 8);
    game.elderWrath = getf(spl, 9);
    game.pledges = static_cast<i32>(geti(spl, 10));
    game.pledgeT = getf(spl, 11);
    game.nextResearch = static_cast<i32>(geti(spl, 12));
    game.researchT = getf(spl, 13);
    game.resets = static_cast<i32>(geti(spl, 14));
    game.goldenClicksLocal = static_cast<i32>(geti(spl, 15));
    game.cookiesSucked = getf(spl, 16);
    game.wrinklersPopped = static_cast<i32>(geti(spl, 17));
    game.santaLevel = static_cast<i32>(geti(spl, 18));
    game.reindeerClicked = static_cast<i32>(geti(spl, 19));
    game.seasonT = getf(spl, 20);
    game.seasonUses = static_cast<i32>(geti(spl, 21));
    game.season = gets(spl, 22, game.baseSeason);
    // wrinklers: amount (23), number (24); shinies (40), amountShinies (41)
    f64 wrinkleAmount = getf(spl, 23);
    i32 wrinkleNumber = static_cast<i32>(geti(spl, 24));
    game.prestige = getf(spl, 25);
    game.heavenlyChips = getf(spl, 26);
    game.heavenlyChipsSpent = getf(spl, 27);
    game.heavenlyCookies = getf(spl, 28);
    // ascensionMode 29 (placeholder - not modelled)
    (void)geti(spl, 29);
    for (i32 i = 0; i < 5; ++i) {
        game.permanentUpgrades[i] = static_cast<i32>(geti(spl, 30 + i));
    }
    game.dragonLevel = static_cast<i32>(geti(spl, 35));
    game.dragonAura = static_cast<i32>(geti(spl, 36));
    game.dragonAura2 = static_cast<i32>(geti(spl, 37));
    game.chimeType = static_cast<i32>(geti(spl, 38));
    game.volume = static_cast<i32>(geti(spl, 39));
    i32 wrinkleShinies = static_cast<i32>(geti(spl, 40));
    f64 wrinkleAmountShinies = getf(spl, 41);
    game.lumps = getf(spl, 42);
    game.lumpsTotal = getf(spl, 43);
    game.lumpT = getf(spl, 44);
    game.lumpRefill = getf(spl, 45);
    game.lumpCurrentType = static_cast<i32>(geti(spl, 46));
    // vault (47): comma-joined list of upgrade ids
    game.vault.clear();
    {
        std::string vaultStr = gets(spl, 47);
        if (!vaultStr.empty()) {
            std::string cur;
            for (char c : vaultStr) {
                if (c == ',') {
                    if (!cur.empty()) game.vault.push_back(static_cast<i32>(getf(std::vector<std::string>{cur}, 0)));
                    cur.clear();
                } else cur.push_back(c);
            }
            if (!cur.empty()) game.vault.push_back(static_cast<i32>(getf(std::vector<std::string>{cur}, 0)));
        }
    }
    game.heralds = static_cast<i32>(geti(spl, 48));
    game.fortuneGC = static_cast<i32>(geti(spl, 49));
    game.fortuneCPS = static_cast<i32>(geti(spl, 50));

    // Restore wrinkler state (JS LoadWrinklers semantics):
    // shinies are assigned to the first slots, then normals; sucked split evenly.
    if (wrinkleNumber > 0 && (wrinkleAmount > 0.0 || wrinkleAmountShinies > 0.0)) {
        i32 fullNumber = wrinkleNumber - wrinkleShinies;
        i32 fullNumberShinies = wrinkleShinies;
        i32 remaining = wrinkleNumber;
        i32 shiniesLeft = wrinkleShinies;
        for (auto& w : game.wrinklers) {
            if (remaining <= 0) break;
            w.phase = 2;
            w.close = 1;
            w.hp = 3.0;
            if (shiniesLeft > 0) {
                w.type = 1;
                w.sucked = fullNumberShinies > 0 ? wrinkleAmountShinies / static_cast<f64>(fullNumberShinies) : 0.0;
                --shiniesLeft;
            } else {
                w.type = 0;
                w.sucked = fullNumber > 0 ? wrinkleAmount / static_cast<f64>(fullNumber) : 0.0;
            }
            --remaining;
        }
    }

    // --- section 5: buildings ---
    // JS iterates Game.Objects in order and writes one entry per vanilla
    // building; entries are matched 1:1 to vanilla buildings in objects[].
    if (sections.size() > 5 && !sections[5].empty()) {
        std::vector<std::string> bents;
        {
            std::string cur;
            for (char c : sections[5]) {
                if (c == ';') { bents.push_back(cur); cur.clear(); }
                else cur.push_back(c);
            }
            bents.push_back(cur); // trailing entry
        }
        auto bgetf = [&](const std::vector<std::string>& v, i32 idx,
                         f64 def = 0.0) -> f64 {
            if (idx < 0 || idx >= static_cast<i32>(v.size()) || v[idx].empty())
                return def;
            try { return std::stod(v[idx]); } catch (...) { return def; }
        };
        i32 bi = 0;
        i32 buildingsOwned = 0;
        for (auto& b : game.objects) {
            if (!b || !b->vanilla) continue;
            if (bi >= static_cast<i32>(bents.size())) break;
            const std::string& bstr = bents[bi];
            ++bi;
            if (bstr.empty()) continue;
            std::vector<std::string> bf;
            {
                std::string cur;
                for (char c : bstr) {
                    if (c == ',') { bf.push_back(cur); cur.clear(); }
                    else cur.push_back(c);
                }
                bf.push_back(cur);
            }
            b->amount = static_cast<i32>(bgetf(bf, 0));
            b->bought = static_cast<i32>(bgetf(bf, 1));
            b->totalCookies = bgetf(bf, 2);
            b->level = static_cast<i32>(bgetf(bf, 3));
            // bf[4]: minigame save
            if (bf.size() > 4 && !bf[4].empty() && b->minigameLoaded && b->minigame) {
                b->minigame->load(bf[4]);
            }
            if (bf.size() > 5) b->muted = static_cast<i32>(bgetf(bf, 5, 0.0)) != 0;
            buildingsOwned += b->amount;
        }
        game.buildingsOwned = buildingsOwned;
        game.recalculateGainsFlag();
    }

    // --- section 6: upgrades bitstring ---
    if (sections.size() > 6) {
        const std::string& bits = sections[6];
        i32 bi = 0;
        for (auto* u : game.upgradesById) {
            if (!u) continue;
            if (bi + 1 < static_cast<i32>(bits.size())) {
                u->unlocked = bits[bi] == '1';
                u->bought = bits[bi + 1] == '1';
                bi += 2;
            }
        }
    }

    // --- section 7: achievements bitstring ---
    if (sections.size() > 7) {
        const std::string& bits = sections[7];
        i32 bi = 0;
        for (auto* a : game.achievementsById) {
            if (!a) continue;
            if (bi < static_cast<i32>(bits.size())) {
                a->won = bits[bi] == '1';
                ++bi;
            }
        }
    }

    // --- section 8: buffs ---
    // type.id,maxTime,time,arg1,arg2,arg3  (maxTime/time stored in frames)
    if (sections.size() > 8 && !sections[8].empty()) {
        f64 fps = static_cast<f64>(game.fps);
        std::string bsec = sections[8];
        size_t pos = 0;
        while ((pos = bsec.find(';')) != std::string::npos) {
            std::string bstr = bsec.substr(0, pos);
            bsec.erase(0, pos + 1);
            if (bstr.empty()) continue;
            std::vector<std::string> bf;
            {
                std::string cur;
                for (char c : bstr) {
                    if (c == ',') { bf.push_back(cur); cur.clear(); }
                    else cur.push_back(c);
                }
                bf.push_back(cur);
            }
            if (bf.size() < 3) continue;
            auto bgetf = [&](i32 idx, f64 def = 0.0) -> f64 {
                if (idx < 0 || idx >= static_cast<i32>(bf.size()) || bf[idx].empty())
                    return def;
                try { return std::stod(bf[idx]); } catch (...) { return def; }
            };
            i32 typeId = static_cast<i32>(bgetf(0));
            // JS stores frames; Buff keeps seconds internally.
            f64 maxTime = bgetf(1) / fps;
            f64 time = bgetf(2) / fps;
            f64 arg1 = bgetf(3);
            f64 arg2 = bgetf(4);
            f64 arg3 = bgetf(5);

            std::shared_ptr<BuffType> type;
            auto it = game.buffTypes.find(typeId);
            if (it != game.buffTypes.end()) type = it->second;
            if (!type) {
                // unknown id: keep a placeholder so saves still round-trip
                type = std::make_shared<BuffType>();
                type->id = typeId;
                type->name = "buff-" + std::to_string(typeId);
                type->vanilla = true;
                game.buffTypes[typeId] = type;
            }
            auto nb = std::make_unique<Buff>();
            nb->type = type;
            nb->maxTime = maxTime;
            nb->time = time;
            nb->arg1 = arg1;
            nb->arg2 = arg2;
            nb->arg3 = arg3;
            game.buffs.push_back(std::move(nb));
        }
    }

    game.recalculateGainsFlag();
    return true;
}
