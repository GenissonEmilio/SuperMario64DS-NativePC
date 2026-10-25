/* menu_online.cpp -- the title's online menu, logic side (run after093, lane NAME1). See menu_online.h.

   THE SAVE MEDIUM FOLLOWS THE PAGE. ADVENTURE ONLINE switches ntr::backup to the signed online save before the ROM builds its file select (the
   title's loader, src/game/stages/d_s_dsmt.cpp arg 20, reads the three files through SaveData::ReadFileData each time the page loads), and the
   next main page that settles switches it back. The solo files are therefore never shown on the online file select and the online ones never
   on the solo one. If a join succeeds the game starts on the online medium the ROM already reads; it stays until the title's main page is up again.

   THE JOIN. Picking an online file swallows the ROM's confirm, writes the launcher's request (hal/online_join.h, lane JOIN1's contract) and shows
   "Finding a server..." with a cancel. JOIN1 applies the plan the way the launcher's own start does: it sets the launcher's environment names and
   starts the game again at its front door, which boots straight into the chosen file with the hub installed (online_join_step answers kJoinEntering
   and this process quits), so the screen stays up until the window closes. A refusal or no answer is a plain sentence and Back. Without JOIN1's
   header in the tree (a build of this lane alone) a pick can only end in that sentence. */
#include "menu_online.h"
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#  if __has_include("online_join.h")
#    include "online_join.h"
#    define ONL_JOIN_REAL 1
#  endif
#endif
#ifndef ONL_JOIN_REAL
#  define ONL_JOIN_REAL 0
#endif

extern "C" char *data_ov007_02103360;   /* the title's element-cursor struct (src/func_ov007_020b3708.c): its first two shorts are the cursor element and its target */
namespace ntr { namespace backup { void select_online(int on); } }

extern "C" {
unsigned char func_ov007_020cc600(int arg);   /* src/game/stages/d_s_dsmt.cpp: the title's client command dispatcher; 20 = read the three files again (allocates a fresh 0xcc byte block) */
int port_title_elem_boxes(int *slot, int *id, int *x0, int *y0, int *x1, int *y1, int max);
}

namespace {

struct St {
    int on;                    /* the online planks are the target */
    unsigned tick;
    unsigned t_toggle;         /* tick of the last toggle (0 = never) */
    int scr;
    unsigned t_scr;
    int flow;                  /* the online file select is the page */
    int med_online;            /* ntr::backup currently reads the online save */
    int entering;              /* JOIN1 started the successor: this process is quitting */
    int pick_slot;
    int help_files;
    char why[96];
    std::chrono::steady_clock::time_point wait0;
    int last_down;
    unsigned t_flow;           /* tick ADVENTURE ONLINE was pressed: the settled main page does not reset the flow for a while after it */
    int released_since;        /* the touch / key that opened a screen must be let go before it can press that screen's button */
    int touch_hold;            /* INPUTMATRIX1: a stylus / click that closed a screen is kept from the ROM until it is lifted */
} g;

enum { kWaitCapMs = 22000 };

void medium(int online)
{
    if ((online != 0) == (g.med_online != 0)) return;
    g.med_online = online ? 1 : 0;
    ntr::backup::select_online(online ? 1 : -1);      /* off = what the environment says (a launcher started Adventure Online keeps its own save) */
    /* the title read the three files once, at the scene's start (the log of SM64DS_SAVE_TRACE shows the 0x0000 / 0x0080 / 0x0100 reads long before the
       main page): the ROM's own command 20 reads them again, from the medium that is selected now, and rebuilds the plates' data from them */
    func_ov007_020cc600(20);
}

int elem_box(int id, int *x0, int *y0, int *x1, int *y1)
{
    int slot[24], ids[24], bx0[24], by0[24], bx1[24], by1[24];
    const int n = port_title_elem_boxes(slot, ids, bx0, by0, bx1, by1, 24);
    for (int k = 0; k < n; ++k)
        if (ids[k] == id) { *x0 = bx0[k]; *y0 = by0[k]; *x1 = bx1[k]; *y1 = by1[k]; return 1; }
    return 0;
}

void set_screen(int s)
{
    g.scr = s;
    g.t_scr = g.tick;
    fprintf(stderr, "[online] screen %d\n", s);
    g.released_since = 0;
}

void say_fail(const char *why)
{
    snprintf(g.why, sizeof g.why, "%s", why && *why ? why : "No server answered. Keep the launcher open and try again.");
    set_screen(ONL_SCR_FAIL);
}

void join_start(int slot)
{
    g.pick_slot = slot;
    g.entering = 0;
    g.wait0 = std::chrono::steady_clock::now();
    set_screen(ONL_SCR_WAIT);
#if ONL_JOIN_REAL
    if (!port::online_join_begin(slot, "")) say_fail(port::online_join_sentence());
#endif
}

void join_cancel()
{
#if ONL_JOIN_REAL
    if (!g.entering) port::online_join_cancel();
#endif
}

}  // namespace

