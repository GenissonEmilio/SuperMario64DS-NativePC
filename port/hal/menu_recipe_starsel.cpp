/* menu_recipe_starsel.cpp -- the STAR SELECT and GAME OVER menus as one-screen recipes (run menu1, lane STARGO1).
 *
 * Both scenes run in the level process (the star select as the interlude, game over through the
 * crossing) and both keep the ROM's own code untouched: the recipes only re-place what the ROM draws
 * and map the host's pad / mouse onto the input the ROM already reads.
 *
 * STAR SELECT (scene 4, dScStarSel_c, src/_ZN12dScStarSel_c8BehaviorEv.cpp)
 *   The upper screen stays the picture (course medal, 3D stars, names). The bottom screen's star
 *   icons are engine-B OBJ sprites in the top 0x28 rows (the ROM's own touch box: ty < 0x28, :187).
 *   One SRC_SUB piece (OBJ only) lifts the icon rows and sets them under the course name, in the free
 *   band between the lives and star counters. Driver NATIVE: the ROM reads D-pad L/R (0x20 / 0x10,
 *   :205-243) and A itself; a click on the placed piece is inverted to the DS touch the ROM's own
 *   box test (:187) reads. One hotspot per icon, from the live object (+0x114 count, +0x11a.. icon x
 *   bytes, +0x131 availability mask), for hover.
 *   VARIANT 1 the stars row; VARIANT 2 the multiplayer character row (+0x133 == 1), where the whole
 *   bottom screen is the choice: drawn as a half-size panel (the fallback's shape).
 *
 * GAME OVER (scene 8, dScGameOver_c, src/actors/dScGameOver_c.cpp)
 *   The upper screen stays the picture (GAME OVER lettering, gradient); the yes/no plates (engine-B BG0
 *   labels + BG1 plates) are set under the lettering, one piece per plate. The ROM's touch boxes are
 *   x 8..0xf7 with y 0x38..0x57 (Continue) and 0x68..0x87 (Quit) (:342, :364).
 *   Driver ROM_CURSOR: the ROM keeps a cursor byte (+0x91, 0 Continue, 1 Quit) that the plates'
 *   highlight (func_ov003_020b0730) paints, and a confirm (IsButtonInputValid) commits the CURRENT
 *   one. Only a touch ever moved it. Here the pad moves the byte and repaints through that same
 *   function, so pad and keys reach Quit too; A (rewritten to Start) commits it.
 */
#include "menu_compose.h"

#include "menu_hostimg.h"

#include <string.h>

extern "C" {
int  SublevelToLevel(int);
extern signed char data_02092110;
void *port_scene_live_object(void);
int  port_scene_live_id(void);
void func_ov003_020b0730(void *self, int arg);
void func_02012790(int id);
}

