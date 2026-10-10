/* settings_page.cpp -- see settings_page.h (run menu1, lane SETMENU1). Pure host code: no ROM, no window, no device. */
#include "settings_page.h"
#include "settings_live.h"
#include "host_settings.h"
#include "ui_text.h"
#include "menu_hostimg.h"
#include "../third_party/stb/stb_image.h"

#if defined(_MSC_VER) || defined(__SSE2__)
#include <emmintrin.h>
#endif
#include <chrono>
#include <deque>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

extern "C" const unsigned char g_setart_png_parchment[], g_setart_png_title[], g_setart_png_back[], g_setart_png_key[], g_setart_png_plank[],
    g_setart_png_dd_top[], g_setart_png_dd_mid[], g_setart_png_dd_bot[], g_setart_png_dd_arrow[];
extern "C" const unsigned int g_setart_png_parchment_len, g_setart_png_title_len, g_setart_png_back_len, g_setart_png_key_len, g_setart_png_plank_len,
    g_setart_png_dd_top_len, g_setart_png_dd_mid_len, g_setart_png_dd_bot_len, g_setart_png_dd_arrow_len;

namespace {

/* ================================================================== art ================================================ */

enum { IM_PARCH, IM_TITLE, IM_BACK, IM_KEY, IM_PLANK, IM_DDTOP, IM_DDMID, IM_DDBOT, IM_DDARROW, IM_COUNT };

struct Img { int tried = 0, w = 0, h = 0; std::vector<uint32_t> px; };     /* premultiplied 0xAARRGGBB */
Img g_im[IM_COUNT];

void load_im(int id)
{
    Img &I = g_im[id];
    if (I.tried) return;
    I.tried = 1;
    const unsigned char *const emb[IM_COUNT] = { g_setart_png_parchment, g_setart_png_title, g_setart_png_back, g_setart_png_key, g_setart_png_plank,
                                                 g_setart_png_dd_top, g_setart_png_dd_mid, g_setart_png_dd_bot, g_setart_png_dd_arrow };
    const unsigned len[IM_COUNT] = { g_setart_png_parchment_len, g_setart_png_title_len, g_setart_png_back_len, g_setart_png_key_len,
                                     g_setart_png_plank_len, g_setart_png_dd_top_len, g_setart_png_dd_mid_len, g_setart_png_dd_bot_len, g_setart_png_dd_arrow_len };
    int w = 0, h = 0, comp = 0;
    unsigned char *p = stbi_load_from_memory(emb[id], (int)len[id], &w, &h, &comp, 4);
    if (!p) return;
    I.w = w; I.h = h; I.px.resize((size_t)w * h);
    for (int i = 0; i < w * h; ++i) {
        const unsigned r = p[i * 4], g = p[i * 4 + 1], b = p[i * 4 + 2], a = p[i * 4 + 3];
        I.px[i] = (a << 24) | (((r * a + 127) / 255) << 16) | (((g * a + 127) / 255) << 8) | ((b * a + 127) / 255);
    }
    stbi_image_free(p);
}

/* area / bilinear resample of a sub-rectangle of a premultiplied image (the same triangle filter as hal/menu_hostimg.cpp, whose
   support grows with a reduction, so a shrink is an area average) */
struct Tap { int first, n; std::vector<float> w; };
void taps(double s0, double sn, int dn, int lim, std::vector<Tap> *out)
{
    out->resize((size_t)dn);
    const double sc = sn / dn, sup = sc > 1.0 ? sc : 1.0;
    for (int d = 0; d < dn; ++d) {
        const double c = s0 + (d + 0.5) * sc;
        int a = (int)floor(c - sup), b = (int)ceil(c + sup);
        if (a < 0) a = 0;
        if (b > lim) b = lim;
        Tap &t = (*out)[d];
        t.first = a; t.n = b - a; t.w.assign((size_t)(t.n > 0 ? t.n : 0), 0.f);
        double sum = 0;
        for (int s = a; s < b; ++s) {
            const double x = fabs((s + 0.5) - c) / sup, wv = x < 1.0 ? 1.0 - x : 0.0;
            t.w[s - a] = (float)wv; sum += wv;
        }
        if (sum <= 0 || t.n <= 0) { t.first = a < lim ? a : lim - 1; t.n = 1; t.w.assign(1, 1.f); sum = 1; }
        for (int s = 0; s < t.n; ++s) t.w[s] = (float)(t.w[s] / sum);
    }
}

void scale_rect(const Img &I, int sx, int sy, int sw, int sh, int dw, int dh, std::vector<uint32_t> *out)
{
    out->assign((size_t)(dw > 0 ? dw : 0) * (dh > 0 ? dh : 0), 0);
    if (I.px.empty() || dw < 1 || dh < 1 || sw < 1 || sh < 1) return;
    std::vector<Tap> tx, ty;
    taps(sx, sw, dw, sx + sw, &tx);
    taps(sy, sh, dh, sy + sh, &ty);
    int ylo = ty[0].first, yhi = ty[0].first + ty[0].n;
    for (const Tap &t : ty) { if (t.first < ylo) ylo = t.first; if (t.first + t.n > yhi) yhi = t.first + t.n; }
    std::vector<float> mid((size_t)dw * (yhi - ylo) * 4);
    for (int y = ylo; y < yhi; ++y) {
        const uint32_t *row = &I.px[(size_t)y * I.w];
        for (int x = 0; x < dw; ++x) {
            float a = 0, r = 0, g = 0, b = 0;
            const Tap &t = tx[x];
            for (int k = 0; k < t.n; ++k) {
                const uint32_t c = row[t.first + k];
                const float wv = t.w[k];
                a += wv * (c >> 24); r += wv * ((c >> 16) & 255); g += wv * ((c >> 8) & 255); b += wv * (c & 255);
            }
            float *m = &mid[((size_t)(y - ylo) * dw + x) * 4];
            m[0] = a; m[1] = r; m[2] = g; m[3] = b;
        }
    }
    for (int y = 0; y < dh; ++y) {
        const Tap &t = ty[y];
        for (int x = 0; x < dw; ++x) {
            float a = 0, r = 0, g = 0, b = 0;
            for (int k = 0; k < t.n; ++k) {
                const float *m = &mid[((size_t)(t.first + k - ylo) * dw + x) * 4];
                const float wv = t.w[k];
                a += wv * m[0]; r += wv * m[1]; g += wv * m[2]; b += wv * m[3];
            }
            int ia = (int)(a + 0.5f), ir = (int)(r + 0.5f), ig = (int)(g + 0.5f), ib = (int)(b + 0.5f);
            if (ia > 255) ia = 255;
            if (ir > ia) ir = ia;
            if (ig > ia) ig = ia;
            if (ib > ia) ib = ia;
            (*out)[(size_t)y * dw + x] = ((uint32_t)ia << 24) | ((uint32_t)ir << 16) | ((uint32_t)ig << 8) | (uint32_t)ib;
        }
    }
}

/* a cache of finished pieces (a scaled picture, a composed box, the sheet): keyed by kind and size */
struct Piece { int kind, a, b, c; int w, h; std::vector<uint32_t> px; };
std::deque<Piece> g_pieces;           /* a deque: a push never moves a piece a caller still holds */

Piece *find_piece(int kind, int a, int b, int c)
{
    for (Piece &p : g_pieces)
        if (p.kind == kind && p.a == a && p.b == b && p.c == c) return &p;
    return 0;
}
Piece *new_piece(int kind, int a, int b, int c, int w, int h)
{
    if (g_pieces.size() >= 48) g_pieces.erase(g_pieces.begin(), g_pieces.begin() + 24);
    g_pieces.push_back(Piece());
    Piece &p = g_pieces.back();
    p.kind = kind; p.a = a; p.b = b; p.c = c; p.w = w; p.h = h;
    p.px.assign((size_t)w * h, 0);
    return &p;
}

/* ============================================================ drawing primitives ======================================= */

typedef UiSurface Surf;

inline uint32_t over(uint32_t d, int sr, int sg, int sb, int sa /* premultiplied colour, alpha */)
{
    const int ia = 255 - sa;
    const int r = sr + (((d >> 16) & 255) * ia + 127) / 255, g = sg + (((d >> 8) & 255) * ia + 127) / 255, b = sb + ((d & 255) * ia + 127) / 255;
    return 0xFF000000u | (uint32_t)((r > 255 ? 255 : r) << 16) | (uint32_t)((g > 255 ? 255 : g) << 8) | (uint32_t)(b > 255 ? 255 : b);
}

struct Clip { int x0, y0, x1, y1; };
Clip clip_of(const Surf &s, const int *c)
{
    Clip k = { 0, 0, s.w, s.h };
    if (c) { if (c[0] > k.x0) k.x0 = c[0]; if (c[1] > k.y0) k.y0 = c[1]; if (c[2] < k.x1) k.x1 = c[2]; if (c[3] < k.y1) k.y1 = c[3]; }
    return k;
}

/* a premultiplied piece onto the surface; alpha 0..255 scales it, light (0..255) adds white under its own alpha, shade (0..255) darkens */
void blit(const Surf &s, const std::vector<uint32_t> &src, int sw, int sh, int dx, int dy, int alpha, int light, int shade, const int *clipv)
{
    const Clip k = clip_of(s, clipv);
    for (int y = 0; y < sh; ++y) {
        const int yy = dy + y;
        if (yy < k.y0 || yy >= k.y1) continue;
        uint32_t *row = s.px + (size_t)yy * s.stride;
        const uint32_t *sp = &src[(size_t)y * sw];
        for (int x = 0; x < sw; ++x) {
            const int xx = dx + x;
            if (xx < k.x0 || xx >= k.x1) continue;
            const uint32_t c = sp[x];
            int a = c >> 24;
            if (!a) continue;
            if (a == 255 && alpha == 255 && !light && !shade) { row[xx] = c; continue; }     /* the opaque wood: a copy */
            int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
            if (shade) { r = r * (255 - shade) / 255; g = g * (255 - shade) / 255; b = b * (255 - shade) / 255; }
            if (light) { r += a * light / 255; g += a * light / 255; b += a * light / 255; if (r > a) r = a; if (g > a) g = a; if (b > a) b = a; }
            if (alpha < 255) { r = r * alpha / 255; g = g * alpha / 255; b = b * alpha / 255; a = a * alpha / 255; }
            row[xx] = over(row[xx], r, g, b, a);
        }
    }
}

/* a whole picture scaled to dw x dh (cached) */
const Piece *scaled_im(int id, int dw, int dh)
{
    if (dw < 1 || dh < 1) return 0;
    load_im(id);
    if (g_im[id].px.empty()) return 0;
    Piece *p = find_piece(1, id, dw, dh);
    if (p) return p;
    p = new_piece(1, id, dw, dh, dw, dh);
    scale_rect(g_im[id], 0, 0, g_im[id].w, g_im[id].h, dw, dh, &p->px);
    return p;
}

void draw_im(const Surf &s, int id, int x, int y, int w, int h, int alpha, int light, int shade, const int *clipv)
{
    const Piece *p = scaled_im(id, w, h);
    if (p) blit(s, p->px, p->w, p->h, x, y, alpha, light, shade, clipv);
}

/* the signed distance (pixels, negative inside) from a pixel centre to the rounded rectangle [0, w) x [0, h) with corner radius r */
inline float rr_sd(float px, float py, float w, float h, float r)
{
    const float cx = w * 0.5f, cy = h * 0.5f;
    const float qx = fabsf(px - cx) - (cx - r), qy = fabsf(py - cy) - (cy - r);
    const float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    const float in = (qx > qy ? qx : qy);
    return sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - r;
}
inline int cov_of(float sd) { const float c = 0.5f - sd; return c <= 0 ? 0 : (c >= 1 ? 255 : (int)(c * 255 + 0.5f)); }

void fill_rrect(const Surf &s, float x, float y, float w, float h, float r, uint32_t rgb, int alpha, const int *clipv)
{
    const Clip k = clip_of(s, clipv);
    const int x0 = (int)floorf(x) - 1, y0 = (int)floorf(y) - 1, x1 = (int)ceilf(x + w) + 1, y1 = (int)ceilf(y + h) + 1;
    const int R = (rgb >> 16) & 255, G = (rgb >> 8) & 255, B = rgb & 255;
    if (r > h * 0.5f) r = h * 0.5f;
    if (r > w * 0.5f) r = w * 0.5f;
    for (int yy = y0; yy < y1; ++yy) {
        if (yy < k.y0 || yy >= k.y1) continue;
        uint32_t *row = s.px + (size_t)yy * s.stride;
        for (int xx = x0; xx < x1; ++xx) {
            if (xx < k.x0 || xx >= k.x1) continue;
            const int c = cov_of(rr_sd(xx + 0.5f - x, yy + 0.5f - y, w, h, r)) * alpha / 255;
            if (!c) continue;
            row[xx] = over(row[xx], R * c / 255, G * c / 255, B * c / 255, c);
        }
    }
}

void fill_circle(const Surf &s, float cx, float cy, float rad, uint32_t rgb, int alpha, const int *clipv)
{
    fill_rrect(s, cx - rad, cy - rad, rad * 2, rad * 2, rad, rgb, alpha, clipv);
}

/* the whole picture one half darker, the way the ROM's own pause dims the game */
void dim_all(const Surf &s)
{
    for (int y = 0; y < s.h; ++y) {
        uint32_t *row = s.px + (size_t)y * s.stride;
        int x = 0;
#if defined(_MSC_VER) || defined(__SSE2__)
        const __m128i keep = _mm_set1_epi32(0x007F7F7F), opaque = _mm_set1_epi32((int)0xFF000000u);
        for (; x + 4 <= s.w; x += 4) {          /* four pixels a step: half of every channel, alpha forced opaque */
            const __m128i v = _mm_loadu_si128((const __m128i *)(row + x));
            _mm_storeu_si128((__m128i *)(row + x), _mm_or_si128(_mm_and_si128(_mm_srli_epi32(v, 1), keep), opaque));
        }
#endif
        for (; x < s.w; ++x) row[x] = 0xFF000000u | ((row[x] >> 1) & 0x007F7F7Fu);
    }
}

/* ===================================================== composed pieces (cached) ======================================== */

/* the parchment sheet: tile + dark rim, rounded, w x h (u100 = the page unit x 100, only to key the cache) */
const Piece *sheet_piece(int w, int h, int u100)
{
    Piece *p = find_piece(2, w, h, u100);
    if (p) return p;
    const double u = u100 / 100.0;
    const int tile = (int)(380 * u + 0.5);
    std::vector<uint32_t> tilepx;
    {
        const Piece *tp = scaled_im(IM_PARCH, tile, tile);
        if (tp) tilepx = tp->px;
    }
    const bool have_tile = !tilepx.empty();
    p = new_piece(2, w, h, u100, w, h);
    const float rad = (float)(26 * u), rim = (float)(6 * u);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float sd = rr_sd(x + 0.5f, y + 0.5f, (float)w, (float)h, rad);
            const int cv = cov_of(sd);
            if (!cv) continue;
            int r, g, b;
            if (sd > -rim) { r = 0x5C; g = 0x2C; b = 0x0A; }
            else if (have_tile) {
                const uint32_t c = tilepx[(size_t)(y % tile) * tile + (x % tile)];
                r = (c >> 16) & 255; g = (c >> 8) & 255; b = c & 255;
            } else { r = 0xE8; g = 0xCC; b = 0x7A; }
            if (sd > -rim - 1.0f && sd <= -rim && have_tile) {   /* soften the rim's inner edge */
                const float f = -rim - sd;               /* 0..1 */
                const uint32_t c = tilepx[(size_t)(y % tile) * tile + (x % tile)];
                const int tr = (c >> 16) & 255, tg = (c >> 8) & 255, tb = c & 255;
                r = (int)(0x5C + (tr - 0x5C) * f); g = (int)(0x2C + (tg - 0x2C) * f); b = (int)(0x0A + (tb - 0x0A) * f);
            }
            p->px[(size_t)y * w + x] = ((uint32_t)cv << 24) | ((uint32_t)(r * cv / 255) << 16) | ((uint32_t)(g * cv / 255) << 8) | (uint32_t)(b * cv / 255);
        }
    return p;
}