const OnlFacts *onl_facts_of(const MenuLayout *L)
{
    const OnlFacts *f = (const OnlFacts *)(L->facts.blob + ONL_FACTS_OFF);
    return f->magic == ONL_MAGIC ? f : 0;
}

int menu_online_enabled(void) { return hal_menu_enabled(MENU_KEY_ONE_SCREEN_MENUS); }
int menu_online_on(void) { return g.on; }
int menu_online_modal(void) { return g.scr != ONL_SCR_NONE; }

void menu_online_facts(OnlFacts *f)
{
    memset(f, 0, sizeof *f);
    f->magic = ONL_MAGIC;
    f->on = g.on;
    f->age = g.t_toggle ? (int)(g.tick - g.t_toggle) : 999;
    if (f->age > 999) f->age = 999;
    f->scr = g.scr;
    f->scr_age = (int)(g.tick - g.t_scr);
    f->flow = g.flow;
    f->help_files = g.help_files;
    memcpy(f->why, g.why, sizeof f->why);
}

void menu_online_scene_left(void)
{
    g.on = 0;
    g.t_toggle = 0;
    g.help_files = 0;
    if (g.scr == ONL_SCR_WAIT) join_cancel();
    g.scr = ONL_SCR_NONE;
    /* the medium and g.flow stay: a started online game reads the online save; the next settled main page switches back */
}

void menu_online_tick(int top, int elem, int req)
{
    ++g.tick;
    if (g.scr != ONL_SCR_NONE && g.tick - g.t_scr > 8) g.released_since = 1;
    if (top == 0 && elem == 0 && req == -1) {            /* the main page is settled */
        if ((g.flow || g.med_online) && g.tick - g.t_flow > 40) {
            g.flow = 0;
            medium(0);
            if (g.scr == ONL_SCR_WAIT) { join_cancel(); g.scr = ONL_SCR_NONE; }
        }
    }
    if (g.scr == ONL_SCR_WAIT && !g.entering) {
        int r = 0;
        const char *why = "";
#if ONL_JOIN_REAL
        const port::OnlineJoinState st = port::online_join_step();
        if (st == port::kJoinEntering) r = 1;
        else if (st == port::kJoinFailed) { r = -1; why = port::online_join_sentence(); }
#endif
        const long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g.wait0).count();
        if (r == 0 && ms > kWaitCapMs) { join_cancel(); r = -1; why = ""; }
        if (r > 0) {
            g.entering = 1;      /* the plan is applied and the game starts again: the wait screen stays up until this window closes */
            fprintf(stderr, "[online] plan applied for file %d, the game starts again\n", g.pick_slot);
        } else if (r < 0) {
            fprintf(stderr, "[online] join failed: %s\n", why[0] ? why : "(no answer)");
            say_fail(why);
        }
    }
}

static void toggle_on()
{
    g.on = !g.on;
    g.t_toggle = g.tick ? g.tick : 1;
    fprintf(stderr, "[online] planks -> %s\n", g.on ? "online" : "solo");
}

static void close_screen()
{
    if (g.scr == ONL_SCR_WAIT) join_cancel();
    fprintf(stderr, "[online] screen %d closed\n", g.scr);
    set_screen(ONL_SCR_NONE);
}

/* a press of the modal screen's button */
static void modal_button()
{
    if (g.scr == ONL_SCR_NONE || !g.released_since) return;
    fprintf(stderr, "[online] screen %d closed by its button\n", g.scr);
    close_screen();
}

int menu_online_press_main(int elem, int edge)
{
    if (!menu_online_enabled()) return elem == 1 ? 1 : 0;       /* the classic title: the MP / VS plate takes no press, as before */
    if (g.scr != ONL_SCR_NONE) {
        if (edge) { if (g.released_since) modal_button(); }
        return 1;
    }
    if (elem == 1) { if (edge) toggle_on(); return 1; }
    if (elem == 2 && g.on) {
        if (edge) { set_screen(ONL_SCR_VS); }
        return 1;
    }
    if (elem == 0 && g.on) {
        if (edge) {
            g.flow = 1;
            g.t_flow = g.tick;
            medium(1);
            fprintf(stderr, "[online] ADVENTURE ONLINE: the file select reads the online save\n");
        }
        return 0;
    }
    return 0;
}

