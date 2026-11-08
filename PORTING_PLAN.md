# Cookie Clicker → C++ Port Plan

## 1. Repository analysis

This is a browser port of **Cookie Clicker v2.021** (Orteil / DashNet, compiled by Cdobby). It is a pure-JavaScript, HTML5 incremental/clicker game.

### File inventory (~22k lines total)

| File | Lines | Role |
|------|-------|------|
| `main.js` | 13,694 | Core engine: init, data, economy, save/load, buildings, upgrades, achievements, buffs, minigames, logic, draw, main loop |
| `minigameGarden.js` | 2,016 | Farm minigame (planting/harvesting/mutations) |
| `minigameGrimoire.js` | 500 | Wizard-tower minigame (spells/grimoire) |
| `minigamePantheon.js` | 500 | Temple minigame (pantheon god slot/buffs) |
| `dungeons.js` | 1,136 | Dungeon minigame |
| `DungeonGen.js` | 871 | Procedural dungeon generation |
| `ajax.js` | 6 | XMLHttpRequest helper (news/update ticker) |
| `base64.js` | 142 | Base64 encode/decode (used by save format) |
| `showads.js` | 3 | Adblock detection |
| `style.css` | 3,148 | All UI styling |
| `index.html` | 231 | DOM shell + ad/consent scripts |
| `img/` | 339 files | Building icons, backgrounds, cookie, UI sprites |
| `snd/` | ~50 files | MP3 sound effects |

### main.js section map (system boundaries)

```
14   MISC HELPER FUNCTIONS            (math, shuffle, ajax, sound, loader, Canvas shims)
562  GAME INITIALIZATION              (Game.Launch)
1142 VARIABLES AND PRESETS            (building/upgrade/achievement raw data begins here)
1342 MOD HOOKS
1382 BAKERY NAME
1422 TOOLTIP
1599 UPDATE CHECKER
1627 DATA GRABBER
1729 SAVE                            (Game.WriteSave / Game.ExportSave / Game.FileSave)
1988 LOAD                            (Game.LoadSave)
2510 RESET                           (Game.Reset / hard/soft reset, ascend)
2975 PRESTIGE                        (heavenly chips, resets)
3453 COALESCING SUGAR LUMPS
3732 COOKIE ECONOMICS                 (Earn/Spend/Dissolve, mouseCps, ClickCookie)
3965 CPS RECALCULATOR                 (CalculateGains, mult/effs)
4229 SHIMMERS (golden cookies & such)
4920 PARTICLES
5165 NOTIFICATIONS
5279 PROMPT
5332 MENUS
5832 NEWS TICKER
6471 BUILDINGS                        (Game.Object class, price/CpS curves, 18 building types)
7697 UPGRADES                         (Game.Upgrade class, tiered upgrades)
9929 ACHIEVEMENTS
10822 BUFFS
11191 GRANDMAPOCALYPSE
11629 SPECIAL THINGS AND STUFF
12047 VISUAL EFFECTS
12796 INITIALIZATION END / ready
13017 LOGIC                           (Game.Logic - per-frame game simulation)
13171 UNLOCKING STUFF
13471 DRAW                            (Game.Draw - Canvas + DOM rendering)
13588 MAIN LOOP                       (setTimeout @ ~30fps: Logic → Draw)
13667 LAUNCH THIS THING
```

### Core data model
- **Game** singleton holding all state (cookies, cookiesPs, buildings[], upgrades[], achievements[], buffs[], shimmers[], season, prestige, lumps, prefs, etc.)
- **Buildings**: 18 types (Cursor … Javascript console). Amount, bought, totalCookies, level, price, baseCps, minigame.
- **Upgrades**: flat + tiered (per-building), each with name/desc/price/icon/bought/unlocked.
- **Achievements**: ~500, won/unlocked, tied to building counts/CpS/cookies.
- **Buffs**: timed modifiers (Click frenzy, Elder frenzy, etc.) with type/maxTime/time/args.
- **Shimmers/Particles**: golden cookies, reindeer, wrath cookies, floating text.
- **Minigames**: Garden, Grimoire, Pantheon, Dungeons (each attached to a building).
- **Save format**: pipe-`|` / semicolon-`;` delimited text, UTF-8 → Base64, terminated with `!END!`, then JS `escape()` for storage in localStorage/cookies/file. `pack3` is currently the identity function (bitfields stored as raw `0/1` strings).

