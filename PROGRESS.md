# Progress Log — Cookie Clicker C++ Port

## Session Summary: Ascension and Prestige Core (v2.021)

### Completed
- **Prestige system**: `prestige = floor(cuberoot(cookiesReset / 1e12))`; one heavenly chip per newly earned prestige level; tracks `cookiesReset`, `prestige`, `heavenlyChips`, `heavenlyChipsSpent` independently; no duplicate rewards; zero-prestige ascensions handled.
- **Ascension lifecycle**: `Game::reincarnate()` → `EarnHeavenlyChips()` → `Reset(false)` → `applyPermanentUpgrades()`. Preserves achievements, sugar lumps, building levels, heavenly upgrades, Garden seed unlocks; resets run-specific state (cookies, buildings amounts, non-prestige upgrades, wrinklers, timers). No partial corruption possible — each field assigned independently.
- **Heavenly upgrade dataset**: complete pool=`prestige` dataset from v2.021 `main.js` presets — all upgrades with IDs, costs, parents (prerequisites), `showIf` visibility conditions, `noPerm` flags. Five heavenly multiplier unlocks (Heavenly chip secret +5%, cookie stand +20%, bakery +50%, confectionery +75%, key +100%) with correct cumulative contributions. Price rounding via `ceil`; prestige pool exempt from ordinary cookie discounts.
- **Permanent upgrade slots**: 5 slots (`-1` = empty). `assignPermanentSlot()` selects eligible ordinary upgrades (not prestige/noPerm, must be bought, no duplicates). Selections preserved through ascensions via save/load. `applyPermanentUpgrades()` validates slots on new runs.
- **Prestige CPS**: `GetHeavenlyMultiplier()` implements all 5 multipliers + Lucky digit/number/payout + Dragon God aura + Creation god. Integrated into `calculateGains()` as `prestige * 0.01 * heavenlyPower * GetHeavenlyMultiplier()`. PseudoCookie upgrades (Heavenly cookies +10%, Wrinkly cookies +10%, Sugar crystal cookies +5% + per-level) applied as CPS multipliers.
- **Persistence**: SaveLoad extended with `ascensions` field (section 4, field 51, append-only for backwards compatibility). Prestige recomputed from `cookiesReset` on load (mirrors JS). Full save/load round-trip verified. Old saves without `ascensions` load correctly (defaults to 0).
- **Statistics**: `ascensions` tracked separately from `prestige` and `resets`. Zero-prestige ascensions increment `ascensions` only; prestige-gaining ascensions increment both.
- **Integration**: wired into `Game::logic()` (periodic unlock checks), `calculateGains()` (CPS), `SaveLoad`, minigame reset on ascension. Core remains UI/platform-independent.

### Files Modified
- `cpp/Game.h` — added `ascensions`, `Reset()`, `reincarnate()`, `EarnHeavenlyChips()`, `GetHeavenlyMultiplier()`, `assignPermanentSlot()`, `applyPermanentUpgrades()`, `unlockHeavenlyChain()`, `unlockAvailablePrestigeUpgrades()`; changed `permanentUpgrades` init to `-1`
- `cpp/Game.cpp` — full implementations of all above; heavenly upgrade dataset (~80 pool=`prestige` upgrades); pseudoCookie CPS effects; unlock chain logic
- `cpp/Upgrade.h` — added `pool`, `parents`, `showIf`, `noPerm` fields
- `cpp/SaveLoad.cpp` — added `ascensions` field (write + load), prestige recomputation on load
- `cpp/SaveLoad.h` — documented `ascensions` field in save format

### Build & Test
- `cmake --build build -j` succeeds
- `cookie_clicker_logic_check` passes (core compiles with logic headers only)
- `timeout 3 ./build/cookie_clicker` exits 124 (expected, no crash)
- 14 deterministic platform-free tests pass: prestige thresholds, multiple levels gained, chip accounting, heavenly upgrade purchases, prerequisites, visibility (showIf), CPS multipliers, repeated ascension, permanent slots, reset preservation, minigame state, save/load round-trip, save/load backwards compatibility, ascension statistics

### Porting Target
- **Target version: v2.021** (established from `main.js` `VERSION=2.021`, `Game.h` `VERSION = 2.021`, PORTING_PLAN §1). HANDOFF.md "v2.058" references are UI layout screenshots only, not game logic. teemo.dev cross-check not applicable (target ≠ 2.058; no external access).

### Active
- None

### Blocked
- Sounds: SDL2_mixer not installed (`Audio.cpp` no-op stub)
- Many heavenly upgrade effects (angels, kittens, synergies, seasons, sugar lumps maturation) are registered as data but their specific mechanics are stubbed — consistent with existing stubbed subsystems
- UI for ascend tree, heavenly upgrade purchasing, permanent slot selection (deferred per non-goals)
