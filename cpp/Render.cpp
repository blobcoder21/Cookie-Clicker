#include "Render.h"

#include <SDL2/SDL.h>
#include <GL/gl.h>

#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "EmbeddedAssets.h"

#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <dirent.h>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

namespace {

SDL_Window* g_window = nullptr;
i32 g_w = 900;
i32 g_h = 600;

FT_Library g_ft = nullptr;
FT_Face g_face = nullptr;

// ---------------------------------------------------------------------------
// Textures (opaque handles -> OpenGL id + dimensions)
// ---------------------------------------------------------------------------
struct Texture {
    GLuint id = 0;
    i32 w = 0;
    i32 h = 0;
};
std::unordered_map<std::string, Texture> g_textures;
std::unordered_map<i32, Texture> g_handleTex;
i32 g_nextTexHandle = 1;

// Asset root (run from repo root; images live in img/, fonts in fonts/)
// Reassigned at init to an extracted temp dir so the executable is
// self-contained regardless of working directory.
std::string kImgDir = "img";
std::string kFontDir = "fonts";

// Temp dir holding extracted embedded assets (cleaned up on shutdown)
std::string g_tempAssetDir;

// ---------------------------------------------------------------------------
// Case-insensitive asset lookup (robust against filename case mismatches)
// ---------------------------------------------------------------------------
std::unordered_map<std::string, std::string> g_assetCaseMap;
bool g_assetCaseMapBuilt = false;

std::string toLower(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// ---------------------------------------------------------------------------
// Embedded asset extraction (writes static assets into a temp dir so the
// executable is self-contained regardless of working directory)
// ---------------------------------------------------------------------------
struct EmbeddedFile {
    const char* virtualPath;  // e.g. "img/farm.png" or "fonts/Tahoma-Bold-Font.ttf"
    const unsigned char* data;
    size_t len;
};

static const EmbeddedFile g_embeddedFiles[] = {
    {"img/perfectCookie.png", EmbeddedAssets::img_perfectCookie_png, EmbeddedAssets::img_perfectCookie_png_len},
    {"img/cursor.png", EmbeddedAssets::img_cursor_png, EmbeddedAssets::img_cursor_png_len},
    {"img/grandma.png", EmbeddedAssets::img_grandma_png, EmbeddedAssets::img_grandma_png_len},
    {"img/farm.png", EmbeddedAssets::img_farm_png, EmbeddedAssets::img_farm_png_len},
    {"img/mine.png", EmbeddedAssets::img_mine_png, EmbeddedAssets::img_mine_png_len},
    {"img/factory.png", EmbeddedAssets::img_factory_png, EmbeddedAssets::img_factory_png_len},
    {"img/bank.png", EmbeddedAssets::img_bank_png, EmbeddedAssets::img_bank_png_len},
    {"img/temple.png", EmbeddedAssets::img_temple_png, EmbeddedAssets::img_temple_png_len},
    {"img/wizardtower.png", EmbeddedAssets::img_wizardtower_png, EmbeddedAssets::img_wizardtower_png_len},
    {"img/shipment.png", EmbeddedAssets::img_shipment_png, EmbeddedAssets::img_shipment_png_len},
    {"img/alchemylab.png", EmbeddedAssets::img_alchemylab_png, EmbeddedAssets::img_alchemylab_png_len},
    {"img/portal.png", EmbeddedAssets::img_portal_png, EmbeddedAssets::img_portal_png_len},
    {"img/timemachine.png", EmbeddedAssets::img_timemachine_png, EmbeddedAssets::img_timemachine_png_len},
    {"img/antimattercondenser.png", EmbeddedAssets::img_antimattercondenser_png, EmbeddedAssets::img_antimattercondenser_png_len},
    {"img/prism.png", EmbeddedAssets::img_prism_png, EmbeddedAssets::img_prism_png_len},
    {"img/chancemaker.png", EmbeddedAssets::img_chancemaker_png, EmbeddedAssets::img_chancemaker_png_len},
    {"img/fractalEngine.png", EmbeddedAssets::img_fractalEngine_png, EmbeddedAssets::img_fractalEngine_png_len},
    {"img/javascriptconsole.png", EmbeddedAssets::img_javascriptconsole_png, EmbeddedAssets::img_javascriptconsole_png_len},
    {"img/idleverse.png", EmbeddedAssets::img_idleverse_png, EmbeddedAssets::img_idleverse_png_len},
    {"img/bgBlue.png", EmbeddedAssets::img_bgBlue_png, EmbeddedAssets::img_bgBlue_png_len},
    {"img/storeTile.png", EmbeddedAssets::img_storeTile_png, EmbeddedAssets::img_storeTile_png_len},
    {"fonts/Tahoma-Bold-Font.ttf", EmbeddedAssets::fonts_Tahoma_Bold_Font_ttf, EmbeddedAssets::fonts_Tahoma_Bold_Font_ttf_len},
};

bool writeFile(const std::string& path, const unsigned char* data, size_t len) {
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    bool ok = (std::fwrite(data, 1, len, fp) == len);
    std::fclose(fp);
    return ok;
}

std::string extractEmbeddedAssets() {
    // Create temp dir: /tmp/cookie_clicker_assets_XXXXXX
    char templatePath[] = "/tmp/cookie_clicker_assets_XXXXXX";
    if (mkdtemp(templatePath) == nullptr) {
        std::fprintf(stderr, "[Render] mkdtemp failed, falling back to cwd assets\n");
        return std::string();
    }
    g_tempAssetDir = templatePath;

    // Create img/ and fonts/ subdirs
    std::string imgDir = g_tempAssetDir + "/img";
    std::string fontDir = g_tempAssetDir + "/fonts";
    ::mkdir(imgDir.c_str(), 0755);
    ::mkdir(fontDir.c_str(), 0755);

    for (const auto& ef : g_embeddedFiles) {
        std::string outPath = g_tempAssetDir + "/" + ef.virtualPath;
        // ensure parent dir exists (create any missing /img or /fonts)
        size_t slash = outPath.find_last_of('/');
        if (slash != std::string::npos) {
            std::string parent = outPath.substr(0, slash);
            ::mkdir(parent.c_str(), 0755);
        }
        if (!writeFile(outPath, ef.data, ef.len)) {
            std::fprintf(stderr, "[Render] failed to write embedded asset: %s\n", outPath.c_str());
        }
    }

    // Point asset dirs at the extracted temp dir
    kImgDir = g_tempAssetDir + "/img";
    kFontDir = g_tempAssetDir + "/fonts";

    std::fprintf(stderr, "[Render] embedded assets extracted to %s\n", g_tempAssetDir.c_str());
    return g_tempAssetDir;
}

void cleanupEmbeddedAssets() {
    if (!g_tempAssetDir.empty()) {
        // Remove files then the dir tree (simple rm -rf via system is avoided;
        // we leave temp files in /tmp — harmless, OS cleans on reboot).
        g_tempAssetDir.clear();
    }
}

void buildAssetCaseMap() {
    if (g_assetCaseMapBuilt) return;
    g_assetCaseMapBuilt = true;
    DIR* dir = opendir(kImgDir.c_str());
    if (!dir) return;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        g_assetCaseMap[toLower(name)] = name;
    }
    closedir(dir);
}

std::string resolveAssetPath(const std::string& path) {
    // exact path first
    std::string fullPath = std::string(kImgDir) + "/" + path;
    FILE* fp = std::fopen(fullPath.c_str(), "rb");
    if (fp) { std::fclose(fp); return fullPath; }
    // then try path as-is
    fp = std::fopen(path.c_str(), "rb");
    if (fp) { std::fclose(fp); return path; }
    // then try cwd-relative img/<path> (dev workflow: repo root has img/ with
    // every source asset; embedded assets already live under kImgDir)
    std::string cwdImg = std::string("img/") + path;
    fp = std::fopen(cwdImg.c_str(), "rb");
    if (fp) { std::fclose(fp); return cwdImg; }
    // then case-insensitive match on the filename portion
    buildAssetCaseMap();
    size_t slash = path.find_last_of("/\\");
    std::string fname = (slash == std::string::npos) ? path : path.substr(slash + 1);
    auto it = g_assetCaseMap.find(toLower(fname));
    if (it != g_assetCaseMap.end()) {
        std::string resolved = std::string(kImgDir) + "/" + it->second;
        FILE* fp2 = std::fopen(resolved.c_str(), "rb");
        if (fp2) { std::fclose(fp2); return resolved; }
    }
    return fullPath; // fallback; loadPNG will fail and we log
}

// ---------------------------------------------------------------------------
// PNG loading via libpng
// ---------------------------------------------------------------------------
struct PngImage {
    i32 w = 0, h = 0;
    std::vector<u8> rgba; // premultiplied-on-upload RGBA
};

bool loadPNG(const std::string& path, PngImage& out) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;