/* a wooden keycap or plank of any width: the art's ends stay, the middle stretches (kind 3 = key, 4 = plank) */
const Piece *slice3_piece(int kind, int imgid, int w, int h, double end_frac)
{
    Piece *p = find_piece(kind, w, h, 0);
    if (p) return p;
    load_im(imgid);
    const Img &I = g_im[imgid];
    p = new_piece(kind, w, h, 0, w, h);
    if (I.px.empty()) return p;
    const int aw = (int)(I.w * (double)h / I.h + 0.5);        /* the art at this height */
    std::vector<uint32_t> full;
    scale_rect(I, 0, 0, I.w, I.h, aw, h, &full);
    int endw = (int)(aw * end_frac + 0.5);
    if (w < aw) endw = (int)(endw * (double)w / aw);          /* narrower than the art: both ends shrink with it */
    for (int x = 0; x < w; ++x) {
        int sx;
        if (w <= aw) sx = (int)((x + 0.5) * aw / w);
        else if (x < endw) sx = x;
        else if (x >= w - endw) sx = aw - (w - x);
        else sx = endw + (int)((x - endw) * (double)(aw - 2 * endw) / (double)(w - 2 * endw));
        if (sx < 0) sx = 0;
        if (sx >= aw) sx = aw - 1;
        for (int y = 0; y < h; ++y) p->px[(size_t)y * w + x] = full[(size_t)y * aw + sx];
    }
    return p;
}

