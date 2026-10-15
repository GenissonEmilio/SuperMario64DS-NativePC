/* hal/char_door_skin.cpp: see char_door_skin.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "char_door_skin.h"
#include "mods.h"
#include "ui_text.h"
#include "../third_party/stb/stb_image.h"
#include "../tests/overlay_font.h"

extern "C" {
/* the HUD's life icons: one OamAttr list per character (src/_ZN3HUD15RenderLifeCountEv.cpp indexes it by the Player's character) */
extern void *_ZN3OAM10LIFE_ICONSE[];
}

namespace {

/* ================================================================ images ==================================================== */

struct Img {
    int w = 0, h = 0;
    std::vector<uint32_t> px;           /* premultiplied 0xAARRGGBB */
    bool ok() const { return w > 0 && h > 0 && !px.empty(); }
};

void load_file(const char *path, Img *out)
{
    *out = Img();
    if (!path || !*path) return;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (16L << 20)) { fclose(f); return; }
    std::vector<unsigned char> buf((size_t)n);
    const size_t got = fread(&buf[0], 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) return;
    int w = 0, h = 0, comp = 0;
    unsigned char *p = stbi_load_from_memory(&buf[0], (int)n, &w, &h, &comp, 4);
    if (!p) return;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) { stbi_image_free(p); return; }
    out->w = w;
    out->h = h;
    out->px.resize((size_t)w * h);
    for (int i = 0; i < w * h; ++i) {
        const unsigned r = p[i * 4], g = p[i * 4 + 1], b = p[i * 4 + 2], a = p[i * 4 + 3];
        out->px[i] = (a << 24) | (((r * a + 127) / 255) << 16) | (((g * a + 127) / 255) << 8) | ((b * a + 127) / 255);
    }
    stbi_image_free(p);
}

inline uint32_t over(uint32_t d, int sr, int sg, int sb, int sa /* premultiplied colour, alpha */)
{
    const int ia = 255 - sa;
    const int r = sr + (((d >> 16) & 255) * ia + 127) / 255, g = sg + (((d >> 8) & 255) * ia + 127) / 255, b = sb + ((d & 255) * ia + 127) / 255;
    return 0xFF000000u | (uint32_t)((r > 255 ? 255 : r) << 16) | (uint32_t)((g > 255 ? 255 : g) << 8) | (uint32_t)(b > 255 ? 255 : b);
}

struct Surf { uint32_t *px; int stride, w, h; };

/* premultiplied pixel src over the surface at (x, y), alpha scaled by a / 255 */
inline void put_pm(const Surf &s, int x, int y, uint32_t c, int a = 255)
{
    if (x < 0 || y < 0 || x >= s.w || y >= s.h) return;
    int sa = (int)(c >> 24), sr = (int)((c >> 16) & 255), sg = (int)((c >> 8) & 255), sb = (int)(c & 255);
    if (a != 255) { sa = sa * a / 255; sr = sr * a / 255; sg = sg * a / 255; sb = sb * a / 255; }
    if (sa == 0) return;
    uint32_t *d = &s.px[(size_t)y * s.stride + x];
    *d = sa == 255 ? (0xFF000000u | (c & 0xFFFFFF)) : over(*d, sr, sg, sb, sa);
}

/* area / bilinear resample of the rectangle (sx, sy, sw, sh) of an image to dw x dh (premultiplied, so edges do not bleed) */
void scale_rect(const Img &I, double sx, double sy, double sw, double sh, int dw, int dh, std::vector<uint32_t> *out, bool nearest = false)
{
    out->assign((size_t)(dw > 0 ? dw : 0) * (dh > 0 ? dh : 0), 0);
    if (!I.ok() || dw < 1 || dh < 1 || sw <= 0 || sh <= 0) return;
    const double kx = sw / dw, ky = sh / dh;
    if (nearest) {   /* a small pixel-art picture blown up: every source pixel stays a hard square */
        for (int y = 0; y < dh; ++y)
            for (int x = 0; x < dw; ++x) {
                int xi = (int)(sx + (x + 0.5) * kx), yi = (int)(sy + (y + 0.5) * ky);
                if (xi < 0) xi = 0;
                if (yi < 0) yi = 0;
                if (xi > I.w - 1) xi = I.w - 1;
                if (yi > I.h - 1) yi = I.h - 1;
                (*out)[(size_t)y * dw + x] = I.px[(size_t)yi * I.w + xi];
            }
        return;
    }
    int nx = (int)ceil(kx), ny = (int)ceil(ky);
    if (nx < 1) nx = 1;
    if (ny < 1) ny = 1;
    if (nx > 6) nx = 6;
    if (ny > 6) ny = 6;
    for (int y = 0; y < dh; ++y)
        for (int x = 0; x < dw; ++x) {
            float a = 0, r = 0, g = 0, b = 0;
            for (int j = 0; j < ny; ++j)
                for (int i = 0; i < nx; ++i) {
                    double fx = sx + (x + (i + 0.5) / nx) * kx - 0.5, fy = sy + (y + (j + 0.5) / ny) * ky - 0.5;
                    if (fx < 0) fx = 0;
                    if (fy < 0) fy = 0;
                    if (fx > I.w - 1) fx = I.w - 1;
                    if (fy > I.h - 1) fy = I.h - 1;
                    const int x0 = (int)fx, y0 = (int)fy;
                    const int x1 = x0 + 1 < I.w ? x0 + 1 : x0, y1 = y0 + 1 < I.h ? y0 + 1 : y0;
                    const float tx = (float)(fx - x0), ty = (float)(fy - y0);
                    const uint32_t c00 = I.px[(size_t)y0 * I.w + x0], c10 = I.px[(size_t)y0 * I.w + x1];
                    const uint32_t c01 = I.px[(size_t)y1 * I.w + x0], c11 = I.px[(size_t)y1 * I.w + x1];
                    const float w00 = (1 - tx) * (1 - ty), w10 = tx * (1 - ty), w01 = (1 - tx) * ty, w11 = tx * ty;
                    a += w00 * (c00 >> 24) + w10 * (c10 >> 24) + w01 * (c01 >> 24) + w11 * (c11 >> 24);
                    r += w00 * ((c00 >> 16) & 255) + w10 * ((c10 >> 16) & 255) + w01 * ((c01 >> 16) & 255) + w11 * ((c11 >> 16) & 255);
                    g += w00 * ((c00 >> 8) & 255) + w10 * ((c10 >> 8) & 255) + w01 * ((c01 >> 8) & 255) + w11 * ((c11 >> 8) & 255);
                    b += w00 * (c00 & 255) + w10 * (c10 & 255) + w01 * (c01 & 255) + w11 * (c11 & 255);
                }
            const float inv = 1.0f / (nx * ny);
            int ia = (int)(a * inv + 0.5f), ir = (int)(r * inv + 0.5f), ig = (int)(g * inv + 0.5f), ib = (int)(b * inv + 0.5f);
            if (ia > 255) ia = 255;
            if (ir > ia) ir = ia;
            if (ig > ia) ig = ia;
            if (ib > ia) ib = ia;
            (*out)[(size_t)y * dw + x] = ((uint32_t)ia << 24) | ((uint32_t)ir << 16) | ((uint32_t)ig << 8) | (uint32_t)ib;
        }
}

