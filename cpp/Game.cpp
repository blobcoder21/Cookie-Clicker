#include "Game.h"
#include "SaveLoad.h"
#include "Building.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <unordered_map>

Game* g_game = nullptr;

// ---------------------------------------------------------------------------
// Helpers mirroring the JS building auto-curve (Game.Object constructor,
// id != 0 branch).
// ---------------------------------------------------------------------------
namespace {

f64 niceRound(f64 v) {
    if (v <= 0.0) return 0.0;
    f64 digits = std::pow(10.0,
        std::ceil(std::log(std::ceil(v)) / std::log(10.0)) / 100.0);
    return std::floor(v / digits + 0.5) * digits; // JS Math.round
}

std::pair<f64,f64> buildingCurve(i32 n) {
    // returns {basePrice, baseCps} for building index n (id != 0)
    f64 baseCps = std::ceil(std::pow(static_cast<f64>(n),
                                     static_cast<f64>(n) * 0.5 + 2.0) * 10.0) / 10.0;
    baseCps = niceRound(baseCps);

    f64 basePrice = (static_cast<f64>(n) + 9.0 +
                     (n < 5 ? 0.0 : std::pow(static_cast<f64>(n) - 5.0, 1.75) * 5.0)) *
                    std::pow(10.0, static_cast<f64>(n)) *
                    std::max(1.0, static_cast<f64>(n) - 14.0);
    basePrice = niceRound(basePrice);
    if (n >= 16) basePrice *= 10.0;
    return {basePrice, baseCps};
}

} // namespace

// ---------------------------------------------------------------------------
// Game singleton
// ---------------------------------------------------------------------------
Game::Game() {
    g_game = this;
}

Game::~Game() {
    g_game = nullptr;
}

void Game::launch() {
    time = static_cast<f64>(now_ms());
    lastDate = time;
    startDate = time;
    fullDate = time;
    bakeryName = getBakeryName();
    seed = std::to_string(static_cast<i64>(time)); // placeholder seed
    ready = true;
}

