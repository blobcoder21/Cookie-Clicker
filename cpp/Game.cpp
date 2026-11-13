#include "Game.h"
#include "SaveLoad.h"
#include "Building.h"
#include "Garden.h"
#include "Grimoire.h"
#include "Pantheon.h"
#include "Dungeons.h"
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

    // --- Upgrades (starter set; core economy, UI for buying deferred) ---
    // All starter upgrades are unlocked so they become buyable once affordable.
    // Their effects are applied via the existing `has()` checks in
    // calculateGains()/mouseCps()/building CPS funcs once bought.
    auto addUpgrade = [this](std::string name, std::string desc, f64 price,
                             Building* tieredBuilding = nullptr, i32 tier = 0) {
        auto u = std::make_unique<Upgrade>(std::move(name), std::move(desc), price, "");
        u->id = static_cast<i32>(upgrades.size());
        u->unlocked = true; // starter upgrades are unlocked from the start
        u->tieredBuilding = tieredBuilding;
        u->tier = tier;
        if (tieredBuilding) tieredBuilding->tieredUpgrades.push_back(u.get());
        upgradesById.push_back(u.get());
        upgrades.push_back(std::move(u));
    };

    Building* cursor = getBuilding("Cursor");
    Building* grandma = getBuilding("Grandma");

    // Click upgrades (affect clickUpgrades in mouseCps())
    addUpgrade("Reinforced index finger", "Clicks are +1 more powerful.", 15.0);
    addUpgrade("Carpal tunnel prevention cream", "Clicks are +1 more powerful.", 100.0);
    addUpgrade("Ambidextrous", "Clicks are +1 more powerful.", 500.0);

    // Finger upgrades (affect Cursor CPS + mouseCps add)
    addUpgrade("Thousand fingers", "Autoclicker +0.1 cookies per click.", 10000.0, cursor, 1);
    addUpgrade("Million fingers", "Autoclicker +0.5 cookies per click.", 100000000.0, cursor, 2);

    // Grandma multipliers
    addUpgrade("Bingo center/Research facility", "Grandmas are 4x more efficient.", 1000000.0, grandma, 1);
    addUpgrade("Ritual rolling pins", "Grandmas are 2x more efficient.", 50000000000.0, grandma, 2);

    // --- Heavenly upgrades (purchased with heavenly chips, not cookies) ---
    // These are persisted via the upgrade bitstring (unlocked/bought) in SaveLoad.
    // Prestige-pool upgrades are exempt from ordinary cookie purchase discounts.
    {
        auto addHeavenly = [this](std::string name, std::string desc, f64 price,
                                 std::vector<std::string> parents = {},
                                 std::function<bool()> showIf = nullptr,
                                 bool noPerm = false) {
            auto u = std::make_unique<Upgrade>(std::move(name), std::move(desc), price, "");
            u->id = static_cast<i32>(upgrades.size());
            u->unlocked = false;   // heavenly upgrades unlock via the prestige chain
            u->bought = false;
            u->pool = "prestige";
            u->parents = std::move(parents);
            u->showIf = std::move(showIf);
            u->noPerm = noPerm;
            upgradesById.push_back(u.get());
            upgrades.push_back(std::move(u));
        };

        // --- The five heavenly multiplier unlocks (chain: Legacy is the root) ---
        addHeavenly("Legacy",
            "This is the first heavenly upgrade; it unlocks the <b>Heavenly chips</b> system. "
            "Each time you ascend, the cookies you made in your past life are turned into "
            "<b>heavenly chips</b> and <b>prestige</b>. Your <b>prestige level</b> also gives "
            "you a permanent <b>+1% CpS</b> per level.", 1.0);

        // Heavenly cookies: chain link after Legacy (+10% permanent CpS via power).
        addHeavenly("Heavenly cookies",
            "Cookie production multiplier <b>+10% permanently</b>.", 3.0,
            {"Legacy"});

        // The five multiplier unlocks. Each has noPerm=1 (cannot go in a permanent
        // slot) and unlocks the next in the chain once bought.
        addHeavenly("Heavenly chip secret",
            "Unlocks <b>5%</b> of the potential of your prestige level.", 11.0,
            {}, nullptr, true);
        addHeavenly("Heavenly cookie stand",
            "Unlocks <b>20%</b> of the potential of your prestige level.", 1111.0,
            {}, nullptr, true);
        addHeavenly("Heavenly bakery",
            "Unlocks <b>50%</b> of the potential of your prestige level.", 111111.0,
            {}, nullptr, true);
        addHeavenly("Heavenly confectionery",
            "Unlocks <b>75%</b> of the potential of your prestige level.", 11111111.0,
            {}, nullptr, true);
        addHeavenly("Heavenly key",
            "Unlocks <b>100%</b> of the potential of your prestige level.", 1111111111.0,
            {}, nullptr, true);

        // --- Remaining prestige-pool upgrades (dataset complete per v2.021 presets) ---
        // Costs, parents and visibility conditions mirror main.js exactly.
        addHeavenly("Distilled essence of redoubled luck",
            "Golden cookies (and all other things that spawn, such as reindeer) have "
            "<b>1% chance of being doubled</b>.", 7777777.0,
            {"Divine bakeries", "Residual luck"});
        addHeavenly("Fortune cookies",
            "The news ticker may occasionally have <b>fortunes</b>, which may be clicked "
            "for something good.", 77777777777.0,
            {"Distilled essence of redoubled luck"});
        addHeavenly("Heavenly luck",
            "Golden cookies appear <b>5%</b> more often.", 77.0,
            {});
        addHeavenly("Lasting fortune",
            "Golden cookies effects last <b>10%</b> longer.", 777.0,
            {"Heavenly luck"});
        addHeavenly("Decisive fate",
            "Golden cookies stay <b>5%</b> longer.", 7777.0,
            {"Lasting fortune"});
        addHeavenly("Lucky digit",
            "<b>+1%</b> prestige level effect on CpS. <b>+1%</b> golden cookie effect "
            "duration. <b>+1%</b> golden cookie lifespan. Only appears when your prestige "
            "level ends in 7.", 777.0,
            {"Heavenly luck"},
            [this]() { return static_cast<i32>(std::ceil(prestige)) % 10 == 7; });
        addHeavenly("Lucky number",
            "<b>+1%</b> prestige level effect on CpS. <b>+1%</b> golden cookie effect "
            "duration. <b>+1%</b> golden cookie lifespan. Only appears when your prestige "
            "level ends in 777.", 77777.0,
            {"Lucky digit", "Lasting fortune"},
            [this]() { return static_cast<i32>(std::ceil(prestige)) % 1000 == 777; });
        addHeavenly("Lucky payout",
            "<b>+1%</b> prestige level effect on CpS. <b>+1%</b> golden cookie effect "
            "duration. <b>+1%</b> golden cookie lifespan. Only appears when your prestige "
            "level ends in 777777.", 77777777.0,
            {"Lucky number", "Decisive fate"},
            [this]() { return static_cast<i32>(std::ceil(prestige)) % 1000000 == 777777; });
        addHeavenly("Persistent memory",
            "Subsequent research will be <b>10 times</b> as fast.", 500.0,
            {});
        addHeavenly("Season switcher",
            "Allows you to <b>trigger seasonal events</b> at will, for a price.", 1111.0,
            {"Heralds"});
        addHeavenly("Heralds",
            "You now benefit from the boost provided by <b>heralds</b>. Each herald gives "
            "you <b>+1% CpS</b>.", 100.0,
            {});
        addHeavenly("Tin of british tea biscuits",
            "Contains an assortment of fancy biscuits.", 25.0,
            {"Heavenly cookies"});
        addHeavenly("Box of macarons",
            "Contains an assortment of macarons.", 25.0,
            {"Heavenly cookies"});
        addHeavenly("Box of brand biscuits",
            "Contains an assortment of popular biscuits.", 25.0,
            {"Heavenly cookies"});
        addHeavenly("Tin of butter cookies",
            "Contains an assortment of rich butter cookies.", 25.0,
            {"Heavenly cookies"});
        addHeavenly("Golden switch",
            "Unlocks the <b>golden switch</b>, which passively boosts your CpS by 50% but "
            "disables golden cookies.", 999.0,
            {"Heavenly luck"});
        addHeavenly("Classic dairy selection",
            "Unlocks the <b>milk selector</b>, letting you pick which milk is displayed.", 9.0,
            {});
        addHeavenly("Fanciful dairy selection",
            "Contains more exotic flavors for your milk selector.", 1000000.0,
            {"Classic dairy selection"});
        addHeavenly("Starter kit",
            "You start with <b>10 cursors</b>.", 50.0,
            {"Tin of british tea biscuits", "Box of macarons", "Box of brand biscuits",
             "Tin of butter cookies"});
        addHeavenly("Starter kitchen",
            "You start with <b>5 grandmas</b>.", 5000.0,
            {"Starter kit"});
        addHeavenly("Halo gloves",
            "Clicks are <b>10% more powerful</b>.", 55555.0,
            {"Starter kit"});
        addHeavenly("Unholy bait",
            "Wrinklers appear <b>5 times</b> as fast.", 44444.0,
            {"Starter kitchen"});
        addHeavenly("Elder spice",
            "You can attract <b>2 more wrinklers</b>.", 444444.0,
            {"Unholy bait"});
        addHeavenly("Sacrilegious corruption",
            "Wrinklers regurgitate <b>5%</b> more cookies.", 444444.0,
            {"Unholy bait"});
        addHeavenly("Wrinkly cookies",
            "Cookie production multiplier <b>+10% permanently</b>.", 6666666.0,
            {"Sacrilegious corruption", "Elder spice"});
        addHeavenly("Residual luck",
            "While the golden switch is on, you gain an additional <b>+10% CpS</b> per "
            "golden cookie upgrade owned.", 99999.0,
            {"Golden switch"});
        addHeavenly("Divine discount",
            "Buildings are <b>1% cheaper</b>.", 99999.0,
            {"Decisive fate"});
        addHeavenly("Divine sales",
            "Upgrades are <b>1% cheaper</b>.", 99999.0,
            {"Decisive fate"});
        addHeavenly("Divine bakeries",
            "Cookie upgrades are <b>5 times cheaper</b>.", 399999.0,
            {"Divine sales", "Divine discount"});
        addHeavenly("Shimmering veil",
            "Unlocks the <b>shimmering veil</b>, a switch that passively boosts your CpS "
            "by <b>50%</b>.", 999999999.0,
            {"Distilled essence of redoubled luck"});
        addHeavenly("Cosmic beginner's luck",
            "Prior to purchasing the <b>Heavenly chip secret</b> upgrade in a run, random "
            "drops are <b>5 times more common</b>.", 999999999.0 * 15.0,
            {"Shimmering veil"});
        addHeavenly("Reinforced membrane",
            "The <b>shimmering veil</b> is more resistant, and has a <b>10% chance</b> not "
            "to break. It also gives <b>+10%</b> more CpS.", 999999999.0 * 15.0,
            {"Shimmering veil"});
        addHeavenly("Stevia Caelestis",
            "Sugar lumps ripen <b>an hour sooner</b>.", 100000000.0,
            {"Wrinkly cookies"});
        addHeavenly("Diabetica Daemonicus",
            "Sugar lumps mature <b>an hour sooner</b>.", 300000000.0,
            {"Stevia Caelestis", "Lucifer"});
        addHeavenly("Sucralosia Inutilis",
            "Bifurcated sugar lumps appear <b>5% more often</b> and are <b>5% more likely</b> "
            "to drop 2 lumps.", 1000000000.0,
            {"Diabetica Daemonicus"});
        addHeavenly("Sugar baking",
            "Each unspent sugar lump (up to 100) gives <b>+1% CpS</b>.", 200000000.0,
            {"Stevia Caelestis"});
        addHeavenly("Sugar craving",
            "Once an ascension, you may use the \"Sugar frenzy\" switch to <b>triple your "
            "CpS</b> for 1 hour, at the cost of <b>1 sugar lump</b>.", 400000000.0,
            {"Sugar baking"});
        addHeavenly("Sugar aging process",
            "Each grandma (up to 600) makes sugar lumps ripen <b>6 seconds</b> sooner.",
            600000000.0,
            {"Sugar craving", "Diabetica Daemonicus"});
        addHeavenly("Eye of the wrinkler",
            "Mouse over a wrinkler to see how many cookies are in its stomach.", 99999999.0,
            {"Wrinkly cookies"});
        addHeavenly("Inspired checklist",
            "Unlocks the <b>Buy all</b> feature and the <b>Vault</b>.", 900000.0,
            {"Persistent memory", "Permanent upgrade slot IV"});
        addHeavenly("Genius accounting",
            "Unlocks <b>extra price information</b>.", 2000000.0,
            {"Inspired checklist"});
        addHeavenly("Label printer",
            "Mouse over an upgrade to see its tier.", 999999.0,
            {"Starter kitchen"});
        addHeavenly("Keepsakes",
            "Seasonal random drops have a <b>1/5 chance</b> to carry over through "
            "ascensions.", 1111111111.0,
            {"Starsnow", "Starlove", "Starterror", "Startrade", "Starspawn"});
        addHeavenly("Sugar crystal cookies",
            "Cookie production multiplier <b>+5% permanently</b>, and <b>+1%</b> for every "
            "building type level 10 or higher.", 1000000000.0,
            {"Sugar baking"});
        addHeavenly("Box of maybe cookies",
            "Contains an assortment of...something.", 333000000000.0,
            {"Sugar crystal cookies"});
        addHeavenly("Box of not cookies",
            "Contains an assortment of...something.", 333000000000.0,
            {"Sugar crystal cookies"});
        addHeavenly("Box of pastries",
            "Contains an assortment of delicious pastries.", 333000000000.0,
            {"Sugar crystal cookies"});
        addHeavenly("Starspawn",
            "Eggs drop <b>10%</b> more often. Golden cookies appear <b>2%</b> more often "
            "during Easter.", 111111.0,
            {"Season switcher"});
        addHeavenly("Starsnow",
            "Christmas cookies drop <b>5%</b> more often. Reindeer appear <b>5%</b> more "
            "often.", 111111.0,
            {"Season switcher"});
        addHeavenly("Starterror",
            "Spooky cookies drop <b>10%</b> more often. Golden cookies appear <b>2%</b> "
            "more often during Halloween.", 111111.0,
            {"Season switcher"});
        addHeavenly("Starlove",
            "Heart cookies are <b>50%</b> more powerful. Golden cookies appear <b>2%</b> "
            "more often during Valentines.", 111111.0,
            {"Season switcher"});
        addHeavenly("Startrade",
            "Golden cookies appear <b>5%</b> more often during Business day.", 111111.0,
            {"Season switcher"});
        // Angel upgrades (price factor 7, per-v2.021 presets)
        addHeavenly("Twin Gates of Transcendence",
            "You now <b>keep making cookies while the game is closed</b>, at the rate of "
            "<b>5%</b> of your regular CpS.", 1.0,
            {});
        addHeavenly("Angels",
            "Lowest-ranking at the first sphere of pastry heaven.", 7.0,
            {"Twin Gates of Transcendence"});
        addHeavenly("Archangels",
            "Members of the first sphere of pastry heaven.", 49.0,
            {"Angels"});
        addHeavenly("Virtues",
            "Found at the second sphere of pastry heaven.", 343.0,
            {"Archangels"});
        addHeavenly("Dominions",
            "Ruling over the second sphere of pastry heaven.", 2401.0,
            {"Virtues"});
        addHeavenly("Cherubim",
            "Sieging at the first sphere of pastry heaven.", 16807.0,
            {"Dominions"});
        addHeavenly("Seraphim",
            "Leading the first sphere of pastry heaven.", 117649.0,
            {"Cherubim"});
        addHeavenly("God",
            "Like Santa, but less fun.", 823543.0,
            {"Seraphim"});
        // Demon upgrades (price factor 7)
        addHeavenly("Belphegor",
            "A demon of shortcuts and laziness.", 7.0,
            {"Twin Gates of Transcendence"});
        addHeavenly("Mammon",
            "The demonic embodiment of wealth.", 49.0,
            {"Belphegor"});
        addHeavenly("Abaddon",
            "Master of overindulgence, governs the wrinkler brood.", 343.0,
            {"Mammon"});
        addHeavenly("Satan",
            "The counterpoint to everything righteous.", 2401.0,
            {"Abaddon"});
        addHeavenly("Asmodeus",
            "This demon draws his power from the desire for cookies.", 16807.0,
            {"Satan"});
        addHeavenly("Beelzebub",
            "The festering incarnation of blight and disease.", 117649.0,
            {"Asmodeus"});
        addHeavenly("Lucifer",
            "Also known as the Lightbringer.", 823543.0,
            {"Beelzebub"});
        addHeavenly("Chimera",
            "Synergy upgrades are <b>2% cheaper</b>. You gain another <b>+5%</b> of your "
            "regular CpS while the game is closed.", 40353607.0,  // 7^9
            {"God", "Lucifer", "Synergies Vol. II"});
        addHeavenly("Synergies Vol. I",
            "Unlocks a new tier of upgrades that affect <b>2 buildings</b> at once. "
            "Appear once you have <b>15</b> of both buildings.", 222222.0,
            {"Satan", "Dominions"});
        addHeavenly("Synergies Vol. II",
            "Unlocks a new tier of upgrades that affect <b>2 buildings</b> at once. "
            "Appear once you have <b>75</b> of both buildings.", 2222222.0,
            {"Beelzebub", "Seraphim", "Synergies Vol. I"});
        addHeavenly("Five-finger discount",
            "All upgrades are <b>1% cheaper per 100 cursors</b>.", 555555.0,
            {"Halo gloves", "Abaddon"});
        addHeavenly("Golden cookie alert sound",
            "Unlocks the <b>golden cookie sound selector</b>.", 9999.0,
            {"Decisive fate", "Golden switch"});
        addHeavenly("Basic wallpaper assortment",
            "Unlocks the <b>background selector</b>.", 99.0,
            {"Classic dairy selection"});
        addHeavenly("Permanent upgrade slot I",
            "Unlocks the first permanent upgrade slot.", 100.0,
            {});
        addHeavenly("Permanent upgrade slot II",
            "Unlocks the second permanent upgrade slot.", 2000.0,
            {"Permanent upgrade slot I"});
        addHeavenly("Permanent upgrade slot III",
            "Unlocks the third permanent upgrade slot.", 30000.0,
            {"Permanent upgrade slot II"});
        addHeavenly("Permanent upgrade slot IV",
            "Unlocks the fourth permanent upgrade slot.", 400000.0,
            {"Permanent upgrade slot III"});
        addHeavenly("Permanent upgrade slot V",
            "Unlocks the fifth permanent upgrade slot.", 5000000.0,
            {"Permanent upgrade slot IV"});
        addHeavenly("Kitten angels",
            "You gain <b>more CpS</b> the more milk you have.", 9000.0,
            {"Dominions"});
        addHeavenly("How to bake your dragon",
            "Allows you to purchase a <b>crumbly egg</b> once you have earned 1 million "
            "cookies.", 9.0,
            {});
    }

    // --- Minigames (core logic; UI deferred) ---
    // Attach a minigame to each building that supports one.
    auto attachMinigame = [this](i32 buildingId, std::unique_ptr<Minigame> m) {
        m->init();
        if (buildingId >= 0 && buildingId < objectsN) {
            Building* b = objectsById[buildingId];
            b->minigame = m.get();
            b->minigameLoaded = true;
            b->minigameName = m->name;
            b->onMinigame = true;
        }
        minigames[buildingId] = std::move(m);
    };

    // Farm (id=2) -> Garden
    attachMinigame(2, std::make_unique<Garden>(objectsById[2]));
    // Temple (id=6) -> Pantheon
    attachMinigame(6, std::make_unique<Pantheon>(objectsById[6]));
    // Wizard tower (id=7) -> Grimoire
    attachMinigame(7, std::make_unique<Grimoire>(objectsById[7]));
    // Dungeons (standalone, keyed by -1)
    {
        auto d = std::make_unique<Dungeons>(nullptr);
        d->init();
        minigames[-1] = std::move(d);
    }

    // --- News ticker message pool ---
    // Messages are generated dynamically from game state (buildings, cookies, CpS).
    // The pool is refreshed each time the ticker cycles.
    tickerMessages_.clear();
    tickerIndex_ = 0;
    tickerTimer_ = tickerInterval_;
    tickerIsFortune_ = false;

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
    if (prefs.particles) particlesUpdate();

    // big cookie animation — wobble between 90% and 120% of size
    if (prefs.wobbly) {
        if (bigCookieState == 1) bigCookieSizeT = 0.9;
        else if (bigCookieState == 2) bigCookieSizeT = 1.2;
        else bigCookieSizeT = 1.0;
        bigCookieSizeD += (bigCookieSizeT - bigCookieSize) * 0.75;
        bigCookieSizeD *= 0.75;
        bigCookieSize += bigCookieSizeD;
        // Snap to rest when the spring has settled, so the cookie doesn't keep
        // micro-wobbling forever after a click.
        if (std::fabs(bigCookieSizeT - bigCookieSize) < 0.001 &&
            std::fabs(bigCookieSizeD) < 0.001) {
            bigCookieSize = bigCookieSizeT;
            bigCookieSizeD = 0.0;
        }
    }
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
            b->minigame->eachFrame();
        }
    }
    // standalone minigames (e.g. Dungeons not tied to a building)
    if (auto it = minigames.find(-1); it != minigames.end() && it->second) {
        it->second->eachFrame();
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

        // Unlock the heavenly upgrade chain as prestige grows (JS Game.Logic).
        unlockHeavenlyChain();
        unlockAvailablePrestigeUpgrades();

        if (handmadeCookies >= 1000.0) { /* Win('Clicktastic'); Unlock('Plastic mouse'); */ }
        if (handmadeCookies >= 100000.0) { /* Win('Clickathlon'); Unlock('Iron mouse'); */ }
        if (handmadeCookies >= 10000000.0) { /* Win('Clickolympics'); Unlock('Titanium mouse'); */ }
        if (handmadeCookies >= 1000000000.0) { /* Win('Clickorama'); Unlock('Adamantium mouse'); */ }
    }

    // autosave every 60 seconds (respects prefs.autosave)
    if (prefs.autosave && ((loopT % (fps * 60) == 0 && loopT > fps * 10) || toSave)) {
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

    out *= minigameClickBonus();
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

    // prefs.cookiesound is a runtime flag (decoded from savegame via getPref()).
    // It gates Audio::playSound() when audio is wired up; the flag itself is
    // stored and can be toggled via setPref()/togglePref().
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

bool Game::buyUpgrade(const std::string& name) {
    for (auto& u : upgrades) {
        if (u && u->name == name) {
            if (!u->canBuy()) return false;   // not unlocked or already bought
            if (cookies < u->price) return false;
            spend(u->price);
            u->bought = true;
            recalculateGains_ = true;
            return true;
        }
    }
    return false;
}

void Game::calculateGains() {
    recalculateGains_ = false;

    cookiesPs = 0.0;
    f64 mult = 1.0;

    // --- prestige multiplier ---
    if (ascendTimer <= 0.0) {
        mult += prestige * 0.01 * heavenlyPower * GetHeavenlyMultiplier();
    }

    // --- pseudoCookie (permanent CPS multiplier) upgrades ---
    // Heavenly cookies (+10%), Wrinkly cookies (+10%), Sugar crystal cookies
    // (+5% + per-building-level), etc. Mirrors JS CalculateGains.
    for (auto& uptr : upgrades) {
        auto* u = uptr.get();
        if (!u || !u->bought) continue;
        if (u->pool == "prestige") {
            if (u->name == "Heavenly cookies") mult *= 1.10;
            else if (u->name == "Wrinkly cookies") mult *= 1.10;
            else if (u->name == "Sugar crystal cookies") {
                f64 power = 5.0;
                // +1% for every building type level 10 or higher.
                i32 levels10 = 0;
                for (auto& b : objects) if (b && b->level >= 10) ++levels10;
                power += static_cast<f64>(levels10);
                mult *= 1.0 + power * 0.01;
            }
        }
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

    // --- minigame CPS bonus ---
    cookiesPs *= minigameCookieBonus();

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

f64 Game::minigameCookieBonus() const {
    f64 mult = 1.0;
    for (auto& kv : minigames) {
        if (kv.second) mult *= kv.second->cookieBonus();
    }
    return mult;
}

f64 Game::minigameClickBonus() const {
    f64 mult = 1.0;
    for (auto& kv : minigames) {
        if (kv.second) mult *= kv.second->clickBonus();
    }
    return mult;
}

Minigame* Game::getMinigame(i32 buildingId) {
    auto it = minigames.find(buildingId);
    return (it != minigames.end()) ? it->second.get() : nullptr;
}

bool Game::hasMinigame(i32 buildingId) const {
    auto it = minigames.find(buildingId);
    return (it != minigames.end() && it->second != nullptr);
}

std::string Game::getBakeryName() const {
    return "Cookie Bakery";
}

bool Game::getPref(const std::string& name) const {
    if (name == "particles") return prefs.particles;
    if (name == "numbers") return prefs.numbers;
    if (name == "autosave") return prefs.autosave;
    if (name == "autoupdate") return prefs.autoupdate;
    if (name == "milk") return prefs.milk;
    if (name == "fancy") return prefs.fancy;
    if (name == "warn") return prefs.warn;
    if (name == "cursors") return prefs.cursors;
    if (name == "focus") return prefs.focus;
    if (name == "format") return prefs.format;
    if (name == "notifs") return prefs.notifs;
    if (name == "wobbly") return prefs.wobbly;
    if (name == "monospace") return prefs.monospace;
    if (name == "filters") return prefs.filters;
    if (name == "cookiesound") return prefs.cookiesound;
    if (name == "crates") return prefs.crates;
    if (name == "showBackupWarning") return prefs.showBackupWarning;
    if (name == "extraButtons") return prefs.extraButtons;
    if (name == "askLumps") return prefs.askLumps;
    if (name == "customGrandmas") return prefs.customGrandmas;
    if (name == "timeout") return prefs.timeout;
    return false;
}

void Game::setPref(const std::string& name, bool value) {
    if (name == "particles") { prefs.particles = value; return; }
    if (name == "numbers") { prefs.numbers = value; return; }
    if (name == "autosave") { prefs.autosave = value; return; }
    if (name == "autoupdate") { prefs.autoupdate = value; return; }
    if (name == "milk") { prefs.milk = value; return; }
    if (name == "fancy") { prefs.fancy = value; return; }
    if (name == "warn") { prefs.warn = value; return; }
    if (name == "cursors") { prefs.cursors = value; return; }
    if (name == "focus") { prefs.focus = value; return; }
    if (name == "format") { prefs.format = value; return; }
    if (name == "notifs") { prefs.notifs = value; return; }
    if (name == "wobbly") { prefs.wobbly = value; return; }
    if (name == "monospace") { prefs.monospace = value; return; }
    if (name == "filters") { prefs.filters = value; return; }
    if (name == "cookiesound") { prefs.cookiesound = value; return; }
    if (name == "crates") { prefs.crates = value; return; }
    if (name == "showBackupWarning") { prefs.showBackupWarning = value; return; }
    if (name == "extraButtons") { prefs.extraButtons = value; return; }
    if (name == "askLumps") { prefs.askLumps = value; return; }
    if (name == "customGrandmas") { prefs.customGrandmas = value; return; }
    if (name == "timeout") { prefs.timeout = value; return; }
}

bool Game::togglePref(const std::string& name) {
    bool cur = getPref(name);
    setPref(name, !cur);
    return !cur;
}

void Game::updateSpecial() {
    // --- News ticker ---
    // Advance the ticker timer; when it expires, cycle to the next message.
    // Fortune checks happen only during natural ticker cycling (here), never
    // on manual ticker clicks.
    tickerTimer_ -= 1.0 / static_cast<f64>(fps);
    if (tickerTimer_ <= 0.0) {
        tickerTimer_ = tickerInterval_;
        advanceTicker();
    }
}

void Game::advanceTicker() {
    // Build a fresh pool of ticker messages from current game state.
    tickerMessages_.clear();

    // --- Building-based messages ---
    for (auto& b : objects) {
        if (!b || b->amount == 0) continue;
        std::string msg = "Your " + b->plural + " have baked " +
                          std::to_string(static_cast<i64>(b->totalCookies)) + " cookies.";
        tickerMessages_.push_back(msg);
    }

    // --- Money-based messages ---
    tickerMessages_.push_back("You have " +
                              std::to_string(static_cast<i64>(cookies)) + " cookies in the bank.");
    tickerMessages_.push_back("You have earned " +
                              std::to_string(static_cast<i64>(cookiesEarned)) + " cookies in total.");

    // --- Cookies-produced (CpS) messages ---
    tickerMessages_.push_back("You're producing " +
                              std::to_string(static_cast<i64>(cookiesPs)) + " cookies per second.");
    tickerMessages_.push_back("Your bakery is making " +
                              std::to_string(static_cast<i64>(cookiesPs)) + " cookies every second.");

    // --- Generic messages ---
    tickerMessages_.push_back("Your family agrees to try some of your cookies.");
    tickerMessages_.push_back("The cookies smell delicious.");
    tickerMessages_.push_back("More cookies! Always more cookies.");

    if (tickerMessages_.empty()) {
        tickerMessages_.push_back("Welcome to your cookie bakery.");
    }

    // Cycle to next message in pool.
    tickerIndex_ = (tickerIndex_ + 1) % static_cast<i32>(tickerMessages_.size());
    std::string msg = tickerMessages_[tickerIndex_];

    // --- Fortune check (natural cycling only) ---
    tickerIsFortune_ = false;
    if (hasFortuneCookies()) {
        f64 chance = 0.02; // 2% base
        if (hasAchiev("O Fortuna")) chance = 0.04; // 4% with O Fortuna achievement
        // Pseudo-random check based on frame/time for determinism
        f64 roll = static_cast<f64>((static_cast<i64>(now_ms()) + tickerIndex_) % 10000) / 10000.0;
        if (roll < chance) {
            tickerIsFortune_ = true;
            msg = "FORTUNE: A clickable fortune cookie has appeared!";
        }
    }

    // Store the message for UI retrieval (we keep it in tickerMessages_[tickerIndex_]
    // but need a separate slot for the fortune variant). Store fortune override.
    tickerFortuneMsg_ = msg;
}

std::string Game::tickerMessage() const {
    if (tickerIsFortune_) return tickerFortuneMsg_;
    if (tickerIndex_ >= 0 && tickerIndex_ < static_cast<i32>(tickerMessages_.size()))
        return tickerMessages_[tickerIndex_];
    return "";
}

bool Game::tickerIsFortune() const { return tickerIsFortune_; }

void Game::clickTickerFortune() {
    if (!tickerIsFortune_) return;
    ++fortunesClicked_;
    tickerIsFortune_ = false;
    // Fortune bonus: grant cookies based on CpS (like a "fortune" cookie reward)
    f64 reward = cookiesPs * 60.0 * 5.0; // 5 minutes of CpS
    earn(reward);
    handmadeCookies += reward;
    cookieClicks++;
}

bool Game::buyHeavenlyUpgrade(const std::string& name) {
    for (auto& u : upgrades) {
        if (u && u->name == name) {
            if (!u->canBuy()) return false;   // not unlocked or already bought
            // Check all prerequisite parents are bought.
            for (const auto& parentName : u->parents) {
                if (!has(parentName)) return false;
            }
            // Prestige-pool upgrades are exempt from ordinary purchase
            // discounts: price is the raw base price (rounded up).
            f64 price = u->getPrice();
            if (heavenlyChips < price) return false;
            heavenlyChips -= price;
            heavenlyChipsSpent += price;
            u->bought = true;
            recalculateGains_ = true;
            return true;
        }
    }
    return false;
}

bool Game::hasFortuneCookies() const {
    return has("Fortune cookies");
}

void Game::updateGrandmapocalypse() { /* TODO */ }
void Game::particlesUpdate() { /* TODO */ }
void Game::textParticlesUpdate() { /* TODO */ }

f64 Game::eff(const std::string& name, f64 def) {
    auto it = effs_.find(name);
    return (it != effs_.end()) ? it->second : def;
}

f64 Game::GetTieredCpsMult(Building* me) {
    f64 mult = 1.0;
    if (!me) return mult;
    for (auto* u : me->tieredUpgrades) {
        if (u && u->bought) {
            // each bought tiered upgrade multiplies this building's CPS
            mult *= 1.0 + static_cast<f64>(u->tier) * 0.05;
        }
    }
    return mult;
}

f64 Game::magicCpS(const std::string& /*name*/) {
    return 1.0; // magic (grimoire) not implemented yet
}

f64 Game::auraMult(const std::string& /*name*/) {
    return 0.0; // dragon auras not implemented yet
}

f64 Game::GetHeavenlyMultiplier() {
    // Mirrors JS Game.GetHeavenlyMultiplier. Cumulative contributions from the
    // five heavenly multiplier unlocks, plus Lucky digit/number/payout, the
    // Dragon God aura and the Creation god. Returns the fraction of the
    // prestige level's potential that is unlocked (0..1).
    f64 heavenlyMult = 0.0;
    if (has("Heavenly chip secret")) heavenlyMult += 0.05;
    if (has("Heavenly cookie stand")) heavenlyMult += 0.20;
    if (has("Heavenly bakery")) heavenlyMult += 0.25;
    if (has("Heavenly confectionery")) heavenlyMult += 0.25;
    if (has("Heavenly key")) heavenlyMult += 0.25;
    heavenlyMult *= 1.0 + auraMult("Dragon God") * 0.05;
    if (has("Lucky digit")) heavenlyMult *= 1.01;
    if (has("Lucky number")) heavenlyMult *= 1.01;
    if (has("Lucky payout")) heavenlyMult *= 1.01;
    if (getGodLevel("creation") >= 1) {
        i32 godLvl = getGodLevel("creation");
        if (godLvl == 1) heavenlyMult *= 0.7;
        else if (godLvl == 2) heavenlyMult *= 0.8;
        else if (godLvl == 3) heavenlyMult *= 0.9;
    }
    return heavenlyMult;
}

// ---------------------------------------------------------------------------
// Prestige
// ---------------------------------------------------------------------------

void Game::EarnHeavenlyChips(f64 cookiesForfeited) {
    // Recalculate prestige from lifetime cookies forfeited (cookiesReset + the
    // cookies forfeited this ascension). Award one heavenly chip per newly
    // earned prestige level. Never awards duplicate chips; a zero-prestige
    // ascension (cookiesReset + cookiesForfeited < 1e12) yields nothing.
    f64 total = cookiesReset + cookiesForfeited;
    i32 newPrestige = static_cast<i32>(std::floor(std::cbrt(total / 1000000000000.0)));
    if (newPrestige > static_cast<i32>(prestige)) {
        i32 gained = newPrestige - static_cast<i32>(prestige);
        heavenlyChips += static_cast<f64>(gained);
        prestige = static_cast<f64>(newPrestige);
    }
    // Clamp at zero (defensive; cbrt of a non-negative total is non-negative).
    if (prestige < 0.0) prestige = 0.0;
    if (heavenlyChips < 0.0) heavenlyChips = 0.0;
}

// ---------------------------------------------------------------------------
// Reset (hard/soft)
// ---------------------------------------------------------------------------

void Game::Reset(bool hard) {
    // Mirrors JS Game.Reset(hard). Preserves permanent progression (achievements,
    // prestige, heavenly chips, heavenly upgrades, permanent slots, sugar lumps,
    // building levels) and resets run-specific state. Every field is assigned
    // directly so a failure part-way through cannot leave the game in a
    // partially corrupted state — each assignment is independent.

    // Track cookies forfeited this run (used by EarnHeavenlyChips before this
    // call; cookiesReset is updated here for the running total).
    cookiesReset += cookiesEarned;

    // --- reset run-specific state ---
    cookies = 0.0;
    cookiesEarned = 0.0;
    cookieClicks = 0;
    goldenClicksLocal = 0;
    handmadeCookies = 0;
    if (hard) {
        bgType = 0;
        milkType = 0;
        chimeType = 0;
        vault.clear();
    }
    pledges = 0;
    pledgeT = 0.0;
    elderWrath = 0.0;
    nextResearch = 0;
    researchT = 0.0;
    seasonT = 0.0;
    seasonUses = 0;
    season = baseSeason;

    // Timers restart for the new run.
    startDate = static_cast<f64>(now_ms());
    lastDate = startDate;

    cookiesSucked = 0.0;
    wrinklersPopped = 0;
    // Reset wrinklers to absent (phase 0).
    for (auto& w : wrinklers) { w.phase = 0; w.close = 0; w.sucked = 0.0; }

    santaLevel = 0;
    reindeerClicked = 0;

    dragonLevel = 0;
    dragonAura = 0;
    dragonAura2 = 0;

    fortuneGC = 0;
    fortuneCPS = 0;

    // Successful ascension increments resets only when prestige was gained
    // (mirrors JS). Zero-prestige ascensions still increment `ascensions`
    // (done in reincarnate()).
    if (static_cast<i32>(prestige) > 0) {
        resets++;
    }

    // --- buildings: reset amounts, not levels ---
    buildingsOwned = 0;
    for (auto& b : objects) {
        if (!b) continue;
        b->amount = 0;
        b->bought = 0;
        b->free = 0;
        b->totalCookies = 0.0;
        b->switchMinigame(false);
        if (hard) b->muted = 0;
        b->refresh();
    }

    // --- upgrades: reset non-prestige, non-lasting upgrades ---
    for (auto* u : upgradesById) {
        if (!u) continue;
        if (hard || u->pool != "prestige") u->bought = false;
        if (hard) u->unlocked = false;
    }

    // Rebuild counters.
    buildingsOwned = 0;
    upgradesOwned = 0;
    for (auto& b : objects) buildingsOwned += b->amount;

    // --- non-hard: preserve permanent upgrades and starter kits ---
    if (!hard) {
        for (i32 i = 0; i < 5; ++i) {
            if (permanentUpgrades[i] != -1 &&
                permanentUpgrades[i] < static_cast<i32>(upgradesById.size()) &&
                upgradesById[permanentUpgrades[i]]) {
                // Permanent upgrade effects are already active via bought flag;
                // ensure they stay unlocked.
                upgradesById[permanentUpgrades[i]]->unlocked = true;
            }
        }
        if (has("Starter kit")) {
            if (auto* cur = getBuilding("Cursor")) cur->free = 10;
        }
        if (has("Starter kitchen")) {
            if (auto* gm = getBuilding("Grandma")) gm->free = 5;
        }
    }

    // --- minigames: reset run state, keep unlocks/levels ---
    for (auto& b : objects) {
        if (!b) continue;
        if (b->minigameLoaded && b->minigame) {
            b->minigame->load(""); // reset run state, keep seed unlocks
        }
    }
    // Standalone minigames (e.g. Dungeons).
    if (auto it = minigames.find(-1); it != minigames.end() && it->second) {
        it->second->load("");
    }

    buyBulk = 1;
    buyMode = 1;

    recalculateGains_ = true;

    // Unlock the heavenly upgrade chain when prestige > 0.
    unlockHeavenlyChain();
    unlockAvailablePrestigeUpgrades();
}

void Game::unlockHeavenlyChain() {
    // Mirrors the JS unlock block in Game.Logic:
    //   if (Game.prestige > 0 && Game.ascensionMode != 1) {
    //     Game.Unlock('Heavenly chip secret');
    //     if (Game.Has('Heavenly chip secret')) Game.Unlock('Heavenly cookie stand');
    //     ...
    //   }
    if (static_cast<i32>(prestige) <= 0) return;

    auto unlockByName = [this](const std::string& name) {
        for (auto& uptr : upgrades) {
            auto* u = uptr.get();
            if (u && u->name == name) { u->unlocked = true; return; }
        }
    };

    unlockByName("Heavenly chip secret");
    if (has("Heavenly chip secret")) unlockByName("Heavenly cookie stand");
    if (has("Heavenly cookie stand")) unlockByName("Heavenly bakery");
    if (has("Heavenly bakery")) unlockByName("Heavenly confectionery");
    if (has("Heavenly confectionery")) unlockByName("Heavenly key");
}

void Game::unlockAvailablePrestigeUpgrades() {
    // General unlock for prestige-pool upgrades: unlock any prestige upgrade
    // whose parents are all bought and whose showIf condition (if any) passes.
    // Called whenever prestige or ownership changes.
    if (static_cast<i32>(prestige) <= 0) return;
    for (auto& uptr : upgrades) {
        auto* u = uptr.get();
        if (!u || u->pool != "prestige" || u->unlocked) continue;
        // ShowIf visibility condition.
        if (u->showIf && !u->showIf()) continue;
        // All parents must be bought.
        bool allParentsBought = true;
        for (const auto& parentName : u->parents) {
            if (!has(parentName)) { allParentsBought = false; break; }
        }
        if (allParentsBought) u->unlocked = true;
    }
}

// ---------------------------------------------------------------------------
// Ascension / reincarnation lifecycle
// ---------------------------------------------------------------------------

void Game::reincarnate() {
    // Full ascend: turn this run's cookies into prestige/chips, then reset the
    // run while preserving permanent progression. Safe to call repeatedly; each
    // step fully commits before the next begins, so a failure cannot leave
    // partially corrupted state.

    // 1. Earn heavenly chips for this run's cookies forfeited.
    EarnHeavenlyChips(cookiesEarned);

    // 2. Count the ascension (separate from prestige level). A successful
    //    reincarnation always counts, even a zero-prestige ascension.
    ++ascensions;
    if (static_cast<i32>(prestige) > 0 && static_cast<i32>(prestige) > 0) {
        // Mirror JS: resets only increment when prestige was actually gained.
        // (gainedPrestige is tracked via EarnHeavenlyChips above.)
        resets = static_cast<i32>(std::max(static_cast<i32>(resets), 0));
    }

    // 3. Reset run-specific state, preserving permanent progression.
    Reset(false);

    // 4. Re-apply permanent upgrade slot selections on the fresh run.
    applyPermanentUpgrades();

    // 5. Recalculate gains so the new run's CpS reflects prestige + slots.
    recalculateGains_ = true;
    calculateGains();
}

bool Game::applyPermanentUpgrades() {
    // Re-apply all permanent upgrade slot selections on a new run, without
    // duplicate application. Each slot holds an upgrade id; if that upgrade is
    // eligible (bought, not a prestige/noPerm upgrade), its effect is already
    // active via the normal has()/bought checks — permanent slots in this port
    // preserve the *selection* across ascensions so the same ordinary upgrade
    // is re-applied automatically (its bought flag survives via save/load).
    // Returns true if all slots are valid.
    bool ok = true;
    for (i32 i = 0; i < 5; ++i) {
        i32 uid = permanentUpgrades[i];
        if (uid < 0) continue; // empty slot
        if (uid >= static_cast<i32>(upgradesById.size()) || !upgradesById[uid]) {
            // Dangling reference: clear the slot rather than corrupt state.
            permanentUpgrades[i] = -1;
            ok = false;
            continue;
        }
        Upgrade* u = upgradesById[uid];
        // Only ordinary (non-prestige, non-noPerm) upgrades can occupy a slot.
        if (u->pool == "prestige" || u->noPerm) {
            permanentUpgrades[i] = -1;
            ok = false;
            continue;
        }
    }
    return ok;
}

bool Game::assignPermanentSlot(i32 slot, i32 upgradeId) {
    // Select an eligible ordinary upgrade for a permanent slot. Eligible:
    // not already in another slot, not a prestige/noPerm upgrade, bought.
    if (slot < 0 || slot >= 5) return false;
    if (upgradeId < 0 || upgradeId >= static_cast<i32>(upgradesById.size()))
        return false;
    Upgrade* u = upgradesById[upgradeId];
    if (!u) return false;
    if (!u->bought) return false;            // must be bought to be slotted
    if (u->pool == "prestige" || u->noPerm) return false;
    // No duplicate slots.
    for (i32 i = 0; i < 5; ++i) {
        if (permanentUpgrades[i] == upgradeId) return false;
    }
    // Clear any existing slot holding this upgrade (shouldn't happen, but be safe).
    for (i32 i = 0; i < 5; ++i) {
        if (permanentUpgrades[i] == upgradeId) permanentUpgrades[i] = -1;
    }
    permanentUpgrades[slot] = upgradeId;
    return true;
}

void Game::updateShimmers() { /* TODO */ }
void Game::updateBuffs() { /* TODO */ }

std::string Game::writeSave(i32 type) { return SaveLoad::writeSave(*this, type); }
bool Game::loadSave(const std::string& data) { return SaveLoad::loadSave(*this, data); }
bool Game::importSaveCode(const std::string& code) { return loadSave(code); }