/* a cache of finished pictures (a scaled image, a nine-slice plank, the sheet) keyed by what and how big */
struct Piece { int kind, a, w, h; std::vector<uint32_t> px; };
std::vector<Piece> g_pieces;

Piece *find_piece(int kind, int a, int w, int h)
{
    for (size_t i = 0; i < g_pieces.size(); ++i) {
        Piece &p = g_pieces[i];
        if (p.kind == kind && p.a == a && p.w == w && p.h == h) return &p;
    }
    return 0;
}
Piece *new_piece(int kind, int a, int w, int h)
{
    if (g_pieces.size() > 80) g_pieces.clear();
    g_pieces.push_back(Piece());
    Piece &p = g_pieces.back();
    p.kind = kind; p.a = a; p.w = w; p.h = h;
    return &p;
}

void blit(const Surf &s, int x, int y, const Piece &p, int a = 255)
{
    for (int j = 0; j < p.h; ++j) {
        const int yy = y + j;
        if (yy < 0 || yy >= s.h) continue;
        for (int i = 0; i < p.w; ++i) put_pm(s, x + i, yy, p.px[(size_t)j * p.w + i], a);
    }
}

/* ================================================================ the skins ================================================= */

enum { A_PARCH, A_TITLE, A_PLANK, A_BACK, A_KEY, A_COUNT };
const char *const ART_NAME[A_COUNT] = { "parchment", "title", "plank", "back", "key" };
Img g_art[A_COUNT];
int g_hd = -1;             /* -1 not decided yet */

/* the HD UI mod's art: <mod folder>/hd_ui/<rel>, the latest enabled mod that has it (mods.h: port_mods_ui_file). Null when no mod has it. */
bool ui_file(const char *rel, std::string *out)
{
    char path[1024];
    if (!port_mods_ui_file(rel, path, sizeof path)) return false;
    *out = path;
    return true;
}

void decide_skin(void)
{
    if (g_hd >= 0) return;
    g_hd = 0;
    const char *e = getenv("SM64DS_CHARMENU_PLAIN");
    if (e && *e && *e != '0') { fprintf(stderr, "[charmenu] SM64DS_CHARMENU_PLAIN: the plain skin\n"); return; }
    int ok = 1;
    for (int i = 0; i < A_COUNT; ++i) {
        std::string p;
        char rel[64];
        snprintf(rel, sizeof rel, "menu/settings/%s.png", ART_NAME[i]);
        if (!ui_file(rel, &p)) { ok = 0; break; }
        load_file(p.c_str(), &g_art[i]);
        if (!g_art[i].ok()) { ok = 0; break; }
    }
    if (ok && !ui_text_ready()) ok = 0;
    g_hd = ok;
    fprintf(stderr, "[charmenu] skin: %s\n", ok ? "HD UI Textures (parchment, planks, DS lettering)" : "plain (no HD UI Textures mod, or its art is missing)");
}

/* colours of the plain skin and the text colours of both */
const uint32_t C_SHEET = 0xE9C96Au, C_EDGE = 0x7A4614u, C_CREAM = 0xFFF1C9u, C_TEXT = 0x7A4614u, C_DIM = 0x8A5C22u, C_WOODTXT = 0xFFF3CCu;

/* a rounded rectangle, anti-aliased by distance, filled with a colour (or a premultiplied picture when pic is set; the picture is
   (pw x ph) and covers the rectangle from its top left) */
void rrect(const Surf &s, int x, int y, int w, int h, float r, uint32_t rgb, int alpha, const Piece *pic = 0)
{
    if (w < 1 || h < 1) return;
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    for (int j = 0; j < h; ++j) {
        const int yy = y + j;
        if (yy < 0 || yy >= s.h) continue;
        for (int i = 0; i < w; ++i) {
            const int xx = x + i;
            if (xx < 0 || xx >= s.w) continue;
            const float px = i + 0.5f, py = j + 0.5f;
            float dx = 0, dy = 0;
            if (px < r) dx = r - px; else if (px > w - r) dx = px - (w - r);
            if (py < r) dy = r - py; else if (py > h - r) dy = py - (h - r);
            float cov = 1.0f;
            if (dx > 0 && dy > 0) {
                const float d = sqrtf(dx * dx + dy * dy);
                cov = r - d + 0.5f;
                if (cov <= 0) continue;
                if (cov > 1) cov = 1;
            }
            const int ca = (int)(cov * alpha + 0.5f);
            if (ca <= 0) continue;
            if (pic && i < pic->w && j < pic->h) {
                put_pm(s, xx, yy, pic->px[(size_t)j * pic->w + i], ca);
            } else {
                const int r8 = (rgb >> 16) & 255, g8 = (rgb >> 8) & 255, b8 = rgb & 255;
                s.px[(size_t)yy * s.stride + xx] = over(s.px[(size_t)yy * s.stride + xx], r8 * ca / 255, g8 * ca / 255, b8 * ca / 255, ca);
            }
        }
    }
}