/* the purple wooden panel (the drop-down box and its list), w x h: the art's top strip, its body cut (not squashed) to the room, its bottom strip */
const Piece *purple_piece(int w, int h)
{
    Piece *p = find_piece(5, w, h, 0);
    if (p) return p;
    load_im(IM_DDTOP); load_im(IM_DDMID); load_im(IM_DDBOT);
    p = new_piece(5, w, h, 0, w, h);
    const Img &T = g_im[IM_DDTOP], &M = g_im[IM_DDMID], &Bm = g_im[IM_DDBOT];
    if (M.px.empty() || T.px.empty() || Bm.px.empty()) return p;
    const double st = h / 180.0 < 0.25 ? 0.25 : (h / 180.0 > 1.0 ? 1.0 : h / 180.0);
    const double across = (double)w / M.w;
    int th = (int)(T.h * across * 3 * st + 0.5); if (th < 2) th = 2;
    int bh = (int)(Bm.h * across * 3 * st + 0.5); if (bh < 1) bh = 1;
    if (th + bh > h) { th = h / 4; bh = h / 8; if (th < 1) th = 1; if (bh < 1) bh = 1; }
    const int mh = h - th - bh;
    std::vector<uint32_t> body, top, bot;
    double srcH = mh / across; if (srcH > M.h) srcH = M.h;
    int sh = (int)(srcH + 0.5); if (sh < 1) sh = 1;
    if (mh > 0) scale_rect(M, 0, 0, M.w, sh, w, mh, &body);
    scale_rect(T, 0, 0, T.w, T.h, w, th, &top);
    scale_rect(Bm, 0, 0, Bm.w, Bm.h, w, bh, &bot);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint32_t c;
            if (y < th) c = top[(size_t)y * w + x];
            else if (y >= h - bh) c = bot[(size_t)(y - (h - bh)) * w + x];
            else c = body.empty() ? 0 : body[(size_t)(y - th) * w + x];
            p->px[(size_t)y * w + x] = c;
        }
    const float rad = h * 0.16f;                               /* rounded corners */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int cv = cov_of(rr_sd(x + 0.5f, y + 0.5f, (float)w, (float)h, rad));
            if (cv >= 255) continue;
            uint32_t &c = p->px[(size_t)y * w + x];
            const int a = (c >> 24) * cv / 255;
            c = ((uint32_t)a << 24) | ((((c >> 16) & 255) * cv / 255) << 16) | ((((c >> 8) & 255) * cv / 255) << 8) | ((c & 255) * cv / 255);
        }
    return p;
}

/* the title's arrow pointing right (the ROM's own selection arrow, turned a quarter), w x h */
const Piece *arrow_right_piece(int w, int h)
{
    Piece *p = find_piece(6, w, h, 0);
    if (p) return p;
    p = new_piece(6, w, h, 0, w, h);
    const uint32_t *src = menu_hostimg_scaled(MENU_IMG_ARROW_DOWN, h, w);   /* turned: its width is our height */
    if (!src) return p;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) p->px[(size_t)y * w + x] = src[(size_t)x * h + (h - 1 - y)];   /* dest(x, y) = src(h - 1 - y, x) */
    return p;
}

/* =========================================================== state and rows ============================================ */

enum Tab { T_CONTROLS = 0, T_CAMERA, T_SOUND, T_COUNT };
enum RType { R_CHOICE, R_SLIDER, R_CHECK, R_BIND, R_HEAD };
struct Row { RType t; const char *label; int id, id2, step; const char *const *names; int nn; };

const char *const RUN_LABELS[] = { "Hold button", "Analog stick", "Always run" };
const char *const CAM_LABELS[] = { "Analog chase", "Free camera", "DS stepped" };

const Row ROWS_CONTROLS[] = {
    { R_CHOICE, "Run", SL_RUN_MODE, -1, 0, RUN_LABELS, 3 },
    { R_SLIDER, "Stick deadzone", SL_STICK_DEADZONE, -1, 1, 0, 0 },
    { R_SLIDER, "Run point", SL_RUN_THRESHOLD, -1, 1, 0, 0 },
    { R_HEAD, "Keys and buttons", -1, -1, 0, 0, 0 },
    { R_BIND, "Up", SL_KEY_BASE + HOST_KEY_UP, SL_KEY_BASE + HOST_KEY_UP_ALT, 0, 0, 0 },
    { R_BIND, "Down", SL_KEY_BASE + HOST_KEY_DOWN, SL_KEY_BASE + HOST_KEY_DOWN_ALT, 0, 0, 0 },
    { R_BIND, "Left", SL_KEY_BASE + HOST_KEY_LEFT, SL_KEY_BASE + HOST_KEY_LEFT_ALT, 0, 0, 0 },
    { R_BIND, "Right", SL_KEY_BASE + HOST_KEY_RIGHT, SL_KEY_BASE + HOST_KEY_RIGHT_ALT, 0, 0, 0 },
    { R_BIND, "Jump", SL_KEY_BASE + HOST_KEY_JUMP, SL_PAD_BASE + HOST_PAD_JUMP, 0, 0, 0 },
    { R_BIND, "Attack", SL_KEY_BASE + HOST_KEY_ATTACK, SL_PAD_BASE + HOST_PAD_ATTACK, 0, 0, 0 },
    { R_BIND, "Crouch", SL_KEY_BASE + HOST_KEY_CROUCH, SL_PAD_BASE + HOST_PAD_CROUCH, 0, 0, 0 },
    { R_BIND, "Run", SL_KEY_BASE + HOST_KEY_RUN, SL_PAD_BASE + HOST_PAD_RUN, 0, 0, 0 },
    { R_BIND, "Start", SL_KEY_BASE + HOST_KEY_START, SL_PAD_BASE + HOST_PAD_START, 0, 0, 0 },
    { R_BIND, "Select", SL_KEY_BASE + HOST_KEY_SELECT, SL_PAD_BASE + HOST_PAD_SELECT, 0, 0, 0 },
    { R_BIND, "Look", SL_KEY_BASE + HOST_KEY_LOOK, SL_PAD_BASE + HOST_PAD_LOOK, 0, 0, 0 },
};
const Row ROWS_CAMERA[] = {
    { R_CHOICE, "Camera", SL_CAMERA_MODE, -1, 0, CAM_LABELS, 3 },
    { R_SLIDER, "Camera speed", SL_CAMERA_SENSITIVITY, -1, 5, 0, 0 },
    { R_SLIDER, "Mouse speed", SL_MOUSE_SENSITIVITY, -1, 5, 0, 0 },
    { R_CHECK, "Invert camera left/right", SL_SWAP_CAMERA_TURN, -1, 0, 0, 0 },
    { R_CHECK, "Invert camera up/down", SL_INVERT_CAMERA_Y, -1, 0, 0, 0 },
    { R_CHECK, "Lock mouse", SL_MOUSE_CAPTURE, -1, 0, 0, 0 },
};
const Row ROWS_SOUND[] = {
    { R_SLIDER, "Volume", SL_VOLUME, -1, 5, 0, 0 },
    { R_SLIDER, "Minimap size", SL_MINIMAP_SCALE, -1, 5, 0, 0 },
};
const char *const TAB_NAMES[T_COUNT] = { "CONTROLS", "CAMERA", "SOUND" };

const Row *rows_of(int tab, int *n)
{
    switch (tab) {
    case T_CAMERA: *n = (int)(sizeof ROWS_CAMERA / sizeof ROWS_CAMERA[0]); return ROWS_CAMERA;
    case T_SOUND: *n = (int)(sizeof ROWS_SOUND / sizeof ROWS_SOUND[0]); return ROWS_SOUND;
    default: *n = (int)(sizeof ROWS_CONTROLS / sizeof ROWS_CONTROLS[0]); return ROWS_CONTROLS;
    }
}