void Game::load() {
    // --- Create the 18 buildings (mirrors JS Game.Object definitions) ---
    auto addBuilding = [this](std::string name, std::string commonName,
                              std::string desc, i32 icon, i32 iconColumn,
                              Building::Art art, f64 price, f64 cps,
                              std::function<void(Building*)> buyFunc,
                              std::function<f64(Building*)> cpsFunc) {
        auto b = std::make_unique<Building>(std::move(name), std::move(commonName),
                                            std::move(desc), std::to_string(icon),
                                            iconColumn, std::move(art), price, cps,
                                            std::move(buyFunc));
        b->cpsFunc = std::move(cpsFunc);
        b->id = objectsN;
        objectsById.push_back(b.get());
        objects.push_back(std::move(b));
        objectsN++;
    };

    // Cursor (id=0): explicit price/cps, curve not applied
    addBuilding("Cursor", "cursor|cursors|clicked|[X] extra finger|[X] extra fingers",
        "Autoclicks once every 10 seconds.", 0, 0, {}, 15.0, 0.1,
        [](Building*) {
            // unlock click upgrades at milestones (handled by Game)
        },
        [](Building* self) -> f64 {
            Game* g = g_game;
            f64 add = 0.0;
            if (g->has("Thousand fingers")) add += 0.1;
            if (g->has("Million fingers")) add += 0.5;
            if (g->has("Billion fingers")) add += 5.0;
            if (g->has("Trillion fingers")) add += 50.0;
            if (g->has("Quadrillion fingers")) add += 500.0;
            if (g->has("Quintillion fingers")) add += 5000.0;
            if (g->has("Sextillion fingers")) add += 50000.0;
            if (g->has("Septillion fingers")) add += 500000.0;
            if (g->has("Octillion fingers")) add += 5000000.0;
            i32 num = 0;
            for (auto& b : g->objects) {
                if (b->name != "Cursor") num += b->amount;
            }
            add = add * static_cast<f64>(num);
            f64 mult = 1.0;
            mult *= g->GetTieredCpsMult(self);
            mult *= g->eff("cursorCps");
            i32 clickUpgrades = (g->has("Reinforced index finger") ? 1 : 0) +
                                (g->has("Carpal tunnel prevention cream") ? 1 : 0) +
                                (g->has("Ambidextrous") ? 1 : 0);
            return g->ComputeCps(0.1, clickUpgrades, add) * mult;
        });

    // Helper for standard buildings: cps = baseCps * tieredMult * magicCpS
    auto standardCps = [](Building* self) -> f64 {
        f64 mult = g_game->GetTieredCpsMult(self);
        mult *= g_game->magicCpS(self->name);
        return self->baseCps * mult;
    };
    auto standardBuy = [](Building*) {
    };

    // Grandma (id=1)
    addBuilding("Grandma", "grandma|grandmas|baked|Grandmas are [X] year older|Grandmas are [X] years older",
        "A nice grandma to bake more cookies.", 1, 1, {}, 100.0, 1.0, standardBuy,
        [](Building* self) -> f64 {
            Game* g = g_game;
            f64 mult = 1.0;
            if (g->has("Bingo center/Research facility")) mult *= 4.0;
            if (g->has("Ritual rolling pins")) mult *= 2.0;
            if (g->has("Naughty list")) mult *= 2.0;
            if (g->has("Elderwort biscuits")) mult *= 1.02;
            mult *= g->eff("grandmaCps");
            mult *= g->GetTieredCpsMult(self);
            f64 add = 0.0;
            if (g->has("One mind")) add += self->amount * 0.02;
            if (g->has("Communal brainsweep")) add += self->amount * 0.02;
            if (g->has("Elder Pact")) {
                if (auto* portal = g->getBuilding("Portal")) add += portal->amount * 0.05;
            }
            i32 numOthers = 0;
            for (auto& b : g->objects) if (b->name != "Grandma") numOthers += b->amount;
            mult *= 1.0 + g->auraMult("Elder Battalion") * 0.01 * static_cast<f64>(numOthers);
            mult *= g->magicCpS(self->name);
            return (self->baseCps + add) * mult;
        });

    // Create remaining buildings via the auto-curve
    struct BuildingDef {
        const char* name;
        const char* commonName;
        const char* desc;
        i32 icon;
        i32 iconColumn;
        const char* artBase;
        f64 artXV, artYV, artW;
        i32 artRows, artX, artY;
    };

    static const BuildingDef defs[] = {
        {"Farm", "farm|farms|harvested|[X] more acre|[X] more acres",
         "Grows cookie plants from cookie seeds.", 3, 2, "farm", 8, 8, 64, 2, 0, 16},
        {"Mine", "mine|mines|mined|[X] mile deeper|[X] miles deeper",
         "Mines out cookie dough and chocolate chips.", 4, 3, "mine", 16, 16, 64, 2, 0, 24},
        {"Factory", "factory|factories|mass-produced|[X] additional patent|[X] additional patents",
         "Produces large quantities of cookies.", 5, 4, "factory", 8, 0, 64, 1, 0, -22},
        {"Bank", "bank|banks|banked|Interest rates [X]% better|Interest rates [X]% better",
         "Generates cookies from interest.", 6, 5, "bank", 8, 4, 56, 1, 0, 13},
        {"Temple", "temple|temples|discovered|[X] sacred artifact retrieved|[X] sacred artifacts retrieved",
         "Full of precious, ancient chocolate.", 7, 6, "temple", 8, 4, 72, 2, 0, -5},
        {"Wizard tower", "wizard tower|wizard towers|summoned|Incantations have [X] more syllable|Incantations have [X] more syllables",
         "Summons cookies with magic spells.", 8, 7, "wizardtower", 16, 16, 48, 2, 0, 20},
        {"Shipment", "shipment|shipments|shipped|[X] galaxy fully explored|[X] galaxies fully explored",
         "Brings in fresh cookies from the cookie planet.", 9, 8, "shipment", 16, 16, 64, 1, 0, 0},
        {"Alchemy lab", "alchemy lab|alchemy labs|transmuted|[X] primordial element mastered|[X] primordial elements mastered",
         "Turns gold into cookies!", 10, 6, "alchemylab", 16, 16, 64, 2, 0, 16},
        {"Portal", "portal|portals|retrieved|[X] dimension enslaved|[X] dimensions enslaved",
         "Opens a door to the Cookieverse.", 11, 7, "portal", 32, 32, 64, 2, 0, 0},
        {"Time machine", "time machine|time machines|recovered|[X] century secured|[X] centuries secured",
         "Brings cookies from the past, before they were even eaten.", 12, 8, "timemachine", 32, 32, 64, 1, 0, 0},
        {"Antimatter condenser", "antimatter condenser|antimatter condensers|condensed|[X] extra quark flavor|[X] extra quark flavors",
         "Condenses the antimatter in the universe into cookies.", 13, 13, "antimattercondenser", 0, 64, 64, 1, 0, 0},
        {"Prism", "prism|prisms|converted|[X] new color discovered|[X] new colors discovered",
         "Converts light itself into cookies.", 14, 14, "prism", 16, 4, 64, 1, 0, 20},
        {"Chancemaker", "chancemaker|chancemakers|spontaneously generated|Chancemakers are powered by [X]-leaf clovers|Chancemakers are powered by [X]-leaf clovers",
         "Generates cookies out of thin air through sheer luck.", 15, 19, "chancemaker", 8, 64, 64, 1, 0, 0},
        {"Fractal engine", "fractal engine|fractal engines|iterated|[X] more dimension|[X] more dimensions",
         "Turns the universe into a fractal cookie.", 16, 20, "fractalEngine", 8, 64, 64, 1, 0, 0},
        {"Javascript console", "javascript console|javascript consoles|coded|[X] more line of code|[X] more lines of code",
         "Runs the game itself, but slower.", 17, 21, "javascriptconsole", 16, 16, 64, 1, 0, 0},
        {"Idleverse", "idleverse|idleverses|idled|[X] more universe|[X] more universes",
         "Generates cookies from nothing at all.", 18, 22, "idleverse", 16, 16, 64, 1, 0, 0},
    };

    for (i32 i = 0; i < 16; ++i) {
        const BuildingDef& d = defs[i];
        i32 id = i + 2; // Cursor=0, Grandma=1, so Farm starts at id=2
        auto curve = buildingCurve(id);
        Building::Art art;
        art.base = d.artBase;
        art.pic = std::string(d.artBase) + ".png";
        art.bg = std::string(d.artBase) + "Background.png";
        art.xV = d.artXV; art.yV = d.artYV; art.w = d.artW;
        art.rows = d.artRows; art.x = d.artX; art.y = d.artY;
        addBuilding(d.name, d.commonName, d.desc, d.icon, d.iconColumn,
                    art, curve.first, curve.second, standardBuy, standardCps);
    }

    // Cursor unlocked by default (matches JS Game.Object: this.locked=false for Cursor)
    objects[0]->locked = false;

    // --- wrinklers: 12 fixed slots (JS Game.wrinklers, getWrinklersMax base 10) ---
    wrinklers.reserve(12);
    for (i32 i = 0; i < 12; ++i) {
        Wrinkler w;
        w.id = i;
        w.phase = 0;
        w.close = 0;
        w.sucked = 0.0;
        w.hp = wrinklerHP;
        w.selected = 0;
        w.type = 0;
        wrinklers.push_back(w);
    }

    // --- buff type registry (JS Game.buffTypes, registration order = id) ---
    auto regBuff = [this](const std::string& name) {
        auto t = std::make_shared<BuffType>();
        t->name = name;
        t->id = static_cast<i32>(buffTypes.size());
        t->vanilla = true;
        buffTypes[t->id] = t;
    };
    regBuff("frenzy");
    regBuff("blood frenzy");      // shown as "Elder frenzy"
    regBuff("clot");
    regBuff("dragon harvest");
    regBuff("everything must go");
    regBuff("cursed finger");
    regBuff("click frenzy");
    regBuff("dragonflight");
    regBuff("cookie storm");
    regBuff("building buff");     // dynamic golden-cookie building buff
    regBuff("building debuff");   // dynamic golden-cookie building debuff
    regBuff("sugar blessing");
    regBuff("haggler luck");
    regBuff("haggler misery");
    regBuff("pixie luck");        // shown as "Crafty pixies"
}