    u8 sig[8];
    if (std::fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8) != 0) {
        std::fclose(fp);
        return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                             nullptr, nullptr);
    if (!png) { std::fclose(fp); return false; }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, nullptr, nullptr);
                 std::fclose(fp); return false; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(fp);
        return false;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    png_uint_32 width, height;
    int bit_depth, color_type;
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type,
                 nullptr, nullptr, nullptr);

    // Normalize to 8-bit RGB/RGBA
    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY &&
        bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    // If a tRNS chunk exists, convert its transparency to an alpha channel.
    // Do NOT also call png_set_add_alpha afterward — it would overwrite
    // the tRNS-derived alpha with 0xFF, making transparent areas opaque.
    bool has_alpha = false;
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
        has_alpha = true;
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    if (!has_alpha &&
        (color_type == PNG_COLOR_TYPE_RGB ||
         color_type == PNG_COLOR_TYPE_GRAY ||
         color_type == PNG_COLOR_TYPE_PALETTE))
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);

    png_read_update_info(png, info);

    int rowbytes = png_get_rowbytes(png, info);
    i32 w = static_cast<i32>(width);
    i32 h = static_cast<i32>(height);
    out.w = w;
    out.h = h;
    out.rgba.resize(static_cast<size_t>(rowbytes) * h);

    std::vector<png_bytep> row_ptrs(h);
    for (i32 i = 0; i < h; ++i)
        row_ptrs[i] = out.rgba.data() + static_cast<size_t>(i) * rowbytes;
    png_read_image(png, row_ptrs.data());

    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(fp);
    return true;
}

