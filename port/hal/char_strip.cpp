/* hal/char_strip.cpp: see char_strip.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "char_strip.h"
#include "char_door_skin.h"
#include "char_registry.h"
#include "char_save.h"
#include "mods.h"

extern "C" {
extern unsigned char data_0209caa0[];
extern signed char data_02092110;
int SublevelToLevel(int);
void *port_scene_live_object(void);
int  port_scene_live_id(void);
int  hal_menu_starsel_up(void);
int  hal_menu_enabled(int key);
int  hal_menu_live(void);
void host_menu_cursor_sound(void);
}

namespace {

const char *const ROM_NAME[4] = { "Mario", "Luigi", "Wario", "Yoshi" };
enum { MENU_KEY_ONE_SCREEN = 2 };          /* hal/menu_compose.h MENU_KEY_ONE_SCREEN_MENUS */
enum { PER_PAGE = 3, MAX_SLOTS = 8 };

/* what the picture is showing now */
int g_up;                  /* 1 while the strip is part of the screen */
int g_frames;              /* strip frames since the star select came up */
int g_pw = 512, g_ph = 384;

/* the characters and the pages */
int g_rom[4], g_nrom;      /* ROM character ids the save has */
int g_ncust;
int g_npages = 1;
int g_page;
CdItem g_slot[MAX_SLOTS];
int g_nslots;
int g_built;               /* the lists exist for this star select */

/* the cursor */
int g_focus;               /* 1 while the cursor is on the strip */
int g_cursor;              /* slot under the cursor (when g_focus) */
int g_hover = -1;
int g_cur = 0;             /* the character being played: 0..3, 4 + n */
int g_cur_set;
int g_pick = -1;           /* a pick for the frame loop to apply */
int g_pending = -1;        /* a pick whose skin differs from this process's: restart at the act */
unsigned g_prev_keys;
int g_prev_mouse_l;
int g_prev_mx = -1, g_prev_my = -1;
int g_last_hover = -1;
int g_hold_key, g_hold_frames;
int g_swallow;             /* the strip took a press: keep every pad bit from the ROM until the pad is released */
int g_begun;               /* the head icons were read for this star select */
int g_ready;               /* every device was seen released once since the strip came up */

unsigned char ob(const void *o, int off) { return ((const unsigned char *)o)[off]; }

int rom_unlocked(int c) { return c == 0 || (*(int *)(data_0209caa0 + 8) & (1 << c)) != 0; }

int current_id(void)
{
    if (g_cur_set) return g_cur;
    const int a = port_char_active();
    return a >= 0 ? 4 + a : (data_0209caa0[0x41] & 3);
}

/* the slots of page p: page 0 is the ROM's characters, the others three costumes each; a last slot (the arrow) when any costume is installed */
void build_page(int p)
{
    g_nslots = 0;
    const int cur = current_id();
    if (p == 0) {
        for (int i = 0; i < g_nrom; ++i) {
            const int c = g_rom[i];
            CdItem &m = g_slot[g_nslots++];
            memset(&m, 0, sizeof m);
            m.id = c; m.base = c; m.current = c == cur;
            snprintf(m.label, sizeof m.label, "%s", ROM_NAME[c]);
        }
    } else {
        for (int i = (p - 1) * PER_PAGE; i < p * PER_PAGE && i < g_ncust; ++i) {
            CdItem &m = g_slot[g_nslots++];
            memset(&m, 0, sizeof m);
            m.id = 4 + i; m.base = port_char_base(i) & 3; m.custom = 1; m.current = 4 + i == cur;
            snprintf(m.label, sizeof m.label, "%s", port_char_name(i));
            snprintf(m.plays, sizeof m.plays, "Plays as %s", ROM_NAME[m.base]);
            m.preview = port_char_preview(i);
            m.icon = port_char_icon(i);
        }
    }
    if (g_ncust > 0) {
        CdItem &m = g_slot[g_nslots++];
        memset(&m, 0, sizeof m);
        m.id = -1;
        snprintf(m.label, sizeof m.label, "More");
    }
}

int page_of(int id) { return id < 4 ? 0 : 1 + (id - 4) / PER_PAGE; }

