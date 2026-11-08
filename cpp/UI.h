#pragma once

#include "Game.h"
#include "Render.h"
#include "Types.h"

#include <string>
#include <unordered_map>

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

// Every position / size / spacing used by the UI. Values mirror the current
// Game::draw() exactly so this refactor stays pixel-identical. A DSi/3DS port
// can supply its own Layout and reuse the same drawing code.
struct Layout {
    // --- panels ------------------------------------------------------------
    f32 leftPanelWidthFraction = 0.30f; // #sectionLeft width (fraction of window)
    i32 storePanelWidth = 318;          // #sectionRight / #store width (px)

    // --- cookie panel ------------------------------------------------------
    f32 cookieCountYFraction = 0.10f;   // #cookies top (fraction of window height)
    f32 cookieCountYOffset = -10.0f;
    f32 cookieCountScale = 1.5f;

    f32 bakeryYFraction = 0.10f;        // #bakeryName top
    f32 bakeryYOffset = 30.0f;
    f32 bakeryScale = 1.25f;
    u32 bakeryColor = 0xCCCCCCFF;

    f32 cpsYFraction = 0.10f;           // CPS line
    f32 cpsYOffset = 46.0f;
    f32 cpsScale = 0.8f;
    u32 cpsColor = 0xAAAAFFFF;

    // big cookie (#bigCookie 256x256, half-size 128)
    f32 bigCookieYFraction = 0.40f;
    f32 bigCookieHalfSize = 128.0f;
    f32 bigCookieMinSize = 0.4f;

    // --- middle panel ------------------------------------------------------
    i32 tabStripY = 90;                 // wood tab strip top
    i32 tabLabelY = 96;                 // tab label baseline
    f32 tabLabelScale = 0.9f;
    i32 tabLabelX[5] = {16, 110, 204, 298, 372};
    const char* tabLabelText[5] = {"Options", "Stats", "Legacy", "Info", "Store"};
    i32 numSections = 4;                // blank bands below the tab strip
    f32 bandMinHeight = 40.0f;

    // --- store panel -------------------------------------------------------
    i32 storeTitleX = 16;
    i32 storeTitleY = 48;
    f32 storeTitleScale = 1.75f;
    u32 storeTitleColor = 0xFFFFFFFF;

    i32 storeFirstRowY = 112;           // first row top (below title)
    i32 storeRowHeight = 64;            // tile height
    i32 storeRowStep = 72;              // 64px tile + 8px gap
    i32 storeTileWidth = 300;           // storeTile.png strip width
    i32 storeTileStripHeight = 64;      // one strip height

    i32 storeIconX = 0;                 // icon x relative to storeX
    i32 storeIconSize = 64;

    i32 storeNameX = 72;                // main name x (shadow drawn +1,+1)
    i32 storeNameY = 8;
    f32 storeNameScale = 1.2f;
    u32 storeNameColorEnabled = 0xFFFFFFFF;
    u32 storeNameColorDisabled = 0x888888FF;
    u32 storeNameShadowColor = 0x000000AA;

    i32 storePriceY = 8;
    f32 storePriceScale = 0.8f;
    u32 storePriceColorAffordable = 0x6FFF6FFF;
    u32 storePriceColorNot = 0xFF6666FF;
    i32 storePriceRightEdge = 76;       // storeX + storeInnerW - 76
    i32 storePriceIconGap = 6;

    i32 storeOwnedRightMargin = 12;
    f32 storeOwnedScale = 1.5f;
    u32 storeOwnedColor = 0x00000066;

    u32 storeSeparatorColor = 0x4A4A60FF;

    // --- colours -----------------------------------------------------------
    u32 middlePanelBgColor = 0x2A2A3AFF;
    u32 storePanelBgColor = 0x00000080;
    u32 bandColor = 0x3A3A4AFF;
    u32 tileColorEnabled = 0xFFFFFFFF;
    u32 tileColorDisabled = 0x555555FF;
};

class UI {
public:
    UI(const Game& game, Render& render, const Layout& layout = Layout());
    ~UI();

    // Draw one frame: background, cookie panel, middle panel, store panel.
    void draw();

    // Report a pointer click at (x, y). Returns the game action the main
    // loop should apply to Game. No hit-testing happens outside this class.
    GameAction on_pointer(i32 x, i32 y);

private:
    const Game& game_;
    Render& render_;
    Layout layout_;

    // texture handles (lazily loaded on first draw)
    i32 bgTex = 0;
    i32 cookieTex = 0;
    i32 panelHorizTex = 0;
    i32 panelVertTex = 0;
    i32 storeTileTex = 0;
    i32 priceCookieTex = 0;

    // building-icon texture cache (logical path -> handle)
    std::unordered_map<std::string, i32> iconTexCache;

    i32 loadIcon(const std::string& iconPath);
};
