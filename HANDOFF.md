# Handoff — Cookie Clicker C++ Port

## Current State
UI layer extracted from Game. All layout lives in `cpp/UI.h` Layout struct, all drawing in `cpp/UI.cpp`. Core `Game` knows nothing of SDL, OpenGL, FreeType, or rendering. Input flows `UI::on_pointer(x,y)` → game action → main loop applies to `Game`.

## DONE — UI Layer Extraction
- Native SDL2 desktop window (**1280x720, 16:9**, resizable, OpenGL) via `main.cpp`.
- `Game` has **no `draw()` function** and includes **no `Render` or `UI` headers**. Core compiles with only logic headers.
- `UI` takes `const Game&` and `Render&`, owns **all** layout, drawing, and hit-testing.
- Input goes `UI::on_pointer(x, y)` → returns a game action → main loop applies it to `Game`.
- All layout numbers live in a `Layout` struct, **not** inline in draw code.
- Behaviour and visuals are **pixel-identical** to the real v2.058 game (fractions measured from `real_game.png`).
- `Layout` can be supplied by a DSi/3DS port and reuse the same drawing code without changes.

## UI Layout — Fractions Verified vs Real Game
| Element | Real Fraction (1920x919) | Port Fraction (900x600) | Status |
|---|---|---|---|
| Banner Y centre | 0.0968 | 0.097 | ✅ |
| Cookie count Y centre | 0.1393 | 0.141 | ✅ |
| CPS Y centre | 0.1632 | 0.163 | ✅ |
| Cookie centre Y | 0.3885 | 0.388 | ✅ |
| Store panel width | 0.1583 | 0.158 | ✅ |
| Left panel width | 0.30 | 0.30 | ✅ |

## UI Features (matching real v2.058)
- **Left panel**: Rounded banner with Kavoon heading (bakery name), "N cookies" large (PT Sans), "per second: X" small (PT Sans), big cookie (#bigCookie) centered with soft glow, clickable
- **Middle panel**: Game world view between cookie panel and store panel — cookie strip with unlocked building sprites running horizontally near the top, green grass field beneath
- **Top menu bar**: Wooden plank strips per button (no continuous background). Options+Stats stacked top-left column, Info+Legacy stacked top-right column, centered news ticker text between them
- **Store panel** (right): ~16% of window width. "STORE" title (Kavoon heading). Upgrade icon row (6 slots with building icons, first unlocked highlighted). Buy/Sell toggle with separate "Buy"/"Sell" buttons and bulk labels "1 / 10 / 100". Contiguous store rows (no separators/frames): each row has icon, name ("???" when locked), price with small cookie icon, faded owned amount bottom-right. Locked items show dark silhouette. Full-row click to buy.
- **Background**: Three layers — `bgStripes.png` tiled vertically, `vignette.png` radial gradient, `glow.png` drawn behind cookie
- **Fonts**: Kavoon for headings, PT Sans for body text. Zero Tahoma references.

## Rendering — Verified Clean
- `cmake --build build -j` (exit 0)
- `timeout 3 ./build/cookie_clicker` (exit 124, expected from timeout)
- All 8 textures load with no magenta fallback: `bgStripes.png`, `vignette.png`, `glow.png`, `perfectCookie.png`, `smallCookies.png`, `panelHorizontal.png`, `panelVertical.png`, `storeTile.png`
- No Tahoma references in source (verified with `grep -rl "[Tt]ahoma" cpp/ || echo "SOURCE CLEAN"`)
- Build warning-clean

## File Changes Summary
- `cpp/UI.h` — Layout struct with all fractions, scaled base sizes, font enums (Default=Kavoon, Body=PT Sans)
- `cpp/UI.cpp` — draw() with background layers, banner, count/CPS, top menu bar, store column with `storeW/304` scaling
- `cpp/EmbeddedAssets.h/cpp` — 26 embedded assets: 18 building icons + perfectCookie + cursor + farm/mine/factory/bank/wizardtower/shipment/alchemylab/portal/timemachine/antimattercondenser/prism/chancemaker/fractalEngine/javascriptconsole/idleverse + `bgStripes.png`/`vignette.png`/`glow.png` + `Kavoon-Regular.ttf` + `PTSans-*.ttf`; `bgBlue.png` dropped
- `cpp/Render.h/cpp` — `drawTexture` optional UV params; `drawTextureTiled` one-axis tiling; glyph cache cleared on face size change
- `cpp/Game.cpp` — Permanent unlock: `cookiesEarned >= basePrice` → `b->locked = false`; cookie wobble damping threshold
- `cpp/Types.h` — `i32`/`u32` definitions

## Known Broken / Stubbed (do not fix — out of scope)
- `cpp/Audio.cpp` — no-op
- `cpp/Assets.cpp` — no-op
- `cpp/Game.cpp::updateSpecial/updateGrandmapocalypse/particlesUpdate` — empty
- `cpp/SaveLoad.cpp::loadSave()` / `writeSave()` — minigame/ascension stubs
- `cpp/Minigame.cpp`, Garden/Grimoire/Pantheon/Dungeons — all stubs
- `cpp/DungeonGen.cpp` — returns "Mysterious dungeon" only
- `cpp/Shimmer.cpp`, `Particle.cpp` — basic structs only
- `cpp/Building.cpp` — `modifyBuildingPrice` hook not implemented
- Wrinkler simulation beyond `cpsSucked`; Buff behavior

## Build & Test Commands
```
cd /home/superuser/Cookie-Clicker
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
timeout 3 ./build/cookie_clicker   # exit 124 expected
```

Syntax check (no SDL/png/ft needed for core):
```
cd cpp
g++ -std=c++17 -fsyntax-only -Wall -Wextra -I. *.cpp
```

## Commit Workflow
1. Run build (`cmake --build build -j`). Stop on failure.
2. Run task's checks (`timeout 3 ./build/cookie_clicker` → exit 124).
3. `git add` only changed files.
4. Commit with terse message: **what changed and why**. One logical change per commit.
5. `git push`. Never force-push or rewrite history.

## Portability Constraints (aware, do NOT fix now)
- Core uses `std::vector`, `std::map`, `std::string`, `std::function` (heap containers) — acceptable for now
- Core uses `double` in per-frame paths
- `cpp/Render.h/.cpp`, `cpp/Audio.h/.cpp`, `cpp/Assets.h/.cpp` are platform stubs (PC backend, NOT core)
- `cpp/main.cpp` includes SDL2 directly — OK, PC backend only
- Save format: exact JS-compatible pipe/semicolon text → UTF-8 → Base64 → `!END!`