/* an outlined rounded rectangle: the edge colour, then the fill inside it */
void framed(const Surf &s, int x, int y, int w, int h, float r, int edge, uint32_t fill, uint32_t edge_rgb, int alpha = 255, const Piece *pic = 0)
{
    rrect(s, x, y, w, h, r, edge_rgb, alpha);
    rrect(s, x + edge, y + edge, w - 2 * edge, h - 2 * edge, r > edge ? r - edge : 0, fill, alpha, pic);
}

/* the nine-slice of an art image to dw x dh: corners are scaled by f (dest pixels per source pixel) so a chamfer keeps its shape */
const Piece *nine(int art, int dw, int dh, int il, int it, int ir, int ib, float f)
{
    if (Piece *p = find_piece(1, art, dw, dh)) return p;
    Piece *p = new_piece(1, art, dw, dh);
    p->px.assign((size_t)dw * dh, 0);
    const Img &I = g_art[art];
    int cl = (int)(il * f + 0.5f), ct = (int)(it * f + 0.5f), cr = (int)(ir * f + 0.5f), cb = (int)(ib * f + 0.5f);
    if (cl + cr > dw) { cl = dw / 2; cr = dw - cl; }
    if (ct + cb > dh) { ct = dh / 2; cb = dh - ct; }
    const int xs[4] = { 0, cl, dw - cr, dw }, ys[4] = { 0, ct, dh - cb, dh };
    const double sxs[4] = { 0, (double)il, (double)(I.w - ir), (double)I.w }, sys_[4] = { 0, (double)it, (double)(I.h - ib), (double)I.h };
    std::vector<uint32_t> tmp;
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i) {
            const int w = xs[i + 1] - xs[i], h = ys[j + 1] - ys[j];
            if (w < 1 || h < 1) continue;
            scale_rect(I, sxs[i], sys_[j], sxs[i + 1] - sxs[i], sys_[j + 1] - sys_[j], w, h, &tmp);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) p->px[(size_t)(ys[j] + y) * dw + xs[i] + x] = tmp[(size_t)y * w + x];
        }
    return p;
}

/* an image scaled to fit inside w x h, centred; returns the piece and where it sits */
const Piece *fitted(int id, const Img &I, int w, int h, int *ox, int *oy)
{
    if (!I.ok() || w < 1 || h < 1) return 0;
    const double k = (double)w / I.w < (double)h / I.h ? (double)w / I.w : (double)h / I.h;
    int dw = (int)(I.w * k + 0.5), dh = (int)(I.h * k + 0.5);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    *ox = (w - dw) / 2;
    *oy = (h - dh) / 2;
    Piece *p = find_piece(2, id, dw, dh);
    if (p) return p;
    p = new_piece(2, id, dw, dh);
    scale_rect(I, 0, 0, I.w, I.h, dw, dh, &p->px, I.w <= 32 && dw >= 2 * I.w);
    return p;
}

/* ================================================================ the head icons ============================================ */

Img g_rom_icon[4];


inline uint32_t rd32(uint32_t a) { return *reinterpret_cast<volatile uint32_t *>(a); }
inline uint16_t rd16(uint32_t a) { return *reinterpret_cast<volatile uint16_t *>(a); }
inline uint8_t rd8(uint32_t a) { return *reinterpret_cast<volatile uint8_t *>(a); }

/* the HUD's life icon of character c: the OamAttr list's sprites drawn from engine A's OBJ memory (tiles at 0x06400000, palette at 0x05000200),
   the way ntr/ppu.cpp's sprite scan reads them. 16 colour and 256 colour sprites, 1D and 2D tile maps, no rotation. */
