# Progress Log — Cookie Clicker C++ Port

## Current focus: SaveLoad — faithful 1:1 port of Cookie Clicker 2.058 save code

### Source of truth
- `ref/main_2058.js` — the real Cookie Clicker 2.058 main.js (downloaded from
  https://orteil.dashnet.org/cookieclicker/main.js; byte-identical to the repo
  copy). `Game.WriteSave` (line 2882) and `Game.LoadSave` (line 3124) ported
  line-by-line, original names and order kept in comments.
- `tests/saves/v2058.txt` and `tests/saves/v1_0393.txt` — fixtures in the outer
  storage encoding (`escape(utf8_to_b64(raw + "!END!"))`).

### Ported (SaveLoad)
**Game.WriteSave** (all sections, original order, JS number formatting):
- section 0: `Game.version`; section 1: reserved empty.
- section 2: `startDate;fullDate;lastDate;bakeryName;seed;appearance|`.
- section 3: 27-bit prefs bitstring (pack3 = identity).
- section 4: 55 misc fields (0..54). `.toString()` floats use shortest
  round-trip (`jsNumberToString`); `parseInt(Math.floor(x))` fields use floor;
  `parseInt(x)` fields (heralds/fortuneGC/fortuneCPS) use truncate.
- section 5: buildings `amount,bought,floor(totalCookies),level,minigameSave,muted,highest;`.
- section 6: upgrades bitstring (unlocked,bought pairs, vanilla only).
- section 7: achievements bitstring (won, vanilla only).
- section 8: buffs `type.id,maxTime,time,arg1,arg2,arg3;` (frames).
- section 9: mod data (`Game.saveModData()` stubbed → "").
- `Game.lastDate = parseInt(Game.time)` at entry (injected-clock behaviour).
- **Number formatting**: new `cpp/jsnum.h` with `jsNumberToString()` matching JS
  `Number.prototype.toString()` exactly (shortest round-trip; exponential iff
  `x < 1e-6` or `x >= 1e21`; plain signed exponent). Verified against Node on
  5044 values including MAX_VALUE/MIN_VALUE/MAX_SAFE_INTEGER — 0 mismatches.
- **Outer encoding**: `encodeForStorage`/`decodeFromStorage` match JS
  `escape(utf8_to_b64(raw)+"!END!")`; terminator + whitespace stripped before
  decode. Verified byte-identical to JS type-1 output.

**Game.LoadSave** (every version-gated branch, parsed separately from side effects):
- `parseSave()` (platform-free, no Game/UI): returns `SaveData` or nullopt on
  truncated/garbage/wrong-version input; game untouched on failure.
- `applySave()`: side effects; stubs where a subsystem isn't built, but raw
  values still stored and re-emitted unchanged.
- Version-gated prefs/upgrades/achievements unpacking:
  - `<1.0503` prefs: `split('')`; `1.0503–2.0046`: `unpack2()`; `>=2.0046`: `split('')`.
  - `<1.035` old non-binary; `1.035–1.0502` old awful packing
    (`UncompressLargeBin` for `<1.05`, `unpack` for `1.05–1.0502`);
    `>=1.0502`: `unpack2()`/`split('')`.
  - `unpack`, `unpack2`, `UncompressBin`, `UncompressLargeBin` ported.
- Prestige always recomputed: `floor(cbrt(cookiesReset/1e12))`.
- Version migrations (`<=1.0466` → heavenlyChips=prestige; `==1.9` → refund).
- Season/milk clamping, wrinkler LoadWrinklers semantics (shinies first),
  buildings/upgrades/achievements/buffs/mod-data application.
- Post-parse side effects (CalculateGains, offline catch-up, loadLumps,
  killShimmers, etc.) stubbed; no tick between load & write in tests, so saved
  fields round-trip byte-identically.

**Port-only data removed from the real string**: the old `ascensions` field at
stats field 51 collided with the real field 51 (`cookiesPsRawHighest`). It is no
longer serialized; `Game.ascensions` is kept as port-only sidecar state.
Field 51 now writes `cookiesPsRawHighest`.

### Game data model changes
- `Game::VERSION = 2.058`.
- Added: `appearance`, `ascensionMode`, `cookiesPsRawHighest`, `volumeMusic`,
  `cookiesSent`, `cookiesReceived`, prefs `cloudSave/bgMusic/notScary/fullscreen/screenreader/discordPresence`.
- `Building`: added `minigameSave` (raw string, re-emitted on write) and
  `highest` (building entry field 6).
- `Game.modSaveData` map added.

### Stubbed (parse + store + re-emit, no simulation)
Minigames (Garden/Grimoire/Pantheon/Dungeons save strings), wrinkler/shimmer
simulation, buff effects, seasons, dragons, lumps maturation, CalculateGains,
offline catch-up, loadLumps, mod effects, YouCustomizer (appearance echoed),
saveModData. These are parsed and re-emitted unchanged so nothing is dropped.

### Verification
- `cpp/SaveLoad.cpp` + logic sources compile clean (build + `cookie_clicker_logic_check`).
- `timeout 3 ./build/cookie_clicker` → exit 124 (expected).
- Golden test vs real `ref/main_2058.js` under Node:
  - C++ `writeSave(state)` == JS `WriteSave(2)(state)` — byte-identical (381 B).
  - C++ `encodeForStorage(raw)` == JS `escape(b64(raw+"!END!"))` — byte-identical (517 B).
  - C++ `loadSave(JS raw)` → `writeSave` == JS raw — byte-identical.
- `tests/saves/v2058.txt`: decode+load+write reproduces the decoded raw.
- `tests/saves/v1_0393.txt`: legacy save (UncompressLargeBin upgrades/achievements,
  no appearance field) loads without crashing, converts to 2.058 form, and that
  output is idempotent on reload.
- Truncated/garbage/wrong-version input → `parseSave` returns nullopt, game
  untouched, no crash.

### Not yet reproduced exactly
- Nothing known: every branch in `WriteSave`/`LoadSave` was ported. The only
  deviations are deliberate stubs of not-yet-built subsystems (minigame
  simulation, buff effects, etc.), which still parse/store/re-emit their raw
  values.

### Files modified
- `cpp/SaveLoad.h` — `SaveData` struct; `parseSave`/`applySave` separation.
- `cpp/SaveLoad.cpp` — full v2.058 WriteSave/LoadSave port + `jsnum.h` formatting.
- `cpp/jsnum.h` (new) — `jsNumberToString`/`jsParseFloat`/`jsParseInt`.
- `cpp/Game.h` — version 2.058 + missing fields.
- `cpp/Building.h` — `minigameSave`, `highest`.
- `cpp/CMakeLists.txt` — added `jsnum.h`.
- `tests/saves/v2058.txt`, `tests/saves/v1_0393.txt` (new fixtures).
- `ref/main_2058.js` (new source of truth).
