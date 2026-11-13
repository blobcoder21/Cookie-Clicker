#pragma once

#include "Types.h"
#include "Building.h"
#include "Upgrade.h"
#include "Achievement.h"
#include "Buff.h"
#include "Shimmer.h"
#include "Particle.h"
#include "Minigame.h"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

// ---------------------------------------------------------------------------
// Game singleton – mirrors the JS `Game` object. All persistent game state
// lives here so save/load can serialize it in one place.
// ---------------------------------------------------------------------------
class Game {
public:
    // --- versioning --------------------------------------------------------
    static constexpr f64 VERSION = 2.058;   // port target: Cookie Clicker 2.058
    static constexpr i32 BETA = 1;

    // --- runtime flags -----------------------------------------------------
    bool ready = false;
    bool onAscend = false;
    f64 ascendTimer = 0.0;
    f64 ascendDuration = 0.0;
    bool sesame = false;           // debug mode
    bool debugTimersOn = false;
    bool toSave = false;           // autosave trigger
    i32 volume = 100;
    i32 fps = GameConst::FPS;
    i32 loopT = 0;
    i32 drawT = 0;

    // --- time --------------------------------------------------------------
    f64 time = 0.0;                // in-game "frame time" (Date.now analogue)
    f64 lastDate = 0.0;
    f64 startDate = 0.0;
    f64 fullDate = 0.0;
    std::string seed;              // seeded randomness
    std::string appearance;        // YouCustomizer gene string (save section 2, field 5)

    // --- bakery ------------------------------------------------------------
    std::string bakeryName;

    // --- currency ----------------------------------------------------------
    f64 cookies = 0.0;
    f64 cookiesEarned = 0.0;
    f64 cookiesPs = 0.0;
    f64 cookiesReset = 0.0;
    f64 handmadeCookies = 0.0;
    f64 cookiesSucked = 0.0;
    f64 cpsSucked = 0.0;
    f64 computedMouseCps = 1.0;
    i32 cookieClicks = 0;
    i32 goldenClicks = 0;
    i32 goldenClicksLocal = 0;
    i32 missedGoldenClicks = 0;

    // --- buildings ---------------------------------------------------------
    std::vector<std::unique_ptr<Building>> objects;   // Game.Objects
    std::vector<Building*> objectsById;               // Game.ObjectsById
    i32 objectsN = 0;
    i32 buildingsOwned = 0;
    f64 priceIncrease = GameConst::PRICE_INCREASE;
    i32 buyBulk = 1;
    i32 buyMode = 1;          // 1 = buy, -1 = sell

    // --- upgrades / achievements -------------------------------------------
    std::vector<std::unique_ptr<Upgrade>> upgrades;
    std::vector<Upgrade*> upgradesById;
    std::vector<Upgrade*> upgradesInStore;
    std::vector<std::unique_ptr<Achievement>> achievements;
    std::vector<Achievement*> achievementsById;
    i32 achievementsOwned = 0;
    i32 upgradesOwned = 0;

    // --- prestige / lumps --------------------------------------------------
    f64 prestige = 0.0;
    f64 heavenlyChips = 0.0;
    f64 heavenlyChipsSpent = 0.0;
    f64 heavenlyCookies = 0.0;
    i32 resets = 0;                       // JS field 14 ("ascensions" count)
    i32 ascensionMode = 0;                // JS field 29
    f64 cookiesPsRawHighest = 0.0;        // JS field 51
    i32 volumeMusic = 50;                 // JS field 52
    i32 cookiesSent = 0;                  // JS field 53
    i32 cookiesReceived = 0;              // JS field 54
    f64 lumps = 0.0;
    f64 lumpsTotal = 0.0;
    f64 lumpT = 0.0;
    f64 lumpRefill = 0.0;
    i32 lumpCurrentType = 0;

    // --- misc --------------------------------------------------------------
    f64 elderWrath = 0.0;
    i32 pledges = 0;
    f64 pledgeT = 0.0;
    i32 nextResearch = 0;
    f64 researchT = 0.0;
    i32 bgType = 0;
    i32 milkType = 0;

    // --- wrinklers ---------------------------------------------------------
    // Mirrors JS Game.wrinklers (12 fixed slots). phase: 0=absent, 1=closing,
    // 2=sucking. sucked = cookies stored inside. cpsSucked is derived from the
    // number of sucking wrinklers each frame in calculateGains().
    struct Wrinkler {
        i32 id = 0;
        i32 phase = 0;
        i32 close = 0;
        f64 sucked = 0.0;
        f64 hp = 0.0;
        i32 selected = 0;
        i32 type = 0; // 0 normal, 1 shiny
    };
    std::vector<Wrinkler> wrinklers;   // fixed 12 slots, created in Game::load()
    i32 wrinklersPopped = 0;
    f64 wrinklerHP = 2.1;              // JS Game.wrinklerHP