void Game::loop() {
    f64 now = static_cast<f64>(now_ms());
    f64 dt = (now - time) / 1000.0;
    time = now;
    if (dt > 0.25) dt = 0.25; // spiral-of-death clamp
    logic(dt);
    loopT++;
}

void Game::logic(f64 dt) {
    (void)dt;
    if (onAscend || ascendTimer > 0.0) return;

    // per-frame building updates
    for (auto& b : objects) {
        if (b && b->eachFrame) b->eachFrame();
    }

    calculateGains();
    updateSpecial();
    updateGrandmapocalypse();
    particlesUpdate();

    // big cookie animation — wobble between 90% and 120% of size
    if (bigCookieState == 1) bigCookieSizeT = 0.9;
    else if (bigCookieState == 2) bigCookieSizeT = 1.2;
    else bigCookieSizeT = 1.0;
    bigCookieSizeD += (bigCookieSizeT - bigCookieSize) * 0.75;
    bigCookieSizeD *= 0.75;
    bigCookieSize += bigCookieSizeD;
    bigCookieSize = std::max(0.1, bigCookieSize);

    // milk progress
    milkProgress = static_cast<f64>(achievementsOwned) / 25.0;
    milkHd += (milkH - milkHd) * 0.02;

    // research
    if (researchT > 0) researchT--;
    if (researchT == 0 && nextResearch != 0) {
        for (auto& u : upgrades) {
            if (u && u->id == nextResearch) {
                if (!u->bought) {
                    u->unlocked = true;
                    recalculateGains_ = true;
                }
                break;
            }
        }
        nextResearch = 0;
        researchT = -1;
    }

    // seasons
    if (seasonT > 0) seasonT--;
    if (seasonT <= 0 && !season.empty() && season != baseSeason) {
        season = baseSeason;
        seasonT = -1;
    }

    // handle cookies: earn cookiesPs / fps per frame
    earn(cookiesPs / static_cast<f64>(fps));

    // minigames
    for (auto& b : objects) {
        if (b->minigameLoaded && b->minigame) {
            // b->minigame->logic(); // stubbed
        }
    }

    // wrinklers
    if (cpsSucked > 0.0) {
        f64 sucked = (cookiesPs / static_cast<f64>(fps)) * cpsSucked;
        dissolve(sucked);
        cookiesSucked += sucked;
    }

    // totalCookies accumulation
    for (auto& b : objects) {
        b->totalCookies += (b->storedTotalCps * globalCpsMult) / static_cast<f64>(fps);
    }

    // recalculate CpS every 10 seconds (for dynamic boosts such as Century egg)
    if (loopT % (fps * 10) == 0) recalculateGains_ = true;

    // unlocking / achievement checks every ~5 seconds
    if (loopT % (fps * 5) == 0 && objectsById.size() > 0) {
        if (std::isnan(cookies)) { cookies = 0.0; cookiesEarned = 0.0; recalculateGains_ = true; }

        f64 timePlayed = now_ms() - startDate;

        if (cookiesEarned >= 1000000.0 && (ascendTimer > 0.0 || resets == 0)) {
            if (timePlayed <= 1000.0 * 60.0 * 35.0) { /* Win('Speed baking I') */ }
            if (timePlayed <= 1000.0 * 60.0 * 25.0) { /* Win('Speed baking II') */ }
            if (timePlayed <= 1000.0 * 60.0 * 15.0) { /* Win('Speed baking III') */ }
            if (cookieClicks <= 15) { /* Win('Neverclick') */ }
            if (cookieClicks <= 0) { /* Win('True Neverclick') */ }
            if (cookiesEarned >= 1000000000.0 && upgradesOwned == 0) { /* Win('Hardcore') */ }
        }

        i32 buildingsOwned = 0;
        i32 minAmount = 100000;
        for (auto& b : objects) {
            buildingsOwned += b->amount;
            minAmount = std::min(b->amount, minAmount);
        }
        if (buildingsOwned >= 100) { /* Win('Builder') */ }
        if (buildingsOwned >= 500) { /* Win('Architect') */ }
        if (buildingsOwned >= 1000) { /* Win('Engineer') */ }
        if (buildingsOwned >= 2000) { /* Win('Lord of Constructs') */ }

        if (handmadeCookies >= 1000.0) { /* Win('Clicktastic'); Unlock('Plastic mouse'); */ }
        if (handmadeCookies >= 100000.0) { /* Win('Clickathlon'); Unlock('Iron mouse'); */ }
        if (handmadeCookies >= 10000000.0) { /* Win('Clickolympics'); Unlock('Titanium mouse'); */ }
        if (handmadeCookies >= 1000000000.0) { /* Win('Clickorama'); Unlock('Adamantium mouse'); */ }
    }

    // autosave every 60 seconds
    if ((loopT % (fps * 60) == 0 && loopT > fps * 10) || toSave) {
        toSave = false;
        writeSave();
    }

    // purchase flash feedback
    if (flashFrames > 0) --flashFrames;

    loopT++;
}