### External dependencies
- HTML5 Canvas 2D (`backgroundCanvas`, `backgroundLeftCanvas`) for background rendering.
- Heavy DOM manipulation for all UI (`innerHTML`, `className`, `classList`).
- `localStorage` / browser cookies for save persistence; `FileReader`/`Blob` for file import/export.
- `XMLHttpRequest` for the news/update ticker and ad detection.
- HTML5 `Audio` for ~50 sound effects.
- David Bau `seedrandom` (ARC4-based seeded PRNG) keyed by `Game.seed` — used for deterministic dungeon generation, plant mutations, sugar-lump types.
- Google Fonts, cookieconsent2, Google AdSense (not part of game logic).

---

## 2. Porting options

| Option | Description | Pros | Cons |
|--------|-------------|------|------|
| **A. Native desktop app (SDL2 + Dear ImGui)** | Rewrite logic in C++; ImGui immediate-mode UI; SDL2 window/input/audio/timer. | Full control, offline, file-based saves, easy to distribute, closest to a "real" port. | Must rewrite all UI/layout from CSS→ImGui; canvas background → custom 2D renderer. |
| **B. WebAssembly (Emscripten)** | Compile C++ logic to WASM; keep existing HTML/CSS/JS UI; JS glue calls into WASM. | Reuses existing UI/assets; incremental port (logic first, UI later). | Requires emscripten toolchain; WASM ↔ JS boundary complexity; still need audio/asset loading shims. |
| **C. Headless / logic-only** | Port game engine + save/load only; no rendering. | Fastest; useful for bots, simulations, save editors, analyzers. | Not a playable game by itself. |

### Recommendation: Option A (native desktop app)
It produces a standalone, offline-playable game and gives a clean place to replicate the exact save format so C++ and JS saves remain interchangeable. Option B is a good fallback if you want to ship to the browser quickly.

---

## 3. Recommended C++ architecture

```
Cookie-Clicker/
├── PORTING_PLAN.md
├── CMakeLists.txt                (top-level build)
├── cpp/
│   ├── CMakeLists.txt
│   ├── main.cpp                  (entry: window, loop, init)
│   ├── Game.h / Game.cpp         (Game singleton; launch, loop, logic, draw)
│   ├── Types.h                   (shared typedefs, math, seedrandom PRNG)
│   ├── Economy.h / Economy.cpp   (Earn/Spend/Dissolve, mouseCps, ClickCookie, CalculateGains)
│   ├── Building.h / Building.cpp (Game.Object equivalent)
│   ├── Upgrade.h / Upgrade.cpp
│   ├── Achievement.h / Achievement.cpp
│   ├── Buff.h / Buff.cpp
        ├── Shimmer.h / Shimmer.cpp   (golden cookies etc.)
        ├── Particle.h / Particle.cpp
        ├── Minigame.h              (base; Garden, Grimoire, Pantheon, Dungeons derive)
        ├── Garden.h/.cpp
        ├── Grimoire.h/.cpp
        ├── Pantheon.h/.cpp
        ├── Dungeons.h/.cpp
        ├── DungeonGen.h/.cpp
        ├── SaveLoad.h / SaveLoad.cpp (exact pipe/base64/UTF-8/!END! format)
        ├── Assets.h / Assets.cpp   (image/sound loading)
        ├── Audio.h / Audio.cpp     (sound playback)
        └── Render.h / Render.cpp   (2D rendering abstraction)
├── third_party/                  (imgui, SDL2, stb_image, miniaudio, seedrandom port)
├── assets/                       (copied img/ + snd/)
└── build/
```

### Key technical decisions

1. **Floating point**: JS numbers are IEEE-754 doubles. Use C++ `double` everywhere for cookie counts/CpS to preserve behavior exactly. Note: `double` loses exact integer precision above 2^53 (~9e15), which the JS game already does; behavior will match. If you need exact giant-integer accounting later, switch to a decimal/big-int type, but `double` is the faithful port.

2. **Save format compatibility**: replicate `Game.WriteSave` / `Game.LoadSave` byte-for-byte:
   - fields joined by `;`, sections by `|`
   - prefs stored as a raw `0/1` bitstring (pack3 = identity)
   - upgrades/achievements stored as packed `0/1` bitstrings
   - UTF-8 string → Base64 → append `!END!` → JS `escape()` equivalent for storage
   - On load: reverse the pipeline; validate version (`version >= 1`, reject `version > Game.version`)
   - Store to a plain text file (`<bakeryName>Bakery.txt`) instead of localStorage.