struct State {
    int open, pending, hold_touch;
    int tab, sel, col;                 /* sel == nrows: the Reset plank */
    int scroll;                        /* body pixels scrolled */
    int dd_open, dd_hi;                /* a drop-down list is open on row sel */
    int cap_active, cap_id, cap_frames;/* a rebind is waiting */
    int drag;                          /* a slider row being dragged, -1 none */
    int cursor_shown;                  /* the pad / key selection arrow shows */
    unsigned held_prev, hold_age[16];
    int ml_prev, mr_prev, mx_prev, my_prev, mouse_known;
    int pf;                            /* page frames since open */
    int w, h;                          /* the last drawn picture size */
    int ensure_vis;                    /* scroll to the selection on the next layout */
    char note[160]; int note_t;
    int pressed_plank;                 /* 0 none, 1 back, 2 reset (mouse held on it) */
    double draw_ms;
};
State S;

void note(const char *s)
{
    snprintf(S.note, sizeof S.note, "%s", s ? s : "");
    S.note_t = s && *s ? 300 : 0;
}

}  // namespace

/* ============================================================ names =================================================== */

extern "C" const char *settings_page_key_name(int vk, char *buf, int cap)
{
    static const struct { int vk; const char *n; } T[] = {
        { 0, "NONE" }, { 0x10, "SHIFT" }, { 0x11, "CTRL" }, { 0x12, "ALT" }, { 0x20, "SPACE" }, { 0x09, "TAB" }, { 0xA0, "L SHIFT" }, { 0xA1, "R SHIFT" },
        { 0xA2, "L CTRL" }, { 0xA3, "R CTRL" }, { 0xA4, "L ALT" }, { 0xA5, "R ALT" }, { 0x14, "CAPS" }, { 0x08, "BKSP" }, { 0xC0, "`" }, { 0xBC, "," }, { 0xBE, "." },
        { 0xBF, "/" }, { 0xBA, ";" }, { 0xDE, "'" }, { 0xDB, "[" }, { 0xDD, "]" }, { 0xBD, "-" }, { 0xBB, "=" }, { 0xDC, "\\" }, { 0x26, "UP" }, { 0x28, "DOWN" },
        { 0x25, "LEFT" }, { 0x27, "RIGHT" }, { 0x0D, "ENTER" }, { 0x1B, "ESC" }, { 0x2D, "INS" }, { 0x2E, "DEL" }, { 0x24, "HOME" }, { 0x23, "END" },
        { 0x21, "PG UP" }, { 0x22, "PG DN" }, { 0x6A, "NUM *" }, { 0x6B, "NUM +" }, { 0x6D, "NUM -" }, { 0x6E, "NUM ." }, { 0x6F, "NUM /" },
    };
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; ++i)
        if (T[i].vk == vk) return T[i].n;
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) { snprintf(buf, cap, "%c", (char)vk); return buf; }
    if (vk >= 0x70 && vk <= 0x87) { snprintf(buf, cap, "F%d", vk - 0x6F); return buf; }
    if (vk >= 0x60 && vk <= 0x69) { snprintf(buf, cap, "NUM %d", vk - 0x60); return buf; }
    snprintf(buf, cap, "KEY %d", vk);
    return buf;
}

extern "C" const char *settings_page_pad_name(int m, char *buf, int cap)
{
    switch (m) {
    case 0: return "NONE";
    case 0x1000: return "A";
    case 0x2000: return "B";
    case 0x4000: return "X";
    case 0x8000: return "Y";
    case 0x0100: return "LB";
    case 0x0200: return "RB";
    case 0x10000: return "LT";
    case 0x20000: return "RT";
    case 0x0010: return "START";
    case 0x0020: return "BACK";
    case 0x0040: return "L3";
    case 0x0080: return "R3";
    case 0x0001: return "D UP";
    case 0x0002: return "D DOWN";
    case 0x0004: return "D LEFT";
    case 0x0008: return "D RIGHT";
    default: snprintf(buf, cap, "PAD %X", (unsigned)m); return buf;
    }
}