void Game::earn(f64 howmuch) {
    cookies += howmuch;
    cookiesEarned += howmuch;
}

void Game::spend(f64 howmuch) {
    cookies -= howmuch;
}

void Game::dissolve(f64 howmuch) {
    cookies -= howmuch;
    cookiesEarned -= howmuch;
    cookies = std::max(0.0, cookies);
    cookiesEarned = std::max(0.0, cookiesEarned);
}

f64 Game::mouseCps() {
    f64 add = 0.0;
    if (has("Thousand fingers")) add += 0.1;
    if (has("Million fingers")) add += 0.5;
    if (has("Billion fingers")) add += 5.0;
    if (has("Trillion fingers")) add += 50.0;
    if (has("Quadrillion fingers")) add += 500.0;
    if (has("Quintillion fingers")) add += 5000.0;
    if (has("Sextillion fingers")) add += 50000.0;
    if (has("Septillion fingers")) add += 500000.0;
    if (has("Octillion fingers")) add += 5000000.0;

    i32 num = 0;
    for (auto& b : objects) num += b->amount;
    if (auto* cur = getBuilding("Cursor")) num -= cur->amount;
    add = add * static_cast<f64>(num);

    if (has("Plastic mouse")) add += cookiesPs * 0.01;
    if (has("Iron mouse")) add += cookiesPs * 0.01;
    if (has("Titanium mouse")) add += cookiesPs * 0.01;
    if (has("Adamantium mouse")) add += cookiesPs * 0.01;
    if (has("Unobtainium mouse")) add += cookiesPs * 0.01;
    if (has("Eludium mouse")) add += cookiesPs * 0.01;
    if (has("Wishalloy mouse")) add += cookiesPs * 0.01;
    if (has("Fantasteel mouse")) add += cookiesPs * 0.01;
    if (has("Nevercrack mouse")) add += cookiesPs * 0.01;
    if (has("Armythril mouse")) add += cookiesPs * 0.01;
    if (has("Technobsidian mouse")) add += cookiesPs * 0.01;
    if (has("Plasmarble mouse")) add += cookiesPs * 0.01;

    if (has("Fortune #104")) add += cookiesPs * 0.01;

    f64 mult = 1.0;
    for (auto& cb : customMouseCps) mult += cb();

    if (has("Santa's helpers")) mult *= 1.1;
    if (has("Cookie egg")) mult *= 1.1;
    if (has("Halo gloves")) mult *= 1.1;
    mult *= eff("click");

    i32 godLvl = getGodLevel("labor");
    if (godLvl == 1) mult *= 1.15;
    else if (godLvl == 2) mult *= 1.1;
    else if (godLvl == 3) mult *= 1.05;

    for (auto& bf : buffs) {
        // buff multClick would go here (stubbed)
        (void)bf;
    }

    mult *= 1.0 + auraMult("Dragon Cursor") * 0.05;

    for (auto& cb : customMouseCpsMult) mult *= cb();

    i32 clickUpgrades = (has("Reinforced index finger") ? 1 : 0) +
                        (has("Carpal tunnel prevention cream") ? 1 : 0) +
                        (has("Ambidextrous") ? 1 : 0);
    f64 out = mult * ComputeCps(1.0, clickUpgrades, add);

    // Cursed finger override
    for (auto& bf : buffs) {
        // if buff is Cursed finger, out = buff.power (stubbed)
        (void)bf;
    }

    return out;
}

