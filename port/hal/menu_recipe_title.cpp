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
 *   file pages (3..6)    the lower screen whole (the page IS the DS lower screen); see THE FILE PAGES below (run menu1, FILE1).
 * Wide canvases keep the same arrangement centred; nothing is stretched.
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
#include "menu_hostimg.h"
#include "ntr/gx.h"
#include "ntr/ppu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
int port_title_elem_boxes(int *slot, int *id, int *x0, int *y0, int *x1, int *y1, int max);
int port_title_page_state(int *top, int *elem, int *req);
}

namespace {

enum { V_ATTRACT = 1, V_MAIN = 2, V_FILES = 3, V_OPTIONS = 4, V_DOODLE = 5, V_TABLE = 6, V_INTRO = 7, V_TOY = 8 };   /* V_TOY: the face press (top 4, elem 0), FACE1 */

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

enum { CH_NONE = 0, CH_INTRO = 1, CH_RETURN = 2 };
enum { CH_LOGO_POP = 22, CH_BOUNCE_LEN = 40, CH_FACE_FADE = 24, CH_LOGO_OUT = 10, CH_LOGO_IN = 14, CH_PLANK_DUR = 28, CH_PLANK_GAP = 6,
       CH_PLANK_START = 4, CH_FLY = 14, CH_SWAP_END = CH_PLANK_START + 2 * CH_PLANK_GAP + CH_PLANK_DUR, CH_VEIL = 10 };
struct Chore {
    int mode;            /* CH_* */
    int in_fam;          /* the main family (V_INTRO / V_MAIN) is the page */
    int prev_variant;
    int ct;              /* frames since the ROM's iris sprite first showed (-1 = not yet) */
    int fam_t;           /* frames since the family was entered */
    int sw_t;            /* frames since the swap began (-1 = not yet) */
    int face_a;          /* 0..255 */
    int skip;            /* a confirm press asked for the resting state */
    int iris_covered;    /* the iris covered the picture on the last drawn frame */
    int ff2_done;
    int attract_t;       /* the ROM's attract counter as last read */
    int fly_t;           /* ASSET6: frames since the press, while the star flies off the top (state 10 before the iris) */
} g_ch;

/* INT2 (the decide / draw split): the ROM values build() needs, read once per tick by menu_title_facts on the logic thread and carried in
   MenuLayout.facts.blob. build() and everything it calls read this and never a ROM address. */
struct TitleFacts {
    int nbox;                 /* the live element decoder's answer (0 = it said nothing: the dumped boxes stand in) */
    short bx[24][5];          /* id, x0, y0, x1, y1 */
    int cur, prs;             /* rom_cursor(): the cursor element and whether it is being pressed */
    int frame;                /* the title Render counter (data_ov007_0210342c[0]) */
    int vis, s48;             /* the fade manager's iris sprite: shown, scale */
    Chore ch;                 /* the choreography clock as of this tick */
    int toy_e;                /* FACE1: ticks since the press page handed over to the drawing page (top 0 / 1, elem 1) while it still shows our picture */
    int dd_e;                 /* FACE2: ticks since the face press handed over to the drawing page (1 on the first hand-over tick), 0 = not reached from the press (no crossfade) */
    int dd_dead;              /* FACE2: 1 while the drawing page's own picture is not the page yet (the ROM's black hand-over frames): no strokes or buttons are cut from it */
    int out_e;                /* FACE2: ticks since the drawing page was left (1 on the first tick of the next page), 0 = not leaving it */
    int wide3d;               /* FILE2: this tick's 3D frame covered the whole wide frame (the page background was zoomed in the 3D itself: no margin pieces) */
    int yh;                   /* FACE3 (review): the Yoshi hold (element machine 4) after the drawing page: our full-canvas iris covers it too */
    int e3o;                  /* FACE3: ticks since the ROM left the free-draw state for the face tracing (the hand button), 1 on the first tick, 0 = not leaving it */
    int e3;                   /* FACE3: ticks since the ROM entered the free-draw state (element machine 3), 1 on the first tick, 0 = not in it */
    int toy_v;                /* FACE1: the face press page's own frame counter (the page object's +0xc) PLUS ONE while top 4 / elem 0 is up, 0 = not the press page (a zeroed blob is not the press page) */
};
static_assert(sizeof(TitleFacts) <= sizeof(MenuFacts::blob), "TitleFacts must fit MenuFacts.blob");
thread_local const TitleFacts *t_f = 0;     /* set for the length of one build() */
int g_iris_cov_req;                         /* build() saw the picture covered: folded into g_ch.iris_covered by the next chore_step (a one-way flag) */
bool box_of(int id, Box *out);

bool box_of_live(int id, Box *out)
{
    int slot[24], ids[24], x0[24], y0[24], x1[24], y1[24];
    const int n = port_title_elem_boxes(slot, ids, x0, y0, x1, y1, 24);
    for (int k = 0; k < n; ++k)
        if (ids[k] == id) { out->id = id; out->x0 = x0[k]; out->y0 = y0[k]; out->x1 = x1[k]; out->y1 = y1[k]; return true; }
    for (int k = 0; k < 24; ++k)
        if (kBoxes[k].id == id) { *out = kBoxes[k]; return true; }
    return false;
}

bool box_of(int id, Box *out)      /* build()'s view: this tick's facts, else the dumped boxes */
{
    if (t_f)
        for (int k = 0; k < t_f->nbox; ++k)
            if (t_f->bx[k][0] == id) {
                out->id = id; out->x0 = t_f->bx[k][1]; out->y0 = t_f->bx[k][2]; out->x1 = t_f->bx[k][3]; out->y1 = t_f->bx[k][4];
                return true;
            }
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
    if (top == 11) return elem == 0 ? V_ATTRACT : g_last;   /* ASSET5: top 11 with the file machine up (elem 5) is the return from a file page, not the first attract */
    if (top == 10) return V_INTRO;                /* the logo arrives while the buttons come in through the ROM's star iris */
    if (elem == 4 && g_last == V_DOODLE) return V_DOODLE;   /* FACE3: the Yoshi press on the drawing page: the ROM holds the page about 40 ticks before its star iris closes; the page stays ours (paper edge to edge) */
    if (elem == 3) return V_DOODLE;                 /* FACE3: the free-draw state after the pencil (reached only from the drawing page): same page family, its own layout in build_doodle */
    if (elem == 2 && g_last == V_DOODLE) return V_DOODLE;   /* FACE2: the ROM's drawing mode proper (elem 2) is the same page as the face tracing (elem 1) */
    if (elem == 1) return ((top == 0 || top == 1) && g_last == V_TOY) ? V_TOY : V_DOODLE;     /* FACE1: the hand-over frames keep our picture */
    if (elem == 0) {
        if (top == 0) return V_MAIN;
        if (top == 8) return V_OPTIONS;
        if (top == 4) return V_TOY;               /* a press on the face: the head squashes and jumps, the plates leave (FACE1: drawn on the main menu's own wallpaper) */
        if ((top == 1 || top == 5) && (g_last == V_FILES || g_last == V_DOODLE)) return V_MAIN;   /* ASSET5: coming back from a file page: the ROM walks the head in, so
                                                                              this is the main family (face fade, no walk-in of the file page's own head) */
        return g_last;                            /* 1, 6: easing in or out */
    }
    if (top == 2) return V_TABLE;
    if (top == 4) return V_TABLE;                 /* the table loading (4/5): its list already types in, FILE1 */
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

int ltune(int idx, int dflt)         /* SM64DS_MENU_LOGO_TUNE=top,height,dx in canvas units (the PC logo's place) */
{
    static int v[4], have = 0;
    if (!have) {
        have = 1;
        for (int i = 0; i < 4; ++i) v[i] = -9999;
        const char *e = getenv("SM64DS_MENU_LOGO_TUNE");
        for (int i = 0; e && *e && i < 4; ++i) {
            char *q;
            v[i] = (int)strtol(e, &q, 10);
            if (*q != ',') break;
            e = q + 1;
        }
    }
    return v[idx] == -9999 ? dflt : v[idx];
}

}  // namespace


/* ---- THE TITLE'S SPRITE-OBJECT FILTER (run menu1, lane ASSET1) ------------------------------------------
   The lower screen of the title is NOT 2D: engine A has no BG pixels and no sprites there (SM64DS_MSG_COMPOSITE_DEBUG:
   composited 0 px, OAM A 0 placed). Everything on it, the wallpaper, the plates, the wrench, the stars, is a 3D quad
   the ROM submits through func_ov007_020c421c (one call per object per frame). The port arm in that function asks
   this hook whether to draw. SM64DS_TITLE_DRAWTRACE=1 prints each call; SM64DS_TITLE_SKIPK=<hex> skips the calls whose
   per-frame index is a set bit (a probe for naming the objects). */
extern "C" {
extern unsigned char data_020a0e40;   /* the local player's slot */
extern int data_020a0e58[];           /* PadData[i]: {u16 held, u16 pressed} */
extern char *data_ov007_02103360;     /* the title's element-cursor struct (src/func_ov007_020b3708.c) */
}
static inline int *data_020a0e58_ref() { return data_020a0e58; }
static inline int data_020a0e40_ref() { return data_020a0e40; }
extern "C" {
extern int (*g_port_title_draw_skip)(char *obj);
extern int *data_ov007_0210342c;
extern char *data_ov007_02104bd8;     /* the title's screen-fade manager (src/func_ov007_020bf690.c) */
}
namespace {
int g_tk_frame = -1, g_tk_idx = 0;
int g_up_stamp = -1;          /* the title Render counter at which menu_title_up last answered (the composite of that frame) */

/* The wallpaper is dropped from the submit only while the one-screen main menu is the page, decided from the ROM's own
   state at draw time. g_up_stamp proves the recipe ran for the PREVIOUS frame of this same page family (it is not called
   when the setting is off or the layout is stacked), and the page is read live so the first frame of a page change is
   already right. */
bool pc_assets_ready()
{
    return menu_hostimg_ready(MENU_IMG_LOGO) && menu_hostimg_ready(MENU_IMG_BTN_MP) && menu_hostimg_ready(MENU_IMG_BTN_ADV) &&
           menu_hostimg_ready(MENU_IMG_BTN_REC) && menu_hostimg_ready(MENU_IMG_ARROW_UP) && menu_hostimg_ready(MENU_IMG_ARROW_DOWN) &&
           menu_hostimg_ready(MENU_IMG_PATTERN);
}

/* FACE2: the drawing page's two paper sprite objects (ids 0x18 and 0x17) are not submitted on the one-screen drawing page: our own paper is
   drawn, and the 3D layer then holds only the strokes and the two buttons, so each is cut by coverage and placed on its own */
bool doodle_skip_active()
{
    if (!data_ov007_0210342c || g_up_stamp != data_ov007_0210342c[0]) return false;
    int top, elem, req;
    if (!port_title_page_state(&top, &elem, &req)) return false;
    return variant_for(top, elem) == V_DOODLE;
}

bool wall_skip_active()
{
    if (!data_ov007_0210342c || g_up_stamp != data_ov007_0210342c[0]) return false;
    int top, elem, req;
    if (!port_title_page_state(&top, &elem, &req)) return false;
    const int v = variant_for(top, elem);
    if (v == V_TOY || v == V_OPTIONS) return pc_assets_ready();   /* FILE2: the sound / backlight page draws the same moving PC pattern */
    return v == V_MAIN || v == V_INTRO;
}

/* POLISH1A: the file pages' orange cursor arrow. The ROM's cursor object (id 0x21) draws the file_menu_U orange down arrow on the file
   machine's pages (elem 5: select, options, scores pick, copy, erase). While the page is the one-screen file page and the HD arrow
   is ready the sprite is not submitted; its place (DS pixels, the object's own x / y: the arrow's top left is (x - 11, y - 8), the tip
   is the bottom centre) and the title frame it was seen on are kept, and the file page draws the HD arrow there. */
int g_oa_frame = -1, g_oa_x = 0, g_oa_y = 0;
bool file_arrow_up()
{
    if (!data_ov007_0210342c || g_up_stamp != data_ov007_0210342c[0]) return false;
    int top, elem, req;
    if (!port_title_page_state(&top, &elem, &req)) return false;
    return elem != 0 && variant_for(top, elem) == V_FILES && menu_hostimg_ready(MENU_IMG_ARROW_ORANGE);
}

int title_skip(char *o)
{
    if (*(unsigned char *)(o + 0x4e) == 0x21 && file_arrow_up()) {
        g_oa_frame = data_ov007_0210342c[0];
        g_oa_x = ((const int *)o)[1] >> 12;
        g_oa_y = ((const int *)o)[2] >> 12;
        return 1;
    }
    if (data_ov007_0210342c) {
        const char *g = (const char *)data_ov007_0210342c;
        for (int i = 0; i < 3; ++i)
            if (o == *(char *const *)(g + 0x108 + 4 * i)) return wall_skip_active() ? 1 : 0;
    }
    if ((*(unsigned char *)(o + 0x4e) == 0x18 || *(unsigned char *)(o + 0x4e) == 0x17) && doodle_skip_active()) return 1;
    /* ASSET3: the ROM's own cursor arrow (object id 0x21, drawn only while the cursor is shown) is replaced by the host's own pixel
       arrow (art cut from this very object: tools/extract_title_arrow.py), so it is not in the picture on the one-screen main menu */
    if (*(unsigned char *)(o + 0x4e) == 0x21 && wall_skip_active() && menu_hostimg_ready(MENU_IMG_ARROW_UP) && menu_hostimg_ready(MENU_IMG_ARROW_DOWN) &&
        menu_hostimg_ready(MENU_IMG_BTN_MP) && menu_hostimg_ready(MENU_IMG_BTN_ADV) && menu_hostimg_ready(MENU_IMG_BTN_REC)) return 1;   /* only when our arrows will be drawn */
    /* ASSET4: the three plate sprites (object id 0x1b at y >= 100; the wrench is the same id at y 20) are also left out of the
       submit when the host planks are drawn, so the 3D layer holds only the face, star, sparkles and wrench and the face piece can be
       the whole layer (the ROM's plate art no longer shows under the host planks, nothing below the chin is ever in it) */
    if (*(unsigned char *)(o + 0x4e) == 0x1b && (((const int *)o)[2] >> 12) >= 100 && wall_skip_active() && menu_hostimg_ready(MENU_IMG_BTN_MP) &&
        menu_hostimg_ready(MENU_IMG_BTN_ADV) && menu_hostimg_ready(MENU_IMG_BTN_REC)) return 1;
    static int trace = -1;
    static unsigned skipk;
    if (trace < 0) {
        const char *e = getenv("SM64DS_TITLE_DRAWTRACE");
        trace = (e && *e && *e != '0') ? 1 : 0;
#ifndef PORT_ROM_CLEAN      /* the object-skip probe is a proof arm: not in the shipped build */
        e = getenv("SM64DS_TITLE_SKIPK");
        skipk = e ? (unsigned)strtoul(e, 0, 16) : 0;
#endif
    }
    if (!trace && !skipk) return 0;
    const int fr = data_ov007_0210342c ? data_ov007_0210342c[0] : 0;
    if (fr != g_tk_frame) { g_tk_frame = fr; g_tk_idx = 0; }
    const int k = g_tk_idx++;
    static int trlo = -2, trhi = -2;
    if (trlo == -2) {
        trlo = trhi = -1;
        const char *e = getenv("SM64DS_TITLE_TRACEFR");      /* FACE1: a-b = trace every object call of those title frames */
        if (e) { char *q; trlo = (int)strtol(e, &q, 10); trhi = *q == '-' ? (int)strtol(q + 1, 0, 10) : trlo; }
    }
    if (trace && (fr == 300 || fr == 480 || fr == 600 || (fr >= trlo && fr <= trhi))) {
        const int *w = (const int *)o;
        fprintf(stderr, "[tdraw] f%d k%d obj=%p flags=%08x p4=%d p8=%d tex=%04x id=%02x a=%d\n", fr, k, (void *)o, w[0], w[1], w[2],
                (unsigned)*(unsigned short *)(o + 0x44), (unsigned)*(unsigned char *)(o + 0x4e), (w[0x50 / 4] >> 12) & 0xff);
    }
    return k < 32 && ((skipk >> k) & 1);
}
struct TitleHookInit { TitleHookInit() { g_port_title_draw_skip = title_skip; } } g_title_hook_init;
/* The ROM's own cursor on the main menu (src/func_ov007_020b3708.c: the cursor element is the short at the head of the
   struct data_ov007_02103360 points to; 0 = ADVENTURE, 1 = VS, 2 = REC ROOM, 7 = the wrench, 0xff = the cursor is not shown yet: the ROM
   shows it on the first non-B key, src/func_ov007_020b3190.c), and whether it is being pressed: the DS A key (0x0001, which
   menu_title_pad_map writes into the held word) is down, or the page has already taken a choice (top-state request). */
void rom_cursor(int *cursor, int *pressed)
{
    *cursor = -1; *pressed = 0;
    if (!data_ov007_02103360) return;
    const short *p0 = *(const short **)data_ov007_02103360;
    if (!p0 || (p0[0] != 7 && (p0[0] < 0 || p0[0] > 2))) return;      /* 7 = the wrench (ASSET3) */
    *cursor = p0[0];
    int top, elem, req;
    if (port_title_page_state(&top, &elem, &req) && top == 0 && req != -1) *pressed = 1;
    const unsigned short *held = (const unsigned short *)((char *)data_020a0e58_ref() + data_020a0e40_ref() * 4);
    if (*held & 0x0001) *pressed = 1;
}

/* FACE1: THE FACE'S SCALE ON THE MAIN MENU. The head (the whole 3D layer minus the wrench corner) is drawn at kn/kd of the DS layer, its chin
   (source y 134.5, measured on the ROM's own render) kept at canvas y 135.7 and its centre (source x 127.8) on the canvas centre, so a bigger
   head grows upward between the small logo and the wrench and never reaches the planks. SM64DS_FACE_K=n,d picks the step (1,1 / 5,6 / 3,4 / 2,3). */
struct FaceK { int n, d; };
FaceK face_k()
{
    static FaceK k = { 0, 0 };
    if (!k.n) {
        k.n = 5; k.d = 6;
        const char *e = getenv("SM64DS_FACE_K");
        int a = 0, b = 0;
        if (e && sscanf(e, "%d,%d", &a, &b) == 2 && a > 0 && b > 0 && a <= 255 && b <= 255) { k.n = a; k.d = b; }
    }
    return k;
}
inline int face_dx(const FaceK &) { return 0; }
inline int face_dy(const FaceK &k) { return (int)(135.7 - 134.5 * k.n / k.d + 0.5); }     /* canvas units of the layer's source y = 0 */

/* THE MAIN MENU, built from the title's own art objects (run menu1, lane ASSET1; Tango, 10-09: "take the top screen
   and replace the bottom screen's background with it", and the stars stay).
   The lower screen is 3D (see the filter note above). With the wallpaper objects skipped at submit, the picture
   holds only the head, the star and sparkles, the three plates and the wrench, and the 3D coverage plane says
   which pixels those are, so each piece below is cut keyed by coverage: transparent wherever the 3D engine drew
   nothing, so no wallpaper tile, no clear colour and no box comes along. The upper screen (engine B: the logo on
   its sky) is the backdrop, with its "TOUCH TO START" sprites left out (the plates stand where they were), and on
   a wide canvas its sky is carried out to the edges by xfill columns, nothing stretched.
   Source rects are in lower-screen DS pixels and tile the screen: head+star+sparkles (216 x 140, the whole upper
   part; a drag on the face stays inside it), sparkles right of the wrench, the wrench, and the three plates. They
   are placed at the lower screen's own positions on the 4:3 core; tune knobs (SM64DS_TITLE_TUNE) move the head. */
void build_main(MenuLayout *L, bool intro)
{
    const int cw = L->w * L->uden / L->unum;                 /* exact canvas width in units, floored */
    const int side = cw > 256 ? (cw - 256 + 1) / 2 + 2 : 0;  /* the wide margin on each side, with an overlap */
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
    L->no_focus_paint = 1;      /* the ROM cursor, the lit plank and the arrow are the selection (ASSET3) */
    if (menu_hostimg_ready(MENU_IMG_LOGO)) {
        /* ASSET2: the sky is a smooth gradient at the window's own resolution, coloured from the DS logo screen's own sky
           (the mean of its leftmost 16 columns, per row; those columns hold no logo), and the "Super Mario 64 PC" logo is a
           host picture scaled to the window: the DS logo layer is not drawn at all. Both fill / centre on any aspect. */
        MenuPiece gr = menu_piece_make(SRC_SUB, 0, 0, 16, 192, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        gr.bg = 1; gr.hostgrad = 1; gr.layers = 0x0f;
        gr.lden = 1; gr.lnum = 1; gr.fx = 0; gr.fy = 0; gr.fw = (short)L->w; gr.fh = (short)L->h;
        menu_add_piece(L, gr);
        int iw = 1, ih = 1;
        menu_hostimg_size(MENU_IMG_LOGO, &iw, &ih);
        const int lt = ltune(0, 26), lh = ltune(1, 112), ldx = ltune(2, 0);
        const int dh = lh * L->unum / L->uden, dw = (int)((long long)dh * iw / ih);
        MenuPiece lg = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        lg.bg = 1; lg.hostimg = MENU_IMG_LOGO;
        lg.lden = 1; lg.lnum = 1; lg.fw = (short)dw; lg.fh = (short)dh;
        lg.fx = (short)((L->w - dw) / 2 + ldx * L->unum / L->uden); lg.fy = (short)(lt * L->unum / L->uden);
        menu_add_piece(L, lg);
    } else {
    if (side > 0) {
        MenuPiece lf = menu_piece_make(SRC_SUB, 0, 0, 1, 192, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        lf.bg = 1; lf.xfill = 1; lf.fill_w = (short)side; lf.layers = (uint8_t)tune(5, 0x0f);
        menu_add_piece(L, lf);
        MenuPiece rt = menu_piece_make(SRC_SUB, 255, 0, 1, 192, MENU_AX_R, MENU_AY_T, 0, 0, 1, 1);
        rt.bg = 1; rt.xfill = 1; rt.fill_w = (short)side; rt.layers = (uint8_t)tune(5, 0x0f);
        menu_add_piece(L, rt);
    }
    MenuPiece bg = menu_piece_make(SRC_SUB, 0, 0, 256, 192, MENU_AX_C, MENU_AY_M, 0, 0, 1, 1);
    bg.bg = 1;
    bg.layers = (uint8_t)tune(5, 0x0f);
    menu_add_piece(L, bg);
    }
    /* THE INTRO (top-state 10): the ROM opens the lower screen through a black star iris, drawn as 3D polygons with a thin
       dark edge, and the plates and the head arrive inside it. On one screen the logo simply fades in alone and the
       lower screen's art appears with the page itself, so nothing of the iris is drawn. */
    if (intro) return;
    if (tune(6, -1) >= 0 && tune(7, -1) >= tune(6, -1)) {
        /* DEBUG (SM64DS_TITLE_TUNE index 6,7 = first,last engine-B OAM entry): the upper screen's OAM entries alone, keyed on
           their own transparency, drawn at the top of the picture: the proof of the OAM-range plane on a live screen */
        MenuPiece op = menu_piece_make(SRC_SUB, 0, 150, 256, 40, MENU_AX_C, MENU_AY_T, 0, 4, 1, 1);
        op.layers = MENU_L_OBJ; op.oam_on = 1; op.oam_lo = (uint8_t)tune(6, 0); op.oam_hi = (uint8_t)tune(7, 0); op.key_backdrop = 1;
        menu_add_piece(L, op);
    }
    /* the stars and sparkles stay where the ROM puts them on the 4:3 core (a piece anchored C with dx = srcx - 128 + w / 2
       lands at its own lower-screen x); the head is cut with room around it for a stretch, scaled 2/3 and set low over the
       logo's DS like a mascot; the wrench sits in the top right corner of the canvas */
    /* ASSET4: THE FACE IS NEVER CUT. The title's lower 3D layer now holds only the head, the star, the sparkles and the wrench
       (wallpaper, plates and the ROM's cursor arrow are skipped at submit), so the face piece is the WHOLE 256 x 192 layer minus
       the wrench's own 40 x 40 corner (excluded by rect, so the two never overlap): whatever pose the idle animation or the
       press squash takes, the head, cap, ears, chin, star and sparkles are inside the source. It keeps the round 3 scale and
       place (a source x maps to centre - 83.33 + 2x/3 units, y to 46 + 2y/3, exactly the old 156 x 140 head piece's transform),
       and stars and sparkles now share that transform so they stay where the ROM put them around the face. A touch-only piece
       holds the old head rect (the ROM's face toy reads a touch on the head). */
    const int hk = tune(0, 2), hd = tune(1, 3);
    MenuPiece face = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_C, MENU_AY_T, tune(2, -18) + 20, tune(3, 46), hk, hd);
    face.keycover = 1; face.bg = 1; face.has_ex = 1; face.ex.x = 216; face.ex.y = 0; face.ex.w = 40; face.ex.h = 40;
    menu_add_piece(L, face);
    MenuPiece head = menu_piece_make(SRC_TOP_PIC, 20, 0, 156, 140, MENU_AX_C, MENU_AY_T, tune(2, -18), tune(3, 46), hk, hd);
    head.touch = 1; head.nodraw = 1;
    menu_add_piece(L, head);
    MenuPiece wr = menu_piece_make(SRC_TOP_PIC, 216, 0, 40, 40, MENU_AX_R, MENU_AY_T, 0, 0, 1, 1);
    wr.keycover = 1; wr.touch = 1;
    const int iwr = menu_add_piece(L, wr);
    static const int kPlateId[3] = { 1, 0, 2 };                      /* the ROM's element ids: VS, ADVENTURE, REC ROOM */
    static const int kPlankImg[3] = { MENU_IMG_BTN_MP, MENU_IMG_BTN_ADV, MENU_IMG_BTN_REC };
    const int cur = t_f ? t_f->cur : -1, prs = t_f ? t_f->prs : 0;
    const bool planks = menu_hostimg_ready(MENU_IMG_BTN_MP) && menu_hostimg_ready(MENU_IMG_BTN_ADV) && menu_hostimg_ready(MENU_IMG_BTN_REC);
    for (int k = 0; k < 3; ++k) {
        Box bx;
        if (!box_of(kPlateId[k], &bx)) continue;
        if (planks) {
            /* ASSET2: a plank drawn at the window's resolution over the ROM's own box (the 3D plate is not drawn: its
               wallpaper object is skipped at submit and its sprite is not in any piece). 40 units tall (the box height),
               centred on the box, the three never overlap; clicks map proportionally into the box, so they reach the ROM exactly. */
            int iw = 1, ih = 1;
            menu_hostimg_size(kPlankImg[k], &iw, &ih);
            const int bh = bx.y1 - bx.y0;
            const int dh = bh * L->unum / L->uden, dw = (int)((long long)dh * iw / ih);
            const int cxu = (bx.x0 + bx.x1) / 2 - 128, cyu2 = bx.y0 + bx.y1;   /* centre x from 128, centre y * 2 */
            MenuPiece pl = menu_piece_make(SRC_HOSTIMG, bx.x0, bx.y0, bx.x1 - bx.x0, bx.y1 - bx.y0, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
            pl.hostimg = (uint8_t)kPlankImg[k]; pl.touch = 1;
            pl.lden = 1; pl.lnum = 1; pl.fw = (short)dw; pl.fh = (short)dh;
            pl.fx = (short)(L->w / 2 + cxu * L->unum / L->uden - dw / 2);
            pl.fy = (short)(cyu2 * L->unum / (2 * L->uden) - dh / 2);
            if (cur == kPlateId[k]) pl.hl = (uint8_t)(1 | (prs ? 2 : 0));
            const int ip = menu_add_piece(L, pl);
            menu_add_hotspot(L, ip, bx.x0, bx.y0, bx.x1 - bx.x0, bx.y1 - bx.y0, -1);
            continue;
        }
        static const int kPlateX[3] = { 0, 70, 188 }, kPlateW[3] = { 70, 118, 68 };
        MenuPiece pl = menu_piece_make(SRC_TOP_PIC, kPlateX[k], 140, kPlateW[k], 52, MENU_AX_C, MENU_AY_B, kPlateX[k] - 128 + kPlateW[k] / 2, 0, 1, 1);
        pl.keycover = 1; pl.touch = 1;
        const int ip = menu_add_piece(L, pl);
        hot(L, ip, &kPlateId[k], 1);
    }
    static const int wr_ids[1] = { 7 };
    hot(L, iwr, wr_ids, 1);
    /* ASSET3: THE SELECTION ARROW. The ROM's own cursor arrow (a small blue triangle, drawn as a 3D object and skipped at submit
       above) is drawn again from its own art at the window's resolution: pointing down at the selected plank from above, or up at the
       wrench from below, bobbing on the ROM's 8-frame cadence. Pieces here are backdrop-class (no touch): a click goes through to
       the plank under it. */
    if (planks && menu_hostimg_ready(MENU_IMG_ARROW_UP) && menu_hostimg_ready(MENU_IMG_ARROW_DOWN) && (cur == 7 || (cur >= 0 && cur <= 2))) {
        static const int kBob[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };       /* the plank pulse's cadence, in half units */
        const int fr = t_f ? t_f->frame : 0;
        const int bob2 = kBob[fr & 7];
        const int aimg = cur == 7 ? MENU_IMG_ARROW_UP : MENU_IMG_ARROW_DOWN;
        int iw = 1, ih = 1;
        menu_hostimg_size(aimg, &iw, &ih);
        const int au = ltune(3, 18);                                  /* the arrow's width in canvas units (the DS art is 22 x 15 pixels) */
        const int dw = au * L->unum / L->uden, dh = (int)((long long)dw * ih / iw);
        MenuPiece ar = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        ar.bg = 1; ar.hostimg = (uint8_t)aimg; ar.lden = 1; ar.lnum = 1; ar.fw = (short)dw; ar.fh = (short)dh;
        if (cur == 7) {
            Box wb;
            if (!box_of(7, &wb)) wb = kBoxes[7];
            ar.fx = (short)(L->w - (256 - (wb.x0 + wb.x1) / 2) * L->unum / L->uden - dw / 2);
            ar.fy = (short)((wb.y1 * 2 + 2 + bob2) * L->unum / (2 * L->uden));
        } else {
            /* ASSET4 (Tango): ONE arrow, centred on the plank, pointing down. ARROWFIX1: raised 2 units so the flat tip no longer sinks into the
               plank's dark top border and reads as cut off (it bobs from 1 unit above the border to 1 unit into it) */
            Box bx;
            if (box_of(cur, &bx)) {
                ar.fy = (short)(((bx.y0 * 2 - 2 + bob2) * L->unum) / (2 * L->uden) - dh);
                ar.fx = (short)(L->w / 2 + ((bx.x0 + bx.x1) / 2 - 128) * L->unum / L->uden - dw / 2);
            } else {
                ar.fx = -1000;
            }
        }
        if (ar.fx > -1000) menu_add_piece(L, ar);
    }
}


/* ====================================================================================================================
   ASSET5: THE TITLE CHOREOGRAPHY (one-screen mode only). Tango, 10-09: the attract shows the bottom screen as the DS does, the star flies off the
   top, the star iris opens on our menu, the PC logo drops and bounces, the face fades in, the logo fades out and back in small at the top
   left while MP / ADVENTURE / REC ROOM bounce in.

   WHAT THE ROM DOES (measured with SM64DS_TITLE_TL, every frame of the trace is a line; 60 frames a second):
     top-state 11 (attract): the star flies up the lower screen and off its top edge (about frame 52 of a run with no input), the sparkle trail
        follows; the ROM goes on to show the logo on the UPPER screen, which one screen does not show.
     top-state 10: counter t from 0. The lower screen is black (the ROM faded it, RequestScreenFade(3, 10)); sound events at t = 30 and t = 128;
        at t = 60 the fade manager (data_ov007_02104bd8) raises its star sprite (the iris): sprite scale s grows by 1889.5 a frame, the star's
        outer radius is 0.004382 * s DS pixels, it covers the whole lower screen after about 35 frames, and the ROM lets it run 68 frames
        (progress at +60 a frame to 4096), then calls func_ov007_020b16f0 which asks for top-state 1.
     top-state 1 (arrival): 30 frames, the head and plates come in; top-state 0 (the settled main menu) follows.
   The DS logo (upper screen) pops in with a vertical damped bounce: centre y 78 +- 20 DS pixels, period 8 frames, amplitude falling 0.78 every 4
   frames (rigid translation, scale 1, x fixed; measured by template matching 20 frames of the engine-B picture).

   WHAT THIS DOES WITH IT. All timing below is in 60 Hz frames.
     - the attract is the lower screen as it is (V_ATTRACT); in one-screen mode, once the star and its first sparkles are off the top of the
       lower screen (attract counter 56), the port gives the ROM the touch-to-start edge it would have taken from the player (the same path the
       ROM's own skip takes: the upper-screen logo reveal that follows is not on this picture anyway). SM64DS_TITLE_FF=0 turns this and the
       next two off.
     - state 10: the picture is black until the ROM's iris sprite appears; the dead 27 frames of the counter between its t = 31 and t = 58 are skipped
       (t is written once; the t = 30 sound has played; nothing is keyed on a t in between).
     - the iris: this file reads the ROM's own sprite scale every frame, so the pacing is the ROM's, and draws the star clean over the wallpaper
       (menu_compose.cpp draw_iris). Once it covers the picture the ROM's own iris is told it is finished (progress set to its end) so the arrival
       follows at once instead of 33 frames later.
     - the PC logo pops in 22 frames after the iris appears (about two thirds open) with the DS logo's own bounce.
     - the swap starts when the logo has bounced and the ROM is at rest (top-state 0, the face is where it is on the 4:3 screen): the face fades in over
       24 frames (it is never drawn before: no walk in from off screen), the big logo fades out over 10, the small logo fades in at the top left over 14,
       and the planks come up from the bottom edge with an overshoot, 6 frames apart, 28 frames each. Input is live as soon as the ROM is at rest.
       A confirm press during the swap finishes it at once and still goes to the ROM (it shows the cursor on ADVENTURE).
     - a return from a file page (the ROM walks the head in again) skips the iris and the big logo: the small logo is there, the page fades in from
       black over 10 frames, and the same swap runs for the face and the planks.
   ==================================================================================================================== */

int ff_on()
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("SM64DS_TITLE_FF"); v = (e && *e == '0') ? 0 : 1; }
    return v;
}

int ctune(int idx, int dflt)         /* SM64DS_CHORE_TUNE=a,b,c,... integer knobs for timing / look iteration */
{
    static int v[12], have = 0;
    if (!have) {
        have = 1;
        for (int i = 0; i < 12; ++i) v[i] = -9999;
        const char *e = getenv("SM64DS_CHORE_TUNE");
        for (int i = 0; e && *e && i < 12; ++i) {
            char *q;
            v[i] = (int)strtol(e, &q, 10);
            if (*q != ',') break;
            e = q + 1;
        }
    }
    return v[idx] == -9999 ? dflt : v[idx];
}

/* the ROM's iris sprite: visible flag and scale (the fade manager's sprite, offsets +0x94 and +0x48) and its progress object */
bool rom_iris(int *vis, int *s48)
{
    *vis = 0; *s48 = 0;
    const char *F = data_ov007_02104bd8;
    if (!F) return false;
    const char *sub = *(const char *const *)(F + 4);
    if (!sub) return false;
    *vis = *(const int *)(sub + 0x94) != 0;
    *s48 = *(const int *)(sub + 0x48);
    return true;
}

float ease_out_back(float t)         /* 0..1 -> 0..1 with a 9 percent overshoot that settles at 1 */
{
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    const float c1 = 1.70158f * 0.8f, c3 = c1 + 1.f, u = t - 1.f;
    return 1.f + c3 * u * u * u + c1 * u * u;
}

/* one frame of the choreography clock, from menu_title_up (once per DS frame, after the ROM ran its tick) */
void chore_step(int variant, int top, int elem, int req)
{
    Chore &c = g_ch;
    const bool fam = variant == V_INTRO || variant == V_MAIN;
    if (!fam) {
        c.in_fam = 0; c.mode = CH_NONE; c.prev_variant = variant;
        if (variant == V_ATTRACT && data_ov007_0210342c) {
            const char *sp = *(const char *const *)((const char *)data_ov007_0210342c + 8);
            c.attract_t = sp ? *(const int *)(sp + 0xc) : 0;
        }
        return;
    }
    if (!c.in_fam) {              /* entering the main family */
        c.in_fam = 1;
        c.mode = (c.prev_variant == V_ATTRACT) ? CH_INTRO : CH_RETURN;
        c.ct = -1; c.fam_t = 0; c.sw_t = -1; c.face_a = 0; c.skip = 0; c.iris_covered = 0; c.ff2_done = 0; c.fly_t = 0;
        if (hal_menu_trace_on()) fprintf(stderr, "[chore] enter family mode=%s top=%d\n", c.mode == CH_INTRO ? "intro" : "return", top);
    }
    ++c.fam_t;
    if (g_iris_cov_req) { g_iris_cov_req = 0; c.iris_covered = 1; }      /* build() saw the picture covered */
    const bool rest = top == 0 && elem == 0 && req == -1;
    int vis = 0, s48 = 0;
    rom_iris(&vis, &s48);
    if (c.mode == CH_INTRO) {
        if (c.ct >= 0 && !vis && !c.iris_covered) c.iris_covered = 1;      /* the ROM's iris is gone: nothing left to draw */
        if (c.ct < 0) {
            if (top == 10 && !vis) ++c.fly_t;
            if (vis && top == 10) { c.ct = 0; if (hal_menu_trace_on()) fprintf(stderr, "[chore] iris starts (s48 %d)\n", s48); }
            else if (top == 1 || top == 0) c.ct = ctune(0, CH_LOGO_POP) + ctune(1, CH_BOUNCE_LEN);   /* no iris came: the intro is over */
        } else {
            ++c.ct;
        }
    }
    const int bounce_end = ctune(0, CH_LOGO_POP) + ctune(1, CH_BOUNCE_LEN);
    const bool bounced = c.mode == CH_RETURN || c.skip || (c.ct >= bounce_end);
    const bool arriving = c.mode == CH_RETURN && top == 1 && elem == 0;      /* ASSET6: the ROM's head is on its way in */
    if (c.sw_t < 0) {
        if ((rest || arriving) && bounced) {
            c.sw_t = 0;
            if (hal_menu_trace_on()) fprintf(stderr, "[chore] swap starts (ct %d, fam_t %d)\n", c.ct, c.fam_t);
        }
    } else {
        ++c.sw_t;
    }
    if (c.skip && c.sw_t >= 0 && c.sw_t < CH_SWAP_END) c.sw_t = CH_SWAP_END;
    /* the face: faded in once the ROM is at rest and the swap began; faded out quickly when the ROM leaves rest (it walks out) */
    if (c.sw_t < 0) c.face_a = 0;
    else if (rest || arriving) {
        const int step = c.skip || c.sw_t >= CH_SWAP_END ? 255 : 255 / CH_FACE_FADE + 1;
        c.face_a += step; if (c.face_a > 255) c.face_a = 255;
    } else {
        c.face_a -= 40; if (c.face_a < 0) c.face_a = 0;
    }
    c.prev_variant = variant;
}

/* ROM state the choreography writes (SM64DS_TITLE_FF=0 turns all of it off). Called from menu_title_up. */
/* ASSET6: THE ATTRACT WAITS FOR THE PLAYER. The ROM (src/func_ov007_020b0a20.c) plays the star's flight and leaves the attract by itself
   when a save exists (func_ov007_020aebac == 0): the animation ends and it asks for state 10. With no save it picks the other animation
   (the star idling over TOUCH TO START) and waits. Tango asked for the waiting picture whatever the save, so while the attract is up the
   port shows the ROM the empty-save words (func_ov007_020aebac reads bytes +0xb..+0xd and the word +4 of the record at g+0x28), and puts the
   real words back on the frame the player presses (pad key, click or touch) so the ROM's own gate takes that press. Restored too whenever the
   attract is left. Nothing else reads those words on the attract page. */
static unsigned char g_sv_b[3];
static int g_sv_i;
static int g_sv_faked;
static char *g_sv_q;       /* INT2: the record the words were taken from; the words only go back into that same record (a torn-down title scene never gets a stale write) */
static char *sv_rec()
{
    if (!data_ov007_0210342c) return 0;
    char **pp = *(char ***)((char *)data_ov007_0210342c + 0x28);
    return pp ? *pp : 0;
}
static void sv_fake()
{
    char *q = sv_rec();
    if (!q || g_sv_faked) return;
    memcpy(g_sv_b, q + 0xb, 3);
    g_sv_i = *(int *)(q + 4);
    g_sv_q = q;
    if (!g_sv_b[0] && !g_sv_b[1] && !g_sv_b[2] && !g_sv_i) return;     /* already empty: the ROM waits by itself */
    memset(q + 0xb, 0, 3);
    *(int *)(q + 4) = 0;
    g_sv_faked = 1;
    if (hal_menu_trace_on()) fprintf(stderr, "[chore] attract holds: save words hidden from the ROM until a press\n");
}
static void sv_restore()
{
    if (!g_sv_faked) return;
    char *q = sv_rec();
    if (q && q == g_sv_q) { memcpy(q + 0xb, g_sv_b, 3); *(int *)(q + 4) = g_sv_i; }
    g_sv_faked = 0;
    g_sv_q = 0;
    if (hal_menu_trace_on()) fprintf(stderr, "[chore] press: save words given back\n");
}
extern "C" void port_title_press_seen(void) { if (g_ch.attract_t >= 6) sv_restore(); }   /* the ROM's gate reads input from its frame 4 on */     /* sub_screen.cpp: a touch / click is going into the ROM this frame */

void chore_rom_writes(int top, int req)
{
    if (!data_ov007_0210342c) return;
    if (top == 11 && g_ch.attract_t >= 0 && !g_sv_faked && g_ch.attract_t < 8 && g_last == V_ATTRACT) sv_fake();
    else if (top != 11) sv_restore();
    if (!ff_on()) return;
    char *g = (char *)data_ov007_0210342c;
    char *sp = *(char **)(g + 8);
    if (!sp) return;
    int *t = (int *)(sp + 0xc);
    /* ASSET6: a return from a file page runs the ROM's star iris backwards (top-state 5, 69 frames) over a page we show as a short veil: the
       iris is told it is finished once the veil is up, so the head's arrival (top-state 1) follows at once */
    if (top == 5 && g_ch.mode == CH_RETURN && g_ch.fam_t >= CH_VEIL && !g_ch.ff2_done && data_ov007_02104bd8) {
        char *o6 = *(char **)data_ov007_02104bd8;
        if (o6 && *(int *)(o6 + 8) == 1) {
            const int step = *(int *)(o6 + 0x18);
            const int want = 0x1000 - step + 1;
            if (*(int *)(o6 + 0x14) < want) *(int *)(o6 + 0x14) = want;
            g_ch.ff2_done = 1;
            if (hal_menu_trace_on()) fprintf(stderr, "[chore] return: ROM iris progress finished early\n");
        }
    }
    /* ASSET6: leaving a file page the ROM idles 30 frames (state 11, element machine 5) before its star iris closes over the page: skipped once its own 6 frame fade-in is over (an iris asked for while that fade runs is dropped) */
    if (top == 11 && g_elem == 5 && *t >= 9 && *t < 29) { *t = 29; if (hal_menu_trace_on()) fprintf(stderr, "[chore] file page exit: idle counter skipped\n"); }
    if (top == 10 && g_ch.mode == CH_INTRO) {
        /* the ROM's iris starts at t = 60; before it the lower screen is black and nothing is keyed on t after the t = 30 sound */
        /* ASSET6: the star has flown off the top (CH_FLY frames after the press); the counter skips to 29 (the t = 30 sound still plays), then to
           58 once past it, so the iris (t = 60) follows within a few frames */
        if (*t >= ctune(9, CH_FLY) && *t < 29) { *t = 29; if (hal_menu_trace_on()) fprintf(stderr, "[chore] ROM counter t jumped to 29\n"); }
        else if (*t >= 31 && *t < 58) { *t = 58; if (hal_menu_trace_on()) fprintf(stderr, "[chore] ROM counter t jumped to 58\n"); }
        /* once our iris covers the picture the ROM's iris is finished: progress to its last step so the callback asks for the arrival */
        if (g_ch.iris_covered && !g_ch.ff2_done && data_ov007_02104bd8) {
            char *o6 = *(char **)data_ov007_02104bd8;
            if (o6 && *(int *)(o6 + 8) == 1) {
                const int step = *(int *)(o6 + 0x18);
                const int want = 0x1000 - step + 1;
                if (*(int *)(o6 + 0x14) < want) *(int *)(o6 + 0x14) = want;
                g_ch.ff2_done = 1;
                if (hal_menu_trace_on()) fprintf(stderr, "[chore] ROM iris progress finished early\n");
            }
        }
    }
}

/* the DS logo's bounce, measured (above): offset in canvas units after the pop, tb frames since it (down is positive) */
float logo_bounce(int tb)
{
    if (tb < 0) return 0.f;
    const float amp = 21.f * expf(-(tb - 2.f) / 16.1f);
    const float a = (amp > 21.f ? 21.f : amp) * sinf(6.2831853f * tb / 8.f);
    return -a;
}

/* THE MAIN MENU WITH OUR ART (logo, planks, arrows and wallpaper all loaded). intro = the ROM is not at rest yet (top-state 10 / 1). */
/* FACE1: THE FACE PRESS PAGE (top 4, elem 0). The ROM plays its press animation on the head (it squashes, jumps off the top and lands back)
   while growing it to 1.345 times the main menu's size (the red cap measured 74 DS px wide on the first frame, 99.5 from frame 60 on, centre
   x 123.2 throughout, cap top at layer y 27.8 and 24.2; shots in out/FACE1/shots). Drawing the layer at the main menu's own transform would
   make the head pop up 1.345 times; instead the layer is drawn at the main menu's step divided by that growth (a smoothstep over 56 frames
   fitted to the cap widths) and anchored on the cap, so the head keeps the main menu's size and place for the whole animation and the toy's
   rest pose. Only the layer's own transform changes, so the ROM's squash and jump are still its own. */
float toy_grow(int v)
{
    const float t = v <= 0 ? 0.f : v >= 56 ? 1.f : (float)v / 56.f;
    return t * t * (3.f - 2.f * t);
}

/* the dest rect (host pixels) of the whole 3D layer when its head is where the main menu draws it, at growth (1 + 0.345 * growth_t) of the
   ROM's own press animation (see toy_grow); growth_t 1 = the toy's rest pose, which is also the size the drawing page's strokes were
   traced at (measured: the outline of the drawing page laid over the press page's rest pose follows the head's face, hat to chin) */
void toy_dest(const MenuLayout *L, float growth_t, int *ox, int *oy, int *ow, int *oh)
{
    const FaceK fk = face_k();
    const float k0 = (float)fk.n / (float)fk.d, gr = 1.f + 0.345f * growth_t, k = k0 / gr;
    const float ax = 123.3f, ay = 27.8f + (24.2f - 27.8f) * growth_t;                            /* the layer point that stays on the cap */
    const float cxu = (256.f - 256.f * k0) / 2.f + 123.3f * k0 + (float)face_dx(fk);             /* where the main menu puts it (canvas units) */
    const float cyu = (float)face_dy(fk) + 27.8f * k0;
    const float upx = (float)L->unum / (float)L->uden;
    const float hostx0 = (float)L->w / 2.f + (cxu - ax * k - 128.f) * upx, hosty0 = (cyu - ay * k) * upx;
    const int x0 = (int)(hostx0 + 0.5f), y0 = (int)(hosty0 + 0.5f);
    *ox = x0; *oy = y0;
    *ow = (int)(hostx0 + 256.f * k * upx + 0.5f) - x0; *oh = (int)(hosty0 + 192.f * k * upx + 0.5f) - y0;
}

void build_main_pc(MenuLayout *L, bool intro)
{
    Chore c = t_f ? t_f->ch : Chore();      /* a copy: build() never writes the clock */
    const int w = L->w, h = L->h, un = L->unum, ud = L->uden;
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
    L->no_focus_paint = 1;
    auto U = [&](int units) { return units * un / ud; };
    auto Uf = [&](float units) { return (int)(units * (float)un / (float)ud + (units < 0 ? -0.5f : 0.5f)); };
    /* the wallpaper: gradient plus the PC pattern at the OG's scale (64 units a tile wide) */
    {
        MenuPiece wl = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        wl.bg = 1; wl.wall = 1; wl.lden = 1; wl.lnum = 1; wl.fx = 0; wl.fy = 0; wl.fw = (short)w; wl.fh = (short)h;
        const int tu = ctune(5, 64);
        wl.wtw = (short)(tu * un / ud);
        wl.wth = (short)((int)((long long)wl.wtw * 730 / 410));
        wl.wox = (short)((ctune(6, -6) * un) / ud);
        wl.woy = (short)((ctune(7, -11) * un) / ud);
        wl.wink = (uint8_t)ctune(8, 107);
        menu_add_piece(L, wl);
    }
    int iw = 1, ih = 1;
    menu_hostimg_size(MENU_IMG_LOGO, &iw, &ih);
    /* the logos: big (centred, as the main menu used to have it) and small (top left) */
    int big_a = 0, small_a = 0;
    float dy = 0.f;
    if (c.mode == CH_INTRO) {
        const int tb = c.ct - ctune(0, CH_LOGO_POP);
        if (c.ct >= 0 && tb >= 0) {
            big_a = tb >= 3 ? 255 : 255 * (tb + 1) / 4;
            dy = logo_bounce(tb);
        }
        if (c.sw_t >= 0) {
            const int o = c.sw_t;
            if (o < CH_LOGO_OUT) big_a = big_a * (CH_LOGO_OUT - o) / CH_LOGO_OUT; else big_a = 0;
            if (o >= CH_LOGO_OUT) small_a = o >= CH_LOGO_OUT + CH_LOGO_IN ? 255 : 255 * (o - CH_LOGO_OUT + 1) / (CH_LOGO_IN + 1);
        }
    } else {
        small_a = 255;
    }
    if (c.skip) { big_a = 0; small_a = 255; }
    if (big_a > 0) {
        const int lt = ltune(0, 26), lh = ltune(1, 112), ldx = ltune(2, 0);
        const int dh = U(lh), dw = (int)((long long)dh * iw / ih);
        MenuPiece lg = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        lg.bg = 1; lg.hostimg = MENU_IMG_LOGO; lg.nofade = 1; lg.alpha = (uint8_t)(big_a >= 255 ? 0 : big_a);
        lg.lden = 1; lg.lnum = 1; lg.fw = (short)dw; lg.fh = (short)dh;
        lg.fx = (short)((w - dw) / 2 + U(ldx)); lg.fy = (short)(U(lt) + Uf(dy));
        menu_add_piece(L, lg);
    }
    if (small_a > 0) {
        const int lh = ctune(2, 40), mx = ctune(3, 6), my = ctune(4, 6);
        const int dh = U(lh), dw = (int)((long long)dh * iw / ih);
        MenuPiece lg = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        lg.bg = 1; lg.hostimg = MENU_IMG_LOGO; lg.nofade = 1; lg.alpha = (uint8_t)(small_a >= 255 ? 0 : small_a);
        lg.lden = 1; lg.lnum = 1; lg.fw = (short)dw; lg.fh = (short)dh;
        lg.fx = (short)U(mx); lg.fy = (short)U(my);
        menu_add_piece(L, lg);
    }
    if (!intro) {
        /* the face (the whole lower 3D layer minus the wrench corner, round 4's piece) at the swap's alpha */
        const FaceK fk = face_k();
        const int hk = fk.n, hd = fk.d;
        const int tv = t_f ? t_f->toy_v - 1 : -1;      /* FACE1: >= 0 on the face press page */
        const bool toy = tv >= 0;
        const int fa = toy ? 255 : c.face_a;
        if (toy) {
            MenuPiece face = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_C, MENU_AY_T, 0, 0, 1, 1);
            face.fixdest = 1; face.keycover = 1; face.bg = 1; face.has_ex = 1; face.ex.x = 216; face.ex.y = 0; face.ex.w = 40; face.ex.h = 40;
            int x0, y0, fw, fh;
            toy_dest(L, toy_grow(tv), &x0, &y0, &fw, &fh);
            face.fx = (short)x0; face.fy = (short)y0; face.fw = (short)fw; face.fh = (short)fh;
            menu_add_piece(L, face);
            if (tv <= 93) L->snap_save = 1;      /* FACE2: the held frame of the crossfade into the drawing page (the ROM's own fade-out dims the picture from counter 94) */
            MenuPiece head = face;                      /* the page reads touches over the whole layer; the same transform inverts them */
            head.keycover = 0; head.has_ex = 0; head.bg = 0; head.touch = 1; head.nodraw = 1;
            menu_add_piece(L, head);
            {
                /* the wrench leaves with the planks */
                const int wa = tv >= 10 ? 0 : 255 - 255 * tv / 10;
                if (wa > 0) {
                    MenuPiece wr = menu_piece_make(SRC_TOP_PIC, 216, 0, 40, 40, MENU_AX_R, MENU_AY_T, 0, 0, 1, 1);
                    wr.keycover = 1; wr.alpha = (uint8_t)(wa >= 255 ? 0 : wa);
                    menu_add_piece(L, wr);
                }
            }
        } else {
        if (fa > 0) {
            MenuPiece face = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_C, MENU_AY_T, face_dx(fk), face_dy(fk), hk, hd);
            face.keycover = 1; face.bg = 1; face.has_ex = 1; face.ex.x = 216; face.ex.y = 0; face.ex.w = 40; face.ex.h = 40;
            face.alpha = (uint8_t)(fa >= 255 ? 0 : fa);
            menu_add_piece(L, face);
        }
        MenuPiece head = menu_piece_make(SRC_TOP_PIC, 20, 0, 156, 134, MENU_AX_C, MENU_AY_T, face_dx(fk) - (30 * hk + hd / 2) / hd, face_dy(fk), hk, hd);
        head.touch = 1; head.nodraw = 1;
        menu_add_piece(L, head);
        MenuPiece wr = menu_piece_make(SRC_TOP_PIC, 216, 0, 40, 40, MENU_AX_R, MENU_AY_T, 0, 0, 1, 1);
        wr.keycover = 1; wr.touch = 1; wr.alpha = (uint8_t)(fa >= 255 ? 0 : fa);
        wr.nodraw = fa <= 0;
        {
            const int iwr = menu_add_piece(L, wr);
            static const int wr_ids[1] = { 7 };
            hot(L, iwr, wr_ids, 1);
        }
        }
        /* the planks, each rising from the bottom edge with an overshoot, staggered */
        static const int kPlateId[3] = { 1, 0, 2 };
        static const int kPlankImg[3] = { MENU_IMG_BTN_MP, MENU_IMG_BTN_ADV, MENU_IMG_BTN_REC };
        const int cur = t_f ? t_f->cur : -1, prs = t_f ? t_f->prs : 0;
        const bool landed = c.sw_t >= CH_SWAP_END - 4 || c.skip;
        for (int k = 0; k < 3; ++k) {
            Box bx;
            if (!box_of(kPlateId[k], &bx)) continue;
            const int pt = c.sw_t - (CH_PLANK_START + CH_PLANK_GAP * k);
            if (!toy && (c.sw_t < 0 || (pt < 0 && !c.skip))) continue;
            float rise = 0.f;                                       /* canvas units below the resting place */
            int plank_a = 255;
            if (toy) {                                              /* FACE1: the planks leave downward, fading, as the head starts to squash */
                const int u = tv - 2 * k;
                if (u >= 14) continue;
                if (u > 0) { const float e = (float)u / 14.f; rise = e * e * 72.f; plank_a = 255 - 255 * u / 14; }
            } else if (!c.skip && pt < CH_PLANK_DUR) rise = (1.f - ease_out_back((float)pt / CH_PLANK_DUR)) * 72.f;
            int pw = 1, ph = 1;
            menu_hostimg_size(kPlankImg[k], &pw, &ph);
            const int bh = bx.y1 - bx.y0;
            const int dh = bh * un / ud, dw = (int)((long long)dh * pw / ph);
            const int cxu = (bx.x0 + bx.x1) / 2 - 128, cyu2 = bx.y0 + bx.y1;
            MenuPiece pl = menu_piece_make(SRC_HOSTIMG, bx.x0, bx.y0, bx.x1 - bx.x0, bx.y1 - bx.y0, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
            pl.hostimg = (uint8_t)kPlankImg[k]; pl.touch = toy ? 0 : 1;
            pl.lden = 1; pl.lnum = 1; pl.fw = (short)dw; pl.fh = (short)dh;
            pl.fx = (short)(w / 2 + cxu * un / ud - dw / 2);
            pl.fy = (short)(cyu2 * un / (2 * ud) - dh / 2 + Uf(rise));
            if (plank_a < 255) pl.alpha = (uint8_t)(plank_a < 1 ? 1 : plank_a);
            if (landed && cur == kPlateId[k]) pl.hl = (uint8_t)(1 | (prs ? 2 : 0));
            const int ip = menu_add_piece(L, pl);
            if (!toy) menu_add_hotspot(L, ip, bx.x0, bx.y0, bx.x1 - bx.x0, bx.y1 - bx.y0, -1);
        }
        /* the selection arrow, once the planks have landed */
        if (landed && menu_hostimg_ready(MENU_IMG_ARROW_UP) && menu_hostimg_ready(MENU_IMG_ARROW_DOWN) && (cur == 7 || (cur >= 0 && cur <= 2))) {
            static const int kBob[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
            const int fr = t_f ? t_f->frame : 0;
            const int bob2 = kBob[fr & 7];
            const int aimg = cur == 7 ? MENU_IMG_ARROW_UP : MENU_IMG_ARROW_DOWN;
            int aw = 1, ah = 1;
            menu_hostimg_size(aimg, &aw, &ah);
            const int au = ltune(3, 18);
            const int dw = au * un / ud, dh = (int)((long long)dw * ah / aw);
            MenuPiece ar = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
            ar.bg = 1; ar.hostimg = (uint8_t)aimg; ar.lden = 1; ar.lnum = 1; ar.fw = (short)dw; ar.fh = (short)dh;
            if (cur == 7) {
                Box wb;
                if (!box_of(7, &wb)) wb = kBoxes[7];
                ar.fx = (short)(w - (256 - (wb.x0 + wb.x1) / 2) * un / ud - dw / 2);
                ar.fy = (short)((wb.y1 * 2 + 2 + bob2) * un / (2 * ud));
                menu_add_piece(L, ar);
            } else {
                Box bx;
                if (box_of(cur, &bx)) {
                    ar.fy = (short)(((bx.y0 * 2 - 2 + bob2) * un) / (2 * ud) - dh);
                    ar.fx = (short)(w / 2 + ((bx.x0 + bx.x1) / 2 - 128) * un / ud - dw / 2);
                    menu_add_piece(L, ar);
                }
            }
        }
    }
    /* the iris: black outside the star while the ROM's iris is up (and all black before it shows), a veil on a returning page */
    if (c.mode == CH_INTRO && !c.iris_covered && c.ct <= ctune(0, CH_LOGO_POP) + 60) {
        const int vis = t_f ? t_f->vis : 0, s48 = t_f ? t_f->s48 : 0;
        MenuPiece ir = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        ir.bg = 1; ir.iris = 1; ir.lden = 1; ir.lnum = 1; ir.fx = 0; ir.fy = 0; ir.fw = (short)w; ir.fh = (short)h;
        float Rds = 0.f;
        if (vis && c.ct >= 0) Rds = 0.004382f * (float)s48;
        if (c.ct >= 0 && !vis) c.iris_covered = 1;
        const float Rpx = Rds * (float)h / 192.f;
        ir.icx = w / 2;
        ir.icy = (int)((96.f + 0.0864f * Rds) * (float)h / 192.f + 0.5f);
        ir.ir16 = (int)(Rpx * 16.f);
        if (c.iris_covered || (Rpx > 0.f && menu_iris_covers(w, h, (float)ir.icx, (float)ir.icy, Rpx))) { c.iris_covered = 1; g_iris_cov_req = 1; }
        else menu_add_piece(L, ir);
    } else if (c.mode == CH_RETURN && c.fam_t <= CH_VEIL) {
        MenuPiece ir = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        ir.bg = 1; ir.iris = 1; ir.lden = 1; ir.lnum = 1; ir.fx = 0; ir.fy = 0; ir.fw = (short)w; ir.fh = (short)h;
        ir.ir16 = 0;
        ir.alpha = (uint8_t)(255 * c.fam_t / (CH_VEIL + 1));
        if (ir.alpha == 0) ir.alpha = 1;
        menu_add_piece(L, ir);
    }
}

void build_main_sel(MenuLayout *L, bool intro)
{
    if (menu_hostimg_ready(MENU_IMG_LOGO) && menu_hostimg_ready(MENU_IMG_BTN_MP) && menu_hostimg_ready(MENU_IMG_BTN_ADV) &&
        menu_hostimg_ready(MENU_IMG_BTN_REC) && menu_hostimg_ready(MENU_IMG_ARROW_UP) && menu_hostimg_ready(MENU_IMG_ARROW_DOWN) &&
        menu_hostimg_ready(MENU_IMG_PATTERN))
        build_main_pc(L, intro);
    else
        build_main(L, intro);
}

/* FACE2: THE DRAWING PAGE (top 0..2 / elem 1, then elem 2: Mario traces his face, then the player draws). Built from pieces:
     the held main menu frame, crossfaded out while the paper fades in (the first DD_XF ticks after the press page hands over),
     the paper (the ROM's own tile, edge to edge, no side bars),
     the strokes: the 3D layer's pixels, keyed by coverage, at the SAME size and place as the head on the main menu (toy_dest at the
       toy's rest pose; the strokes were traced over that pose), so the outline lands on the head it replaces; a drag on it maps through
       the same transform to the ROM's canvas,
     the Yoshi button and the pencil, cut from the same layer by their own boxes and anchored to the left and right edges at their DS rows
       (4:3: exactly where the DS puts them; wide: the same offsets from the edges), each its own touch piece in the ROM's own box. */
enum { DD_XF = 18 };       /* the crossfade, in DS ticks: 0.3 s */

/* FACE3: the free-draw page's paper: 0 = the drawing page's cream paper stays, 1 = the page's own green paper fades in (SM64DS_PAPER3=0/1) */
int paper3_mode()
{
    static int m = -1;
    if (m < 0) { const char *e = getenv("SM64DS_PAPER3"); m = (e && *e == '0') ? 0 : 1; }
    return m;
}

void build_doodle(MenuLayout *L)
{
    const int w = L->w, h = L->h;
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
    L->no_focus_paint = 1;
    const int e = (t_f && t_f->dd_e > 0) ? t_f->dd_e : 1000;
    const bool live = !(t_f && t_f->dd_dead);
    const bool closing = t_f && t_f->vis && (t_f->e3 > 0 || t_f->yh);    /* FACE3: the Yoshi press: the ROM closes its star iris (polygons in the 3D layer, drawn only in the 4:3 area) */
    if (live && e >= DD_XF && !closing && !(t_f && t_f->yh)) L->snap_save = 1;      /* the held frame of the way out (and of the iris: the last frame before the ROM's iris shows) */
    if (e < DD_XF) {
        MenuPiece sn = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        sn.bg = 1; sn.snap = 1; sn.lden = 1; sn.lnum = 1; sn.fx = 0; sn.fy = 0; sn.fw = (short)w; sn.fh = (short)h;
        menu_add_piece(L, sn);
    }
    {
        MenuPiece pp = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        pp.bg = 1; pp.paper = 1; pp.lden = 1; pp.lnum = 1; pp.fx = 0; pp.fy = 0; pp.fw = (short)w; pp.fh = (short)h;
        const int a = e >= DD_XF ? 255 : 255 * e / DD_XF;
        pp.alpha = (uint8_t)(a >= 255 ? 0 : (a < 1 ? 1 : a));
        menu_add_piece(L, pp);
    }
    const int e3 = t_f ? t_f->e3 : 0;      /* FACE3: the free-draw state (the ROM's element machine 3, after the pencil) */
    if (e3 > 0 && paper3_mode() == 1) {    /* the page's own green paper fades in over the cream one (0.3 s) */
        MenuPiece pg = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        pg.bg = 1; pg.paper = 2; pg.lden = 1; pg.lnum = 1; pg.fx = 0; pg.fy = 0; pg.fw = (short)w; pg.fh = (short)h;
        const int a = e3 >= DD_XF ? 255 : 255 * e3 / DD_XF;
        pg.alpha = (uint8_t)(a >= 255 ? 0 : (a < 1 ? 1 : a));
        menu_add_piece(L, pg);
    }
    const int e3o = t_f ? t_f->e3o : 0;
    if (e3 == 0 && e3o > 0 && e3o < DD_XF && paper3_mode() == 1) {    /* the hand button: the green paper fades back to the drawing page's cream (0.3 s) */
        MenuPiece pg = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        pg.bg = 1; pg.paper = 2; pg.lden = 1; pg.lnum = 1; pg.fx = 0; pg.fy = 0; pg.fw = (short)w; pg.fh = (short)h;
        const int a = 255 - 255 * e3o / DD_XF;
        pg.alpha = (uint8_t)(a < 1 ? 1 : a);
        menu_add_piece(L, pg);
    }
    if (closing && live) {      /* the page is frozen under our own iris: the held frame, then the star over the whole canvas (same star, same radius as the ROM's) */
        MenuPiece sn = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        sn.bg = 1; sn.snap = 1; sn.lden = 1; sn.lnum = 1; sn.fx = 0; sn.fy = 0; sn.fw = (short)w; sn.fh = (short)h;
        menu_add_piece(L, sn);
        MenuPiece ir = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        ir.bg = 1; ir.iris = 1; ir.lden = 1; ir.lnum = 1; ir.fx = 0; ir.fy = 0; ir.fw = (short)w; ir.fh = (short)h;
        const float Rds = 0.004382f * (float)t_f->s48, Rpx = Rds * (float)h / 192.f;
        ir.icx = w / 2;
        ir.icy = (int)((96.f + 0.0864f * Rds) * (float)h / 192.f + 0.5f);
        ir.ir16 = (int)(Rpx * 16.f);
        if (!(Rpx > 0.f && menu_iris_covers(w, h, (float)ir.icx, (float)ir.icy, Rpx))) menu_add_piece(L, ir);
        return;
    }
    if (!live) return;
    static const DsRect kYo = { 0, 132, 60, 60 }, kPen = { 196, 132, 60, 60 }, kBar = { 84, 168, 88, 24 };     /* the buttons' rows of the lower screen: boxes x 4..44 / 212..252, y 148..188, with the slide-in path below */
    int x0, y0, fw, fh;
    toy_dest(L, 1.f, &x0, &y0, &fw, &fh);
    if (e3 > 0) {      /* FACE3: the canvas starts at the head's size and place (the cut from the drawing page is seamless), then grows to the DS canvas's own size, centred, for drawing */
        const float t = (float)(e3 - 8) / 26.f, g = t <= 0.f ? 0.f : t >= 1.f ? 1.f : t * t * (3.f - 2.f * t);
        const float upx = (float)L->unum / (float)L->uden, ffw = 256.f * upx, ffh = 192.f * upx;
        const float fx0 = (float)w * 0.5f - ffw * 0.5f, fy0 = ((float)h - ffh) * 0.5f;
        x0 = (int)((float)x0 + (fx0 - (float)x0) * g + 0.5f); y0 = (int)((float)y0 + (fy0 - (float)y0) * g + 0.5f);
        fw = (int)((float)fw + (ffw - (float)fw) * g + 0.5f); fh = (int)((float)fh + (ffh - (float)fh) * g + 0.5f);
    }
    MenuPiece st = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_C, MENU_AY_T, 0, 0, 1, 1);
    st.fixdest = 1; st.keycover = 1; st.bg = 1; st.has_ex = 1; st.ex = kYo; st.has_ex2 = 1; st.ex2 = kPen;
    if (e3 > 0) { st.has_ex3 = 1; st.ex3 = kBar; }
    if (e3 > 0 && e3 <= 12) { st.ex2.y = 100; st.ex2.h = 92; }      /* the pencil's tick sprite while it slides out */
    st.fx = (short)x0; st.fy = (short)y0; st.fw = (short)fw; st.fh = (short)fh;
    menu_add_piece(L, st);
    MenuPiece tc = st;                           /* the touch twin: the same transform inverts a click on the strokes' area to the ROM's canvas */
    tc.keycover = 0; tc.has_ex = 0; tc.has_ex2 = 0; tc.bg = 0; tc.touch = 1; tc.nodraw = 1;
    tc.has_ex3 = 0;
    const int th = e3 > 0 ? 146 : 130;           /* FACE3: the free-draw canvas reaches down to the button rows (box tops at 148) */
    tc.s.h = (short)th; tc.fh = (short)((fh * th + 96) / 192);     /* ends above the button rows so it never meets the buttons' own touch boxes */
    menu_add_piece(L, tc);
    MenuPiece yo = menu_piece_make(SRC_TOP_PIC, kYo.x, kYo.y, kYo.w, kYo.h, MENU_AX_L, MENU_AY_B, 0, 0, 1, 1);
    yo.keycover = 1; yo.touch = 1;
    menu_add_piece(L, yo);
    MenuPiece pe = menu_piece_make(SRC_TOP_PIC, kPen.x, kPen.y, kPen.w, kPen.h, MENU_AX_R, MENU_AY_B, 0, 0, 1, 1);
    pe.keycover = 1; pe.touch = 1;
    menu_add_piece(L, pe);
    if (e3 > 0) {      /* FACE3: the ink bar, its own piece, centred on the bottom edge (the ROM's box 88..168 x 172..188) */
        MenuPiece br = menu_piece_make(SRC_TOP_PIC, kBar.x, kBar.y, kBar.w, kBar.h, MENU_AX_C, MENU_AY_B, 0, 0, 1, 1);
        br.keycover = 1; br.touch = 1;
        menu_add_piece(L, br);
    }
}

}  // namespace

