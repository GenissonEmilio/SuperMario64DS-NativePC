/* menu_recipe_pause.cpp -- the one-screen PAUSE menu recipe (run menu1, lane PAUSE1).
 *
 * WHAT THE PLAYER GETS (Tango's layout, 10-09). The level stays dimmed behind (the ROM itself dims it
 * while the pause is up: Stage::PS_Init, src/_ZN5Stage7PS_InitEv.cpp:70-111, both engines BLDY 7).
 * LEFT column: the ROM's own top-screen text (course name, star row, coin record; engine A BG3 + OBJ),
 * and under it, in the castle when the course card pages, the ROM's own paging arrows. RIGHT column: the
 * ROM's own bottom-screen plates stacked (Continue / Controller Modes / Options / Quit or Exit Course;
 * engine B BG1 plates + BG0 labels). Nothing is redrawn: the ROM keeps running unchanged and this recipe
 * only says where its pieces land on one picture.
 *
 * STATES (data_0209f248 = Stage pause state; src/_ZN5Stage9PS_UpdateEv.cpp and atlas section 1):
 *   variant 1 castle main (state 0 init, 1)      plates drawn y 24/64/104/144 (touch boxes y 32/72/112/152, 8 lower, as the ROM has them), paging arrows when !data_0209f2f0
 *   variant 2 course main (states 2 init, 3)     plates y 24/64/104/144
 *   variant 3 controller modes (7, 8)            top diagram + 3 mode rows + OK + Back
 *   variant 4 controller modes via Select        same, no Back (data_0209f218 != 0: PS_Init :94)
 *   variant 5 options (9, 0xa, 0xb)              sound row + backlight row + OK + Back
 *   variant 6 save page (0xc, 0xd)               plates y 40/80/120 (Continue / Save / Don't Save)
 *   variant 7 saving status (0xe, 0xf)           the status box, no controls
 *   variant 8 don't save confirm (0x10, 0x11)    YES / NO
 *   variant 9 VS pause (4 init, 5)               plates y 40/80/120 (Continue / Exit / Title)
 * Leaving states (6, 0x12, 0x13) keep the last variant up (hysteresis). Every other transit state is a
 * page-builder frame that maps to the variant it builds.
 *
 * DRIVER MIXED (hal_menu_pad in menu_compose.cpp): up/down move the ROM's own plate cursor through
 * Stage::UpdateMenuButtons exactly as hal_lc_menu_pad does (so the highlight is the ROM's palette bank);
 * left/right press the paging arrows as a synthesised touch; A is the ROM's confirm. The touch-only pages
 * (3, 4, 5) use the core's focus + synthesised touch, and in netplay everything does.
 *
 * ROM EVIDENCE for every rect below is the touch box in PS_Update (cited per table). No English widths:
 * the OK button box depends on the language exactly as PS_Update picks it (GetOwnerLanguage 5 / 3 / other).
 */
#include "menu_compose.h"

#include <stdio.h>
#include <string.h>

extern "C" {
extern unsigned char data_0209f248;    /* the pause state PS_Update is in */
extern unsigned char data_0209f1ec;    /* the state it goes to next */
extern unsigned char data_0209f2c4;    /* 0 = playing, else the pause is up (Stage::Behavior :206-243) */
extern unsigned char data_0209f20c;    /* level-clear owns the game */
extern unsigned char data_0209f2d8;    /* game mode: 1 = VS */
extern unsigned char data_0209f2f0;    /* 1 = no stars yet: the ROM hides the course arrows */
extern unsigned char data_0209f218;    /* 1 = opened by Select: controller mode only, no Back */
extern unsigned char data_0209f2e0;    /* the selected plate */
extern unsigned char data_0209f2b4;    /* how many plates */
extern unsigned char data_0209f22c;    /* press hold ticks */
extern unsigned char data_0209f244;    /* press flash ticks */
extern unsigned char data_0209f300;    /* input lock after open */
extern unsigned char data_0209f2c8;    /* the course the card shows */
extern unsigned char data_0209f2dc;    /* the highlighted controller mode */
extern unsigned char data_0209f234;    /* the sound mode */
extern unsigned char data_0209f2ec;    /* the backlight: 0 on, 1 off */
int GetOwnerLanguage(void);
void _ZN5Stage17UpdateMenuButtonsEb(int b);
void func_02012790(int id);
}

