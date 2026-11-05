#include "SaveLoad.h"
#include "Game.h"
#include "Building.h"
#include <sstream>
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
    auto numField = [&](f64 v) { s << v << ';'; };
    auto intField = [&](i64 v) { s << v << ';'; };

    s << game.VERSION << '|';
    s << '|'; // reserved

    // --- run stats (section 2) ---
    numField(game.startDate);
    numField(game.fullDate);
    numField(game.lastDate);
    field(game.bakeryName);
    field(game.seed);

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
    numField(game.cookies);
    numField(game.cookiesEarned);
    intField(game.cookieClicks);
    intField(game.goldenClicks);
    numField(game.handmadeCookies);
    intField(game.missedGoldenClicks);
    intField(game.bgType);
    intField(game.milkType);
    numField(game.cookiesReset);
    intField(game.elderWrath);
    intField(game.pledges);
    intField(game.pledgeT);
    intField(game.nextResearch);
    intField(game.researchT);
    intField(game.resets);
    intField(game.goldenClicksLocal);
    numField(game.cookiesSucked);

    // wrinklers (placeholder - not yet modelled in skeleton)
    numField(0.0); // amount
    intField(0);   // number
    numField(game.prestige);
    numField(game.heavenlyChips);
    numField(game.heavenlyChipsSpent);
    numField(game.heavenlyCookies);
    intField(game.ascendTimer > 0 ? 1 : 0); // ascensionMode placeholder

    for (i32 i = 0; i < 5; ++i) intField(game.permanentUpgrades[i]);

    intField(game.dragonLevel);
    intField(game.dragonAura);
    intField(game.dragonAura2);
    intField(game.chimeType);
    intField(game.volume);
    intField(0); // shinies
    numField(0.0); // shinies amount
    numField(game.lumps);
    numField(game.lumpsTotal);
    numField(game.lumpT);
    numField(game.lumpRefill);
    intField(game.lumpCurrentType);
    // vault: comma-joined list
    s << "0;"; // placeholder
    intField(game.heralds);
    intField(game.fortuneGC);
    intField(game.fortuneCPS);

    s << '|';

    // --- buildings (section 5) ---
    for (auto& b : game.objects) {
        if (!b->vanilla) continue;
        s << b->amount << ',';
        s << b->bought << ',';
        numField(std::floor(b->totalCookies));
        intField(b->level);
        // minigame save (placeholder)
        s << ',';
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
    for (auto& bf : game.buffs) {
        if (!bf->type) continue;
        s << bf->type->id << ',';
        numField(bf->maxTime);
        numField(bf->time);
        // args placeholders
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
    // wrinklers 17,18 (not modelled yet)
    game.prestige = getf(spl, 19);
    game.heavenlyChips = getf(spl, 20);
    game.heavenlyChipsSpent = getf(spl, 21);
    game.heavenlyCookies = getf(spl, 22);
    // ascensionMode 23
    for (i32 i = 0; i < 5; ++i) {
        game.permanentUpgrades[i] = static_cast<i32>(geti(spl, 24 + i));
    }
    i32 base = 29;
    game.dragonLevel = static_cast<i32>(geti(spl, base));
    game.dragonAura = static_cast<i32>(geti(spl, base + 1));
    game.dragonAura2 = static_cast<i32>(geti(spl, base + 2));
    game.chimeType = static_cast<i32>(geti(spl, base + 3));
    game.volume = static_cast<i32>(geti(spl, base + 4));
    // shinies base+5, shiniesAmount base+6
    game.lumps = getf(spl, base + 7);
    game.lumpsTotal = getf(spl, base + 8);
    game.lumpT = getf(spl, base + 9);
    game.lumpRefill = getf(spl, base + 10);
    game.lumpCurrentType = static_cast<i32>(geti(spl, base + 11));
    // vault base+12 (comma joined - placeholder)
    game.heralds = static_cast<i32>(geti(spl, base + 13));
    game.fortuneGC = static_cast<i32>(geti(spl, base + 14));
    game.fortuneCPS = static_cast<i32>(geti(spl, base + 15));

    // --- section 5: buildings ---
    if (sections.size() > 5 && !sections[5].empty()) {
        std::string bsec = sections[5];
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
            // find building by name is not possible here (name not stored);
            // skeleton skips building amounts. Full port must match building order.
        }
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
    if (sections.size() > 8) {
        // placeholder: buff parsing requires the Buff type registry
    }

    game.recalculateGainsFlag();
    return true;
}
