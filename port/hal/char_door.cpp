/* hal/char_door.cpp: see char_door.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "char_door.h"
#include "char_door_skin.h"
#include "char_registry.h"
#include "char_save.h"
#include "host_settings.h"
#include "mods.h"

extern "C" {
extern unsigned char data_0209caa0[];
/* the door callback seam, defined in unmatched/Door_Behavior.cpp (null on targets that never seat it) */
extern int (*port_char_door_idle_hook)(void *door, void *player);
/* hal/settings_page_hooks.cpp: the stylus filter asks whether the picker is up */
extern int (*port_char_door_touch_open)(void);
/* ROM bodies the hook reads (all matched src, C names as src/game/player/Player.cpp uses them) */
int _ZN6Player7IsStateERNS_5StateE(void *self, void *state);
extern int data_ov002_0211013c[];
extern int data_ov002_02110154[];
extern int data_ov002_0211043c[];
unsigned char NumStars(void);
int _ZN8daDoor_c19func_ov100_021452e4EP6Player(char *door, char *player);   /* the door's own "player is in front of me" test */
}


namespace {

const char *const ROM_NAME[4] = { "Mario", "Luigi", "Wario", "Yoshi" };

enum { CD_NONE = 0, CD_BOX = 1, CD_PICK = 2 };

int g_want;            /* a request: CD_BOX or CD_PICK, honoured by the frame loop */
int g_mode;            /* CD_NONE while closed */
/* After a picker or box closes (or a restart put the player in the doorway) the door stays shut until the player has walked
   away from where they stood: the spot is taken on the next hook call (anchor pending), and the picker may open again once
   they are more than STEP_AWAY units from it. A position test, not the doorway test: a character swap upsets the player's
   state for a while, and that must not look like having left. */
int g_anchor_pending;
int g_anchored;
int g_anchor_x, g_anchor_z;
enum { STEP_AWAY = 120 };
void disarm_all(void)
{
    g_anchor_pending = 1;
    g_anchored = 0;
}
int g_pick = -1;
CdItem g_item[16];
int g_n;
int g_sel;
int g_top;             /* first visible row */
int g_pw = 512, g_ph = 384, g_adv = 8, g_line = 10;
unsigned g_prev_keys;
int g_prev_mouse_l;
int g_prev_mouse_r;
int g_prev_mx = -1, g_prev_my = -1;
int g_hot_back, g_hot_ok;   /* the pointer is on the Back plank / the box's OK plank */
int g_hold_key;
int g_hold_frames;
int g_input_ready;     /* every device was released once since it opened: the press that brought the player here is not an answer */
int g_cur = 0;
int g_frames;
int g_saw_closed;      /* this session saw the door still shut (the ROM's own first pass is ahead of us, or under way) */
int g_star0;           /* the star count while it was shut: the ROM's star has been given once the count is above it */

enum { MAX_ROWS = 5 };   /* visible list rows (the layout shows five) */

/* CHARLOCK1: flags2 bit c (SaveData::IsCharacterUnlocked); Yoshi always, the character every file starts with (SetDefaultValues) */
int rom_char_unlocked(int c)
{
    return c == 3 || (*(int *)(data_0209caa0 + 8) & (1 << c)) != 0;
}

void build_list(int cur)
{
    g_n = 0;
    for (int c = 0; c < 4; ++c) {
        if (!rom_char_unlocked(c)) continue;      /* the ROM's own screens show only the characters this save has; a locked one is not listed */
        CdItem &m = g_item[g_n];
        memset(&m, 0, sizeof m);
        m.id = c;
        m.base = c;
        m.current = c == cur;
        snprintf(m.label, sizeof m.label, "%s", ROM_NAME[c]);
        ++g_n;
    }
    for (int i = 0; i < port_char_count() && g_n < 16; ++i) {
        CdItem &m = g_item[g_n];
        memset(&m, 0, sizeof m);
        m.id = 4 + i;
        m.base = port_char_base(i) & 3;
        m.custom = 1;
        m.current = 4 + i == cur;
        snprintf(m.label, sizeof m.label, "%s", port_char_name(i));
        snprintf(m.plays, sizeof m.plays, "Plays as %s", ROM_NAME[m.base]);
        snprintf(m.credits, sizeof m.credits, "%s", port_char_credits(i));
        m.preview = port_char_preview(i);
        m.icon = port_char_icon(i);
        ++g_n;
    }
    g_sel = 0;
    for (int i = 0; i < g_n; ++i)
        if (g_item[i].id == cur) g_sel = i;
    g_top = 0;
    if (g_sel >= MAX_ROWS) g_top = g_sel - MAX_ROWS + 1;
}

const char *box_text(void) { return "Custom character select unlocked"; }

CdGeom geom(void)
{
    CdGeom g;
    cdskin_geom(g_pw, g_ph, g_n, g_mode == CD_BOX ? CDS_BOX : CDS_PICK, &g);
    return g;
}

bool inside(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }

void close_now(const char *why)
{
    fprintf(stderr, "[chardoor] %s closed (%s)\n", g_mode == CD_BOX ? "box" : "picker", why);
    g_mode = CD_NONE;
    disarm_all();          /* stay shut until the player leaves the doorway */
}

/* The ROM menu cursor sound: the shared host_menu_cursor_sound() (hal/menu_compose.h, EXTRAS093) asks for it and the frame loop's flush
   plays it once per frame, so a pad move and a mouse hover in one frame make one sound. */
extern "C" void host_menu_cursor_sound(void);
int g_frame_ctr;  // g_frames is declared further down
void cursor_sound(void)
{
    host_menu_cursor_sound();
}

void cursor_step(int d)
{
    if (g_n <= 0) return;
    g_sel = (g_sel + d + g_n) % g_n;
    cursor_sound();
    if (g_sel < g_top) g_top = g_sel;
    if (g_sel >= g_top + MAX_ROWS) g_top = g_sel - MAX_ROWS + 1;
    fprintf(stderr, "[chardoor] cursor %d: %s\n", g_sel, g_item[g_sel].label);
}

/* what the hook sees: are we allowed to take over this door at all */
int door_hook(void *door, void *pl)
{
    char *d = (char *)door;
    char *p = (char *)pl;
    if (port_mods_multiplayer()) return 0;                         /* the ROM door, in every multiplayer path */
    /* POLISH093: with no character mod enabled there is nothing to pick: the white door is the cartridge's own (its star on the
       first pass, the door walked through after), with no unlock box and no picker */
    if (port_char_count() == 0) return 0;
    if (!p || *(int *)(d + 8) != 0xd) return 0;
    const unsigned flags1 = *(unsigned *)(data_0209caa0 + 4);
    if (!(flags1 & (0x100u << 6))) {                               /* the ROM has not opened it yet: its key, its star */
        g_saw_closed = 1;
        g_star0 = NumStars();
        return 0;
    }
    /* the "opened" flag rises with the key, BEFORE the ROM walks the player through and gives the star: stay out of
       its way until the star count has gone up. A save whose door was opened before this session owes no star. */
    if (g_saw_closed && NumStars() <= g_star0) return 0;
    g_saw_closed = 0;
    if (g_mode != CD_NONE || g_want != 0) return 1;                /* one is up or on its way */
    const int slot = port_char_save_slot();
    /* CHARSET1: with CharactersUnlockedFromStart on, the picker opens at once (no unlock box); the ROM's own star flow ahead of it is unchanged */
    const int first = !port_char_save_unlocked(slot) && !host_setting_chars_from_start();
    /* the box comes as soon as the ROM's star is done (the player stands wherever the ROM left them); the picker only
       when the player walks into the doorway */
    if (!first) {
        const int px = *(int *)(p + 0x5c) >> 12, pz = *(int *)(p + 0x64) >> 12;
        if (g_anchor_pending) { g_anchor_x = px; g_anchor_z = pz; g_anchor_pending = 0; g_anchored = 1; }
        if (g_anchored) {
            const long dx = px - g_anchor_x, dz = pz - g_anchor_z;
            if (dx * dx + dz * dz > (long)STEP_AWAY * STEP_AWAY) g_anchored = 0;
            else return 1;                                          /* just closed: do not let it open the door under them */
        }
        if (!_ZN8daDoor_c19func_ov100_021452e4EP6Player(d, p)) return 0;                   /* not in the doorway: the ROM's own watch */
    }
    /* the walking states the ROM's Player::CanEnterDoor accepts, tested without committing the player to a door state */
    const int walking = _ZN6Player7IsStateERNS_5StateE(p, data_ov002_0211013c) ||
                        _ZN6Player7IsStateERNS_5StateE(p, data_ov002_02110154) ||
                        _ZN6Player7IsStateERNS_5StateE(p, data_ov002_0211043c);
    if (!walking || *(int *)(p + 0x360) != 0) return first ? 0 : 1; /* not ready yet */
    if (first) {
        g_want = CD_BOX;
        fprintf(stderr, "[chardoor] the white door was opened (star given by the ROM): unlocking custom character select on slot %c\n", 'A' + slot);
        port_char_save_set_unlocked(slot, 1);
    } else {
        g_want = CD_PICK;
        fprintf(stderr, "[chardoor] the white door asks for the picker\n");
    }
    return 1;
}

/* SM64DS_CHARMENU_MOUSE="x,y@f0-f1;...": the pointer stands at picture pixel (x, y) from picker frame f0 to f1 + 2 and the left button is down on
   frames f0 + 1 .. f1 (picker frames count from 1 when it opens). Proof rows only: a scripted run has no focused window to read a pointer from.
   SM64DS_CHARMENU_MOUSE_R="f0-f1": the right button is down on those frames. */
#ifndef PORT_ROM_CLEAN
void scripted_pointer(SpInput *si, int frame)
{
    static const char *env = (const char *)1;
    static const char *envr = (const char *)1;
    if (env == (const char *)1) {
        env = getenv("SM64DS_CHARMENU_MOUSE");
        envr = getenv("SM64DS_CHARMENU_MOUSE_R");
        if (env || envr) fprintf(stderr, "[chardoor] scripted pointer: SM64DS_CHARMENU_MOUSE=%s SM64DS_CHARMENU_MOUSE_R=%s\n", env ? env : "-", envr ? envr : "-");
    }
    if (env) {
        const char *s = env;
        while (*s) {
            int x = 0, y = 0, f0 = 0, f1 = 0, got = 0;
            if (sscanf(s, "%d,%d@%d-%d%n", &x, &y, &f0, &f1, &got) >= 4) {
                if (frame >= f0 && frame <= f1 + 2) {
                    si->mouse_valid = 1; si->mouse_x = x; si->mouse_y = y; si->mouse_l = frame >= f0 + 1 && frame <= f1;
                    static int last = -1;
                    const int st = si->mouse_l ? 2 : 1;
                    if (st != last) { last = st; fprintf(stderr, "[chardoor] scripted pointer at (%d,%d) on picker frame %d, button %s\n", x, y, frame, st == 2 ? "down" : "up"); }
                }
                s += got;
            }
            while (*s && *s != ';') ++s;
            if (*s == ';') ++s;
        }
    }
    if (envr) {
        int f0 = 0, f1 = 0;
        if (sscanf(envr, "%d-%d", &f0, &f1) >= 2 && frame >= f0 && frame <= f1) si->mouse_r = 1;
    }
}
#else
void scripted_pointer(SpInput *, int) {}
#endif

int seat(void)
{
    port_char_door_idle_hook = door_hook;
    port_char_door_touch_open = []() -> int { return g_mode != CD_NONE ? 1 : 0; };
    return 1;
}
const int g_seated = seat();

}  // namespace