namespace {

inline unsigned char ob(const void *o, int off) { return ((const unsigned char *)o)[off]; }

/* ---------------------------------------------------------------- star select */

const int kStarScene = 4;

int up_starsel(int scene_id)
{
    if (scene_id != kStarScene) return 0;
    const void *o = port_scene_live_object();
    if (o && ob(o, 0x133) == 1) return 2;      /* the multiplayer character row */
    return 1;
}

/* THE UPPER SCREEN ONLY (Tango 10-10: the "TOUCH ME!" head and the row of small star icons on the lower screen are
   not drawn; they were redundant with the big stars and the head is a touch control). The ROM still runs them: they
   are engine-B sprites nobody sees, and a click is turned into the touch the ROM's own boxes read.
   THE BIG STARS are 3D models (src/_ZN12dScStarSel_c6RenderEv.cpp:90-101)80-95): centred on x = 0, 0x18000 apart, at one
   depth, so their screen x is linear in the index. Measured on the 4:3 picture: centres at DS x 128 + (i - (n-1)/2) * 34.8
   (kStarDx10 tenths), centre row y 32, about 34 units across. A click on big star i is mapped to the ROM's icon box i
   (x_i - 8 .. x_i + 7, y < 0x28: :187) and on the star that is already selected to the head's box (func_ov003_020ae358.c:
   x 0x58..0xa7, y within 0x28 of the byte at +0x12b), so a second click (or a double click) starts the act, as the head
   did. The pieces are touch-only host rectangles (nodraw) mapped proportionally into those boxes. */
const int kStarDx10 = 348, kStarCy = 32, kStarHalfW = 17, kStarY0 = 8, kStarY1 = 58;

/* MENUSPLIT1: everything build_starsel needs from the live scene object, read once per tick by the decide step */
void starsel_facts(MenuFacts *f)
{
    const void *o = port_scene_live_object();
    if (!o || port_scene_live_id() != kStarScene) return;
    /* Bowser and secret courses: InitResources gates the star count, mask and selection on the course (level <= 0xe,
       src/_ZN12dScStarSel_c13InitResourcesEv.cpp:280), so those bytes are not set there; the ROM draws no star row. */
    if (SublevelToLevel(data_02092110) > 0xe) return;
    f->starsel_valid = 1;
    f->starsel_n = ob(o, 0x114);
    f->starsel_mask = ob(o, 0x131);
    for (int i = 0; i < 8; ++i) f->starsel_x[i] = ob(o, 0x11a + i);
    f->starsel_head_y = ob(o, 0x12b);
    f->spare[0] = 1 + ob(o, 0x115);       /* the selected act index + 1 (0 = nothing read) */
    { static int calls; f->spare[2] = ++calls; }     /* a per-tick counter for the pointer's bob */
    f->spare[1] = ob(o, 0x135);           /* the input phase: 0 intro, 1 waiting for the pad to come up, 2 live */
}

/* STARHOOK1: the big stars' rects for hal/menu_starsel_api.cpp (run menu1, for mp1's players-in-stars heads). The upper screen is
   the same picture whether OneScreenMenus is on or off (the recipe keeps it as the ROM draws it), so the rects are in
   TICK-FRAMEBUFFER pixels (w x h = the picture size, unit = h / 192) and do not depend on the layout. Reads no ROM: only the facts.
   WHERE THE STARS ARE. The ROM places star i in the world at x = (i - (n-1)/2) * 0x18000, y 0x28000 (src/_ZN12dScStarSel_c6RenderEv.cpp:90-101) and InitResources writes the matching icon column bytes starsel_x[i] = 0x92 (n even) or 0x80 (n odd) - (n/2) * 0x23 + i * 0x23
   (src/_ZN12dScStarSel_c13InitResourcesEv.cpp:324-328), 35 DS pixels apart, centred on 128. Measured on the 4:3 picture (STARSEL2 shots,
   n = 1..6): the big star's centre column is exactly starsel_x[i] and its centre row 34.5 (0x28000 through the fixed camera; the 16:9 and
   21:9 pictures give the same numbers in units of h / 192, within 0.6 percent of a unit per star). The idle star is 30 x 28.5 DS pixels
   across; the selected one is drawn 1.25x (Render :93-96, scale 0x1400 / 0x1000) and turns about its vertical axis, so its width
   varies and the rect is the widest it gets. Half extents in tenths of a DS pixel. */
const int kBigHalfW10 = 150, kBigHalfH10 = 145, kBigSelHalfW10 = 188, kBigSelHalfH10 = 181, kBigCy10 = 345;

bool big_star_rect(const MenuFacts &F, int w, int h, int star, int out[4])
{
    if (!F.starsel_valid || star < 0 || star >= F.starsel_n || star >= 8 || w <= 0 || h <= 0) return false;
    const int cur = F.spare[0] - 1;
    const int hw10 = (star == cur) ? kBigSelHalfW10 : kBigHalfW10, hh10 = (star == cur) ? kBigSelHalfH10 : kBigHalfH10;
    const long long cx10 = (long long)(F.starsel_x[star] - 128) * 10;     /* tenths of a DS unit from the centre column */
    /* host = centre + tenths * h / 1920 (one DS unit is h / 192 host pixels); the width and height are exact */
    out[0] = w / 2 + (int)(((cx10 - hw10) * h) / 1920);
    out[1] = (int)((((long long)kBigCy10 - hh10) * h) / 1920);
    out[2] = (int)((2LL * hw10 * h) / 1920);
    out[3] = (int)((2LL * hh10 * h) / 1920);
    return true;
}

void build_starsel(int variant, MenuLayout *L)
{
    if (variant == 2) {
        /* the character row: the whole bottom screen, half size, low and centred (a scale-down of the fallback) */
        MenuPiece p = menu_piece_make(SRC_SUB, 0, 0, 256, 192, MENU_AX_C, MENU_AY_B, 0, -4, 1, 2);
        p.touch = 1;
        const int pi = menu_add_piece(L, p);
        for (int j = 0; j < 3; ++j)
            for (int i = 0; i < 3; ++i) menu_add_hotspot(L, pi, i * 85, j * 64, 85, 64, -1);
        L->backdrop = MENU_BACKDROP_TOP_AS_IS;
        L->stage = MENU_STAGE_POST_FADE;
        return;
    }
    const MenuFacts &F = L->facts;       /* the live object's fields, read once per tick by starsel_facts */
    L->no_focus_paint = 1;
    {
        /* no live facts (the layout selftest, a course without a star row): one invisible touch rect on the centre star,
           so the layout is never empty */
        const int n = F.starsel_valid ? F.starsel_n : 1, cur = F.starsel_valid ? F.spare[0] - 1 : 0;
        const unsigned mask = F.starsel_valid ? F.starsel_mask : 1u;
        const long long UN = L->unum, UD = L->uden;
        for (int i = 0; i < n && i < 8; ++i) {
            if (!((mask >> i) & 1)) continue;
            /* the star's centre, DS x tenths -> host x (the picture is centred, one unit = UN / UD host pixels) */
            const int cx10 = 1280 + (2 * i - (n - 1)) * kStarDx10 / 2;
            const int x0 = L->w / 2 + (int)(((long long)(cx10 - 1280) - 10 * kStarHalfW) * UN / (10 * UD));
            const int x1 = L->w / 2 + (int)(((long long)(cx10 - 1280) + 10 * kStarHalfW) * UN / (10 * UD));
            const int y0 = (int)(kStarY0 * UN / UD), y1 = (int)(kStarY1 * UN / UD);
            int sx, sy, sw, sh;
            if (i == cur) {
                /* the head's box, kept inside the part the ROM tests */
                int hy = F.starsel_valid ? F.starsel_head_y : 0x28;
                sx = 0x60; sw = 0x40; sy = hy - 0x20; sh = 0x40;
                if (sy < 0) sy = 0;
            } else {
                sx = (F.starsel_valid ? (int)F.starsel_x[i] : 128) - 8; if (sx < 0) sx = 0; if (sx > 240) sx = 240;
                sy = 0; sw = 16; sh = 0x28;
            }
            MenuPiece p = menu_piece_make(SRC_HOSTIMG, sx, sy, sw, sh, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
            p.nodraw = 1; p.touch = 1; p.lden = 1; p.lnum = 1;
            p.fx = (short)x0; p.fy = (short)y0; p.fw = (short)(x1 - x0); p.fh = (short)(y1 - y0);
            const int pi = menu_add_piece(L, p);
            if (pi >= 0) menu_add_hotspot(L, pi, sx, sy, sw, sh, -1);
        }
        /* THE POINTER: the game's own small blue triangle (menu_hostimg.h) over the selected star, bobbing on the
           8-frame cadence. It is a backdrop piece: a click goes through to the star under it. */
        if (F.starsel_valid && cur >= 0 && cur < n && menu_hostimg_ready(MENU_IMG_ARROW_DOWN)) {
            static const int kBob[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };       /* half units */
            int iw = 1, ih = 1;
            menu_hostimg_size(MENU_IMG_ARROW_DOWN, &iw, &ih);
            const int dw = 14 * (int)UN / (int)UD, dh = (int)((long long)dw * ih / iw);
            MenuPiece ar = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
            ar.bg = 1; ar.hostimg = MENU_IMG_ARROW_DOWN; ar.lden = 1; ar.lnum = 1; ar.fw = (short)dw; ar.fh = (short)dh;
            const int cx10 = (2 * cur - (n - 1)) * kStarDx10 / 2;
            ar.fx = (short)(L->w / 2 + (int)((long long)cx10 * UN / (10 * UD)) - dw / 2);
            int top = (int)(((long long)(2 * (kStarCy - 12) + kBob[(L->facts.spare[2]) & 7]) * UN) / (2 * UD)) - dh;
            if (top < 0) top = 0;
            ar.fy = (short)top;
            menu_add_piece(L, ar);
        }
    }
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

/* ------------------------------------------------------------------ game over */

const int kGoScene = 8;

int up_gameover(int scene_id) { return scene_id == kGoScene ? 1 : 0; }

/* THE LAYOUT, BUILT PER ASSET (Tango 10-10). Engine A's layers are taken apart: BG2 is the grey gradient, BG0 is the
   "TOUCH TO SELECT" text, the OBJ layer is the GAME OVER lettering (dropping in) and the two bouncing arrows. The
   compositor keeps one layer per cell, so hiding the text alone would leave the backdrop colour in its cells; the
   gradient is therefore lifted too and drawn here, from one clean column carried across the WHOLE picture (xfill: the
   gradient is the same in every column), so a wide picture has no seam and no navy side bars. The lettering is its own
   piece (rows 0..135, which holds the drop-in and leaves the arrows out) and the text layer is drawn nowhere.
   The two plates (engine-B BG0 labels + BG1 plates) go under the lettering, one piece each, at 3/4 size: Continue is
   bottom rows 0x36..0x5a, Quit 0x66..0x8a (a plate is two rows taller than its touch box on each side,
   dScGameOver_c::Behavior :342 / :364). */
const int kPlYc = 0x36, kPlYq = 0x66, kPlH = 36;
const int kGradCol = 16;      /* a column clear of the arrows (x 0..12), the text (from x 27) and the lettering */

void build_gameover(int, MenuLayout *L)
{
    /* BG2 rows 174..183 are the band the ROM hides under the text: on some frames (the arrows' bounce) those cells read as
       the backdrop colour or navy. The gradient is drawn from rows 0..173, rows 172..173 repeat over the band's 10 rows,
       and rows 184..191 are drawn as they are. */
    for (int k = 0; k < 7; k++) {
        const int sy = k == 0 ? 0 : (k <= 5 ? 172 : 184), sh = k == 0 ? 174 : (k <= 5 ? 2 : 8),
                  dy = k == 0 ? 0 : (k <= 5 ? 174 + (k - 1) * 2 : 184);
        MenuPiece g = menu_piece_make(SRC_TOP_2D, kGradCol, sy, 1, sh, MENU_AX_C, MENU_AY_T, 0, dy, 1, 1);
        g.layers = MENU_L_BG2; g.bg = 1; g.xfill = 1; g.fill_w = (short)L->canvas_w;
        menu_add_piece(L, g);
    }
    MenuPiece t = menu_piece_make(SRC_TOP_2D, 0, 0, 256, 136, MENU_AX_C, MENU_AY_T, 0, 0, 1, 1);
    t.layers = MENU_L_OBJ; t.bg = 1;
    menu_add_piece(L, t);
    MenuPiece c = menu_piece_make(SRC_SUB, 0, kPlYc, 256, kPlH, MENU_AX_C, MENU_AY_T, 0, 118, 3, 4);
    c.layers = MENU_L_BG0 | MENU_L_BG1;
    c.key_backdrop = 1;
    c.touch = 1;
    const int pc = menu_add_piece(L, c);
    MenuPiece q = menu_piece_make(SRC_SUB, 0, kPlYq, 256, kPlH, MENU_AX_C, MENU_AY_T, 0, 150, 3, 4);
    q.layers = MENU_L_BG0 | MENU_L_BG1;
    q.key_backdrop = 1;
    q.touch = 1;
    const int pq = menu_add_piece(L, q);
    menu_add_hotspot(L, pc, 8, 0x38, 0xf0, 0x20, 0);
    menu_add_hotspot(L, pq, 8, 0x68, 0xf0, 0x20, 1);
    /* THE CURSOR: the ROM has none (it only paints the selected plate pale yellow after the first pad press), so one of the
       game's own small triangles points at the selected plate from the first frame, centred, its tip just inside the
       plate's top edge (for Quit it overlaps the lower edge of the Continue plate). A backdrop piece: a click goes to the
       plate. Only down / up arrows exist; a side-pointing one is an artist ask. */
    if (L->facts.spare[0] > 0 && menu_hostimg_ready(MENU_IMG_ARROW_DOWN)) {
        static const int kBob[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };       /* half units */
        const long long UN = L->unum, UD = L->uden;
        int iw = 1, ih = 1;
        menu_hostimg_size(MENU_IMG_ARROW_DOWN, &iw, &ih);
        const int dw = 14 * (int)UN / (int)UD, dh = (int)((long long)dw * ih / iw);
        MenuPiece ar = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        ar.bg = 1; ar.hostimg = MENU_IMG_ARROW_DOWN; ar.lden = 1; ar.lnum = 1; ar.fw = (short)dw; ar.fh = (short)dh;
        ar.fx = (short)(L->w / 2 - dw / 2);
        const int plate_top = L->facts.spare[0] == 1 ? 118 : 150;
        int top = (int)(((long long)(2 * (plate_top - 2) + kBob[L->facts.spare[2] & 7]) * UN) / (2 * UD)) - dh;
        if (top < 0) top = 0;
        ar.fy = (short)top;
        menu_add_piece(L, ar);
    }
    L->lift_owners = MENU_L_BG2 | MENU_L_OBJ;
    L->hide_owners = MENU_L_BG0;
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

/* MENUSPLIT1 facts for the game over cursor: the ROM's own cursor byte (+0x91, 0 Continue, 1 Quit) once the intro is in
   (+0x94 >= 2); spare[0] = 1 + the selected plate, spare[2] = a per-tick counter for the arrow's bob. */
void gameover_facts(MenuFacts *f)
{
    const void *o = port_scene_live_object();
    if (!o || port_scene_live_id() != kGoScene) return;
    if (ob(o, 0x94) >= 2) f->spare[0] = 1 + (ob(o, 0x91) ? 1 : 0);
    { static int calls; f->spare[2] = ++calls; }
}

int g_go_shown;      /* the pad cursor has painted its highlight on this screen */

/* The ROM reads the pad only once the intro is in (+0x94 >= 2) and while its FSM waits (+0x90 == 0). */
int pad_ok_gameover(void)
{
    const void *o = port_scene_live_object();
    if (!o || port_scene_live_id() != kGoScene) { g_go_shown = 0; return 0; }
    return ob(o, 0x94) >= 2 && ob(o, 0x90) == 0;
}

/* Move the ROM's own cursor byte and repaint the plates through the ROM's own function. up / left =
   Continue, down / right = Quit. */
int rom_cursor_gameover(int dir, int *sel_out)
{
    void *o = port_scene_live_object();
    if (!o || port_scene_live_id() != kGoScene) return 0;
    unsigned char *s = (unsigned char *)o;
    const int want = (dir == MENU_DIR_UP || dir == MENU_DIR_LEFT) ? 0 : 1;
    int moved = 0;
    if (s[0x91] != want || !g_go_shown) {
        g_go_shown = 1;      /* the plates show no highlight until something paints one: the first press always does */
        s[0x91] = (unsigned char)want;
        func_ov003_020b0730(o, 0);
        func_02012790(0x12e);
        moved = 1;
    }
    if (sel_out) *sel_out = s[0x91];
    return moved;
}

}  // namespace

extern "C++" {
int  menu_starsel_up(int scene_id) { return up_starsel(scene_id); }
void menu_starsel_build(int variant, MenuLayout *L) { build_starsel(variant, L); }
void menu_starsel_facts(MenuFacts *f) { starsel_facts(f); }
int  menu_gameover_up(int scene_id) { return up_gameover(scene_id); }
void menu_gameover_build(int variant, MenuLayout *L) { build_gameover(variant, L); }
int  menu_gameover_rom_cursor(int dir, int *sel) { return rom_cursor_gameover(dir, sel); }
int  menu_gameover_pad_ok(void) { return pad_ok_gameover(); }
void menu_gameover_facts(MenuFacts *f) { gameover_facts(f); }
int  menu_starsel_big_rect(const MenuFacts *F, int w, int h, int star, int out[4]) { return big_star_rect(*F, w, h, star, out) ? 1 : 0; }
}