namespace {

const char *id_name(int id, char *buf, int cap)
{
    if (id < SL_PAD_BASE) return settings_page_key_name(sl_get(id), buf, cap);
    return settings_page_pad_name(sl_get(id), buf, cap);
}

/* ============================================================ layout ================================================== */

struct RowL { int y, h; };
struct Lay {
    double u;
    int w, h;
    int sx, sy, sw, sh;               /* the sheet */
    int title[4], back[4];            /* planks: x, y, w, h */
    int tab_x[T_COUNT], tab_w[T_COUNT], tab_y, tab_h;
    int body[4];                      /* x0, y0, x1, y1 (the clipped area) */
    int rowh, lab_x, ctl_x, ctl_w, rail_w;
    int reset[4];                     /* x, y, w, h */
    int content_h, scroll_max;
    int nrows;
    RowL r[24];
    int bar[4];                       /* scroll bar track: x, y, w, h */
};
Lay L;

inline int P(double v) { return (int)(v * L.u + 0.5); }

void compute_layout(int w, int h)
{
    L.w = w; L.h = h;
    const double u = (double)w / 1024.0, v = (double)h / 768.0;
    L.u = u < v ? u : v;
    if (L.u < 0.3) L.u = 0.3;
    L.sw = P(900); L.sh = P(664);
    L.sx = (w - L.sw) / 2; L.sy = (h - L.sh) / 2 + P(36);
    if (L.sy + L.sh > h - 2) L.sy = h - 2 - L.sh;
    L.title[2] = P(380); L.title[3] = L.title[2] * 160 / 600;
    L.title[0] = (w - L.title[2]) / 2; L.title[1] = L.sy - L.title[3] / 2;
    L.back[2] = P(158); L.back[3] = L.back[2] * 182 / 320;
    L.back[0] = L.sx + P(30); L.back[1] = L.sy - L.back[3] / 2;
    L.tab_h = P(44); L.tab_y = L.sy + P(74);
    {   /* the tab names in a row, centred on the sheet */
        const float em = (float)P(34);
        const float gap = (float)P(46);
        float tot = 0, tw[T_COUNT];
        for (int i = 0; i < T_COUNT; ++i) { tw[i] = ui_text_width(TAB_NAMES[i], em); tot += tw[i] + (i ? gap : 0); }
        float x = L.sx + (L.sw - tot) * 0.5f;
        for (int i = 0; i < T_COUNT; ++i) { L.tab_x[i] = (int)(x + 0.5f); L.tab_w[i] = (int)(tw[i] + 0.5f); x += tw[i] + gap; }
    }
    L.body[0] = L.sx + P(14); L.body[1] = L.tab_y + L.tab_h + P(14); L.body[2] = L.sx + L.sw - P(14); L.body[3] = L.sy + L.sh - P(84);
    L.rowh = P(54);
    L.lab_x = L.sx + P(60); L.ctl_x = L.sx + P(380); L.ctl_w = P(380); L.rail_w = P(290);
    L.reset[2] = P(190); L.reset[3] = P(46);
    L.reset[0] = L.sx + L.sw - P(40) - L.reset[2]; L.reset[1] = L.sy + L.sh - P(30) - L.reset[3];
    int n = 0;
    rows_of(S.tab, &n);
    L.nrows = n;
    L.content_h = n * L.rowh;
    const int vis = L.body[3] - L.body[1];
    L.scroll_max = L.content_h > vis ? L.content_h - vis : 0;
    if (S.ensure_vis && S.sel < n) {
        const int top = S.sel * L.rowh, bot = top + L.rowh;
        if (top < S.scroll) S.scroll = top;
        if (bot > S.scroll + vis) S.scroll = bot - vis;
        S.ensure_vis = 0;
    } else if (S.ensure_vis) S.ensure_vis = 0;
    if (S.scroll > L.scroll_max) S.scroll = L.scroll_max;
    if (S.scroll < 0) S.scroll = 0;
    for (int i = 0; i < n && i < 24; ++i) { L.r[i].y = L.body[1] + i * L.rowh - S.scroll; L.r[i].h = L.rowh; }
    L.bar[0] = L.sx + L.sw - P(34); L.bar[2] = P(12); L.bar[1] = L.body[1]; L.bar[3] = vis;
}

/* the keycap widths of one binding row, for both layout and the draw */
int cap_h() { return P(46); }
int cap_w_for(const char *txt)
{
    const int kh = cap_h();
    const float em = kh * 0.50f;
    const float tw = ui_text_width(txt, em);
    const int w = (int)(tw + kh * 1.25f + 0.5f);
    const int mn = (int)(kh * 1.76f + 0.5f);
    return w < mn ? mn : w;
}

struct CapPos { int x, w; };
void caps_of(const Row &R, CapPos *cp /* [2] */)
{
    char b1[24], b2[24];
    const char *t1 = id_name(R.id, b1, sizeof b1);
    const char *t2 = R.id2 >= 0 ? id_name(R.id2, b2, sizeof b2) : "";
    cp[0].w = cap_w_for(t1);
    cp[1].w = R.id2 >= 0 ? cap_w_for(t2) : 0;
    cp[0].x = L.ctl_x;
    cp[1].x = L.ctl_x + cp[0].w + P(24);
}

/* ============================================================ row logic =============================================== */

bool row_focusable(const Row &R)
{
    if (R.t == R_HEAD) return false;
    if (R.t == R_BIND) return true;
    return sl_applicable(R.id) != 0;
}

int step_of(const Row &R) { return R.step > 0 ? R.step : 1; }

void set_value(int id, int v)
{
    if (!sl_set(id, v)) {
        const char *why = sl_set_refused();
        if (why && *why) note(why);
    }
}

void adjust(const Row &R, int dir, int big)
{
    const SlInfo *I = sl_info(R.id);
    if (!I) return;
    int v = sl_get(R.id);
    if (R.t == R_SLIDER) {
        v += dir * step_of(R) * (big ? 5 : 1);
        v = v < I->min ? I->min : (v > I->max ? I->max : v);
        set_value(R.id, v);
    } else if (R.t == R_CHOICE) {
        v += dir;
        if (v < I->min) v = I->max;
        if (v > I->max) v = I->min;
        set_value(R.id, v);
    } else if (R.t == R_CHECK) {
        set_value(R.id, v ? 0 : 1);
    }
}

void reset_tab()
{
    int n = 0;
    const Row *rows = rows_of(S.tab, &n);
    for (int pass = 0; pass < 2; ++pass)           /* a binding restored before the one that held its key: the second pass settles it */
        for (int i = 0; i < n; ++i) {
            const Row &R = rows[i];
            if (R.t == R_HEAD) continue;
            const SlInfo *a = sl_info(R.id);
            if (a && sl_get(R.id) != a->def) sl_set(R.id, a->def);
            if (R.t == R_BIND && R.id2 >= 0) {
                const SlInfo *b = sl_info(R.id2);
                if (b && sl_get(R.id2) != b->def) sl_set(R.id2, b->def);
            }
        }
    sl_flush();
    note("Back to the defaults");
}

void start_capture(int id)
{
    sl_set_refused();
    if (sl_capture_start(id)) {
        S.cap_active = 1; S.cap_id = id; S.cap_frames = 0;
        note("");
    } else {
        note("That cannot be changed right now");
    }
}

void switch_tab(int t)
{
    if (t < 0) t = T_COUNT - 1;
    if (t >= T_COUNT) t = 0;
    if (t == S.tab) return;
    sl_flush();
    S.tab = t; S.sel = 0; S.col = 0; S.scroll = 0; S.dd_open = 0; S.drag = -1;
    int n = 0;
    const Row *rows = rows_of(t, &n);
    while (S.sel < n && !row_focusable(rows[S.sel])) ++S.sel;
    S.ensure_vis = 1;
}

void move_sel(int dir)
{
    int n = 0;
    const Row *rows = rows_of(S.tab, &n);
    int s = S.sel;
    for (;;) {
        s += dir;
        if (s < 0 || s > n) return;                    /* n is the Reset plank */
        if (s == n || row_focusable(rows[s])) break;
    }
    S.sel = s;
    if (s < n && rows[s].t != R_BIND) S.col = 0;
    S.ensure_vis = 1;
    S.dd_open = 0;
}

/* ============================================================ the frame =============================================== */

int repeat_fire(int bit, unsigned held)
{
    /* an edge fires at once; a key held fires again after 16 frames and then every 3 */
    if (!((held >> bit) & 1)) { S.hold_age[bit] = 0; return 0; }
    const unsigned a = S.hold_age[bit]++;
    return a == 0 || (a > 16 && ((a - 16) % 3) == 0);
}
int edge_only(int bit, unsigned held)
{
    const int was = (S.held_prev >> bit) & 1;
    return ((held >> bit) & 1) && !was;
}

bool in_rect(int x, int y, const int *r) { return x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]; }

int row_at(int x, int y)
{
    if (x < L.body[0] || x >= L.body[2] || y < L.body[1] || y >= L.body[3]) return -1;
    for (int i = 0; i < L.nrows && i < 24; ++i)
        if (y >= L.r[i].y && y < L.r[i].y + L.r[i].h) return i;
    return -1;
}

void slider_geometry(int *rx, int *rw)
{
    *rx = L.ctl_x + P(17); *rw = L.rail_w - P(34);
}

void drag_slider(const Row &R, int mx)
{
    const SlInfo *I = sl_info(R.id);
    if (!I) return;
    int rx, rw;
    slider_geometry(&rx, &rw);
    double f = (double)(mx - rx) / rw;
    f = f < 0 ? 0 : (f > 1 ? 1 : f);
    int v = I->min + (int)floor(f * (I->max - I->min) + 0.5);
    const int st = step_of(R);
    v = ((v - I->min + st / 2) / st) * st + I->min;
    v = v < I->min ? I->min : (v > I->max ? I->max : v);
    if (v != sl_get(R.id)) set_value(R.id, v);
}

void activate(const Row &R, int col)
{
    switch (R.t) {
    case R_CHOICE: S.dd_open = 1; S.dd_hi = sl_get(R.id); break;
    case R_CHECK: adjust(R, 1, 0); break;
    case R_BIND: start_capture(col ? R.id2 : R.id); break;
    default: break;
    }
}

}  // namespace

/* ============================================================ public: state ============================================ */

extern "C" int settings_page_open(void) { return S.open; }
extern "C" void settings_page_request(void) { S.pending = 1; S.hold_touch = 1; }
extern "C" int settings_page_pending(void) { return S.pending; }
extern "C" void settings_page_clear_request(void) { S.pending = 0; }
extern "C" int settings_page_tab(void) { return S.tab; }
extern "C" int settings_page_row(void) { return S.sel; }
extern "C" const char *settings_page_note(void) { return S.note; }
extern "C" double settings_page_draw_ms(void) { return S.draw_ms; }

extern "C" void settings_page_open_now(int from)
{
    (void)from;
    const int w = S.w, h = S.h;
    memset(&S, 0, sizeof S);
    S.w = w ? w : 1024; S.h = h ? h : 768;
    S.open = 1; S.drag = -1; S.hold_touch = 1;
    S.tab = T_CONTROLS;
    S.cursor_shown = 1;          /* the arrow stands on the first row from the start, whoever opened it */
    S.ensure_vis = 1;
    S.held_prev = ~0u;           /* the press that opened it is still down: no edges on the first frame */
    for (int i = 0; i < 16; ++i) S.hold_age[i] = 1;
    S.ml_prev = 1; S.mr_prev = 1;
    sl_set_refused();
    int n = 0;
    const Row *rows = rows_of(S.tab, &n);
    while (S.sel < n && !row_focusable(rows[S.sel])) ++S.sel;
}

extern "C" void settings_page_close(void)
{
    if (S.cap_active) { sl_capture_cancel(); S.cap_active = 0; }
    sl_flush();
    S.open = 0; S.pending = 0; S.hold_touch = 1;
    S.dd_open = 0; S.drag = -1;
}

extern "C" int settings_page_swallow_touch(int touch_down)
{
    if (S.open) return 1;
    if (S.hold_touch) {
        if (touch_down) return 1;
        S.hold_touch = 0;
    }
    return 0;
}