GLuint uploadTexture(const PngImage& img) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.w, img.h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, img.rgba.data());
    return id;
}

} // namespace

// ---------------------------------------------------------------------------
// Drawing (outside the anonymous namespace so Render member functions link)
// ---------------------------------------------------------------------------
void Render::drawRect(f32 x, f32 y, f32 w, f32 h, u32 color) {
    f32 r = ((color >> 24) & 0xFF) / 255.0f;
    f32 g_ = ((color >> 16) & 0xFF) / 255.0f;
    f32 b = ((color >> 8) & 0xFF) / 255.0f;
    f32 a = (color & 0xFF) / 255.0f;
    glColor4f(r, g_, b, a);
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    glColor4f(1, 1, 1, 1);
    glEnable(GL_TEXTURE_2D);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void Render::init(void* window) {
    extractEmbeddedAssets(); // self-contained assets regardless of cwd

    g_window = static_cast<SDL_Window*>(window);
    SDL_GL_CreateContext(g_window);
    SDL_GL_SetSwapInterval(1);
    SDL_GetWindowSize(g_window, &g_w, &g_h);

    if (FT_Init_FreeType(&g_ft) != 0) {
        g_ft = nullptr;
    }

    glViewport(0, 0, g_w, g_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<GLdouble>(g_w), static_cast<GLdouble>(g_h), 0.0,
            -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Render::shutdown() {
    cleanupEmbeddedAssets();

    for (auto& kv : g_textures) {
        if (kv.second.id) glDeleteTextures(1, &kv.second.id);
    }
    g_textures.clear();
    if (g_face) FT_Done_Face(g_face);
    if (g_ft) FT_Done_FreeType(g_ft);
    g_window = nullptr;
}

void Render::newFrame() {
    SDL_GetWindowSize(g_window, &g_w, &g_h);
    glViewport(0, 0, g_w, g_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<GLdouble>(g_w), static_cast<GLdouble>(g_h), 0.0,
            -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClear(GL_COLOR_BUFFER_BIT);
}

void Render::render() {
    SDL_GL_SwapWindow(g_window);
}

// ---------------------------------------------------------------------------
// Texture handle management
// ---------------------------------------------------------------------------
i32 Render::loadTexture(const std::string& path) {
    auto it = g_textures.find(path);
    if (it != g_textures.end() && it->second.id != 0) {
        // find existing handle
        for (auto& ht : g_handleTex) {
            if (ht.second.id == it->second.id) return ht.first;
        }
    }
    i32 handle = g_nextTexHandle++;

    PngImage img;
    std::string resolved = resolveAssetPath(path);
    if (!loadPNG(resolved, img)) {
        std::fprintf(stderr, "[Render] FAILED to load texture: %s (tried %s) -> magenta fallback\n",
                     path.c_str(), resolved.c_str());
        PngImage fb;
        fb.w = 8; fb.h = 8;
        fb.rgba.assign(8 * 8 * 4, 0);
        for (i32 i = 0; i < 8 * 8 * 4; i += 4) {
            fb.rgba[i + 0] = 255;
            fb.rgba[i + 1] = 0;
            fb.rgba[i + 2] = 255;
            fb.rgba[i + 3] = 255;
        }
        GLuint id = uploadTexture(fb);
        Texture t{id, 8, 8};
        g_textures[path] = t;
        g_handleTex[handle] = t;
        return handle;
    }

    std::fprintf(stderr, "[Render] loaded texture: %s -> %s (%dx%d)\n",
                 path.c_str(), resolved.c_str(), img.w, img.h);
    GLuint id = uploadTexture(img);
    Texture t{id, img.w, img.h};
    g_textures[path] = t;
    g_handleTex[handle] = t;
    return handle;
}

void Render::drawTexture(i32 handle, f32 x, f32 y, f32 w, f32 h, u32 color,
                         f32 srcX, f32 srcY, f32 srcW, f32 srcH) {
    auto it = g_handleTex.find(handle);
    if (it == g_handleTex.end() || !it->second.id) return;
    GLuint id = it->second.id;
    const Texture& t = it->second;

    f32 r = ((color >> 24) & 0xFF) / 255.0f;
    f32 g = ((color >> 16) & 0xFF) / 255.0f;
    f32 b = ((color >> 8) & 0xFF) / 255.0f;
    f32 a = (color & 0xFF) / 255.0f;
    glColor4f(r, g, b, a);

    // drawText() leaves GL_TEXTURE_2D disabled; re-enable it here so the
    // texture is actually applied (otherwise the quad renders flat white).
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, id);

    // Source rectangle in texture pixels (0 = full texture).
    f32 texW = static_cast<f32>(t.w);
    f32 texH = static_cast<f32>(t.h);
    if (srcW <= 0.0f) srcW = texW;
    if (srcH <= 0.0f) srcH = texH;
    f32 u0 = srcX / texW;
    f32 v0 = srcY / texH;
    f32 u1 = (srcX + srcW) / texW;
    f32 v1 = (srcY + srcH) / texH;

    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0); glVertex2f(x, y);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);
    glTexCoord2f(u1, v1); glVertex2f(x + w, y + h);
    glTexCoord2f(u0, v1); glVertex2f(x, y + h);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glColor4f(1, 1, 1, 1);
}

void Render::drawTextureTiled(i32 handle, f32 x, f32 y, f32 w, f32 h,
                              u32 color, i32 axis) {
    auto it = g_handleTex.find(handle);
    if (it == g_handleTex.end() || !it->second.id) return;
    const Texture& t = it->second;
    if (t.w <= 0 || t.h <= 0) return;

    // Snap to whole pixels so tiled strips don't shimmer at fractional offsets.
    i32 sx = static_cast<i32>(std::floor(x + 0.5f));
    i32 sy = static_cast<i32>(std::floor(y + 0.5f));
    i32 sw = static_cast<i32>(std::floor(w + 0.5f));
    i32 sh = static_cast<i32>(std::floor(h + 0.5f));
    if (sw <= 0 || sh <= 0) return;

    if (axis == 0) {
        // Horizontal strip: tile across the width, single tile tall at natural size.
        // Clip the last tile so it ends exactly at sx + sw (no overhang/seam).
        i32 ty = sy;
        i32 th = t.h;
        i32 cx = sx;
        while (cx < sx + sw) {
            i32 cw = t.w;
            if (cx + cw > sx + sw) cw = sx + sw - cx; // clip last tile to exact edge
            if (cw <= 0) break;
            // 1:1 mapping (dest size == source size) -> no scaling.
            Render::drawTexture(handle, static_cast<f32>(cx), static_cast<f32>(ty),
                                static_cast<f32>(cw), static_cast<f32>(th), color,
                                0.0f, 0.0f, static_cast<f32>(cw), static_cast<f32>(th));
            cx += t.w;
        }
    } else {
        // Vertical strip: tile down the height, single tile wide at natural size.
        // Clip the last tile so it ends exactly at sy + sh.
        i32 tx = sx;
        i32 tw = t.w;
        i32 cy = sy;
        while (cy < sy + sh) {
            i32 ch = t.h;
            if (cy + ch > sy + sh) ch = sy + sh - cy; // clip last tile to exact edge
            if (ch <= 0) break;
            Render::drawTexture(handle, static_cast<f32>(tx), static_cast<f32>(cy),
                                static_cast<f32>(tw), static_cast<f32>(ch), color,
                                0.0f, 0.0f, static_cast<f32>(tw), static_cast<f32>(ch));
            cy += t.h;
        }
    }
}

bool Render::textureSize(i32 handle, i32* w, i32* h) {
    auto it = g_handleTex.find(handle);
    if (it == g_handleTex.end() || !it->second.id) return false;
    if (w) *w = it->second.w;
    if (h) *h = it->second.h;
    return true;
}

// ---------------------------------------------------------------------------
// Text rendering via FreeType (cached glyph alpha textures)
// ---------------------------------------------------------------------------
struct Glyph {
    GLuint id = 0;
    i32 w = 0, h = 0;
    i32 bearingX = 0, bearingY = 0;
    i32 advance = 0; // in 26.6 fixed point -> pixels
};
std::unordered_map<char, Glyph> g_glyphs;
f32 g_currentPtSize = 0.0f; // tracked so we can reload glyphs when size changes

void ensureFace(f32 ptSize) {
    if (!g_ft) {
        static bool logged_ft = false;
        if (!logged_ft) {
            std::fprintf(stderr, "[Render] FreeType failed to initialize; text disabled\n");
            logged_ft = true;
        }
        return;
    }
    if (!g_face) {
        // Try Merriweather fonts from cwd first (preferred per style.css),
        // then fall back to embedded Tahoma.
        std::vector<std::string> fontPaths = {
            "fonts/Merriweather_24pt-Bold.ttf",
            "fonts/Merriweather-VariableFont_opsz,wdth,wght.ttf",
            "fonts/Merriweather_24pt-Regular.ttf",
            std::string(kFontDir) + "/Tahoma-Bold-Font.ttf",
        };
        bool loaded = false;
        for (const auto& path : fontPaths) {
            if (FT_New_Face(g_ft, path.c_str(), 0, &g_face) == 0) {
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            g_face = nullptr;
            static bool logged_font = false;
            if (!logged_font) {
                std::fprintf(stderr, "[Render] failed to load any font\n");
                logged_font = true;
            }
            return;
        }
    }
    // Reload glyphs whenever the face size changes, since g_glyphs is keyed
    // by character only and FT_Load_Char renders at the current face size.
    if (g_face && ptSize != g_currentPtSize) {
        FT_Set_Char_Size(g_face, 0, static_cast<FT_F26Dot6>(ptSize * 64), 96, 96);
        g_currentPtSize = ptSize;
        g_glyphs.clear();
    }
}

Glyph& getGlyph(char c) {
    auto it = g_glyphs.find(c);
    if (it != g_glyphs.end()) return it->second;
    Glyph g;
    if (g_face && FT_Load_Char(g_face, c, FT_LOAD_RENDER) == 0) {
        FT_Bitmap& bm = g_face->glyph->bitmap;
        g.w = bm.width;
        g.h = bm.rows;
        g.bearingX = g_face->glyph->bitmap_left;
        g.bearingY = g_face->glyph->bitmap_top;
        g.advance = g_face->glyph->advance.x / 64;

        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // alpha-only texture
        // FreeType bitmaps may have row padding (bm.pitch != bm.width).
        // Tell OpenGL the real row stride so glyphs upload without diagonal tearing.
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, bm.pitch > 0 ? bm.pitch : bm.width);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, bm.width, bm.rows, 0,
                     GL_ALPHA, GL_UNSIGNED_BYTE, bm.buffer);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        g.id = id;
    }
    return g_glyphs[c] = g;
}

