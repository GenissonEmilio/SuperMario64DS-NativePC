/* hal/char_hud.cpp: see char_hud.h. Never a crash: a missing or bad picture costs only that picture (one stderr line). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <algorithm>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "char_hud.h"
#include "char_registry.h"
#include "../third_party/stb/stb_image.h"

extern "C" {
extern void *_ZN3OAM10LIFE_ICONSE[];
extern void *_ZN3OAM15MM_PLAYER_ICONSE[];
extern unsigned char data_ov002_02111178;     /* HUD::RenderLifeCount: states 3..5 draw on engine A, else B */
extern unsigned char data_ov001_020abb34[];   /* the four star select portrait lists, 0x20 bytes apart, base 0 1 2 3 */
/* The minimap's own vtable and Render, under C names (linker aliases to the class TU's C++ symbols; see seat_minimap below). */
extern void *char_hud_dmap_vtable[];
extern int __fastcall char_hud_dmap_render(void *self, void *dead_edx);
}
#if defined(_MSC_VER) && defined(_M_IX86)
#pragma comment(linker, "/alternatename:_char_hud_dmap_vtable=??_7dMap_c@@6B@")
#pragma comment(linker, "/alternatename:@char_hud_dmap_render@8=?Render@dMap_c@@UAEHXZ")
#endif

namespace {

struct Pic {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    bool ok() const { return w > 0 && h > 0; }
};

void load_png(const char *path, Pic *out)
{
    *out = Pic();
    if (!path || !*path) return;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (8L << 20)) { fclose(f); return; }
    std::vector<unsigned char> buf((size_t)n);
    const size_t got = fread(&buf[0], 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) return;
    int w = 0, h = 0, comp = 0;
    unsigned char *p = stbi_load_from_memory(&buf[0], (int)n, &w, &h, &comp, 4);
    if (!p) return;
    if (w > 0 && h > 0 && w <= 2048 && h <= 2048) {
        out->w = w;
        out->h = h;
        out->rgba.assign(p, p + (size_t)w * h * 4);
    }
    stbi_image_free(p);
}

inline uint32_t rd32(uint32_t a) { return *reinterpret_cast<volatile uint32_t *>(a); }
inline uint16_t rd16(uint32_t a) { return *reinterpret_cast<volatile uint16_t *>(a); }
inline uint8_t rd8(uint32_t a) { return *reinterpret_cast<volatile uint8_t *>(a); }
inline void wr16(uint32_t a, uint16_t v) { *reinterpret_cast<volatile uint16_t *>(a) = v; }
inline void wr8(uint32_t a, uint8_t v) { *reinterpret_cast<volatile uint8_t *>(a) = v; }

int g_log = -1;
bool logging()
{
    if (g_log < 0) { const char *e = getenv("SM64DS_CHARHUD_LOG"); g_log = e && *e && *e != '0'; }
    return g_log != 0;
}

const int kSizes[3][4][2] = { { {8, 8}, {16, 16}, {32, 32}, {64, 64} }, { {16, 8}, {32, 8}, {32, 16}, {64, 32} }, { {8, 16}, {8, 32}, {16, 32}, {32, 64} } };

struct Ent { int x, y, w, h; unsigned a0, a1, a2; };