namespace {

enum { V_CASTLE = 1, V_COURSE, V_CTL, V_CTL_SEL, V_OPT, V_SAVE, V_STATUS, V_NOSAVE, V_VS };

/* ---- the variant, with hysteresis over the leaving states ---------------- */

int g_last_variant;
int g_cursor_shown;

int pause_variant(void)
{
    if (!data_0209f2c4 || data_0209f20c) return 0;
    const unsigned st = data_0209f248;
    int v = 0;
    switch (st) {
    case 0: case 1: v = V_CASTLE; break;
    case 2: case 3: v = V_COURSE; break;
    case 4: case 5: v = V_VS; break;
    case 7: case 8: v = data_0209f218 ? V_CTL_SEL : V_CTL; break;
    case 9: case 0xa: case 0xb: v = V_OPT; break;
    case 0xc: case 0xd: v = V_SAVE; break;
    case 0xe: case 0xf: v = V_STATUS; break;
    case 0x10: case 0x11: v = V_NOSAVE; break;
    default: v = g_last_variant; break;       /* 6, 0x12, 0x13: leaving, ride through */
    }
    return v;
}

int pause_up(int scene_id)
{
    int v = 0;
    /* the Stage is scene 3; a level boot that never runs the scene thunk answers -1 (hal_menu_hook_h1), and
       the stacked layout is MENU_SCENE_GATED: no recipe */
    if (scene_id == 3 || scene_id == -1) v = pause_variant();
    if (v != g_last_variant) g_cursor_shown = 0;
    g_last_variant = v;
    if (hal_menu_trace_on()) {      /* SM64DS_MENU_TRACE: every change of the ROM's pause words, for the proof rows */
        static int last[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        const int now[10] = { v, data_0209f2c4, data_0209f248, data_0209f1ec, data_0209f2c8, data_0209f2e0, data_0209f218,
                              data_0209f2dc, data_0209f234, data_0209f2ec };
        if (memcmp(now, last, sizeof now) != 0) {
            memcpy(last, now, sizeof last);
            fprintf(stderr, "[pause] variant=%d f2c4=%d f248=%d f1ec=%d course=%d sel=%d select=%d mode=%d sound=%d backlight=%d\n", now[0], now[1],
                    now[2], now[3], now[4], now[5], now[6], now[7], now[8], now[9]);
        }
    }
    return v;
}

/* ---- scale choice --------------------------------------------------------- */

struct Sc { int n, d; };
const Sc kSc[4] = { { 1, 1 }, { 3, 4 }, { 2, 3 }, { 1, 2 } };

int ceil_mul(int w, const Sc &s) { return (w * s.n + s.d - 1) / s.d; }
int floor_mul(int w, const Sc &s) { return (w * s.n) / s.d; }
int rank(const Sc &s) { return s.n * 12 / s.d; }   /* 12, 9, 8, 6 */

/* Two columns of width wa and wb on a canvas `cw` units wide: the largest whole-step scales that keep
   `need` units of margin in total. The best pair has the largest smaller scale, then the larger second
   column (the controls), then the larger first. */
bool fit(int wa, int wb, int cw, int need, Sc *a, Sc *b)
{
    int best = -1, bm = 0, bb = 0, ba = 0;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            if (ceil_mul(wa, kSc[i]) + ceil_mul(wb, kSc[j]) + need > cw) continue;
            const int m = rank(kSc[i]) < rank(kSc[j]) ? rank(kSc[i]) : rank(kSc[j]);
            if (best < 0 || m > bm || (m == bm && (rank(kSc[j]) > bb || (rank(kSc[j]) == bb && rank(kSc[i]) > ba)))) {
                best = 1; bm = m; bb = rank(kSc[j]); ba = rank(kSc[i]);
                *a = kSc[i]; *b = kSc[j];
            }
        }
    return best >= 0;
}

struct Hs { short x, y, w, h; int8_t rom; };

/* the left piece (top-screen stuff) and the right piece (bottom-screen stuff) */
struct Cols {
    Sc a, b;
    int marg;       /* canvas units from each canvas edge to its column */
    int wa_px, wb_px;
};

Cols make_cols(const MenuLayout *L, int wa, int wb)
{
    Cols C;
    Sc a = kSc[3], b = kSc[3];
    if (!fit(wa, wb, L->canvas_w, 12, &a, &b)) fit(wa, wb, L->canvas_w, 0, &a, &b);
    C.a = a; C.b = b;
    C.wa_px = ceil_mul(wa, a);
    C.wb_px = ceil_mul(wb, b);
    int free_u = L->canvas_w - C.wa_px - C.wb_px;
    int m = free_u / 3;
    if (m > 40) m = 40;
    if (m < 0) m = 0;
    C.marg = m;
    return C;
}

MenuPiece top_piece(int sx, int sy, int sw, int sh, uint8_t layers, const Cols &C, int dy, int dx = 0)
{
    MenuPiece p = menu_piece_make(SRC_TOP_2D, sx, sy, sw, sh, MENU_AX_L, MENU_AY_T, C.marg + dx, dy, C.a.n, C.a.d);
    p.layers = layers;
    return p;
}

MenuPiece sub_piece(int sx, int sy, int sw, int sh, const Cols &C, int dy, int touch, uint8_t layers = MENU_L_BG0 | MENU_L_BG1 | MENU_L_OBJ)
{
    MenuPiece p = menu_piece_make(SRC_SUB, sx, sy, sw, sh, MENU_AX_R, MENU_AY_T, -C.marg, dy, C.b.n, C.b.d);
    p.layers = layers;     /* plates + labels (+ the ROM's own star / arrows OBJs where a page has them); never the minimap */
    p.key_backdrop = 1;
    p.touch = (uint8_t)touch;
    return p;
}

int top_dy(int block_h) { return (192 - block_h) / 2; }

void add_hs(MenuLayout *L, int piece, const Hs &h)
{
    menu_add_hotspot(L, piece, h.x, h.y, h.w, h.h, h.rom);
}

/* ---- language dependent boxes (PS_Update.cpp :704-718 controller, :944-961 options) ------------- */

void ok_box(int controller, short *x, short *w)
{
    const int lang = GetOwnerLanguage();
    if (controller) {
        if (lang == 5) { *x = 0x44; *w = 0x64; }
        else if (lang == 3) { *x = 0x3c; *w = 0x74; }
        else { *x = 0x4c; *w = 0x54; }
    } else {
        if (lang == 5) { *x = 0x40; *w = 0x70; }
        else if (lang == 3) { *x = 0x48; *w = 0x60; }
        else { *x = 0x50; *w = 0x50; }
    }
}

/* ---- the builders ---------------------------------------------------------- */

const int kUiW = 248;     /* every one-screen pause page sizes its two columns from the same widths, so the text and the controls keep one scale across pages */
const int kTextX = 20, kTextY = 24, kTextW = 216, kTextH = 136;   /* top text block (BG3 + OBJ). Measured on the reference exe (lifted rows 36..147, text columns 27..228), clear of the bouncing band (rows 166..184) and of the two OBJ course arrows (columns 4..19 and 236..251) */

void build_plates(MenuLayout *L, int variant, int py, int ph, const Hs *hs, int nhs, int back, int with_arrows)
{
    const Cols C = make_cols(L, kTextW, kUiW);
    const int hT = ceil_mul(kTextH, C.a);
    const int hA = with_arrows ? ceil_mul(32, C.a) : 0;
    const int gap = with_arrows ? 6 : 0;
    const int dy = top_dy(hT + gap + hA);
    /* with the paging arrows up the ROM's two top-screen arrows (columns 15..29 and 229..241) sit at the
       text rect's edges, so the rect narrows to columns 30..228 and keeps its place on the picture */
    const int tx = with_arrows ? 30 : kTextX, tw = with_arrows ? 199 : kTextW;
    menu_add_piece(L, top_piece(tx, kTextY, tw, kTextH, MENU_L_BG3 | MENU_L_OBJ, C, dy, ceil_mul(tx - kTextX, C.a)));
    const int pl = menu_add_piece(L, sub_piece(8, py, 240, ph, C, top_dy(ceil_mul(ph, C.b)), 1, MENU_L_BG0 | MENU_L_BG1));
    int arrow_l = -1, arrow_r = -1;
    if (with_arrows) {
        /* the ROM's own arrows: bottom-screen OBJs in the corners, touch boxes x < 0x38 and x >= 0xc8,
           y < 0x20 (PS_Update.cpp :322-401); the left one sits under the text block's left end, the
           right one under its right end */
        const int aw = ceil_mul(56, C.a);
        MenuPiece pa = menu_piece_make(SRC_SUB, 0, 0, 56, 32, MENU_AX_L, MENU_AY_T, C.marg, dy + hT + gap, C.a.n, C.a.d);
        pa.layers = MENU_L_OBJ; pa.key_backdrop = 1; pa.touch = 1;
        arrow_l = menu_add_piece(L, pa);
        MenuPiece pb = menu_piece_make(SRC_SUB, 200, 0, 56, 32, MENU_AX_L, MENU_AY_T,
                                       C.marg + ceil_mul(kTextW, C.a) - aw, dy + hT + gap, C.a.n, C.a.d);
        pb.layers = MENU_L_OBJ; pb.key_backdrop = 1; pb.touch = 1;
        arrow_r = menu_add_piece(L, pb);
    }
    for (int i = 0; i < nhs; ++i) add_hs(L, pl, hs[i]);
    if (with_arrows) {
        menu_add_hotspot(L, arrow_l, 0, 0, 0x38, 0x20, -1);
        menu_add_hotspot(L, arrow_r, 0xc8, 0, 0x38, 0x20, -1);
    }
    L->back = (int8_t)back;
    L->lift_owners = MENU_L_BG3 | MENU_L_OBJ;
    L->hide_owners = MENU_L_BG2;   /* the "TOUCH TO SELECT" band: drawn nowhere */
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
    (void)variant;
}

/* controller modes (7, 8): the diagram on the top screen, the mode rows + OK (+ Back) below */
void build_ctl(MenuLayout *L, int with_back)
{
    const Cols C = make_cols(L, 252, kUiW);
    const MenuPiece d = [&] {
        MenuPiece p = menu_piece_make(SRC_TOP_2D, 2, 0, 252, 192, MENU_AX_L, MENU_AY_M, C.marg, 0, C.a.n, C.a.d);
        p.layers = MENU_L_BG2 | MENU_L_BG3 | MENU_L_OBJ;
        return p;
    }();
    menu_add_piece(L, d);
    const int ps = menu_add_piece(L, sub_piece(4, 28, 248, 160, C, 0, 1));
    L->p[ps].ay = MENU_AY_M;
    const Hs rows[3] = { { 8, 32, 240, 32, -1 }, { 8, 72, 240, 32, -1 }, { 8, 112, 240, 32, -1 } };
    for (int i = 0; i < 3; ++i) add_hs(L, ps, rows[i]);
    short ox, ow;
    ok_box(1, &ox, &ow);
    const int ok = menu_add_hotspot(L, ps, ox, 0x98, ow, 0x20, -1);
    int back = ok;
    if (with_back) back = menu_add_hotspot(L, ps, 0xd8, 0x98, 0x20, 0x20, -1);
    L->back = (int8_t)back;
    L->lift_owners = MENU_L_BG2 | MENU_L_BG3 | MENU_L_OBJ;
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

/* options (9, 0xa, 0xb): castle text left, sound + backlight + OK + Back right */
void build_opt(MenuLayout *L)
{
    const Cols C = make_cols(L, kTextW, kUiW);
    menu_add_piece(L, top_piece(kTextX, kTextY, kTextW, kTextH, MENU_L_BG3 | MENU_L_OBJ, C, top_dy(ceil_mul(kTextH, C.a))));
    MenuPiece p = sub_piece(4, 28, 248, 160, C, 0, 1);
    p.ay = MENU_AY_M;
    const int ps = menu_add_piece(L, p);
    /* PS_Update.cpp :939-1230: sound arrows, the two backlight boxes, OK, Back */
    const Hs hs[4] = { { 0x5c, 0x30, 0x10, 0x10, -1 }, { 0xec, 0x30, 0x10, 0x10, -1 },
                       { 0x70, 0x58, 0x38, 0x20, -1 }, { 0xb0, 0x58, 0x38, 0x20, -1 } };
    for (int i = 0; i < 4; ++i) add_hs(L, ps, hs[i]);
    short ox, ow;
    ok_box(0, &ox, &ow);
    menu_add_hotspot(L, ps, ox, 0x98, ow, 0x20, -1);
    L->back = (int8_t)menu_add_hotspot(L, ps, 0xd8, 0x98, 0x20, 0x20, -1);
    L->lift_owners = MENU_L_BG3 | MENU_L_OBJ;
    L->hide_owners = MENU_L_BG2;   /* the "TOUCH TO SELECT" band: drawn nowhere */
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

/* saving status (0xe, 0xf): the box the ROM draws, nothing to press */
void build_status(MenuLayout *L)
{
    const Cols C = make_cols(L, kTextW, kUiW);
    menu_add_piece(L, top_piece(kTextX, kTextY, kTextW, kTextH, MENU_L_BG3 | MENU_L_OBJ, C, top_dy(ceil_mul(kTextH, C.a))));
    MenuPiece p = sub_piece(56, 40, 144, 80, C, 0, 0);
    p.ay = MENU_AY_M;
    menu_add_piece(L, p);
    L->back = -1;
    L->lift_owners = MENU_L_BG3 | MENU_L_OBJ;
    L->hide_owners = MENU_L_BG2;   /* the "TOUCH TO SELECT" band: drawn nowhere */
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

/* don't save confirm (0x10, 0x11): the question and YES / NO (PS_Update.cpp :1370-1451) */
void build_nosave(MenuLayout *L)
{
    const Cols C = make_cols(L, kTextW, kUiW);
    menu_add_piece(L, top_piece(kTextX, kTextY, kTextW, kTextH, MENU_L_BG3 | MENU_L_OBJ, C, top_dy(ceil_mul(kTextH, C.a))));
    MenuPiece p = sub_piece(24, 64, 208, 128, C, 0, 1);
    p.ay = MENU_AY_M;
    const int ps = menu_add_piece(L, p);
    menu_add_hotspot(L, ps, 0x28, 0x98, 0x50, 0x20, -1);                 /* YES x 0x28..0x77 */
    L->back = (int8_t)menu_add_hotspot(L, ps, 0x88, 0x98, 0x50, 0x20, -1);   /* NO x 0x88..0xd7 */
    L->lift_owners = MENU_L_BG3 | MENU_L_OBJ;
    L->hide_owners = MENU_L_BG2;   /* the "TOUCH TO SELECT" band: drawn nowhere */
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

void pause_build(int variant, MenuLayout *L)
{
    switch (variant) {
    case V_CASTLE: {   /* PS_Update.cpp :224-321 */
        const Hs hs[4] = { { 8, 0x20, 240, 32, 0 }, { 8, 0x48, 240, 32, 1 }, { 8, 0x70, 240, 32, 2 }, { 8, 0x98, 240, 32, 3 } };
        build_plates(L, variant, 24, 160, hs, 4, 0, !data_0209f2f0);
        break;
    }
    case V_COURSE: {   /* :430-551 */
        const Hs hs[4] = { { 8, 0x18, 240, 32, 0 }, { 8, 0x40, 240, 32, 1 }, { 8, 0x68, 240, 32, 2 }, { 8, 0x90, 240, 32, 3 } };
        build_plates(L, variant, 24, 160, hs, 4, 0, 0);
        break;
    }
    case V_SAVE: {     /* :1249-1339 */
        const Hs hs[3] = { { 8, 0x28, 240, 32, 0 }, { 8, 0x50, 240, 32, 1 }, { 8, 0x78, 240, 32, 2 } };
        build_plates(L, variant, 40, 120, hs, 3, 0, 0);
        break;
    }
    case V_VS: {       /* :552-636 */
        const Hs hs[3] = { { 8, 0x28, 240, 32, 0 }, { 8, 0x50, 240, 32, 1 }, { 8, 0x78, 240, 32, 2 } };
        build_plates(L, variant, 40, 120, hs, 3, 0, 0);
        break;
    }
    case V_CTL: build_ctl(L, 1); break;
    case V_CTL_SEL: build_ctl(L, 0); break;
    case V_OPT: build_opt(L); break;
    case V_STATUS: build_status(L); break;
    case V_NOSAVE: build_nosave(L); break;
    default: break;
    }
}

/* ---- the ROM-cursor driver (called by hal_menu_pad for every pad frame while this recipe is up) ----
   dir = MENU_DIR_* or -1 (no direction this frame). *owns is set to 1 on the pages whose plates are the
   ROM's own cursor (A is then the ROM's confirm, the core rewrites it to Start) and 0 on the touch-only
   pages (A presses the focused hotspot). Returns 1 when it consumed the direction. */
int pause_cursor(int dir, int *owns)
{
    const unsigned st = data_0209f248;
    const int plates = st == 1 || st == 3 || st == 5 || st == 0xd;
    if (owns) *owns = plates;
    if (!plates || dir < 0) return 0;
    if (data_0209f300 != 0 || data_0209f22c != 0 || data_0209f244 != 0) return 1;
    if (dir == MENU_DIR_LEFT || dir == MENU_DIR_RIGHT) {
        /* the course arrows are touch boxes at the top corners of the bottom screen (PS_Update.cpp :322-401),
           live in the castle main page only and only when the card pages (data_0209f2f0 == 0) */
        if (st == 1 && !data_0209f2f0)
            hal_menu_touch_queue_press(dir == MENU_DIR_LEFT ? 0x1c : 0xe4, 0x10);
        return 1;
    }
    const int last = data_0209f2b4 ? data_0209f2b4 - 1 : 0;
    int sel = data_0209f2e0;
    int moved = 0;
    if (!g_cursor_shown) {
        g_cursor_shown = 1;
        moved = 1;
    } else if (dir == MENU_DIR_UP && sel > 0) {
        --sel;
        moved = 1;
    } else if (dir == MENU_DIR_DOWN && sel < last) {
        ++sel;
        moved = 1;
    }
    if (moved) {
        data_0209f2e0 = (unsigned char)sel;
        _ZN5Stage17UpdateMenuButtonsEb(0);
        func_02012790(0x12e);
    }
    return 1;
}

}  // namespace

int menu_pause_up(int scene_id) { return pause_up(scene_id); }
void menu_pause_build(int variant, MenuLayout *L) { pause_build(variant, L); }
int menu_pause_cursor(int dir, int *owns) { return pause_cursor(dir, owns); }
int menu_pause_pad_ok(void) { return data_0209f2c4 != 0 && data_0209f20c == 0; }