void log_slots(void)
{
    for (int i = 0; i < g_nslots; ++i)
        fprintf(stderr, "[strip] slot %d: %s%s\n", i, g_slot[i].id < 0 ? "(arrow)" : g_slot[i].label, g_slot[i].current ? " (current)" : "");
}

void build_lists(void)
{
    g_nrom = 0;
    for (int c = 0; c < 4; ++c)
        if (rom_unlocked(c)) g_rom[g_nrom++] = c;
    g_ncust = port_char_count();
    if (g_ncust > 16) g_ncust = 16;
    g_npages = 1 + (g_ncust + PER_PAGE - 1) / PER_PAGE;
    g_page = page_of(current_id());
    if (g_page >= g_npages) g_page = 0;
    build_page(g_page);
    g_built = 1;
    fprintf(stderr, "[strip] built: %d ROM character(s), %d custom, %d page(s); page %d, %d slot(s), current %d\n", g_nrom, g_ncust, g_npages, g_page, g_nslots, current_id());
    log_slots();
}

/* does the star select want a strip at all: an answer from the ROM's own object and the layout */
int wanted(void)
{
    if (!hal_menu_starsel_up()) return 0;
    if (!hal_menu_enabled(MENU_KEY_ONE_SCREEN)) return 0;
    if (!hal_menu_live()) return 0;
    if (port_mods_multiplayer()) return 0;
    const unsigned char *o = (const unsigned char *)port_scene_live_object();
    if (!o) return 0;
    if (SublevelToLevel(data_02092110) > 0xe) return 0;        /* Bowser and the secret courses: the ROM's star-less screen */
    if (ob(o, 0x133) == 1) return 0;                           /* the multiplayer character row (VS) */
    return 1;
}

CsGeom geom(void)
{
    CsGeom g;
    cdskin_strip_geom(g_pw, g_ph, g_nslots, &g);
    return g;
}

int slot_at(const CsGeom &g, int x, int y)
{
    for (int i = 0; i < g.n; ++i) {
        const int sx = g.x0 + i * g.pitch;
        if (x >= sx && x < sx + g.sw && y >= g.y0 - (int)(2 * g.k) && y < g.y0 + g.sh) return i;
    }
    return -1;
}

void move_cursor(int to)
{
    if (to < 0 || to >= g_nslots || to == g_cursor) return;
    g_cursor = to;
    host_menu_cursor_sound();
    fprintf(stderr, "[strip] cursor %d: %s\n", g_cursor, g_slot[g_cursor].id < 0 ? "(arrow)" : g_slot[g_cursor].label);
}

/* 1 when the slot was a pick */
int activate(int i)
{
    if (i < 0 || i >= g_nslots) return 0;
    if (g_slot[i].id < 0) {
        g_page = (g_page + 1) % g_npages;
        build_page(g_page);
        g_cursor = g_nslots - 1;      /* the arrow: A again goes on */
        host_menu_cursor_sound();
        fprintf(stderr, "[strip] arrow: page %d of %d, %d slot(s)\n", g_page, g_npages, g_nslots);
        log_slots();
        return 0;
    }
    g_pick = g_slot[i].id;
    g_cur = g_pick;
    g_cur_set = 1;
    build_page(g_page);
    fprintf(stderr, "[strip] picked slot %d: %s (id %d)\n", i, g_slot[i].label, g_pick);
    return 1;
}

#ifndef PORT_ROM_CLEAN
/* SM64DS_STRIP_MOUSE="x,y@f0-f1;...": the pointer stands at picture pixel (x, y) from strip frame f0 to f1 + 2 and the left button is down on frames
   f0 + 1 .. f1. Proof rows only: a scripted run has no focused window to read a pointer from. */
void scripted_pointer(SpInput *si, int frame)
{
    static const char *env = (const char *)1;
    if (env == (const char *)1) {
        env = getenv("SM64DS_STRIP_MOUSE");
        if (env) fprintf(stderr, "[strip] scripted pointer: SM64DS_STRIP_MOUSE=%s\n", env);
    }
    if (!env) return;
    const char *s = env;
    while (*s) {
        int x = 0, y = 0, f0 = 0, f1 = 0, got = 0;
        if (sscanf(s, "%d,%d@%d-%d%n", &x, &y, &f0, &f1, &got) >= 4) {
            if (frame >= f0 && frame <= f1 + 2) {
                si->mouse_valid = 1; si->mouse_x = x; si->mouse_y = y; si->mouse_l = frame >= f0 + 1 && frame <= f1;
            }
            s += got;
        }
        while (*s && *s != ';') ++s;
        if (*s == ';') ++s;
    }
}