/* The OamAttr list of one element: up to 8 sprites, attr3 0xffff marks the last (that one is drawn too). Same list shape the picker reads. */
int parse_list(const void *list, Ent *e)
{
    const unsigned short *a = (const unsigned short *)list;
    if (!a) return 0;
    int n = 0;
    for (int i = 0; i < 8; ++i) {
        const unsigned a0 = a[i * 4], a1 = a[i * 4 + 1], a2 = a[i * 4 + 2], a3 = a[i * 4 + 3];
        const bool last = a3 == 0xffff;
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
    return n;
}

/* One VRAM write: a byte (tile data) or a halfword (palette). */
struct Wr { uint32_t addr; uint16_t v; uint8_t half; };

struct Plan {
    uint32_t dc[2] = { 0, 0 };
    std::vector<Wr> wr;
    bool ok = false;
};

struct Rgb { int r, g, b; };   /* 5 bit */

/* Quantise the opaque colours (5 bit RGB, with counts) of one palette group to at most maxc entries. out[k] = entry k's colour,
   map[colour key] = entry index (0 based, caller adds 1). */
void quantise(const std::vector<std::pair<uint16_t, int> > &cols, int maxc, std::vector<uint16_t> *pal, std::vector<std::pair<uint16_t, int> > *map)
{
    std::vector<std::pair<int, uint16_t> > byc;
    for (size_t i = 0; i < cols.size(); ++i) byc.push_back(std::make_pair(-cols[i].second, cols[i].first));
    std::sort(byc.begin(), byc.end());
    const size_t keep = byc.size() < (size_t)maxc ? byc.size() : (size_t)maxc;
    for (size_t i = 0; i < keep; ++i) pal->push_back(byc[i].second);
    for (size_t i = 0; i < byc.size(); ++i) {
        const uint16_t c = byc[i].second;
        int best = 0, bd = 1 << 30;
        for (size_t k = 0; k < pal->size(); ++k) {
            const uint16_t q = (*pal)[k];
            const int dr = (c & 31) - (q & 31), dg = ((c >> 5) & 31) - ((q >> 5) & 31), db = ((c >> 10) & 31) - ((q >> 10) & 31);
            const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
            if (d < bd) { bd = d; best = (int)k; }
        }
        map->push_back(std::make_pair(c, best));
    }
}

/* The pixel of the picture under bbox pixel (px, py) of a W x H box: the average of the source area it covers, alpha weighted. */
bool sample(const Pic &P, int W, int H, int px, int py, uint16_t *col555)
{
    const int x0 = px * P.w / W, x1 = ((px + 1) * P.w + W - 1) / W;
    const int y0 = py * P.h / H, y1 = ((py + 1) * P.h + H - 1) / H;
    long sr = 0, sg = 0, sb = 0, sa = 0, n = 0;
    for (int y = y0; y < y1 && y < P.h; ++y)
        for (int x = x0; x < x1 && x < P.w; ++x) {
            const uint8_t *p = &P.rgba[((size_t)y * P.w + x) * 4];
            sr += p[0] * p[3]; sg += p[1] * p[3]; sb += p[2] * p[3]; sa += p[3]; ++n;
        }
    if (n == 0 || sa * 2 < n * 255) return false;     /* mostly clear: no pixel */
    const int r = (int)(sr / sa), g = (int)(sg / sa), b = (int)(sb / sa);
    *col555 = (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
    return true;
}

/* Everything one ELEMENT needs written: nl sprite lists (list l drawn on engine eng[l] from picture pic[l]). Lists of one element often
   share tiles and palette banks (the minimap marker is a face sprite shared by three looks plus a cap sprite per look), so the palettes
   are built per (engine, colour depth, bank) over every list that uses the bank, and one colour always gets one index. */
void build_element(int nl, const int *eng, const void *const *lists, const Pic *const *pic, Plan *pl, const char *tag)
{
    pl->wr.clear();
    pl->ok = false;
    pl->dc[0] = rd32(0x04000000u);
    pl->dc[1] = rd32(0x04001000u);
    struct LB { Ent e[8]; int n, x0, y0, W, H; std::vector<int> col; };
    std::vector<LB> lb((size_t)nl);
    struct Grp { int eng; bool c256; unsigned pal; std::vector<std::pair<uint16_t, int> > cols; std::vector<uint16_t> pal_out; std::vector<std::pair<uint16_t, int> > map; };
    std::vector<Grp> grp;
    for (int l = 0; l < nl; ++l) {
        LB &B = lb[(size_t)l];
        B.n = parse_list(lists[l], B.e);
        if (B.n == 0 || !pic[l] || !pic[l]->ok()) { B.n = 0; continue; }
        int x1 = -9999, y1 = -9999;
        B.x0 = 9999; B.y0 = 9999;
        for (int i = 0; i < B.n; ++i) {
            if (B.e[i].x < B.x0) B.x0 = B.e[i].x;
            if (B.e[i].y < B.y0) B.y0 = B.e[i].y;
            if (B.e[i].x + B.e[i].w > x1) x1 = B.e[i].x + B.e[i].w;
            if (B.e[i].y + B.e[i].h > y1) y1 = B.e[i].y + B.e[i].h;
        }
        B.W = x1 - B.x0; B.H = y1 - B.y0;
        if (B.W < 1 || B.H < 1 || B.W > 256 || B.H > 256) { B.n = 0; continue; }
        B.col.assign((size_t)B.W * B.H, -1);
        for (int y = 0; y < B.H; ++y)
            for (int x = 0; x < B.W; ++x) {
                uint16_t c;
                if (sample(*pic[l], B.W, B.H, x, y, &c)) B.col[(size_t)y * B.W + x] = c;
            }
        for (int i = 0; i < B.n; ++i) {
            const Ent &s = B.e[i];
            const bool c256 = (s.a0 & 0x2000) != 0;
            const unsigned pal = (s.a2 >> 12) & 0xF;
            int g = -1;
            for (size_t k = 0; k < grp.size(); ++k) if (grp[k].eng == eng[l] && grp[k].c256 == c256 && grp[k].pal == pal) g = (int)k;
            if (g < 0) { Grp ng; ng.eng = eng[l]; ng.c256 = c256; ng.pal = pal; grp.push_back(ng); g = (int)grp.size() - 1; }
            Grp &G = grp[(size_t)g];
            for (int sy = 0; sy < s.h; ++sy)
                for (int sx = 0; sx < s.w; ++sx) {
                    const int c = B.col[(size_t)(s.y - B.y0 + sy) * B.W + (s.x - B.x0 + sx)];
                    if (c < 0) continue;
                    bool found = false;
                    for (size_t k = 0; k < G.cols.size(); ++k) if (G.cols[k].first == c) { ++G.cols[k].second; found = true; break; }
                    if (!found) G.cols.push_back(std::make_pair((uint16_t)c, 1));
                }
        }
    }
    for (size_t g = 0; g < grp.size(); ++g) {
        Grp &G = grp[g];
        const uint32_t pltt = G.eng ? 0x05000600u : 0x05000200u;
        const uint32_t objext = (G.eng && ((pl->dc[1] >> 31) & 1)) ? 0x068A0000u : 0;
        /* a 16 colour bank has entries 1..15; a 256 colour sprite on the extended palette 1..255; a 256 colour sprite on the shared flat
           palette is mapped onto the colours already there (writing it would recolour every other 256 colour sprite) */
        if (G.c256 && !objext) {
            std::vector<uint16_t> have;
            for (int k = 1; k < 256; ++k) have.push_back(rd16(pltt + (uint32_t)k * 2u) & 0x7FFF);
            for (size_t c = 0; c < G.cols.size(); ++c) {
                const uint16_t col = G.cols[c].first;
                int best = 0, bd = 1 << 30;
                for (size_t k = 0; k < have.size(); ++k) {
                    const uint16_t q = have[k];
                    const int dr = (col & 31) - (q & 31), dg = ((col >> 5) & 31) - ((q >> 5) & 31), db = ((col >> 10) & 31) - ((q >> 10) & 31);
                    const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
                    if (d < bd) { bd = d; best = (int)k; }
                }
                G.map.push_back(std::make_pair(col, best));
            }
        } else {
            quantise(G.cols, G.c256 ? 255 : 15, &G.pal_out, &G.map);
            for (size_t k = 0; k < G.pal_out.size(); ++k) {
                const uint32_t a = G.c256 ? objext + (G.pal * 256u + 1u + (uint32_t)k) * 2u : pltt + (G.pal * 16u + 1u + (uint32_t)k) * 2u;
                Wr w = { a, (uint16_t)G.pal_out[k], 1 };
                pl->wr.push_back(w);
            }
        }
    }
    /* tiles: each sprite's stored pixels, 16 colour tiles two to a byte */
    for (int l = 0; l < nl; ++l) {
        const LB &B = lb[(size_t)l];
        const uint32_t dispcnt = pl->dc[eng[l]];
        const uint32_t boundary = 32u << ((dispcnt >> 20) & 3);
        const bool map1d = (dispcnt >> 4) & 1;
        const uint32_t vram = eng[l] ? 0x06600000u : 0x06400000u;
        for (int i = 0; i < B.n; ++i) {
            const Ent &s = B.e[i];
            const bool c256 = (s.a0 & 0x2000) != 0, hflip = (s.a1 & 0x1000) != 0, vflip = (s.a1 & 0x2000) != 0;
            const unsigned pal = (s.a2 >> 12) & 0xF;
            const Grp *G = 0;
            for (size_t k = 0; k < grp.size(); ++k) if (grp[k].eng == eng[l] && grp[k].c256 == c256 && grp[k].pal == pal) G = &grp[k];
            if (!G) continue;
            const uint32_t tile = s.a2 & 0x3FF;
            std::vector<uint8_t> idxv((size_t)s.w * s.h, 0);
            for (int sy = 0; sy < s.h; ++sy)
                for (int sx = 0; sx < s.w; ++sx) {
                    const int c = B.col[(size_t)(s.y - B.y0 + sy) * B.W + (s.x - B.x0 + sx)];
                    uint8_t idx = 0;
                    if (c >= 0)
                        for (size_t k = 0; k < G->map.size(); ++k) if (G->map[k].first == c) { idx = (uint8_t)(G->map[k].second + 1); break; }
                    idxv[(size_t)sy * s.w + sx] = idx;
                }
            for (int ty = 0; ty < s.h; ++ty)
                for (int tx = 0; tx < s.w; ++tx) {
                    const int sx = hflip ? s.w - 1 - tx : tx, sy = vflip ? s.h - 1 - ty : ty;
                    const uint8_t idx = idxv[(size_t)sy * s.w + sx];
                    const int tcol = tx >> 3, trow = ty >> 3, fx = tx & 7, fy = ty & 7;
                    const uint32_t slot = map1d ? (uint32_t)(trow * (s.w / 8) + tcol) * (c256 ? 2u : 1u) : (uint32_t)(trow * 32 + (c256 ? tcol * 2 : tcol));
                    const uint32_t cell = vram + tile * boundary + slot * 32u;
                    if (c256) {
                        Wr w = { cell + (uint32_t)fy * 8u + (uint32_t)fx, idx, 0 };
                        pl->wr.push_back(w);
                    } else {
                        const uint32_t a = cell + (uint32_t)fy * 4u + (uint32_t)fx / 2u;
                        size_t at = pl->wr.size();
                        for (size_t k = pl->wr.size(); k > 0; --k) if (!pl->wr[k - 1].half && pl->wr[k - 1].addr == a) { at = k - 1; break; }
                        uint8_t b = at != pl->wr.size() ? (uint8_t)pl->wr[at].v : 0;
                        b = (fx & 1) ? (uint8_t)((b & 0x0F) | (idx << 4)) : (uint8_t)((b & 0xF0) | idx);
                        if (at != pl->wr.size()) pl->wr[at].v = b;
                        else { Wr w = { a, b, 0 }; pl->wr.push_back(w); }
                    }
                }
        }
    }
    pl->ok = !pl->wr.empty();
    if (logging()) {
        fprintf(stderr, "[charhud] %s: %d list(s), %u write(s), dispcnt %08x %08x\n", tag, nl, (unsigned)pl->wr.size(), (unsigned)pl->dc[0], (unsigned)pl->dc[1]);
        for (int l = 0; l < nl; ++l)
            for (int i = 0; i < lb[(size_t)l].n; ++i) {
                const Ent &e = lb[(size_t)l].e[i];
                fprintf(stderr, "[charhud]   list %d engine %c sprite %d: %dx%d at %d,%d tile %u %s pal %u\n", l, eng[l] ? 'B' : 'A', i, e.w, e.h, e.x, e.y,
                        e.a2 & 0x3FF, (e.a0 & 0x2000) ? "256col" : "16col", (e.a2 >> 12) & 0xF);
            }
    }
}

void apply_plan(const Plan &pl)
{
    for (size_t i = 0; i < pl.wr.size(); ++i) {
        const Wr &w = pl.wr[i];
        if (w.half) { if (rd16(w.addr) != w.v) wr16(w.addr, w.v); }
        else if (rd8(w.addr) != (uint8_t)w.v) wr8(w.addr, (uint8_t)w.v);
    }
}


/* SM64DS_CHARHUD_DUMP=<folder>: write the ROM's own pictures of these elements (all four base characters, as the VRAM holds them now)
   as <name>.rgba (8 byte header w h as 32 bit LE, then w*h RGBA bytes). A tool for authors; it never changes the game. */
void dump_list(int eng, const void *list, const char *name)
{
    const char *dir = getenv("SM64DS_CHARHUD_DUMP");
    if (!dir || !*dir) return;
    Ent e[8];
    const int n = parse_list(list, e);
    if (n == 0) return;
    const uint32_t dispcnt = rd32(eng ? 0x04001000u : 0x04000000u);
    const uint32_t boundary = 32u << ((dispcnt >> 20) & 3);
    const bool map1d = (dispcnt >> 4) & 1;
    const uint32_t vram = eng ? 0x06600000u : 0x06400000u;
    const uint32_t pltt = eng ? 0x05000600u : 0x05000200u;
    const uint32_t objext = (eng && ((dispcnt >> 31) & 1)) ? 0x068A0000u : 0;
    int x0 = 9999, y0 = 9999, x1 = -9999, y1 = -9999;
    for (int i = 0; i < n; ++i) {
        if (e[i].x < x0) x0 = e[i].x;
        if (e[i].y < y0) y0 = e[i].y;
        if (e[i].x + e[i].w > x1) x1 = e[i].x + e[i].w;
        if (e[i].y + e[i].h > y1) y1 = e[i].y + e[i].h;
    }
    const int W = x1 - x0, H = y1 - y0;
    if (W < 1 || H < 1 || W > 256 || H > 256) return;
    std::vector<uint8_t> px((size_t)W * H * 4, 0);
    for (int k = n - 1; k >= 0; --k) {
        const Ent &s = e[k];
        const bool c256 = (s.a0 & 0x2000) != 0, hflip = (s.a1 & 0x1000) != 0, vflip = (s.a1 & 0x2000) != 0;
        const uint32_t tile = s.a2 & 0x3FF, pal = (s.a2 >> 12) & 0xF;
        for (int sy = 0; sy < s.h; ++sy)
            for (int sx = 0; sx < s.w; ++sx) {
                const int tx = hflip ? s.w - 1 - sx : sx, ty = vflip ? s.h - 1 - sy : sy;
                const int tcol = tx >> 3, trow = ty >> 3, fx = tx & 7, fy = ty & 7;
                const uint32_t slot = map1d ? (uint32_t)(trow * (s.w / 8) + tcol) * (c256 ? 2u : 1u) : (uint32_t)(trow * 32 + (c256 ? tcol * 2 : tcol));
                const uint32_t cell = vram + tile * boundary + slot * 32u;
                uint32_t col;
                if (c256) {
                    const uint32_t idx = rd8(cell + (uint32_t)fy * 8u + (uint32_t)fx);
                    if (!idx) continue;
                    col = objext ? rd16(objext + (pal * 256u + idx) * 2u) : rd16(pltt + idx * 2u);
                } else {
                    const uint8_t b = rd8(cell + (uint32_t)fy * 4u + (uint32_t)fx / 2u);
                    const uint32_t idx = (fx & 1) ? (b >> 4) : (b & 0xF);
                    if (!idx) continue;
                    col = rd16(pltt + (pal * 16u + idx) * 2u);
                }
                const uint32_t r = col & 0x1F, g = (col >> 5) & 0x1F, b = (col >> 10) & 0x1F;
                const int dx = s.x - x0 + sx, dy = s.y - y0 + sy;
                uint8_t *o = &px[((size_t)dy * W + dx) * 4];
                o[0] = (uint8_t)(r << 3 | r >> 2); o[1] = (uint8_t)(g << 3 | g >> 2); o[2] = (uint8_t)(b << 3 | b >> 2); o[3] = 255;
            }
    }
    char path[512];
    snprintf(path, sizeof path, "%s/%s_eng%c.rgba", dir, name, eng ? 'B' : 'A');
    FILE *f = fopen(path, "wb");
    if (!f) return;
    const uint32_t hdr[2] = { (uint32_t)W, (uint32_t)H };
    fwrite(hdr, 4, 2, f);
    fwrite(&px[0], 1, px.size(), f);
    fclose(f);
    fprintf(stderr, "[charhud] dumped %s\n", path);
}

void dump_all(int what)
{
    static const char *const dir = getenv("SM64DS_CHARHUD_DUMP");
    if (!dir || !*dir || what > 3) return;
    static unsigned done;
    if (done & (1u << what)) return;
    done |= 1u << what;
    char nm[64];
    for (int b = 0; b < 4; ++b) {
        if (what == PCH_LIFE || what == PCH_STARSEL_LIFE) {
            snprintf(nm, sizeof nm, "%s_b%d", what == PCH_LIFE ? "life" : "sslife", b);
            dump_list(0, _ZN3OAM10LIFE_ICONSE[b], nm);
            if (what == PCH_LIFE) dump_list(1, _ZN3OAM10LIFE_ICONSE[b], nm);
        } else if (what == PCH_MINIMAP) {
            for (int k = 0; k < 4; ++k) { snprintf(nm, sizeof nm, "map_b%d_k%d", b, k); dump_list(1, _ZN3OAM15MM_PLAYER_ICONSE[b * 4 + k], nm); }
        } else {
            snprintf(nm, sizeof nm, "face_b%d", b);
            dump_list(1, data_ov001_020abb34 + 0x20 * b, nm);
        }
    }
}

/* One element: its pictures (a second one only for the minimap's no-cap marker) and its one plan, rebuilt when a display mode changes. */
struct Slot {
    Pic pic[2];
    bool tried = false;
    Plan plan;
};

Slot g_slot[5];

const char *pic_path(int what, int ci, int which)
{
    if (what == PCH_MINIMAP) return which ? port_char_map_icon_nocap(ci) : port_char_map_icon(ci);
    return what == PCH_LIFE || what == PCH_LIFE_ROM || what == PCH_STARSEL_LIFE ? port_char_hud_icon(ci) : port_char_face(ci);
}

/* lists[l] drawn on engines[l] from picture which[l] (0 the main picture, 1 the no-cap one, which falls back to the main one) */
void do_lists(int what, const void *const *lists, const int *engines, const int *which, int nl, const char *tag)
{
    const int ci = port_char_active();
    if (ci < 0) return;
    Slot &S = g_slot[what];
    if (!S.tried) {
        S.tried = true;
        for (int w = 0; w < 2; ++w) {
            const char *p = pic_path(what, ci, w);
            if (!p) continue;
            load_png(p, &S.pic[w]);
            if (!S.pic[w].ok()) fprintf(stderr, "[charhud] %s: %s is not a usable picture; the base art is used\n", tag, p);
            else fprintf(stderr, "[charhud] %s: repainting from %s (%dx%d)\n", tag, p, S.pic[w].w, S.pic[w].h);
        }
    }
    const Pic *pp[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    bool any = false;
    for (int l = 0; l < nl && l < 8; ++l) {
        const Pic *P = &S.pic[which[l]];
        pp[l] = P->ok() ? P : S.pic[0].ok() ? &S.pic[0] : 0;
        if (pp[l]) any = true;
    }
    if (!any) return;
    if (!S.plan.ok || S.plan.dc[0] != rd32(0x04000000u) || S.plan.dc[1] != rd32(0x04001000u)) build_element(nl, engines, lists, pp, &S.plan, tag);
    if (S.plan.ok) apply_plan(S.plan);
}

}  // namespace

extern "C" void port_char_hud_apply(int what);

namespace {
/* THE MINIMAP SEAT. This line's dMap_c is main's class TU (src/actors/dMap_c.cpp): it owns the vtable the minimap object really uses, and its
   Render is called straight through it. The port's own Minimap::Render / Behavior copies (unmatched/Minimap_*.cpp, where the marker repaint
   used to be called from) are not in this link, so the repaint is put in front of Render here instead: the slot that holds Render in the
   class TU's vtable gets a wrapper that repaints and then runs the original. Done once, the first time a costume with a minimap marker is
   active; a vtable lives in read-only data, so the page is opened for the one pointer write. */
int (__fastcall *g_dmap_render_orig)(void *, void *) = 0;
int __fastcall dmap_render_wrap(void *self, void *edx)
{
    port_char_hud_apply(PCH_MINIMAP);
    return g_dmap_render_orig(self, edx);
}
void seat_minimap(void)
{
    static bool tried = false;
    if (tried) return;
    tried = true;
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86)
    void *orig = (void *)&char_hud_dmap_render;
    for (int i = 0; i < 24; ++i) {
        if (char_hud_dmap_vtable[i] != orig) continue;
        DWORD old = 0;
        if (!VirtualProtect(&char_hud_dmap_vtable[i], sizeof(void *), PAGE_READWRITE, &old)) break;
        g_dmap_render_orig = (int (__fastcall *)(void *, void *))orig;
        char_hud_dmap_vtable[i] = (void *)dmap_render_wrap;
        VirtualProtect(&char_hud_dmap_vtable[i], sizeof(void *), old, &old);
        fprintf(stderr, "[charhud] minimap marker: repaint seated in front of dMap_c::Render (vtable slot %d)\n", i);
        return;
    }
    fprintf(stderr, "[charhud] minimap marker: dMap_c::Render is not in the minimap's vtable; the base marker is used\n");
#endif
}
}  // namespace

extern "C" void port_char_hud_apply(int what)
{
    if (what != PCH_MINIMAP && port_char_active() >= 0 && port_char_map_icon(port_char_active())) seat_minimap();
    dump_all(what);
    const int ci = port_char_active();
    if (ci < 0) return;
    if (!pic_path(what, ci, 0) && !pic_path(what, ci, 1)) return;
    const int base = port_char_base(ci);
    if (base < 0 || base > 3) return;
    static const int z4[4] = { 0, 0, 0, 0 };
    if (what == PCH_LIFE || what == PCH_LIFE_ROM) {
        /* HUD::InitResources puts the same tile file at the start of both engines' OBJ VRAM and the palette in both. The port's adventure arm
           draws the counter on engine A; the ROM's own leaf draws on A in states 3..5 and on B otherwise */
        const void *l[1] = { _ZN3OAM10LIFE_ICONSE[base] };
        const int eng[1] = { what == PCH_LIFE ? 0 : (data_ov002_02111178 >= 3 && data_ov002_02111178 < 6) ? 0 : 1 };
        do_lists(what, l, eng, z4, 1, "life icon");
    } else if (what == PCH_MINIMAP) {
        /* Minimap::Render draws MM_PLAYER_ICONS[base * 4 + cap]: cap 0..2 = the Mario / Luigi / Wario cap worn, 3 = no cap */
        const void *l[4];
        const int eng[4] = { 1, 1, 1, 1 };
        const int wh[4] = { 0, 0, 0, 1 };
        for (int k = 0; k < 4; ++k) l[k] = _ZN3OAM15MM_PLAYER_ICONSE[base * 4 + k];
        do_lists(what, l, eng, wh, 4, "minimap icon");
    } else if (what == PCH_STARSEL_LIFE) {
        const void *l[1] = { _ZN3OAM10LIFE_ICONSE[base] };
        const int eng[1] = { 0 };
        do_lists(what, l, eng, z4, 1, "star select life icon");
    } else if (what == PCH_STARSEL) {
        const void *l[1] = { data_ov001_020abb34 + 0x20 * base };
        const int eng[1] = { 1 };
        do_lists(what, l, eng, z4, 1, "star select face");
    }
}
