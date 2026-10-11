/* ui_text.cpp -- see ui_text.h (run menu1, lane SETMENU1). */
#include "ui_text.h"
#include "../third_party/stb/stb_image.h"

#include <math.h>
#include <string.h>
#include <vector>
#include <unordered_map>

extern "C" const unsigned char g_setart_png_font[];
extern "C" const unsigned int g_setart_png_font_len;

namespace {

struct UiGlyphSrc { int ch, x, y, w, h, bx, by; float adv; };
#include "../res/menu/settings/font_metrics.inc"

struct Glyph { int ox, oy, w, h; std::vector<uint8_t> a; };   /* ox, oy: top left from the pen on the baseline, in pixels */

std::vector<uint8_t> g_atlas;
int g_tried, g_ok;
std::unordered_map<uint32_t, Glyph> g_cache;

void load()
{
    if (g_tried) return;
    g_tried = 1;
    int w = 0, h = 0, comp = 0;
    unsigned char *p = stbi_load_from_memory(g_setart_png_font, (int)g_setart_png_font_len, &w, &h, &comp, 1);
    if (!p) return;
    if (w != UI_FONT_AW || h != UI_FONT_AH) { stbi_image_free(p); return; }
    g_atlas.assign(p, p + (size_t)w * h);
    stbi_image_free(p);
    g_ok = 1;
}

const UiGlyphSrc *src_of(int ch)
{
    if (ch < 32 || ch > 126) ch = '?';
    return &kGlyphSrc[ch - 32];
}

/* the glyph at k = em / 96: dest pixel (i, j) covers the source box [i / k, (i + 1) / k) x [j / k, (j + 1) / k) of the glyph's own
   coordinates (pen at 0, baseline at 0, up negative); its value is the mean atlas coverage over that box */
const Glyph *glyph_at(int ch, int em)
{
    const uint32_t key = (uint32_t)ch << 16 | (uint32_t)em;
    auto it = g_cache.find(key);
    if (it != g_cache.end()) return &it->second;
    if (g_cache.size() > 3000) g_cache.clear();
    Glyph &G = g_cache[key];
    const UiGlyphSrc *s = src_of(ch);
    if (!s->w) return &G;
    const double k = (double)em / UI_FONT_EM;
    const int x0 = (int)floor(s->bx * k), x1 = (int)ceil((s->bx + s->w) * k);
    const int y0 = (int)floor(s->by * k), y1 = (int)ceil((s->by + s->h) * k);
    G.ox = x0; G.oy = y0; G.w = x1 - x0; G.h = y1 - y0;
    if (G.w < 1 || G.h < 1 || G.w > 1024 || G.h > 1024) { G.w = G.h = 0; return &G; }
    G.a.assign((size_t)G.w * G.h, 0);
    const double inv = 1.0 / k;
    for (int j = 0; j < G.h; ++j) {
        const double sy0 = (y0 + j) * inv - s->by, sy1 = (y0 + j + 1) * inv - s->by;   /* atlas rows, relative to the glyph's box */
        const int ra = (int)floor(sy0), rb = (int)ceil(sy1);
        for (int i = 0; i < G.w; ++i) {
            const double sx0 = (x0 + i) * inv - s->bx, sx1 = (x0 + i + 1) * inv - s->bx;
            const int ca = (int)floor(sx0), cb = (int)ceil(sx1);
            double acc = 0, area = 0;
            for (int r = ra; r < rb; ++r) {
                const double wy = fmin(sy1, r + 1) - fmax(sy0, (double)r);
                if (wy <= 0) continue;
                for (int c = ca; c < cb; ++c) {
                    const double wx = fmin(sx1, c + 1) - fmax(sx0, (double)c);
                    if (wx <= 0) continue;
                    const double wgt = wx * wy;
                    area += wgt;
                    if (r >= 0 && r < s->h && c >= 0 && c < s->w) acc += wgt * g_atlas[(size_t)(s->y + r) * UI_FONT_AW + s->x + c];
                }
            }
            const int v = area > 0 ? (int)(acc / area + 0.5) : 0;
            G.a[(size_t)j * G.w + i] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
    return &G;
}

}  // namespace

int ui_text_ready(void) { load(); return g_ok; }

float ui_text_width(const char *s, float em_px)
{
    load();
    int em = (int)(em_px + 0.5f);
    if (em < 4) em = 4;
    const float k = (float)em / UI_FONT_EM;
    float w = 0;
    for (; *s; ++s) w += src_of((unsigned char)*s)->adv * k;
    return w;
}

float ui_text_cap_height(float em_px) { return em_px * 0.75f; }

float ui_text_draw(const UiSurface *dst, float x, float by, const char *s, float em_px, uint32_t rgb, int alpha, const int *clip)
{
    load();
    if (!g_ok || !dst || !dst->px || alpha <= 0) return x;
    int em = (int)(em_px + 0.5f);
    if (em < 4) em = 4;
    if (em > 400) em = 400;
    const float k = (float)em / UI_FONT_EM;
    int cx0 = 0, cy0 = 0, cx1 = dst->w, cy1 = dst->h;
    if (clip) { cx0 = clip[0] > cx0 ? clip[0] : cx0; cy0 = clip[1] > cy0 ? clip[1] : cy0; cx1 = clip[2] < cx1 ? clip[2] : cx1; cy1 = clip[3] < cy1 ? clip[3] : cy1; }
    const int R = (rgb >> 16) & 255, G8 = (rgb >> 8) & 255, B = rgb & 255;
    const int iby = (int)floorf(by + 0.5f);
    for (; *s; ++s) {
        const int ch = (unsigned char)*s;
        const UiGlyphSrc *gs = src_of(ch);
        const int px0 = (int)floorf(x + 0.5f);
        if (gs->w) {
            const Glyph *g = glyph_at(ch, em);
            for (int j = 0; j < g->h; ++j) {
                const int yy = iby + g->oy + j;
                if (yy < cy0 || yy >= cy1) continue;
                uint32_t *row = dst->px + (size_t)yy * dst->stride;
                const uint8_t *a = &g->a[(size_t)j * g->w];
                for (int i = 0; i < g->w; ++i) {
                    const int xx = px0 + g->ox + i;
                    if (xx < cx0 || xx >= cx1) continue;
                    int v = a[i];
                    if (!v) continue;
                    v = v * alpha / 255;
                    const uint32_t d = row[xx];
                    const int dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255;
                    row[xx] = 0xFF000000u | (uint32_t)((dr + (R - dr) * v / 255) << 16) | (uint32_t)((dg + (G8 - dg) * v / 255) << 8) | (uint32_t)(db + (B - db) * v / 255);
                }
            }
        }
        x += gs->adv * k;
    }
    return x;
}

int ui_text_cache_count(void) { return (int)g_cache.size(); }
