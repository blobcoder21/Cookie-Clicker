# Handoff

## Next task (one scoped item)
Implement `Game::calculateGains()` fully so purchasing a building raises `cookiesPs` and the cookie count grows over time. Scope: building CpS only (no upgrades/buffs/heavenly power yet). Wire it into `Game::logic()` so `dt` accumulates cookies each frame.

## Exact build/test commands
```
cd /home/superuser/Cookie-Clicker
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
timeout 3 ./build/cookie_clicker   # runs fixed-timestep loop, exits 0
```
Syntax check (no SDL needed):
```
cd cpp
g++ -std=c++17 -fsyntax-only -Wall -Wextra -I. *.cpp
```

## How to verify it's done (a check that passes, not "compiles")
After implementing calculateGains: start the game, buy one Cursor via the (still-stubbed) UI or directly in code, then confirm `cookiesPs > 0` and `cookies` increases by `cookiesPs * elapsed_seconds` after several frames. A programmatic check: after `N` loop iterations with one Cursor owned, `Game.cookies >= N * (1.0 / 30.0) * Cursor.cps` (within floating tolerance). "Compiles" is NOT sufficient verification.

## Known broken, stubbed, or unverified code (honest list)
UNVERIFIED — nothing in the gameplay path has been run end-to-end; only the empty loop was executed.

Stubs (no-op / placeholder):
- `cpp/Render.cpp` — Render::init/newFrame/render/shutdown all no-op; no window drawing yet
- `cpp/Audio.cpp` — Audio::init/playSound/setVolume all no-op; no sound playback
- `cpp/Assets.cpp` — Assets::init no-op, hasImage always returns false
- `cpp/Game.cpp::draw()` — empty; nothing is drawn
- `cpp/Game.cpp::logic()` — `dt` unused; only calls per-frame building hooks + placeholder calculateGains + updateSpecial/updateGrandmapocalypse/particlesUpdate (all empty)
- `cpp/Game.cpp::mouseCps()` — partial: includes some "fingers" upgrades but click calc is a placeholder (`mult * add`), missing Cookie egg, eff click, buffs, auras, gods
- `cpp/Game.cpp::calculateGains()` — placeholder: only sums `amount * cps` across buildings, no upgrades/buffs/mults/heavenly power
- `cpp/SaveLoad.cpp::loadSave()` — section 5 (buildings) section is iterated but amounts are NOT applied to buildings; wrinklers not modelled; vault not parsed; buffs (section 8) not parsed
- `cpp/SaveLoad.cpp::writeSave()` — building minigame save field is empty comma; wrinklers write hardcoded 0; vault writes "0"
- `cpp/Minigame.cpp` and Garden/Grimoire/Pantheon/Dungeons — all stubs (init sets loaded=true, nothing else)
- `cpp/DungeonGen.cpp` — returns "Mysterious dungeon" only
- `cpp/Shimmer.cpp`, `Particle.cpp` — basic structs only, not wired into game
- `cpp/Building.cpp` — `modifyBuildingPrice` hook not implemented; `getSumPrice` correct but unused

Unverified:
- Base64 encode/decode round-trip with special characters
- `escape()`/`unescape()` pipeline for non-ASCII bakery names/seeds
- Save/load compatibility with browser-made saves (C++ → browser and browser → C++)
- PRNG parity with David Bau seedrandom (std::mt19937 used instead, UNVERIFIED for dungeons/mutations/lumps)
- `double` cookie math matching JS exactly over long runs (UNVERIFIED)

## Decisions already made (do not redo)
- All cookie math uses C++ `double` to match JS IEEE-754 behavior
- Main loop is fixed-timestep at ~30 FPS (`GameConst::FPS`), matching the JS `setTimeout` loop
- Rendering target: SDL2 + OpenGL, Dear ImGui optional (not present on this box; stub used)
- SDL2_mixer optional (not present; Audio is stub)
- Save format: exact JS-compatible pipe/semicolon text → UTF-8 → Base64 → `!END!` → escape; `pack3` = identity (raw `0/1` bitstrings)
- Save persistence: file-based (`<bakeryName>Bakery.txt`) instead of localStorage/cookies
- PRNG: `std::mt19937` seeded from time; David Bau seedrandom NOT yet ported
- Offline progress uses `std::chrono::system_clock` wall-clock delta (JS `Date.now` equivalent)
- `build/` directory is generated output; do not commit
