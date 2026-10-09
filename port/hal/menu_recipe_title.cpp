/* menu_recipe_title.cpp -- the TITLE family as one-screen menu recipe "title" (run menu1, lane TITLE1).
 *
 * WHAT THE TITLE IS. Scene 1 (dScDSMT_c, ov007). On this family the two engines are swapped
 * (POWCNT1 bit 15 clear): ENGINE A is the physical LOWER screen (wallpaper, the 3D Mario head, the
 * buttons, the file plates, all touchable) and ENGINE B is the physical UPPER screen (logo, castle
 * photo, the prompt banner). In the compose layer's words the "top picture" (SRC_TOP_PIC) is therefore
 * the touch screen and SRC_SUB is the picture screen. Neither can be lifted by layer (P0ATLAS 0.3), so
 * every piece is cut from a FINISHED picture along rows and columns that are empty.
 *
 * THE LAYOUT, per page (canvas = 192 units tall, 256 wide at 4:3; pieces never stretch):
 *   attract (variant 1)   the lower screen as it is (star, TOUCH TO START) fills the picture; a click
 *                         anywhere is the touch the ROM wants.
 *   main menu (2)         the upper screen (logo on sky) is the BACKDROP, full size. The button bar of the
 *                         lower screen (VS / ADVENTURE / REC ROOM, rows 134..191, wallpaper between the
 *                         plates) sits on the bottom edge over the upper screen's "TOUCH TO START" banner,
 *                         which means nothing on this page; the wrench plate moves to the right edge above
 *                         the bar. The Mario head is the one thing not kept (it is a decoration and a
 *                         hidden doodle button; there is no room for it beside the logo).
 *   file pages (3)        backdrop = the upper screen (castle photo + the page's own banner: TOUCH TO
 *                         START / FILE OPTIONS / COPY WHICH FILE? ...). The lower screen's page content
 *                         (rows 36..181: the plates and the back arrow) is one panel at 3/4 scale in the
 *                         upper part of the picture, so the banner on the upper screen's lower edge
 *                         stays readable.
 *   options (4)           the sound / backlight page: same, with the page rows 16..183.
 *   doodle (5)            the "draw a picture" page: the lower screen whole, at 3/4, under the banner.
 * Wide canvases keep the same arrangement centred: the extra width is the host's ambient fill (the
 * menu stays 4:3 native, DESIGN G); nothing is stretched.
 *
 * DRIVER. NATIVE. The ROM reads the pad on every title page (src/func_ov007_020b3190.c: any non-B key
 * shows its own cursor, the D-pad moves it through func_ov007_020b3708's table, a confirm key presses
 * the focused element). Only the key MEANING differs: the port's confirm is DS bit 0x0002 (jump), which
 * is the ROM's B; the ROM's confirm is A, 0x0001. menu_title_pad_map swaps them. Mouse clicks reach the
 * ROM as the stylus through the touch pieces (core H3); hover shows the port's focus outline.
 *
 * HOTSPOTS. The ROM's own element boxes through port_title_elem_boxes (hal/title_entry.cpp, the shared
 * decoder, arithmetic of src/func_ov007_020b46b0.c), with the dumped boxes as the fallback when the
 * scene global is not built (the layout selftest). Which ids a page shows is read off the pictures
 * (out/TITLE1/shots), not guessed from positions.
 */
#include "menu_compose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
int port_title_elem_boxes(int *slot, int *id, int *x0, int *y0, int *x1, int *y1, int max);
int port_title_page_state(int *top, int *elem, int *req);
}