extern "C" char *data_ov007_02104bd8;     /* the title's screen-fade manager (src/func_ov007_020bf690.c): {obj6, iris sprite, flags, _, request} */

/* ASSET5 measurement (SM64DS_TITLE_TL=1): one line per DS frame with the page, the page's own counter and the fade manager's
   fields (the star iris is the manager's sprite: scale at +0x40/+0x48, drawn while +0x94 is set) */
static void title_timeline(int top, int elem, int req)
{
    static int on = -1;
    if (on < 0) { const char *e = getenv("SM64DS_TITLE_TL"); on = (e && *e && *e != '0') ? 1 : 0; }
    if (!on || !data_ov007_0210342c) return;
    const char *g = (const char *)data_ov007_0210342c;
    const char *sp = *(const char *const *)(g + 8);
    const int t = sp ? *(const int *)(sp + 0xc) : -1;
    int f10 = -1, o8 = -1, o10 = -1, o14 = -1, o18 = -1, o1c = -1, fl = -1, vis = -1, s40 = 0, s48 = 0, rot = 0;
    int px = 0, py = 0, pz = 0;
    const char *F = data_ov007_02104bd8;
    if (F) {
        const char *o6 = *(const char *const *)F, *sub = *(const char *const *)(F + 4);
        f10 = *(const int *)(F + 0x10); fl = *(const int *)(F + 8);
        if (o6) { o8 = *(const int *)(o6 + 8); o10 = *(const int *)(o6 + 0x10); o14 = *(const int *)(o6 + 0x14); o18 = *(const int *)(o6 + 0x18); o1c = *(const int *)(o6 + 0x1c); }
        if (sub) { vis = *(const int *)(sub + 0x94); s40 = *(const int *)(sub + 0x40); s48 = *(const int *)(sub + 0x48); rot = *(const short *)(sub + 0x4c);
                   px = *(const int *)(sub + 0x34); py = *(const int *)(sub + 0x38); pz = *(const int *)(sub + 0x3c); }
    }
    fprintf(stderr, "[tl] f%d top=%d elem=%d req=%d t=%d | fade f10=%d fl8=%d o6{8=%d 10=%d 14=%d 18=%d 1c=%d} iris vis=%d s40=%d s48=%d rot=%d pos=%d,%d,%d\n",
            data_ov007_0210342c[0], top, elem, req, t, f10, fl, o8, o10, o14, o18, o1c, vis, s40, s48, rot, px, py, pz);
}

