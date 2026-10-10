/* menu_bigmap.cpp -- Select shows the bottom-screen map big, over the picture (run menu1, lane MAP1).
 *
 * WHAT THE PLAYER GETS. In a course (or inside the castle), with OneScreenMenus on, pressing Select puts the bottom screen's
 * map in the middle of the picture at about 85 percent of its height, the picture dimmed behind it. The game keeps running
 * under it (an overlay, not a pause: the ROM never sees Select, so nothing it simulates changes). Moving (stick, d-pad, A,
 * B), Start, a pause, a message box, or a second Select puts it away. Single player only: netplay and VS leave Select as
 * the ROM has it (a pad rewrite in a transport session desyncs it; the VS pause is the ROM's).
 *
 * WHAT THE ROM DID WITH SELECT (and no longer does with the setting on): src/_ZN5Stage8BehaviorEv.cpp:229-243, with the pause
 * allowed and Start not pressed, a Select edge in PadData[slot].pressed (data_020a0e5a + slot * 4, bit 0x4) sets
 * data_0209f2c4 = 1 and runs PS_Init / PS_Update on the controller mode screen (states 7 / 8). The edge is cleared in both
 * spellings of the pressed word (data_020a0e58 + slot * 4 + 2 and data_020a0e5a + slot * 4) and the held bit dropped, the
 * same seam the settings page uses (hal_menu_pad_seam). Controller Modes stays on the pause menu's plate (PS_Update :750).
 *
 * THE ROM CONDITIONS MIRRORED for "the map may open now" are the ones in front of that branch (:200-214): not in the
 * pause (data_0209f2c4) or the level-clear screen (data_0209f20c), data_0209f294 == 0, no message (data_0209d660), no
 * fader (data_0209d4b0), no level change pending (data_02092110 < 0), Stage::CanPause and not Stage::IsPauseDisabled, not VS
 * (data_0209f2d8 != 1).
 */
#include "hal/menu_bigmap.h"

#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
extern unsigned char data_020a0e40;    /* the local player's slot */
extern int data_020a0e58[];            /* PadData[i]: {u16 held, u16 pressed} */
extern int data_020a0e5a[];            /* the split spelling of .pressed */
extern unsigned char data_0209f2c4;    /* the pause is up */
extern unsigned char data_0209f20c;    /* level-clear owns the game */
extern unsigned char data_0209f294;
extern unsigned char data_0209d660;    /* a message is active */
extern int data_0209d4b0;              /* the fader */
extern signed char data_02092110;      /* staged next level, -1 = none */
extern unsigned char data_0209f2d8;    /* game mode: 1 = VS */
extern char data_0209f4a0[];           /* the Ctrl block's analog magnitude, stride 0x18 per slot */
int host_setting_one_screen_menus(void);
int port_scene_live_id(void);
int port_level_id(void);
int hal_sub_screen_stacked(void);
}

class Stage {
public:
    static int IsPauseDisabled();
    static int CanPause();
};

namespace {

int g_up;            /* latched by the tick, read by the draw */
int g_level_was = -99;
unsigned g_tick;

int trace_on()
{
    static int v = -1;
    if (v < 0) v = getenv("SM64DS_BIGMAP_TRACE") ? 1 : 0;
    return v;
}

void note(const char *what)
{
    if (trace_on()) fprintf(stderr, "[bigmap] tick %u %s\n", g_tick, what);
}

int enabled()
{
    static int v = -1;
    if (v < 0) v = host_setting_one_screen_menus() ? 1 : 0;
    return v;
}

} // namespace

extern "C" int menu_bigmap_up(void) { return g_up; }

