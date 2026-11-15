#pragma once

#include "Game.h"
#include "Render.h"
#include "Types.h"

#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// UI layer — owns all layout, drawing, and hit-testing.
//
// Game supplies state (const Game&). Render supplies platform primitives.
// UI never mutates Game. User input is reported back as a GameAction and the
// main loop applies it to Game. Menu state (which tab/menu is open) lives in
// the UI, not in Game.
//
// Two UI profiles share this common interface:
//   UiDesktop  -> 1280x720 three-panel desktop layout (LayoutDesktop)
//   UiHandheld -> DSi dual-screen layout (LayoutHandheld: top + bottom screen)
// A CMake option (UI_PROFILE) picks which profile main() builds.
// ---------------------------------------------------------------------------

struct GameAction {
    enum class Type : i32 {
        None,
        ClickCookie,    // click the big cookie
        BuyBuilding,    // buy a building; buildingIndex into Game::objects
        ClickFortune,   // ticker fortune click (desktop)
        TogglePref,     // toggle a Game preference; prefName names it (handheld)
        SaveGame,       // trigger a manual save (handheld)
        WipeSave,       // hard reset (handheld)
    };
    Type type = Type::None;
    i32 buildingIndex = -1; // index into Game::objects (BuyBuilding only)
    std::string prefName;   // preference name (TogglePref only)
};

// A transient on-screen notification (toast). Skeleton: data + lifecycle
// only; styling and placement are intentionally minimal.
struct Toast {
    std::string text;
    f32 life = 0.0f;      // seconds remaining
    f32 maxLife = 0.0f;   // total lifetime
    f32 y = 0.0f;         // screen y (top-down), set when spawned
    bool active = false;
};

// Common UI interface. Concrete profiles: UiDesktop, UiHandheld.
class UI {
public:
    virtual ~UI() = default;

    // Draw one frame.
    virtual void draw() = 0;

    // Update per-frame UI state (toast lifetimes, etc.).
    virtual void update(f32 dt) = 0;

    // Report a pointer click at (x, y) in window coordinates. Returns the game
    // action the main loop should apply to Game. No hit-testing happens outside
    // this class.
    virtual GameAction on_pointer(i32 x, i32 y) = 0;

    // Queue a transient on-screen notification (toast).
    virtual void notify(const std::string& text) = 0;
};

// UI profile selected at build time via the UI_PROFILE CMake option.
enum class UIProfile { Desktop, Ds, ThreeDs };

// Build the UI for the given profile. Defined in UI.cpp.
std::unique_ptr<UI> CreateUI(UIProfile profile, const Game& game, Render& render);

// ---------------------------------------------------------------------------
// Per-profile layout structs. All drawing numbers live here, never inline.
// ---------------------------------------------------------------------------

// Desktop layout (1280x720 restored, three panels: cookie / middle / store).
struct LayoutDesktop {
    // --- panels ------------------------------------------------------------
    f32 leftPanelWidthFraction = 0.30f; // #sectionLeft width (fraction of window)
    i32 storePanelWidth = 318;          // #sectionRight / #store width (px)

    // --- top menu bar ------------------------------------------------------
    i32 topBarY = 0;
    i32 topBarH = 80;
    u32 topBarColor = 0xFFFFFFFF;

    i32 topBtnY = 26;
    i32 topBtnH = 34;
    i32 btnPadX = 14;
    i32 btnGap = 10;

    i32 btnOptionsX = 16;
    i32 btnOptionsW = 110;
    i32 btnStatsX = 16;
    i32 btnStatsW = 110;
    i32 btnInfoX = 16;
    i32 btnInfoW = 90;
    i32 btnLegacyX = 16;
    i32 btnLegacyW = 90;

    i32 newsTickerY = 44;
    f32 newsTickerScale = 0.85f;
    u32 newsTickerColor = 0xDDDDDDFF;

    // --- cookie panel (left) ----------------------------------------------
    f32 bakeryYFraction = 0.097f;
    f32 bakeryScale = 0.7f;
    u32 bakeryColor = 0xF5F5F5FF;
    u32 bannerColor = 0x2A2A3AFF;
    u32 bannerOutline = 0x8A8A8AFF;
    f32 bannerPadX = 10.0f;
    f32 bannerPadY = 3.0f;
    f32 bannerRadius = 6.0f;