/* SM64DS_STRIP_BMP="path@frame": the picture the strip was just drawn on, written at that strip frame (the presented picture of the one-screen
   layout, host overlays included). Several frames: SM64DS_STRIP_BMP="path%d@f1,f2" is not supported; one file per frame is "path@f" run once. */
void capture(const uint32_t *px, int stride, int w, int h)
{
    static const char *env = (const char *)1;
    static char path[512];
    static int at = -1, done;
    if (env == (const char *)1) {
        env = getenv("SM64DS_STRIP_BMP");
        if (env) {
            const char *a = strrchr(env, '@');
            const size_t n = a ? (size_t)(a - env) : strlen(env);
            if (n < sizeof path) { memcpy(path, env, n); path[n] = 0; at = a ? atoi(a + 1) : 0; }
        }
    }
    if (!env || done || at < 0 || g_frames < at) return;
    done = 1;
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "[strip] could not write %s\n", path); return; }
    const int rowb = w * 3, pad = (4 - (rowb & 3)) & 3;
    unsigned char hdr[54];
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B'; hdr[1] = 'M';
    const unsigned sz = 54 + (unsigned)(rowb + pad) * (unsigned)h;
    const unsigned off = 54, dib = 40;
    memcpy(hdr + 2, &sz, 4);
    memcpy(hdr + 10, &off, 4);
    memcpy(hdr + 14, &dib, 4);
    memcpy(hdr + 18, &w, 4);
    memcpy(hdr + 22, &h, 4);
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    std::vector<unsigned char> row((size_t)(rowb + pad), 0);
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            const uint32_t c = px[(size_t)y * stride + x];
            row[(size_t)x * 3] = (unsigned char)(c & 255);
            row[(size_t)x * 3 + 1] = (unsigned char)((c >> 8) & 255);
            row[(size_t)x * 3 + 2] = (unsigned char)((c >> 16) & 255);
        }
        fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f);
    fprintf(stderr, "[strip] wrote %s at strip frame %d: the picture with the strip on it, %dx%d\n", path, g_frames, w, h);
}
#else
void scripted_pointer(SpInput *, int) {}
void capture(const uint32_t *, int, int, int) {}
#endif

void forget(void)
{
    g_up = 0; g_frames = 0; g_built = 0; g_focus = 0; g_hover = -1; g_pick = -1; g_swallow = 0; g_ready = 0; g_begun = 0;
    g_cur_set = 0;
    g_prev_keys = 0; g_prev_mouse_l = 0; g_hold_key = g_hold_frames = 0; g_last_hover = -1;
}

}  // namespace