namespace {

enum { V_ATTRACT = 1, V_MAIN = 2, V_FILES = 3, V_OPTIONS = 4, V_DOODLE = 5, V_TABLE = 6 };

/* the 24 element boxes as dumped on the shipped ROM (SM64DS_TITLE_ELEMS, identical at f299, f419, f650,
   f719): id, x0, y0, x1, y1 in touch-screen DS pixels. The live decoder wins when it answers. */
struct Box { int id, x0, y0, x1, y1; };
const Box kBoxes[24] = {
    { 0, 72, 148, 184, 188 },  { 1, 2, 148, 66, 188 },    { 2, 190, 148, 254, 188 }, { 3, 212, 148, 252, 188 },
    { 4, 212, 148, 252, 188 }, { 5, 88, 172, 168, 188 },  { 6, 4, 148, 44, 188 },    { 7, 216, 0, 256, 40 },
    { 8, 96, 148, 160, 180 },  { 9, 112, 25, 232, 57 },   { 10, 86, 25, 118, 57 },   { 11, 224, 25, 256, 57 },
    { 12, 112, 73, 168, 105 }, { 13, 176, 73, 232, 105 }, { 14, 8, 44, 80, 92 },     { 15, 92, 68, 164, 116 },
    { 16, 176, 44, 248, 92 },  { 17, 14, 96, 86, 132 },   { 18, 92, 96, 164, 132 },  { 19, 170, 96, 242, 132 },
    { 20, 180, 140, 252, 176 }, { 21, 28, 140, 156, 176 }, { 22, 54, 108, 126, 148 }, { 23, 130, 108, 202, 148 },
};

bool box_of(int id, Box *out)
{
    int slot[24], ids[24], x0[24], y0[24], x1[24], y1[24];
    const int n = port_title_elem_boxes(slot, ids, x0, y0, x1, y1, 24);
    for (int k = 0; k < n; ++k)
        if (ids[k] == id) { out->id = id; out->x0 = x0[k]; out->y0 = y0[k]; out->x1 = x1[k]; out->y1 = y1[k]; return true; }
    for (int k = 0; k < 24; ++k)
        if (kBoxes[k].id == id) { *out = kBoxes[k]; return true; }
    return false;
}

/* the page the ROM is on -> a variant. top-state and element machine are the ROM's own words
   (src/func_ov007_020b0548 dispatcher); the table below is what the traces showed. */
int g_last = V_ATTRACT;
int g_top = -1, g_elem = -1;

int variant_for(int top, int elem)
{
    /* THE TRACE (SM64DS_MENU_TRACE, windowed scene runs): attract 11 -> intro 10 -> arrival 1/0 -> main 0/0;
       a choice leaves through 6/0 (file select) or 4/0 (the doodle) and lands on 0/5 (file select) or 0/1
       (doodle, then 1/1, 2/1); the wrench is 8/0; file options 1/5; scores pick 3/5, copy 7/5, erase 9/5;
       the score table is 4/5 (loading) then 2/5. Transit states keep the last stable page. */
    if (top == 11) return V_ATTRACT;
    if (elem == 1) return V_DOODLE;
    if (elem == 0) {
        if (top == 0) return V_MAIN;
        if (top == 8) return V_OPTIONS;
        return g_last;                            /* 10, 1, 6, 4: easing in or out */
    }
    if (top == 2) return V_TABLE;
    if (top == 4) return g_last;
    return V_FILES;                               /* 0, 1, 3, 7, 9 on the file machine */
}

/* piece + hotspots helper: a hotspot per id, boxes clamped into the piece's source rect */
void hot(MenuLayout *L, int piece, const int *ids, int n)
{
    const DsRect &s = L->p[piece].s;
    for (int k = 0; k < n; ++k) {
        Box b;
        if (!box_of(ids[k], &b)) continue;
        int x0 = b.x0 < s.x ? s.x : b.x0, y0 = b.y0 < s.y ? s.y : b.y0;
        int x1 = b.x1 > s.x + s.w ? s.x + s.w : b.x1, y1 = b.y1 > s.y + s.h ? s.y + s.h : b.y1;
        if (x1 <= x0 || y1 <= y0) continue;
        menu_add_hotspot(L, piece, x0, y0, x1 - x0, y1 - y0, -1);
    }
}

int tune(int idx, int dflt)          /* SM64DS_TITLE_TUNE=a,b,c,d,... integer knobs for layout iteration */
{
    static int v[8], have = -1;
    if (have < 0) {
        have = 0;
        for (int i = 0; i < 8; ++i) v[i] = -9999;
        const char *e = getenv("SM64DS_TITLE_TUNE");
        for (int i = 0; e && *e && i < 8; ++i) {
            char *q;
            v[i] = (int)strtol(e, &q, 10);
            if (*q != ',') break;
            e = q + 1;
        }
    }
    return v[idx] == -9999 ? dflt : v[idx];
}

}  // namespace

int menu_title_up(int scene_id)
{
    if (scene_id != 1) return 0;
    int top, elem, req;
    if (!port_title_page_state(&top, &elem, &req)) return g_last;   /* not built yet: the attract */
    if (top != g_top || elem != g_elem) {
        g_top = top; g_elem = elem;
        if (hal_menu_trace_on()) fprintf(stderr, "[menu] title page top=%d elem=%d req=%d\n", top, elem, req);
    }
    g_last = variant_for(top, elem);
    return g_last;
}

