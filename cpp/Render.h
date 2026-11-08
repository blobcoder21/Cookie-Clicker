#pragma once

#include "Types.h"
#include <string>

// ---------------------------------------------------------------------------
// Internal font reference.
//
// Fonts are referred to by an internal ID rather than by filename, so that
// reserved font names (per SIL OFL) never leak out of the render backend.
// The PC backend maps each internal ID to an on-disk path inside Render.cpp.
// ---------------------------------------------------------------------------
enum class InternalFont : i32 {
    Default = 0,   // primary game font (Kavoon-Regular on PC)
    Fallback = 1,  // embedded fallback (Tahoma)
};

// ---------------------------------------------------------------------------
// Platform rendering abstraction.
//
// The game core (Game, Building, SaveLoad, ...) must NOT know about SDL,
// OpenGL, or ImGui. All rendering goes through these pure-C++ primitives;
// the PC backend (Render.cpp / main.cpp) implements them with SDL2 + OpenGL,
// libpng (PNG decoding) and FreeType (TTF text).
//
// Texture handles are opaque i32s so the core never sees a GLuint.
// ---------------------------------------------------------------------------
class Render {
public:
    Render();
    ~Render();

    // Lifecycle -------------------------------------------------------------
    void init(void* window);        // window is an SDL_Window*
    void shutdown();
    void newFrame();                // clear + reset orthographic projection
    void render();                  // present / swap buffers

    // 2D drawing (screen pixels, origin top-left, y down) -----------------
    // color is 0xRRGGBBAA
    void drawRect(f32 x, f32 y, f32 w, f32 h, u32 color);
    void drawText(f32 x, f32 y, const char* text, u32 color,
                         f32 scale = 1.0f);
    f32 measureText(const char* text, f32 scale = 1.0f);
    // Returns the offset from drawText's y to the text's visual vertical centre
    // (typically negative for digits, since the centre sits above y).
    f32 measureTextCenterY(const char* text, f32 scale = 1.0f);

    // Images (loaded from img/) ---------------------------------------------
    i32 loadTexture(const std::string& path);   // returns opaque handle
    // Draw texture (optionally a sub-rectangle srcX/srcY/srcW/srcH in texture
    // pixels; pass 0 for srcW/srcH to use the full texture).
    void drawTexture(i32 handle, f32 x, f32 y, f32 w, f32 h,
                            u32 color = 0xFFFFFFFF,
                            f32 srcX = 0.0f, f32 srcY = 0.0f,
                            f32 srcW = 0.0f, f32 srcH = 0.0f);

    // Tile a texture along one axis inside [x, x+w] x [y, y+h] WITHOUT scaling.
    // axis == 0 tiles horizontally (repeat across w, clip last tile to fit);
    // axis == 1 tiles vertically. The texture is drawn at its natural size; on
    // the non-tiled axis a single tile is placed at natural size. All positions
    // are snapped to whole pixels so you don't get shimmering seams. Draw the
    // strips first, then any joint/end caps on top afterwards so they cover the
    // tile boundaries.
    void drawTextureTiled(i32 handle, f32 x, f32 y, f32 w, f32 h,
                                 u32 color = 0xFFFFFFFF, i32 axis = 0);

    // Returns texture dimensions in source pixels (false if not loaded).
    bool textureSize(i32 handle, i32* w, i32* h);

    // Clickable areas (registered during draw; cleared each newFrame) -------
    void registerCookie(f32 x, f32 y, f32 r);
    void registerButton(i32 id, f32 x, f32 y, f32 w, f32 h);
    i32 hitTest(i32 mx, i32 my);  // HIT_COOKIE / building id / HIT_NONE

    // Window size -----------------------------------------------------------
    i32 windowW();
    i32 windowH();

    static constexpr i32 HIT_COOKIE = -1;
    static constexpr i32 HIT_BUILDING_BASE = 1000;
    static constexpr i32 HIT_NONE = -2;
};