void Game::clickCookie(i32 amount) {
    if (onAscend || ascendTimer > 0.0) return;
    f64 now = static_cast<f64>(now_ms());
    if (now - lastClick < 1000.0 / 250.0) return; // min click interval
    lastClick = now;
    f64 amt = amount ? static_cast<f64>(amount) : mouseCps();
    earn(amt);
    handmadeCookies += amt;
    cookieClicks++;
    bigCookieSizeT = 1.2;   // pop to 120% on click
    bigCookieSizeD += 0.3;  // upward velocity for the bounce
}


f64 Game::ComputeCps(f64 baseCps, i32 clickUpgrades, f64 add) {
    return baseCps * (1.0 + static_cast<f64>(clickUpgrades) * 0.01) + add;
}

void Game::buyBuilding(Building* b, i32 amount) {
    if (!b) return;
    if (amount <= 0) amount = 1;
    i32 idx = -1;
    for (i32 i = 0; i < objectsN; ++i) {
        if (objects[i].get() == b) {
            idx = i;
            break;
        }
    }
    bool boughtAny = false;
    for (i32 i = 0; i < amount; ++i) {
        f64 price = b->getPrice();
        if (cookies < price) break;
        spend(price);
        b->buy(1);
        buildingsOwned++;
        recalculateGains_ = true;
        boughtAny = true;
    }
    if (boughtAny && idx >= 0) {
        flashBuildingIndex = idx;
        flashFrames = 12; // ~0.4s at 30 FPS
    }
}

