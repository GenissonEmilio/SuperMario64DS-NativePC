/* hal/char_strip.cpp: see char_strip.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <chrono>
#include <math.h>

#include "char_strip.h"
#include "char_door_skin.h"
#include "char_registry.h"
#include "char_save.h"
#include "host_settings.h"
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
unsigned int _ZN5Sound6Play2DEjj(unsigned int bank, unsigned int id);
unsigned int func_02012790(unsigned int id);
}
void sd_seq_skip_lead_rest_arm(int ms);   /* hal/sdat/sdat.h (C++ linkage) */

/* SNDCAM1 (S4): the select sound and the voice a character gives when picked. The voice is the clip the ROM plays when that character is
   unlocked: Mario / Luigi / Wario are the player bank's sounds 0xd9 / 0xda / 0xdb (the waves NCS_WAVE_SE_VOICE_MARIO 0, _LUIGI 9, _WARIO 7,
   Player::St_* character change, src/game/player/Player.cpp: PlayBank0), Yoshi is system sound 0xd8 (NCS_WAVE_SE_VOICE_YOSSY_MG 8). */
extern "C" void port_char_strip_pick_sound(int ch)
{
    /* CAMVOICE1 (C2): Mario / Luigi / Wario's clips open with a 96 tick rest (the cartridge plays them after the unlock jingle), so on the pick they
       came a second behind the select sound while Yoshi's, which has none, came with it. The voice goes first and its leading rest is skipped
       (sd_seq_skip_lead_rest_arm, one use, lapses in 400 ms); the select sound follows. */
    switch (ch & 3) {
    case 0: sd_seq_skip_lead_rest_arm(400); _ZN5Sound6Play2DEjj(0, 0xd9); break;
    case 1: sd_seq_skip_lead_rest_arm(400); _ZN5Sound6Play2DEjj(0, 0xda); break;
    case 2: sd_seq_skip_lead_rest_arm(400); _ZN5Sound6Play2DEjj(0, 0xdb); break;
    default: func_02012790(0xd8); break;
    }
    func_02012790(0x12e);
}