    // --- vault -------------------------------------------------------------
    // Upgrade ids that have been vaulted (shift-clicked), mirroring JS
    // Game.vault (comma-joined list of ids in the save).
    std::vector<i32> vault;
    std::string season;            // current season name, "" = none
    f64 seasonT = 0.0;
    i32 seasonUses = 0;
    std::string baseSeason;
    i32 santaLevel = 0;
    i32 reindeerClicked = 0;
    i32 dragonLevel = 0;
    i32 dragonAura = 0;
    i32 dragonAura2 = 0;
    i32 chimeType = 0;
    i32 heralds = 0;
    i32 fortuneGC = 0;
    i32 fortuneCPS = 0;

    // --- permanent upgrades (5 slots; -1 = empty slot) -----------------------
    // Mirrors JS Game.permanentUpgrades ([-1,-1,-1,-1,-1]). Preserved across
    // ascensions. Each slot holds the id of an ordinary upgrade whose effect is
    // re-applied at the start of every new run.
    i32 permanentUpgrades[5] = {-1, -1, -1, -1, -1};

    // --- ascension statistics ------------------------------------------------
    // Successful ascension count (separate from prestige level). Incremented
    // only when an ascension actually grants at least one prestige level.
    // PORT-ONLY: v2.058's real save format stores only `resets` (section 4,
    // field 14). This field is NOT serialized into the real save string so it
    // cannot collide with the real field 51 (cookiesPsRawHighest); it lives as
    // port-only sidecar state.
    i32 ascensions = 0;

    // --- preferences -------------------------------------------------------
    struct Prefs {
        bool particles = true;
        bool numbers = true;
        bool autosave = true;
        bool autoupdate = true;
        bool milk = true;
        bool fancy = true;
        bool warn = false;
        bool cursors = false;
        bool focus = false;
        bool format = false;
        bool notifs = true;
        bool wobbly = false;
        bool monospace = false;
        bool filters = true;
        bool cookiesound = true;
        bool crates = false;
        bool showBackupWarning = true;
        bool extraButtons = true;
        bool askLumps = false;
        bool customGrandmas = true;
        bool timeout = false;
        bool cloudSave = true;
        bool bgMusic = true;
        bool notScary = false;
        bool fullscreen = false;
        bool screenreader = false;
        bool discordPresence = true;
    } prefs;

    // --- runtime collections -----------------------------------------------
    std::vector<std::unique_ptr<Buff>> buffs;
    std::map<std::string, Buff*> buffMap;   // by buff type id
    std::vector<std::unique_ptr<Shimmer>> shimmers;
    std::vector<std::unique_ptr<Particle>> particles;

    // --- buff type registry ------------------------------------------------
    // Buff archetypes keyed by numeric id (mirrors JS Game.buffTypes).
    // Used by save/load to resolve a saved buff type id back to a BuffType.
    std::map<i32, std::shared_ptr<BuffType>> buffTypes;

    // --- minigame registry -------------------------------------------------
    // one minigame per building that supports it (Farm, Wizard tower, ...)
    std::map<i32, std::unique_ptr<Minigame>> minigames; // keyed by building id

    // --- hooks (mod/custom callbacks) --------------------------------------
    using CustomCb = std::function<void()>;
    std::vector<CustomCb> customSave;
    std::vector<CustomCb> customDraw;
    // mod save data (keyed by mod id), mirrors JS Game.modSaveData
    std::map<std::string, std::string> modSaveData;
    // mouse Cps callbacks return an additive bonus (mirrors JS customMouseCps)
    std::vector<std::function<f64()>> customMouseCps;
    // mouse Cps multiplier callbacks return a factor (mirrors JS customMouseCpsMult)
    std::vector<std::function<f64()>> customMouseCpsMult;
    std::vector<CustomCb> customCookieClicks;

    // tooltip/effect helper (mirrors JS Game.eff)
    f64 eff(const std::string& name, f64 def = 1.0);

    // --- construction/destruction ------------------------------------------
    Game();
    ~Game();

    // --- lifecycle (mirrors Game.Launch / Game.Load) ----------------------
    void launch();
    void load();                   // asset load + init data
    void readyToLaunch();          // end of init, "game ready"

    // --- main loop ---------------------------------------------------------
    void loop();                   // one frame: logic + draw
    void logic(f64 dt);            // Game.Logic

    // --- ascension ---------------------------------------------------------
    void Reset(bool hard);          // hard/soft reset; preserves permanent progression

    // --- economy -----------------------------------------------------------
    void earn(f64 howmuch);
    void spend(f64 howmuch);
    void dissolve(f64 howmuch);
    f64 mouseCps();
    void clickCookie(i32 amount = 0);
    void calculateGains();         // Game.CalculateGains (recompute cookiesPs)
    f64 ComputeCps(f64 baseCps, i32 clickUpgrades, f64 add);  // JS ComputeCps
    f64 GetTieredCpsMult(Building* me);                        // tiered CPS mult
    f64 magicCpS(const std::string& name);                     // grimoire magic mult
    f64 auraMult(const std::string& name);                     // dragon aura mult
    void buyBuilding(Building* b, i32 amount = 1);              // purchase building
    void sellBuilding(Building* b, i32 amount = 1);             // sell building
    bool buyUpgrade(const std::string& name);                   // purchase upgrade (core; UI deferred)