3. **Seeded randomness**: port David Bau's `seedrandom` (ARC4-based) to C++ so `Math.seedrandom(Game.seed)` sequences (dungeons, plant mutations, lump types) are identical. If exact sequence parity isn't required, any decent PRNG (std::mt19937) seeded from `Game.seed` works for most purposes.

4. **Timing / main loop**: JS uses `setTimeout(Game.Loop, 1000/Game.fps)` (~30 FPS). In C++ use a fixed timestep with `std::chrono::steady_clock` + `std::this_thread::sleep_for`, or render on vsync and accumulate delta seconds for `Game.Logic(dt)`. Offline progress (cookies earned while closed) is computed from wall-clock delta on load (`Date.now()` → `std::chrono::system_clock`).

5. **UI layer separation (extraction goal)**: Split UI/layout out of `Game` into a separate `UI` layer so the core is portable to DSi/3DS without remaking UI.
   - **Rule 1**: `Game` has **no `draw()` function** and includes **no `Render` or `UI` headers**. Core compiles with only logic headers on the include path.
   - **Rule 2**: `UI` takes `const Game&` and `Render&`, owns **all** layout, drawing, and hit-testing.
   - **Rule 3**: Input goes `UI::on_pointer(x, y)` → returns a game action → main loop applies it to `Game`.
   - **Rule 4**: All layout numbers live in a `Layout` struct, **not** inline in draw code.
   - **Rule 5**: Behaviour and visuals must be **pixel-identical** to now — this is a pure refactor, no visual change.
   - **Rule 6**: Verify that the logic target compiles with only the logic headers on the include path.

   This lets the PC port use a single-screen layout (cookie panel left, store right) while a DS/Dsi port can reuse the same `UI` logic with a two-screen layout — only `Render` is rewritten per platform.

6. **Audio**: SDL_mixer or miniaudio for MP3/WAV playback with volume and concurrent instances (the JS uses 12 sound-instance channels).

7. **Network**: The news/update ticker and AdSense are non-essential to gameplay. Port them as optional (HTTP fetch via cURL/WinHTTP) or drop them for the initial release.

---

## Portability constraints (PC first, DSi / 3DS end targets)

The end targets are Nintendo DSi (DSi mode, BlocksDS) and later 3DS. The PC build is a development host, not the final platform. The following constraints apply to the **game core** (economy, buildings, upgrades, achievements, save/load, minigame logic):

- **No platform dependencies in the core.** Core `.h` / `.cpp` files must NOT `#include` or call ImGui, OpenGL, SDL, or any filesystem API (`<fstream>`, `<filesystem>`, `fopen`, `stat`, `opendir`, etc.). The core must compile on a hosted DSi/3DS toolchain with only the C++ standard library.
- **Four thin platform interfaces only.** The core talks to the platform exclusively through: `Render`, `Audio`, `Input`, `Storage`. ImGui/OpenGL/SDL live only in the PC backend. Each interface is a small pure-C++ abstract header the core depends on; the PC backend provides the implementation.
- **Minimize heap and streams.** Avoid `<iostream>` / `<sstream>` and exceptions in the core. Prefer fixed-size arrays over `std::vector` / `std::map` where counts are bounded (buildings, upgrades, achievements, buffs, shimmers, particles). Accept `std::string` only for save/load text and names; avoid per-frame heap churn.
- **Keep doubles out of per-frame paths.** Cookie counts and CpS may be `double` for save-format fidelity, but per-frame simulation (`Logic(dt)`) should avoid `double` where fixed-point or integer math suffices, to stay friendly to DSi's lack of an FPU.
- **Save format stays byte-compatible.** The base64 pipe/semicolon/`!END!`/JS-`escape()` save format must remain identical to the browser so DSi/3DS/PC/browser can all read each other's saves. `Storage` handles read/write of that byte string; the core only produces/consumes the string.

These constraints are a design target for refactor work; the current `cpp/` skeleton predates them and violates several — see HANDOFF.md for the violation task list.

---

## 4. Subsystem porting notes