void Game::sellBuilding(Building* b, i32 amount) {
    if (!b) return;
    if (amount == -1) amount = b->amount;
    if (amount <= 0) amount = 1;
    f64 total = 0.0;
    for (i32 i = 0; i < amount; ++i) {
        if (b->amount <= 0) break;
        f64 price = b->getPrice();
        f64 giveBack = b->getSellMultiplier();
        f64 back = std::floor(price * giveBack);
        total += back;
        b->sell(1);
        buildingsOwned--;
    }
    earn(total);
    recalculateGains_ = true;
}

void Game::calculateGains() {
    recalculateGains_ = false;

    cookiesPs = 0.0;
    f64 mult = 1.0;

    // --- prestige multiplier ---
    if (ascendTimer <= 0.0) {
        mult += prestige * 0.01 * heavenlyPower * GetHeavenlyMultiplier();
    }

    mult *= eff("cps");

    // --- heralds ---
    if (has("Heralds") && ascendTimer <= 0.0) {
        mult *= 1.0 + 0.01 * static_cast<f64>(heralds);
    }

    // --- specific cookie upgrades ---
    if (has("Specialized chocolate chips")) mult *= 1.01;
    if (has("Designer cocoa beans")) mult *= 1.02;
    if (has("Underworld ovens")) mult *= 1.03;
    if (has("Exotic nuts")) mult *= 1.04;
    if (has("Arcane sugar")) mult *= 1.05;

    if (has("Increased merriness")) mult *= 1.15;
    if (has("Improved jolliness")) mult *= 1.15;
    if (has("A lump of coal")) mult *= 1.01;
    if (has("An itchy sweater")) mult *= 1.01;
    if (has("Santa's dominion")) mult *= 1.2;

    if (has("Fortune #100")) mult *= 1.01;
    if (has("Fortune #101")) mult *= 1.07;

    if (has("Santa's legacy")) mult *= 1.0 + static_cast<f64>(santaLevel + 1) * 0.03;

    // --- building CpS ---
    for (auto& b : objects) {
        f64 storedCps;
        if (b->cpsFunc) {
            storedCps = b->cpsFunc(b.get());
        } else {
            storedCps = b->baseCps;
        }
        if (ascendTimer <= 0.0) {
            storedCps *= (1.0 + static_cast<f64>(b->level) * 0.01);
        }
        b->storedCps = storedCps;
        b->storedTotalCps = static_cast<f64>(b->amount) * storedCps;
        cookiesPs += b->storedTotalCps;
    }

    // --- "egg" building ---
    if (has("\"egg\"")) { cookiesPs += 9.0; }

    // --- milk / kittens ---
    milkProgress = static_cast<f64>(achievementsOwned) / 25.0;
    f64 milkMult = 1.0;
    if (has("Santa's milk and cookies")) milkMult *= 1.05;
    milkMult *= 1.0 + auraMult("Breath of Milk") * 0.05;
    milkMult *= eff("milk");

    f64 catMult = 1.0;
    if (has("Kitten helpers")) catMult *= (1.0 + milkProgress * 0.1 * milkMult);
    if (has("Kitten workers")) catMult *= (1.0 + milkProgress * 0.125 * milkMult);
    if (has("Kitten engineers")) catMult *= (1.0 + milkProgress * 0.15 * milkMult);
    if (has("Kitten overseers")) catMult *= (1.0 + milkProgress * 0.175 * milkMult);
    if (has("Kitten managers")) catMult *= (1.0 + milkProgress * 0.2 * milkMult);
    if (has("Kitten accountants")) catMult *= (1.0 + milkProgress * 0.2 * milkMult);
    if (has("Kitten specialists")) catMult *= (1.0 + milkProgress * 0.2 * milkMult);
    if (has("Kitten experts")) catMult *= (1.0 + milkProgress * 0.2 * milkMult);
    if (has("Kitten consultants")) catMult *= (1.0 + milkProgress * 0.2 * milkMult);
    if (has("Kitten assistants to the regional manager")) catMult *= (1.0 + milkProgress * 0.175 * milkMult);
    if (has("Kitten marketeers")) catMult *= (1.0 + milkProgress * 0.15 * milkMult);
    if (has("Kitten analysts")) catMult *= (1.0 + milkProgress * 0.125 * milkMult);
    if (has("Kitten executives")) catMult *= (1.0 + milkProgress * 0.115 * milkMult);
    if (has("Kitten angels")) catMult *= (1.0 + milkProgress * 0.1 * milkMult);
    if (has("Fortune #103")) catMult *= (1.0 + milkProgress * 0.05 * milkMult);

    mult *= catMult;

    // --- eggs ---
    f64 eggMult = 1.0;
    if (has("Chicken egg")) eggMult *= 1.01;
    if (has("Duck egg")) eggMult *= 1.01;
    if (has("Turkey egg")) eggMult *= 1.01;
    if (has("Quail egg")) eggMult *= 1.01;
    if (has("Robin egg")) eggMult *= 1.01;
    if (has("Ostrich egg")) eggMult *= 1.01;
    if (has("Cassowary egg")) eggMult *= 1.01;
    if (has("Salmon roe")) eggMult *= 1.01;
    if (has("Frogspawn")) eggMult *= 1.01;
    if (has("Shark egg")) eggMult *= 1.01;
    if (has("Turtle egg")) eggMult *= 1.01;
    if (has("Ant larva")) eggMult *= 1.01;
    if (has("Century egg")) {
        f64 day = std::floor((now_ms() - startDate) / 1000.0 / 10.0) * 10.0 / 60.0 / 60.0 / 24.0;
        day = std::min(day, 100.0);
        eggMult *= 1.0 + (1.0 - std::pow(1.0 - day / 100.0, 3.0)) * 0.1;
    }
    mult *= eggMult;

    // --- sugar baking ---
    if (has("Sugar baking")) mult *= (1.0 + std::min(100.0, lumps) * 0.01);

    // --- auras ---
    mult *= 1.0 + auraMult("Radiant Appetite");

    f64 rawCookiesPs = cookiesPs * mult;
    (void)rawCookiesPs; // CPS achievements check stubbed

    // --- bakery name easter eggs ---
    std::string name = bakeryName;
    for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (name == "orteil") mult *= 0.99;
    else if (name == "ortiel") mult *= 0.98;

    // --- wrinklers ---
    i32 sucking = 0;
    for (auto& w : wrinklers) if (w.phase == 2) ++sucking;
    f64 suckRate = 1.0 / 20.0;
    suckRate *= eff("wrinklerEat");
    cpsSucked = static_cast<f64>(sucking) * suckRate;

    if (has("Elder Covenant")) mult *= 0.95;

    if (has("Golden switch [off]")) {
        f64 goldenSwitchMult = 1.5;
        if (has("Residual luck")) {
            // goldenCookieUpgrades would be checked here (stubbed)
        }
        mult *= goldenSwitchMult;
    }
    if (has("Shimmering veil [off]")) {
        f64 veilMult = 0.5;
        if (has("Reinforced membrane")) veilMult += 0.1;
        mult *= 1.0 + veilMult;
    }
    if (has("Magic shenanigans")) mult *= 1000.0;
    if (has("Occult obstruction")) mult *= 0.0;

    // --- buffs (multCpS) ---
    for (auto& bf : buffs) {
        (void)bf;
    }

    unbuffedCps = cookiesPs * mult;
    globalCpsMult = mult;
    cookiesPs *= globalCpsMult;

    computedMouseCps = mouseCps();
}