int menu_title_up(int scene_id)
{
    if (scene_id != 1) { g_up_stamp = -1; return 0; }   /* FILEFIX1: out of the title scene no stale stamp can match the title counter (file_wide_cb runs on every frame's first 3D command) */
    int top, elem, req;
    if (!port_title_page_state(&top, &elem, &req)) return g_last;   /* not built yet: the attract */
    title_timeline(top, elem, req);
    if (top != g_top || elem != g_elem) {
        g_top = top; g_elem = elem;
        if (hal_menu_trace_on()) fprintf(stderr, "[menu] title page top=%d elem=%d req=%d\n", top, elem, req);
    }
    g_last = variant_for(top, elem);
    if (hal_menu_trace_on() && getenv("SM64DS_HEAD_TRACE") && data_ov007_0210342c) {   /* FACE1 probe: the head object's three lerped fields */
        const char *g = (const char *)data_ov007_0210342c;
        const char *m = *(const char *const *)(g + 0x30);
        if (m) fprintf(stderr, "[head] f%d top=%d elem=%d x=%d y=%d z=%d s=%d  f20=%d v=%d\n", data_ov007_0210342c[0], top, elem, *(const int *)(m + 0x14),
                       *(const int *)(m + 0x18), *(const int *)(m + 0x1c), *(const int *)(m + 0x2c), *(const int *)(g + 0x20),
                       *(const int *)(*(const char *const *)(g + 8) + 0xc));
    }
    if (data_ov007_0210342c) g_up_stamp = data_ov007_0210342c[0];
    chore_step(g_last, top, elem, req);
    chore_rom_writes(top, req);
    return g_last;
}

