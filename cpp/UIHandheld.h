#pragma once

#include "UI.h"
#include "Types.h"

#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// UiHandheld — DSi dual-screen layout (LayoutHandheld).
//
// Screen 1 (top): Cookie panel. Screen 2 (bottom): tabbed Buildings / Store /
// Menu view. Layout numbers live in LayoutHandheld (top + bottom size each).
// ---------------------------------------------------------------------------
class UiHandheld : public UI {
public:
    enum class ScreenTab : i32 { Buildings = 0, Store, Menu };
    enum class MenuScreen : i32 { None, Options, Stats, Info };

    UiHandheld(const Game& game, Render& render, const LayoutHandheld& layout = LayoutHandheld());
    ~UiHandheld() override = default;

    void draw() override;
    void update(f32 dt) override;
    GameAction on_pointer(i32 x, i32 y) override;
    void notify(const std::string& text) override;

private:
    const Game& game_;
    Render& render_;
    LayoutHandheld layout_;

    ScreenTab tab_ = ScreenTab::Buildings;
    MenuScreen menu_ = MenuScreen::None;
    i32 scrollY_ = 0;
    i32 maxScroll_ = 0;

    i32 glowTex = 0;
    i32 cookieTex = 0;
    std::unordered_map<std::string, i32> iconTexCache;

    std::vector<Toast> toasts;

    struct MenuRow {
        i32 id = 0;
        enum class Kind : i32 { Picker, Toggle, Action };
        Kind kind = Kind::Picker;
        i32 y = 0;
        std::string label;
        std::string prefName;
        GameAction::Type action = GameAction::Type::None;
        bool value = false;
    };
    std::vector<MenuRow> menuRows_;

    i32 screenX() const;
    i32 screen2Y() const;
    i32 loadIcon(const std::string& iconPath);

    void drawCookieScreen(i32 x0, i32 y0);
    void drawTabScreen(i32 x0, i32 y0);
    void drawTextCentered(i32 x0, i32 yc, i32 w, const char* text, u32 color,
                          f32 scale, InternalFont font);
    void drawArrow(i32 x, i32 y, bool on, const char* glyph);

    i32 contentHeight(i32 contentStart);
    i32 fillMenuRows(i32 contentStart);
    void drawScreen2Content(i32 x0, i32 contentTop, i32 contentBottom,
                            i32 contentRight, i32 padX, i32 rowW);

    GameAction handleMenuRow(i32 index);
};
