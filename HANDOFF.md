# Handoff

## Next task (one scoped item)
**DONE — Save/Load building persistence (sections 5, 8, wrinklers, vault).**
See Verification status below. A C++ save now round-trips byte-identical through the escape/base64 pipeline, a browser-made save (JS field order) loads into the C++ build, and the game resumes with correct buildings/CpS.

**DONE — Early native PC GUI (layout matches index.html / style.css).**
- Native SDL2 desktop window (900x600, resizable, OpenGL).
- Left panel (#sectionLeft, 30% width): cookie count (#cookies, top 10%), bakery name, CPS, and the big cookie (#cookieAnchor, centered at top 40%) rendered from `img/perfectCookie.png`, clickable. Wobble clamped between 90% and 120% (click pops to 120% with spring bounce).
- Right panel (#sectionRight / #store, rightmost ~318px): Store with `.product` tiles (300x64, `storeTile.png` strips at natural width centered in each cell), each with building icon (64x64, cursor uses top-left quadrant of sprite sheet), name, green price when affordable / red when not, `"???"` when locked.
- No separate BUY button: clicking anywhere in a store row buys one building. Purchase feedback: bought building's cell flashes green ~0.4s; clicking the cookie pops it larger.
- Fonts: switched from Tahoma to **Merriweather** (`Merriweather_24pt-Bold.ttf` from cwd `fonts/`, with embedded Tahoma as fallback). All store text now uses Merriweather.
- Hard requirement respected: `cpp/` core (`Game`, `Building`, `SaveLoad`, economy…) knows nothing of SDL/OpenGL/FreeType. All rendering goes through the `Render` platform layer (`cpp/Render.cpp`), which decodes PNGs with libpng and renders text with FreeType2. `Game::draw()` issues only `Render::*` calls. `main.cpp` (PC backend) owns SDL, hit-tests the cookie/buy buttons against areas registered during `draw()`, and calls `game.clickCookie()` / `game.buyBuilding()`.
- `main.cpp` now calls `game.load()` (previously missed, so buildings were never created). `Game::loop()` is logic-only; `draw()` is invoked once per rendered frame from `main.cpp`.
- Font fix: glyph cache is cleared when the FreeType face size changes (was cached by char only, so all text used the first scale's bitmaps — now cookie count, CPS, bakery name, and building names each render at their correct size).
- Asset fix: 23 GUI assets embedded (18 building icons, big cookie, bgBlue, storeTile, TTF font) in `cpp/EmbeddedAssets.*` and extracted to a temp dir at `Render::init`, so the executable runs from any working directory with no missing textures or fonts.

Next scoped item: continue per PORTING_PLAN.md milestones (upgrades panel, save/load file persistence, settings menu, polish). Resist mod support and elaborate rendering until this window is solid.

### Next architectural goal — Extract a UI layer

**Goal:** Split rendering/layout out of `Game` so the core is portable to DSi/3DS without remaking UI.

Rules:
1. `Game` has **no `draw()` function** and includes **no `Render` or `UI` headers**. Core compiles with only logic headers on the include path.
2. `UI` takes `const Game&` and `Render&`, owns **all** layout, drawing, and hit-testing.
3. Input goes `UI::on_pointer(x, y)` → returns a game action → main loop applies it to `Game`.
4. All layout numbers live in a `Layout` struct, **not** inline in draw code.
5. Behaviour and visuals must be **pixel-identical** to now — this is a pure refactor, no visual change.
6. Verify that the logic target compiles with only the logic headers on the include path.

## Exact build/test commands
```
cd /home/superuser/Cookie-Clicker
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
timeout 3 ./build/cookie_clicker   # runs fixed-timestep loop, opens a window; killed by timeout after 3s; exit 124 is expected
```
Requires (present on this box): SDL2, OpenGL, libpng, FreeType2. CMake finds all four (`find_package(PNG)`, `find_package(Freetype)`).

Asset handling: the GUI assets needed by the minimal GUI (18 building icons, big cookie, bgBlue, storeTile, TTF font) are embedded into the binary as byte arrays (`cpp/EmbeddedAssets.*`) and extracted to a temp dir at `Render::init`, so the executable is self-contained regardless of working directory. Run from anywhere: `./build/cookie_clicker` or `cd build && ./cookie_clicker`.

Syntax check (no SDL/png/ft needed for core):
```
cd cpp
g++ -std=c++17 -fsyntax-only -Wall -Wextra -I. *.cpp
```

## Commit & build workflow

At the end of every task:
1. Run the build (`cmake --build build -j`). If it fails to compile, stop and report it.
2. Run the task's checks (`timeout 3 ./build/cookie_clicker` → expect exit 124).
3. `git add` only the files you changed.
4. Commit with a terse message saying **what changed and why**. One logical change per commit — never mix refactors, features, and asset changes.
5. `git push`. Never force-push or rewrite history. Never commit a broken build.

## Verification status

**calculateGains — VERIFIED**
- Start the game, buy one Cursor directly in code (`game.cookies = 100; game.buyBuilding(cursor, 1)`)
- Confirmed `cookiesPs == 0.1` (> 0)
- Confirmed `cookies` increases by `cookiesPs * elapsed_seconds` (0.2 over 2s at 30 FPS, exact match)
- Programmatic check passed: after N loop iterations with one Cursor owned, `Game.cookies >= N * (1.0/30.0) * Cursor.cps` within floating tolerance

**Save/Load — VERIFIED** (this scoped item)
- Standalone harness (`/tmp/opencode/saveload_test.cpp`, compiled against `cpp/` sources): 0 failures
- `writeSave` → `encodeForStorage` → `decodeFromStorage` → `loadSave` round-trips byte-identical (also fixed a `base64Decode` padding bug that appended a stray `0xff` byte)
- Save format now matches JS `Game.WriteSave` field order exactly: section 2 ends `seed|` (was `seed;`, merging sections 2/3); section 4 fields 0–50 in JS order (cookiesSucked 16, wrinklersPopped 17, santaLevel 18, reindeerClicked 19, seasonT 20, seasonUses 21, season 22, wrinklers.amount 23, wrinklers.number 24, prestige 25 … dragonLevel 35, volume 39, wrinklers.shinies 40, wrinklers.amountShinies 41, lumps 42 … lumpCurrentType 46, vault 47 comma-joined, heralds 48, fortuneGC 49, fortuneCPS 50)
- Section 5 (buildings): write emits `amount,bought,floor(totalCookies),level,,muted;` per vanilla building in `objects` order; load applies amount, bought, totalCookies, level and muted to `Game.objects` and recomputes `buildingsOwned` + `recalculateGainsFlag`
- Section 8 (buffs): write emits `type.id,maxTime,time,arg1,arg2,arg3;` (times in frames × fps); load resolves `type.id` via a new `Game.buffTypes` registry (unknown ids get a placeholder so saves still round-trip), restores maxTime/time (frames → seconds) and args
- Wrinklers: `Wrinkler` model (12 slots) added; `cpsSucked` derived from sucking (phase==2) wrinklers; save computes amount/number/shinies/amountShinies (JS `SaveWrinklers` semantics); load restores phase/sucked/type (JS `LoadWrinklers` semantics, shinies first)
- Vault: `std::vector<i32> Game.vault`; save writes comma-joined ids, load parses them
- Doubles written with `std::setprecision(17)` so cookie counts round-trip exactly through both C++ and JS number parsing
- Synthetic JS-field-order ("browser") save loads correctly: cookies, building amounts/bought/totalCookies/level/muted, and `cookiesPs > 0` after load all verified

**Rendering fixes — VERIFIED this session:**
- Text diagonal tearing fixed in `Render::getGlyph()`: `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` + `GL_UNPACK_ROW_LENGTH` set to FreeType's `bm.pitch` before uploading glyph alpha bitmaps (padded rows no longer cause stripes).
- Big cookie white square fixed in `Render::drawTexture()`: added `glEnable(GL_TEXTURE_2D)` before binding (`drawText()` leaves texturing disabled). Corrected asset path `imperfectCookie.png` → `perfectCookie.png`; replaced embedded byte data with real `perfectCookie.png` (98,484 bytes); removed all `imperfectCookie` references.
- PNG alpha: `loadPNG()` no longer calls `png_set_add_alpha` after `png_set_tRNS_to_alpha` (was overwriting tRNS transparency with opaque white). `loadTexture` logs every success/failure instead of silently falling back to magenta.
- Background: `Game::draw()` draws `img/bgBlue.png` full-screen instead of a dark rect.
- Cookie count text scale reduced 2.0f → 1.5f (y -14 → -10) so it fits on screen.
- Build + run verified: `cmake --build build -j` (exit 0), `timeout 3 ./build/cookie_clicker` (exit 124, expected from timeout). Cookie renders as a proper cookie; text clean; blue background shows.

**Store panel — VERIFIED this session:**
- `bgBlue.png` path fixed (`"img/bgBlue.png"` → `"bgBlue.png"`); `bgBlue.png` embedded (433,960 bytes).
- Cursor icon now shows only the white index finger: `cursor.png` is a 2x2 sprite sheet (64x64 total). `drawTexture()` gained optional `srcX/srcY/srcW/srcH` UV parameters; cursor draws top-left 32x32 quadrant stretched to 64x64.
- BUY buttons removed: full-row click buys one building; hit-test → `game.buyBuilding()` works via existing `main.cpp` handler.
- `storeTile.png` (74,308 bytes) embedded; each row uses one 64px strip via UV coords at natural 300px width centered in cell.
- Building names (.product .content): scale 1.2f (~19px) with dark text shadow below — prominent white name when unlocked, grey `"???"` when locked. Price (.price): scale 0.8f (~13px), green when affordable / red when locked/unaffordable, with small `smallCookies.png` cookie icon drawn just before the price.
- Big faded owned amount (`.product .content .owned`): repositioned per Image 1 to bottom-right corner (`right:12px`, vertically centred in the cell) instead of top:6px, scale 1.5f (~24px), color `0x00000066`.
- Thin horizontal cell separator line (`0x4A4A60FF`) at bottom of each row → grid-of-cells look.
- Cookie wobble clamped 90%–120%; click pop target 1.2 + velocity 0.3 (was `+= 1.5`).
- Permanent unlock: `cookiesEarned >= basePrice` sets `b->locked = false` (save-compatible).
- Render additions: `Render::drawTextureTiled()` tiles a texture along one axis at natural size (never scaled), clips the last tile to the exact edge length, snaps to whole pixels; `Render::textureSize()` queries source-pixel dims. `resolveAssetPath()` now also falls back to cwd `img/<path>` so non-embedded dev assets (e.g. `panelHorizontal.png`, `panelVertical.png`) load when run from repo root.
- Layout restructured per Image 2: `Game::draw()` now renders a middle sectioned panel (tabs Options Stats Legacy Info Store along the top wood strip; blank bands separated by tiled `panelHorizontal` borders; vertical `panelVertical` wood dividers between left cookie panel / middle panel / right store panel) — all panel borders placed, never scaled, last tile clipped to exact edge, positions snapped to whole pixels. Right store panel keeps `"STORE"` title, store cells (storeTile strip, icon, name, price with cookie icon, faded owned amount bottom-right scale 1.5f vertically centred), full-row click to buy, cell separator lines, flashing purchase feedback.
- Build + run verified: `cmake --build build -j` (exit 0), `timeout 3 ./build/cookie_clicker` (exit 124, expected). All textures load incl. panelHorizontal (512x16) and panelVertical (16x512), no magenta fallback.
- Fonts: switched from Tahoma to **Merriweather** (`Merriweather_24pt-Bold.ttf` from cwd `fonts/`, embedded Tahoma as fallback). Font sizes: cookie count 1.5f (~24px), bakery name 1.25f (~20px), CPS 0.8f (~13px), store title 1.75f (~28px), building name 1.2f (~19px), price 0.8f (~13px), owned amount 1.5f (~24px).

## Known broken, stubbed, or unverified code (honest list)

Stubs (no-op / placeholder):
- `cpp/Audio.cpp` — Audio::init/playSound/setVolume all no-op; no sound playback
- `cpp/Assets.cpp` — Assets::init no-op, hasImage always returns false
- `cpp/Game.cpp::updateSpecial/updateGrandmapocalypse/particlesUpdate` — empty
- `cpp/SaveLoad.cpp::loadSave()` — minigame saves (section 5 field 4) ignored (minigames stubbed); ascensionMode (section 4 field 29) read as placeholder; season variables loaded but season behavior stubbed
- `cpp/SaveLoad.cpp::writeSave()` — building minigame save field is empty comma (matches JS when no minigame); ascensionMode writes placeholder 0
- `cpp/Minigame.cpp` and Garden/Grimoire/Pantheon/Dungeons — all stubs (init sets loaded=true, nothing else)
- `cpp/DungeonGen.cpp` — returns "Mysterious dungeon" only
- `cpp/Shimmer.cpp`, `Particle.cpp` — basic structs only, not wired into game
- `cpp/Building.cpp` — `modifyBuildingPrice` hook not implemented; `getSumPrice` correct but unused; buy/sell cookie deduction handled by Game (not Building)
- Wrinkler simulation beyond `cpsSucked` (spawning, clicking/popping, hp, close animation) not implemented — save/load and CpS sucking only
- Buff behavior (multCpS/multClick effects in `calculateGains`/`mouseCps`) still stubbed — save/load round-trip only

Partially implemented:
- `cpp/Game.cpp::mouseCps()` — includes fingers/mouse upgrades/buffs/aura/gods, but missing Cookie egg, eff click, custom callbacks fully wired
- `cpp/Game.cpp::calculateGains()` — full port: building CpS + prestige + cookie upgrades + kittens + eggs + sugar baking + auras + golden switch + elder covenant + wrinklers + buffs + globalCpsMult. VERIFIED for base building CpS.
- `cpp/Game.cpp::logic()` — earns cookies per frame, handles milk/research/seasons/wrinklers/totalCookies/autosave; missing minigame logic, particle shower, achievement wins
- `cpp/Game.cpp::clickCookie()` — earns cookies, updates handmadeCookies/cookieClicks; missing particles, sound, customCookieClicks, loseShimmeringVeil

Unverified:
- Base64 encode/decode round-trip with special characters
- `escape()`/`unescape()` pipeline for non-ASCII bakery names/seeds
- Save/load compatibility with browser-made saves (C++ → browser and browser → C++)
- PRNG parity with David Bau seedrandom (std::mt19937 used instead, UNVERIFIED for dungeons/mutations/lumps)
- `double` cookie math matching JS exactly over long runs (UNVERIFIED)

## Portability constraint violations (tasks — do not fix now, just aware)

The current `cpp/` skeleton predates the DSi/3DS constraints and violates them. These are listed as tasks; do NOT fix as part of the next scoped item.

- `cpp/Game.h` / `cpp/Game.cpp`: uses `std::vector`, `std::map`, `std::string`, `std::function` (heap containers) in core; uses `double` in per-frame `logic()` / `mouseCps()` / `calculateGains()`.
- `cpp/Building.h` / `cpp/Building.cpp`: uses `std::vector`, `std::string`, `std::function` (heap) in core.
- `cpp/Upgrade.h/.cpp`, `cpp/Achievement.h/.cpp`, `cpp/Buff.h/.cpp`: use `std::string`, `std::function` (heap) in core.
- `cpp/SaveLoad.h/.cpp`: core save/load uses `std::ostringstream` (stream/heap) and `std::vector<char>`; acceptable only because save/load is not per-frame, but still violates "avoid streams" — prefer fixed buffers.
- `cpp/Types.h`: `now_ms()` uses `std::chrono::system_clock` (host wall clock) — fine for PC, but `Storage`/platform should supply time on DSi/3DS.
- `cpp/main.cpp`: includes SDL2 directly — OK, this is the PC backend, NOT core.
- `cpp/Render.h/.cpp`, `cpp/Audio.h/.cpp`, `cpp/Assets.h/.cpp`: platform stubs — OK, these are the PC backend, NOT core. They must become the `Render`/`Audio`/`Input`/`Storage` implementations.

## Decisions already made (do not redo)
- All cookie math uses C++ `double` to match JS IEEE-754 behavior
- Main loop is fixed-timestep at ~30 FPS (`GameConst::FPS`), matching the JS `setTimeout` loop
- Rendering target: SDL2 + OpenGL window; PNG decoding via libpng; text via FreeType2 (TTF). Dear ImGui not used. `cpp/Render.cpp` is the PC backend implementation; the core never sees SDL/OpenGL types.
- SDL2_mixer optional (not present; Audio is stub)
- Save format: exact JS-compatible pipe/semicolon text → UTF-8 → Base64 → `!END!` → escape; `pack3` = identity (raw `0/1` bitstrings)
- Save persistence: file-based (`<bakeryName>Bakery.txt`) instead of localStorage/cookies
- PRNG: `std::mt19937` seeded from time; David Bau seedrandom NOT yet ported
- Offline progress uses `std::chrono::system_clock` wall-clock delta (JS `Date.now` equivalent)
- `build/` directory is generated output; do not commit