extern "C" int settings_page_frame(const SpInput *in)
{
    if (!S.open) return 0;
    ++S.pf;
    if (S.note_t > 0 && --S.note_t == 0) S.note[0] = 0;
    compute_layout(S.w, S.h);
    const unsigned held = in->keys;
    const int mx = in->mouse_x, my = in->mouse_y;
    const int mvalid = in->mouse_valid;
    const int l_edge = in->mouse_l && !S.ml_prev;
    const int r_edge = in->mouse_r && !S.mr_prev;
    const int moved = mvalid && S.mouse_known && (mx != S.mx_prev || my != S.my_prev);   /* a pointer resting where the page opened does not pick a row */
    int n = 0;
    const Row *rows = rows_of(S.tab, &n);
    int closing = 0;

    if (S.cap_active) {
        /* a rebind waiting: the engine's capture owns the keys; the page only watches and gives up */
        ++S.cap_frames;
        const int st = sl_capture_state();
        const char *why = sl_capture_refused();
        if (st == SL_CAP_BOUND) { S.cap_active = 0; note(""); }
        else if (st == SL_CAP_CANCELLED) { S.cap_active = 0; note("Kept the old binding"); }
        else if (!sl_capture_active()) { S.cap_active = 0; }
        else {
            if (why && *why) note(why);
            if (((held & SPK_CAP_CANCEL) && edge_only(9, held)) || l_edge || r_edge || S.cap_frames > 900) {
                sl_capture_cancel();
                S.cap_active = 0;
                note("Kept the old binding");
            }
        }
    } else if (S.dd_open && S.sel < n) {
        /* the open drop-down list */
        const Row &R = rows[S.sel];
        const SlInfo *I = sl_info(R.id);
        const int bh = P(46), by = L.r[S.sel].y + (L.rowh - bh) / 2, lh = P(44);
        if (repeat_fire(0, held)) { if (S.dd_hi > I->min) --S.dd_hi; }
        if (repeat_fire(1, held)) { if (S.dd_hi < I->max) ++S.dd_hi; }
        if (edge_only(4, held)) { set_value(R.id, S.dd_hi); S.dd_open = 0; }
        else if (edge_only(5, held)) { S.dd_open = 0; }
        else if (l_edge) {
            if (mx >= L.ctl_x && mx < L.ctl_x + L.ctl_w && my >= by + bh && my < by + bh + lh * R.nn + P(16)) {
                const int i = (my - (by + bh + P(8))) / lh;
                if (i >= 0 && i < R.nn) set_value(R.id, I->min + i);
            }
            S.dd_open = 0;
        } else if (moved && mx >= L.ctl_x && mx < L.ctl_x + L.ctl_w) {
            const int i = (my - (by + bh + P(8))) / lh;
            if (i >= 0 && i < R.nn) S.dd_hi = I->min + i;
        }
    } else {
        /* ---- navigation ---- */
        if (edge_only(5, held)) closing = 1;
        if (edge_only(6, held)) { switch_tab(S.tab - 1); S.cursor_shown = 1; }
        if (edge_only(7, held)) { switch_tab(S.tab + 1); S.cursor_shown = 1; }
        rows = rows_of(S.tab, &n);
        if (repeat_fire(0, held)) { move_sel(-1); S.cursor_shown = 1; }
        if (repeat_fire(1, held)) { move_sel(+1); S.cursor_shown = 1; }
        const int lf = repeat_fire(2, held), rt = repeat_fire(3, held);
        if (lf || rt) {
            S.cursor_shown = 1;
            const int dir = rt ? 1 : -1;
            if (S.sel < n) {
                const Row &R = rows[S.sel];
                if (R.t == R_BIND) { if (R.id2 >= 0) S.col = dir > 0 ? 1 : 0; }
                else if (R.t == R_SLIDER || R.t == R_CHOICE) adjust(R, dir, 0);
                else if (R.t == R_CHECK && ((dir > 0) != (sl_get(R.id) != 0))) adjust(R, dir, 0);
            }
        }
        if (edge_only(4, held) && S.sel <= n) {
            S.cursor_shown = 1;
            if (S.sel == n) reset_tab();
            else activate(rows[S.sel], S.col);
        }
        if (edge_only(8, held) && S.sel < n && rows[S.sel].t == R_BIND) {
            const Row &R = rows[S.sel];
            set_value(S.col && R.id2 >= 0 ? R.id2 : R.id, 0);
        }

        /* ---- the mouse ---- */
        if (mvalid) {
            const int over_row = row_at(mx, my);
            if (moved) {
                S.cursor_shown = 1;
                if (over_row >= 0 && over_row < n && row_focusable(rows[over_row])) {
                    if (S.sel != over_row) { S.sel = over_row; S.col = 0; }
                    if (rows[over_row].t == R_BIND && rows[over_row].id2 >= 0) {
                        CapPos cp[2]; caps_of(rows[over_row], cp);
                        S.col = (mx >= cp[1].x - P(12)) ? 1 : 0;
                    }
                } else if (in_rect(mx, my, L.reset)) S.sel = n;
            }
            if (in->wheel) {
                S.scroll -= in->wheel * (L.rowh * 3 / 2);
                if (S.scroll < 0) S.scroll = 0;
                if (S.scroll > L.scroll_max) S.scroll = L.scroll_max;
            }
            if (l_edge) {
                S.cursor_shown = 1;
                if (in_rect(mx, my, L.back)) { S.pressed_plank = 1; }
                else if (in_rect(mx, my, L.reset)) { S.pressed_plank = 2; S.sel = n; }
                else {
                    int tabhit = -1;
                    for (int i = 0; i < T_COUNT; ++i)
                        if (mx >= L.tab_x[i] - P(10) && mx < L.tab_x[i] + L.tab_w[i] + P(10) && my >= L.tab_y && my < L.tab_y + L.tab_h) tabhit = i;
                    if (tabhit >= 0) switch_tab(tabhit);
                    else if (over_row >= 0 && over_row < n && row_focusable(rows[over_row])) {
                        const Row &R = rows[over_row];
                        S.sel = over_row;
                        if (R.t == R_SLIDER) {
                            if (mx >= L.ctl_x - P(10) && mx < L.ctl_x + L.rail_w + P(10)) { S.drag = over_row; drag_slider(R, mx); }
                        } else if (R.t == R_CHECK) {
                            if (mx >= L.lab_x && mx < L.ctl_x + L.ctl_w) adjust(R, 1, 0);
                        } else if (R.t == R_CHOICE) {
                            if (mx >= L.ctl_x && mx < L.ctl_x + L.ctl_w) activate(R, 0);
                        } else if (R.t == R_BIND) {
                            CapPos cp[2]; caps_of(R, cp);
                            const int kh = cap_h(), ky = L.r[over_row].y + (L.rowh - kh) / 2;
                            if (my >= ky && my < ky + kh) {
                                if (mx >= cp[0].x && mx < cp[0].x + cp[0].w) { S.col = 0; activate(R, 0); }
                                else if (R.id2 >= 0 && mx >= cp[1].x && mx < cp[1].x + cp[1].w) { S.col = 1; activate(R, 1); }
                            }
                        }
                    }
                }
            }
            if (r_edge && over_row >= 0 && over_row < n && rows[over_row].t == R_BIND) {
                CapPos cp[2]; caps_of(rows[over_row], cp);
                const int col = (rows[over_row].id2 >= 0 && mx >= cp[1].x - P(12)) ? 1 : 0;
                set_value(col ? rows[over_row].id2 : rows[over_row].id, 0);
            }
            if (S.drag >= 0 && in->mouse_l && S.drag < n) drag_slider(rows[S.drag], mx);
            if (S.pressed_plank && !in->mouse_l) {
                if (S.pressed_plank == 1 && in_rect(mx, my, L.back)) closing = 1;
                else if (S.pressed_plank == 2 && in_rect(mx, my, L.reset)) reset_tab();
                S.pressed_plank = 0;
            }
        }
        if (S.drag >= 0 && !in->mouse_l) { S.drag = -1; sl_flush(); }
    }

    if (S.sel > L.nrows) S.sel = L.nrows;
    S.held_prev = held;
    S.ml_prev = in->mouse_l; S.mr_prev = in->mouse_r;
    if (mvalid) { S.mx_prev = mx; S.my_prev = my; S.mouse_known = 1; }
    if (closing) { settings_page_close(); return 0; }
    return 1;
}

/* ============================================================ the draw ================================================ */