extern "C" {

int port_char_door_open(void) { return g_mode != CD_NONE; }
int port_char_door_pending(void) { return g_want != 0; }
void port_char_door_clear_request(void) { g_want = 0; }

void port_char_door_set_metrics(int w, int h, int adv, int line)
{
    if (w > 0) g_pw = w;
    if (h > 0) g_ph = h;
    if (adv > 0) g_adv = adv;
    if (line > 0) g_line = line;
}

void port_char_door_open_now(int cur)
{
    g_mode = g_want ? g_want : CD_PICK;
    g_want = 0;
    g_pick = -1;
    g_cur = cur;
    g_prev_keys = ~0u;     /* nothing counts until a release is seen */
    g_prev_mouse_l = 1;
    g_prev_mouse_r = 1;
    g_input_ready = 0;
    g_hold_key = 0;
    g_hold_frames = 0;
    g_frames = 0;
    g_prev_mx = g_prev_my = -1;
    g_hot_back = g_hot_ok = 0;
    cdskin_begin();
    if (g_mode == CD_PICK) build_list(cur);
    fprintf(stderr, "[chardoor] %s open, %d rows, cursor %d\n", g_mode == CD_BOX ? "box" : "picker", g_mode == CD_BOX ? 1 : g_n, g_sel);
    if (g_mode == CD_PICK)
        for (int i = 0; i < g_n; ++i)
            fprintf(stderr, "[chardoor] row %d: %s%s\n", i, g_item[i].label, i == g_sel ? "  <" : "");
}

void port_char_door_disarm(void) { disarm_all(); }

void port_char_door_cancel(void)
{
    if (g_mode != CD_NONE) close_now("pause taken away");
    g_want = 0;
}

int port_char_door_take_pick(void)
{
    const int r = g_pick;
    g_pick = -1;
    return r;
}

int port_char_door_rows(void) { return g_mode == CD_BOX ? 1 : g_n; }
int port_char_door_cursor(void) { return g_sel; }

int port_char_door_frame(const struct SpInput *in0)
{
    if (g_mode == CD_NONE) return 0;
    ++g_frames;
    g_frame_ctr = g_frames;
    SpInput sin = *in0;
    scripted_pointer(&sin, g_frames);
    const SpInput *in = &sin;
    const unsigned k = in->keys & (SPK_UP | SPK_DOWN | SPK_LEFT | SPK_RIGHT | SPK_ACCEPT | SPK_BACK);
    const unsigned edge = k & ~g_prev_keys;
    int mclick = 0;
    if (in->mouse_valid) {
        if (in->mouse_l && !g_prev_mouse_l) mclick = 1;
    }
    if (!g_input_ready) {
        /* wait for every device to be seen up once */
        if (k == 0 && !in->mouse_l) { g_input_ready = 1; fprintf(stderr, "[chardoor] devices up, input ready on picker frame %d\n", g_frames); }
        g_prev_keys = k;
        g_prev_mouse_l = in->mouse_l ? 1 : 0;
        g_prev_mouse_r = in->mouse_r ? 1 : 0;
        return 1;
    }
    unsigned act = edge;
    /* key repeat on up / down */
    {
        const unsigned ud = k & (SPK_UP | SPK_DOWN);
        if (ud && ud == (unsigned)g_hold_key) {
            ++g_hold_frames;
            if (g_hold_frames > 18 && (g_hold_frames % 4) == 0) act |= ud;
        } else {
            g_hold_key = (int)ud;
            g_hold_frames = 0;
        }
    }
    int accept = 0, back = 0;
    const CdGeom gm = geom();
    if (in->mouse_valid) {
        const int mx = in->mouse_x, my = in->mouse_y;
        g_hot_back = g_mode == CD_PICK && inside(mx, my, gm.bx, gm.by, gm.bw, gm.bh);
        g_hot_ok = g_mode == CD_BOX && inside(mx, my, gm.ox, gm.oy, gm.ow, gm.oh);
    } else {
        g_hot_back = g_hot_ok = 0;
    }
    if (g_mode == CD_BOX) {
        if ((act & (SPK_ACCEPT | SPK_BACK)) || mclick) accept = 1;
    } else {
        if (act & SPK_UP) cursor_step(-1);
        if (act & SPK_DOWN) cursor_step(1);
        if (act & SPK_ACCEPT) accept = 1;
        if (act & SPK_BACK) back = 1;
        if (in->mouse_r && !g_prev_mouse_r) back = 1;
        if (in->wheel > 0) cursor_step(-1);
        if (in->wheel < 0) cursor_step(1);
        if (in->mouse_valid) {
            const int mx = in->mouse_x, my = in->mouse_y;
            int row = -1;
            if (mx >= gm.lx && mx < gm.lx + gm.lw && my >= gm.ly && my < gm.ly + gm.rows * gm.rowh)
                row = g_top + (my - gm.ly) / gm.rowh;
            if (row >= 0 && row < g_n && (mx != g_prev_mx || my != g_prev_my) && row != g_sel) {
                g_sel = row;
                cursor_sound();
                fprintf(stderr, "[chardoor] cursor %d: %s (mouse)\n", g_sel, g_item[g_sel].label);
            }
            if (mclick && row >= 0 && row < g_n) { g_sel = row; accept = 1; }
            if (mclick && g_hot_back) back = 1;
            g_prev_mx = mx; g_prev_my = my;
        }
    }
    g_prev_keys = k;
    g_prev_mouse_l = in->mouse_l ? 1 : 0;
    g_prev_mouse_r = in->mouse_r ? 1 : 0;
    if (accept) {
        if (g_mode == CD_PICK) {
            g_pick = g_item[g_sel].id;
            fprintf(stderr, "[chardoor] picked row %d: %s (id %d)\n", g_sel, g_item[g_sel].label, g_pick);
            close_now("picked");
        } else {
            close_now("ok");
        }
        return 0;
    }
    if (back) {
        close_now("back");
        return 0;
    }
    return 1;
}

void port_char_door_draw(uint32_t *px, int stride, int w, int h)
{
    if (g_mode == CD_NONE || !px) return;
    g_pw = w;
    g_ph = h;
    const CdGeom gm = geom();
    cdskin_draw(px, stride, w, h, &gm, g_item, g_n, g_sel, g_top, g_mode == CD_BOX ? CDS_BOX : CDS_PICK, box_text(), g_hot_back, g_hot_ok || g_mode == CD_BOX);
}

}