### Save / Load (highest fidelity requirement)
`WriteSave` builds a string like:
```
<version>||
<runStart>;<legacyStart>;<lastDate>;<bakeryName>;<seed>|
<prefs bitstring>|
<cookies>;<cookiesEarned>;<cookieClicks>;...<season>|
<building sections: amount,bought,totalCookies,level,minigame.save,muted;>|
<upgrades bitstring>|
<achievements bitstring>|
<buffs: type.id,maxTime,time,arg1..;>|
```
Then `utf8_to_b64(str) + '!END!'` → `escape()` for disk. The C++ port must parse/generate this exactly so you can import a save made in the browser and vice versa. `pack3` being identity simplifies the bitfield handling (no bit-packing to implement).

### Economics / CPS
- `CalculateGains` runs every frame: sums building CpS, applies upgrades, buffs, aura, god effects, minigame effs, heavenly power, prestige.
- `mouseCps` (click value) depends on "fingers" upgrades, mouse upgrades, buffs, and `ComputeCps`.
- All multipliers are plain `double` arithmetic — straightforward to port.

### Buildings & upgrades
- `Game.Object` constructor: id, name, basePrice, baseCps, icon, art, buyFunction. Price curve: `basePrice * 1.15^(amount - free)`, rounded up.
- Upgrades are tiered per building (`MakeTiered`). Each upgrade has `unlocked`/`bought` booleans.
- The raw data (names, descriptions, prices, icons) lives in the `VARIABLES AND PRESETS` section — extract into C++ data tables (JSON or hardcoded structs).

### Rendering differences
| JS | C++ equivalent |
|----|----------------|
| Canvas 2D `drawImage`/`fillPattern` | OpenGL textured quads via ImGui backend, or software renderer |
| DOM `innerHTML`/`className` | ImGui widgets (Tables for store rows, Text, Buttons, Tooltips) |
| CSS animations/transitions | ImGui animation via `ImGui::GetTime()` / per-frame lerp |
| `requestAnimationFrame`/setTimeout loop | Fixed-timestep C++ loop |

### Minigames
- Garden (Farm), Grimoire (Wizard tower), Pantheon (Temple), Dungeons are large self-contained systems. Port them after the core loop is stable, one at a time.

---

## 5. Implementation order (milestones)

1. **Skeleton**: CMake project, window + ImGui + SDL2, main loop, empty `Game` struct.
2. **Core data types**: `double`-based cookie counters, `Building`, `Upgrade`, `Achievement` containers; hardcode the 18 buildings and a starter set of upgrades/achievements from the PRESETS section.
3. **Economy & logic**: `CalculateGains`, `Earn/Spend`, `mouseCps`, `ClickCookie`, building purchase, `Game.Logic(dt)` fixed-timestep.
4. **Save/Load**: replicate exact format; file-based persistence; verify a browser-made save loads and a C++-made save can be imported into the browser.
5. **UI**: cookie panel, store rows, upgrades panel, stats/legacy/options menus, tooltips, prompts/notifications.
   - Before fleshing out upgrades/stats/legacy panels, **extract the UI layer** per the rules in §3 (decision 5): move all layout out of `Game::draw()` into a `UI` class taking `const Game&` and `Render&`, with all layout numbers in a `Layout` struct. This is a pure refactor — visuals must remain pixel-identical. Once extracted, add new panels (upgrades, stats, legacy, options) to the `UI` layer, not to `Game`.
6. **Rendering**: background canvas equivalent, particles, shimmers (golden cookies).
7. **Audio**: load & play the MP3 SFX; volume control.
8. **Minigames**: Garden → Grimoire → Pantheon → Dungeons (optional, last).
9. **Polish**: prestige/reset, ascension, sugar lumps, grandmapocalypse, seasons, news ticker (optional).

---

## 6. Risks & open questions

- **Save compatibility**: verify exact behavior of `escape()`/`unescape()` for non-ASCII bakery names (JS `escape` produces `%uXXXX`); C++ side must match if you want cross-platform save interchange.
- **Floating-point divergence**: C++ `double` should match JS, but order of operations in `CalculateGains` must be preserved to avoid tiny drift over long runs.
- **seedrandom**: porting David Bau's ARC4 PRNG exactly is required only if you need deterministic parity for dungeons/plant mutations.
- **339 images + ~50 sounds**: need to be copied into `assets/` and loaded; some are CSS-sprite-sheet based (`buildings.png`) — you'll need sub-rect UVs.
- **DOM-heavy effects** (floating "+N cookies" numbers, sparkles, shimmers as DOM elements) must be reimplemented as renderable entities.