void menu_title_build(int variant, MenuLayout *L)
{
    if (variant == V_ATTRACT) {
        MenuPiece p = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_C, MENU_AY_M, 0, 0, 1, 1);
        p.touch = 1;
        menu_add_piece(L, p);
        L->backdrop = MENU_BACKDROP_TOP_AS_IS;
        L->stage = MENU_STAGE_POST_FADE;
        return;
    }
    /* the backdrop: the upper screen, whole */
    MenuPiece bg = menu_piece_make(SRC_SUB, 0, 0, 256, 192, MENU_AX_C, MENU_AY_M, 0, 0, 1, 1);
    bg.bg = 1;
    menu_add_piece(L, bg);
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;

    if (variant == V_MAIN) {
        MenuPiece bar = menu_piece_make(SRC_TOP_PIC, 0, 134, 256, 58, MENU_AX_C, MENU_AY_B, 0, 0, 1, 1);
        bar.touch = 1;
        const int ibar = menu_add_piece(L, bar);
        MenuPiece wr = menu_piece_make(SRC_TOP_PIC, 212, 0, 44, 44, MENU_AX_L, MENU_AY_T, 6, 6, 1, 1);
        wr.touch = 1;
        const int iwr = menu_add_piece(L, wr);
        static const int bar_ids[3] = { 1, 0, 2 };
        hot(L, ibar, bar_ids, 3);
        static const int wr_ids[1] = { 7 };
        hot(L, iwr, wr_ids, 1);
        return;
    }
    const int kn = tune(0, 3), kd = tune(1, 4);
    if (variant == V_TABLE) {
        /* THE SCORE TABLE ON ONE PAGE (the N64 table players asked for, in204137_02): the DS shows the course
           list over TWO screens, courses 1..9 on the upper and 10..15 + the castle secret stars on the lower.
           Both halves are cut along their empty rows and STACKED at 2/3, upper half first, so it reads
           top to bottom as one list; the back arrow keeps its own piece in the right margin. The dimmed
           castle both screens show is the page's own backdrop (the lower screen whole). */
        L->p[0] = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_C, MENU_AY_M, 0, 0, 1, 1);
        L->p[0].bg = 1;                         /* replaces the upper-screen backdrop added above */
        MenuPiece top = menu_piece_make(SRC_SUB, 0, 10, 256, 174, MENU_AX_C, MENU_AY_T, 0, 2, 2, 3);
        menu_add_piece(L, top);
        MenuPiece low = menu_piece_make(SRC_TOP_PIC, 0, 12, 256, 108, MENU_AX_C, MENU_AY_T, 0, 118, 2, 3);
        low.touch = 1;
        menu_add_piece(L, low);
        const int wide = L->canvas_w >= 360;      /* room beside the stacked lists (21:9 and wider) */
        MenuPiece bk = menu_piece_make(SRC_TOP_PIC, 176, 136, 80, 44, MENU_AX_R, MENU_AY_B, -2, -2, 1, wide ? 1 : 2);
        bk.touch = 1;
        const int ibk = menu_add_piece(L, bk);
        static const int bk_ids[1] = { 20 };
        hot(L, ibk, bk_ids, 1);
        return;
    }
    if (variant == V_DOODLE) {
        MenuPiece p = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_C, MENU_AY_T, 0, tune(3, 4), kn, kd);
        p.touch = 1;
        menu_add_piece(L, p);
        return;
    }
    /* V_FILES and V_OPTIONS: ONE panel size and place for every page, so the picture does not jump between
       file select, file options, scores, copy and erase */
    MenuPiece p = menu_piece_make(SRC_TOP_PIC, 0, 4, 256, 180, MENU_AX_C, MENU_AY_T, 0, tune(3, 4), kn, kd);
    p.touch = 1;
    const int ip = menu_add_piece(L, p);
    if (variant == V_OPTIONS) {
        static const int ids[6] = { 10, 9, 11, 12, 13, 8 };
        hot(L, ip, ids, 6);
        return;
    }
    static const int fids[8] = { 14, 15, 16, 17, 18, 19, 21, 20 };
    hot(L, ip, fids, 8);
}

/* THE KEY MEANING. pressed is one PadData pressed word (DS key bits: A 0x0001, B 0x0002, Start 0x0008,
   right/left/up/down 0x10/0x20/0x40/0x80). The port's confirm (pad A, Space, Enter) arrives as 0x0002;
   the title ROM confirms on A (0x0001) and treats B as back, so confirm moves to 0x0001 and the host
   back key (Backspace / the bound B-equivalent) becomes the ROM's B. */
void menu_title_pad_map(uint16_t *pressed, int host_back)
{
    uint16_t k = *pressed;
    if (k & MENU_PAD_CONFIRM) k = (uint16_t)((k & ~(unsigned)MENU_PAD_CONFIRM) | 0x0001);
    if (host_back) k = (uint16_t)(k | 0x0002);
    *pressed = k;
}