void Render::drawText(f32 x, f32 y, const char* text, u32 color, f32 scale) {
    if (!text) return;
    ensureFace(16.0f * scale);

    f32 r = ((color >> 24) & 0xFF) / 255.0f;
    f32 g_ = ((color >> 16) & 0xFF) / 255.0f;
    f32 b = ((color >> 8) & 0xFF) / 255.0f;
    f32 a = (color & 0xFF) / 255.0f;
    glColor4f(r, g_, b, a);

    glEnable(GL_TEXTURE_2D);
    f32 cx = x;
    for (const char* p = text; *p; ++p) {
        if (*p == ' ') {
            cx += 8.0f * scale; // space width
            continue;
        }
        Glyph& g = getGlyph(*p);
        if (!g.id) {
            cx += 8.0f * scale;
            continue;
        }
        glBindTexture(GL_TEXTURE_2D, g.id);
        f32 gx = cx + g.bearingX * scale;
        f32 gy = y - g.bearingY * scale;
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(gx, gy);
        glTexCoord2f(1, 0); glVertex2f(gx + g.w * scale, gy);
        glTexCoord2f(1, 1); glVertex2f(gx + g.w * scale, gy + g.h * scale);
        glTexCoord2f(0, 1); glVertex2f(gx, gy + g.h * scale);
        glEnd();
        cx += g.advance * scale;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);
}