bool Game::has(const std::string& name) const {
    for (auto& u : upgrades) if (u && u->name == name) return u->bought;
    return false;
}

bool Game::hasAchiev(const std::string& name) const {
    for (auto& a : achievements) if (a && a->name == name) return a->won;
    return false;
}

i32 Game::getGodLevel(const std::string& /*name*/) const {
    return 0; // pantheon not implemented yet
}

void Game::unlock(const std::string& name) {
    for (auto& u : upgrades) if (u && u->name == name) u->unlocked = true;
}

Building* Game::getBuilding(const std::string& name) {
    for (auto& b : objects) if (b && b->name == name) return b.get();
    return nullptr;
}

std::string Game::getBakeryName() const {
    return "Cookie Bakery";
}

void Game::updateSpecial() { /* TODO: special menu / news ticker */ }
void Game::updateGrandmapocalypse() { /* TODO */ }
void Game::particlesUpdate() { /* TODO */ }
void Game::textParticlesUpdate() { /* TODO */ }

f64 Game::eff(const std::string& name, f64 def) {
    auto it = effs_.find(name);
    return (it != effs_.end()) ? it->second : def;
}

f64 Game::GetTieredCpsMult(Building* /*me*/) {
    return 1.0; // tiered upgrades not implemented yet
}

f64 Game::magicCpS(const std::string& /*name*/) {
    return 1.0; // magic (grimoire) not implemented yet
}

f64 Game::auraMult(const std::string& /*name*/) {
    return 0.0; // dragon auras not implemented yet
}

f64 Game::GetHeavenlyMultiplier() {
    return 1.0; // prestige not implemented yet
}

void Game::updateShimmers() { /* TODO */ }
void Game::updateBuffs() { /* TODO */ }

std::string Game::writeSave(i32 type) { return SaveLoad::writeSave(*this, type); }
bool Game::loadSave(const std::string& data) { return SaveLoad::loadSave(*this, data); }
bool Game::importSaveCode(const std::string& code) { return loadSave(code); }