### Rendering status — PC backend (Render.cpp) — VERIFIED WORKING

The following rendering bugs were fixed and verified (build + run, exit 124 from `timeout`):
- **Text diagonal tearing**: `Render::getGlyph()` now sets `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` and `GL_UNPACK_ROW_LENGTH` to FreeType's `bm.pitch` before uploading glyph alpha bitmaps.
- **Big cookie white square**: `Render::drawTexture()` now calls `glEnable(GL_TEXTURE_2D)` before binding. Asset path corrected `imperfectCookie.png` → `perfectCookie.png`; embedded byte data replaced with real `perfectCookie.png`. All `imperfectCookie` references removed.
- **PNG alpha**: `loadPNG()` no longer calls `png_set_add_alpha` after `png_set_tRNS_to_alpha`. `loadTexture` logs every success/failure.
- **Background**: `Game::draw()` draws `img/bgBlue.png` full-screen instead of a dark rect. `bgBlue.png` embedded (433,960 bytes).
- **Cookie text size**: scale reduced 2.0f → 1.5f so it fits on screen.
- **Texture sub-rect UVs**: `drawTexture()` gained optional `srcX/srcY/srcW/srcH` params (0 = full texture). Cursor uses top-left 32x32 of `cursor.png` sprite sheet; store rows use one 64px strip of `storeTile.png` per row at natural 300px width centered in cell.
- **Store panel**: no BUY button — full-row click buys; locked buildings show `"???"` with red price; permanent unlock via `cookiesEarned >= basePrice`; building names scale 1.2f with text shadow, price scale 0.8f with `smallCookies` icon prefix; big faded owned amount inside cell repositioned per Image 1 to bottom-right corner (`right:12px`, vertically centred) instead of top:6px, scale 1.5f; cell separator lines → grid-of-cells look. Panel borders (`panelHorizontal`/`panelVertical`) tiled at natural size, never scaled, per Image 2 sectioned layout. Middle panel top tab strip: Options Stats Legacy Info Store. Font switched to Merriweather.
- **Cookie wobble**: clamped between 90% and 120%; click pop target 1.2 + velocity 0.3.

Text renders clean, cookie renders as a proper cookie, blue background shows, store cells look correct. No further rendering work needed for the current scoped item.

---

## 7. Starter C++ skeleton

A minimal `cpp/` skeleton is included in this repository (CMake + SDL2 + core `Game` loop + save/load stubs). It compiles the framework without the full game data, so you can iterate subsystem by subsystem.

### Building the skeleton

```bash
cd /home/superuser/Cookie-Clicker
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/cookie_clicker
```

## Commit & build workflow

At the end of every task:
1. Run the build (`cmake --build build -j`). If it fails to compile, stop and report it.
2. Run the task's checks (`timeout 3 ./build/cookie_clicker` → expect exit 124).
3. `git add` only the files you changed.
4. Commit with a terse message saying **what changed and why**. One logical change per commit — never mix refactors, features, and asset changes.
5. `git push`. Never force-push or rewrite history. Never commit a broken build.

Requirements:
- **SDL2** (required) – present on this box; `find_package(SDL2)` works.
- **OpenGL** (required) – for the future ImGui renderer.
- **SDL2_mixer** (optional) – audio; if missing, `Audio.cpp` is a no-op stub.
- **Dear ImGui** (optional) – vendor into `third_party/imgui/`; if missing, `Render.cpp` is a stub.

Notes:
- `main.cpp` opens an SDL window and runs a fixed-timestep loop (`~30 FPS`, matching the JS `setTimeout` loop).
- Right now it does nothing visible: `Render`/`Audio`/`Assets` are stubs and `Game::load()` hasn't populated buildings/upgrades. Fill these in per the milestones in §5.
- The build directory is not part of the port; add `build/` to `.gitignore`.

## 8. License note

The original code and graphics are copyright Orteil, 2013-2019. The README asks that you not re-host, profit from, or present the code as your own. This port is a learning/exercise derivative of that source; keep the same attribution and respect those terms if you distribute it.