namespace {

const char *const ROM_NAME[4] = { "Mario", "Luigi", "Wario", "Yoshi" };
enum { MENU_KEY_ONE_SCREEN = 2 };          /* hal/menu_compose.h MENU_KEY_ONE_SCREEN_MENUS */
enum { PER_PAGE = 3, MAX_SLOTS = 24 };

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
int g_open;                /* custom characters are open for this strip (unlock flag or the from-the-start setting) */
int g_begun;               /* the head icons were read for this star select */
int g_ready;               /* every device was seen released once since the strip came up */

unsigned char ob(const void *o, int off) { return ((const unsigned char *)o)[off]; }

/* CHARLOCK1: the ROM characters this save file has. The cartridge keeps them in the record's flags2 word (data_0209caa0 + 8): bit 0 Mario,
   1 Luigi, 2 Wario, set at their doors; SaveData::IsCharacterUnlocked (src/engine/save/SaveData.cpp) is flags2 & (1 << c) with no exception, and
   SaveData::SetDefaultValues starts every file with Yoshi only (flags2 |= 8, character byte 3). Mario was listed always here, so a new file
   offered him. Yoshi is always listed: the star select's own row never tests his bit (d_s_star_sel_methods.cpp tests i < 3). */
int rom_unlocked(int c)
{
    if (c == 3 || (*(int *)(data_0209caa0 + 8) & (1 << c)) != 0) return 1;
#ifndef PORT_ROM_CLEAN
    /* SM64DS_STRIP_UNLOCK=<mask>: proof runs only, the characters (bit n) the strip lists although the save has not got them yet; the ROM's save is not touched */
    static int extra = -1;
    if (extra < 0) { const char *e = getenv("SM64DS_STRIP_UNLOCK"); extra = e ? (int)strtol(e, 0, 0) : 0; }
    if (extra & (1 << c)) return 1;
#endif
    return 0;
}

int current_id(void)
{
    if (g_cur_set) return g_cur;
    const int a = port_char_active();
    return a >= 0 ? 4 + a : (data_0209caa0[0x41] & 3);
}

/* STARSEL2: the panel lists every character in one list (the ROM characters the save has, then every open custom character), four to a
   column; there are no pages and no arrow slot any more. p is kept for the old callers and ignored. */
void build_page(int p)
{
    (void)p;
    g_nslots = 0;
    const int cur = current_id();
    for (int i = 0; i < g_nrom && g_nslots < MAX_SLOTS; ++i) {
        const int c = g_rom[i];
        CdItem &m = g_slot[g_nslots++];
        memset(&m, 0, sizeof m);
        m.id = c; m.base = c; m.current = c == cur;
        snprintf(m.label, sizeof m.label, "%s", ROM_NAME[c]);
    }
    for (int i = 0; i < g_ncust && g_nslots < MAX_SLOTS; ++i) {
        CdItem &m = g_slot[g_nslots++];
        memset(&m, 0, sizeof m);
        m.id = 4 + i; m.base = port_char_base(i) & 3; m.custom = 1; m.current = 4 + i == cur;
        snprintf(m.label, sizeof m.label, "%s", port_char_name(i));
        snprintf(m.plays, sizeof m.plays, "Plays as %s", ROM_NAME[m.base]);
        m.preview = port_char_preview(i);
        m.icon = port_char_icon(i);
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
    /* CHARSET1: the arrow and the costumes behind it are earned at the white door (this save slot's unlock flag), or open from the start when
       the CharactersUnlockedFromStart setting is on */
    g_open = host_setting_chars_from_start() || port_char_save_unlocked(port_char_save_slot());
    g_ncust = g_open ? port_char_count() : 0;
    if (g_ncust > 16) g_ncust = 16;
    g_npages = 1;
    g_page = 0;
    build_page(g_page);
    g_built = 1;
    fprintf(stderr, "[strip] built: %d ROM character(s), %d custom%s, %d page(s); page %d, %d slot(s), current %d\n", g_nrom, g_ncust, g_open ? "" : (port_char_count() > 0 ? " (locked: no unlock yet)" : ""), g_npages, g_page, g_nslots, current_id());
    log_slots();
}

/* does the star select want a strip at all: an answer from the ROM's own object and the layout */
int wanted(void)
{
    static int why = -1;
    int w = 0;
    const unsigned char *o = 0;
    if (!hal_menu_starsel_up()) w = 1;
    else if (!hal_menu_enabled(MENU_KEY_ONE_SCREEN)) w = 2;
    else if (!hal_menu_live()) w = 3;
    else if (port_mods_multiplayer()) w = 4;
    else if (!(o = (const unsigned char *)port_scene_live_object())) w = 5;
    else if (SublevelToLevel(data_02092110) > 0xe) w = 6;        /* Bowser and the secret courses: the ROM's star-less screen */
    else if (ob(o, 0x133) == 1) w = 7;                           /* the multiplayer character row (VS) */
    else if (!host_setting_character_strip()) w = 8;             /* CAPSTOGGLE1: the CharacterStrip setting is off: the DS's own caps row, no strip */
    if (w != why && w != 1) { why = w; fprintf(stderr, "[strip] wanted=%d (0 yes, 2 one-screen menus off, 3 no recipe live, 4 multiplayer, 5 no scene, 6 course has no star row, 7 VS row, 8 CharacterStrip setting off)\n", w); }
    return w == 0;
}

/* STARSEL2: THE SLIDE. 0 closed (only the tab on the right edge), 1 fully out. It is a function of the wall clock (an eased move from where
   it was when the target last changed), so the picture side reads the same value on every Smooth Motion picture between two ticks and the
   panel glides at the picture rate; with Smooth Motion off it steps once a tick along the same curve. */
const double kSlideMs = 220.0;
double g_sl_from, g_sl_to, g_sl_t0;
int g_mouse_open;          /* the pointer opened it (it closes when the pointer leaves) */
int g_tab_hover;           /* the pointer is on the tab */

#ifndef PORT_ROM_CLEAN
/* SM64DS_CHARPANEL_CLOCK_MS=<ms>: proof rows only. The slide's clock is a picture count (each picture the panel is drawn on is <ms> later
   than the one before), so a picture dump, which slows the game far below the display rate, still shows the slide picture by picture as a
   display at 1000 / <ms> pictures a second would. */
double g_fake_step = -1.0, g_fake_now = 1000.0;
#endif
double now_ms(void)
{
#ifndef PORT_ROM_CLEAN
    if (g_fake_step < 0.0) { const char *e = getenv("SM64DS_CHARPANEL_CLOCK_MS"); g_fake_step = e ? atof(e) : 0.0; }
    if (g_fake_step > 0.0) return g_fake_now;
#endif
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

double slide_at(double t)
{
    const double from = g_sl_from, to = g_sl_to, t0 = g_sl_t0;
    if (to == from) return to;
    double p = (t - t0) / (kSlideMs * fabs(to - from));
    if (p >= 1.0) return to;
    if (p < 0.0) p = 0.0;
    const double q = 1.0 - p;
    return from + (to - from) * (1.0 - q * q * q);
}

void slide_to(double target, const char *why)
{
    if (target == g_sl_to) return;
    const double t = now_ms();
    g_sl_from = slide_at(t);
    g_sl_t0 = t;
    g_sl_to = target;
    fprintf(stderr, "[charpanel] %s (%s), from %.2f\n", target > 0.5 ? "opens" : "closes", why, g_sl_from);
}

void slide_snap(double v) { g_sl_from = g_sl_to = v; g_sl_t0 = now_ms(); }

CpGeom geom_at(double open)
{
    CpGeom g;
    cdskin_panel_geom(g_pw, g_ph, g_nslots, (float)open, &g);
    return g;
}

CpGeom geom(void) { return geom_at(slide_at(now_ms())); }

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
    g_pick = g_slot[i].id;
    port_char_strip_pick_sound(g_slot[i].base & 3);
    {   /* the star select's own life counter (its icon index, +0x116) follows the pick, as it does when the screen is entered as that character */
        unsigned char *o = (unsigned char *)port_scene_live_object();
        if (o) o[0x116] = (unsigned char)(g_slot[i].base & 3);
    }
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
    g_mouse_open = 0; g_tab_hover = 0; slide_snap(0.0);
    g_cur_set = 0;
    g_prev_keys = 0; g_prev_mouse_l = 0; g_hold_key = g_hold_frames = 0; g_last_hover = -1;
}

}  // namespace

extern "C" {

int port_char_strip_up(void) { return g_up; }
int port_char_strip_focus(void) { return g_up && g_focus; }
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
        if (!host_setting_character_strip()) g_pending = -1;     /* CAPSTOGGLE1: turned off while a costume pick waited: the DS screen restarts nothing */
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
    if (ob(o, 0x139) != 0) {                 /* the act is chosen and the screen is leaving: the panel goes with it */
        g_focus = 0;
        g_mouse_open = 0;
        slide_snap(0.0);
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
    {   /* repeat on the d-pad while held */
        const unsigned lr = k & (SPK_LEFT | SPK_RIGHT | SPK_UP | SPK_DOWN);
        if (lr && lr == (unsigned)g_hold_key) {
            ++g_hold_frames;
            if (g_hold_frames > 18 && (g_hold_frames % 4) == 0) act |= lr;
        } else {
            g_hold_key = (int)lr;
            g_hold_frames = 0;
        }
    }
    /* STARSEL2: THE PANEL'S KEYS. Right on the last star the ROM can select moves onto the tab: the panel slides out and the cursor is on the
       character being played. In the panel Up / Down walk a column, Right goes to the next column, Left to the one before; Left on the first
       column, B or Escape close it and give the cursor back to the stars; A, Enter or Space pick (the pick closes it too). */
    if (!g_focus) {
        if ((act & SPK_RIGHT) && ob(o, 0x133) == 0) {
            const int nst = ob(o, 0x114), cur = ob(o, 0x115), mask = ob(o, 0x131);
            int last = -1;
            for (int i = 0; i < nst && i < 8; ++i) if ((mask >> i) & 1) last = i;
            if (cur == last) {
                g_focus = 1;
                g_cursor = 0;
                for (int i = 0; i < g_nslots; ++i) if (g_slot[i].current) g_cursor = i;
                g_swallow = 1;
                slide_to(1.0, "pad: right past the last star");
                host_menu_cursor_sound();
                fprintf(stderr, "[strip] cursor on the panel: %d: %s\n", g_cursor, g_slot[g_cursor].label);
            }
        }
    } else {
        const int col = g_cursor / 4, row = g_cursor % 4, cols = (g_nslots + 3) / 4;
        if (act & SPK_UP) { if (row > 0) move_cursor(g_cursor - 1); }
        else if (act & SPK_DOWN) { if (row < 3 && g_cursor + 1 < g_nslots) move_cursor(g_cursor + 1); }
        else if (act & SPK_RIGHT) {
            if (col + 1 < cols) move_cursor(g_cursor + 4 < g_nslots ? g_cursor + 4 : g_nslots - 1);
        } else if (act & SPK_LEFT) {
            if (col > 0) move_cursor(g_cursor - 4);
            else { g_focus = 0; g_swallow = 1; if (!g_mouse_open) slide_to(0.0, "pad: left out of the panel"); fprintf(stderr, "[strip] cursor back on the stars\n"); }
        }
        if (g_focus && (act & SPK_ACCEPT)) {
            if (activate(g_cursor)) { g_focus = 0; g_swallow = 1; g_mouse_open = 0; slide_to(0.0, "pad: picked"); }
        }
        if (g_focus && (act & SPK_BACK)) { g_focus = 0; g_swallow = 1; if (!g_mouse_open) slide_to(0.0, "pad: back"); fprintf(stderr, "[strip] cursor back on the stars\n"); }
    }
    /* THE POINTER. On the tab or the panel: it slides out (and stays while the pointer is on it); off both: a panel the pointer opened slides
       back. A click on a head picks it (the panel stays out while the pointer is still on it); a click on the tab opens or closes it, which is
       also what a stylus tap does (a touch screen has no hover). */
    g_hover = -1;
    g_tab_hover = 0;
    if (in->mouse_valid) {
        const int mx = in->mouse_x, my = in->mouse_y;
        const CpGeom gm = geom();
        const bool open = g_sl_to > 0.5;
        const int s = open ? cdskin_panel_slot_at(&gm, mx, my) : -1;
        const bool on_tab = mx >= gm.tx && mx < gm.tx + gm.tw && my >= gm.ty && my < gm.ty + gm.th;
        const bool inside = cdskin_panel_hit(&gm, mx, my) != 0;
        const bool moved = mx != g_prev_mx || my != g_prev_my;
        g_hover = s;
        g_tab_hover = on_tab ? 1 : 0;
        if (s != g_last_hover && moved) {
            g_last_hover = s;
            if (s >= 0) { host_menu_cursor_sound(); fprintf(stderr, "[strip] hover slot %d: %s\n", s, g_slot[s].label); }
        }
        if (mclick && s >= 0) {
            g_cursor = s;
            activate(s);
            g_focus = 0;
            g_swallow = 1;
        } else if (mclick && on_tab) {
            g_swallow = 1;
            if (open) { g_mouse_open = 0; g_focus = 0; slide_to(0.0, "pointer: tab clicked"); }
            else { g_mouse_open = 1; slide_to(1.0, "pointer: tab clicked"); }
        } else if (moved) {
            if (inside && !open) { g_mouse_open = 1; slide_to(1.0, "pointer: on the tab"); }
            else if (!inside && open && g_mouse_open && !g_focus) { g_mouse_open = 0; slide_to(0.0, "pointer: left the panel"); }
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
    if (g_up || g_built) { fprintf(stderr, "[strip] gone\n"); forget(); }
}

void port_char_strip_draw(uint32_t *px, int stride, int w, int h)
{
    /* STARSEL2: the panel is a piece of the star select's one-screen recipe (port_char_strip_panel_draw, every picture); this per-tick
       host-UI hook only writes the proof picture now (SM64DS_STRIP_BMP), with or without a panel. */
    if (!px || !hal_menu_starsel_up()) return;
    if (!g_up) {
        static int off_frames;
        const int sv = g_frames;
        g_frames = ++off_frames;
        capture(px, stride, w, h);
        g_frames = sv;
        return;
    }
    g_pw = w;
    g_ph = h;
    capture(px, stride, w, h);
}

/* STARSEL2: the panel and its tab on a picture of the star select (the recipe's custom piece calls this for every picture it draws) */
void port_char_strip_panel_draw(uint32_t *px, int stride, int w, int h)
{
    if (!g_up || !px || w <= 0 || h <= 0) return;
    g_pw = w;
    g_ph = h;
    if (!g_begun) { g_begun = 1; cdskin_begin(); }
#ifndef PORT_ROM_CLEAN
    if (g_fake_step > 0.0) g_fake_now += g_fake_step;
#endif
    const double t = now_ms(), open = slide_at(t);
    const CpGeom gm = geom_at(open);
#ifndef PORT_ROM_CLEAN
    {   /* SM64DS_CHARPANEL_TRACE=1: one line per picture the panel is drawn on while it moves (proof rows: the slide steps every picture) */
        static const bool tr = getenv("SM64DS_CHARPANEL_TRACE") != 0;
        static double last = -1.0;
        if (tr && open != last) { fprintf(stderr, "[charpanel] pic t %.1f open %.4f panel x %d tab x %d\n", t, open, gm.px, gm.tx); last = open; }
    }
#endif
    cdskin_panel_draw(px, stride, w, h, &gm, g_slot, g_nslots, g_focus ? g_cursor : -1, g_hover, g_tab_hover || g_focus);
}

}  // extern "C"
