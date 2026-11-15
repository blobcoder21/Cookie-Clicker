#include "UI.h"

#include "UIHandheld.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Display formatting helpers are shared inline in UI.h; this TU also sees them
// via that header. (cleanNum / commaInt / formatCookies)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// UiDesktop — 1280x720 three-panel desktop layout (LayoutDesktop).
//
// Restored from the commit before the DSi dual-screen change. Game supplies
// state; Render supplies platform primitives; layout lives in LayoutDesktop.
// ---------------------------------------------------------------------------
class UiDesktop : public UI {
public:
    UiDesktop(const Game& game, Render& render, const LayoutDesktop& layout = LayoutDesktop())
        : game_(game), render_(render), layout_(layout) {}
    ~UiDesktop() override = default;

    void draw() override;
    void update(f32 dt) override;
    GameAction on_pointer(i32 x, i32 y) override;
    void notify(const std::string& text) override;

private:
    const Game& game_;
    Render& render_;
    LayoutDesktop layout_;

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

    // cached ticker rect (set during draw, checked during on_pointer)
    i32 tickerRectX = 0, tickerRectY = 0, tickerRectW = 0, tickerRectH = 0;

    i32 loadIcon(const std::string& iconPath);
};

i32 UiDesktop::loadIcon(const std::string& iconPath) {
    auto it = iconTexCache.find(iconPath);
    if (it != iconTexCache.end()) return it->second;
    i32 handle = render_.loadTexture(iconPath);
    iconTexCache[iconPath] = handle;
    return handle;
}

void UiDesktop::update(f32 dt) {
    // Decrement toast lifetimes and drop expired ones (skeleton).
    for (auto it = toasts.begin(); it != toasts.end();) {
        it->life -= dt;
        if (it->life <= 0.0f) {
            it = toasts.erase(it);
        } else {
            ++it;
        }
    }
}

void UiDesktop::notify(const std::string& text) {
    Toast t;
    t.text = text;
    t.life = layout_.toastLife;
    t.maxLife = layout_.toastLife;
    t.active = true;
    (void)t;
    toasts.push_back(std::move(t));
}