void read_rom_icon(int c, Img *out)
{
    *out = Img();
    const unsigned short *a = (const unsigned short *)_ZN3OAM10LIFE_ICONSE[c];
    if (!a) return;
    static const int kSizes[3][4][2] = { { {8, 8}, {16, 16}, {32, 32}, {64, 64} }, { {16, 8}, {32, 8}, {32, 16}, {64, 32} }, { {8, 16}, {8, 32}, {16, 32}, {32, 64} } };
    const uint32_t dispcnt = rd32(0x04000000u);
    const uint32_t boundary = 32u << ((dispcnt >> 20) & 3);
    const bool map1d = (dispcnt >> 4) & 1;
    struct E { int x, y, w, h; unsigned a0, a1, a2; } e[8];
    int n = 0;
    for (int i = 0; i < 8; ++i) {
        const unsigned a0 = a[i * 4], a1 = a[i * 4 + 1], a2 = a[i * 4 + 2], a3 = a[i * 4 + 3];
        const bool last = a3 == 0xffff;     /* attr3 0xffff marks the LAST entry of the list (that entry is drawn too) */
        const int shape = (a0 >> 14) & 3, size = (a1 >> 14) & 3;
        if (shape == 3) { if (last) break; continue; }
        int x = a1 & 0x1FF, y = a0 & 0xFF;
        if (x >= 256) x -= 512;
        if (y >= 128) y -= 256;
        e[n].x = x; e[n].y = y; e[n].w = kSizes[shape][size][0]; e[n].h = kSizes[shape][size][1];
        e[n].a0 = a0; e[n].a1 = a1; e[n].a2 = a2;
        ++n;
        if (last) break;
    }
    if (n == 0) return;
    int x0 = 9999, y0 = 9999, x1 = -9999, y1 = -9999;
    for (int i = 0; i < n; ++i) {
        if (e[i].x < x0) x0 = e[i].x;
        if (e[i].y < y0) y0 = e[i].y;
        if (e[i].x + e[i].w > x1) x1 = e[i].x + e[i].w;
        if (e[i].y + e[i].h > y1) y1 = e[i].y + e[i].h;
    }
    const int W = x1 - x0, H = y1 - y0;
    if (W < 1 || H < 1 || W > 64 || H > 64) return;
    out->w = W;
    out->h = H;
    out->px.assign((size_t)W * H, 0);
    for (int k = n - 1; k >= 0; --k) {
        const E &s = e[k];
        const bool c256 = (s.a0 & 0x2000) != 0, hflip = (s.a1 & 0x1000) != 0, vflip = (s.a1 & 0x2000) != 0;
        const uint32_t tile = s.a2 & 0x3FF, pal = (s.a2 >> 12) & 0xF;
        for (int sy = 0; sy < s.h; ++sy)
            for (int sx = 0; sx < s.w; ++sx) {
                const int tx = hflip ? s.w - 1 - sx : sx, ty = vflip ? s.h - 1 - sy : sy;
                const int tcol = tx >> 3, trow = ty >> 3, fx = tx & 7, fy = ty & 7;
                const uint32_t slot = map1d ? (uint32_t)(trow * (s.w / 8) + tcol) * (c256 ? 2u : 1u) : (uint32_t)(trow * 32 + (c256 ? tcol * 2 : tcol));
                const uint32_t cell = 0x06400000u + tile * boundary + slot * 32u;
                uint32_t col;
                if (c256) {
                    const uint32_t idx = rd8(cell + fy * 8u + fx);
                    if (!idx) continue;
                    col = rd16(0x05000200u + idx * 2u);
                } else {
                    const uint8_t b = rd8(cell + fy * 4u + fx / 2);
                    const uint32_t idx = (fx & 1) ? (b >> 4) : (b & 0xF);
                    if (!idx) continue;
                    col = rd16(0x05000200u + (pal * 16u + idx) * 2u);
                }
                const uint32_t r = (col >> 0) & 0x1F, g = (col >> 5) & 0x1F, b = (col >> 10) & 0x1F;
                const uint32_t px = 0xFF000000u | ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
                const int dx = s.x - x0 + sx, dy = s.y - y0 + sy;
                if (dx >= 0 && dy >= 0 && dx < W && dy < H) out->px[(size_t)dy * W + dx] = px;
            }
    }
    int lit = 0;
    for (size_t i = 0; i < out->px.size(); ++i) if (out->px[i]) ++lit;
    fprintf(stderr, "[charmenu] head icon %d: %dx%d, %d sprite(s), %d lit pixels\n", c, W, H, n, lit);
    if (lit == 0) *out = Img();
}

/* the pictures of the mod characters of this open, read once per open */
struct CustomArt { Img icon, preview; int tried = 0; };
CustomArt g_cust[16];

CustomArt &cust(int id)
{
    CustomArt &c = g_cust[(id - 4) & 15];
    return c;
}

/* what to draw for an entry: the small icon and the big picture. Order: the mod's icon file or preview, the base's HUD head. */
const Img *icon_for(const CdItem &it)
{
    if (it.custom) {
        CustomArt &c = cust(it.id);
        if (!c.tried) { c.tried = 1; load_file(it.icon, &c.icon); load_file(it.preview, &c.preview); }
        if (c.icon.ok()) return &c.icon;
        if (c.preview.ok()) return &c.preview;
    }
    return g_rom_icon[it.base & 3].ok() ? &g_rom_icon[it.base & 3] : 0;
}
const Img *preview_for(const CdItem &it)
{
    if (it.custom) {
        CustomArt &c = cust(it.id);
        if (!c.tried) { c.tried = 1; load_file(it.icon, &c.icon); load_file(it.preview, &c.preview); }
        if (c.preview.ok()) return &c.preview;
        if (c.icon.ok()) return &c.icon;
    }
    return g_rom_icon[it.base & 3].ok() ? &g_rom_icon[it.base & 3] : 0;
}
int image_id(const CdItem &it, const Img *I)
{
    if (!I) return 0;
    if (it.custom) {
        CustomArt &c = cust(it.id);
        if (I == &c.icon) return 100 + (it.id & 15) * 2;
        if (I == &c.preview) return 101 + (it.id & 15) * 2;
    }
    return 10 + (it.base & 3);
}

/* ================================================================ text ===================================================== */

