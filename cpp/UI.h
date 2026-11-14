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
// This port targets the DSi (two 256x192 screens) and follows the original's
// `.mobile` single-panel layout: screen 1 shows the Cookie panel, screen 2
// shows a tabbed Buildings / Store / Menu view. All layout numbers live in
// Layout so the drawing code never contains inline geometry.
//
// Game supplies state (const Game&). Render supplies platform primitives.
// UI never mutates Game. User input is reported back as a GameAction and the
// main loop applies it to Game. Menu state (which tab/menu is open) lives in
// the UI, not in Game.
// ---------------------------------------------------------------------------

struct GameAction {
    enum class Type : i32 {
        None,
        ClickCookie,    // screen 1: click the big cookie
        BuyBuilding,    // screen 2: buy a building; buildingIndex into Game::objects
        ClickFortune,   // ticker fortune click (not used in the .mobile layout)
        TogglePref,     // toggle a Game preference; prefName names it
        SaveGame,       // trigger a manual save
        WipeSave,       // hard reset
    };
    Type type = Type::None;
    i32 buildingIndex = -1; // index into Game::objects (BuyBuilding only)
    std::string prefName;   // preference name (TogglePref only)
};

// A transient on-screen notification (toast). Skeleton: data + lifecycle
// only; styling and placement are intentionally minimal so they can be
// refined later without touching Game or Render.
struct Toast {
    std::string text;
    f32 life = 0.0f;      // seconds remaining
    f32 maxLife = 0.0f;   // total lifetime
    bool active = false;
};

// Every position / size / spacing used by the UI. Values mirror the original
// game's .mobile layout scaled to the DSi's two 256x192 screens. A different
// platform can supply its own Layout and reuse the same drawing code.
struct Layout {
    // --- DSi dual-screen geometry -------------------------------------------
    // Two 256x192 screens stacked vertically (top = Cookie, bottom = tabbed
    // Buildings / Store / Menu) with a bezel gap between them.
    i32 screenW = 256;
    i32 screenH = 192;
    i32 screenGap = 24;

    // screen 1 (top): Cookie panel ------------------------------------------
    f32 bakeryYFraction = 0.09f;   // bakery name visual centre (fraction of screen height)
    f32 bakeryScale = 0.55f;
    u32 bakeryColor = 0xE8E8E8FF;

    f32 cookieCountYFraction = 0.30f;
    f32 cookieCountScale = 0.9f;
    u32 cookieCountColor = 0xFFFFFFFF;

    f32 cpsYFraction = 0.47f;
    f32 cpsScale = 0.55f;
    u32 cpsColor = 0xC8C8C8FF;

    f32 bigCookieYFraction = 0.80f;
    f32 bigCookieHalfSize = 40.0f; // cookie radius at scale 1.0 (px)
    f32 bigCookieMinSize = 0.5f;
    f32 cookieGlowScale = 2.6f;    // glow diameter / cookie diameter

    // screen 2 (bottom): tab bar --------------------------------------------
    i32 tabBarH = 40;
    u32 tabBarColor = 0x2A2A3AFF;   // inactive tab background
    u32 tabActiveColor = 0xFFD700FF; // active tab background / accent
    u32 tabTextColor = 0xE0E0E0FF;
    u32 tabTextActiveColor = 0x000000FF;
    f32 tabTextScale = 0.6f;

    // screen 2: content panel -----------------------------------------------
    i32 contentPadX = 8;            // horizontal padding inside the content area
    i32 contentTitleY = 6;          // panel title baseline below the screen top
    f32 contentTitleScale = 0.7f;   // Kavoon heading ("BUILDINGS", "STORE", "MENU")
    u32 contentTitleColor = 0xFFFFFFFF;
    u32 screenBgColor = 0x1E1E24FF; // screen background
    u32 bezelColor = 0x101014FF;    // bezel gap between the two screens

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
    u32 rowLockedColor = 0x353535FF;   // locked-item silhouette
    u32 rowOwnedColor = 0xC8C8C8FF;    // "xN" owned count
    u32 rowBgColor = 0x26262CFF;       // row background
    u32 rowHighlightColor = 0x3A3A45FF; // buyable row background

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

class UI {
public:
    // Screen 2 tabs: Buildings / Store / Menu. Cookie has screen 1 to itself.
    enum class ScreenTab : i32 { Buildings = 0, Store, Menu };

    // Menus opened from the Menu tab. UI state only: Game knows nothing of it.
    enum class MenuScreen : i32 { None, Options, Stats, Info };

    UI(const Game& game, Render& render, const Layout& layout = Layout());
    ~UI();

    // Draw one frame: both DSi screens, cookie panel, tabbed view, toasts.
    void draw();

    // Update per-frame UI state (toast lifetimes, etc.). Called from the main
    // loop each fixed timestep.
    void update(f32 dt);

    // Report a pointer click at (x, y) in window coordinates. Returns the game
    // action the main loop should apply to Game. No hit-testing happens outside
    // this class; UI-only input (tabs, scroll, menu picking) is handled here.
    GameAction on_pointer(i32 x, i32 y);

    // Queue a transient on-screen notification (toast).
    void notify(const std::string& text);

private:
    const Game& game_;
    Render& render_;
    Layout layout_;

    // screen 2 state (UI-only, never in Game)
    ScreenTab tab_ = ScreenTab::Buildings;
    MenuScreen menu_ = MenuScreen::None;
    i32 scrollY_ = 0;      // screen 2 content scroll offset (px)
    i32 maxScroll_ = 0;    // clamped scroll range, (re)computed at draw time

    // texture handles (lazily loaded on first draw)
    i32 glowTex = 0;
    i32 cookieTex = 0;

    // building-icon texture cache (logical path -> handle)
    std::unordered_map<std::string, i32> iconTexCache;

    // active toasts
    std::vector<Toast> toasts;

    // menu rows drawn this frame (id 4000 + index); on_pointer uses them to
    // interpret menu clicks consistently with what was drawn.
    struct MenuRow {
        i32 id = 0;
        enum class Kind : i32 { Picker, Toggle, Action };
        Kind kind = Kind::Picker;
        i32 y = 0;            // absolute row top, before scrolling
        std::string label;
        std::string prefName; // Toggle only
        GameAction::Type action = GameAction::Type::None; // Action only
        bool value = false;   // Toggle current state
    };
    std::vector<MenuRow> menuRows_;

    // geometry
    i32 screenX() const;
    i32 screen2Y() const;

    // drawing helpers
    void drawCookieScreen(i32 x0, i32 y0);
    void drawTabScreen(i32 x0, i32 y0);
    void drawTextCentered(i32 x0, i32 yc, i32 w, const char* text, u32 color,
                          f32 scale, InternalFont font);
    void drawArrow(i32 x, i32 y, bool on, const char* glyph);

    // screen 2 content measurement / drawing
    i32 contentHeight(i32 contentStart);          // content height below the title
    i32 fillMenuRows(i32 contentStart);           // menu tab: fill menuRows_, return end y
    void drawScreen2Content(i32 x0, i32 contentTop, i32 contentBottom,
                            i32 contentRight, i32 padX, i32 rowW);

    // menu row handling (returns a GameAction for toggle/save/wipe, None otherwise)
    GameAction handleMenuRow(i32 index);

    i32 loadIcon(const std::string& iconPath);
};