namespace {

const uint32_t C_TEXT = 0x7A3A0C, C_QUIET = 0xA06E3C, C_LETTER = 0x8C4200, C_CREAM = 0xFFF3CD;

void text_c(const Surf &s, float cx, float by, const char *t, float em, uint32_t rgb, int alpha, const int *clip)
{
    const float w = ui_text_width(t, em);
    ui_text_draw(&s, cx - w * 0.5f, by, t, em, rgb, alpha, clip);
}
/* a line of text vertically centred (by its capital height) on cy */
void text_mid(const Surf &s, float x, float cy, const char *t, float em, uint32_t rgb, int alpha, const int *clip)
{
    ui_text_draw(&s, x, cy + ui_text_cap_height(em) * 0.5f, t, em, rgb, alpha, clip);
}

void draw_cap(const Surf &s, int x, int y, int w, int h, const char *txt, int sel, int waiting, const int *clip)
{
    const Piece *p = slice3_piece(3, IM_KEY, w, h, 110.0 / 392.0);
    if (p) blit(s, p->px, p->w, p->h, x, y, 255, sel ? 40 : 0, 0, clip);
    const float em = h * 0.50f;
    text_c(s, x + w * 0.5f, y + h * 0.5f + ui_text_cap_height(em) * 0.5f, waiting ? "..." : txt, em, C_LETTER, 255, clip);
}

void draw_dd_box(const Surf &s, int x, int y, int w, int h, const char *txt, int sel, int alpha, const int *clip)
{
    const Piece *p = purple_piece(w, h);
    if (p) blit(s, p->px, p->w, p->h, x, y, alpha, sel ? 30 : 0, 0, clip);
    const float em = h * 0.56f;
    text_mid(s, (float)(x + P(18)), y + h * 0.5f, txt, em, 0xFFFFFF, alpha, clip);
    const int aw = h * 40 / 46, ah = aw * 58 / 92;
    draw_im(s, IM_DDARROW, x + w - aw - P(14), y + (h - ah) / 2, aw, ah, alpha, 0, 0, clip);
}

void draw_check(const Surf &s, int x, int y, int sz, int on, int sel, int alpha, const int *clip)
{
    fill_rrect(s, (float)x, (float)y, (float)sz, (float)sz, sz * 0.19f, C_TEXT, alpha, clip);
    const float b = sz * 0.105f;
    fill_rrect(s, x + b, y + b, sz - 2 * b, sz - 2 * b, sz * 0.12f, sel ? 0xFFFAE6 : C_CREAM, alpha, clip);
    if (on) {
        const float em = sz * 0.72f;
        const float w = ui_text_width("X", em);
        ui_text_draw(&s, x + sz * 0.5f - w * 0.5f, y + sz * 0.5f + ui_text_cap_height(em) * 0.5f, "X", em, C_TEXT, alpha, clip);
    }
}

void draw_slider(const Surf &s, int x, int cy, int rail_w, int v, int vmin, int vmax, int alpha, const int *clip)
{
    const float rh = (float)P(12);
    const float kr = (float)P(17);
    fill_rrect(s, (float)x, cy - rh * 0.5f, (float)rail_w, rh, rh * 0.5f, C_TEXT, alpha, clip);
    const float f = vmax > vmin ? (float)(v - vmin) / (vmax - vmin) : 0;
    const float kx = x + kr + f * (rail_w - 2 * kr);
    fill_circle(s, kx, (float)cy, kr, C_TEXT, alpha, clip);
    fill_circle(s, kx, (float)cy, kr - P(4), C_CREAM, alpha, clip);
}

}  // namespace

extern "C" void settings_page_draw(uint32_t *px, int stride, int w, int h)
{
    if (!S.open || !px || w < 64 || h < 64) return;
    const auto t0 = std::chrono::steady_clock::now();
    S.w = w; S.h = h;
    compute_layout(w, h);
    Surf s = { px, stride, w, h };
    int n = 0;
    const Row *rows = rows_of(S.tab, &n);

    dim_all(s);
    {   /* the sheet */
        const Piece *sp = sheet_piece(L.sw, L.sh, (int)(L.u * 100 + 0.5));
        if (sp) blit(s, sp->px, sp->w, sp->h, L.sx, L.sy, 255, 0, 0, 0);
    }
    draw_im(s, IM_TITLE, L.title[0], L.title[1], L.title[2], L.title[3], 255, 0, 0, 0);
    {
        const int hov = S.mouse_known && in_rect(S.mx_prev, S.my_prev, L.back);
        draw_im(s, IM_BACK, L.back[0], L.back[1] + (S.pressed_plank == 1 ? P(3) : 0), L.back[2], L.back[3], 255, hov ? 30 : 0, S.pressed_plank == 1 ? 25 : 0, 0);
    }
    {   /* tabs */
        const float em = (float)P(34);
        const int by = L.tab_y + L.tab_h - P(10);
        for (int i = 0; i < T_COUNT; ++i) {
            const int on = i == S.tab;
            ui_text_draw(&s, (float)L.tab_x[i], (float)by, TAB_NAMES[i], em, on ? C_TEXT : C_QUIET, 255, 0);
            if (on) fill_rrect(s, (float)L.tab_x[i], (float)(by + P(6)), (float)L.tab_w[i], (float)P(5), (float)P(2), C_TEXT, 255, 0);
        }
        const float sm = (float)P(22);
        text_mid(s, (float)(L.sx + P(50)), (float)(L.tab_y + L.tab_h / 2 - P(2)), "LB", sm, C_QUIET, 200, 0);
        const float rw = ui_text_width("RB", sm);
        text_mid(s, (float)(L.sx + L.sw - P(50)) - rw, (float)(L.tab_y + L.tab_h / 2 - P(2)), "RB", sm, C_QUIET, 200, 0);
    }
    const int *clip = L.body;
    for (int i = 0; i < n && i < 24; ++i) {
        const Row &R = rows[i];
        const int y = L.r[i].y, rh = L.r[i].h;
        if (y + rh < L.body[1] || y > L.body[3]) continue;
        const int cy = y + rh / 2;
        const int app = (R.t == R_BIND || R.t == R_HEAD) ? 1 : sl_applicable(R.id) != 0;
        const int alpha = app ? 255 : 110;
        const int sel = i == S.sel && S.cursor_shown;
        if (R.t == R_HEAD) {
            text_mid(s, (float)L.lab_x, (float)(cy + P(6)), R.label, (float)P(26), C_QUIET, 255, clip);
            continue;
        }
        if (R.t != R_CHECK) text_mid(s, (float)L.lab_x, (float)cy, R.label, (float)P(30), app ? C_TEXT : C_QUIET, alpha, clip);
        char buf[32];
        switch (R.t) {
        case R_CHOICE: {
            const SlInfo *I = sl_info(R.id);
            const int v = sl_get(R.id);
            const char *t = (v >= I->min && v <= I->max && v - I->min < R.nn) ? R.names[v - I->min] : "?";
            draw_dd_box(s, L.ctl_x, cy - P(23), L.ctl_w, P(46), t, sel, alpha, clip);
            break;
        }
        case R_SLIDER: {
            const SlInfo *I = sl_info(R.id);
            int rx, rw;
            slider_geometry(&rx, &rw);
            draw_slider(s, L.ctl_x, cy, L.rail_w, sl_get(R.id), I->min, I->max, alpha, clip);
            snprintf(buf, sizeof buf, "%d%%", sl_get(R.id));
            const float vem = (float)P(28);
            const float vw = ui_text_width(buf, vem);
            text_mid(s, (float)(L.ctl_x + L.rail_w + P(86)) - vw, (float)cy, buf, vem, app ? C_TEXT : C_QUIET, alpha, clip);
            break;
        }
        case R_CHECK: {
            /* a check box reads box first, then its words, as the launcher's do */
            const int sz = P(38);
            draw_check(s, L.lab_x, cy - sz / 2, sz, sl_get(R.id) != 0, sel, alpha, clip);
            text_mid(s, (float)(L.lab_x + sz + P(18)), (float)cy, R.label, (float)P(30), app ? C_TEXT : C_QUIET, alpha, clip);
            break;
        }
        case R_BIND: {
            CapPos cp[2];
            caps_of(R, cp);
            const int kh = cap_h(), ky = cy - kh / 2;
            for (int c = 0; c < (R.id2 >= 0 ? 2 : 1); ++c) {
                const int id = c ? R.id2 : R.id;
                const int waiting = S.cap_active && S.cap_id == id;
                char nb[24];
                draw_cap(s, cp[c].x, ky, cp[c].w, kh, id_name(id, nb, sizeof nb), sel && S.col == c, waiting, clip);
            }
            break;
        }
        default: break;
        }
    }
    if (L.scroll_max > 0) {   /* the scroll bar */
        fill_rrect(s, (float)L.bar[0], (float)L.bar[1], (float)L.bar[2], (float)L.bar[3], L.bar[2] * 0.5f, 0x000000, 34, 0);
        int th = (int)((double)L.bar[3] * L.bar[3] / L.content_h);
        if (th < L.bar[2]) th = L.bar[2];
        const int ty = L.bar[1] + (int)((double)(L.bar[3] - th) * S.scroll / L.scroll_max);
        fill_rrect(s, (float)L.bar[0], (float)ty, (float)L.bar[2], (float)th, L.bar[2] * 0.5f, C_TEXT, 255, 0);
    }
    /* the selection arrow: the ROM's own title arrow, turned to point at the row */
    if (S.cursor_shown && S.sel < n && rows[S.sel].t != R_HEAD) {
        const int aw = P(24), ah = P(34);
        const Piece *ap = arrow_right_piece(aw, ah);
        const int ay = L.r[S.sel].y + L.rowh / 2 - ah / 2;
        if (ap) blit(s, ap->px, ap->w, ap->h, L.sx + P(18) + ((S.pf / 12) % 2 ? P(2) : 0), ay, 255, 0, 0, L.body);
    }
    {   /* the footer: the note on the left, the Reset plank on the right */
        const char *msg = S.note[0] ? S.note
                          : (S.cap_active ? (S.cap_id < SL_PAD_BASE ? "Press a key. Esc keeps the old one." : "Press a button. BACK keeps the old one.") : "");
        const int fy = L.reset[1] + L.reset[3] / 2;
        if (msg[0]) {
            float em = (float)P(24);
            const float room = (float)(L.reset[0] - P(16) - L.lab_x);   /* the note stops short of the Reset plank: a long one shrinks to fit */
            const float need = ui_text_width(msg, em);
            if (room > 0 && need > room) em *= room / need;
            ui_text_draw(&s, (float)L.lab_x, fy + ui_text_cap_height(em) * 0.5f, msg, em, S.note[0] ? 0x9A2A10 : C_QUIET, 255, 0);
        } else if (S.cursor_shown && S.sel < n && rows[S.sel].t == R_BIND) {
            const float em = (float)P(22);
            ui_text_draw(&s, (float)L.lab_x, fy + ui_text_cap_height(em) * 0.5f, "A: change   Y or Delete: clear", em, C_QUIET, 255, 0);
        }
        const int sel = S.sel == n && S.cursor_shown;
        const int hov = S.mouse_known && in_rect(S.mx_prev, S.my_prev, L.reset);
        const Piece *pl = slice3_piece(4, IM_PLANK, L.reset[2], L.reset[3], 0.12);
        const int dy = S.pressed_plank == 2 ? P(2) : 0;
        if (pl) blit(s, pl->px, pl->w, pl->h, L.reset[0], L.reset[1] + dy, 255, (sel || hov) ? 28 : 0, S.pressed_plank == 2 ? 25 : 0, 0);
        const float em = L.reset[3] * 0.50f;
        text_c(s, L.reset[0] + L.reset[2] * 0.5f, L.reset[1] + dy + L.reset[3] * 0.5f + ui_text_cap_height(em) * 0.5f, "RESET", em, C_LETTER, 255, 0);
        if (sel) {
            const int aw = P(24), ah = P(34);
            const Piece *ap = arrow_right_piece(aw, ah);
            if (ap) blit(s, ap->px, ap->w, ap->h, L.reset[0] - aw - P(8), L.reset[1] + L.reset[3] / 2 - ah / 2, 255, 0, 0, 0);
        }
    }
    if (S.dd_open && S.sel < n && rows[S.sel].t == R_CHOICE) {   /* the open drop-down list, over the rows */
        const Row &R = rows[S.sel];
        const int bh = P(46), by = L.r[S.sel].y + (L.rowh - bh) / 2, lh = P(44);
        const int lx = L.ctl_x, ly = by + bh + P(2), lw = L.ctl_w, lhh = lh * R.nn + P(16);
        const Piece *p = purple_piece(lw, lhh);
        if (p) blit(s, p->px, p->w, p->h, lx, ly, 255, 0, 0, 0);
        const SlInfo *I = sl_info(R.id);
        for (int i = 0; i < R.nn; ++i) {
            const int ry = ly + P(8) + i * lh;
            if (I->min + i == S.dd_hi) fill_rrect(s, (float)(lx + P(6)), (float)ry, (float)(lw - P(12)), (float)lh, (float)P(6), 0xFFFFFF, 80, 0);
            text_mid(s, (float)(lx + P(18)), ry + lh * 0.5f, R.names[i], lh * 0.62f, 0xFFFFFF, 255, 0);
        }
    }
    S.draw_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    {   /* SM64DS_SETPAGE_COST=1: the cost of the page's draw, a line every 120 frames (proof rows only) */
        static int on = -1, cnt;
        static double sum, mn = 1e9, mx;
        if (on < 0) on = getenv("SM64DS_SETPAGE_COST") != 0;
        if (on) {
            sum += S.draw_ms; ++cnt;
            if (S.draw_ms < mn) mn = S.draw_ms;
            if (S.draw_ms > mx) mx = S.draw_ms;
            if (cnt == 120) {
                fprintf(stderr, "[settings] page draw %dx%d: %d frames, mean %.2f ms, min %.2f, max %.2f\n", w, h, cnt, sum / cnt, mn, mx);
                cnt = 0; sum = 0; mn = 1e9; mx = 0;
            }
        }
    }
}