extern "C" {

int port_char_strip_up(void) { return g_up; }
int port_char_strip_slots(void) { return g_nslots; }
int port_char_strip_page(void) { return g_page; }
int port_char_strip_current(void) { return current_id(); }
void port_char_strip_set_current(int id) { g_cur = id; g_cur_set = 1; if (g_built) build_page(g_page); }
int port_char_strip_skin_pending(void) { return g_pending; }
void port_char_strip_reset(void) { forget(); g_pending = -1; }

int port_char_strip_take_pick(void)
{
    const int r = g_pick;
    g_pick = -1;
    return r;
}

/* the frame loop tells the strip what its pick did: pending = a skin change waits for the act (id), else -1 */
void port_char_strip_set_pending(int id) { g_pending = id; }

unsigned port_char_strip_frame(const struct SpInput *in0)
{
    if (!wanted()) {
        if (g_up || g_built) fprintf(stderr, "[strip] gone\n");
        forget();                   /* g_pending survives the scene: the restart at the act reads it */
        return 0;
    }
    if (!g_built) {
        g_pending = -1;
        build_lists();
    }
    g_up = 0;
    if (g_nrom + g_ncust <= 1) return 0;    /* nothing to choose: one character and no costume */
    const unsigned char *o = (const unsigned char *)port_scene_live_object();
    ++g_frames;
    if (ob(o, 0x139) != 0) {                 /* the act is chosen and the screen is leaving: the strip goes with it */
        g_focus = 0;
        return 0;
    }
    g_up = 1;
    SpInput sin = *in0;
    scripted_pointer(&sin, g_frames);
    const SpInput *in = &sin;
    const unsigned k = in->keys & (SPK_UP | SPK_DOWN | SPK_LEFT | SPK_RIGHT | SPK_ACCEPT | SPK_BACK);
    const unsigned edge = k & ~g_prev_keys;
    int mclick = 0;
    if (in->mouse_valid && in->mouse_l && !g_prev_mouse_l) mclick = 1;
    const unsigned hide = 0x0080;             /* Down is the strip's, for every character (the ROM's Yoshi row never opens) */
    if (!g_ready) {
        if (k == 0 && !in->mouse_l) g_ready = 1;
        g_prev_keys = k;
        g_prev_mouse_l = in->mouse_l ? 1 : 0;
        return hide;
    }
    unsigned act = edge;
    {   /* repeat on left / right while held */
        const unsigned lr = k & (SPK_LEFT | SPK_RIGHT);
        if (lr && lr == (unsigned)g_hold_key) {
            ++g_hold_frames;
            if (g_hold_frames > 18 && (g_hold_frames % 4) == 0) act |= lr;
        } else {
            g_hold_key = (int)lr;
            g_hold_frames = 0;
        }
    }
    const CsGeom gm = geom();
    if (!g_focus) {
        if (act & SPK_DOWN) {
            g_focus = 1;
            g_cursor = 0;
            for (int i = 0; i < g_nslots; ++i) if (g_slot[i].current) g_cursor = i;
            host_menu_cursor_sound();
            fprintf(stderr, "[strip] cursor on the strip: %d: %s\n", g_cursor, g_slot[g_cursor].id < 0 ? "(arrow)" : g_slot[g_cursor].label);
        }
    } else {
        if (act & SPK_LEFT) move_cursor(g_cursor - 1);
        if (act & SPK_RIGHT) move_cursor(g_cursor + 1);
        if (act & SPK_ACCEPT) {
            if (activate(g_cursor)) { g_focus = 0; g_swallow = 1; }      /* a pick gives the cursor back to the stars */
        }
        if (act & (SPK_UP | SPK_BACK)) { g_focus = 0; g_swallow = 1; fprintf(stderr, "[strip] cursor back on the stars\n"); }
    }
    g_hover = -1;
    if (in->mouse_valid) {
        const int mx = in->mouse_x, my = in->mouse_y;
        const int s = slot_at(gm, mx, my);
        g_hover = s;
        if (s != g_last_hover && (mx != g_prev_mx || my != g_prev_my)) {
            g_last_hover = s;
            if (s >= 0) { host_menu_cursor_sound(); fprintf(stderr, "[strip] hover slot %d\n", s); }
        }
        if (mclick && s >= 0) {
            g_cursor = s;
            activate(s);
            g_swallow = 1;
        }
        g_prev_mx = mx;
        g_prev_my = my;
    }
    g_prev_keys = k;
    g_prev_mouse_l = in->mouse_l ? 1 : 0;
    if (g_focus) return 0xFFFF;
    if (g_swallow) {
        if (k == 0 && !in->mouse_l) g_swallow = 0;
        return 0xFFFF;
    }
    return hide;
}

void port_char_strip_idle(void)
{
    if (g_up || g_built) { fprintf(stderr, "[strip] gone
"); forget(); }
}

void port_char_strip_draw(uint32_t *px, int stride, int w, int h)
{
    if (!g_up || !px) return;
    g_pw = w;
    g_ph = h;
    if (!g_begun) { g_begun = 1; cdskin_begin(); }
    const CsGeom gm = geom();
    cdskin_strip_draw(px, stride, w, h, &gm, g_slot, g_nslots, g_focus ? g_cursor : -1, g_hover);
    capture(px, stride, w, h);
}

}  // extern "C"