void UiDesktop::draw() {
    i32 w = render_.windowW();
    i32 h = render_.windowH();

    if (!bgStripesTex) bgStripesTex = render_.loadTexture("bgStripes.png");
    if (!vignetteTex) vignetteTex = render_.loadTexture("vignette.png");
    if (!glowTex) glowTex = render_.loadTexture("glow.png");
    if (!panelHorizTex) panelHorizTex = render_.loadTexture("panelHorizontal.png");
    if (!panelVertTex) panelVertTex = render_.loadTexture("panelVertical.png");
    if (!cookieTex) cookieTex = render_.loadTexture("perfectCookie.png");
    if (!priceCookieTex) priceCookieTex = render_.loadTexture("smallCookies.png");

    render_.drawTextureTiled(bgStripesTex, 0.0f, 0.0f,
                             static_cast<f32>(w), static_cast<f32>(h),
                             0xFFFFFFFF, 0);
    render_.drawTexture(vignetteTex, 0.0f, 0.0f,
                        static_cast<f32>(w), static_cast<f32>(h),
                        0xFFFFFFFF);

    i32 leftW = static_cast<i32>(static_cast<f32>(w) * layout_.leftPanelWidthFraction);
    i32 storeW = static_cast<i32>(static_cast<f32>(w) * layout_.storePanelWidthFraction);
    if (storeW < 60) storeW = 60;
    i32 sx = w - storeW;

    i32 pvW2 = 0, pvH = 0;
    render_.textureSize(panelVertTex, &pvW2, &pvH);

    if (pvW2 > 0 && pvH > 0) {
        render_.drawTextureTiled(panelVertTex,
                                 static_cast<f32>(leftW), 0.0f,
                                 static_cast<f32>(pvW2), static_cast<f32>(h),
                                 0xFFFFFFFF, 1);
        render_.drawTextureTiled(panelVertTex,
                                 static_cast<f32>(sx - pvW2), 0.0f,
                                 static_cast<f32>(pvW2), static_cast<f32>(h),
                                 0xFFFFFFFF, 1);
    }

    i32 cx = leftW / 2;

    const InternalFont heading = InternalFont::Default;
    const InternalFont body = InternalFont::Body;

    // --- bakery name banner ---
    std::string bakery = game_.getBakeryName();
    f32 bakeryScale = layout_.bakeryScale;
    f32 bw = render_.measureText(bakery.c_str(), bakeryScale, heading);
    f32 bakeryOff = render_.measureTextCenterY(bakery.c_str(), bakeryScale, heading);
    f32 bannerYc = static_cast<f32>(h) * layout_.bakeryYFraction;
    f32 bakeryY = bannerYc - bakeryOff;
    f32 textH = bakeryScale * 16.0f;
    f32 bannerW = bw + 2.0f * layout_.bannerPadX;
    f32 bannerH = textH + 2.0f * layout_.bannerPadY;
    f32 bannerX = static_cast<f32>(cx) - bannerW / 2.0f;
    f32 bannerY = bannerYc - bannerH / 2.0f;
    render_.drawRect(bannerX, bannerY, bannerW, bannerH, layout_.bannerColor);
    render_.drawText(static_cast<f32>(cx) - bw / 2.0f, bakeryY,
                     bakery.c_str(), layout_.bakeryColor, bakeryScale, heading);

    // --- cookie count ---
    std::string cookieStr = formatCookies(game_.cookies);
    f32 cw = render_.measureText(cookieStr.c_str(), layout_.cookieCountScale, body);
    f32 cookieYc = static_cast<f32>(h) * layout_.cookieCountYFraction;
    f32 cookieOff = render_.measureTextCenterY(cookieStr.c_str(), layout_.cookieCountScale, body);
    f32 cookieY = cookieYc - cookieOff;
    render_.drawText(static_cast<f32>(cx) - cw / 2.0f, cookieY,
                     cookieStr.c_str(), layout_.cookieCountColor,
                     layout_.cookieCountScale, body);

    // --- CPS ---
    std::string cpsStr = "per second: " + formatCookies(game_.cookiesPs);
    f32 cw2 = render_.measureText(cpsStr.c_str(), layout_.cpsScale, body);
    f32 cpsYc = static_cast<f32>(h) * layout_.cpsYFraction;
    f32 cpsOff = render_.measureTextCenterY(cpsStr.c_str(), layout_.cpsScale, body);
    f32 cpsY = cpsYc - cpsOff;
    render_.drawText(static_cast<f32>(cx) - cw2 / 2.0f, cpsY,
                     cpsStr.c_str(), layout_.cpsColor,
                     layout_.cpsScale, body);

    // --- big cookie ---
    f64 size = game_.bigCookieSizeValue();
    if (size < layout_.bigCookieMinSize) size = layout_.bigCookieMinSize;
    i32 cr = static_cast<i32>(layout_.bigCookieHalfSize * size);
    i32 cy = static_cast<i32>(static_cast<f32>(h) * layout_.bigCookieYFraction);
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

    // --- middle panel ---
    {
        i32 mx = leftW;
        i32 mw = sx - leftW;
        f32 stripY = static_cast<f32>(layout_.middleCookieStripY)
            * (static_cast<f32>(h) / 919.0f);
        f32 stripH = static_cast<f32>(layout_.middleCookieStripH)
            * (static_cast<f32>(h) / 919.0f);
        f32 stripX = static_cast<f32>(mx) + 16.0f;
        f32 iconSize = stripH * 0.8f;
        f32 iconY = stripY + (stripH - iconSize) / 2.0f;
        for (auto& b : game_.objects) {
            if (!b || b->locked) continue;
            std::string iconBase = b->art.base.empty() ? b->name : b->art.base;
            for (auto& c : iconBase)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string iconPath = iconBase + ".png";
            i32 iconHandle = loadIcon(iconPath);
            f32 srcX = 0.0f, srcY = 0.0f, srcW = 0.0f, srcH = 0.0f;
            if (iconPath == "cursor.png") {
                srcX = 0.0f; srcY = 0.0f; srcW = 32.0f; srcH = 32.0f;
            }
            render_.drawTexture(iconHandle, stripX, iconY,
                                iconSize, iconSize, 0xFFFFFFFF,
                                srcX, srcY, srcW, srcH);
            stripX += iconSize + 8.0f;
        }

        f32 grassY = static_cast<f32>(layout_.middleGrassY)
            * (static_cast<f32>(h) / 919.0f);
        f32 grassH = static_cast<f32>(h) - grassY;
        render_.drawRect(static_cast<f32>(mx), grassY,
                         static_cast<f32>(mw), grassH,
                         layout_.grassColor);
    }

    // --- top menu bar ---
    auto drawWoodStrip = [&](f32 x, f32 y, f32 wid, f32 hei) {
        if (panelHorizTex == 0) return;
        i32 phH = 0;
        render_.textureSize(panelHorizTex, nullptr, &phH);
        if (phH <= 0) phH = 16;
        i32 rows = static_cast<i32>(std::ceil(hei / static_cast<f32>(phH)));
        for (i32 r = 0; r < rows; ++r) {
            f32 ry = y + static_cast<f32>(r) * static_cast<f32>(phH);
            f32 rh = std::min(static_cast<f32>(phH), hei - (ry - y));
            render_.drawTextureTiled(panelHorizTex, x, ry, wid, rh,
                                     layout_.topBarColor, 0);
        }
    };

    auto buttonRightEdge = [&](i32 relX, i32 relW) {
        return static_cast<f32>(sx + storeW - relX - relW);
    };

    {
        f32 bx = static_cast<f32>(leftW + layout_.btnOptionsX);
        f32 by = static_cast<f32>(layout_.topBtnY);
        drawWoodStrip(bx, by, static_cast<f32>(layout_.btnOptionsW),
                      static_cast<f32>(layout_.topBtnH));
        render_.drawText(bx + layout_.btnPadX, by + layout_.topBtnH / 2.0f,
                         "Options", 0xFFFFFFFF, layout_.newsTickerScale, body);
        f32 sx2 = static_cast<f32>(leftW + layout_.btnStatsX);
        f32 sy2 = static_cast<f32>(layout_.topBtnY + layout_.topBtnH + 4.0f);
        drawWoodStrip(sx2, sy2, static_cast<f32>(layout_.btnStatsW),
                      static_cast<f32>(layout_.topBtnH));
        render_.drawText(sx2 + layout_.btnPadX, sy2 + layout_.topBtnH / 2.0f,
                         "Stats", 0xFFFFFFFF, layout_.newsTickerScale, body);
    }
    {
        f32 bx = buttonRightEdge(layout_.btnInfoX, layout_.btnInfoW);
        f32 by = static_cast<f32>(layout_.topBtnY);
        drawWoodStrip(bx, by, static_cast<f32>(layout_.btnInfoW),
                      static_cast<f32>(layout_.topBtnH));
        render_.drawText(bx + layout_.btnPadX, by + layout_.topBtnH / 2.0f,
                         "Info", 0xFFFFFFFF, layout_.newsTickerScale, body);
        f32 lx2 = buttonRightEdge(layout_.btnLegacyX, layout_.btnLegacyW);
        f32 ly2 = static_cast<f32>(layout_.topBtnY + layout_.topBtnH + 4.0f);
        drawWoodStrip(lx2, ly2, static_cast<f32>(layout_.btnLegacyW),
                      static_cast<f32>(layout_.topBtnH));
        render_.drawText(lx2 + layout_.btnPadX, ly2 + layout_.topBtnH / 2.0f,
                         "Legacy", 0xFFFFFFFF, layout_.newsTickerScale, body);
    }
    {
        f32 leftEdge = static_cast<f32>(leftW + layout_.btnOptionsX + layout_.btnOptionsW + layout_.btnGap);
        f32 rightEdge = buttonRightEdge(layout_.btnInfoX, layout_.btnInfoW);
        f32 tw = rightEdge - leftEdge;
        if (tw > 0) {
            std::string tickerText = game_.tickerMessage();
            bool isFortune = game_.tickerIsFortune();
            u32 tickerColor = isFortune ? 0xFFD700FF : layout_.newsTickerColor;
            f32 twText = render_.measureText(tickerText.c_str(), layout_.newsTickerScale, body);
            f32 tx = leftEdge + (tw - twText) / 2.0f;
            render_.drawRect(leftEdge, static_cast<f32>(layout_.topBarY + 10),
                             tw, static_cast<f32>(layout_.topBarH - 20), 0x00000080);
            render_.drawText(tx, static_cast<f32>(layout_.newsTickerY),
                             tickerText.c_str(), tickerColor,
                             layout_.newsTickerScale, body);
            tickerRectX = static_cast<i32>(leftEdge);
            tickerRectY = static_cast<i32>(layout_.topBarY + 10);
            tickerRectW = static_cast<i32>(tw);
            tickerRectH = static_cast<i32>(layout_.topBarH - 20);
        } else {
            tickerRectW = 0;
        }
    }

    // --- right panel: store ---
    f32 storeWf = static_cast<f32>(storeW);
    f32 storeScale = storeWf / 304.0f;
    auto SX = [&](i32 basePx) { return static_cast<f32>(sx) + static_cast<f32>(basePx) * storeScale; };
    auto SY = [&](i32 basePx) { return static_cast<f32>(basePx) * (static_cast<f32>(h) / 919.0f); };
    auto SCALE = [&](f32 base) { return base * storeScale; };

    render_.drawText(SX(layout_.storeTitleX), SY(layout_.storeTitleY), "STORE",
                     layout_.storeTitleColor, SCALE(layout_.storeTitleScale), heading);

    i32 firstUnlocked = -1;
    for (i32 i = 0; i < static_cast<i32>(game_.objects.size()); ++i) {
        if (game_.objects[i] && !game_.objects[i]->locked) { firstUnlocked = i; break; }
    }
    for (i32 i = 0; i < 6; ++i) {
        f32 gx = SX(layout_.upgradeSlotX)
            + static_cast<f32>(i) * SCALE(static_cast<f32>(layout_.upgradeSlotW + layout_.upgradeSlotGap));
        f32 gy = SY(layout_.upgradesY);
        f32 gw = SCALE(static_cast<f32>(layout_.upgradeSlotW));
        f32 gh = SCALE(static_cast<f32>(layout_.upgradeSlotH));
        bool highlight = (firstUnlocked >= 0 && i == 0);
        render_.drawRect(gx, gy, gw, gh, highlight ? layout_.buySellActive : 0x4A4A4AFF);
        render_.drawRect(gx + 1.0f, gy + 1.0f, gw - 2.0f, gh - 2.0f, 0x222222FF);
        if (i < static_cast<i32>(game_.objects.size()) && game_.objects[i]) {
            auto& b = game_.objects[i];
            std::string iconBase = b->art.base.empty() ? b->name : b->art.base;
            for (auto& c : iconBase)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string iconPath = iconBase + ".png";
            i32 iconHandle = loadIcon(iconPath);
            f32 srcX = 0.0f, srcY = 0.0f, srcW = 0.0f, srcH = 0.0f;
            if (iconPath == "cursor.png") {
                srcX = 0.0f; srcY = 0.0f; srcW = 32.0f; srcH = 32.0f;
            }
            f32 iconS = gw * 0.75f;
            render_.drawTexture(iconHandle, gx + (gw - iconS) / 2.0f, gy + (gh - iconS) / 2.0f,
                                iconS, iconS, b->locked ? 0x888888FF : 0xFFFFFFFF,
                                srcX, srcY, srcW, srcH);
        }
    }

    // Buy / Sell toggle + bulk labels.
    {
        f32 btx = SX(layout_.storeTitleX);
        f32 bty = SY(layout_.buySellY);
        f32 bw = SCALE(static_cast<f32>(layout_.buySellW));
        f32 bh = SCALE(static_cast<f32>(layout_.buySellH));
        f32 halfW = bw / 2.0f;
        render_.drawRect(btx, bty, halfW, bh, layout_.buySellActive);
        render_.drawText(btx + SCALE(4.0f), bty + bh / 2.0f, "Buy", 0x000000FF, SCALE(0.9f), body);
        render_.drawRect(btx + halfW, bty, halfW, bh, layout_.buySellColor);
        render_.drawText(btx + halfW + SCALE(4.0f), bty + bh / 2.0f, "Sell", 0x000000FF, SCALE(0.9f), body);
        f32 blx = btx + bw + SCALE(static_cast<f32>(layout_.bulkGap));
        render_.drawText(blx, SY(layout_.bulkY), "1", layout_.bulkColor, SCALE(layout_.bulkScale), body);
        render_.drawText(blx + SCALE(24.0f), SY(layout_.bulkY), "10", layout_.bulkColor, SCALE(layout_.bulkScale), body);
        render_.drawText(blx + SCALE(52.0f), SY(layout_.bulkY), "100", layout_.bulkColor, SCALE(layout_.bulkScale), body);
    }

    // Store rows.
    if (!storeTileTex) storeTileTex = render_.loadTexture("storeTile.png");
    f32 rowY = SY(layout_.storeFirstRowY);
    f32 rowStep = SCALE(static_cast<f32>(layout_.storeRowHeight));
    i32 row = 0;
    for (auto& b : game_.objects) {
        if (!b) continue;
        if (rowY + rowStep > static_cast<f32>(h)) break;

        bool canAfford = game_.cookies >= b->getPrice();
        if (!b->locked && game_.cookiesEarned >= b->basePrice) {
        } else if (game_.cookiesEarned >= b->basePrice) {
            b->locked = false;
        }
        bool isLocked = b->locked;
        bool isEnabled = canAfford && !isLocked;

        f32 cellW = SCALE(static_cast<f32>(layout_.storeTileWidth));
        f32 cellH = rowStep;
        f32 cellX = static_cast<f32>(sx) + (storeW - cellW) / 2.0f;
        u32 tileColor = isEnabled ? layout_.tileColorEnabled : layout_.tileColorDisabled;
        render_.drawRect(cellX, rowY, cellW, cellH, tileColor);

        f32 iconW = SCALE(static_cast<f32>(layout_.storeIconSize));
        if (!isLocked) {
            std::string iconBase = b->art.base.empty() ? b->name : b->art.base;
            for (auto& c : iconBase)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string iconPath = iconBase + ".png";
            i32 iconHandle = loadIcon(iconPath);
            f32 srcX = 0.0f, srcY = 0.0f, srcW = 0.0f, srcH = 0.0f;
            if (iconPath == "cursor.png") {
                srcX = 0.0f; srcY = 0.0f; srcW = 32.0f; srcH = 32.0f;
            }
            render_.drawTexture(iconHandle,
                                SX(layout_.storeIconX), rowY,
                                iconW, iconW, 0xFFFFFFFF, srcX, srcY, srcW, srcH);
        } else {
            f32 pad = SCALE(12.0f);
            render_.drawRect(SX(layout_.storeIconX) + pad, rowY + pad,
                             iconW - 2.0f * pad, iconW - 2.0f * pad,
                             layout_.storeLockedColor);
        }

        std::string displayName = isLocked ? "???" : b->name;
        u32 nameColor = isEnabled ? layout_.storeNameColorEnabled : layout_.storeNameColorDisabled;
        f32 nameScale = SCALE(layout_.storeNameScale);
        f32 nameX = SX(layout_.storeNameX);
        f32 nameY = rowY + SCALE(8.0f);
        render_.drawText(nameX, nameY, displayName.c_str(), nameColor, nameScale, body);

        std::string priceStr = formatCookies(b->getPrice());
        f32 pw = render_.measureText(priceStr.c_str(), SCALE(layout_.storePriceScale), body);
        u32 priceColor = isEnabled ? layout_.storePriceColorAffordable : layout_.storePriceColorNot;
        f32 priceY = nameY + nameScale * SCALE(15.0f) + SCALE(3.0f);
        f32 priceRight = static_cast<f32>(sx + storeW) - SCALE(static_cast<f32>(layout_.storeOwnedRightMargin));
        f32 priceIconW = 0.0f, priceIconH = 0.0f;
        i32 priceIconWi = 0, priceIconHi = 0;
        render_.textureSize(priceCookieTex, &priceIconWi, &priceIconHi);
        if (priceIconWi > 0 && priceIconHi > 0) {
            priceIconW = static_cast<f32>(priceIconWi);
            priceIconH = static_cast<f32>(priceIconHi);
            f32 iconScale = SCALE(14.0f) / priceIconW;
            f32 iconDrawW = priceIconW * iconScale;
            f32 iconDrawH = priceIconH * iconScale;
            f32 iconY = priceY + (SCALE(14.0f) - iconDrawH) / 2.0f;
            render_.drawTexture(priceCookieTex,
                                priceRight - pw - iconDrawW - SCALE(static_cast<f32>(layout_.storePriceIconGap)),
                                iconY, iconDrawW, iconDrawH, priceColor);
        }
        render_.drawText(priceRight - pw, priceY, priceStr.c_str(),
                         priceColor, SCALE(layout_.storePriceScale), body);

        if (b->amount > 0) {
            std::string ownedStr = std::to_string(b->amount);
            const f32 scale = SCALE(layout_.storeOwnedScale);
            f32 ow = render_.measureText(ownedStr.c_str(), scale, body);
            f32 ownedX = priceRight - ow;
            f32 ownedY = rowY + cellH / 2.0f
                - render_.measureTextCenterY(ownedStr.c_str(), scale, body);
            render_.drawText(ownedX, ownedY, ownedStr.c_str(),
                             layout_.storeOwnedColor, scale, body);
        }

        render_.registerButton(1000 + row, static_cast<f32>(sx),
                               rowY, storeW, cellH);

        rowY += rowStep;
        ++row;
    }

    // --- toasts ---
    if (!toasts.empty()) {
        f32 toastY = static_cast<f32>(h) - 40.0f;
        for (auto& t : toasts) {
            f32 tw = render_.measureText(t.text.c_str(), layout_.toastScale, body);
            render_.drawRect(layout_.toastX - 4.0f, toastY - 4.0f,
                             tw + 8.0f, static_cast<f32>(16 * layout_.toastScale) + 8.0f,
                             layout_.toastBgColor);
            render_.drawText(layout_.toastX, toastY, t.text.c_str(),
                             layout_.toastColor, layout_.toastScale, body);
            toastY -= layout_.toastSpacing;
        }
    }
}