/* INT2: THE TITLE'S ROM FACTS for build(), once per tick on the logic thread after menu_title_up (so the choreography clock is this tick's) */
void menu_title_facts(MenuFacts *f)
{
    TitleFacts tf;
    memset(&tf, 0, sizeof tf);
    int slot[24], ids[24], x0[24], y0[24], x1[24], y1[24];
    const int n = port_title_elem_boxes(slot, ids, x0, y0, x1, y1, 24);
    tf.nbox = n > 0 ? (n > 24 ? 24 : n) : 0;
    for (int k = 0; k < tf.nbox; ++k) {
        tf.bx[k][0] = (short)ids[k]; tf.bx[k][1] = (short)x0[k]; tf.bx[k][2] = (short)y0[k]; tf.bx[k][3] = (short)x1[k]; tf.bx[k][4] = (short)y1[k];
    }
    rom_cursor(&tf.cur, &tf.prs);
    tf.frame = data_ov007_0210342c ? data_ov007_0210342c[0] : 0;
    rom_iris(&tf.vis, &tf.s48);
    tf.ch = g_ch;
    tf.wide3d = ntr::gx_menu_wide_active() ? 1 : 0;
    tf.toy_v = 0;
    {
        static int last_v = -1, last_e = 0, dd_chain = 0, dd_e = 0, was_dd = 0, out_e = 0;
        int top = -1, elem = -1, req = -1;
        const bool st = port_title_page_state(&top, &elem, &req) != 0;
        static int e3c = 0, e3oc = 0;
        const int e3p = e3c;
        e3c = (st && (elem == 3 || (elem == 4 && top == 0 && e3c > 0))) ? e3c + 1 : 0;
        tf.e3 = e3c;
        if (st && e3p > 0 && e3c == 0 && elem == 2) e3oc = 1;
        else if (e3oc > 0 && st && elem == 2) e3oc = e3oc > DD_XF + 1 ? 0 : e3oc + 1;
        else e3oc = 0;
        tf.e3o = e3oc;
        tf.yh = (st && elem == 4 && g_last == V_DOODLE) ? 1 : 0;
        static int yoshi_out = 0;
        yoshi_out = (st && elem == 4) ? 1 : (st && elem == 2) ? 0 : yoshi_out;      /* the Yoshi press: the ROM's own star iris covers the hand-over, no held frame fades over it */
        if (st && top == 4 && elem == 0 && data_ov007_0210342c) {
            const char *sp = *(const char *const *)((const char *)data_ov007_0210342c + 8);
            tf.toy_v = (sp ? *(const int *)(sp + 0xc) : 0);
            if (tf.toy_v < 0) tf.toy_v = 0;
            tf.toy_v += 1;
            last_v = tf.toy_v; last_e = 0;
            dd_chain = 1; dd_e = 0; was_dd = 0; out_e = 0;
        } else if (last_v >= 0 && elem == 1 && (top == 0 || top == 1)) {   /* the ROM's hand-over: its own fade, the drawing page not up yet */
            tf.toy_v = 1001; tf.toy_e = ++last_e;
        } else {
            last_v = -1; last_e = 0;
        }
        /* FACE2: the drawing page chain (g_last is this tick's page) */
        if (g_last == V_DOODLE || (g_last == V_TOY && tf.toy_v >= 1000)) {
            if (dd_chain) { dd_e = tf.toy_v >= 1000 ? tf.toy_e : dd_e + 1; tf.dd_e = dd_e; }
            tf.dd_dead = (tf.toy_v >= 1000 || (elem == 1 && top != 2)) ? 1 : 0;
            was_dd = 1; out_e = 0;
        } else if (was_dd && yoshi_out) {
            was_dd = 0; out_e = 0; dd_chain = 0;
        } else if (was_dd) {
            tf.out_e = ++out_e;
            if (out_e >= DD_XF + 2) { was_dd = 0; out_e = 0; dd_chain = 0; }
        }
    }
    memcpy(f->blob, &tf, sizeof tf);
}

