# Progress Log — Cookie Clicker C++ Port

## Session Summary: UI Layer Extraction & Layout Polish (Images 1 → 2)

### Completed
- **Background replacement**: `bgBlue.png` dropped; three-layer tiled background using `bgStripes.png` (vertical timeline), `vignette.png` (radial gradient), `glow.png` (soft cookie glow)
- **Font overhaul**: All Tahoma references removed from `cpp/`; Kavoon for headings, PT Sans for body text embedded as `Kavoon-Regular.ttf` + `PTSans-*.ttf`
- **Layout struct in UI.h fully rewritten**: All positions/sizes measured from `real_game.png` (1920x919) and scaled to 1280x720 port. Fractions verified against real game layout.
- **UI.cpp draw() overhauled**: Proper panel layout, text rendering, store column with scaled content (by `storeW/304`), rounded banner, PT Sans body text
- **Store column**: Width fraction `0.158f` matches real game (~16% of window). Content scaled proportionally. Locked items show `"???"` in dark silhouette. No row separation/frames.
- **Top menu bar**: Options+Stats stacked top-left column, Info+Legacy stacked top-right column, centered news ticker between them. Per-button wood plank strips, no continuous background strip.
- **Middle panel (new)**: Game world view between cookie panel and store panel — cookie strip with unlocked building sprites running horizontally near the top, green grass field beneath
- **Store upgrade row**: Shows building icons per slot instead of empty rects; first unlocked building's slot highlighted gold
- **Buy/Sell toggle**: Separate "Buy" and "Sell" buttons with bulk labels "1 / 10 / 100"
- **CPS text**: Changed to `"per second: X"` format
- **Cookie wobble**: Settled with spring damping threshold (`|target-size - current-size| < 0.001` and `|velocity| < 0.001` → snap to target and zero velocity)
- **Window size**: 900x600 (3:2) → 1280x720 (16:9) to match Image 2 aspect ratio
- **Build**: `cmake --build build -j` succeeds; `timeout 3 ./build/cookie_clicker` exits 124 (expected)

### Layout Fractions Verified vs Real Game (1920x919 → 1280x720)
| Element | Real Fraction | Port Fraction | Status |
|---|---|---|---|
| Banner Y centre | 0.0968 | 0.097 | ✅ |
| Cookie count Y centre | 0.1393 | 0.141 | ✅ |
| CPS Y centre | 0.1632 | 0.163 | ✅ |
| Cookie centre Y | 0.3885 | 0.388 | ✅ |
| Store panel width | 0.1583 | 0.158 | ✅ |
| Left panel width | 0.30 | 0.30 | ✅ |

### Active
- None

### Blocked
- None

### Files Modified (key changes)
- `cpp/UI.h` — Layout struct: added middle panel section (cookie strip Y/H, grass Y), added `grassColor`; top bar buttons restacked (Options+Stats left column, Info+Legacy right column); store panel section clarified
- `cpp/UI.cpp` — draw() additions: middle panel cookie strip (building sprites) + grass field; top bar buttons stacked vertically in corners with centered news ticker; store upgrade row shows building icons with first slot highlighted; Buy/Sell toggle shows separate "Buy"/"Sell" buttons; CPS text now "per second: X"
- `cpp/main.cpp` — window size changed from 900x600 (3:2) to 1280x720 (16:9) to match Image 2 aspect ratio
- `cpp/EmbeddedAssets.h/cpp` — TTF fonts replaced (Kavoon + PT Sans); `bgBlue.png` dropped; `bgStripes.png`/`vignette.png`/`glow.png`/`storeTile.png` embedded
- `cpp/Render.h/cpp` — `drawTexture` gained optional `srcX/srcY/srcW/srcH` UV parameters; `drawTextureTiled` added; font glyph cache cleared on size change
- `cpp/Game.cpp` — Permanent building unlock via `cookiesEarned >= basePrice`; cookie wobble clamping
- `cpp/Types.h` — Type definitions

### Porting target (current direction)
- **Phase 1 — Core economy (buildings → upgrades → minigames → sounds → clean UI UX later)**
  - **Buildings**: all 18 ported in `Game.cpp::load()` ✓ (Cursor … Idleverse). Unlock via `cookiesEarned >= basePrice` implemented.
  - **Upgrades**: starter upgrade data populated in `Game::load()`, `Game::buyUpgrade()`, `GetTieredCpsMult()` wired to bought tiered upgrades ✓
  - **Minigames (now)**: Garden (Farm plot grid, plant growth/mutations/harvest), Grimoire (Wizard tower mana/spells/cooldowns), Pantheon (Temple god slots/buffs/drawbacks), Dungeons (procedural generation via DungeonGen, floor progression, rewards). All core logic only — `draw()` stubbed, no UI dependencies. Wired into `Game::logic()` (`eachFrame()`), `calculateGains()` (CPS bonus), `mouseCps()` (click bonus), and SaveLoad (per-building minigame save strings).
  - **Sounds (next)**: `Audio.cpp` is a no-op stub; SDL2_mixer not installed on this box.
  - **Clean UI UX**: deferred until core economy (buildings + upgrades + minigames + save/load round-trip) is stable.
- **Phase 2 (after core stable)**: Save/Load round-trip verification, store row click-to-buy, particles, minigame UI.

### Active
- Minigame core logic (Garden, Grimoire, Pantheon, Dungeons + Game wiring + SaveLoad)

### Blocked
- Sounds: SDL2_mixer not installed on this box (`Audio.cpp` remains no-op stub)