struct Font {
    bool hd;
    float u;
    /* size: an em in layout units (hd). The plain font is the 5x7 pixel font at a FRACTIONAL scale (about 1.2 for a row), so a name of 20
       letters fits the list as the HD lettering does; never below 1. */
    float sc(float em) const { const float s = em * u / 15.5f; return s < 1.0f ? 1.0f : s; }
    float width(const char *s, float em) const
    {
        if (hd) return ui_text_width(s, em * u);
        return (float)strlen(s) * OVL_ADVANCE * sc(em);
    }
    /* draws with the top of a capital at y (hd: baseline = y + cap height) */
    void draw(const Surf &s, float x, float y, const char *str, float em, uint32_t rgb, int alpha = 255) const
    {
        if (hd) {
            UiSurface us = { s.px, s.stride, s.w, s.h };
            ui_text_draw(&us, x, y + ui_text_cap_height(em * u), str, em * u, rgb, alpha, 0);
            return;
        }
        const float k = sc(em);
        float pen = x;
        for (const char *c = str; *c; ++c) {
            const unsigned char ch = (unsigned char)*c;
            if (ch >= 0x20 && ch <= 0x7e) {
                const unsigned char *g = OVL_FONT[ch - 0x20];
                const int x0 = (int)floorf(pen), y0 = (int)floorf(y);
                const int gw = (int)ceilf(OVL_GLYPH_W * k) + 1, gh = (int)ceilf(OVL_GLYPH_H * k) + 1;
                for (int dy = 0; dy < gh; ++dy)
                    for (int dx = 0; dx < gw; ++dx) {
                        int hit = 0;
                        for (int sub = 0; sub < 4; ++sub) {
                            const float fx = (x0 + dx + 0.25f + (sub & 1) * 0.5f - pen) / k, fy = (y0 + dy + 0.25f + (sub >> 1) * 0.5f - y) / k;
                            const int sx = (int)floorf(fx), sy = (int)floorf(fy);
                            if (fx < 0 || fy < 0 || sx >= OVL_GLYPH_W || sy >= OVL_GLYPH_H) continue;
                            if (g[sy] & (0x80 >> sx)) ++hit;
                        }
                        if (!hit) continue;
                        const int px = x0 + dx, py = y0 + dy;
                        if (px < 0 || py < 0 || px >= s.w || py >= s.h) continue;
                        const int a = hit * alpha / 4;
                        uint32_t *d = &s.px[(size_t)py * s.stride + px];
                        *d = over(*d, (int)(((rgb >> 16) & 255) * a / 255), (int)(((rgb >> 8) & 255) * a / 255), (int)((rgb & 255) * a / 255), a);
                    }
            }
            pen += OVL_ADVANCE * k;
        }
    }
    float cap(float em) const { return hd ? ui_text_cap_height(em * u) : 7.0f * sc(em); }
};

/* a string cut to fit w pixels (an ellipsis when it had to be shortened) */
std::string fit(const Font &f, const char *s, float em, float w)
{
    std::string r = s;
    if (f.width(r.c_str(), em) <= w) return r;
    while (r.size() > 1) {
        r.erase(r.size() - 1);
        const std::string t = r + "..";
        if (f.width(t.c_str(), em) <= w) return t;
    }
    return r;
}

/* word wrap into at most maxl lines of w pixels (the last one carries an ellipsis when text is left) */
void wrap(const Font &f, const char *s, float em, float w, int maxl, std::vector<std::string> *out)
{
    out->clear();
    std::string cur;
    std::string word;
    std::string text = s;
    text += ' ';
    size_t i = 0;
    bool cut = false;
    while (i < text.size() && !cut) {
        word.clear();
        while (i < text.size() && text[i] != ' ') word += text[i++];
        ++i;
        if (word.empty()) continue;
        std::string t = cur.empty() ? word : cur + " " + word;
        if (f.width(t.c_str(), em) <= w) { cur = t; continue; }
        if (!cur.empty()) {
            out->push_back(cur);
            if ((int)out->size() >= maxl) { cut = true; break; }
        }
        cur = fit(f, word.c_str(), em, w);
    }
    if (!cut && !cur.empty()) out->push_back(cur);
    if (cut || (int)out->size() > maxl) {
        out->resize((size_t)maxl);
        (*out)[(size_t)maxl - 1] = fit(f, ((*out)[(size_t)maxl - 1] + "...").c_str(), em, w);
    }
}

/* a small five pointed star, anti-aliased, for the "current" mark */
void star(const Surf &s, float cx, float cy, float R, uint32_t rgb, uint32_t edge)
{
    const float r = R * 0.45f;
    float vx[10], vy[10];
    for (int i = 0; i < 10; ++i) {
        const float a = -1.5707963f + i * 0.6283185f;
        const float rr = (i & 1) ? r : R;
        vx[i] = cx + cosf(a) * rr;
        vy[i] = cy + sinf(a) * rr;
    }
    const int x0 = (int)floorf(cx - R - 2), x1 = (int)ceilf(cx + R + 2), y0 = (int)floorf(cy - R - 2), y1 = (int)ceilf(cy + R + 2);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            int in = 0, hit = 0;
            float md = 1e9f;
            for (int sub = 0; sub < 4; ++sub) {
                const float px = x + 0.25f + (sub & 1) * 0.5f, py = y + 0.25f + (sub >> 1) * 0.5f;
                int c = 0;
                float d2 = 1e9f;
                for (int i = 0, j = 9; i < 10; j = i++) {
                    if (((vy[i] > py) != (vy[j] > py)) && (px < (vx[j] - vx[i]) * (py - vy[i]) / (vy[j] - vy[i]) + vx[i])) c = !c;
                    const float ex = vx[j] - vx[i], ey = vy[j] - vy[i];
                    float t = ((px - vx[i]) * ex + (py - vy[i]) * ey) / (ex * ex + ey * ey);
                    t = t < 0 ? 0 : (t > 1 ? 1 : t);
                    const float qx = vx[i] + ex * t - px, qy = vy[i] + ey * t - py;
                    const float d = qx * qx + qy * qy;
                    if (d < d2) d2 = d;
                }
                if (c) { ++in; if (d2 < md) md = d2; }
                if (c && d2 < 0.8f) ++hit;
            }
            if (!in) continue;
            const uint32_t col = hit ? edge : rgb;
            const int a = in * 255 / 4;
            const int r8 = (col >> 16) & 255, g8 = (col >> 8) & 255, b8 = col & 255;
            if (x >= 0 && y >= 0 && x < s.w && y < s.h)
                s.px[(size_t)y * s.stride + x] = over(s.px[(size_t)y * s.stride + x], r8 * a / 255, g8 * a / 255, b8 * a / 255, a);
        }
}

}  // namespace

/* ================================================================ the layout =============================================== */

