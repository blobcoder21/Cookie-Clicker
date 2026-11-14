#include "UI.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

// ---------------------------------------------------------------------------
// Display formatting for cookie counts and prices (uppercase suffixes).
// Ported verbatim from Game.cpp (these helpers are display-only, so they live
// in the UI layer now).
// ---------------------------------------------------------------------------

// Strip trailing zeros (and a dangling decimal point) from a formatted number.
std::string cleanNum(std::string s) {
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        while (s.size() > dot + 1 && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

// Insert thousands separators into the integer part of a number string.
std::string commaInt(std::string s) {
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

std::string formatCookies(f64 v) {
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

UI::UI(const Game& game, Render& render, const Layout& layout)
    : game_(game), render_(render), layout_(layout) {}

UI::~UI() = default;

i32 UI::loadIcon(const std::string& iconPath) {
    auto it = iconTexCache.find(iconPath);
    if (it != iconTexCache.end()) return it->second;
    i32 handle = render_.loadTexture(iconPath);
    iconTexCache[iconPath] = handle;
    return handle;
}

void UI::update(f32 dt) {
    // Decrement toast lifetimes and drop expired ones.
    for (auto it = toasts.begin(); it != toasts.end();) {
        it->life -= dt;
        if (it->life <= 0.0f) {
            it = toasts.erase(it);
        } else {
            ++it;
        }
    }
}

void UI::notify(const std::string& text) {
    Toast t;
    t.text = text;
    t.life = layout_.toastLife;
    t.maxLife = layout_.toastLife;
    t.active = true;
    toasts.push_back(std::move(t));
}

i32 UI::screenX() const {
    return (static_cast<i32>(render_.windowW()) - layout_.screenW) / 2;
}

i32 UI::screen2Y() const {
    return layout_.screenH + layout_.screenGap;
}

void UI::drawTextCentered(i32 x0, i32 yc, i32 w, const char* text, u32 color,
                          f32 scale, InternalFont font) {
    f32 tw = render_.measureText(text, scale, font);
    f32 ty = static_cast<f32>(yc) - render_.measureTextCenterY(text, scale, font);
    render_.drawText(static_cast<f32>(x0) + (static_cast<f32>(w) - tw) / 2.0f, ty,
                     text, color, scale, font);
}

void UI::drawArrow(i32 x, i32 y, bool on, const char* glyph) {
    const InternalFont body = InternalFont::Body;
    render_.drawRect(static_cast<f32>(x), static_cast<f32>(y),
                     static_cast<f32>(layout_.arrowW), static_cast<f32>(layout_.arrowH),
                     on ? layout_.arrowColor : layout_.arrowColorDisabled);
    f32 tw = render_.measureText(glyph, layout_.arrowTextScale, body);
    f32 ty = static_cast<f32>(y) + static_cast<f32>(layout_.arrowH) / 2.0f
        - render_.measureTextCenterY(glyph, layout_.arrowTextScale, body);
    render_.drawText(static_cast<f32>(x) + (static_cast<f32>(layout_.arrowW) - tw) / 2.0f, ty,
                     glyph, on ? 0x000000FF : 0x2A2A2AFF, layout_.arrowTextScale, body);
}

void UI::draw() {
    i32 w = render_.windowW();
    i32 h = render_.windowH();

    if (!glowTex) glowTex = render_.loadTexture("glow.png");
    if (!cookieTex) cookieTex = render_.loadTexture("perfectCookie.png");

    i32 x0 = screenX();

    // Bezel gap between the two DSi screens.
    render_.drawRect(0.0f, static_cast<f32>(layout_.screenH),
                     static_cast<f32>(w), static_cast<f32>(layout_.screenGap),
                     layout_.bezelColor);

    // Screen 1 (top): Cookie panel.
    drawCookieScreen(x0, 0);
    // Screen 2 (bottom): tabbed Buildings / Store / Menu view.
    drawTabScreen(x0, screen2Y());
}

void UI::drawCookieScreen(i32 x0, i32 y0) {
    i32 w = layout_.screenW;
    i32 h = layout_.screenH;
    const InternalFont heading = InternalFont::Default; // Kavoon
    const InternalFont body = InternalFont::Body;       // PT Sans

    render_.drawRect(static_cast<f32>(x0), static_cast<f32>(y0),
                     static_cast<f32>(w), static_cast<f32>(h), layout_.screenBgColor);

    // Bakery name (Kavoon heading).
    drawTextCentered(x0, y0 + static_cast<i32>(static_cast<f32>(h) * layout_.bakeryYFraction), w,
                     game_.getBakeryName().c_str(), layout_.bakeryColor,
                     layout_.bakeryScale, heading);

    // Cookie count ("N cookies").
    std::string cookieStr = formatCookies(game_.cookies) + " cookies";
    drawTextCentered(x0, y0 + static_cast<i32>(static_cast<f32>(h) * layout_.cookieCountYFraction), w,
                     cookieStr.c_str(), layout_.cookieCountColor,
                     layout_.cookieCountScale, body);

    // CPS ("per second: X"), styled differently when wrinklers suck CpS.
    f64 cps = game_.cookiesPs * (1.0 - game_.cpsSucked);
    std::string cpsStr = "per second: " + formatCookies(cps);
    u32 cpsColor = game_.cpsSucked > 0.0 ? 0xFF6666FF : layout_.cpsColor;
    drawTextCentered(x0, y0 + static_cast<i32>(static_cast<f32>(h) * layout_.cpsYFraction), w,
                     cpsStr.c_str(), cpsColor, layout_.cpsScale, body);

    // Big cookie with soft glow behind it.
    f64 size = game_.bigCookieSizeValue();
    if (size < layout_.bigCookieMinSize) size = layout_.bigCookieMinSize;
    i32 cr = static_cast<i32>(layout_.bigCookieHalfSize * size);
    i32 cx = x0 + w / 2;
    i32 cy = y0 + static_cast<i32>(static_cast<f32>(h) * layout_.bigCookieYFraction);
    f32 glowDia = static_cast<f32>(cr) * 2.0f * layout_.cookieGlowScale;
    render_.drawTexture(glowTex,
                        static_cast<f32>(cx) - glowDia / 2.0f,
                        static_cast<f32>(cy) - glowDia / 2.0f,
                        glowDia, glowDia, 0xFFFFFFFF);
    render_.drawTexture(cookieTex,
                        static_cast<f32>(cx - cr), static_cast<f32>(cy - cr),
                        static_cast<f32>(cr) * 2.0f, static_cast<f32>(cr) * 2.0f,
                        0xFFFFFFFF);
    render_.registerCookie(static_cast<f32>(cx), static_cast<f32>(cy),
                           static_cast<f32>(cr));

    // Toasts stacked upward from the bottom-left of screen 1.
    const InternalFont toastFont = InternalFont::Body;
    f32 toastX = static_cast<f32>(x0) + layout_.toastX;
    f32 toastY = static_cast<f32>(y0 + h) - 20.0f;
    for (auto& t : toasts) {
        f32 tw = render_.measureText(t.text.c_str(), layout_.toastScale, toastFont);
        render_.drawRect(toastX - 4.0f, toastY - 4.0f,
                         tw + 8.0f, 16.0f * layout_.toastScale + 8.0f,
                         layout_.toastBgColor);
        render_.drawText(toastX, toastY, t.text.c_str(),
                         layout_.toastColor, layout_.toastScale, toastFont);
        toastY -= layout_.toastSpacing;
    }
}

void UI::drawTabScreen(i32 x0, i32 y0) {
    i32 w = layout_.screenW;
    i32 h = layout_.screenH;
    const InternalFont heading = InternalFont::Default;
    const InternalFont body = InternalFont::Body;

    render_.drawRect(static_cast<f32>(x0), static_cast<f32>(y0),
                     static_cast<f32>(w), static_cast<f32>(h), layout_.screenBgColor);

    // --- tab bar (bottom of screen 2): Buildings / Store / Menu ---
    i32 tabY = y0 + h - layout_.tabBarH;
    i32 tabW = w / 3;
    static const char* const tabLabels[3] = {"Buildings", "Store", "Menu"};
    for (i32 i = 0; i < 3; ++i) {
        bool active = (tab_ == static_cast<ScreenTab>(i));
        render_.drawRect(static_cast<f32>(x0 + i * tabW), static_cast<f32>(tabY),
                         static_cast<f32>(tabW), static_cast<f32>(layout_.tabBarH),
                         active ? layout_.tabActiveColor : layout_.tabBarColor);
        f32 tw = render_.measureText(tabLabels[i], layout_.tabTextScale, body);
        f32 ty = static_cast<f32>(tabY) + static_cast<f32>(layout_.tabBarH) / 2.0f
            - render_.measureTextCenterY(tabLabels[i], layout_.tabTextScale, body);
        render_.drawText(static_cast<f32>(x0 + i * tabW) + (static_cast<f32>(tabW) - tw) / 2.0f,
                         ty, tabLabels[i],
                         active ? layout_.tabTextActiveColor : layout_.tabTextColor,
                         layout_.tabTextScale, body);
        render_.registerButton(2000 + i, x0 + i * tabW, tabY, tabW, layout_.tabBarH);
    }

    // --- content area ---
    f32 titleH = layout_.contentTitleScale * 16.0f;
    i32 contentStart = y0 + layout_.contentTitleY + static_cast<i32>(titleH) + 10;
    i32 contentBottom = tabY - 6;
    i32 availH = contentBottom - contentStart;

    i32 needH = static_cast<i32>(titleH) + 10 + contentHeight(contentStart);
    i32 maxScroll = std::max(0, needH - availH);
    if (scrollY_ > maxScroll) scrollY_ = maxScroll;
    maxScroll_ = maxScroll;

    if (tab_ != ScreenTab::Menu) menuRows_.clear();

    const char* title =
        tab_ == ScreenTab::Buildings ? "BUILDINGS" :
        tab_ == ScreenTab::Store ? "STORE" : "MENU";
    drawTextCentered(x0, y0 + layout_.contentTitleY, w, title,
                     layout_.contentTitleColor, layout_.contentTitleScale, heading);

    // Scroll arrows at the top-right / bottom-right of the content area.
    i32 arX = x0 + w - layout_.arrowW - 4;
    drawArrow(arX, y0 + 6, scrollY_ > 0, "^");
    drawArrow(arX, contentBottom - layout_.arrowH, scrollY_ < maxScroll_, "v");
    render_.registerButton(3000, arX, y0 + 6, layout_.arrowW, layout_.arrowH);
    render_.registerButton(3001, arX, contentBottom - layout_.arrowH,
                           layout_.arrowW, layout_.arrowH);

    // Panel content (building/store rows or menu rows).
    i32 padX = layout_.contentPadX;
    i32 rowW = w - 2 * padX - layout_.arrowW - 8;
    i32 contentRight = x0 + padX + rowW;
    drawScreen2Content(x0, contentStart, contentBottom, contentRight, padX, rowW);
}

i32 UI::contentHeight(i32 contentStart) {
    i32 y = contentStart;
    switch (tab_) {
    case ScreenTab::Buildings:
    case ScreenTab::Store:
        for (auto& b : game_.objects) {
            if (!b) continue;
            if (tab_ == ScreenTab::Buildings && b->amount <= 0) continue;
            y += layout_.rowH + layout_.rowGap;
        }
        return y - contentStart;
    case ScreenTab::Menu:
        return fillMenuRows(contentStart) - contentStart;
    }
    return 0;
}

i32 UI::fillMenuRows(i32 contentStart) {
    menuRows_.clear();
    i32 y = contentStart;
    auto addRow = [&](MenuRow::Kind kind, const char* label,
                      GameAction::Type action = GameAction::Type::None,
                      const std::string& prefName = "", bool value = false) {
        MenuRow row;
        row.id = 4000 + static_cast<i32>(menuRows_.size());
        row.kind = kind;
        row.y = y;
        row.label = label;
        row.action = action;
        row.prefName = prefName;
        row.value = value;
        menuRows_.push_back(row);
        y += layout_.menuItemH;
    };

    if (menu_ == MenuScreen::None) {
        addRow(MenuRow::Kind::Picker, "Options");
        addRow(MenuRow::Kind::Picker, "Stats");
        addRow(MenuRow::Kind::Picker, "Info");
    } else if (menu_ == MenuScreen::Options) {
        addRow(MenuRow::Kind::Action, "Save", GameAction::Type::SaveGame);
        addRow(MenuRow::Kind::Action, "Export");
        addRow(MenuRow::Kind::Action, "Import");
        addRow(MenuRow::Kind::Action, "Wipe save", GameAction::Type::WipeSave);
        addRow(MenuRow::Kind::Toggle, "Particles", GameAction::Type::None, "particles",
               game_.getPref("particles"));
        addRow(MenuRow::Kind::Toggle, "Numbers", GameAction::Type::None, "numbers",
               game_.getPref("numbers"));
        addRow(MenuRow::Kind::Toggle, "Short numbers", GameAction::Type::None, "format",
               game_.getPref("format"));
        addRow(MenuRow::Kind::Toggle, "Wobbly cookie", GameAction::Type::None, "wobbly",
               game_.getPref("wobbly"));
        addRow(MenuRow::Kind::Toggle, "Fast notes", GameAction::Type::None, "notifs",
               game_.getPref("notifs"));
    } else if (menu_ == MenuScreen::Stats) {
        i32 buildingsOwned = 0;
        for (auto& b : game_.objects)
            if (b) buildingsOwned += b->amount;
        addRow(MenuRow::Kind::Picker,
               ("Cookies in bank: " + formatCookies(game_.cookies)).c_str());
        addRow(MenuRow::Kind::Picker,
               ("Cookies baked (run): " + formatCookies(game_.cookiesEarned)).c_str());
        addRow(MenuRow::Kind::Picker,
               ("Cookies baked (all time): " +
                formatCookies(game_.cookiesEarned + game_.cookiesReset)).c_str());
        if (game_.cookiesReset > 0.0) {
            addRow(MenuRow::Kind::Picker,
                   ("Cookies forfeited: " + formatCookies(game_.cookiesReset)).c_str());
        }
        addRow(MenuRow::Kind::Picker,
               ("Buildings owned: " + std::to_string(buildingsOwned)).c_str());
        addRow(MenuRow::Kind::Picker,
               ("Cookies per second: " + formatCookies(game_.cookiesPs)).c_str());
        addRow(MenuRow::Kind::Picker,
               ("Cookies per click: " + formatCookies(game_.computedMouseCps)).c_str());
        addRow(MenuRow::Kind::Picker,
               ("Cookie clicks: " + std::to_string(game_.cookieClicks)).c_str());
        addRow(MenuRow::Kind::Picker,
               ("Golden clicks: " + std::to_string(game_.goldenClicksLocal)).c_str());
        addRow(MenuRow::Kind::Picker, "Version: v2.058");
    } else if (menu_ == MenuScreen::Info) {
        addRow(MenuRow::Kind::Picker, "Cookie Clicker");
        addRow(MenuRow::Kind::Picker, "C++ port of v2.058");
        addRow(MenuRow::Kind::Picker, "Original game: Orteil / DashNet");
        addRow(MenuRow::Kind::Picker, "Target: Nintendo DSi");
        addRow(MenuRow::Kind::Picker, "Two 256x192 screens");
    }
    return y;
}

void UI::drawScreen2Content(i32 x0, i32 contentTop, i32 contentBottom,
                            i32 contentRight, i32 padX, i32 rowW) {
    const InternalFont body = InternalFont::Body;
    i32 rowY = contentTop - scrollY_;

    if (tab_ == ScreenTab::Buildings || tab_ == ScreenTab::Store) {
        bool storeMode = (tab_ == ScreenTab::Store);
        for (i32 i = 0; i < static_cast<i32>(game_.objects.size()); ++i) {
            auto& b = game_.objects[i];
            if (!b) continue;
            if (!storeMode && b->amount <= 0) continue;

            bool locked = b->locked;
            bool affordable = game_.cookies >= b->getPrice();
            bool enabled = !locked && affordable;

            f32 drawY = static_cast<f32>(rowY);
            bool visible = drawY + static_cast<f32>(layout_.rowH) > static_cast<f32>(contentTop) &&
                           drawY < static_cast<f32>(contentBottom);
            if (visible) {
                // Row background.
                render_.drawRect(static_cast<f32>(x0 + padX), drawY,
                                 static_cast<f32>(rowW), static_cast<f32>(layout_.rowH),
                                 enabled ? layout_.rowHighlightColor : layout_.rowBgColor);

                // Building icon (dark silhouette when locked).
                f32 iconX = static_cast<f32>(x0 + padX + layout_.rowIconX);
                f32 iconY = drawY + (static_cast<f32>(layout_.rowH) - layout_.rowIconSize) / 2.0f;
                if (!locked) {
                    std::string iconBase = b->art.base.empty() ? b->name : b->art.base;
                    for (auto& c : iconBase)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    std::string iconPath = iconBase + ".png";
                    i32 iconHandle = loadIcon(iconPath);
                    f32 srcX = 0.0f, srcY = 0.0f, srcW = 0.0f, srcH = 0.0f;
                    if (iconPath == "cursor.png") {
                        srcX = 0.0f; srcY = 0.0f; srcW = 32.0f; srcH = 32.0f;
                    }
                    u32 iconColor = enabled ? 0xFFFFFFFF : 0x888888FF;
                    render_.drawTexture(iconHandle, iconX, iconY,
                                        static_cast<f32>(layout_.rowIconSize),
                                        static_cast<f32>(layout_.rowIconSize),
                                        iconColor, srcX, srcY, srcW, srcH);
                } else {
                    f32 pad = 4.0f;
                    render_.drawRect(iconX + pad, iconY + pad,
                                     static_cast<f32>(layout_.rowIconSize) - 2.0f * pad,
                                     static_cast<f32>(layout_.rowIconSize) - 2.0f * pad,
                                     layout_.rowLockedColor);
                }

                // Name ("???" when locked).
                std::string name = locked ? "???" : b->name;
                f32 nameX = iconX + static_cast<f32>(layout_.rowIconSize) + 4.0f;
                u32 nameColor = enabled ? layout_.rowNameColor : layout_.rowNameColorLocked;
                render_.drawText(nameX,
                                 drawY + static_cast<f32>(layout_.rowH) / 2.0f
                                     - render_.measureTextCenterY(name.c_str(), layout_.rowNameScale, body),
                                 name.c_str(), nameColor, layout_.rowNameScale, body);

                if (!locked) {
                    // Owned count ("xN") right after the name.
                    if (b->amount > 0) {
                        std::string owned = "x" + std::to_string(b->amount);
                        f32 nw = render_.measureText(name.c_str(), layout_.rowNameScale, body);
                        f32 ow = render_.measureText(owned.c_str(), layout_.rowPriceScale, body);
                        render_.drawText(nameX + nw + 4.0f,
                                         drawY + static_cast<f32>(layout_.rowH) / 2.0f
                                             - render_.measureTextCenterY(owned.c_str(),
                                                                          layout_.rowPriceScale, body),
                                         owned.c_str(), layout_.rowOwnedColor,
                                         layout_.rowPriceScale, body);
                    }
                    // Price right-aligned.
                    std::string priceStr = formatCookies(b->getPrice());
                    f32 pw = render_.measureText(priceStr.c_str(), layout_.rowPriceScale, body);
                    u32 priceColor = affordable ? layout_.rowPriceColorAfford : layout_.rowPriceColorNot;
                    render_.drawText(static_cast<f32>(contentRight) - pw,
                                     drawY + static_cast<f32>(layout_.rowH) / 2.0f
                                         - render_.measureTextCenterY(priceStr.c_str(),
                                                                      layout_.rowPriceScale, body),
                                     priceStr.c_str(), priceColor, layout_.rowPriceScale, body);

                    // Click the row to buy one (Game validates affordability).
                    if (enabled) {
                        render_.registerButton(1000 + i, x0 + padX,
                                               static_cast<i32>(drawY), rowW, layout_.rowH);
                    }
                }
            }
            rowY += layout_.rowH + layout_.rowGap;
        }
    } else {
        // Menu tab: Options / Stats / Info rows (menuRows_ filled by contentHeight()).
        for (auto& row : menuRows_) {
            f32 drawY = static_cast<f32>(row.y - scrollY_);
            if (drawY + static_cast<f32>(layout_.menuItemH) < static_cast<f32>(contentTop) ||
                drawY > static_cast<f32>(contentBottom))
                continue;
            render_.drawRect(static_cast<f32>(x0 + padX), drawY,
                             static_cast<f32>(rowW), static_cast<f32>(layout_.menuItemH),
                             layout_.menuItemBg);
            f32 tx = static_cast<f32>(x0 + padX)
                + (row.kind == MenuRow::Kind::Toggle ? 20.0f : 0.0f);
            render_.drawText(tx,
                             drawY + static_cast<f32>(layout_.menuItemH) / 2.0f
                                 - render_.measureTextCenterY(row.label.c_str(),
                                                              layout_.menuTextScale, body),
                             row.label.c_str(), layout_.menuTextColor,
                             layout_.menuTextScale, body);
            if (row.kind == MenuRow::Kind::Toggle) {
                i32 cb = 14;
                f32 cbY = drawY + (static_cast<f32>(layout_.menuItemH) - cb) / 2.0f;
                render_.drawRect(static_cast<f32>(x0 + padX), cbY,
                                 static_cast<f32>(cb), static_cast<f32>(cb),
                                 row.value ? layout_.checkboxOn : layout_.checkboxOff);
                if (!row.value) {
                    render_.drawRect(static_cast<f32>(x0 + padX) + 1.0f, cbY + 1.0f,
                                     static_cast<f32>(cb) - 2.0f, static_cast<f32>(cb) - 2.0f,
                                     layout_.screenBgColor);
                }
            }
            render_.registerButton(row.id, x0 + padX, static_cast<i32>(drawY),
                                     rowW, layout_.menuItemH);
        }
    }
}

GameAction UI::handleMenuRow(i32 index) {
    if (index < 0 || index >= static_cast<i32>(menuRows_.size()))
        return GameAction{GameAction::Type::None, -1};
    MenuRow& row = menuRows_[static_cast<size_t>(index)];
    switch (row.kind) {
    case MenuRow::Kind::Picker:
        if (row.label == "Options") menu_ = MenuScreen::Options;
        else if (row.label == "Stats") menu_ = MenuScreen::Stats;
        else menu_ = MenuScreen::Info;
        scrollY_ = 0;
        break;
    case MenuRow::Kind::Toggle:
        return GameAction{GameAction::Type::TogglePref, -1, row.prefName};
    case MenuRow::Kind::Action:
        if (row.action == GameAction::Type::SaveGame)
            return GameAction{GameAction::Type::SaveGame};
        if (row.action == GameAction::Type::WipeSave)
            return GameAction{GameAction::Type::WipeSave};
        // Export / Import: file operations not implemented on this port yet.
        notify("Not implemented yet");
        break;
    }
    return GameAction{GameAction::Type::None, -1};
}

GameAction UI::on_pointer(i32 x, i32 y) {
    i32 hit = render_.hitTest(x, y);

    if (hit == Render::HIT_COOKIE) {
        return GameAction{GameAction::Type::ClickCookie, -1};
    }

    // Screen 2 tabs: switching tabs closes any open submenu.
    if (hit >= 2000 && hit < 3000) {
        tab_ = static_cast<ScreenTab>(hit - 2000);
        menu_ = MenuScreen::None;
        scrollY_ = 0;
        menuRows_.clear();
        return GameAction{GameAction::Type::None, -1};
    }

    // Scroll arrows.
    if (hit == 3000) {
        scrollY_ = std::max(0, scrollY_ - layout_.rowH);
        return GameAction{GameAction::Type::None, -1};
    }
    if (hit == 3001) {
        scrollY_ = std::min(maxScroll_, scrollY_ + layout_.rowH);
        return GameAction{GameAction::Type::None, -1};
    }

    // Menu rows (only meaningful on the Menu tab).
    if (tab_ == ScreenTab::Menu && hit >= 4000 &&
        hit < 4000 + static_cast<i32>(menuRows_.size())) {
        return handleMenuRow(hit - 4000);
    }

    // Store / building rows: buy one building. Game validates affordability.
    if (hit >= Render::HIT_BUILDING_BASE && hit < 2000) {
        i32 idx = hit - Render::HIT_BUILDING_BASE;
        if (idx >= 0 && idx < static_cast<i32>(game_.objects.size())) {
            return GameAction{GameAction::Type::BuyBuilding, idx};
        }
    }
    return GameAction{GameAction::Type::None, -1};
}