f32 Render::measureText(const char* text, f32 scale) {
    if (!text) return 0.0f;
    ensureFace(16.0f * scale);
    f32 w = 0.0f;
    for (const char* p = text; *p; ++p) {
        if (*p == ' ') { w += 8.0f * scale; continue; }
        Glyph& g = getGlyph(*p);
        if (!g.id) { w += 8.0f * scale; continue; }
        w += g.advance * scale;
    }
    return w;
}

f32 Render::measureTextCenterY(const char* text, f32 scale) {
    if (!text) return 0.0f;
    ensureFace(16.0f * scale);
    f32 maxTop = 0.0f;
    f32 maxBottom = 0.0f;
    bool first = true;
    for (const char* p = text; *p; ++p) {
        if (*p == ' ') continue;
        Glyph& g = getGlyph(*p);
        if (!g.id) continue;
        // glyph top/bottom relative to drawText's y (which sits at the baseline area)
        f32 top = -static_cast<f32>(g.bearingY) * scale;
        f32 bottom = top + static_cast<f32>(g.h) * scale;
        if (first) { maxTop = top; maxBottom = bottom; first = false; }
        else { if (top < maxTop) maxTop = top; if (bottom > maxBottom) maxBottom = bottom; }
    }
    if (first) return 0.0f;
    return (maxTop + maxBottom) / 2.0f; // visual centre offset from y
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------
struct HitArea {
    i32 kind = 0;
    i32 id = -1;
    f32 x = 0, y = 0, w = 0, h = 0, r = 0;
};
HitArea g_hits[64];
i32 g_hitsN = 0;

void Render::registerCookie(f32 x, f32 y, f32 r) {
    if (g_hitsN >= 64) return;
    g_hits[g_hitsN++] = {HIT_COOKIE, -1, x, y, 0, 0, r};
}

void Render::registerButton(i32 id, f32 x, f32 y, f32 w, f32 h) {
    if (g_hitsN >= 64) return;
    g_hits[g_hitsN++] = {HIT_BUILDING_BASE, id, x, y, w, h, 0};
}

i32 Render::hitTest(i32 mx, i32 my) {
    for (i32 i = 0; i < g_hitsN; ++i) {
        const HitArea& a = g_hits[i];
        if (a.kind == HIT_COOKIE) {
            f32 dx = static_cast<f32>(mx) - a.x;
            f32 dy = static_cast<f32>(my) - a.y;
            if (dx * dx + dy * dy <= a.r * a.r) return HIT_COOKIE;
        } else {
            if (mx >= static_cast<i32>(a.x) && mx <= static_cast<i32>(a.x + a.w) &&
                my >= static_cast<i32>(a.y) && my <= static_cast<i32>(a.y + a.h)) {
                return a.id;
            }
        }
    }
    return HIT_NONE;
}

// ---------------------------------------------------------------------------
// Window size
// ---------------------------------------------------------------------------
i32 Render::windowW() { return g_w; }
i32 Render::windowH() { return g_h; }