/* ---- THE FILE PAGES (run menu1, lane FILE1) ----------------------------------------------------------------
   File select, file options, the scores pick, copy, erase, the sound / backlight page and the doodle page ARE the DS lower
   screen (Tango, 10-09: "nothing really needs to change at all, just make the bottom screen fit 4:3 and widescreen"). The
   lower screen is engine A's picture (the title's engines are swapped), so each page is that picture whole:
     4:3     one piece, the whole 256 x 192 lower screen at 1/1, which is the whole window. No castle photo, no banner,
             no panel.
     wide    the same piece, centred at its own size (the DS arrangement of plates, back arrow and rows is untouched,
             nothing is stretched), and the margins on both sides carry the page's own background out: the picture's
             leftmost / rightmost column repeated across the margin (xfill), which is the page's colour per row
             (the reflection of the castle, or the SUPER MARIO DS wallpaper, fades into itself at the edge).
   WHAT THE BACKGROUND IS (measured, SM64DS_TITLE_DRAWTRACE + a probe that skipped every submitted sprite object): on the
   file select family the lower screen is a 3D reflection of the castle (two 256 x 256 textures, drawn by the page's own
   model, NOT by func_ov007_020c421c) with the plates and glyphs on top as sprite objects; the wallpaper objects k0..k2
   of the main menu are not drawn there at all. The sound / backlight and doodle pages show the SUPER MARIO DS pattern
   wallpaper objects. Both stay in the picture. TODO(ASSET5): once the moving "Super Mario 64 PC" pattern background of
   the main menu is built, the options and doodle pages should draw it in place of the wallpaper objects, and the
   file pages' margins could take its colour.
   Hotspots are the ROM's own element boxes in lower-screen DS pixels (piece 1:1), so a click lands in the box the ROM
   hit-tests (src/func_ov007_020b46b0.c arithmetic through port_title_elem_boxes).
   THE SCORE TABLE is two screens of text in the ROM: courses 1..9 and the header on the upper screen (engine B), courses
   10..15 and the castle's secret stars on the lower (engine A, a 2D BG layer that types in), over the file plates,
   which the ROM never removes, so on the DS they show through the list. On one page:
     background  the lower screen's own picture (reflection tinted by the page, the back arrow where the ROM puts it),
                 with the three file plates and their star / number glyphs not submitted (the sprite-object filter
                 below) and engine A's text layers lifted out of the picture (lift_owners) so they are drawn as text
                 alone;
     the list    two text pieces stacked at 2/3, the upper screen's text layers alone (keyed on the backdrop colour,
                 own transparency) and the lower screen's lifted text cells (own alpha), one list top to bottom like the
                 N64 table, in the left of the 4:3 area so the back arrow keeps its own place at the lower right.
   Nothing is cut from a finished picture except the background itself. */