    f32 cookieCountYFraction = 0.141f;
    f32 cookieCountScale = 1.1f;
    u32 cookieCountColor = 0xFFFFFFFF;

    f32 cpsYFraction = 0.163f;
    f32 cpsScale = 0.55f;
    u32 cpsColor = 0xC8C8C8FF;

    f32 bigCookieYFraction = 0.388f;
    f32 bigCookieHalfSize = 40.0f;
    f32 bigCookieMinSize = 0.4f;
    f32 cookieGlowScale = 2.6f;

    // --- middle panel (center) --------------------------------------------
    i32 middleCookieStripY = 128;
    i32 middleCookieStripH = 48;
    i32 middleGrassY = 180;

    // --- store panel (right) ----------------------------------------------
    f32 storePanelWidthFraction = 0.158f;

    i32 storeTitleX = 16;
    i32 storeTitleY = 44;
    f32 storeTitleScale = 1.7f;
    u32 storeTitleColor = 0xFFFFFFFF;

    i32 upgradesY = 96;
    i32 upgradeSlotW = 40;
    i32 upgradeSlotH = 40;
    i32 upgradeSlotGap = 8;
    i32 upgradeSlotX = 16;

    i32 buySellY = 150;
    i32 buySellH = 26;
    i32 buySellW = 94;
    u32 buySellColor = 0xFFFFFFFF;
    u32 buySellActive = 0xFFD700FF;

    i32 bulkY = 150;
    f32 bulkScale = 0.8f;
    u32 bulkColor = 0xFFFFFF66;
    i32 bulkGap = 10;

    i32 storeFirstRowY = 198;
    i32 storeRowHeight = 64;
    i32 storeTileWidth = 300;
    i32 storeTileStripHeight = 64;

    i32 storeIconX = 0;
    i32 storeIconSize = 64;

    i32 storeNameX = 76;
    f32 storeNameScale = 1.15f;
    u32 storeNameColorEnabled = 0xFFFFFFFF;
    u32 storeNameColorDisabled = 0x888888FF;

    f32 storePriceScale = 0.9f;
    u32 storePriceColorAffordable = 0x6FFF6FFF;
    u32 storePriceColorNot = 0xFF6666FF;
    i32 storePriceIconGap = 6;

    i32 storeOwnedRightMargin = 14;
    f32 storeOwnedScale = 1.5f;
    u32 storeOwnedColor = 0x00000066;

    u32 storeLockedColor = 0x303030FF;

    // --- colours -----------------------------------------------------------
    u32 bandColor = 0x3A3A4AFF;
    u32 tileColorEnabled = 0xFFFFFFFF;
    u32 tileColorDisabled = 0x555555FF;
    u32 grassColor = 0x7FC94FFF;

    // --- toasts (skeleton) --------------------------------------------------
    f32 toastX = 16.0f;
    f32 toastYStart = 0.0f;
    f32 toastSpacing = 26.0f;
    f32 toastScale = 0.8f;
    f32 toastLife = 4.0f;
    u32 toastColor = 0xFFFFFFFF;
    u32 toastBgColor = 0x000000B0;
};

// Handheld / DSi dual-screen layout. Two screens (top + bottom), each with
// its own size, separated by a bezel gap.
struct LayoutHandheld {
    // --- screen geometry ----------------------------------------------------
    i32 screenW = 256;
    i32 topScreenH = 192;    // screen 1: Cookie panel
    i32 bottomScreenH = 192; // screen 2: tabbed Buildings / Store / Menu
    i32 screenGap = 24;

    // screen 1 (top): Cookie panel ------------------------------------------
    f32 bakeryYFraction = 0.09f;
    f32 bakeryScale = 0.55f;
    u32 bakeryColor = 0xE8E8E8FF;

    f32 cookieCountYFraction = 0.30f;
    f32 cookieCountScale = 0.9f;
    u32 cookieCountColor = 0xFFFFFFFF;

    f32 cpsYFraction = 0.47f;
    f32 cpsScale = 0.55f;
    u32 cpsColor = 0xC8C8C8FF;

    f32 bigCookieYFraction = 0.80f;
    f32 bigCookieHalfSize = 40.0f;
    f32 bigCookieMinSize = 0.5f;
    f32 cookieGlowScale = 2.6f;