extern "C" int menu_bigmap_pad_seam(void)
{
    if (!enabled()) { g_up = 0; return 0; }
    ++g_tick;
    const int scene = port_scene_live_id();
    const int lvl = port_level_id();
    if (trace_on()) {
        static int last_scene = -99, last_stacked = -99;
        const int st = hal_sub_screen_stacked();
        if (scene != last_scene || st != last_stacked) {
            last_scene = scene; last_stacked = st;
            fprintf(stderr, "[bigmap] tick %u scene %d level %d stacked %d\n", g_tick, scene, lvl, st);
        }
    }
    /* a level only: scene 3, or -1 where the level path never runs the scene thunk; never the stacked layout */
    if ((scene != 3 && scene != -1) || hal_sub_screen_stacked()) { g_up = 0; return 0; }
    if (lvl != g_level_was) {            /* a new level always starts with the map away */
        g_level_was = lvl;
        if (g_up) note("close: level changed");
        g_up = 0;
    }
    const int slot = data_020a0e40;
    unsigned short *held = (unsigned short *)((char *)data_020a0e58 + slot * 4);
    unsigned short *pressed = (unsigned short *)((char *)data_020a0e58 + slot * 4 + 2);
    unsigned short *split = (unsigned short *)((char *)data_020a0e5a + slot * 4);

    const int in_play = data_0209f2c4 == 0 && data_0209f20c == 0 && data_0209f2d8 != 1;
    if (!in_play) {                      /* the pause, the level-clear screen, VS: the ROM's own, nothing of ours */
        if (g_up) note("close: pause / level clear");
        g_up = 0;
        return 0;
    }
    const int rom_would_pause_ok =
        data_0209f294 == 0 && data_0209d660 == 0 && data_0209d4b0 == 0 && data_02092110 < 0
        && Stage::CanPause() != 0 && Stage::IsPauseDisabled() == 0;

    const int sel = ((*pressed | *split) & 0x4) != 0;
    const int start = ((*pressed | *split) & 0x8) != 0;
    if (sel) {
        /* the ROM never sees this Select, whether or not the map can open */
        *pressed &= (unsigned short)~0x4;
        *split &= (unsigned short)~0x4;
        *held &= (unsigned short)~0x4;
        if (g_up) { g_up = 0; note("close: Select"); }
        else if (rom_would_pause_ok) { g_up = 1; note("open: Select"); }
        else note("Select swallowed, map not allowed now");
        return g_up;
    }
    if (g_up) {
        const int mag = *(const short *)(data_0209f4a0 + slot * 0x18);
        const int moved = (*held & 0x00F3) != 0 || mag != 0;
        if (start) { g_up = 0; note("close: Start"); }
        else if (!rom_would_pause_ok) { g_up = 0; note("close: game busy"); }
        else if (moved) { g_up = 0; note(mag ? "close: stick" : "close: pad"); }
    }
    return g_up;
}

/* ====================== DRAW SIDE: reads no ROM memory ====================== */

namespace {

/* sharp bilinear: nearest inside a source pixel, a one-destination-pixel blend at its edges, so the map keeps the
   cartridge's crisp lines at any scale and no step is uneven. */
struct Axis { int i0, i1, w; };   /* w: weight of i1 in 0..256 */

void build_axis(Axis *a, int n_dst, int n_src)
{
    const double k = (double)n_dst / n_src;                   /* destination pixels per source pixel */
    for (int d = 0; d < n_dst; ++d) {
        const double s = ((d + 0.5) * n_src) / n_dst;         /* source coordinate, pixel edges at integers */
        int i = (int)s;
        const double f = s - i;                               /* position inside source pixel i */
        int i1;
        double dist;                                          /* source distance to the nearest pixel boundary */
        if (f < 0.5) { i1 = i > 0 ? i - 1 : 0; dist = f; }
        else { i1 = i + 1 < n_src ? i + 1 : n_src - 1; dist = 1.0 - f; }
        /* blend only within half a destination pixel of the boundary: weight 0.5 on the boundary, 0 beyond */
        double t = 0.5 - dist * k;
        if (t < 0) t = 0;
        if (t > 0.5) t = 0.5;
        a[d].i0 = i; a[d].i1 = i1; a[d].w = (int)(t * 256.0 + 0.5);
    }
}

inline uint32_t lerp(uint32_t a, uint32_t b, int w)      /* w of b, 0..256 */
{
    const uint32_t rb = (((a & 0x00FF00FFu) * (uint32_t)(256 - w) + (b & 0x00FF00FFu) * (uint32_t)w) >> 8) & 0x00FF00FFu;
    const uint32_t g = (((a & 0x0000FF00u) * (uint32_t)(256 - w) + (b & 0x0000FF00u) * (uint32_t)w) >> 8) & 0x0000FF00u;
    return 0xFF000000u | rb | g;
}

} // namespace

