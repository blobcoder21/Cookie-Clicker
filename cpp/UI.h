#pragma once

#include "Game.h"
#include "Render.h"
#include "Types.h"

#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// UI layer — owns all layout, drawing, and hit-testing.
//
// Game supplies state (const Game&). Render supplies platform primitives.
// UI never mutates Game. User input is reported back as a GameAction and the
// main loop applies it to Game.
//
// All layout numbers live in Layout so a platform (e.g. DSi two-screen) can
// supply a different Layout without touching drawing code.
// ---------------------------------------------------------------------------

struct GameAction {
    enum class Type : i32 { None, ClickCookie, BuyBuilding };
    Type type = Type::None;
    i32 buildingIndex = -1; // index into Game::objects (BuyBuilding only)
};

// A transient on-screen notification (toast). Skeleton: data + lifecycle
// only; styling and placement are intentionally minimal so they can be
// refined later without touching Game or Render.
struct Toast {
    std::string text;
    f32 life = 0.0f;      // seconds remaining
    f32 maxLife = 0.0f;   // total lifetime
    f32 y = 0.0f;         // screen y (top-down), set when spawned
    bool active = false;
};

// Every position / size / spacing used by the UI. Values mirror the current
// Game::draw() exactly so this refactor stays pixel-identical. A DSi/3DS port
// can supply its own Layout and reuse the same drawing code.
// Every position / size / spacing used by the UI. Values mirror the real
// Cookie Clicker (v2.058) layout measured from screenshots/refs/real_game.png
// (1920x919) and scaled to the 900x600 desktop window. A DSi/3DS port can
// supply its own Layout and reuse the same drawing code.
// ---------------------------------------------------------------------------
struct Layout {
    // --- panels ------------------------------------------------------------
    f32 leftPanelWidthFraction = 0.30f; // #sectionLeft width (fraction of window)
    i32 storePanelWidth = 318;          // #sectionRight / #store width (px)

    // --- top menu bar (wooden plank strip spanning from the left panel's
    //     right edge to the right edge, above the middle + store area) ----
    i32 topBarY = 0;                    // strip top
    i32 topBarH = 80;                   // strip height
    u32 topBarColor = 0xFFFFFFFF;       // wood tint

    i32 topBtnY = 26;                   // button top within the strip
    i32 topBtnH = 34;                   // button height
    i32 btnPadX = 14;                   // text padding inside a button
    i32 btnGap = 10;                    // gap between buttons / ticker

    // Button x positions are relative to the left panel's right edge (leftW)
    // for Options/Stats, or relative to the right edge for Info/Legacy.
    // Layout matches Image 2: Options+Stats stacked top-left,
    // Info+Legacy stacked top-right.
    i32 btnOptionsX = 16;
    i32 btnOptionsW = 110;
    i32 btnStatsX = 16;
    i32 btnStatsW = 110;
    i32 btnInfoX = 16;                  // from right edge: storeX + storeW - x - w
    i32 btnInfoW = 90;
    i32 btnLegacyX = 16;                // from right edge
    i32 btnLegacyW = 90;

    i32 newsTickerY = 44;               // news ticker baseline
    f32 newsTickerScale = 0.85f;
    u32 newsTickerColor = 0xDDDDDDFF;

    // --- cookie panel (left) ----------------------------------------------
    // Rounded banner with the bakery name (Kavoon heading).
    // bakeryYFraction is the banner visual-centre position (fraction of height).
    f32 bakeryYFraction = 0.097f;
    f32 bakeryScale = 0.7f;
    u32 bakeryColor = 0xF5F5F5FF;
    u32 bannerColor = 0x2A2A3AFF;       // banner background fill
    u32 bannerOutline = 0x8A8A8AFF;
    f32 bannerPadX = 10.0f;
    f32 bannerPadY = 3.0f;
    f32 bannerRadius = 6.0f;

    // "N cookies" large (PT Sans body). cookieCountYFraction is the visual centre.
    f32 cookieCountYFraction = 0.141f;
    f32 cookieCountScale = 1.1f;
    u32 cookieCountColor = 0xFFFFFFFF;

    // "per second: X" small beneath the count. cpsYFraction is the visual centre.
    f32 cpsYFraction = 0.163f;
    f32 cpsScale = 0.55f;
    u32 cpsColor = 0xC8C8C8FF;

    // big cookie (#bigCookie). Centered in the left panel.
    f32 bigCookieYFraction = 0.388f;
    f32 bigCookieHalfSize = 40.0f;   // half-width at scale 1.0 (matches real-game proportion)
    f32 bigCookieMinSize = 0.4f;

    // glow behind the cookie (drawn before the cookie).
    f32 cookieGlowScale = 2.6f;      // glow diameter / cookie diameter

    // --- middle panel (center) ---------------------------------------------
    // Game world view between the cookie panel and the store panel.
    // A cookie strip (building sprites) runs horizontally near the top,
    // with a green grass field beneath it.
    i32 middleCookieStripY = 128;    // top of the cookie-sprite strip
    i32 middleCookieStripH = 48;     // height of the sprite strip
    i32 middleGrassY = 180;          // top of the green grass field

