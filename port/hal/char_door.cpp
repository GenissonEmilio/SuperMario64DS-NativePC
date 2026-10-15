/* hal/char_door.cpp: see char_door.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "char_door.h"
#include "char_registry.h"
#include "char_save.h"
#include "mods.h"

extern "C" {
extern unsigned char data_0209caa0[];
/* the door callback seam, defined in unmatched/Door_Behavior.cpp (null on targets that never seat it) */
extern int (*port_char_door_idle_hook)(void *door, void *player);
/* hal/settings_page_hooks.cpp: the stylus filter asks whether the picker is up */
extern int (*port_char_door_touch_open)(void);
/* ROM bodies the hook reads (all matched src, C names as src/actors/Player.cpp uses them) */
int _ZN6Player7IsStateERNS_5StateE(void *self, void *state);
extern int data_ov002_0211013c[];
extern int data_ov002_02110154[];
extern int data_ov002_0211043c[];
unsigned char NumStars(void);
int func_ov100_021452e4(char *door, char *player);   /* the door's own "player is in front of me" test */
}

namespace {

const char *const ROM_NAME[4] = { "Mario", "Luigi", "Wario", "Yoshi" };

enum { CD_NONE = 0, CD_BOX = 1, CD_PICK = 2 };

struct Item {
    int id;           /* 0..3 plain, 4 + n custom n */
    char label[96];
};

int g_want;            /* a request: CD_BOX or CD_PICK, honoured by the frame loop */
int g_mode;            /* CD_NONE while closed */
int g_armed = 1;       /* the door may open the picker again (the player has left its doorway since) */
int g_pick = -1;
Item g_item[16];
int g_n;
int g_sel;
int g_top;             /* first visible row */
int g_pw = 512, g_ph = 384, g_adv = 8, g_line = 10;
unsigned g_prev_keys;
int g_prev_mouse_l;
int g_prev_mouse_r;
int g_prev_mx = -1, g_prev_my = -1;
int g_hold_key;
int g_hold_frames;
int g_input_ready;     /* every device was released once since it opened: the press that brought the player here is not an answer */
int g_cur = 0;
int g_frames;
int g_saw_closed;      /* this session saw the door still shut (the ROM's own first pass is ahead of us, or under way) */
int g_star0;           /* the star count while it was shut: the ROM's star has been given once the count is above it */

enum { MAX_ROWS = 8 };

int rom_char_unlocked(int c)
{
    return c == 0 || (*(int *)(data_0209caa0 + 8) & (1 << c)) != 0;
}

void build_list(int cur)
{
    g_n = 0;
    for (int c = 0; c < 4; ++c) {
        if (!rom_char_unlocked(c)) continue;
        g_item[g_n].id = c;
        snprintf(g_item[g_n].label, sizeof g_item[g_n].label, "%s", ROM_NAME[c]);
        ++g_n;
    }
    for (int i = 0; i < port_char_count() && g_n < 16; ++i) {
        g_item[g_n].id = 4 + i;
        snprintf(g_item[g_n].label, sizeof g_item[g_n].label, "%s  %s", port_char_name(i), ROM_NAME[port_char_base(i) & 3]);
        ++g_n;
    }
    g_sel = 0;
    for (int i = 0; i < g_n; ++i)
        if (g_item[i].id == cur) g_sel = i;
    g_top = 0;
    if (g_sel >= MAX_ROWS) g_top = g_sel - MAX_ROWS + 1;
}

/* the panel: where it sits and how big, from the metrics. rows = visible list rows */
struct Panel {
    int x, y, w, h;
    int x_text;
    int rows;
    int title_y;
    int row_y0;
};

int text_w(const char *s) { return (int)strlen(s) * g_adv; }

const char *box_text(void) { return "Custom character select unlocked"; }
const char *title_text(void) { return "Character"; }

Panel layout(void)
{
    Panel p;
    int wmax = 0;
    if (g_mode == CD_BOX) {
        wmax = text_w(box_text());
        p.rows = 1;
    } else {
        wmax = text_w(title_text());
        for (int i = 0; i < g_n; ++i)
            if (text_w(g_item[i].label) > wmax) wmax = text_w(g_item[i].label);
        wmax += 2 * g_adv;
        p.rows = g_n < MAX_ROWS ? g_n : MAX_ROWS;
    }
    p.w = wmax + 4 * g_adv;
    const int lines = (g_mode == CD_BOX ? 1 : 1) + 1 + p.rows;   /* title or message, a gap line, the rows */
    p.h = lines * g_line + g_line;
    p.x = (g_pw - p.w) / 2;
    if (p.x < 2) p.x = 2;
    p.y = (g_ph - p.h) / 2;
    if (p.y < 2) p.y = 2;
    p.x_text = p.x + 2 * g_adv;
    p.title_y = p.y + g_line / 2;
    p.row_y0 = p.title_y + 2 * g_line;
    return p;
}

void close_now(const char *why)
{
    fprintf(stderr, "[chardoor] %s closed (%s)\n", g_mode == CD_BOX ? "box" : "picker", why);
    g_mode = CD_NONE;
    g_armed = 0;           /* stay shut until the player leaves the doorway */
}

void cursor_step(int d)
{
    if (g_n <= 0) return;
    g_sel = (g_sel + d + g_n) % g_n;
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
    const int first = !port_char_save_unlocked(slot);
    /* the box comes as soon as the ROM's star is done (the player stands wherever the ROM left them); the picker only
       when the player walks into the doorway */
    if (!first) {
        if (!func_ov100_021452e4(d, p)) { g_armed = 1; return 0; }  /* not in the doorway: the ROM's own watch */
        if (!g_armed) return 1;                                     /* just closed: do not let it open the door under them */
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
    if (g_mode == CD_PICK) build_list(cur);
    fprintf(stderr, "[chardoor] %s open, %d rows, cursor %d\n", g_mode == CD_BOX ? "box" : "picker", g_mode == CD_BOX ? 1 : g_n, g_sel);
    if (g_mode == CD_PICK)
        for (int i = 0; i < g_n; ++i)
            fprintf(stderr, "[chardoor] row %d: %s%s\n", i, g_item[i].label, i == g_sel ? "  <" : "");
}

void port_char_door_disarm(void) { g_armed = 0; }

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

int port_char_door_frame(const struct SpInput *in)
{
    if (g_mode == CD_NONE) return 0;
    ++g_frames;
    const unsigned k = in->keys & (SPK_UP | SPK_DOWN | SPK_LEFT | SPK_RIGHT | SPK_ACCEPT | SPK_BACK);
    const unsigned edge = k & ~g_prev_keys;
    int mclick = 0;
    if (in->mouse_valid) {
        if (in->mouse_l && !g_prev_mouse_l) mclick = 1;
    }
    if (!g_input_ready) {
        /* wait for every device to be seen up once */
        if (k == 0 && !in->mouse_l) g_input_ready = 1;
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
    if (g_mode == CD_BOX) {
        if ((act & (SPK_ACCEPT | SPK_BACK)) || mclick) accept = 1;
    } else {
        const Panel p = layout();
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
            if (mx >= p.x && mx < p.x + p.w && my >= p.row_y0 && my < p.row_y0 + p.rows * g_line)
                row = g_top + (my - p.row_y0) / g_line;
            if (row >= 0 && row < g_n && (mx != g_prev_mx || my != g_prev_my) && row != g_sel) {
                g_sel = row;
                fprintf(stderr, "[chardoor] cursor %d: %s (mouse)\n", g_sel, g_item[g_sel].label);
            }
            if (mclick && row >= 0 && row < g_n) { g_sel = row; accept = 1; }
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

void port_char_door_draw(const struct CharDoorPaint *pt)
{
    if (g_mode == CD_NONE || !pt) return;
    const Panel p = layout();
    pt->shade(pt->ctx, p.x, p.y, p.w, p.h);
    pt->shade(pt->ctx, p.x, p.y, p.w, p.h);
    if (g_mode == CD_BOX) {
        pt->text(pt->ctx, p.x_text, p.title_y, box_text(), 0xFFFFFFFFu);
        pt->text(pt->ctx, p.x_text, p.row_y0, "> OK", 0xFFFFE060u);
        return;
    }
    pt->text(pt->ctx, p.x_text, p.title_y, title_text(), 0xFF80C0FFu);
    for (int i = 0; i < p.rows; ++i) {
        const int ix = g_top + i;
        if (ix >= g_n) break;
        const int y = p.row_y0 + i * g_line;
        const int sel = ix == g_sel;
        if (sel) pt->text(pt->ctx, p.x_text, y, ">", 0xFFFFE060u);
        pt->text(pt->ctx, p.x_text + 2 * g_adv, y, g_item[ix].label, sel ? 0xFFFFE060u : 0xFFB0B0B0u);
    }
    if (g_top > 0) pt->text(pt->ctx, p.x + p.w - 3 * g_adv, p.row_y0, "^", 0xFFB0B0B0u);
    if (g_top + p.rows < g_n) pt->text(pt->ctx, p.x + p.w - 3 * g_adv, p.row_y0 + (p.rows - 1) * g_line, "v", 0xFFB0B0B0u);
}

}