extern "C" void menu_bigmap_draw(const uint32_t *sub, uint32_t *dst, int w, int h, int stride, int *rect_out)
{
    const auto t0 = std::chrono::steady_clock::now();
    /* the map is 4:3; about 85 percent of the height, or 90 percent of the width, whichever is smaller */
    int th = h * 85 / 100;
    int tw = th * 256 / 192;
    if (tw > w * 90 / 100) { tw = w * 90 / 100; th = tw * 192 / 256; }
    if (tw < 128 || th < 96) { if (rect_out) rect_out[0] = rect_out[1] = rect_out[2] = rect_out[3] = 0; return; }
    const int x0 = (w - tw) / 2, y0 = (h - th) / 2;
    const int bw = th / 96 > 2 ? th / 96 : 2;                 /* frame width */

    /* the picture dimmed behind it: half. Only outside the map's own rectangle (the map is drawn over the rest), and the frame
       is the ring between the two rectangles: nothing is written twice. */
    const int fx0 = x0 - bw, fy0 = y0 - bw, fx1 = x0 + tw + bw, fy1 = y0 + th + bw;   /* the frame's outer rectangle */
    for (int y = 0; y < h; ++y) {
        uint32_t *row = dst + y * stride;
        const int in_band = y >= fy0 && y < fy1;
        const int xa = in_band ? (fx0 > 0 ? fx0 : 0) : w;                 /* dim [0, xa) and [xb, w) */
        const int xb = in_band ? (fx1 < w ? fx1 : w) : w;
        for (int x = 0; x < xa; ++x) row[x] = 0xFF000000u | ((row[x] & 0xFEFEFEu) >> 1);
        for (int x = xb; x < w; ++x) row[x] = 0xFF000000u | ((row[x] & 0xFEFEFEu) >> 1);
        if (in_band) {
            const int dy = y < y0 ? y0 - y : (y >= y0 + th ? y - (y0 + th) + 1 : 0);
            for (int x = xa; x < xb; ++x) {
                if (dy == 0 && x >= x0 && x < x0 + tw) { x = x0 + tw - 1; continue; }   /* the map's own pixels */
                const int dx = x < x0 ? x0 - x : (x >= x0 + tw ? x - (x0 + tw) + 1 : 0);
                const int d = dx > dy ? dx : dy;                  /* 1 = next to the map: a light line, then a dark one */
                row[x] = d <= 1 ? 0xFFF0F0E8u : 0xFF101010u;
            }
        }
    }

    static Axis *ax = nullptr, *ay = nullptr;
    static int axn = 0, ayn = 0;
    if (axn != tw) { free(ax); ax = (Axis *)malloc(sizeof(Axis) * tw); build_axis(ax, tw, 256); axn = tw; }
    if (ayn != th) { free(ay); ay = (Axis *)malloc(sizeof(Axis) * th); build_axis(ay, th, 192); ayn = th; }
    int prev_i0 = -1;
    for (int y = 0; y < th; ++y) {
        uint32_t *out = dst + (y0 + y) * stride + x0;
        const int wy = ay[y].w;
        if (wy == 0 && ay[y].i0 == prev_i0 && y > 0 && ay[y - 1].w == 0) {   /* the same source row again: copy the row just made */
            memcpy(out, out - stride, (size_t)tw * 4);
            continue;
        }
        prev_i0 = ay[y].i0;
        const uint32_t *r0 = sub + ay[y].i0 * 256;
        const uint32_t *r1 = sub + ay[y].i1 * 256;
        for (int x = 0; x < tw; ++x) {
            const int a = ax[x].i0, b = ax[x].i1, wx = ax[x].w;
            uint32_t p;
            if (wx == 0) {
                p = r0[a];
                if (wy) p = lerp(p, r1[a], wy);
            } else {
                p = lerp(r0[a], r0[b], wx);
                if (wy) p = lerp(p, lerp(r1[a], r1[b], wx), wy);
            }
            out[x] = 0xFF000000u | (p & 0x00FFFFFFu);
        }
    }
    if (rect_out) {
        rect_out[0] = x0 - bw; rect_out[1] = y0 - bw; rect_out[2] = x0 + tw + bw; rect_out[3] = y0 + th + bw;
    }
    if (trace_on()) {
        static unsigned n;
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if ((n++ & 63) == 0)
            fprintf(stderr, "[bigmap] draw %dx%d at %d,%d in %.3f ms (picture %dx%d)\n", tw, th, x0, y0, ms, w, h);
    }
}