extern "C" {
extern int (*g_port_title_draw_skip)(char *obj);
}
namespace {
int ftune(int idx, int dflt)          /* SM64DS_FILE_TUNE=a,b,c,... integer knobs for layout iteration (FILE1) */
{
    static int v[10], have = 0;
    if (!have) {
        have = 1;
        for (int i = 0; i < 10; ++i) v[i] = -9999;
        const char *e = getenv("SM64DS_FILE_TUNE");
        for (int i = 0; e && *e && i < 10; ++i) {
            char *q;
            v[i] = (int)strtol(e, &q, 10);
            if (*q != ',') break;
            e = q + 1;
        }
    }
    return v[idx] == -9999 ? dflt : v[idx];
}

/* the score table's page: its plates (and their glyphs) are not submitted. They sit above DS row 130; the back arrow
   (y 158) and every other object stays. Chained in front of the main menu's filter. */
int (*g_file_prev_skip)(char *obj);
bool table_skip_active()
{
    if (!data_ov007_0210342c || g_up_stamp != data_ov007_0210342c[0]) return false;
    int top, elem, req;
    if (!port_title_page_state(&top, &elem, &req)) return false;
    return variant_for(top, elem) == V_TABLE;
}
int file_skip_inner(char *o);
int file_skip(char *o)
{
    const int r = file_skip_inner(o);
    if (!r) ntr::gx_menu_sprite(1);      /* FILE2: this object is drawn: its triangles are sprite objects, not the page model */
    return r;
}
void file_draw_end() { ntr::gx_menu_sprite(0); }
int file_skip_inner(char *o)
{
    if (table_skip_active()) {
        const int y = *(const int *)(o + 8) >> 12;
        static int tr = -1;
        if (tr < 0) { const char *e = getenv("SM64DS_FILE_TRACE"); tr = (e && *e && *e != '0') ? 1 : 0; }
        if (tr && (data_ov007_0210342c[0] % 120) == 0)
            fprintf(stderr, "[file] table obj id=%02x x=%d y=%d %s\n", (unsigned)*(unsigned char *)(o + 0x4e), *(const int *)(o + 4) >> 12, y,
                    y < 130 ? "skipped" : "kept");
        if (y < 130) return 1;
    }
    return g_file_prev_skip ? g_file_prev_skip(o) : 0;
}
}  // namespace
extern "C" {
extern void (*g_port_title_draw_end)(void);
}
namespace {
/* FILE2: THE WIDE BACKGROUND. On the pages whose lower screen is the castle reflection (file select, file options, scores pick,
   copy, erase, the score table) the 3D layer of a wide window covers the whole frame: the page model (the reflection) is zoomed
   in the 3D about the frame's centre so it fills the width (the top and bottom crop), while the plates, glyphs and the back arrow
   (sprite objects) are drawn by the same pass at their DS places in the centred 4:3 area, at their DS size. Asked once per frame
   by ntr/gx.cpp at the first 3D command, in the ROM's Render, so the page is the ROM's live page. */
bool file_wide_cb(float *zoom, float *dy)
{
    static int off = -1;
    if (off < 0) { const char *e = getenv("SM64DS_FILE_WIDE"); off = (e && *e == '0') ? 1 : 0; }
    if (off || !ntr::widescreen || ntr::active_h <= 0) return false;
    if (!hal_menu_live()) return false;      /* FILEFIX1: no recipe owns the picture (setting off, gameplay): never walk the title's pointers */
    if (!data_ov007_0210342c || g_up_stamp != data_ov007_0210342c[0]) return false;
    int top, elem, req;
    if (!port_title_page_state(&top, &elem, &req)) return false;
    const int v = variant_for(top, elem);
    if (v != V_FILES && v != V_TABLE) return false;
    /* the canvas width in DS units over the width of real page content: the reflection plane is about 292 DS units wide (measured,
       its edge is a dark zigzag outline beyond that), so the page is zoomed exactly as much as keeps the plane's content on both
       edges (288 = a little inside it) and never more; 256 would be the fit-the-width zoom, 9999 no zoom at all (a true wide field,
       which shows the empty plane edge) */
    static int wc = 288, dp = 0, tuned = 0;       /* SM64DS_FILE_WIDE_TUNE=content width in DS units, vertical shift permille of the half height; read once */
    if (!tuned) {
        tuned = 1;
        if (const char *e = getenv("SM64DS_FILE_WIDE_TUNE")) sscanf(e, "%d,%d", &wc, &dp);
    }
    const float cwu = (float)ntr::active_w * 192.0f / (float)(ntr::present_h() > 0 ? ntr::present_h() : 192);   /* the canvas width in DS units (192 rows = one DS screen) */
    float z = cwu / (float)(wc > 64 ? wc : 64);
    if (z < 1.0f) z = 1.0f;
    *zoom = z;
    *dy = (float)dp / 1000.0f;
    return true;
}
struct FileHookInit {
    FileHookInit()
    {
        g_file_prev_skip = g_port_title_draw_skip; g_port_title_draw_skip = file_skip;
        g_port_title_draw_end = file_draw_end;
        ntr::gx_set_menu_wide_cb(file_wide_cb);
    }
} g_file_hook_init;

/* the margins of a wide canvas: the picture's outermost column carried across, one piece a side, under the page */
void file_fills(MenuLayout *L)
{
    const int cw = L->w * L->uden / L->unum;
    const int side = cw > 256 ? (cw - 256 + 1) / 2 + 2 : 0;       /* each margin, with an overlap under the page */
    if (side <= 0) return;
    /* source width 0: the draw maps every margin pixel to ONE host column of the edge DS column (width 1 would spread the margin
       over that DS column's four host columns in vertical steps) */
    MenuPiece lf = menu_piece_make(SRC_TOP_PIC, 0, 0, 0, 192, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
    lf.bg = 1; lf.xfill = 1; lf.fill_w = (short)side;
    menu_add_piece(L, lf);
    MenuPiece rt = menu_piece_make(SRC_TOP_PIC, 255, 0, 0, 192, MENU_AX_R, MENU_AY_T, 0, 0, 1, 1);
    rt.bg = 1; rt.xfill = 1; rt.fill_w = (short)side;
    menu_add_piece(L, rt);
}

/* FILE2: the moving PC pattern wallpaper of the main menu (gradient + the "Super Mario 64 PC" pattern, the OG's scale and drift), full canvas */
void add_pc_wall(MenuLayout *L)
{
    const int un = L->unum, ud = L->uden;
    MenuPiece wl = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
    wl.bg = 1; wl.wall = 1; wl.lden = 1; wl.lnum = 1; wl.fx = 0; wl.fy = 0; wl.fw = (short)L->w; wl.fh = (short)L->h;
    const int tu = ctune(5, 64);
    wl.wtw = (short)(tu * un / ud);
    wl.wth = (short)((int)((long long)wl.wtw * 730 / 410));
    wl.wox = (short)((ctune(6, -6) * un) / ud);
    wl.woy = (short)((ctune(7, -11) * un) / ud);
    wl.wink = (uint8_t)ctune(8, 107);
    menu_add_piece(L, wl);
}

void file_pages_build(int variant, MenuLayout *L)
{
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
    /* FILE2: the sound / backlight page: the ROM's wallpaper objects are not submitted (wall_skip_active), the moving PC pattern is
       drawn full width under the page's own 3D objects (the head, the sliders, the plates), cut by the 3D coverage so nothing
       but those objects comes along. The reflection pages with a wide 3D frame already carry their background to the edges. */
    const bool pat = variant == V_OPTIONS && pc_assets_ready();
    if (pat) add_pc_wall(L);
    else if (!(t_f && t_f->wide3d)) file_fills(L);
    /* the page: the whole lower screen, 1/1, centred (4:3: the whole window) */
    MenuPiece pg = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_C, MENU_AY_M, 0, 0, 1, 1);
    pg.bg = 1; pg.touch = 1;
    if (pat) pg.keycover = 1;
    if (t_f && t_f->wide3d) pg.nodraw = 1;      /* FILE2: the wide 3D frame already is the picture, in place: this piece only maps clicks */
    const int ip = menu_add_piece(L, pg);
    if (pat) {
        /* the page's own 2D layer (the SOUND and BACKLIGHT labels, the star) is lifted out of the picture and drawn over the 3D objects,
           in place, with its own transparency */
        L->lift_owners = MENU_L_BG1 | MENU_L_BG2 | MENU_L_BG3 | MENU_L_OBJ;
        MenuPiece tx = menu_piece_make(SRC_TOP_2D, 0, 0, 256, 192, MENU_AX_C, MENU_AY_M, 0, 0, 1, 1);
        tx.layers = L->lift_owners;
        menu_add_piece(L, tx);
    }
    if (variant == V_TABLE) {
        L->lift_owners = (uint8_t)ftune(0, MENU_L_BG1 | MENU_L_BG2 | MENU_L_BG3 | MENU_L_OBJ);
        const int kn = ftune(5, 2), kd = ftune(6, 3);
        const int us = ftune(1, 10), uh = ftune(2, 176), ls = ftune(3, 12), lh = ftune(4, 108);
        const int dx = ftune(7, -39);                         /* the piece's centre from the canvas centre: the text sits at the left of the 4:3 area */
        const int dy0 = ftune(8, 0);
        MenuPiece up = menu_piece_make(SRC_SUB, 0, us, 256, uh, MENU_AX_C, MENU_AY_T, dx, dy0, kn, kd);
        up.layers = (uint8_t)ftune(9, MENU_L_BG1); up.key_backdrop = 1;   /* BG1 is the text; engine B's OBJ layer holds a few stray sprite pixels beside two rows (measured) */
        menu_add_piece(L, up);
        MenuPiece lo = menu_piece_make(SRC_TOP_2D, 0, ls, 256, lh, MENU_AX_C, MENU_AY_T, dx, dy0 + (uh * kn + kd - 1) / kd, kn, kd);   /* rounded up so the two never overlap */
        lo.layers = L->lift_owners;
        menu_add_piece(L, lo);
        static const int bk_ids[1] = { 20 };
        hot(L, ip, bk_ids, 1);
        return;
    }
    if (variant == V_DOODLE || variant == V_TOY) return;
    if (variant == V_OPTIONS) {
        static const int ids[6] = { 10, 9, 11, 12, 13, 8 };
        hot(L, ip, ids, 6);
        return;
    }
    /* POLISH1A: the HD orange cursor arrow where the ROM's sprite was (kept this frame or the one before: the page and the sprite are
       submitted by the same Render), same 22 x 15 DS footprint, tip anchored at the bottom centre; drawn over the plates */
    if (variant == V_FILES && t_f && g_oa_frame >= 0 && t_f->frame - g_oa_frame <= 1 &&
        menu_hostimg_ready(MENU_IMG_ARROW_ORANGE)) {
        const int un = L->unum, ud = L->uden;
        const int dw = 22 * un / ud, dh = 15 * un / ud;
        MenuPiece ar = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        ar.bg = 1; ar.hostimg = MENU_IMG_ARROW_ORANGE; ar.lden = 1; ar.lnum = 1; ar.fw = (short)dw; ar.fh = (short)dh;
        ar.fx = (short)(L->w / 2 + (g_oa_x - 11 - 128) * un / ud);
        ar.fy = (short)((g_oa_y - 8) * un / ud + (L->h - 192 * un / ud) / 2);
        menu_add_piece(L, ar);
    }
    static const int fids[8] = { 14, 15, 16, 17, 18, 19, 21, 20 };
    hot(L, ip, fids, 8);
}
}  // namespace