/* ============================================================ scripted input (proof rows) ============================== */

extern "C" int settings_page_env_open_at(void)
{
    static int read, at = -1;
    if (!read) {
        read = 1;
        const char *e = getenv("SM64DS_SETPAGE_AT");
        if (e && *e) at = atoi(e);
    }
    return at;
}

/* SM64DS_SETPAGE_SCRIPT = "f:cmd[:a[:b]];..." with f the page frame (1 = the first frame the page is open). Commands: up down left right ok back tabl tabr clear
   capcancel (held for that one frame), move:x:y (the pointer, stays), ldown / lup, rdown / rup (a button, stays), wheel:n (that frame). */
extern "C" int settings_page_scripted(SpInput *in, int pf)
{
    static int read, n;
    struct Step { int f; char cmd[16]; int a, b; };
    static Step st[256];
    static int mx = -1, my = -1, ml, mr;
    if (!read) {
        read = 1;
        const char *e = getenv("SM64DS_SETPAGE_SCRIPT");
        while (e && *e && n < 256) {
            Step s;
            memset(&s, 0, sizeof s);
            char cmd[16] = {};
            int f = 0, got = 0;
            if (sscanf(e, "%d:%15[a-z]%n", &f, cmd, &got) >= 2) {
                e += got;
                int a = 0, b = 0;
                if (*e == ':') { ++e; a = (int)strtol(e, (char **)&e, 10); }
                if (*e == ':') { ++e; b = (int)strtol(e, (char **)&e, 10); }
                s.f = f; snprintf(s.cmd, sizeof s.cmd, "%s", cmd); s.a = a; s.b = b;
                st[n++] = s;
            }
            while (*e && *e != ';') ++e;
            if (*e == ';') ++e;
        }
    }
    if (!n) return 0;
    in->keys = 0; in->wheel = 0;
    for (int i = 0; i < n; ++i) {
        if (st[i].f != pf) continue;
        const char *c = st[i].cmd;
        if (!strcmp(c, "up")) in->keys |= SPK_UP;
        else if (!strcmp(c, "down")) in->keys |= SPK_DOWN;
        else if (!strcmp(c, "left")) in->keys |= SPK_LEFT;
        else if (!strcmp(c, "right")) in->keys |= SPK_RIGHT;
        else if (!strcmp(c, "ok")) in->keys |= SPK_ACCEPT;
        else if (!strcmp(c, "back")) in->keys |= SPK_BACK;
        else if (!strcmp(c, "tabl")) in->keys |= SPK_TABL;
        else if (!strcmp(c, "tabr")) in->keys |= SPK_TABR;
        else if (!strcmp(c, "clear")) in->keys |= SPK_CLEAR;
        else if (!strcmp(c, "capcancel")) in->keys |= SPK_CAP_CANCEL;
        else if (!strcmp(c, "move")) { mx = st[i].a; my = st[i].b; }
        else if (!strcmp(c, "ldown")) ml = 1;
        else if (!strcmp(c, "lup")) ml = 0;
        else if (!strcmp(c, "rdown")) mr = 1;
        else if (!strcmp(c, "rup")) mr = 0;
        else if (!strcmp(c, "wheel")) in->wheel += st[i].a;
        else if (!strcmp(c, "key")) in->inject_key = st[i].a;
    }
    in->mouse_valid = mx >= 0; in->mouse_x = mx; in->mouse_y = my; in->mouse_l = ml; in->mouse_r = mr;
    return 1;
}