    // --- building/upgrade helpers ------------------------------------------
    bool has(const std::string& name) const;           // Game.Has (upgrade)
    bool hasAchiev(const std::string& name) const;
    i32 getGodLevel(const std::string& name) const;    // 0 if no pantheon god
    void unlock(const std::string& name);
    Building* getBuilding(const std::string& name);

    // --- minigame accessors (core logic; UI deferred) -----------------------
    f64 minigameCookieBonus() const;   // combined CpS mult from all minigames
    f64 minigameClickBonus() const;    // combined click mult from all minigames
    Minigame* getMinigame(i32 buildingId);
    bool hasMinigame(i32 buildingId) const;

    // --- save / load (exact JS-compatible format) --------------------------
    std::string writeSave(i32 type = 0);   // type: 0=store, 1=string, 2=uncompressed, 3=commented
    bool loadSave(const std::string& data);
    bool importSaveCode(const std::string& code);

    // --- misc --------------------------------------------------------------
    std::string getBakeryName() const;
    void recalculateGainsFlag() { recalculateGains_ = true; }

    // --- settings / prefs (runtime flags, decoded from savegame) -----------
    bool getPref(const std::string& name) const;
    void setPref(const std::string& name, bool value);
    bool togglePref(const std::string& name);

    // --- read-only access for the UI layer (animation / feedback state) ----
    f64 bigCookieSizeValue() const { return bigCookieSize; }
    i32 flashBuildingIndexValue() const { return flashBuildingIndex; }
    i32 flashFramesValue() const { return flashFrames; }

    // --- news ticker --------------------------------------------------------
    std::string tickerMessage() const;        // current ticker text for UI
    bool tickerIsFortune() const;             // whether current message is a clickable fortune
    void clickTickerFortune();                // UI calls this when clicking a fortune
    i32 fortunesClicked() const { return fortunesClicked_; }

    // --- heavenly upgrades --------------------------------------------------
    bool buyHeavenlyUpgrade(const std::string& name);   // purchase with heavenly chips
    bool hasFortuneCookies() const;             // whether Fortune cookies (ID 643) is bought

    // Prestige calculation: floor(cuberoot(lifetime cookies baked / 1e12)).
    // Award one heavenly chip per newly earned prestige level.
    void EarnHeavenlyChips(f64 cookiesForfeited);
    f64 GetHeavenlyMultiplier();                // prestige CPS mult (heavenly upgrades etc.)

    // Ascension / reincarnation lifecycle. Preserves permanent progression and
    // resets run-specific state. Safe to call repeatedly; never leaves partial
    // state behind.
    void reincarnate();                          // full ascend: earn chips, then reset run

    // Permanent upgrade slot management.
    bool assignPermanentSlot(i32 slot, i32 upgradeId);  // select an eligible ordinary upgrade
    i32 permanentUpgradeId(i32 slot) const { return permanentUpgrades[slot]; }
    bool applyPermanentUpgrades();               // re-apply all slot selections on a new run

    // --- heavenly upgrade chain unlocking ---
    // Unlocks Legacy when prestige > 0, then chains each multiplier upgrade to
    // the previous one once bought. Mirrors JS Game.Logic unlock block.
    void unlockHeavenlyChain();

    // General unlock for prestige-pool upgrades whose parents are all bought.
    void unlockAvailablePrestigeUpgrades();

private:
    bool recalculateGains_ = true;
    std::map<std::string, f64> effs_;   // Game.effs backing store
    f64 heavenlyPower = 1.0;
    f64 globalCpsMult = 1.0;
    f64 unbuffedCps = 0.0;
    f64 lastClick = 0.0;
    bool canClick = true;
    i32 autoclickerDetected = 0;
    i32 bigCookieState = 0;
    f64 bigCookieSize = 0.0;
    f64 bigCookieSizeD = 0.0;
    f64 bigCookieSizeT = 1.0;

    // GUI feedback (set by buyBuilding; drawn by Render, not part of sim)
    i32 flashBuildingIndex = -1;
    i32 flashFrames = 0;

    i32 cookieClickSound = 1;
    f64 milkProgress = 0.0;
    f64 milkH = 0.0;
    f64 milkHd = 0.0;

    // news ticker state (core logic, no UI dependencies)
    std::vector<std::string> tickerMessages_;   // pool of generated messages
    i32 tickerIndex_ = 0;                       // current message in pool
    f64 tickerTimer_ = 0.0;                     // seconds until next message
    bool tickerIsFortune_ = false;              // current message is a clickable fortune
    std::string tickerFortuneMsg_;              // fortune message override
    i32 fortunesClicked_ = 0;                   // total fortunes clicked
    f64 tickerInterval_ = 15.0;                 // seconds between ticker messages

    void updateSpecial();
    void updateGrandmapocalypse();
    void particlesUpdate();
    void textParticlesUpdate();
    void updateShimmers();
    void updateBuffs();

    // news ticker internals
    void advanceTicker();
};

extern Game* g_game; // accessor, like the global `Game` in JS