void cdskin_geom(int w, int h, int n, int mode, CdGeom *g)
{
    memset(g, 0, sizeof *g);
    /* one unit is a 384th of the height; a picture narrower than the panel wants shrinks the unit instead of clipping the panel */
    const float SHEET_W = 448.0f;
    float u = h / 384.0f;
    if (SHEET_W * u > w - 8) u = (w - 8) / SHEET_W;
    if (u < 0.5f) u = 0.5f;
    g->u = u;
    auto U = [&](float v) { return (int)(v * u + 0.5f); };
    if (mode == CDS_BOX) {
        g->sw = U(380); g->sh = U(132);
        g->sx = (w - g->sw) / 2; g->sy = (h - g->sh) / 2;
        g->ow = U(84); g->oh = U(34);
        g->ox = g->sx + (g->sw - g->ow) / 2; g->oy = g->sy + g->sh - g->oh - U(12);
        g->dx = g->sx + U(16); g->dy = g->sy + U(20); g->dw = g->sw - U(32);
        return;
    }
    /* the sheet sits high: the minimap's inset takes the bottom right corner of a 4:3 picture and the sheet must not run under it */
    g->sw = U(SHEET_W); g->sh = U(236);
    g->sx = (w - g->sw) / 2; g->sy = U(44);
    g->tw = U(200); g->th = U(52);
    g->tx = g->sx + (g->sw - g->tw) / 2; g->ty = g->sy - U(24);
    g->bw = U(72); g->bh = U(41);
    g->bx = g->sx - U(6); g->by = g->sy - U(26);
    g->rowh = U(36);
    g->rows = n < 5 ? (n < 1 ? 1 : n) : 5;
    g->lx = g->sx + U(20); g->ly = g->sy + U(38); g->lw = U(210);
    g->pw = U(84); g->ph = U(84);
    g->px = g->sx + U(246) + (U(178) - g->pw) / 2; g->py = g->sy + U(34);
    g->dx = g->sx + U(240); g->dy = g->py + g->ph + U(16); g->dw = U(190);
}

int cdskin_hd(void)
{
    decide_skin();
    return g_hd;
}

void cdskin_begin(void)
{
    decide_skin();
    for (int i = 0; i < 4; ++i) read_rom_icon(i, &g_rom_icon[i]);

    for (int i = 0; i < 16; ++i) g_cust[i] = CustomArt();
    g_pieces.clear();
}

/* ================================================================ the draw ================================================= */

static void draw_sheet(const Surf &s, const CdGeom &g, bool hd)
{
    const float r = 14.0f * g.u;
    const int edge = g.u * 3 < 2 ? 2 : (int)(g.u * 3);
    /* a soft shadow: two darker rounded rectangles under the sheet */
    rrect(s, g.sx + (int)(3 * g.u), g.sy + (int)(5 * g.u), g.sw, g.sh, r, 0x000000u, 60);
    rrect(s, g.sx + (int)(1 * g.u), g.sy + (int)(2 * g.u), g.sw, g.sh, r, 0x000000u, 50);
    if (hd) {
        Piece *p = find_piece(3, 0, g.sw - 2 * edge, g.sh - 2 * edge);
        if (!p) {
            p = new_piece(3, 0, g.sw - 2 * edge, g.sh - 2 * edge);
            /* the parchment covers the sheet, cropped to its shape (never stretched out of proportion) */
            const Img &I = g_art[A_PARCH];
            const double ar = (double)(g.sw - 2 * edge) / (g.sh - 2 * edge);
            double cw = I.w, ch = I.w / ar;
            if (ch > I.h) { ch = I.h; cw = I.h * ar; }
            scale_rect(I, (I.w - cw) / 2, (I.h - ch) / 2, cw, ch, g.sw - 2 * edge, g.sh - 2 * edge, &p->px);
        }
        rrect(s, g.sx, g.sy, g.sw, g.sh, r, C_EDGE, 255);
        rrect(s, g.sx + edge, g.sy + edge, g.sw - 2 * edge, g.sh - 2 * edge, r - edge, 0, 255, p);
    } else {
        framed(s, g.sx, g.sy, g.sw, g.sh, r, edge, C_SHEET, C_EDGE);
    }
}

/* a wooden or cream plank behind a label: kind 0 wood row, 1 cream plaque */
static void draw_plank(const Surf &s, const Font &f, bool hd, int x, int y, int w, int h, int kind, float scale)
{
    if (hd) {
        const Piece *p = kind == 1 ? nine(A_KEY, w, h, 70, 55, 70, 55, (float)h / 171.0f * 1.0f) : nine(A_PLANK, w, h, 18, 18, 18, 18, (float)h / 150.0f);
        if (p) blit(s, x, y, *p);
        return;
    }
    (void)f; (void)scale;
    const int edge = 2 > (int)(f.u * 2) ? 2 : (int)(f.u * 2);
    if (kind == 1) framed(s, x, y, w, h, h * 0.32f, edge, C_CREAM, C_EDGE);
    else framed(s, x, y, w, h, h * 0.32f, edge, 0xE4B658u, C_EDGE);
}

