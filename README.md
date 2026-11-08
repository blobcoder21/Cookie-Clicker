# Cookie-Clicker

The full master source code for the famous browser game Cookie Clicker, now on PC natively.

## Credits & Attribution

This project is inspired by and based on **Cookie Clicker**, originally created by **Orteil (Julien Thiennot)**.

- Original game: [Cookie Clicker](https://orteil.dashnet.org/cookieclicker/)
- Creator: [Orteil](https://orteil.dashnet.org/)
- Studio: [DashNet](https://www.dashnet.org/)

Cookie Clicker, its original artwork, assets, characters, and associated content remain the property of their respective copyright holders.

This project is an independent desktop implementation and is not affiliated with, endorsed by, or officially supported by Orteil or DashNet.

All original implementation code in this project is credited to its respective contributors.

## Architecture

The codebase separates game logic from platform rendering through a `Render` abstraction. Work in progress: extracting a dedicated **UI layer** (layout + hit-testing) out of `Game` so the core stays portable to DSi/3DS without remaking the interface for each platform's screen layout.

## Commit & build workflow

At the end of every task:
1. Run the build (`cmake --build build -j`). If it fails to compile, stop and report it.
2. Run the task's checks (`timeout 3 ./build/cookie_clicker` → expect exit 124).
3. `git add` only the files you changed.
4. Commit with a terse message saying **what changed and why**. One logical change per commit — never mix refactors, features, and asset changes.
5. `git push`. Never force-push or rewrite history. Never commit a broken build.