namespace {
struct TitleFactsScope { TitleFactsScope(const TitleFacts *f) { t_f = f; } ~TitleFactsScope() { t_f = 0; } };
}

static void menu_title_build_impl(int variant, MenuLayout *L);
/* INT2: the attract and the first frames of the intro are pure black edge to edge: the side areas of a wide canvas would otherwise show the
   ROM's upper-screen backdrop (a lighter blue-grey) around the black 4:3 lower screen. A full-canvas black veil goes under the picture. */
static void add_black_under(MenuLayout *L)
{
    MenuPiece bk = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
    bk.bg = 1; bk.iris = 1; bk.lden = 1; bk.lnum = 1; bk.fx = 0; bk.fy = 0; bk.fw = (short)L->w; bk.fh = (short)L->h;
    bk.ir16 = 0;
    menu_add_piece(L, bk);
}

void menu_title_build(int variant, MenuLayout *L)
{
    TitleFactsScope scope((const TitleFacts *)L->facts.blob);
    menu_title_build_impl(variant, L);
    if (t_f && t_f->out_e > 0 && t_f->out_e < DD_XF && variant != V_DOODLE && (variant == V_MAIN || variant == V_TOY)) {     /* FACE2: the drawing page's last frame fades out over the page that follows */
        MenuPiece sn = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
        sn.bg = 1; sn.snap = 1; sn.lden = 1; sn.lnum = 1; sn.fx = 0; sn.fy = 0; sn.fw = (short)L->w; sn.fh = (short)L->h;
        const int a = 255 - 255 * t_f->out_e / DD_XF;
        sn.alpha = (uint8_t)(a >= 255 ? 0 : (a < 1 ? 1 : a));
        menu_add_piece(L, sn);
    }
}