void cdskin_draw(uint32_t *px, int stride, int w, int h, const CdGeom *gp, const CdItem *it, int n, int sel, int top, int mode,
                 const char *box_msg, int back_hot, int ok_hot)
{
    decide_skin();
    const CdGeom &g = *gp;
    const Surf s = { px, stride, w, h };
    const bool hd = g_hd != 0;
    Font f;
    f.hd = hd;
    f.u = g.u;
    auto U = [&](float v) { return (int)(v * g.u + 0.5f); };
    const uint32_t ink = C_TEXT;

    draw_sheet(s, g, hd);

    if (mode == CDS_BOX) {
        const float em = 19.0f;
        std::vector<std::string> ln;
        wrap(f, box_msg, em, (float)g.dw, 2, &ln);
        for (size_t i = 0; i < ln.size(); ++i) {
            const float tw = f.width(ln[i].c_str(), em);
            f.draw(s, g.dx + (g.dw - tw) / 2, (float)(g.dy + (int)i * U(26)), ln[i].c_str(), em, ink);
        }
        draw_plank(s, f, hd, g.ox, g.oy, g.ow, g.oh, ok_hot ? 1 : 0, 1.0f);
        const float tw = f.width("OK", 18.0f);
        f.draw(s, g.ox + (g.ow - tw) / 2, g.oy + (g.oh - f.cap(18.0f)) / 2, "OK", 18.0f, ok_hot && !hd ? C_TEXT : (ok_hot ? C_TEXT : (hd ? C_WOODTXT : C_TEXT)));
        return;
    }

    /* the title plank straddles the top edge of the sheet */
    {
        draw_plank(s, f, hd, g.tx, g.ty, g.tw, g.th, 1, 1.0f);
        const float em = 24.0f;
        const char *t = "Characters";
        const float tw = f.width(t, em);
        f.draw(s, g.tx + (g.tw - tw) / 2, g.ty + (g.th - f.cap(em)) / 2, t, em, C_TEXT);
    }
    /* Back: the plank with its word on it in the HD skin, a plain plank otherwise */
    if (hd) {
        const Img &I = g_art[A_BACK];
        Piece *p = find_piece(4, 0, g.bw, g.bh);
        if (!p) { p = new_piece(4, 0, g.bw, g.bh); scale_rect(I, 0, 0, I.w, I.h, g.bw, g.bh, &p->px); }
        blit(s, g.bx, g.by, *p);
        if (back_hot) {   /* a lit edge while the pointer is on it */
            const int e = g.u < 1 ? 1 : (int)g.u;
            rrect(s, g.bx + e, g.by + e, g.bw - 2 * e, g.bh - 2 * e, g.bh * 0.3f, 0xFFFFFFu, 40);
        }
    } else {
        draw_plank(s, f, hd, g.bx, g.by, g.bw, g.bh, back_hot ? 1 : 0, 1.0f);
        const float tw = f.width("Back", 17.0f);
        f.draw(s, g.bx + (g.bw - tw) / 2, g.by + (g.bh - f.cap(17.0f)) / 2, "Back", 17.0f, C_TEXT);
    }

    /* the list */
    for (int i = 0; i < g.rows; ++i) {
        const int ix = top + i;
        if (ix >= n) break;
        const CdItem &m = it[ix];
        const int y = g.ly + i * g.rowh;
        const int rh = g.rowh - U(4);
        const bool on = ix == sel;
        /* the selected row is the cream plaque, the others wood (plain: a plain plank or just the text) */
        if (hd) draw_plank(s, f, hd, g.lx, y, g.lw, rh, on ? 1 : 0, 1.0f);
        else if (on) draw_plank(s, f, hd, g.lx, y, g.lw, rh, 1, 1.0f);
        const uint32_t tc = hd ? (on ? C_TEXT : C_WOODTXT) : C_TEXT;
        /* the head icon, 24 units, its own pixels */
        const int isz = U(26);
        int tx = g.lx + U(12);
        if (const Img *I = icon_for(m)) {
            int ox = 0, oy = 0;
            if (const Piece *p = fitted(image_id(m, I), *I, isz, isz, &ox, &oy)) blit(s, g.lx + U(10) + ox, y + (rh - isz) / 2 + oy, *p);
            tx = g.lx + U(10) + isz + U(8);
        }
        const float em = 19.0f;
        const float room = (float)(g.lx + g.lw - U(10) - (m.current ? U(22) : 0) - tx);
        const std::string name = fit(f, m.label, em, room);
        if (hd && !on) f.draw(s, (float)tx + 1, y + (rh - f.cap(em)) / 2 + 1, name.c_str(), em, 0x7A4614u, 110);   /* a soft shadow on the wood */
        f.draw(s, (float)tx, y + (rh - f.cap(em)) / 2, name.c_str(), em, tc);
        if (m.current) star(s, (float)(g.lx + g.lw - U(16)), (float)(y + rh / 2), 7.5f * g.u, 0xFFD23Cu, 0x8A4B08u);
    }
    /* more above / below: small triangles on the sheet's edge */
    {
        const uint32_t tc = C_DIM;
        const int cx = g.lx + g.lw / 2;
        auto tri = [&](int x, int y, int dir) {
            const int hh = U(5);
            for (int j = 0; j < hh; ++j) {
                const int half = dir > 0 ? j : hh - 1 - j;
                for (int i = -half; i <= half; ++i) {
                    const int yy = y + j, xx = x + i;
                    if (xx >= 0 && yy >= 0 && xx < s.w && yy < s.h) s.px[(size_t)yy * s.stride + xx] = 0xFF000000u | tc;
                }
            }
        };
        if (top > 0) tri(cx, g.ly - U(10), -1);
        if (top + g.rows < n) tri(cx, g.ly + g.rows * g.rowh + U(1), 1);
    }

    /* the detail column: a frame with the picture, then the name, the base and the credits of the selected entry */
    if (sel >= 0 && sel < n) {
        const CdItem &m = it[sel];
        draw_plank(s, f, hd, g.px - U(8), g.py - U(8), g.pw + U(16), g.ph + U(16), 1, 1.0f);
        const int pad = U(18);
        if (const Img *I = preview_for(m)) {
            int ox = 0, oy = 0;
            const int iw = g.pw - (hd ? pad : U(6)), ih = g.ph - (hd ? pad : U(6));
            if (const Piece *p = fitted(image_id(m, I) + 200, *I, iw, ih, &ox, &oy)) blit(s, g.px + (g.pw - iw) / 2 + ox, g.py + (g.ph - ih) / 2 + oy, *p);
        }
        int y = g.dy;
        const float em = hd ? 21.0f : 19.0f;
        const std::string nm = fit(f, m.label, em, (float)g.dw);
        f.draw(s, g.dx + (g.dw - f.width(nm.c_str(), em)) / 2, (float)y, nm.c_str(), em, C_TEXT);
        y += U(26);
        if (m.plays[0]) {
            const std::string pl = fit(f, m.plays, 16.0f, (float)g.dw);
            f.draw(s, g.dx + (g.dw - f.width(pl.c_str(), 16.0f)) / 2, (float)y, pl.c_str(), 16.0f, C_DIM);
            y += U(22);
        }
        if (m.credits[0]) {
            std::vector<std::string> ln;
            wrap(f, m.credits, 14.0f, (float)g.dw, 2, &ln);
            for (size_t i = 0; i < ln.size(); ++i) {
                f.draw(s, g.dx + (g.dw - f.width(ln[i].c_str(), 14.0f)) / 2, (float)y, ln[i].c_str(), 14.0f, C_DIM);
                y += U(18);
            }
        }
    }
}