GameAction UiDesktop::on_pointer(i32 x, i32 y) {
    i32 hit = render_.hitTest(x, y);
    if (hit == Render::HIT_COOKIE) {
        return GameAction{GameAction::Type::ClickCookie, -1};
    }
    if (hit >= Render::HIT_BUILDING_BASE) {
        i32 idx = hit - Render::HIT_BUILDING_BASE;
        if (idx >= 0 && idx < static_cast<i32>(game_.objects.size())) {
            return GameAction{GameAction::Type::BuyBuilding, idx};
        }
    }
    if (tickerRectW > 0 &&
        x >= tickerRectX && x < tickerRectX + tickerRectW &&
        y >= tickerRectY && y < tickerRectY + tickerRectH &&
        game_.tickerIsFortune()) {
        return GameAction{GameAction::Type::ClickFortune, -1};
    }
    return GameAction{GameAction::Type::None, -1};
}

// ---------------------------------------------------------------------------
// Factory: pick the concrete UI profile.
//
// Desktop is the default; DSi/3DS handheld share the dual-screen code.
// ---------------------------------------------------------------------------
std::unique_ptr<UI> CreateUI(UIProfile profile, const Game& game, Render& render) {
    switch (profile) {
    case UIProfile::Desktop:
        return std::make_unique<UiDesktop>(game, render);
    case UIProfile::Ds:
    case UIProfile::ThreeDs:
        return std::make_unique<UiHandheld>(game, render);
    }
    return std::make_unique<UiDesktop>(game, render);
}
