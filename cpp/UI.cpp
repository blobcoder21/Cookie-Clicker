#include "UI.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Display formatting for cookie counts and prices (uppercase suffixes).
// Ported verbatim from Game.cpp (these helpers are display-only, so they live
// in the UI layer now).
// ---------------------------------------------------------------------------
namespace {

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

void UI::notify(const std::string& text) {
    Toast t;
    t.text = text;
    t.life = layout_.toastLife;
    t.maxLife = layout_.toastLife;
    t.active = true;
    // Position: stack from the bottom of the cookie panel upwards.
    // y is set at draw time based on how many toasts are active.
    (void)t;
    toasts.push_back(std::move(t));
}

void UI::draw() {
    // drawT was a per-frame draw counter; it is not used for any decision, so
    // it is intentionally not maintained here (no behavioural impact).

    i32 w = render_.windowW();
    i32 h = render_.windowH();

    // background (matches body background: bgBlue.png)
    // NOTE: pass "bgBlue.png" (no "img/" prefix) — resolveAssetPath() already
    // prepends kImgDir which ends in "/img", so "img/bgBlue.png" would produce
    // a double "img/img/" path and fail to load.
    if (!bgTex) bgTex = render_.loadTexture("bgBlue.png");
    render_.drawTexture(bgTex, 0, 0, static_cast<f32>(w), static_cast<f32>(h), 0xFFFFFFFF);

    // ------------------------------------------------------------------
    // Left panel: #sectionLeft  (width 30%, cookie panel)
    // ------------------------------------------------------------------
    i32 leftW = static_cast<i32>(static_cast<f32>(w) * layout_.leftPanelWidthFraction);

    // cookie count (#cookies title, top 10%, centered)
    std::string cookieStr = formatCookies(game_.cookies);
    f32 cw = render_.measureText(cookieStr.c_str(), layout_.cookieCountScale);
    render_.drawText(static_cast<f32>(leftW / 2) - cw / 2,
                     static_cast<f32>(h * layout_.cookieCountYFraction) + layout_.cookieCountYOffset,
                     cookieStr.c_str(), 0xFFFFFFFF, layout_.cookieCountScale);

    // bakery name (#bakeryName, top 10%)
    std::string bakery = game_.getBakeryName();
    f32 bw = render_.measureText(bakery.c_str(), layout_.bakeryScale);
    render_.drawText(static_cast<f32>(leftW / 2) - bw / 2,
                     static_cast<f32>(h * layout_.bakeryYFraction) + layout_.bakeryYOffset,
                     bakery.c_str(), layout_.bakeryColor, layout_.bakeryScale);

    // CPS (#cookies-style line below bakery name)
    std::string cpsStr = formatCookies(game_.cookiesPs) + " /s";
    f32 cw2 = render_.measureText(cpsStr.c_str(), layout_.cpsScale);
    render_.drawText(static_cast<f32>(leftW / 2) - cw2 / 2,
                     static_cast<f32>(h * layout_.cpsYFraction) + layout_.cpsYOffset,
                     cpsStr.c_str(), layout_.cpsColor, layout_.cpsScale);

    // big cookie (#cookieAnchor: left 50%, top 40%; #bigCookie 256x256)
    if (!cookieTex) cookieTex = render_.loadTexture("perfectCookie.png");
    i32 cx = leftW / 2;
    i32 cy = static_cast<i32>(h * layout_.bigCookieYFraction);
    f64 size = game_.bigCookieSizeValue();
    if (size < layout_.bigCookieMinSize) size = layout_.bigCookieMinSize;
    i32 cr = static_cast<i32>(layout_.bigCookieHalfSize * size); // half of 256px cookie
    render_.drawTexture(cookieTex,
                        static_cast<f32>(cx - cr), static_cast<f32>(cy - cr),
                        static_cast<f32>(cr) * 2.0f, static_cast<f32>(cr) * 2.0f,
                        0xFFFFFFFF);
    render_.registerCookie(static_cast<f32>(cx), static_cast<f32>(cy),
                           static_cast<f32>(cr));

    // ------------------------------------------------------------------
    // Middle panel: sectioned panel with wood borders (never scaled).
    // Vertical wood dividers separate it from the left (cookie) panel and
    // right (store) panel. Top tab strip: Options Stats Legacy Info Store.
    // ------------------------------------------------------------------
    i32 storeW = layout_.storePanelWidth;

    // wood borders, tiled at natural size (never scaled)
    if (!panelHorizTex) panelHorizTex = render_.loadTexture("panelHorizontal.png");
    if (!panelVertTex) panelVertTex = render_.loadTexture("panelVertical.png");
    i32 phW = 0, phH = 0;
    i32 pvW2 = 0, pvH = 0;
    render_.textureSize(panelHorizTex, &phW, &phH);
    render_.textureSize(panelVertTex, &pvW2, &pvH);

    i32 sx = w - storeW;
    if (sx < leftW + 2 * pvW2) sx = leftW + 2 * pvW2; // guard for narrow windows
    i32 middleX = leftW + pvW2;   // after left vertical wood divider
    i32 middleW = sx - pvW2 - middleX; // before right vertical wood divider
    if (middleW < 1) middleW = 1;

    // vertical wood divider between left (cookie) panel and middle panel
    if (pvW2 > 0 && pvH > 0) {
        render_.drawTextureTiled(panelVertTex,
                                 static_cast<f32>(leftW), 0.0f,
                                 static_cast<f32>(pvW2), static_cast<f32>(h),
                                 0xFFFFFFFF, 1); // axis 1 = vertical
    }

    // middle panel background fill
    render_.drawRect(static_cast<f32>(middleX), 0.0f,
                     static_cast<f32>(middleW), static_cast<f32>(h),
                     layout_.middlePanelBgColor);

    // top tab strip (wood horizontal bar across middle panel top)
    if (phW > 0 && phH > 0) {
        render_.drawTextureTiled(panelHorizTex,
                                 static_cast<f32>(middleX), static_cast<f32>(layout_.tabStripY),
                                 static_cast<f32>(middleW), static_cast<f32>(phH),
                                 0xFFFFFFFF, 0);
    }
    // tab labels
    for (i32 i = 0; i < 5; ++i) {
        render_.drawText(static_cast<f32>(middleX + layout_.tabLabelX[i]),
                         static_cast<f32>(layout_.tabLabelY),
                         layout_.tabLabelText[i], 0xFFFFFFFF, layout_.tabLabelScale);
    }

    // blank horizontal bands separated by tiled panelHorizontal borders
    f32 bandY = static_cast<f32>(layout_.tabStripY) + static_cast<f32>(phH); // below tab strip
    f32 bordersH = static_cast<f32>(phH * (layout_.numSections - 1));
    f32 bandH = (static_cast<f32>(h) - bandY - bordersH) / static_cast<f32>(layout_.numSections);
    if (bandH < layout_.bandMinHeight) bandH = layout_.bandMinHeight;
    for (i32 i = 0; i < layout_.numSections; ++i) {
        // blank band
        render_.drawRect(static_cast<f32>(middleX), bandY,
                         static_cast<f32>(middleW), bandH, layout_.bandColor);
        bandY += bandH;
        // wood border between bands (clip last tile to exact width)
        if (i < layout_.numSections - 1 && phW > 0 && phH > 0) {
            render_.drawTextureTiled(panelHorizTex,
                                     static_cast<f32>(middleX), bandY,
                                     static_cast<f32>(middleW), static_cast<f32>(phH),
                                     0xFFFFFFFF, 0); // axis 0 = horizontal
            bandY += static_cast<f32>(phH);
        }
    }

    // vertical wood divider between middle panel and right (store) panel
    if (pvW2 > 0 && pvH > 0) {
        render_.drawTextureTiled(panelVertTex,
                                 static_cast<f32>(sx - pvW2), 0.0f,
                                 static_cast<f32>(pvW2), static_cast<f32>(h),
                                 0xFFFFFFFF, 1); // axis 1 = vertical
    }

    // ------------------------------------------------------------------
    // Right panel: #sectionRight / #store (rightmost ~318px)
    // ------------------------------------------------------------------
    // store panel background inside the wood border (divider drawn above)
    i32 storeX = sx;
    i32 storeInnerW = storeW;
    render_.drawRect(static_cast<f32>(storeX), 0.0f,
                     static_cast<f32>(storeInnerW), static_cast<f32>(h),
                     layout_.storePanelBgColor);

    // store title (#storeTitle)
    render_.drawText(static_cast<f32>(storeX + layout_.storeTitleX),
                     static_cast<f32>(layout_.storeTitleY), "STORE",
                     layout_.storeTitleColor, layout_.storeTitleScale);

    i32 rowY = layout_.storeFirstRowY; // below the store title
    i32 row = 0;
    for (auto& b : game_.objects) {
        if (!b) continue;
        if (rowY + layout_.storeRowHeight > h) break; // don't draw past the bottom

        bool flashing = (game_.flashBuildingIndexValue() == row && game_.flashFramesValue() > 0);

        // store tile background (storeTile.png, 300x256 = 4 strips of 64px).
        // Each row is a "cell" using one horizontal strip of the tile sheet.
        if (!storeTileTex) storeTileTex = render_.loadTexture("storeTile.png");
        bool canAfford = game_.cookies >= b->getPrice();
        // Permanent unlock: lifetime cookies earned >= base price
        if (!b->locked && game_.cookiesEarned >= b->basePrice) {
            // already unlocked, keep unlocked
        } else if (game_.cookiesEarned >= b->basePrice) {
            b->locked = false;
        }
        bool isLocked = b->locked;
        bool isEnabled = canAfford && !isLocked;
        u32 tileColor = (flashing || isEnabled) ? layout_.tileColorEnabled : layout_.tileColorDisabled;
        f32 stripY = static_cast<f32>((row % 4) * 64);
        // Draw the 300px-wide tile strip at its natural width (centered in the row).
        f32 cellX = static_cast<f32>(storeX) + (static_cast<f32>(storeInnerW) - static_cast<f32>(layout_.storeTileWidth)) / 2.0f;
        render_.drawTexture(storeTileTex,
                            cellX, static_cast<f32>(rowY),
                            static_cast<f32>(layout_.storeTileWidth), static_cast<f32>(layout_.storeTileStripHeight),
                            tileColor,
                            0.0f, stripY, static_cast<f32>(layout_.storeTileWidth), static_cast<f32>(layout_.storeTileStripHeight));

        // building icon (64x64) — only visible when unlocked
        if (!isLocked) {
            std::string iconBase = b->art.base.empty() ? b->name : b->art.base;
            for (auto& c : iconBase)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string iconPath = iconBase + ".png";
            i32 iconHandle = loadIcon(iconPath);
            // cursor.png is a 2x2 sprite sheet of 4 cursors (64x64 total).
            // Show only the top-left quadrant (white index finger, 32x32),
            // stretched to fill the 64x64 icon slot.
            f32 srcX = 0.0f, srcY = 0.0f, srcW = 0.0f, srcH = 0.0f; // 0 = full texture
            if (iconPath == "cursor.png") {
                srcX = 0.0f; srcY = 0.0f; srcW = 32.0f; srcH = 32.0f;
            }
            render_.drawTexture(iconHandle,
                                static_cast<f32>(storeX + layout_.storeIconX), static_cast<f32>(rowY),
                                static_cast<f32>(layout_.storeIconSize), static_cast<f32>(layout_.storeIconSize),
                                0xFFFFFFFF,
                                srcX, srcY, srcW, srcH);
        }

        // building name (.product .content, left 64px) — "???" when locked.
        // Name is vertically centred in the cell, slightly above centre per Image 2.
        std::string displayName = isLocked ? "???" : b->name;
        u32 nameColor = isEnabled ? layout_.storeNameColorEnabled : layout_.storeNameColorDisabled;
        f32 nameScale = layout_.storeNameScale;
        f32 nameCx = render_.measureTextCenterY(displayName.c_str(), nameScale);
        f32 nameY = static_cast<f32>(rowY + 32) - nameCx; // centre of 64px cell
        // text shadow (dark offset behind main text)
        render_.drawText(static_cast<f32>(storeX + layout_.storeNameX + 1), nameY + 1.0f, displayName.c_str(),
                         layout_.storeNameShadowColor, nameScale);
        render_.drawText(static_cast<f32>(storeX + layout_.storeNameX), nameY, displayName.c_str(),
                         nameColor, nameScale);

        // price (.price: green #6f6 when affordable, red #f66 when locked/unaffordable)
        // Stacked right under the name, per Image 2.
        std::string priceStr = formatCookies(b->getPrice());
        f32 pw = render_.measureText(priceStr.c_str(), layout_.storePriceScale);
        u32 priceColor = isEnabled ? layout_.storePriceColorAffordable : layout_.storePriceColorNot;
        f32 priceY = nameY + nameScale * 14.0f + static_cast<f32>(layout_.storePriceGapBelowName);
        // small cookie icon before price (16x16 at natural size, clipped to cell)
        if (!priceCookieTex) priceCookieTex = render_.loadTexture("smallCookies.png");
        f32 priceIconW = 0.0f, priceIconH = 0.0f;
        i32 priceIconWi = 0, priceIconHi = 0;
        render_.textureSize(priceCookieTex, &priceIconWi, &priceIconHi);
        if (priceIconWi > 0 && priceIconHi > 0) {
            priceIconW = static_cast<f32>(priceIconWi);
            priceIconH = static_cast<f32>(priceIconHi);
            f32 iconScale = 16.0f / static_cast<f32>(priceIconW);
            f32 iconDrawW = static_cast<f32>(priceIconW) * iconScale;
            f32 iconDrawH = static_cast<f32>(priceIconH) * iconScale;
            f32 iconY = priceY + (14.0f - iconDrawH) / 2.0f;
            render_.drawTexture(priceCookieTex,
                                static_cast<f32>(storeX + storeInnerW - layout_.storePriceRightEdge) - pw - iconDrawW - static_cast<f32>(layout_.storePriceIconGap),
                                iconY,
                                iconDrawW, iconDrawH, priceColor);
        }
        render_.drawText(static_cast<f32>(storeX + storeInnerW - layout_.storePriceRightEdge) - pw,
                         priceY, priceStr.c_str(),
                         priceColor, layout_.storePriceScale);

        // big faded amount-owned (.product .content .owned)
        // Repositioned to bottom-right corner of the cell (right:12px),
        // vertically centred in the cell.
        if (b->amount > 0) {
            std::string ownedStr = std::to_string(b->amount);
            const f32 scale = layout_.storeOwnedScale;
            f32 ow = render_.measureText(ownedStr.c_str(), scale);
            // right edge of the 300px cell, minus 12px margin, minus text width
            f32 ownedX = cellX + static_cast<f32>(layout_.storeTileWidth) - static_cast<f32>(layout_.storeOwnedRightMargin) - ow;
            // vertically centre the digit inside the 64px-tall cell
            f32 cy = render_.measureTextCenterY(ownedStr.c_str(), scale);
            render_.drawText(ownedX,
                             static_cast<f32>(rowY + 32 - cy), ownedStr.c_str(),
                             layout_.storeOwnedColor, scale);
        }

        // Click anywhere in the row to buy (no separate BUY button).
        render_.registerButton(1000 + row, static_cast<f32>(storeX),
                               static_cast<f32>(rowY),
                               static_cast<f32>(storeInnerW), static_cast<f32>(layout_.storeRowHeight));

        // Cell separator line between rows (grid of cells effect).
        render_.drawRect(static_cast<f32>(storeX), static_cast<f32>(rowY + layout_.storeRowHeight),
                         static_cast<f32>(storeInnerW), 1.0f, layout_.storeSeparatorColor);

        rowY += layout_.storeRowStep; // 64px tile + 8px gap
        ++row;
    }

    // --- toasts (skeleton): draw stacked upward from bottom-left of cookie panel
    if (!toasts.empty()) {
        i32 leftW = static_cast<i32>(static_cast<f32>(render_.windowW()) * layout_.leftPanelWidthFraction);
        f32 toastY = static_cast<f32>(render_.windowH()) - 40.0f; // start near bottom of screen
        for (auto& t : toasts) {
            f32 tw = render_.measureText(t.text.c_str(), layout_.toastScale);
            // background pill
            render_.drawRect(layout_.toastX - 4.0f, toastY - 4.0f,
                             tw + 8.0f, static_cast<f32>(16 * layout_.toastScale) + 8.0f,
                             layout_.toastBgColor);
            render_.drawText(layout_.toastX, toastY, t.text.c_str(),
                             layout_.toastColor, layout_.toastScale);
            toastY -= layout_.toastSpacing;
        }
    }
}

GameAction UI::on_pointer(i32 x, i32 y) {
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
    return GameAction{GameAction::Type::None, -1};
}