/* ================================================================ the star select strip =================================================== */

void cdskin_strip_geom(int w, int h, int n, CsGeom *g)
{
    memset(g, 0, sizeof *g);
    g->k = h / 192.0f;
    const float k = g->k;
    g->n = n;
    g->sw = (int)(24 * k + 0.5f);
    g->sh = (int)(21 * k + 0.5f);
    g->pitch = (int)(27 * k + 0.5f);
    const int total = n > 0 ? (n - 1) * g->pitch + g->sw : 0;
    g->x0 = (w - total) / 2;
    g->y0 = h - g->sh - (int)(1 * k + 0.5f);
}

void cdskin_strip_draw(uint32_t *px, int stride, int w, int h, const CsGeom *gp, const CdItem *it, int n, int focus, int hover)
{
    decide_skin();
    const CsGeom &g = *gp;
    const Surf s = { px, stride, w, h };
    const bool hd = g_hd != 0;
    Font f;
    f.hd = hd;
    f.u = g.k * 0.5f;       /* the skin's unit is a 384th of the height */
    const float k = g.k;
    auto K = [&](float v) { return (int)(v * k + 0.5f); };
    const int lit = focus >= 0 ? focus : hover;
    for (int i = 0; i < n && i < g.n; ++i) {
        const CdItem &m = it[i];
        const bool on = i == lit;
        const int x = g.x0 + i * g.pitch;
        const int y = g.y0 - (on ? K(2) : 0);
        const bool cream = m.current && m.id >= 0;
        /* a soft shadow under the slot */
        rrect(s, x + K(1), y + K(2), g.sw, g.sh, g.sh * 0.32f, 0x000000u, 70);
        draw_plank(s, f, hd, x, y, g.sw, g.sh, (cream || (on && m.id < 0)) ? 1 : 0, 1.0f);
        if (on) {   /* the lit edge of the slot the cursor or the pointer is on */
            const int e = k < 1 ? 1 : (int)k;
            rrect(s, x + e, y + e, g.sw - 2 * e, g.sh - 2 * e, g.sh * 0.3f, 0xFFFFFFu, 55);
        }
        if (m.id < 0) {
            /* the arrow: a solid triangle pointing right, centred on its own plank */
            const float cx = x + g.sw * 0.5f + 0.5f * k, cy = y + g.sh * 0.5f, hh = 5.5f * k, ww = 8.0f * k;
            const uint32_t col = (hd && !on) ? 0xFFF3CCu : C_TEXT;
            const int x0 = (int)floorf(cx - ww * 0.5f), x1 = (int)ceilf(cx + ww * 0.5f);
            for (int xx = x0; xx <= x1; ++xx) {
                const float t = ((xx + 0.5f) - (cx - ww * 0.5f)) / ww;     /* 0 at the base, 1 at the tip */
                if (t < 0 || t > 1) continue;
                const float half = hh * (1.0f - t);
                const int y0 = (int)floorf(cy - half), y1 = (int)ceilf(cy + half);
                for (int yy = y0; yy <= y1; ++yy) {
                    const float cov = (yy + 0.5f >= cy - half && yy + 0.5f <= cy + half) ? 1.0f : 0.5f;
                    if (xx < 0 || yy < 0 || xx >= s.w || yy >= s.h) continue;
                    const int a = (int)(cov * 255);
                    s.px[(size_t)yy * s.stride + xx] = over(s.px[(size_t)yy * s.stride + xx], ((col >> 16) & 255) * a / 255, ((col >> 8) & 255) * a / 255, (col & 255) * a / 255, a);
                }
            }
            continue;
        }
        const int isz = g.sh - K(5);
        if (const Img *I = icon_for(m)) {
            int ox = 0, oy = 0;
            if (const Piece *p = fitted(image_id(m, I) + 400, *I, isz, isz, &ox, &oy)) blit(s, x + (g.sw - isz) / 2 + ox, y + (g.sh - isz) / 2 + oy, *p);
        }
        if (m.current) star(s, (float)(x + g.sw - K(3)), (float)(y + K(3)), 4.2f * k, 0xFFD23Cu, 0x8A4B08u);
    }
    /* the name of the slot the cursor or the pointer is on, on a cream plaque above it (the arrow has none) */
    if (lit >= 0 && lit < n && it[lit].id >= 0) {
        const CdItem &m = it[lit];
        const float em = 15.0f;
        const float tw = f.width(m.label, em);
        const int pw = (int)tw + K(12), ph = K(15);
        int px0 = g.x0 + lit * g.pitch + g.sw / 2 - pw / 2;
        if (px0 < K(2)) px0 = K(2);
        if (px0 + pw > w - K(2)) px0 = w - K(2) - pw;
        const int py0 = g.y0 - K(2) - ph - K(3);
        draw_plank(s, f, hd, px0, py0, pw, ph, 1, 1.0f);
        f.draw(s, (float)(px0 + (pw - tw) / 2), py0 + (ph - f.cap(em)) / 2, m.label, em, C_TEXT);
    }
}