    // --- store panel (right) ----------------------------------------------
    // Panel width is a fraction of the window, matching the real game
    // (store column ~16% of width). Store content (tile, icon, text) is
    // scaled at draw time by storeW / 304 (the real store panel width in px)
    // so everything fits and stays proportional.
    f32 storePanelWidthFraction = 0.158f;

    // Base sizes mirror the real game's store column (~304px wide at 1920px
    // window width). These are scaled by storeW/304 at draw time.
    i32 storeTitleX = 16;
    i32 storeTitleY = 44;
    f32 storeTitleScale = 1.7f;      // Kavoon heading "STORE"
    u32 storeTitleColor = 0xFFFFFFFF;

    // Upgrades row (icon slots) beneath the STORE title.
    i32 upgradesY = 96;
    i32 upgradeSlotW = 40;
    i32 upgradeSlotH = 40;
    i32 upgradeSlotGap = 8;
    i32 upgradeSlotX = 16;

    // Buy / Sell toggle beneath the upgrades row.
    i32 buySellY = 150;
    i32 buySellH = 26;
    i32 buySellW = 94;
    u32 buySellColor = 0xFFFFFFFF;
    u32 buySellActive = 0xFFD700FF;

    // Bulk labels "1 / 10 / 100" to the right of the Buy/Sell toggle.
    i32 bulkY = 150;
    f32 bulkScale = 0.8f;
    u32 bulkColor = 0xFFFFFF66;
    i32 bulkGap = 10;

    // Store rows: contiguous (no separation between elements), no frames.
    i32 storeFirstRowY = 198;        // first row top (below buy/sell)
    i32 storeRowHeight = 64;         // row height == row step (contiguous)
    i32 storeTileWidth = 300;        // storeTile.png strip width (base px)
    i32 storeTileStripHeight = 64;   // one strip height

    i32 storeIconX = 0;              // icon x relative to storeX (left edge of row cell)
    i32 storeIconSize = 64;          // icon fills the row height (base px)

    i32 storeNameX = 76;             // name x (64px icon + gap) (base px)
    f32 storeNameScale = 1.15f;
    u32 storeNameColorEnabled = 0xFFFFFFFF;
    u32 storeNameColorDisabled = 0x888888FF;

    // Price stacked below the name, with a small cookie icon before it.
    f32 storePriceScale = 0.9f;
    u32 storePriceColorAffordable = 0x6FFF6FFF;
    u32 storePriceColorNot = 0xFF6666FF;
    i32 storePriceIconGap = 6;

    // Big faded owned amount on the right.
    i32 storeOwnedRightMargin = 14;
    f32 storeOwnedScale = 1.5f;
    u32 storeOwnedColor = 0x00000066;

    // Locked item silhouette (dark shadowed).
    u32 storeLockedColor = 0x303030FF;

    // --- colours -----------------------------------------------------------
    u32 bandColor = 0x3A3A4AFF;
    u32 tileColorEnabled = 0xFFFFFFFF;
    u32 tileColorDisabled = 0x555555FF;

    // Grass field colour (middle panel bottom).
    u32 grassColor = 0x7FC94FFF;

    // --- toasts (skeleton) --------------------------------------------------
    f32 toastX = 16.0f;                  // toast area left edge (inside cookie panel)
    f32 toastYStart = 0.0f;              // set at draw time: bottom area of cookie panel
    f32 toastSpacing = 26.0f;            // vertical gap between toasts
    f32 toastScale = 0.8f;               // text scale
    f32 toastLife = 4.0f;                // seconds each toast stays on screen
    u32 toastColor = 0xFFFFFFFF;         // toast text color
    u32 toastBgColor = 0x000000B0;       // toast background (semi-transparent dark)
};


class UI {
public:
    UI(const Game& game, Render& render, const Layout& layout = Layout());
    ~UI();

    // Draw one frame: background, cookie panel, middle panel, store panel.
    void draw();

    // Update per-frame UI state (toast lifetimes, etc.). Called from the main
    // loop each fixed timestep.
    void update(f32 dt);

    // Report a pointer click at (x, y). Returns the game action the main
    // loop should apply to Game. No hit-testing happens outside this class.
    GameAction on_pointer(i32 x, i32 y);

    // Queue a transient on-screen notification (toast). Skeleton: text +
    // lifetime; styling/placement refined later.
    void notify(const std::string& text);

private:
    const Game& game_;
    Render& render_;
    Layout layout_;

    // texture handles (lazily loaded on first draw)
    i32 bgStripesTex = 0;
    i32 vignetteTex = 0;
    i32 glowTex = 0;
    i32 cookieTex = 0;
    i32 panelHorizTex = 0;
    i32 panelVertTex = 0;
    i32 storeTileTex = 0;
    i32 priceCookieTex = 0;

    // building-icon texture cache (logical path -> handle)
    std::unordered_map<std::string, i32> iconTexCache;

    // active toasts (skeleton)
    std::vector<Toast> toasts;

    i32 loadIcon(const std::string& iconPath);
};