int menu_online_press_file(int elem, int edge)
{
    if (!g.flow) return 0;
    if (edge) fprintf(stderr, "[online] confirm on the online file select, ROM cursor element %d\n", elem);
    if (g.scr != ONL_SCR_NONE) {
        if (edge) { if (g.released_since) modal_button(); }
        return 1;
    }
    if (elem == 255 || elem < 0) {                       /* the ROM's cursor is not shown yet: the first confirm only shows it, on file A */
        if (edge && data_ov007_02103360) {
            short *p0 = *(short **)data_ov007_02103360;
            if (p0) p0[1] = 14;
        }
        return 1;
    }
    if (elem >= 14 && elem <= 16) {
        if (edge) {
            fprintf(stderr, "[online] file %d picked: asking the launcher for a server\n", elem - 14);
            join_start(elem - 14);
        }
        return 1;
    }
    return 0;
}

static int in_box(int sx, int sy, int x, int y, int w, int h) { return sx >= x && sx < x + w && sy >= y && sy < y + h; }

int menu_online_touch(int kind, int down, int sx, int sy)
{
    const int edge = down && !g.last_down;
    g.last_down = down;
    if (!down) {
        g.released_since = 1;
        g.touch_hold = 0;
        return 0;
    }
    if (!menu_online_enabled()) return 0;
    if (g.scr != ONL_SCR_NONE) {
        /* the press that closes the screen keeps being swallowed until the finger is up: the screen is gone on the next frame and the ROM's
           title underneath (the face, the planks) would otherwise see the rest of that same press as a fresh touch */
        if (edge && g.released_since && in_box(sx, sy, ONL_BTN_X, ONL_BTN_Y, ONL_BTN_W, ONL_BTN_H)) { modal_button(); g.touch_hold = 1; }
        return 1;
    }
    if (g.touch_hold) return 1;
    if (kind == 1) {
        if (g.on && in_box(sx, sy, ONL_HELP_X, ONL_HELP_Y, ONL_HELP_W, ONL_HELP_H)) {
            if (edge) { g.help_files = 0; set_screen(ONL_SCR_HELP); }
            return 1;
        }
        int x0, y0, x1, y1;
        for (int id = 0; id < 3; ++id) {
            if (!elem_box(id, &x0, &y0, &x1, &y1) || !in_box(sx, sy, x0, y0, x1 - x0, y1 - y0)) continue;
            return menu_online_press_main(id == 0 ? 0 : id, edge);        /* ids 0, 1, 2 */
        }
        return 0;
    }
    if (kind == 2 && g.flow) {
        if (in_box(sx, sy, ONL_HELP_X, ONL_HELP_Y, ONL_HELP_W, ONL_HELP_H)) {
            if (edge) { g.help_files = 1; set_screen(ONL_SCR_HELP); }
            return 1;
        }
        int x0, y0, x1, y1;
        for (int id = 14; id <= 16; ++id) {
            if (!elem_box(id, &x0, &y0, &x1, &y1) || !in_box(sx, sy, x0, y0, x1 - x0, y1 - y0)) continue;
            return menu_online_press_file(id, edge);
        }
    }
    return 0;
}

/* a D-pad step (DS bits 0x10 right, 0x20 left, 0x40 up, 0x80 down) the ROM's cursor cannot take: the way to the ? button. 1 = taken. */
int menu_online_dir(int kind, int cursor, unsigned dirs)
{
    if (!menu_online_enabled() || g.scr != ONL_SCR_NONE) return 0;
    if (kind == 1 && g.on && cursor == 7 && (dirs & 0x20)) { g.help_files = 0; set_screen(ONL_SCR_HELP); return 1; }          /* wrench, then left */
    if (kind == 2 && g.flow && (cursor == 14 || cursor == 15 || cursor == 16) && (dirs & 0x40)) { g.help_files = 1; set_screen(ONL_SCR_HELP); return 1; }   /* the file row, then up */
    return 0;
}

int menu_online_help_pad(int kind)
{
    if (!menu_online_enabled() || g.scr != ONL_SCR_NONE) return 0;
    if (kind == 1 && g.on) { g.help_files = 0; set_screen(ONL_SCR_HELP); return 1; }
    if (kind == 2 && g.flow) { g.help_files = 1; set_screen(ONL_SCR_HELP); return 1; }
    return 0;
}

int menu_online_back(int kind)
{
    if (!menu_online_enabled()) return 0;
    if (g.scr != ONL_SCR_NONE) { close_screen(); return 1; }
    if (kind == 1 && g.on) { toggle_on(); return 1; }
    return 0;
}