static void menu_title_build_impl(int variant, MenuLayout *L)
{
    if (variant == V_ATTRACT) {
        add_black_under(L);
        MenuPiece p = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_C, MENU_AY_M, 0, 0, 1, 1);
        p.touch = 1; p.key_backdrop = 1;
        menu_add_piece(L, p);
        L->backdrop = MENU_BACKDROP_TOP_AS_IS;
        L->stage = MENU_STAGE_POST_FADE;
        return;
    }
    if (variant == V_INTRO && t_f && t_f->ch.mode == CH_INTRO && t_f->ch.ct < 0 && !getenv("SM64DS_CHORE_BLACK")) {
        add_black_under(L);
        /* ASSET6: the first frames of the ROM's state 10 are the lower screen going out (the star leaves, the screen fades to black): shown
           as the ROM draws them until its iris sprite appears */
        MenuPiece p = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_C, MENU_AY_M, 0, 0, 1, 1);
        p.touch = 1; p.key_backdrop = 1;
        menu_add_piece(L, p);
        L->backdrop = MENU_BACKDROP_TOP_AS_IS;
        L->stage = MENU_STAGE_POST_FADE;
        /* the star: the ROM takes its own off the picture on the press frame, so ours (cut from the ROM's render, res/menu/title_star.png)
           takes its place and flies up off the top edge, accelerating (the ROM's flight measured 16 frames from the middle of the screen
           to off the top) */
        if (menu_hostimg_ready(MENU_IMG_STAR) && t_f->ch.fly_t >= 0) {
            const int un = L->unum, ud = L->uden;
            const float ft = (float)t_f->ch.fly_t;
            const float cy = 91.6f - 0.55f * ft * ft;                  /* DS pixels, the star's centre */
            int sw = 1, sh = 1;
            menu_hostimg_size(MENU_IMG_STAR, &sw, &sh);
            const int dw = 25 * un / ud, dh = (int)((long long)dw * sh / sw);
            if (cy > -16.f) {
                MenuPiece st = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
                st.bg = 1; st.hostimg = MENU_IMG_STAR; st.nofade = 1; st.lden = 1; st.lnum = 1; st.fw = (short)dw; st.fh = (short)dh;
                st.fx = (short)(L->w / 2 + (int)((127.6f - 128.f) * (float)un / (float)ud) - dw / 2);
                st.fy = (short)((int)(cy * (float)un / (float)ud) - dh / 2 + (L->h - 192 * un / ud) / 2);
                menu_add_piece(L, st);
            }
        }
        return;
    }
    if (variant == V_DOODLE || (variant == V_TOY && t_f && t_f->toy_v >= 1000)) {   /* FACE2: the hand-over frames are the crossfade, not a main menu with a dip */
        build_doodle(L);
        return;
    }
    if (variant == V_MAIN || variant == V_INTRO || (variant == V_TOY && t_f && t_f->toy_v > 0 && pc_assets_ready())) {
        build_main_sel(L, variant == V_INTRO);
        return;
    }
    file_pages_build(variant, L);
}

/* THE KEY MEANING. pressed is one PadData pressed word (DS key bits: A 0x0001, B 0x0002, Start 0x0008,
   right/left/up/down 0x10/0x20/0x40/0x80). The port's confirm (pad A, Space, Enter) arrives as 0x0002;
   the title ROM confirms on A (0x0001) and treats B as back, so confirm moves to 0x0001 and the host
   back key (Backspace / the bound B-equivalent) becomes the ROM's B. */
extern "C" {
extern unsigned char data_020a0e40;   /* the local player's slot */
extern int data_020a0e58[];           /* PadData[i]: {u16 held, u16 pressed} */
}

/* ASSET3: THE ROM CURSOR, STARTED AND FOLLOWING THE MOUSE.
   The ROM shows its cursor on the first non-B key and starts it on ADVENTURE (src/func_ov007_020b3190.c: it writes the target
   element 0 into the cursor struct's second short; the cursor machinery of func_ov007_020b2e64 does the rest). On the one-screen main
   menu the port does that same write once the arrival has settled, so ADVENTURE is selected without a first press. A mouse hover
   (hal_menu_hover_box) writes the hovered element the same way, so the ROM's cursor, the lit plank and the arrow always name the
   element a confirm will press. 1 = VS (MP), 0 = ADVENTURE, 2 = REC ROOM, 7 = the wrench. Only on the settled main page with no
   choice taken. */
static void title_cursor_assist()
{
    static int stable;
    static unsigned hov_seen;
    int top, elem, req;
    if (!data_ov007_02103360 || !port_title_page_state(&top, &elem, &req) || top != 0 || elem != 0 || req != -1) { stable = 0; return; }
    short *p0 = *(short **)data_ov007_02103360;
    if (!p0) return;
    if (stable < 20) { ++stable; return; }
    int hx, hy;
    unsigned st;
    if (hal_menu_hover_box(&hx, &hy, &st) && st != hov_seen) {
        hov_seen = st;
        static const int ids[4] = { 1, 0, 2, 7 };
        for (int k = 0; k < 4; ++k) {
            Box b;
            if (box_of_live(ids[k], &b) && b.x0 == hx && b.y0 == hy) {
                if (p0[0] != ids[k]) p0[1] = (short)ids[k];
                return;
            }
        }
    }
    if (p0[0] == 0xff) p0[1] = 0;
}

void menu_title_pad_map(uint16_t *pressed, int host_back)
{
    title_cursor_assist();
    /* ASSET5: THE ATTRACT'S TOUCH-TO-START, given by the port once the star is off the top of the lower screen (counter 56; SM64DS_TITLE_FF=0
       turns it off), for one frame as the DS A key edge the ROM's own skip reads; and a confirm during the swap finishes the choreography. */
    int inject = 0;
    {
        int top, elem, req;
        if (port_title_page_state(&top, &elem, &req) && top == 11 && g_sv_faked && g_ch.attract_t >= 6 && (*pressed & (MENU_PAD_CONFIRM | 0x0001 | 0x0008))) {
            sv_restore();
            inject = 1;      /* the held word gets the A edge too, so a Start / Enter / Space press is an edge the ROM's gate reads */
        }
        if (g_ch.in_fam && g_ch.sw_t < CH_SWAP_END && ((*pressed & MENU_PAD_CONFIRM) || g_ch.skip)) {
            if (!g_ch.skip && hal_menu_trace_on()) fprintf(stderr, "[chore] confirm during the choreography: finishing it\n");
            g_ch.skip = 1;
        }
    }
    /* The ROM's key record (src/func_ov007_020b3190.c: `*(u16*)p & ~*(u16*)(p+2)`, and the repeat counters of
       func_ov007_020b2e64) is built from the HELD word, so the pressed word alone is not enough: the held
       word moves 0x0002 -> 0x0001 too, and a host back is a one-frame B in it. */
    unsigned short *held = (unsigned short *)((char *)data_020a0e58 + data_020a0e40 * 4);
    unsigned short h = *held;
    /* ASSET6: PAD B (the port's punch binding, DS A bit 0x0001 natively; pad A arrives as 0x0002) is the back arrow on every page that has one
       (file select, file options, scores, copy / erase, doodle): the ROM has only a touch back, so it is the same host back Backspace gives.
       On the main page and the attract 0x0001 stays a confirm. */
    if ((g_last == V_FILES || g_last == V_OPTIONS || g_last == V_TABLE || g_last == V_DOODLE || g_last == V_TOY) && ((*pressed | h) & 0x0001)) {
        if (*pressed & 0x0001) host_back = 1;
        *pressed = (uint16_t)(*pressed & ~0x0001u);
        h = (unsigned short)(h & ~0x0001u);
    }
    /* FACE3: the free-draw page has no pad handling in the ROM (touch only): the back key (Backspace, pad B) taps its Yoshi button, the ROM's own box 6, which leaves for the main menu */
    if (host_back && g_last == V_DOODLE && g_elem == 3) {
        Box b;
        if (box_of_live(6, &b)) hal_menu_touch_queue_press((b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2);
    }
    if (h & MENU_PAD_CONFIRM) h = (unsigned short)((h & ~(unsigned)MENU_PAD_CONFIRM) | 0x0001);
    if (inject) h = (unsigned short)(h | 0x0001);
    if (host_back) h = (unsigned short)(h | 0x0002);
    *held = h;
    uint16_t k = *pressed;
    if (k & MENU_PAD_CONFIRM) k = (uint16_t)((k & ~(unsigned)MENU_PAD_CONFIRM) | 0x0001);
    if (host_back) k = (uint16_t)(k | 0x0002);
    *pressed = k;
}
