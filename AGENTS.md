# Cookie-Clicker

C++ port of Cookie Clicker v2.021 (Orteil / DashNet). Native SDL2 + OpenGL desktop app, C++17, CMake. Window 1280x720 resizable, fixed-timestep loop (~30 FPS). Assets (PNGs + Kavoon/PT Sans fonts) are embedded in `cpp/EmbeddedAssets.{h,cpp}`. Entrypoint: `cpp/main.cpp`.

## Build & run

```
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
timeout 3 ./build/cookie_clicker   # runs forever; exit 124 from timeout is expected
```

Required (installed on this box): SDL2, OpenGL, libpng, freetype. Optional: SDL2_mixer (audio; missing → `Audio.cpp` no-op stub), Dear ImGui (`third_party/imgui`; missing → `Render.cpp` stub). `build/` and `stock footage/` are gitignored — never commit build output.

## Verify the core stays platform-free

`Game` must never include SDL / Render / UI headers. Compile-only check (no link, no platform libs):

```
cmake --build build --target cookie_clicker_logic_check
```

Source of truth for commands: `cpp/CMakeLists.txt`. There is no CI, test suite, linter, formatter, or `opencode.json` — verification is the build plus the 3-second smoke run above.

## Architecture (portability target: DSi / 3DS)

- `Game` has no `draw()` and includes no `Render`/`UI`/platform headers. Core compiles with logic headers only (`cpp/Game.cpp` includes only `Game.h`, `SaveLoad.h`, `Building.h`, and STL).
- `UI` owns all layout, drawing, hit-testing. It takes `const Game&` and `Render&`. Layout numbers live in `UI.h`'s `Layout` struct, never inline in draw code.
- Input: `UI::on_pointer(x, y)` → `GameAction` → `main.cpp` applies it to `Game`. No hit-testing outside `UI`.
- Save format is byte-compatible with the browser game: pipe/semicolon text → UTF-8 → Base64 → `!END!`.
- Fonts: Kavoon for headings, PT Sans for body. Zero Tahoma references.

## Stubbed subsystems (known, out of scope — do not fix unless the task is about them)

`Audio.cpp`, `Assets.cpp` (no-op); `Game.cpp::updateSpecial/updateGrandmapocalypse/particlesUpdate` (empty); `SaveLoad.cpp` (minigame/ascension stubs); `Minigame.cpp`, Garden/Grimoire/Pantheon/Dungeons (stubs); `DungeonGen.cpp` (returns "Mysterious dungeon"); `Shimmer.cpp`/`Particle.cpp` (basic structs); `Building.cpp::modifyBuildingPrice` (not implemented); wrinkler sim beyond `cpsSucked`; buff behavior; floating cookie particles.

## Commit workflow (end of every task)

1. `cmake --build build -j` — stop and report if it fails to compile.
2. `timeout 3 ./build/cookie_clicker` — expect exit 124.
3. `git add` only the files you changed.
4. Commit: terse, **what changed and why**. One logical change per commit — never mix refactors, features, and asset changes.
5. `git push`. Never force-push or rewrite history. Never commit a broken build.
