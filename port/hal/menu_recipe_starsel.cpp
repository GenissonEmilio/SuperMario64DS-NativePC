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

#include <string.h>

extern "C" {
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

/* The icon rows: the ROM's icons are 16 px sprites centred on x_i, drawn about y 20..32; the touch
   box is the whole top 0x28 rows. Rows 14..38 hold the icons and their selected pulse; the source
   rect must stay inside the ROM's box so a click anywhere on the piece is a hit the ROM tests. */
const int kRowY0 = 14, kRowH = 24;
/* the head and "TOUCH ME!" rows of the bottom screen: x 0x44..0xbb, y 56..191 */
const int kHeadX0 = 0x44, kHeadW = 0x78, kHeadY0 = 56, kHeadH = 136;

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
    /* 4:3 (narrow): three quarter size in the free band between the lives and the star counters (their inner
       edges are at x 48 and 208), under the course name. Wide: the same band is 245 units across, so the row
       stays at full size (the lives and the star counter ride the picture's edges there). */
    const int wide = menu_wide(L);
    MenuPiece p = menu_piece_make(SRC_SUB, 0, kRowY0, 256, kRowH, MENU_AX_C, MENU_AY_T, 0, wide ? 166 : 172,
                                  wide ? 1 : 3, wide ? 1 : 4);
    p.layers = MENU_L_OBJ;
    p.key_backdrop = 1;
    p.touch = 1;
    const int pi = menu_add_piece(L, p);
    const void *o = port_scene_live_object();
    if (o && port_scene_live_id() == kStarScene) {
        const int n = ob(o, 0x114);
        const unsigned mask = ob(o, 0x131);
        for (int i = 0; i < n && i < 8; ++i) {
            if (!((mask >> i) & 1)) continue;
            int x = ob(o, 0x11a + i) - 8;
            if (x < 0) x = 0;
            if (x > 240) x = 240;
            menu_add_hotspot(L, pi, x, kRowY0, 16, kRowH, -1);
        }
    }
    /* THE START BUTTON. On the DS only a pad button starts the chosen act, or a touch on the Mario head
       (src/func_ov003_020ae358.c: x in 0x58..0xa7 and y within 0x28 of the head's own centre row, the byte at
       +0x12b, which bobs). The head and its "TOUCH ME!" line are engine-B OBJs; they are set to the right of
       the course medal, at half size, where the upper screen is empty. */
    MenuPiece h = menu_piece_make(SRC_SUB, kHeadX0, kHeadY0, kHeadW, kHeadH, MENU_AX_C, MENU_AY_T, 86, 94, 1, 2);
    h.layers = MENU_L_OBJ;
    h.key_backdrop = 1;
    h.touch = 1;
    const int ph = menu_add_piece(L, h);
    if (o && port_scene_live_id() == kStarScene) {
        int y = (int)ob(o, 0x12b) - 0x28;
        if (y < 0) y = 0;
        if (y + 0x50 > 192) y = 192 - 0x50;
        menu_add_hotspot(L, ph, 0x58, y, 0x50, 0x50, -1);
    }
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

/* ------------------------------------------------------------------ game over */

const int kGoScene = 8;

int up_gameover(int scene_id) { return scene_id == kGoScene ? 1 : 0; }

/* THE LAYOUT. The upper screen stays the picture as the ROM draws it: the GAME OVER lettering sits at DS
   rows 84..110 over the dark gradient and the "TOUCH TO SELECT" band at 167..183. The two plates go in the free
   band between them (DS rows 112..165), one piece each so the 14 rows between them in the bottom screen are
   not carried along: Continue is bottom rows 0x36..0x5a, Quit 0x66..0x8a (a plate is two rows taller than its
   touch box on each side, dScGameOver_c::Behavior :342 / :364), each at 2/3 so 36 rows become 24.
   (Lifting the lettering or hiding the band would leave the engine-A backdrop colour behind them: the
   compositor keeps only the topmost layer per cell, so the gradient under a lifted sprite is gone.) */
const int kPlYc = 0x36, kPlYq = 0x66, kPlH = 36;

void build_gameover(int, MenuLayout *L)
{
    MenuPiece c = menu_piece_make(SRC_SUB, 0, kPlYc, 256, kPlH, MENU_AX_C, MENU_AY_T, 0, 113, 2, 3);
    c.layers = MENU_L_BG0 | MENU_L_BG1;
    c.key_backdrop = 1;
    c.touch = 1;
    const int pc = menu_add_piece(L, c);
    MenuPiece q = menu_piece_make(SRC_SUB, 0, kPlYq, 256, kPlH, MENU_AX_C, MENU_AY_T, 0, 141, 2, 3);
    q.layers = MENU_L_BG0 | MENU_L_BG1;
    q.key_backdrop = 1;
    q.touch = 1;
    const int pq = menu_add_piece(L, q);
    menu_add_hotspot(L, pc, 8, 0x38, 0xf0, 0x20, 0);
    menu_add_hotspot(L, pq, 8, 0x68, 0xf0, 0x20, 1);
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
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
int  menu_gameover_up(int scene_id) { return up_gameover(scene_id); }
void menu_gameover_build(int variant, MenuLayout *L) { build_gameover(variant, L); }
int  menu_gameover_rom_cursor(int dir, int *sel) { return rom_cursor_gameover(dir, sel); }
int  menu_gameover_pad_ok(void) { return pad_ok_gameover(); }
}
