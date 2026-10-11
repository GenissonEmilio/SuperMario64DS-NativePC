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
#include "menu_hostimg.h"
#include "menu_pause_art_ids.h"

#include <chrono>
#include <stdio.h>
#include <stdlib.h>
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
int SublevelToLevel(int i);
extern signed char data_0209f2f8;   /* the level the pause is in (PS_Init: castle = 0x1d) */
extern unsigned char data_0209f2b8; /* the save status countdown */
void _ZN5Stage17UpdateMenuButtonsEb(int b);
void func_02012790(int id);
}

namespace {

enum { V_CASTLE = 1, V_COURSE, V_CTL, V_CTL_SEL, V_OPT, V_SAVE, V_STATUS, V_NOSAVE, V_VS };

/* ---- the variant, with hysteresis over the leaving states ---------------- */

int g_last_variant;

int pause_variant(void)
{
    if (!data_0209f2c4 || data_0209f20c) return 0;
    const unsigned st = data_0209f248;
    int v = 0;
    switch (st) {
    case 0: case 1: v = V_CASTLE; break;
    case 2: case 3: v = V_COURSE; break;
    case 4: case 5: v = 0; break;      /* VS pause: the ROM draws its own until a real lobby proof exists (PAUSEFIX1 F3) */
    case 7: case 8: v = data_0209f218 ? V_CTL_SEL : V_CTL; break;
    case 9: case 0xa: case 0xb: v = V_OPT; break;
    case 0xc: case 0xd: v = V_SAVE; break;
    case 0xe: case 0xf: v = V_STATUS; break;
    case 0x10: case 0x11: v = V_NOSAVE; break;
    default: v = g_last_variant; break;       /* 6, 0x12, 0x13: leaving, ride through */
    }
    return v;
}

/* THE FIRST-OPEN HITCH. Decoding and scaling the pause's planks, card and arrows on the first frame the pause is up cost ~95 ms in one
   frame (perf trace, 1024x768). The recipe's up() runs every tick, so once the level has been running a moment and the pause is not up it
   solves each variant once on a scratch layout and scales every picture to the size that variant will draw at. PAUSEFIX1: spread thin. A
   slice is one decode or one scale (never both for a picture that took real time), at most about 1 ms of work chained past the first
   unit, and only when the shared warm-up slot is free (menu_warm_claim: one slot per few ms for this and the settings page together, none
   while the game is hitching). The save pages' big pictures (saving, saved, banner, about 11 MB) are not warmed: they load on first use.
   The cache keeps the size; a window resize warms again. */
int g_idle_ticks, g_warm_v, g_warm_p, g_warm_busy, g_warm_w, g_warm_h, g_warm_solved_v;
double g_last_claim = -1e9, g_last_call = -1e9;
int g_calm;

double wnow_ms(void)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool warm_skip_id(int id) { return id == MENU_IMG_P_SAVING || id == MENU_IMG_P_SAVED || id == MENU_IMG_P_BANNER; }
bool g_warm_solving;     /* inside a warm-up solve: have() of a skipped picture answers yes without decoding it (the layout then has its piece, the warm-up skips it) */
const int kWarmPre[] = { MENU_IMG_P_PK_CONTINUE, MENU_IMG_P_PK_OK, MENU_IMG_P_CARD, MENU_IMG_P_ARROW_L, MENU_IMG_ARROW_DOWN, MENU_IMG_STAR };
int g_warm_pre;          /* the pictures every page's layout asks for, decoded one a slice before the first solve */

/* one slice. The decode and the scale of a picture run on the hostimg worker thread (menu_hostimg_prefetch), up to four jobs queued, so the game
   thread only installs finished work (vector moves) and solves a layout (well under a millisecond). */
void warm_slice(int w, int h)
{
    static MenuLayout W;
    const MenuRecipe *R = 0;
    for (int i = 0; i < g_menu_nrecipes; ++i)
        if (!strcmp(g_menu_recipes[i].name, "pause")) R = &g_menu_recipes[i];
    if (!R) { g_warm_v = V_VS; return; }
    static const bool trace = getenv("SM64DS_WARM_TRACE") != 0;
    const double t0 = wnow_ms();
    menu_hostimg_prefetch_poll();             /* installs what the worker finished */
    while (g_warm_v < V_VS) {
        if (g_warm_pre < (int)(sizeof kWarmPre / sizeof kWarmPre[0])) {
            /* the pictures every layout asks for are decoded first (a 2x2 scale rides along), so the solve below never decodes on this thread:
               queue them all, then wait until each is in */
            int pending = 0;
            for (int i = 0; i < (int)(sizeof kWarmPre / sizeof kWarmPre[0]); ++i)
                if (menu_hostimg_prefetch(kWarmPre[i], 2, 2) != 1) ++pending;
            if (pending) return;
            g_warm_pre = (int)(sizeof kWarmPre / sizeof kWarmPre[0]);
            continue;
        }
        if (g_warm_solved_v != g_warm_v) {
            g_warm_solving = true;
            menu_layout_solve(&W, R, g_warm_v, w, h);
            g_warm_solving = false;
            g_warm_solved_v = g_warm_v;
            if (trace) fprintf(stderr, "[warm] pause solve v%d: %.2f ms\n", g_warm_v, wnow_ms() - t0);
        }
        int k = 0, found = -1;
        for (int i = 0; i < W.np; ++i) {
            if (W.p[i].src != SRC_HOSTIMG || !W.p[i].hostimg || warm_skip_id(W.p[i].hostimg) || W.dest[i][2] <= 0 || W.dest[i][3] <= 0) continue;
            if (k++ == g_warm_p) { found = i; break; }
        }
        if (found < 0) { ++g_warm_v; g_warm_p = 0; continue; }     /* that variant is done */
        if (menu_hostimg_prefetch(W.p[found].hostimg, W.dest[found][2], W.dest[found][3]) < 0) return;      /* the queue is full: next slice */
        ++g_warm_p;                                                  /* cached or queued: on to the next picture */
        if (wnow_ms() - t0 >= 1.0) return;
    }
}

/* The shared warm-up slot, for the pause's pictures and the settings page's caches together. 1 = this call may do one slice. A slot is
   handed out at most every 4 ms (so two warmers never land on the same tick), and not at all until the callers have been seen running at
   an even pace for a few calls (a call more than 35 ms after the last one means the level or something else is hitching: wait). */
extern "C" int menu_warm_claim(void)
{
    static const bool off = getenv("SM64DS_NO_MENU_WARM") != 0;
    if (off) return 0;
    const double t = wnow_ms();
    if (t - g_last_call > 35.0) g_calm = 0; else ++g_calm;
    g_last_call = t;
    if (g_calm < 8) return 0;
    if (t - g_last_claim < 4.0) return 0;
    g_last_claim = t;
    return 1;
}

void warm_tick(int scene_id)
{
    if (scene_id != 3 && scene_id != -1) { g_idle_ticks = 0; return; }
    int w, h;
    if (!hal_menu_last_size(&w, &h)) return;
    if (w != g_warm_w || h != g_warm_h) { g_warm_w = w; g_warm_h = h; g_warm_v = 1; g_warm_p = 0; g_warm_busy = 0; g_warm_solved_v = 0; g_idle_ticks = 0; }
    if (g_warm_v == 0) g_warm_v = 1;
    if (++g_idle_ticks < 30 || g_warm_v >= V_VS) return;
    if (!menu_warm_claim()) return;
    warm_slice(w, h);
}

int pause_up(int scene_id)
{
    int v = 0;
    /* the Stage is scene 3; a level boot that never runs the scene thunk answers -1 (hal_menu_hook_h1), and
       the stacked layout is MENU_SCENE_GATED: no recipe */
    if (scene_id == 3 || scene_id == -1) v = pause_variant();
    g_last_variant = v;
    if (!v) warm_tick(scene_id);
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

void ok_box(int controller, int lang, short *x, short *w)
{
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

/* LAYOUT CONSTANTS, DS units before the column scale. One left column width and one plank column width for EVERY page, so the two
   columns keep one scale across pages (the text never jumps when a page changes).
   LEFT column (kLeftW): [arrow][card][arrow], the course card (gold frame + cream paper, our art, drawn behind the ROM's own top text) with
   the course paging arrows (our art) at its sides. RIGHT column (kUiW): the planks. */
const int kUiW = 240;
const int kTextX = 20, kTextY = 30, kTextW = 216, kTextH = 124;   /* top text block (BG3 + OBJ). Measured on the reference exe (lifted rows 36..147, text columns 27..228; the rect keeps 6 empty rows above and below), clear of the bouncing band (rows 166..184) and of the two OBJ course arrows (columns 4..19 and 236..251) */
const int kPadH = 6, kPadV = 5;               /* card paper around the text rect */
const int kAw = 14, kAh = 21, kAgap = 2;      /* paging arrow art size and its gap to the card */
const int kLeftW = 2 * (kAw + kAgap) + kTextW + 2 * kPadH;   /* 260: wide enough for the full text rect (no paging arrows up) */
const int kLeftX = kAw + kAgap + kPadH;       /* the text rect's offset from the column's left edge */

const int kPlankPitch = 40;                   /* the ROM's plate pitch (PS_Update: rows 0x20, 0x48, ... and 0x18, 0x40, ...) */

bool english(const MenuLayout *L) { return L->facts.language <= 1; }
bool have(int id) { return (g_warm_solving && warm_skip_id(id)) || menu_hostimg_ready(id) != 0; }
bool arts_ready(void) { return have(MENU_IMG_P_PK_CONTINUE) && have(MENU_IMG_P_PK_OK) && have(MENU_IMG_P_CARD) && have(MENU_IMG_P_ARROW_L); }
bool planks_on(const MenuLayout *L) { return english(L) && arts_ready(); }
bool in_course(const MenuLayout *L) { return L->facts.spare[2] != 0; }   /* 0 = the castle (and the selftest, which has no game) */

int ur(int units_n, int n, int d) { return (units_n * n + d / 2) / d; }   /* a DS length at the column scale, rounded */

/* Left column: card (castle only), text rect, paging arrows. */
void build_left(MenuLayout *L, const Cols &C, int with_arrows, int castle_card, int *card_piece)
{
    const int blockH = kTextH + 2 * kPadV;
    const int hB = ceil_mul(blockH, C.a);
    const int dyB = top_dy(hB);
    const int tx = with_arrows ? 30 : kTextX, tw = with_arrows ? 198 : kTextW;
    *card_piece = -1;
    if (castle_card && have(MENU_IMG_P_CARD)) {
        /* the card spans the text rect with its paper margin, centred on the text */
        const int cw = tw + 2 * kPadH;
        MenuPiece cp = menu_piece_make(SRC_HOSTIMG, 0, 0, cw, blockH, MENU_AX_L, MENU_AY_T,
                                       C.marg + ur(kAw + kAgap + (kTextW - tw) / 2, C.a.n, C.a.d), dyB, C.a.n, C.a.d);
        cp.hostimg = (uint8_t)MENU_IMG_P_CARD;
        cp.bg = 1;
        *card_piece = menu_add_piece(L, cp);
    }
    menu_add_piece(L, top_piece(tx, kTextY, tw, kTextH, MENU_L_BG3 | MENU_L_OBJ, C, dyB + ur(kPadV, C.a.n, C.a.d),
                                ur(kLeftX + (tx - kTextX), C.a.n, C.a.d)));
    if (with_arrows && have(MENU_IMG_P_ARROW_L)) {
        /* the paging arrows: fixed host rects either side of the card; a click anywhere on one reaches the ROM's own touch box
           (PS_Update.cpp :322-401: x < 0x38 and x >= 0xc8, y < 0x20) */
        menu_layout_dest(L);
        const int *cd = L->dest[*card_piece >= 0 ? *card_piece : 0];
        const int aw = ur(kAw, C.a.n, C.a.d) * L->unum / L->uden, ah = ur(kAh, C.a.n, C.a.d) * L->unum / L->uden;
        const int gap = ur(kAgap, C.a.n, C.a.d) * L->unum / L->uden;
        const int cy = cd[1] + cd[3] / 2;
        for (int side = 0; side < 2; ++side) {
            MenuPiece ap = menu_piece_make(SRC_HOSTIMG, side ? 0xc8 : 0, 0, 0x38, 0x20, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
            ap.hostimg = (uint8_t)(side ? MENU_IMG_P_ARROW_R : MENU_IMG_P_ARROW_L);
            ap.touch = 1; ap.lden = 1; ap.lnum = 1;
            ap.fw = (short)aw; ap.fh = (short)ah;
            ap.fx = (short)(side ? cd[0] + cd[2] + gap : cd[0] - gap - aw);
            ap.fy = (short)(cy - ah / 2);
            const int ip = menu_add_piece(L, ap);
            menu_add_hotspot(L, ip, side ? 0xc8 : 0, 0, 0x38, 0x20, -1);
        }
    }
}

/* one plank: anchored right at the column, dy in canvas units (top of the piece). sx..sw / sy / sh = the ROM's own touch box. */
int add_plank(MenuLayout *L, const Cols &C, int img, int sx, int sy, int sw, int sh, int dy, int rom, int hl)
{
    MenuPiece p = menu_piece_make(SRC_HOSTIMG, sx, sy, sw, sh, MENU_AX_R, MENU_AY_T, -C.marg - ur(248 - (sx + sw), C.b.n, C.b.d), dy, C.b.n, C.b.d);
    p.hostimg = (uint8_t)img;
    p.touch = 1;
    p.hl = (uint8_t)hl;
    const int ip = menu_add_piece(L, p);
    menu_add_hotspot(L, ip, sx, sy, sw, sh, rom);
    return ip;
}

/* the game's small arrow over the chosen plank (fixed host rect, bobbing on the ROM's 8 frame cadence) */
void add_pointer(MenuLayout *L, int plank_piece)
{
    if (plank_piece < 0 || !have(MENU_IMG_ARROW_DOWN)) return;
    menu_layout_dest(L);
    static const int kBob[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
    const int bob2 = kBob[L->facts.spare[1] & 7];
    int iw = 1, ih = 1;
    menu_hostimg_size(MENU_IMG_ARROW_DOWN, &iw, &ih);
    const int *d = L->dest[plank_piece];
    const int aw = L->unum * 10 / L->uden, ah = (int)((long long)aw * ih / iw);
    MenuPiece ar = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
    ar.bg = 1; ar.hostimg = MENU_IMG_ARROW_DOWN; ar.lden = 1; ar.lnum = 1; ar.fw = (short)aw; ar.fh = (short)ah;
    ar.fx = (short)(d[0] + d[2] / 2 - aw / 2);
    ar.fy = (short)(d[1] + (2 + bob2) * L->unum / (2 * L->uden) - ah);
    menu_add_piece(L, ar);
}

/* the plank pages (castle 1, course 2, save 6, VS 9): the left column, then the planks (ours in English, else the ROM's plates) */
void build_plates(MenuLayout *L, int variant, int py, int ph, const Hs *hs, int nhs, int back, int with_arrows,
                  const int *imgs)
{
    const Cols C = make_cols(L, kLeftW, kUiW);
    int card;
    build_left(L, C, with_arrows, !in_course(L), &card);
    const bool pk = planks_on(L) && imgs;
    int sel = (int)L->facts.spare[0];
    if (sel < 0 || sel >= nhs) sel = 0;
    int selp = -1;
    if (pk) {
        /* nhs planks at the ROM's pitch, as one block centred on the picture */
        const int pitch = ur(kPlankPitch, C.b.n, C.b.d), hp = ceil_mul(32, C.b);
        const int blockH = (nhs - 1) * pitch + hp;
        const int top = top_dy(blockH);
        for (int i = 0; i < nhs; ++i) {
            const int ip = add_plank(L, C, imgs[i], hs[i].x, hs[i].y, hs[i].w, hs[i].h, top + i * pitch, hs[i].rom,
                                     (i == sel ? 1 : 0) | (i == sel && L->facts.spare[3] ? 2 : 0));
            if (i == sel) selp = ip;
        }
    } else {
        const int pl = menu_add_piece(L, sub_piece(8, py, 240, ph, C, top_dy(ceil_mul(ph, C.b)), 1, MENU_L_BG0 | MENU_L_BG1));
        for (int i = 0; i < nhs; ++i) add_hs(L, pl, hs[i]);
    }
    L->back = (int8_t)back;
    L->lift_owners = MENU_L_BG3 | MENU_L_OBJ;
    L->hide_owners = MENU_L_BG1 | MENU_L_BG2;   /* the card (BG2, ours replaces it) and the "TOUCH TO SELECT" band (BG1; the castle card fix 15707787b5 makes the ROM load both): drawn nowhere */
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
    if (pk) {
        add_pointer(L, selp);
        L->no_focus_paint = 1;      /* the lit plank and the arrow are the selection */
    }
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
    short ox, ow;
    ok_box(1, L->facts.language, &ox, &ow);
    int back = -1;
    if (planks_on(L)) {
        /* rows at the ROM's pitch (boxes y 32, 72, 112) then OK on the same pitch (y 152): the right column's block is rows 32..184 */
        const int pitch = ur(kPlankPitch, C.b.n, C.b.d), hp = ceil_mul(32, C.b);
        const int top = top_dy(3 * pitch + hp);
        static const int kImg[3] = { MENU_IMG_P_PK_STANDARD, MENU_IMG_P_PK_TOUCH, MENU_IMG_P_PK_DUAL };
        const int mode = (int)L->facts.spare[4];
        for (int i = 0; i < 3; ++i) {
            const int ip = add_plank(L, C, kImg[i], 8, 32 + 40 * i, 240, 32, top + i * pitch, -1, 0);
            if (i == mode && have(MENU_IMG_STAR)) {
                /* the chosen mode's star, our title star art, on the plank's left */
                menu_layout_dest(L);
                const int *pd = L->dest[ip];
                const int sh = pd[3] * 7 / 10;
                MenuPiece st = menu_piece_make(SRC_HOSTIMG, 0, 0, 1, 1, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
                st.bg = 1; st.hostimg = MENU_IMG_STAR; st.lden = 1; st.lnum = 1; st.fw = (short)sh; st.fh = (short)sh;
                st.fx = (short)(pd[0] + pd[2] * 3 / 100); st.fy = (short)(pd[1] + (pd[3] - sh) / 2);
                menu_add_piece(L, st);
            }
        }
        back = L->nh;
        add_plank(L, C, MENU_IMG_P_PK_OK, ox, 0x98, ow, 0x20, top + 3 * pitch, -1, 0);
        if (with_back) {
            /* the ROM's own Back button (OBJ), DS art: no hi-res version yet (artist ask) */
            const MenuPiece b = sub_piece(0xd8, 0x98, 0x20, 0x20, C, top + 3 * pitch, 1, MENU_L_BG0 | MENU_L_BG1 | MENU_L_OBJ);
            const int bp = menu_add_piece(L, b);
            back = menu_add_hotspot(L, bp, 0xd8, 0x98, 0x20, 0x20, -1);
        }
    } else {
        const int ps = menu_add_piece(L, sub_piece(4, 28, 248, 160, C, 0, 1));
        L->p[ps].ay = MENU_AY_M;
        const Hs rows[3] = { { 8, 32, 240, 32, -1 }, { 8, 72, 240, 32, -1 }, { 8, 112, 240, 32, -1 } };
        for (int i = 0; i < 3; ++i) add_hs(L, ps, rows[i]);
        back = menu_add_hotspot(L, ps, ox, 0x98, ow, 0x20, -1);
        if (with_back) back = menu_add_hotspot(L, ps, 0xd8, 0x98, 0x20, 0x20, -1);
    }
    L->back = (int8_t)back;
    L->lift_owners = MENU_L_BG2 | MENU_L_BG3 | MENU_L_OBJ;
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

/* options (9, 0xa, 0xb): the ROM's own page. With OneScreenMenus on the Options plate opens the port's settings page instead
   (hal/settings_page_hooks.cpp), so this page is the fallback only: castle text (on the card) left, sound + backlight + OK + Back right (DS art). */
void build_opt(MenuLayout *L)
{
    const Cols C = make_cols(L, kLeftW, kUiW);
    int card;
    build_left(L, C, 0, !in_course(L), &card);
    MenuPiece p = sub_piece(4, 28, 248, 160, C, 0, 1);
    p.ay = MENU_AY_M;
    const int ps = menu_add_piece(L, p);
    /* PS_Update.cpp :939-1230: sound arrows, the two backlight boxes, OK, Back */
    const Hs hs[4] = { { 0x5c, 0x30, 0x10, 0x10, -1 }, { 0xec, 0x30, 0x10, 0x10, -1 },
                       { 0x70, 0x58, 0x38, 0x20, -1 }, { 0xb0, 0x58, 0x38, 0x20, -1 } };
    for (int i = 0; i < 4; ++i) add_hs(L, ps, hs[i]);
    short ox, ow;
    ok_box(0, L->facts.language, &ox, &ow);
    menu_add_hotspot(L, ps, ox, 0x98, ow, 0x20, -1);
    L->back = (int8_t)menu_add_hotspot(L, ps, 0xd8, 0x98, 0x20, 0x20, -1);
    L->lift_owners = MENU_L_BG3 | MENU_L_OBJ;
    L->hide_owners = MENU_L_BG1 | MENU_L_BG2;   /* the card (BG2, ours replaces it) and the "TOUCH TO SELECT" band (BG1; the castle card fix 15707787b5 makes the ROM load both): drawn nowhere */
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

/* saving status (0xe, 0xf): the box the ROM draws, nothing to press. English: our card with the two lines (the ROM's second text
   appears when its countdown data_0209f2b8 reaches 0x3c: PS_Update.cpp :1348-1357) */
void build_status(MenuLayout *L)
{
    const Cols C = make_cols(L, kLeftW, kUiW);
    int card;
    build_left(L, C, 0, !in_course(L), &card);
    if (planks_on(L) && have(MENU_IMG_P_SAVING) && have(MENU_IMG_P_SAVED)) {
        const int saved = L->facts.spare[5] != 0 && L->facts.spare[5] <= 0x3c;
        MenuPiece p = menu_piece_make(SRC_HOSTIMG, 56, 40, 144, 80, MENU_AX_R, MENU_AY_M, -C.marg - ur(248 - 200, C.b.n, C.b.d), 0, C.b.n, C.b.d);
        p.hostimg = (uint8_t)(saved ? MENU_IMG_P_SAVED : MENU_IMG_P_SAVING);
        menu_add_piece(L, p);
    } else {
        MenuPiece p = sub_piece(56, 40, 144, 80, C, 0, 0);
        p.ay = MENU_AY_M;
        menu_add_piece(L, p);
    }
    L->back = -1;
    L->lift_owners = MENU_L_BG3 | MENU_L_OBJ;
    L->hide_owners = MENU_L_BG1 | MENU_L_BG2;   /* the card (BG2, ours replaces it) and the "TOUCH TO SELECT" band (BG1; the castle card fix 15707787b5 makes the ROM load both): drawn nowhere */
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

/* don't save confirm (0x10, 0x11): the question and YES / NO (PS_Update.cpp :1370-1451) */
void build_nosave(MenuLayout *L)
{
    const Cols C = make_cols(L, kLeftW, kUiW);
    int card;
    build_left(L, C, 0, !in_course(L), &card);
    if (planks_on(L)) {
        /* the question (the ROM's own bottom screen text, DS art) on our banner, the two planks under it */
        const int hb = ceil_mul(32, C.b), gap = ur(8, C.b.n, C.b.d);
        const int top = top_dy(2 * hb + gap);
        if (have(MENU_IMG_P_BANNER)) {
            MenuPiece bn = menu_piece_make(SRC_HOSTIMG, 24, 60, 208, 32, MENU_AX_R, MENU_AY_T, -C.marg - ur(248 - 232, C.b.n, C.b.d), top, C.b.n, C.b.d);
            bn.hostimg = (uint8_t)MENU_IMG_P_BANNER; bn.bg = 1;
            menu_add_piece(L, bn);
            MenuPiece q = sub_piece(46, 68, 166, 20, C, top + ur(6, C.b.n, C.b.d), 0, MENU_L_BG0 | MENU_L_BG1);   /* the ROM's message strip has a bar tile at x 34..40 and one near x 222: the text sits at x 61..195 */
            q.dx = (short)(-C.marg - ur(248 - 212, C.b.n, C.b.d));
            menu_add_piece(L, q);
        }
        add_plank(L, C, MENU_IMG_P_PK_YES, 0x28, 0x98, 0x50, 0x20, top + hb + gap, -1, 0);                   /* YES x 0x28..0x77 */
        add_plank(L, C, MENU_IMG_P_PK_NO, 0x88, 0x98, 0x50, 0x20, top + hb + gap, -1, 0);                     /* NO x 0x88..0xd7 */
        L->back = (int8_t)(L->nh - 1);                                                                       /* the hotspot of NO */
    } else {
        MenuPiece p = sub_piece(24, 64, 208, 128, C, 0, 1);
        p.ay = MENU_AY_M;
        const int ps = menu_add_piece(L, p);
        menu_add_hotspot(L, ps, 0x28, 0x98, 0x50, 0x20, -1);                 /* YES x 0x28..0x77 */
        L->back = (int8_t)menu_add_hotspot(L, ps, 0x88, 0x98, 0x50, 0x20, -1);   /* NO x 0x88..0xd7 */
    }
    L->lift_owners = MENU_L_BG3 | MENU_L_OBJ;
    L->hide_owners = MENU_L_BG1 | MENU_L_BG2;   /* the card (BG2, ours replaces it) and the "TOUCH TO SELECT" band (BG1; the castle card fix 15707787b5 makes the ROM load both): drawn nowhere */
    L->backdrop = MENU_BACKDROP_TOP_AS_IS;
    L->stage = MENU_STAGE_POST_FADE;
}

void pause_build(int variant, MenuLayout *L)
{
    switch (variant) {
    case V_CASTLE: {   /* PS_Update.cpp :224-321 */
        const Hs hs[4] = { { 8, 0x20, 240, 32, 0 }, { 8, 0x48, 240, 32, 1 }, { 8, 0x70, 240, 32, 2 }, { 8, 0x98, 240, 32, 3 } };
        static const int img[4] = { MENU_IMG_P_PK_CONTINUE, MENU_IMG_P_PK_CTL, MENU_IMG_P_PK_OPTIONS, MENU_IMG_P_PK_QUIT };
        build_plates(L, variant, 24, 160, hs, 4, 0, !L->facts.pause_f2f0, img);
        break;
    }
    case V_COURSE: {   /* :430-551 */
        const Hs hs[4] = { { 8, 0x18, 240, 32, 0 }, { 8, 0x40, 240, 32, 1 }, { 8, 0x68, 240, 32, 2 }, { 8, 0x90, 240, 32, 3 } };
        static const int img[4] = { MENU_IMG_P_PK_CONTINUE, MENU_IMG_P_PK_CTL, MENU_IMG_P_PK_OPTIONS, MENU_IMG_P_PK_EXIT };
        build_plates(L, variant, 24, 160, hs, 4, 0, 0, img);
        break;
    }
    case V_SAVE: {     /* :1249-1339 */
        const Hs hs[3] = { { 8, 0x28, 240, 32, 0 }, { 8, 0x50, 240, 32, 1 }, { 8, 0x78, 240, 32, 2 } };
        static const int img[3] = { MENU_IMG_P_PK_CONTINUE, MENU_IMG_P_PK_SAVE, MENU_IMG_P_PK_DONTSAVE };
        build_plates(L, variant, 40, 120, hs, 3, 0, 0, img);
        break;
    }
    case V_VS: {       /* :552-636: the three VS labels were never captured, so the ROM's plates stay (artist ask) */
        const Hs hs[3] = { { 8, 0x28, 240, 32, 0 }, { 8, 0x50, 240, 32, 1 }, { 8, 0x78, 240, 32, 2 } };
        build_plates(L, variant, 40, 120, hs, 3, 0, 0, 0);
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
    if (dir == MENU_DIR_UP && sel > 0) {
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



/* MENUSPLIT1: the ROM values build() needs, read once per tick by the decide step. build() reads nothing else.
   spare[0] the chosen plate, [1] a frame counter (the pointer's bob), [2] 1 = in a course (no card: the text is white on the dimmed level),
   [3] 1 while the press is held (the ROM's own 8 tick hold after a confirm: the plank sinks), [4] the chosen controller mode, [5] the save countdown */
unsigned g_hov_seen;
int g_frame;

int pause_cursor_live(void)
{
    const unsigned st = data_0209f248;
    return (st == 1 || st == 3 || st == 5 || st == 0xd) && data_0209f300 == 0 && data_0209f22c == 0 && data_0209f244 == 0;
}

void set_chosen(int sel)
{
    const int last = data_0209f2b4 ? data_0209f2b4 - 1 : 0;
    if (sel < 0 || sel > last || sel == (int)data_0209f2e0) return;
    data_0209f2e0 = (unsigned char)sel;
    _ZN5Stage17UpdateMenuButtonsEb(0);
}

void pause_facts(MenuFacts *f)
{
    f->language = (int16_t)GetOwnerLanguage();
    f->pause_f2f0 = data_0209f2f0;
    /* MOUSE HOVER: a pointer that moved onto a plank makes it the ROM's chosen plate (silent), so the lit plank, the pointer and a
       later Enter all name the same plate */
    int hx, hy;
    unsigned st;
    if (hal_menu_hover_box(&hx, &hy, &st) && st != g_hov_seen) {
        g_hov_seen = st;
        if (pause_cursor_live()) {
            const int y0 = data_0209f248 == 1 ? 0x20 : (data_0209f248 == 3 ? 0x18 : 0x28);
            if (hy >= y0 && (hy - y0) % 0x28 == 0) set_chosen((hy - y0) / 0x28);
        }
    }
    f->spare[0] = data_0209f2e0;
    f->spare[1] = ++g_frame;
    f->spare[2] = SublevelToLevel(data_0209f2f8) != 0x1d;
    f->spare[3] = data_0209f22c != 0;
    f->spare[4] = data_0209f2dc;
    f->spare[5] = data_0209f2b8;
}

}  // namespace

int menu_pause_up(int scene_id) { return pause_up(scene_id); }
void menu_pause_facts(MenuFacts *f) { pause_facts(f); }
void menu_pause_build(int variant, MenuLayout *L) { pause_build(variant, L); }
int menu_pause_cursor(int dir, int *owns) { return pause_cursor(dir, owns); }
int menu_pause_pad_ok(void) { return data_0209f2c4 != 0 && data_0209f20c == 0; }