    // screen 2 (bottom): tab bar --------------------------------------------
    i32 tabBarH = 40;
    u32 tabBarColor = 0x2A2A3AFF;
    u32 tabActiveColor = 0xFFD700FF;
    u32 tabTextColor = 0xE0E0E0FF;
    u32 tabTextActiveColor = 0x000000FF;
    f32 tabTextScale = 0.6f;

    // screen 2: content panel -----------------------------------------------
    i32 contentPadX = 8;
    i32 contentTitleY = 6;
    f32 contentTitleScale = 0.7f;
    u32 contentTitleColor = 0xFFFFFFFF;
    u32 screenBgColor = 0x1E1E24FF;
    u32 bezelColor = 0x101014FF;

    // rows shared by the Buildings and Store panels
    i32 rowH = 30;
    i32 rowGap = 2;
    i32 rowIconSize = 24;
    i32 rowIconX = 6;
    f32 rowNameScale = 0.5f;
    u32 rowNameColor = 0xFFFFFFFF;
    u32 rowNameColorLocked = 0x8A8A8AFF;
    f32 rowPriceScale = 0.5f;
    u32 rowPriceColorAfford = 0x6FFF6FFF;
    u32 rowPriceColorNot = 0xFF6666FF;
    u32 rowLockedColor = 0x353535FF;
    u32 rowOwnedColor = 0xC8C8C8FF;
    u32 rowBgColor = 0x26262CFF;
    u32 rowHighlightColor = 0x3A3A45FF;

    // menu (Options / Stats / Info)
    f32 menuTextScale = 0.55f;
    u32 menuTextColor = 0xE0E0E0FF;
    i32 menuItemH = 28;
    u32 menuItemBg = 0x26262CFF;
    u32 checkboxOn = 0xFFD700FF;
    u32 checkboxOff = 0x4A4A4AFF;

    // scroll arrows (top-right / bottom-right of the content area)
    i32 arrowW = 20;
    i32 arrowH = 20;
    f32 arrowTextScale = 0.7f;
    u32 arrowColor = 0xC8C8C8FF;
    u32 arrowColorDisabled = 0x4A4A4AFF;

    // toasts: stacked upward from the bottom-left of screen 1
    f32 toastX = 8.0f;
    f32 toastSpacing = 24.0f;
    f32 toastScale = 0.6f;
    f32 toastLife = 4.0f;
    u32 toastColor = 0xFFFFFFFF;
    u32 toastBgColor = 0x000000B0;
};

// ---------------------------------------------------------------------------
// Display formatting for cookie counts and prices (uppercase suffixes).
// Ported verbatim from Game.cpp (display-only helpers shared by both profiles).
// ---------------------------------------------------------------------------

namespace {
inline std::string cleanNum(std::string s) {
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        while (s.size() > dot + 1 && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

inline std::string commaInt(std::string s) {
    size_t dot = s.find('.');
    std::string intPart = (dot == std::string::npos) ? s : s.substr(0, dot);
    std::string frac = (dot == std::string::npos) ? std::string() : s.substr(dot);
    std::string out;
    i32 count = 0;
    for (i32 i = static_cast<i32>(intPart.size()) - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) out.insert(out.begin(), ',');
        out.insert(out.begin(), intPart[static_cast<size_t>(i)]);
        ++count;
    }
    out += frac;
    return out;
}

inline std::string formatCookies(f64 v) {
    if (v < 0.0) v = 0.0;
    static const char* suf[] = {"", "K", "M", "B", "T", "Qa", "Qt", "Sx",
                                "Sp", "Oc", "No"};
    i32 s = 0;
    f64 x = v;
    while (x >= 1000.0 && s < 10) {
        x /= 1000.0;
        ++s;
    }
    char num[32];
    if (s == 0) {
        if (v < 1000.0)
            std::snprintf(num, sizeof(num), "%.2f", v);
        else
            std::snprintf(num, sizeof(num), "%lld",
                          static_cast<long long>(std::floor(v)));
    } else {
        std::snprintf(num, sizeof(num), "%.2f", x);
    }
    std::string out = commaInt(cleanNum(std::string(num)));
    if (s > 0) out += suf[s];
    return out;
}
} // namespace
