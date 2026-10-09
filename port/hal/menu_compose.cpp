/* menu_compose.cpp -- the one-screen menu compose layer (run menu1, lane P0CORE).
 * See menu_compose.h for the model and runs/menu1/in/DESIGN.md sections B, C, F.
 *
 * NOTHING IN HERE TOUCHES THE ROM OR THE FRAME. It takes planes by pointer,
 * writes an ARGB buffer it is handed, and keeps its own small state. It is
 * linked into the exe and called by nobody until phase 1 wires the hooks, so
 * with no environment variable set the exe behaves as before.
 *
 * MENU LAYOUT SELFTEST: SM64DS_MENU_SELFTEST=1 (a static initialiser in this
 * file, so walk_window.cpp needs no edit: it runs before main, prints the
 * table to stderr and, when SM64DS_MENU_SELFTEST_OUT names a file, to that file
 * too, then exits 0 on all PASS and 1 otherwise).
 * SM64DS_MENU_TRACE=1 prints [menu] lines.
 */
#include "menu_compose.h"
#include "menu_hostimg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <vector>

namespace {

/* ---- small arithmetic ---------------------------------------------------- */

long long fdiv(long long a, long long b)       /* floor, b > 0 */
{
    long long q = a / b;
    if ((a % b) != 0 && a < 0) --q;
    return q;
}

int gcd_i(int a, int b)
{
    while (b) { const int t = a % b; a = b; b = t; }
    return a < 0 ? -a : a;
}

int iabs(int a) { return a < 0 ? -a : a; }

/* canvas units -> host pixels, floor, for the reduced fraction unum/uden */
int u2h(const MenuLayout *L, int c) { return (int)fdiv((long long)c * L->unum, L->uden); }

int trace_on(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("SM64DS_MENU_TRACE");
        if (!e || !*e) e = getenv("SM64DS_SWAP_TRACE");   /* the level-clear compose's old name, kept as an alias */
        v = (e && *e && *e != '0') ? 1 : 0;
    }
    return v;
}

#define MTRACE(...) do { if (trace_on()) { fprintf(stderr, "[menu] " __VA_ARGS__); fputc('\n', stderr); fflush(stderr); } } while (0)

/* DS blend arithmetic in 5 bits per channel, the way the hardware does it:
   toward white  c' = c + ((31 - c) * evy >> 4);  toward black  c' = c - (c * evy >> 4) */
uint32_t blend_ch(uint32_t argb, int evy, int to_white)
{
    uint32_t out = argb & 0xff000000u;
    for (int sh = 0; sh <= 16; sh += 8) {
        int c5 = (int)((argb >> sh) & 0xff) >> 3;
        c5 = to_white ? c5 + (((31 - c5) * evy) >> 4) : c5 - ((c5 * evy) >> 4);
        const uint32_t c8 = (uint32_t)((c5 << 3) | (c5 >> 2));
        out |= c8 << sh;
    }
    return out;
}

int luma(uint32_t argb)
{
    return (int)(((argb >> 16) & 0xff) * 3 + ((argb >> 8) & 0xff) * 6 + (argb & 0xff));
}

/* ---- module state ------------------------------------------------------- */

MenuLayout g_cur;           /* this frame's layout (frame_begin) */
MenuLayout g_drawn;         /* the layout last drawn (what the inverse reads) */
int  g_live;
int  g_drawn_valid;
int  g_cur_recipe = -1;
int  g_cur_variant;
int  g_focus_shown;
int  g_q_real_held;
int  g_drag_piece = -1;
int  g_netplay;
int  g_fallback_frames;
unsigned g_frame;
int  (*g_enable)(int key);
const MenuSources *g_src;
int  g_dst_stride;                   /* HOOK1: pixels per row of the dst buffer; 0 = the picture width */
MenuLiftInfo g_lift;                 /* HOOK1: this frame's lift plan (hal_menu_lift_info) */
int  g_lift_on;
int  g_lift_pending;                 /* the lift was promised and the draw has not honoured it yet */
/* the engine-B planes by layer mask, made by the host callback at most once per mask per generation */
struct PlaneEnt { uint8_t layers, oam_on, lo, hi; const uint32_t *p; };
PlaneEnt g_planes[4];
int  g_nplanes;
DsRect g_latch[MENU_MAX_PIECES];     /* autocrop latches, by piece index of the live recipe/variant */
uint8_t g_latched[MENU_MAX_PIECES];
int  g_latch_recipe = -2, g_latch_variant;

int stride_of(int w) { return g_dst_stride > 0 ? g_dst_stride : w; }

/* the sub plane a piece reads: the host's masked scan-out for its layers, else the one plane */
const uint32_t *sub_plane(const MenuPiece *p, const MenuSources *s)
{
    if (!s) return 0;
    if (s->sub_for) {
        const int oam = p->oam_on && s->sub_for_oam;
        for (int i = 0; i < g_nplanes; ++i)
            if (g_planes[i].layers == p->layers && g_planes[i].oam_on == (oam ? 1 : 0) &&
                (!oam || (g_planes[i].lo == p->oam_lo && g_planes[i].hi == p->oam_hi)))
                return g_planes[i].p ? g_planes[i].p : s->sub;
        const uint32_t *q = oam ? s->sub_for_oam(p->layers, p->oam_lo, p->oam_hi) : s->sub_for(p->layers);
        if (g_nplanes < 4) {
            PlaneEnt &e = g_planes[g_nplanes++];
            e.layers = p->layers; e.oam_on = oam ? 1 : 0; e.lo = p->oam_lo; e.hi = p->oam_hi; e.p = q;
        }
        return q ? q : s->sub;
    }
    return s->sub;
}

int enabled(int key)
{
    if (key == MENU_KEY_NONE) return 1;
    return g_enable ? g_enable(key) : 0;
}

/* ---- source access ------------------------------------------------------ */

int src_opaque(const MenuPiece *p, const MenuSources *s, int sx, int sy)
{
    if (!s) return 0;
    switch (p->src) {
    case SRC_SUB: {
        const uint32_t *sp = sub_plane(p, s);
        if (!sp || sx < 0 || sy < 0 || sx >= 256 || sy >= 192) return 0;
        if (p->row_ok && !p->row_ok(sy)) return 0;
        const uint32_t c = sp[sy * 256 + sx];
        if (p->key_backdrop && (c & 0xffffffu) == (s->sub_backdrop & 0xffffffu)) return 0;
        return 1;
    }
    case SRC_TOP_2D: {
        if (!s->top2d || sx < 0 || sy < 0 || sx >= 256 || sy >= 192) return 0;
        const MenuTopCell &c = s->top2d[sy * 256 + sx];
        if ((c.argb >> 24) == 0) return 0;
        if (c.owner > 4 || !((p->layers >> c.owner) & 1)) return 0;
        if (p->row_ok && !p->row_ok(sy)) return 0;
        return 1;
    }
    case SRC_TOP_HOSTROW:
        if (!s->hostrow || sx < 0 || sy < 0 || sx >= s->hostrow_w || sy >= s->hostrow_h) return 0;
        return (s->hostrow[sy * s->hostrow_stride + sx] >> 24) != 0;
    case SRC_TOP_PIC:
    default:
        return 0;   /* a picture has no "edge pixels" to cut: the census does not apply */
    }
}

}  // namespace

/* ====================================================================== */
/* layout solver                                                          */
/* ====================================================================== */

int menu_wide(const MenuLayout *L) { return L->canvas_w >= MENU_WIDE_CANVAS; }

int menu_scale_allowed(int n, int d)
{
    return (n == 1 && d == 1) || (n == 3 && d == 4) || (n == 2 && d == 3) || (n == 1 && d == 2);
}

void menu_layout_begin(MenuLayout *L, int w, int h)
{
    memset(L, 0, sizeof *L);
    L->w = w;
    L->h = h;
    L->focus = -1;
    L->back = -1;
    if (w < 1 || h < 1) { L->unum = 1; L->uden = 192; L->canvas_w = 0; return; }
    const int g = gcd_i(h, 192);
    L->unum = h / g;
    L->uden = 192 / g;
    L->canvas_w = (int)((long long)w * 192 / h);
}

MenuPiece menu_piece_make(MenuSrc src, int sx, int sy, int sw, int sh, int ax, int ay, int dx, int dy,
                          int knum, int kden)
{
    MenuPiece p;
    memset(&p, 0, sizeof p);
    p.src = src;
    p.layers = MENU_L_ALL;
    p.s.x = (short)sx; p.s.y = (short)sy; p.s.w = (short)sw; p.s.h = (short)sh;
    p.ax = (uint8_t)ax; p.ay = (uint8_t)ay;
    p.dx = (short)dx; p.dy = (short)dy;
    p.knum = (uint8_t)knum; p.kden = (uint8_t)kden;
    return p;
}

int menu_add_piece(MenuLayout *L, const MenuPiece &p)
{
    if (L->np >= MENU_MAX_PIECES) return -1;
    L->p[L->np] = p;
    return L->np++;
}

int menu_add_hotspot(MenuLayout *L, int piece, int bx, int by, int bw, int bh, int rom_index)
{
    if (L->nh >= MENU_MAX_HOTSPOTS) return -1;
    MenuHotspot &h = L->hs[L->nh];
    memset(&h, 0, sizeof h);
    h.piece = (uint8_t)piece;
    h.box.x = (short)bx; h.box.y = (short)by; h.box.w = (short)bw; h.box.h = (short)bh;
    h.rom_index = (int8_t)rom_index;
    for (int d = 0; d < 4; ++d) h.nav[d] = -2;
    return L->nh++;
}

/* SCALE AND PLACEMENT RULE (P0COREFIX). A piece's canvas size is the EXACT
   rational s.w * knum / kden (and s.h likewise), held in twelfths of a canvas
   unit (12 is divisible by every allowed kden 1, 2, 3, 4). The canvas spans
   the picture exactly: one unit is unum / uden host pixels and the canvas is
   w * uden / unum units wide, so a centred piece sits at (w - width) / 2 with
   no floored canvas_w in the way. Both edges of a piece are the floor of the
   exact rational edge position, so
     - the dest size differs from the exact size (s.w * knum * unum / (kden *
       uden)) by LESS THAN ONE host pixel;
     - two pieces that meet on a canvas edge share the same host edge (no seam,
       no overlap).
   A piece that pokes out of the picture by no more than one canvas unit plus
   one pixel (the rounding slack, e.g. a 256 wide piece on a 255.8 unit wide
   canvas at 1022x767) is clamped to the picture; anything further out is left
   outside so the selftest bounds check still catches a bad recipe. */
void menu_layout_dest(MenuLayout *L)
{
    const long long UN = L->unum, UD = L->uden;
    const long long D = 2LL * UD * 12;
    int slack = u2h(L, 1) + 1;
    if (slack < 1) slack = 1;
    for (int i = 0; i < L->np; ++i) {
        const MenuPiece &P = L->p[i];
        int *d = L->dest[i];
        if (P.lden > 0) {   /* HOOK1: the recipe placed it in host pixels */
            d[0] = P.fx; d[1] = P.fy; d[2] = P.fw; d[3] = P.fh;
            continue;
        }
        if (P.src == SRC_TOP_HOSTROW) {
            /* host pixels, 1:1; anchors and offsets in host terms */
            const int dw = P.s.w, dh = P.s.h;
            int x0 = P.ax == MENU_AX_L ? 0 : (P.ax == MENU_AX_C ? (L->w - dw) / 2 : L->w - dw);
            int y0 = P.ay == MENU_AY_T ? 0 : (P.ay == MENU_AY_M ? (L->h - dh) / 2 : L->h - dh);
            x0 += u2h(L, P.dx);
            y0 += u2h(L, P.dy);
            d[0] = x0; d[1] = y0; d[2] = dw; d[3] = dh;
            continue;
        }
        const int kn = P.knum ? P.knum : 1, kd = P.kden ? P.kden : 1;
        const long long cw12 = P.xfill ? (long long)P.fill_w * 12 : (long long)P.s.w * kn * 12 / kd,
                        ch12 = (long long)P.s.h * kn * 12 / kd;
        const long long dx12 = (long long)P.dx * 12, dy12 = (long long)P.dy * 12;
        const long long TW = (long long)L->w * UD * 12, TH = (long long)L->h * UD * 12;   /* the picture, in 1/(uden*12) px */
        const long long nx0 = P.ax == MENU_AX_L ? 2 * dx12 * UN
                            : P.ax == MENU_AX_C ? TW - cw12 * UN + 2 * dx12 * UN
                                                : 2 * (TW - cw12 * UN + dx12 * UN);
        const long long ny0 = P.ay == MENU_AY_T ? 2 * dy12 * UN
                            : P.ay == MENU_AY_M ? TH - ch12 * UN + 2 * dy12 * UN
                                                : 2 * (TH - ch12 * UN + dy12 * UN);
        int x0 = (int)fdiv(nx0, D), x1 = (int)fdiv(nx0 + 2 * cw12 * UN, D);
        int y0 = (int)fdiv(ny0, D), y1 = (int)fdiv(ny0 + 2 * ch12 * UN, D);
        if (x0 < 0 && x0 >= -slack) x0 = 0;
        if (x1 > L->w && x1 <= L->w + slack) x1 = L->w;
        if (y0 < 0 && y0 >= -slack) y0 = 0;
        if (y1 > L->h && y1 <= L->h + slack) y1 = L->h;
        d[0] = x0; d[1] = y0; d[2] = x1 - x0; d[3] = y1 - y0;
    }
}

void menu_layout_solve(MenuLayout *L, const MenuRecipe *r, int variant, int w, int h)
{
    menu_layout_begin(L, w, h);
    L->recipe = r;
    L->variant = variant;
    if (r && r->build && w >= 8 && h >= 8) r->build(variant, L);
    menu_layout_dest(L);
    if (L->focus >= L->nh) L->focus = -1;
}

/* ====================================================================== */
/* inverse                                                                */
/* ====================================================================== */

static int dest_contains(const int *d, int bx, int by)
{
    return d[2] > 0 && d[3] > 0 && bx >= d[0] && bx < d[0] + d[2] && by >= d[1] && by < d[1] + d[3];
}

static void piece_inv(const MenuLayout *L, int i, int bx, int by, int *dsx, int *dsy)
{
    const MenuPiece &P = L->p[i];
    const int *d = L->dest[i];
    int x, y;
    if (P.lden > 0 && P.src != SRC_HOSTIMG) {   /* the legacy rational: DS = floor(host * lden / lnum), swap_inverse's own arithmetic */
        const int ln = P.lnum > 0 ? P.lnum : 1;
        x = P.s.x + (int)fdiv((long long)(bx - d[0]) * P.lden, ln);
        y = P.s.y + (int)fdiv((long long)(by - d[1]) * P.lden, ln);
    } else {
        x = P.s.x + (int)fdiv((long long)(bx - d[0]) * P.s.w, d[2] > 0 ? d[2] : 1);
        y = P.s.y + (int)fdiv((long long)(by - d[1]) * P.s.h, d[3] > 0 ? d[3] : 1);
    }
    if (x < P.s.x) x = P.s.x;
    if (x > P.s.x + P.s.w - 1) x = P.s.x + P.s.w - 1;
    if (y < P.s.y) y = P.s.y;
    if (y > P.s.y + P.s.h - 1) y = P.s.y + P.s.h - 1;
    *dsx = x;
    *dsy = y;
}

int menu_hit(const MenuLayout *L, int bx, int by, int *piece, int *dsx, int *dsy)
{
    for (int i = L->np - 1; i >= 0; --i) {   /* topmost first: later pieces draw over earlier ones */
        if (!L->p[i].touch) continue;
        if (!dest_contains(L->dest[i], bx, by)) continue;
        piece_inv(L, i, bx, by, dsx, dsy);
        if (piece) *piece = i;
        return 1;
    }
    return 0;
}

/* Outside every touch piece: clamp to the piece the drag started on (or, with
   no drag, the nearest touch piece) and say in = 0, as swap_inverse does for
   its plate rectangle. No touch piece at all: (-1, -1, 0). */
void menu_inverse(const MenuLayout *L, int bx, int by, int *drag_piece, int *dsx, int *dsy, int *in)
{
    int pi = -1;
    if (menu_hit(L, bx, by, &pi, dsx, dsy)) {
        if (drag_piece) *drag_piece = pi;
        *in = 1;
        return;
    }
    *in = 0;
    int best = -1;
    if (drag_piece && *drag_piece >= 0 && *drag_piece < L->np && L->p[*drag_piece].touch) {
        best = *drag_piece;
    } else {
        long long bd = -1;
        for (int i = 0; i < L->np; ++i) {
            if (!L->p[i].touch) continue;
            const int *d = L->dest[i];
            const int cx = bx < d[0] ? d[0] : (bx > d[0] + d[2] - 1 ? d[0] + d[2] - 1 : bx);
            const int cy = by < d[1] ? d[1] : (by > d[1] + d[3] - 1 ? d[1] + d[3] - 1 : by);
            const long long dd = (long long)(cx - bx) * (cx - bx) + (long long)(cy - by) * (cy - by);
            if (bd < 0 || dd < bd) { bd = dd; best = i; }
        }
    }
    if (best < 0) { *dsx = -1; *dsy = -1; return; }
    const int *d = L->dest[best];
    int cx = bx < d[0] ? d[0] : (bx > d[0] + d[2] - 1 ? d[0] + d[2] - 1 : bx);
    int cy = by < d[1] ? d[1] : (by > d[1] + d[3] - 1 ? d[1] + d[3] - 1 : by);
    piece_inv(L, best, cx, cy, dsx, dsy);
}

/* ====================================================================== */
/* hotspots and the focus graph                                           */
/* ====================================================================== */

static int ds_to_host_x(const MenuLayout *L, int i, int dsx)
{
    const MenuPiece &P = L->p[i];
    const int *d = L->dest[i];
    return d[0] + (int)fdiv((long long)(dsx - P.s.x) * d[2], P.s.w > 0 ? P.s.w : 1);
}

static int ds_to_host_y(const MenuLayout *L, int i, int dsy)
{
    const MenuPiece &P = L->p[i];
    const int *d = L->dest[i];
    return d[1] + (int)fdiv((long long)(dsy - P.s.y) * d[3], P.s.h > 0 ? P.s.h : 1);
}

int menu_hotspot_rect(const MenuLayout *L, int hs, int r[4])
{
    r[0] = r[1] = r[2] = r[3] = 0;
    if (hs < 0 || hs >= L->nh) return 0;
    const MenuHotspot &H = L->hs[hs];
    if (H.piece >= L->np) return 0;
    const MenuPiece &P = L->p[H.piece];
    int x0 = H.box.x, y0 = H.box.y, x1 = H.box.x + H.box.w, y1 = H.box.y + H.box.h;
    if (x0 < P.s.x) x0 = P.s.x;
    if (y0 < P.s.y) y0 = P.s.y;
    if (x1 > P.s.x + P.s.w) x1 = P.s.x + P.s.w;
    if (y1 > P.s.y + P.s.h) y1 = P.s.y + P.s.h;
    if (x1 <= x0 || y1 <= y0) return 0;
    const int hx0 = ds_to_host_x(L, H.piece, x0), hx1 = ds_to_host_x(L, H.piece, x1);
    const int hy0 = ds_to_host_y(L, H.piece, y0), hy1 = ds_to_host_y(L, H.piece, y1);
    r[0] = hx0; r[1] = hy0;
    r[2] = hx1 > hx0 ? hx1 - hx0 : 1;
    r[3] = hy1 > hy0 ? hy1 - hy0 : 1;
    return 1;
}

void menu_hotspot_centre(const MenuLayout *L, int hs, int *hx, int *hy)
{
    int r[4];
    menu_hotspot_rect(L, hs, r);
    *hx = r[0] + r[2] / 2;
    *hy = r[1] + r[3] / 2;
}

int menu_focus_hover(MenuLayout *L, int bx, int by)
{
    for (int i = L->nh - 1; i >= 0; --i) {
        int r[4];
        if (!menu_hotspot_rect(L, i, r)) continue;
        if (dest_contains(r, bx, by)) { L->focus = i; return 1; }
    }
    return 0;
}

/* Automatic spatial navigation over the hotspots' placed centres: the nearest
   hotspot in the pressed direction. Candidates inside the 45 degree cone of
   the direction beat the ones outside it; within a class the score is
   primary distance + twice the lateral distance; a tie goes to the larger
   overlap of the two rects across the movement axis, then the lower index. */
int menu_focus_move(MenuLayout *L, int dir)
{
    if (L->nh <= 0) return L->focus = -1;
    if (L->focus < 0 || L->focus >= L->nh) return L->focus = 0;
    const int f = L->focus;
    const int n = L->hs[f].nav[dir & 3];
    if (n >= 0 && n < L->nh) return L->focus = n;
    if (n == -1) return f;
    int fr[4], fcx, fcy;
    menu_hotspot_rect(L, f, fr);
    menu_hotspot_centre(L, f, &fcx, &fcy);
    int best = -1, best_class = 2, best_score = 0, best_ov = -1;
    for (int j = 0; j < L->nh; ++j) {
        if (j == f) continue;
        int jr[4], jcx, jcy;
        if (!menu_hotspot_rect(L, j, jr)) continue;
        menu_hotspot_centre(L, j, &jcx, &jcy);
        const int dx = jcx - fcx, dy = jcy - fcy;
        int prim, lat, ov;
        switch (dir & 3) {
        case MENU_DIR_UP:    prim = -dy; lat = iabs(dx); break;
        case MENU_DIR_DOWN:  prim = dy;  lat = iabs(dx); break;
        case MENU_DIR_LEFT:  prim = -dx; lat = iabs(dy); break;
        default:             prim = dx;  lat = iabs(dy); break;
        }
        if (prim <= 0) continue;
        if ((dir & 3) <= MENU_DIR_DOWN) {   /* movement is vertical: overlap along x */
            const int a = fr[0] > jr[0] ? fr[0] : jr[0];
            const int b = (fr[0] + fr[2]) < (jr[0] + jr[2]) ? (fr[0] + fr[2]) : (jr[0] + jr[2]);
            ov = b - a;
        } else {
            const int a = fr[1] > jr[1] ? fr[1] : jr[1];
            const int b = (fr[1] + fr[3]) < (jr[1] + jr[3]) ? (fr[1] + fr[3]) : (jr[1] + jr[3]);
            ov = b - a;
        }
        const int cls = prim >= lat ? 0 : 1;
        const int score = prim + 2 * lat;
        int better = 0;
        if (best < 0 || cls < best_class) better = 1;
        else if (cls == best_class) {
            if (score < best_score) better = 1;
            else if (score == best_score && ov > best_ov) better = 1;
        }
        if (better) { best = j; best_class = cls; best_score = score; best_ov = ov; }
    }
    if (best >= 0) L->focus = best;
    return L->focus;
}

/* ====================================================================== */
/* piece draw, backdrop, highlight, census                                */
/* ====================================================================== */

/* ---- ASSET2: host-resolution gradient and picture ------------------------------------------------------------- */
namespace {
float g_sky_ref = 0.f, g_fade = 1.f;     /* brightest sky luminance seen on this page, and this frame's fade factor */
int   g_sky_frame = -1000;

void draw_hostgrad(const MenuLayout *L, int i, const MenuSources *S, uint32_t *dst, int w, int h)
{
    const MenuPiece &P = L->p[i];
    const int *d = L->dest[i];
    const uint32_t *sp = sub_plane(&P, S);
    if (!sp || d[2] <= 0 || d[3] <= 0) return;
    /* the mean colour of each DS row over the sample columns */
    float rr[192], gg[192], bb[192];
    int n = P.s.w;
    if (n < 1) n = 1;
    for (int y = 0; y < 192; ++y) {
        /* the MEDIAN of each channel over the sample columns: a few columns that catch the edge of an object (the DS logo's
           left end, a black sprite pixel) are a minority and never reach the answer */
        int r[256], g[256], b[256], c = 0;
        for (int x = P.s.x; x < P.s.x + n && x < 256 && c < 256; ++x) {
            if (x < 0) continue;
            const uint32_t v = sp[y * 256 + x];
            r[c] = (v >> 16) & 255; g[c] = (v >> 8) & 255; b[c] = v & 255; ++c;
        }
        if (!c) { r[0] = g[0] = b[0] = 0; c = 1; }
        std::sort(r, r + c); std::sort(g, g + c); std::sort(b, b + c);
        rr[y] = (float)r[c / 2]; gg[y] = (float)g[c / 2]; bb[y] = (float)b[c / 2];
    }
    /* The DS gradient is banded (runs of equal rows of uneven length, each channel stepping at its own rows). Smooth it: two
       passes of a box of +-8 rows with odd reflection at both ends (so the end slopes are kept), which removes the band
       lumps and keeps the ramp's overall shape and colours. Every DS row is then one stop at its centre. */
    float sy[192], sr[192], sg[192], sb[192];
    const int ns = 192;
    {
        float *ch[3] = { rr, gg, bb }, *out[3] = { sr, sg, sb };
        for (int k = 0; k < 3; ++k) {
            float cur[192], nxt[192];
            memcpy(cur, ch[k], sizeof cur);
            for (int pass = 0; pass < 2; ++pass) {
                for (int y = 0; y < 192; ++y) {
                    float acc = 0.f;
                    for (int j = -8; j <= 8; ++j) {
                        int q = y + j;
                        float v;
                        if (q < 0) v = 2.f * cur[0] - cur[-q > 191 ? 191 : -q];
                        else if (q > 191) v = 2.f * cur[191] - cur[(382 - q) < 0 ? 0 : 382 - q];
                        else v = cur[q];
                        acc += v;
                    }
                    nxt[y] = acc / 17.f;
                }
                memcpy(cur, nxt, sizeof cur);
            }
            memcpy(out[k], cur, sizeof cur);
        }
        for (int y = 0; y < 192; ++y) sy[y] = y + 0.5f;
    }
    /* the fade factor: the middle of the sky now over the brightest it has been on this page */
    {
        const float lum = 0.3f * rr[96] + 0.59f * gg[96] + 0.11f * bb[96];
        if ((int)g_frame - g_sky_frame > 30) g_sky_ref = 0.f;
        g_sky_frame = (int)g_frame;
        if (lum > g_sky_ref) g_sky_ref = lum;
        g_fade = g_sky_ref > 8.f ? lum / g_sky_ref : 1.f;
        if (g_fade > 1.f) g_fade = 1.f;
        if (g_fade < 0.f) g_fade = 0.f;
        if (hal_menu_trace_on() && (g_frame % 60) == 0)
            MTRACE("hostgrad: %d stops (smoothed), mid rgb %.1f %.1f %.1f, fade %.3f", ns, rr[96], gg[96], bb[96], g_fade);
    }
    static const int bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    const size_t stride = (size_t)stride_of(w);
    int seg = 0;
    for (int yy = 0; yy < d[3]; ++yy) {
        const int py = d[1] + yy;
        if (py < 0 || py >= h) continue;
        const float v = (yy + 0.5f) * 192.f / d[3];
        while (seg + 1 < ns && sy[seg + 1] <= v) ++seg;
        float r, g, b;
        if (v <= sy[0] || ns == 1) { r = sr[0]; g = sg[0]; b = sb[0]; }
        else if (seg + 1 >= ns) { r = sr[ns - 1]; g = sg[ns - 1]; b = sb[ns - 1]; }
        else {
            const float t = (v - sy[seg]) / (sy[seg + 1] - sy[seg]);
            r = sr[seg] + (sr[seg + 1] - sr[seg]) * t; g = sg[seg] + (sg[seg + 1] - sg[seg]) * t; b = sb[seg] + (sb[seg + 1] - sb[seg]) * t;
        }
        uint32_t *row = dst + (size_t)py * stride;
        const int xa = d[0] < 0 ? 0 : d[0], xb = d[0] + d[2] > w ? w : d[0] + d[2];
        for (int x = xa; x < xb; ++x) {
            const float dz = (bayer[py & 3][x & 3] - 7.5f) / 16.f;
            int ir = (int)(r + 0.5f + dz), ig = (int)(g + 0.5f + dz), ib = (int)(b + 0.5f + dz);
            ir = ir < 0 ? 0 : ir > 255 ? 255 : ir; ig = ig < 0 ? 0 : ig > 255 ? 255 : ig; ib = ib < 0 ? 0 : ib > 255 ? 255 : ib;
            row[x] = 0xff000000u | ((uint32_t)ir << 16) | ((uint32_t)ig << 8) | (uint32_t)ib;
        }
    }
}

void draw_hostimg(const MenuLayout *L, int i, uint32_t *dst, int w, int h)
{
    const MenuPiece &P = L->p[i];
    const int *d = L->dest[i];
    const uint32_t *src = menu_hostimg_scaled(P.hostimg, d[2], d[3]);
    if (!src) return;
    const size_t stride = (size_t)stride_of(w);
    int f256 = P.hostimg == MENU_IMG_LOGO ? (int)(g_fade * 256.f + 0.5f) : 256;
    /* the plank's own look: selected = lifted, pulsing on the ROM's 8-frame cadence; pressed = sunk and darker */
    const int owns = L->focus >= 0 && L->focus < L->nh && L->hs[L->focus].piece == i;
    int sel = (P.hl & 1) || (owns && g_focus_shown);
    const int prs = (P.hl & 2) || (owns && g_q_real_held);
    int boost = 0, sink = 0;
    if (P.hostimg != MENU_IMG_LOGO) {
        static const int pulse[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
        if (sel) boost = 292 + pulse[g_frame & 7] * 10;    /* x1.14 .. x1.30 */
        if (prs) { f256 = f256 * 216 / 256; boost = 0; sink = d[3] * 3 / 100; if (sink < 1) sink = 1; }
    }
    for (int yy = 0; yy < d[3]; ++yy) {
        const int py = d[1] + yy + sink;
        if (py < 0 || py >= h) continue;
        uint32_t *row = dst + (size_t)py * stride;
        const uint32_t *sr = src + (size_t)yy * d[2];
        for (int xx = 0; xx < d[2]; ++xx) {
            const int px = d[0] + xx;
            if (px < 0 || px >= w) continue;
            const uint32_t c = sr[xx];
            const unsigned a = c >> 24;
            if (!a) continue;
            unsigned cr = ((c >> 16) & 255) * f256 >> 8, cg = ((c >> 8) & 255) * f256 >> 8, cb = (c & 255) * f256 >> 8;
            if (boost) {   /* brightened by boost/256 (a lit plank keeps its lettering dark), premultiplied, capped at alpha */
                cr = cr * boost >> 8; cg = cg * boost >> 8; cb = cb * boost >> 8;
                if (cr > a) cr = a;
                if (cg > a) cg = a;
                if (cb > a) cb = a;
            }
            const uint32_t o = row[px];
            const unsigned ia = 255 - a;
            const unsigned orr = cr + (((o >> 16) & 255) * ia + 127) / 255, og = cg + (((o >> 8) & 255) * ia + 127) / 255,
                           ob = cb + ((o & 255) * ia + 127) / 255;
            row[px] = 0xff000000u | ((orr > 255 ? 255 : orr) << 16) | ((og > 255 ? 255 : og) << 8) | (ob > 255 ? 255 : ob);
        }
    }
}
}  // namespace

void menu_piece_draw(const MenuLayout *L, int i, const MenuSources *S, uint32_t *dst, int w, int h)
{
    if (!S || !dst || i < 0 || i >= L->np) return;
    const MenuPiece &P = L->p[i];
    if (P.src == SRC_HOSTIMG) { draw_hostimg(L, i, dst, w, h); return; }
    if (P.hostgrad) { draw_hostgrad(L, i, S, dst, w, h); return; }
    const int *d = L->dest[i];
    const int x0 = d[0], y0 = d[1], dw = d[2], dh = d[3];
    if (dw <= 0 || dh <= 0) return;
    const size_t stride = (size_t)stride_of(w);
    const uint32_t *subp = P.src == SRC_SUB ? sub_plane(&P, S) : 0;
    /* the visible column range, and one source index per column, computed once per piece */
    const int xa = x0 < 0 ? -x0 : 0;
    const int xb = (x0 + dw > w) ? w - x0 : dw;
    if (xa >= xb) return;
    static std::vector<int> colx;
    if ((int)colx.size() < dw) colx.resize((size_t)dw);
    int *cx = &colx[0];
    if (P.src == SRC_TOP_PIC) {
        if (!S->pic || S->pic_rw < 1 || S->pic_rh < 1) return;
        for (int xx = xa; xx < xb; ++xx) {
            /* exact rational map of the destination pixel into the host picture */
            const long long ux = (long long)P.s.x * dw + (long long)xx * P.s.w;
            const int qx = S->pic_rx + (int)fdiv(ux * S->pic_rw, 256LL * dw);
            cx[xx] = (qx < 0 || qx >= S->pic_w) ? -1 : qx;
        }
    } else if (P.src != SRC_TOP_HOSTROW) {
        const int lim = 256;
        for (int xx = xa; xx < xb; ++xx) {
            int sx = P.xfill ? P.s.x : P.lden > 0 ? P.s.x + (int)fdiv((long long)xx * P.lden, P.lnum > 0 ? P.lnum : 1)
                                : P.s.x + (int)fdiv((long long)xx * P.s.w, dw);
            if (P.lden > 0 && sx > P.s.x + P.s.w - 1) sx = P.s.x + P.s.w - 1;
            cx[xx] = (sx < 0 || sx >= lim) ? -1 : sx;
        }
    }
    const uint32_t bd = S->sub_backdrop & 0xffffffu;
    for (int yy = 0; yy < dh; ++yy) {
        const int py = y0 + yy;
        if (py < 0 || py >= h) continue;
        uint32_t *row = dst + (size_t)py * stride + x0;
        if (P.src == SRC_TOP_HOSTROW) {
            if (!S->hostrow) return;
            const int sy = P.s.y + yy;
            if (sy < 0 || sy >= S->hostrow_h) continue;
            for (int xx = xa; xx < xb; ++xx) {
                const int sx = P.s.x + xx;
                if (sx < 0 || sx >= S->hostrow_w) continue;
                const uint32_t c = S->hostrow[(size_t)sy * S->hostrow_stride + sx];
                if (c >> 24) row[xx] = c | 0xff000000u;
            }
            continue;
        }
        if (P.src == SRC_TOP_PIC) {
            const long long uy = (long long)P.s.y * dh + (long long)yy * P.s.h;
            const int qy = S->pic_ry + (int)fdiv(uy * S->pic_rh, 192LL * dh);
            if (qy < 0 || qy >= S->pic_h) continue;
            const uint32_t *prow = S->pic + (size_t)qy * S->pic_stride;
            for (int xx = xa; xx < xb; ++xx) {
                const int qx = cx[xx];
                if (qx < 0) continue;
                if (P.keycover) {
                    if (!S->pic_cover || !S->pic_cover[(size_t)qy * S->pic_stride + qx]) continue;
                }
                row[xx] = prow[qx] | 0xff000000u;
            }
            continue;
        }
        int sy = P.lden > 0 ? P.s.y + (int)fdiv((long long)yy * P.lden, P.lnum > 0 ? P.lnum : 1)
                            : P.s.y + (int)fdiv((long long)yy * P.s.h, dh);
        if (P.lden > 0 && sy > P.s.y + P.s.h - 1) sy = P.s.y + P.s.h - 1;
        if (P.row_ok && !P.row_ok(sy)) continue;
        if (sy < 0 || sy >= 192) continue;
        if (P.src == SRC_SUB) {
            if (!subp) continue;
            const uint32_t *srow = subp + sy * 256;
            for (int xx = xa; xx < xb; ++xx) {
                const int sx = cx[xx];
                if (sx < 0) continue;
                const uint32_t c = srow[sx];
                if (P.key_backdrop && (c & 0xffffffu) == bd) continue;
                row[xx] = c | 0xff000000u;
            }
        } else {   /* SRC_TOP_2D */
            if (!S->top2d) continue;
            const MenuTopCell *trow = S->top2d + sy * 256;
            for (int xx = xa; xx < xb; ++xx) {
                const int sx = cx[xx];
                if (sx < 0) continue;
                const MenuTopCell &c = trow[sx];
                if ((c.argb >> 24) == 0) continue;
                if (c.owner > 4 || !((P.layers >> c.owner) & 1)) continue;
                uint32_t col = c.argb;
                if (S->bm_apply) col = S->bm_apply(col, c.bm);
                row[xx] = col | 0xff000000u;
            }
        }
    }
}

void menu_dim(uint32_t *dst, int w, int h, int evy)
{
    if (!dst || evy <= 0) return;
    const size_t st = (size_t)stride_of(w);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) dst[y * st + x] = blend_ch(dst[y * st + x], evy, 0);
}

/* The 8-frame pulse: evy 0,1,2,3,4,3,2,1 toward white; the outline is one
   canvas unit (at least one pixel) in the darkest colour of the hotspot's own
   rim, sampled before the outline is drawn. */
void menu_highlight_paint(const MenuLayout *L, uint32_t *dst, int w, int h, unsigned frame)
{
    if (!dst || L->focus < 0 || L->focus >= L->nh) return;
    int r[4];
    if (!menu_hotspot_rect(L, L->focus, r)) return;
    static const int pulse[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
    const int evy = pulse[frame & 7];
    const int x0 = r[0], y0 = r[1], x1 = r[0] + r[2], y1 = r[1] + r[3];
    int t = u2h(L, 1);
    if (t < 1) t = 1;
    uint32_t dark = 0xff000000u;
    int dl = 1 << 30;
    for (int y = y0; y < y1; ++y) {
        if (y < 0 || y >= h) continue;
        for (int x = x0; x < x1; ++x) {
            if (x < 0 || x >= w) continue;
            uint32_t &px = dst[(size_t)y * stride_of(w) + x];
            const int rim = (x < x0 + 1 || x >= x1 - 1 || y < y0 + 1 || y >= y1 - 1);
            if (rim && (px >> 24)) {
                const int l = luma(px);
                if (l < dl) { dl = l; dark = px | 0xff000000u; }
            }
            if ((px >> 24) && evy) px = blend_ch(px, evy, 1);
        }
    }
    for (int y = y0; y < y1; ++y) {
        if (y < 0 || y >= h) continue;
        for (int x = x0; x < x1; ++x) {
            if (x < 0 || x >= w) continue;
            if (x < x0 + t || x >= x1 - t || y < y0 + t || y >= y1 - t) dst[(size_t)y * stride_of(w) + x] = dark;
        }
    }
}

/* Opaque pixels on a source-rect edge that continue across it. An edge that
   lies on the plane border cannot cut anything and is not counted. A pixel
   counts when it is opaque inside the rect on the edge AND the pixel just
   outside is opaque too: that is a glyph cut in two (an autocropped rect has
   opaque edge pixels by construction and must still read 0). counts: top,
   bottom, left, right. Returns the total. */
int menu_seam_census(const MenuLayout *L, int piece, const MenuSources *S, int counts[4])
{
    counts[0] = counts[1] = counts[2] = counts[3] = 0;
    if (piece < 0 || piece >= L->np || !S) return 0;
    const MenuPiece &P = L->p[piece];
    if (P.src == SRC_TOP_PIC || P.src == SRC_HOSTIMG || P.hostgrad) return 0;
    const int pw = P.src == SRC_TOP_HOSTROW ? S->hostrow_w : 256;
    const int ph = P.src == SRC_TOP_HOSTROW ? S->hostrow_h : 192;
    const int sx0 = P.s.x, sy0 = P.s.y, sx1 = P.s.x + P.s.w - 1, sy1 = P.s.y + P.s.h - 1;
    for (int x = sx0; x <= sx1; ++x) {
        if (sy0 > 0 && src_opaque(&P, S, x, sy0) && src_opaque(&P, S, x, sy0 - 1)) ++counts[0];
        if (sy1 < ph - 1 && src_opaque(&P, S, x, sy1) && src_opaque(&P, S, x, sy1 + 1)) ++counts[1];
    }
    for (int y = sy0; y <= sy1; ++y) {
        if (sx0 > 0 && src_opaque(&P, S, sx0, y) && src_opaque(&P, S, sx0 - 1, y)) ++counts[2];
        if (sx1 < pw - 1 && src_opaque(&P, S, sx1, y) && src_opaque(&P, S, sx1 + 1, y)) ++counts[3];
    }
    const int tot = counts[0] + counts[1] + counts[2] + counts[3];
    if (tot) MTRACE("seam census: piece %d has %d cut pixels (top %d bottom %d left %d right %d)", piece, tot,
                    counts[0], counts[1], counts[2], counts[3]);
    return tot;
}

/* the opaque bounding box of the piece's layers inside its s; 0 if nothing is opaque */
int menu_autocrop_rect(const MenuPiece *P, const MenuSources *S, DsRect *out)
{
    int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
    for (int y = P->s.y; y < P->s.y + P->s.h; ++y)
        for (int x = P->s.x; x < P->s.x + P->s.w; ++x)
            if (src_opaque(P, S, x, y)) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    if (x1 < 0) return 0;
    out->x = (short)x0; out->y = (short)y0; out->w = (short)(x1 - x0 + 1); out->h = (short)(y1 - y0 + 1);
    return 1;
}

/* ====================================================================== */
/* the synthesised-touch queue                                            */
/* ====================================================================== */

namespace {
int g_q_state;          /* 0 idle, 1 press frame 1, 2 press frame 2, 3 release due */
int g_q_x, g_q_y;
}

int hal_menu_touch_queue_press(int dsx, int dsy)
{
    if (g_q_state != 0 || g_q_real_held) return 0;
    g_q_x = dsx;
    g_q_y = dsy;
    g_q_state = 1;
    return 1;
}

int hal_menu_touch_queue_poll(int *dsx, int *dsy, int *down)
{
    if (g_q_state == 0) return 0;
    if (g_q_real_held) { g_q_state = 0; return 0; }   /* a real stylus wins; the press is dropped */
    *dsx = g_q_x;
    *dsy = g_q_y;
    if (g_q_state == 1 || g_q_state == 2) {
        *down = 1;
        ++g_q_state;
        return 1;
    }
    *down = 0;      /* the release edge, then idle */
    g_q_state = 0;
    return 1;
}

void hal_menu_touch_real_held(int held) { g_q_real_held = held ? 1 : 0; }

/* ====================================================================== */
/* frame entry points                                                     */
/* ====================================================================== */

static int scene_is_menu(int scene)
{
    return scene == 1 || scene == 4 || scene == 5 || scene == 6 || scene == 8;
}

void hal_menu_set_enable_fn(int (*fn)(int key)) { g_enable = fn; }
void hal_menu_set_netplay(int on) { g_netplay = on ? 1 : 0; }
void hal_menu_set_sources(const MenuSources *s) { g_src = s; g_nplanes = 0; }
int  hal_menu_live(void) { return g_live; }
int  hal_menu_draws_in(int stage) { return g_live && g_cur.stage == stage; }
int  hal_menu_fallback_frames(void) { return g_fallback_frames; }
int  hal_menu_enabled(int key) { return enabled(key); }
int  hal_menu_uses_cover(void)
{
    if (!g_live) return 0;
    for (int i = 0; i < g_cur.np; ++i)
        if (g_cur.p[i].keycover) return 1;
    return 0;
}
const MenuLiftInfo *hal_menu_lift_info(void) { return g_lift_on ? &g_lift : 0; }
int  hal_menu_trace_on(void) { return trace_on(); }
void hal_menu_set_dst_stride(int stride) { g_dst_stride = stride; }
void hal_menu_note_lift_drawn(void) { g_lift_pending = 0; }
int  hal_menu_uses_src(int src)
{
    if (!g_live) return 0;
    for (int i = 0; i < g_cur.np; ++i)
        if ((int)g_cur.p[i].src == src) return 1;
    return 0;
}

void hal_menu_frame_begin(int w, int h, int scene_id)
{
    ++g_frame;
    g_nplanes = 0;
    /* THE LIFT PROMISE FROM LAST FRAME, CHECKED (the net under the whole lift/draw mechanism, kept from the
       level-clear compose: the 0.4.1 defect was text lifted out of the picture and never drawn back). Once per
       process, to stderr, never fatal. */
    if (g_lift_pending) {
        static int said;
        if (!said) {
            said = 1;
            fprintf(stderr, "[compose] LIFT WITHOUT DRAW: a menu recipe lifted engine-A pixels out of a frame its "
                    "draw never reached; the frame predicate is missing a term the draw depends on\n");
            fflush(stderr);
        }
        g_lift_pending = 0;
    }
    g_lift_on = 0;
    int pick = -1, variant = 0, fb = -1;
    for (int i = 0; i < g_menu_nrecipes; ++i) {
        const MenuRecipe &R = g_menu_recipes[i];
        if (R.flags & MENU_RF_FALLBACK) { fb = i; continue; }
        if (!R.up || !R.build || !enabled(R.enable_key)) continue;
        const int v = R.up(scene_id);
        if (v > 0) { pick = i; variant = v; break; }
    }
    int is_fb = 0;
    if (pick < 0 && fb >= 0 && scene_id != MENU_SCENE_GATED && scene_is_menu(scene_id) &&
        enabled(MENU_KEY_ONE_SCREEN_MENUS)) {
        pick = fb;
        variant = 1;
        is_fb = 1;
    }
    if (pick < 0 || w < 8 || h < 8) {
        if (g_live) MTRACE("recipe off (scene %d)", scene_id);
        g_live = 0;
        g_cur_recipe = -1;
        g_focus_shown = 0;
        g_drag_piece = -1;
        g_latch_recipe = -2;
        g_drawn_valid = 0;      /* the drawn layout is the old menu's: never invert through it */
        g_q_state = 0;          /* a queued synthesised press must not fire into the next scene */
        return;
    }
    const MenuRecipe *R = &g_menu_recipes[pick];
    if (pick != g_cur_recipe || variant != g_cur_variant) {
        /* a new recipe or variant: the first tick must not read the old rects, and an
           old press (aimed at the old rects) is dropped */
        g_drawn_valid = 0;
        g_drag_piece = -1;
        g_q_state = 0;
    }
    const int changed = pick != g_cur_recipe || variant != g_cur_variant || g_cur.w != w || g_cur.h != h;
    int keep_focus = (pick == g_cur_recipe && variant == g_cur_variant) ? g_cur.focus : -1;
    menu_layout_solve(&g_cur, R, variant, w, h);
    g_cur.focus = keep_focus < g_cur.nh ? keep_focus : -1;
    if (pick != g_latch_recipe || variant != g_latch_variant) {
        memset(g_latched, 0, sizeof g_latched);
        g_latch_recipe = pick;
        g_latch_variant = variant;
        g_focus_shown = 0;
    }
    for (int i = 0; i < g_cur.np; ++i)
        if (g_latched[i]) { g_cur.p[i].s = g_latch[i]; }
    menu_layout_dest(&g_cur);
    g_live = g_cur.np > 0;
    g_cur_recipe = pick;
    g_cur_variant = variant;
    if (is_fb) ++g_fallback_frames;
    /* the lift plan for the engine-A compositor, one truth for the frame */
    memset(&g_lift, 0, sizeof g_lift);
    if (g_live) {
        g_lift.lift2d = g_cur.lift_owners;
        g_lift.hide = g_cur.hide_owners;
        for (int i = 0; i < g_cur.np && g_lift.nbands < 4; ++i)
            if (g_cur.p[i].src == SRC_TOP_HOSTROW) {
                g_lift.band[g_lift.nbands].layers = g_cur.p[i].layers;
                g_lift.band[g_lift.nbands].r0 = g_cur.p[i].s.y;
                g_lift.band[g_lift.nbands].r1 = g_cur.p[i].s.y + g_cur.p[i].s.h;
                ++g_lift.nbands;
            }
        g_lift_on = (g_lift.lift2d | g_lift.hide | g_lift.nbands) != 0;
        g_lift_pending = (g_lift.lift2d | g_lift.nbands) != 0;
    }
    if (changed) {
        MTRACE("recipe=%s variant=%d size=%dx%d unit=%d/%d canvas_w=%d %s stage=%d fallback_frames=%d", R->name,
               variant, w, h, g_cur.unum, g_cur.uden, g_cur.canvas_w, menu_wide(&g_cur) ? "wide" : "narrow",
               g_cur.stage, g_fallback_frames);
        for (int i = 0; i < g_cur.np; ++i)
            MTRACE("  piece %d src=%d s=(%d,%d,%d,%d) -> dest=(%d,%d,%d,%d) k=%d/%d touch=%d", i, (int)g_cur.p[i].src,
                   g_cur.p[i].s.x, g_cur.p[i].s.y, g_cur.p[i].s.w, g_cur.p[i].s.h, g_cur.dest[i][0],
                   g_cur.dest[i][1], g_cur.dest[i][2], g_cur.dest[i][3], g_cur.p[i].knum, g_cur.p[i].kden,
                   g_cur.p[i].touch);
        for (int i = 0; i < g_cur.nh; ++i) {
            int r[4];
            menu_hotspot_rect(&g_cur, i, r);
            MTRACE("  hotspot %d piece=%d box=(%d,%d,%d,%d) host=(%d,%d,%d,%d) rom=%d", i, g_cur.hs[i].piece,
                   g_cur.hs[i].box.x, g_cur.hs[i].box.y, g_cur.hs[i].box.w, g_cur.hs[i].box.h, r[0], r[1], r[2],
                   r[3], g_cur.hs[i].rom_index);
        }
    }
}

/* ASSET1: what a menu frame costs, in microseconds, summed per process and printed (SM64DS_MENU_TRACE) every 120
   drawn frames: the recipe's draw here, and the host's source preparation (hal_menu_note_cost, sub_screen.cpp). */
static double g_cost_draw_us, g_cost_prep_us;
static int g_cost_n;
void hal_menu_note_cost(double prep_us) { g_cost_prep_us += prep_us; }

void hal_menu_present(uint32_t *dst, int w, int h, int stage)
{
    if (!g_live || !dst || g_cur.stage != stage) return;
    const std::chrono::steady_clock::time_point t_in = std::chrono::steady_clock::now();
    struct CostEnd {
        std::chrono::steady_clock::time_point t;
        ~CostEnd()
        {
            g_cost_draw_us += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t).count();
            if (++g_cost_n % 120 == 0 && trace_on())
                fprintf(stderr, "[menu] cost over %d drawn frames: draw %.0f us, source prep %.0f us per frame\n", g_cost_n,
                        g_cost_draw_us / g_cost_n, g_cost_prep_us / g_cost_n);
        }
    } cost_end = { t_in };
    const MenuSources *S = g_src;
    g_nplanes = 0;
    if (g_cur.w != w || g_cur.h != h) {       /* the picture changed under the frame: solve again */
        const MenuRecipe *R = g_cur.recipe;
        const int f = g_cur.focus;
        menu_layout_solve(&g_cur, R, g_cur_variant, w, h);
        g_cur.focus = f < g_cur.nh ? f : -1;
        for (int i = 0; i < g_cur.np; ++i)       /* the latched autocrops belong to this open, not to one size */
            if (g_latched[i]) g_cur.p[i].s = g_latch[i];
        menu_layout_dest(&g_cur);
    }
    if (S) {   /* latch the autocrop rects the first frame their pieces have pixels */
        int again = 0;
        for (int i = 0; i < g_cur.np; ++i) {
            if (!g_cur.p[i].autocrop || g_latched[i]) continue;
            DsRect r;
            if (menu_autocrop_rect(&g_cur.p[i], S, &r)) {
                g_latch[i] = r;
                g_latched[i] = 1;
                g_cur.p[i].s = r;
                again = 1;
                MTRACE("autocrop latched piece %d -> (%d,%d,%d,%d)", i, r.x, r.y, r.w, r.h);
            }
        }
        if (again) menu_layout_dest(&g_cur);
    }
    if (g_cur.backdrop == MENU_BACKDROP_DIM_AUTO && S && !S->rom_dimming) menu_dim(dst, w, h, 8);
    for (int i = 0; i < g_cur.np; ++i) menu_piece_draw(&g_cur, i, S, dst, w, h);
    if (g_cur.focus >= 0 && g_focus_shown && g_cur.focus < g_cur.nh && g_cur.p[g_cur.hs[g_cur.focus].piece].src != SRC_HOSTIMG)
        menu_highlight_paint(&g_cur, dst, w, h, g_frame);
    g_drawn = g_cur;
    g_drawn_valid = 1;
    g_lift_pending = 0;      /* the draw honoured the lift */
}

void hal_menu_touch_inverse(int bx, int by, int *dsx, int *dsy, int *in)
{
    if (!g_drawn_valid || !g_live) { *dsx = -1; *dsy = -1; *in = 0; return; }
    menu_inverse(&g_drawn, bx, by, &g_drag_piece, dsx, dsy, in);
    /* menu_inverse sets the drag piece only on a hit; MenuInverse is also what
       a held stylus clamps to, so a miss keeps whatever piece it started on */
}

void hal_menu_touch_end(void) { g_drag_piece = -1; }

int hal_menu_hover(int bx, int by)
{
    if (!g_live) return 0;
    if (!menu_focus_hover(&g_cur, bx, by)) return 0;
    g_focus_shown = 1;
    return 1;
}

void hal_menu_pad(uint16_t *pressed, int host_back)
{
    if (!g_live || !pressed || !g_cur.recipe) return;
    const MenuRecipe *R = g_cur.recipe;
    if (R->pad_ok && !R->pad_ok()) return;      /* the ROM is not reading the menu this frame */
    if (R->pad_map) { R->pad_map(pressed, host_back); return; }   /* TITLE1: a native recipe with its own key meaning */
    int drv = R->driver;
    if (g_netplay && drv == MENU_DRV_ROM_CURSOR) drv = MENU_DRV_TOUCH;   /* never write local ROM state in a session */
    if (g_netplay && drv == MENU_DRV_MIXED) drv = MENU_DRV_TOUCH;
    /* the host back key (Backspace / the bound B-equivalent) presses the recipe's named back
       hotspot as a synthesised touch, whatever the driver: the ROM has no pad back */
    if (host_back && g_cur.back >= 0 && g_cur.back < g_cur.nh) {
        const MenuHotspot &H = g_cur.hs[g_cur.back];
        hal_menu_touch_queue_press(H.box.x + H.box.w / 2, H.box.y + H.box.h / 2);
    }
    if (drv == MENU_DRV_NATIVE) return;                                   /* the ROM reads the pad itself */
    uint16_t k = *pressed;
    int dir = -1;
    if (k & MENU_PAD_UP) dir = MENU_DIR_UP;
    else if (k & MENU_PAD_DOWN) dir = MENU_DIR_DOWN;
    else if (k & MENU_PAD_LEFT) dir = MENU_DIR_LEFT;
    else if (k & MENU_PAD_RIGHT) dir = MENU_DIR_RIGHT;

    if (drv == MENU_DRV_ROM_CURSOR) {
        if (dir >= 0 && R->rom_cursor) { int sel = 0; R->rom_cursor(dir, &sel); }
        k = (uint16_t)(k & ~0xf0u);
        if (k & MENU_PAD_CONFIRM) k = (uint16_t)((k & ~(unsigned)MENU_PAD_CONFIRM) | MENU_PAD_START);
        *pressed = k;
        return;
    }

    /* TOUCH, and MIXED (vertical keys go to the ROM cursor when the recipe has one) */
    int handled_dir = 0, owns = 0;
    if (drv == MENU_DRV_MIXED && R->rom_cursor) {
        /* PAUSE1 contract (status\PAUSE1.md): called every pad frame (dir -1 = none pressed); a nonzero
           return consumes the direction; owns = 1 means this page's plates are the ROM cursor, so A is
           the ROM's own confirm (Start), exactly as the ROM_CURSOR driver does it */
        const int r = R->rom_cursor(dir, &owns);
        if (dir >= 0 && r) handled_dir = 1;
    }
    if (dir >= 0 && !handled_dir) {
        if (!g_focus_shown) {          /* the first press only shows the cursor */
            g_focus_shown = 1;
            if (g_cur.focus < 0 && g_cur.nh > 0) g_cur.focus = 0;
        } else {
            menu_focus_move(&g_cur, dir);
        }
    }
    if ((k & MENU_PAD_CONFIRM) && owns) {
        k = (uint16_t)((k & ~(unsigned)MENU_PAD_CONFIRM) | MENU_PAD_START);
    } else if (k & MENU_PAD_CONFIRM) {
        if (g_focus_shown && g_cur.focus >= 0 && g_cur.focus < g_cur.nh) {
            const MenuHotspot &H = g_cur.hs[g_cur.focus];
            hal_menu_touch_queue_press(H.box.x + H.box.w / 2, H.box.y + H.box.h / 2);
        }
        k = (uint16_t)(k & ~(unsigned)MENU_PAD_CONFIRM);
    }
    k = (uint16_t)(k & ~0xf0u);          /* direction bits are the port's while a recipe is up */
    *pressed = k;
}

/* ====================================================================== */
/* MENU LAYOUT SELFTEST                                                   */
/* ====================================================================== */

namespace {

FILE *g_out;

struct Aspect { const char *name; int aw, ah; };
const Aspect kAspects[5] = { { "4:3", 4, 3 }, { "16:10", 16, 10 }, { "16:9", 16, 9 }, { "21:9", 21, 9 },
                             { "32:9", 32, 9 } };

void out(const char *s)
{
    fputs(s, stderr);
    if (g_out) fputs(s, g_out);
}

/* every hotspot reaches every other by directions alone */
int focus_connected(const MenuLayout *L, int *from, int *to)
{
    if (L->nh < 2) return 1;
    for (int a = 0; a < L->nh; ++a) {
        char seen[MENU_MAX_HOTSPOTS];
        int stack[MENU_MAX_HOTSPOTS], sp = 0;
        memset(seen, 0, sizeof seen);
        seen[a] = 1;
        stack[sp++] = a;
        while (sp) {
            const int c = stack[--sp];
            for (int d = 0; d < 4; ++d) {
                MenuLayout T = *L;
                T.focus = c;
                const int n = menu_focus_move(&T, d);
                if (n >= 0 && !seen[n]) { seen[n] = 1; stack[sp++] = n; }
            }
        }
        for (int b = 0; b < L->nh; ++b)
            if (!seen[b]) { *from = a; *to = b; return 0; }
    }
    return 1;
}

struct Result { int scale, corners, outside, hot, bounds, overlap, focus, exact; char why[96]; };

void check_layout(const MenuLayout *L, Result *R)
{
    R->scale = R->corners = R->outside = R->hot = R->bounds = R->overlap = R->focus = R->exact = 1;
    R->why[0] = 0;
    char *why = R->why;
    for (int i = 0; i < L->np; ++i) {
        const MenuPiece &P = L->p[i];
        const int *d = L->dest[i];
        if (P.src != SRC_TOP_HOSTROW && P.lden == 0 && !menu_scale_allowed(P.knum, P.kden)) {
            R->scale = 0; sprintf(why, "piece %d scale %d/%d", i, P.knum, P.kden);
        }
        if (d[2] < 1 || d[3] < 1 || d[0] < 0 || d[1] < 0 || d[0] + d[2] > L->w || d[1] + d[3] > L->h) {
            R->bounds = 0; sprintf(why, "piece %d dest (%d,%d,%d,%d) outside %dx%d", i, d[0], d[1], d[2], d[3], L->w, L->h);
        }
        if (P.src != SRC_TOP_HOSTROW && P.lden == 0 && !P.xfill) {
            /* the scale rule: dest size within one host pixel of the exact rational size */
            const double kn = P.knum ? P.knum : 1, kd = P.kden ? P.kden : 1, un = (double)L->unum / L->uden;
            const double ew = P.s.w * kn / kd * un, eh = P.s.h * kn / kd * un;
            const double cew = ew > L->w ? L->w : ew, ceh = eh > L->h ? L->h : eh;
            const double dw = d[2] - cew, dh = d[3] - ceh;
            if (dw >= 1.0 || dw <= -1.0 - (ew > L->w ? 1.0 : 0.0) || dh >= 1.0 || dh <= -1.0 - (eh > L->h ? 1.0 : 0.0)) {
                R->exact = 0; sprintf(why, "piece %d dest %dx%d vs exact %.2fx%.2f", i, d[2], d[3], ew, eh);
            }
        }
        if (P.xfill) { /* a fill is not a scaled copy: its width is fill_w units */ }
        if (P.touch && P.src != SRC_SUB && P.src != SRC_TOP_PIC && P.src != SRC_HOSTIMG) { R->corners = 0; sprintf(why, "piece %d touch but not SRC_SUB", i); }
        for (int j = i + 1; j < L->np; ++j) {
            if (P.bg || L->p[j].bg) continue;
            const int *e = L->dest[j];
            if (d[0] < e[0] + e[2] && e[0] < d[0] + d[2] && d[1] < e[1] + e[3] && e[1] < d[1] + d[3]) {
                R->overlap = 0; sprintf(why, "pieces %d and %d overlap", i, j);
            }
        }
        if (!P.touch || d[2] < 1 || d[3] < 1) continue;
        const int cx[4] = { d[0], d[0] + d[2] - 1, d[0], d[0] + d[2] - 1 };
        const int cy[4] = { d[1], d[1], d[1] + d[3] - 1, d[1] + d[3] - 1 };
        for (int c = 0; c < 4; ++c) {
            int pi = -1, sx, sy;
            const int in = menu_hit(L, cx[c], cy[c], &pi, &sx, &sy);
            if (!in || pi != i || sx < P.s.x || sx >= P.s.x + P.s.w || sy < P.s.y || sy >= P.s.y + P.s.h) {
                R->corners = 0; sprintf(why, "piece %d corner %d inverts to piece %d (%d,%d) in=%d", i, c, pi, sx, sy, in);
            }
        }
        const int ox[4] = { d[0] - 1, d[0] + d[2], d[0] + d[2] / 2, d[0] + d[2] / 2 };
        const int oy[4] = { d[1] + d[3] / 2, d[1] + d[3] / 2, d[1] - 1, d[1] + d[3] };
        for (int c = 0; c < 4; ++c) {
            int pi = -1, sx, sy;
            if (menu_hit(L, ox[c], oy[c], &pi, &sx, &sy)) {
                if (pi == i) { R->outside = 0; sprintf(why, "piece %d edge %d still inside", i, c); }
                continue;   /* another touch piece owns that pixel: not this piece's to disown */
            }
            int dsx, dsy, in, drag = -1;
            menu_inverse(L, ox[c], oy[c], &drag, &dsx, &dsy, &in);
            if (in) { R->outside = 0; sprintf(why, "piece %d edge %d reads in", i, c); }
        }
    }
    for (int k = 0; k < L->nh; ++k) {
        const MenuHotspot &H = L->hs[k];
        if (H.piece >= L->np || !L->p[H.piece].touch) { R->hot = 0; sprintf(why, "hotspot %d not on a touch piece", k); continue; }
        const MenuPiece &P = L->p[H.piece];
        if (H.box.x < P.s.x || H.box.y < P.s.y || H.box.x + H.box.w > P.s.x + P.s.w ||
            H.box.y + H.box.h > P.s.y + P.s.h) {
            R->hot = 0; sprintf(why, "hotspot %d box outside its piece source", k); continue;
        }
        int hx, hy, pi = -1, sx = -1, sy = -1;
        menu_hotspot_centre(L, k, &hx, &hy);
        const int in = menu_hit(L, hx, hy, &pi, &sx, &sy);
        if (!in || pi != H.piece || sx < H.box.x || sx >= H.box.x + H.box.w || sy < H.box.y || sy >= H.box.y + H.box.h) {
            R->hot = 0; sprintf(why, "hotspot %d centre (%d,%d) -> piece %d ds (%d,%d) in=%d", k, hx, hy, pi, sx, sy, in);
        }
    }
    int a = 0, b = 0;
    if (!focus_connected(L, &a, &b)) { R->focus = 0; sprintf(why, "hotspot %d cannot reach %d", a, b); }
}

struct GridSize { const char *name; int w, h; };

int run_grid(void)
{
    int fails = 0, cases = 0;
    static const int heights[2] = { 384, 768 };
    /* odd sizes: not a whole number of canvas units wide, odd heights, narrow windows */
    static const GridSize odd[5] = { { "1022x767", 1022, 767 }, { "1366x768", 1366, 768 }, { "1365x767", 1365, 767 },
                                     { "2560x1079", 2560, 1079 }, { "641x480", 641, 480 } };
    char line[256];
    out("MENU LAYOUT SELFTEST\n");
    out("recipe     h    aspect  size       var wide np nh  scale exact corner outsd hot  bounds overlap focus  result\n");
    for (int ri = 0; ri < g_menu_nrecipes; ++ri) {
        const MenuRecipe &Rc = g_menu_recipes[ri];
        if (Rc.nvariants == 0) {
            sprintf(line, "%-10s stub (no layout yet): skipped\n", Rc.name);
            out(line);
            continue;
        }
        for (int v = 1; v <= Rc.nvariants; ++v)
            for (int ci = 0; ci < 15; ++ci) {
                int w, h;
                const char *an;
                if (ci < 10) {
                    h = heights[ci / 5];
                    const Aspect &A = kAspects[ci % 5];
                    w = (h * A.aw * 2 + A.ah) / (A.ah * 2);
                    an = A.name;
                } else {
                    w = odd[ci - 10].w; h = odd[ci - 10].h; an = "odd";
                }
                static MenuLayout L;
                menu_layout_solve(&L, &Rc, v, w, h);
                Result R;
                check_layout(&L, &R);
                const int ok = L.np > 0 && R.scale && R.exact && R.corners && R.outside && R.hot && R.bounds && R.overlap && R.focus;
                ++cases;
                if (!ok) ++fails;
                sprintf(line, "%-10s %-4d %-7s %4dx%-5d %-3d %-4s %-2d %-2d  %-5s %-5s %-6s %-5s %-4s %-6s %-7s %-5s  %s%s%s\n",
                        Rc.name, h, an, w, h, v, menu_wide(&L) ? "wide" : "nrw", L.np, L.nh,
                        R.scale ? "ok" : "FAIL", R.exact ? "ok" : "FAIL", R.corners ? "ok" : "FAIL",
                        R.outside ? "ok" : "FAIL", R.hot ? "ok" : "FAIL", R.bounds ? "ok" : "FAIL",
                        R.overlap ? "ok" : "FAIL", R.focus ? "ok" : "FAIL", ok ? "PASS" : "FAIL", ok ? "" : " : ",
                        ok ? "" : R.why);
                out(line);
            }
    }
    sprintf(line, "grid: %d cases, %d failed\n", cases, fails);
    out(line);
    return fails;
}

uint32_t t_bm(uint32_t argb, uint8_t bm) { return bm ? (argb & 0xfefefeu) | (argb & 0xff000000u) : argb; }
int fn_enable_all(int) { return 1; }

int check(const char *what, int ok)
{
    char line[160];
    sprintf(line, "live: %-58s %s\n", what, ok ? "PASS" : "FAIL");
    out(line);
    return ok ? 0 : 1;
}

/* the frame path end to end: frame_begin, present, inverse, pad, queue, fallback, census, autocrop */
int run_live(void)
{
    int fails = 0;
    /* heap, not static: ~6.9 MB of .bss in the game exe pushed the image end past the budget
       (it must stay well below 0x02000000); the selftest owns these only while it runs */
    uint32_t *sub = (uint32_t *)calloc(256 * 192, sizeof(uint32_t));
    MenuTopCell *top = (MenuTopCell *)calloc(256 * 192, sizeof(MenuTopCell));
    uint32_t *pic = (uint32_t *)calloc(1024 * 768, sizeof(uint32_t));
    uint32_t *dst = (uint32_t *)calloc(1024 * 768, sizeof(uint32_t));
    if (!sub || !top || !pic || !dst) {
        free(sub); free(top); free(pic); free(dst);
        return check("selftest buffers allocate", 0);
    }
    const uint32_t kBack = 0xff102030u, kCol = 0xff336699u;
    for (int i = 0; i < 256 * 192; ++i) sub[i] = kCol;
    for (int y = 0; y < 192; ++y) for (int x = 0; x < 8; ++x) sub[y * 256 + x] = kBack;   /* keyed columns */
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 256; ++x) {
            MenuTopCell &c = top[y * 256 + x];
            c.argb = 0;
            c.owner = 3;
            if (y >= 20 && y < 60 && x >= 30 && x < 90) { c.argb = 0xffeeeeefu; c.bm = 1; }   /* BG3, bm verdict set */
            if (y >= 20 && y < 60 && x >= 120 && x < 160) { c.argb = 0xffffffffu; c.owner = 0; } /* BG0: not in the piece's mask */
        }
    for (int i = 0; i < 1024 * 768; ++i) pic[i] = 0xff202020u;
    MenuSources S;
    memset(&S, 0, sizeof S);
    S.sub = sub; S.sub_backdrop = kBack; S.top2d = top; S.pic = pic;
    S.pic_stride = 1024; S.pic_w = 1024; S.pic_h = 768; S.pic_rx = 128; S.pic_ry = 0; S.pic_rw = 768; S.pic_rh = 768;
    S.bm_apply = t_bm;
    hal_menu_set_sources(&S);
    hal_menu_set_enable_fn(0);
    hal_menu_frame_begin(1024, 768, 1);
    fails += check("nothing enabled -> scene 1 not live (default build state)", !hal_menu_live());
    hal_menu_frame_begin(1024, 768, 3);
    fails += check("nothing enabled -> scene 3 not live", !hal_menu_live());
    hal_menu_set_enable_fn(fn_enable_all);
    hal_menu_frame_begin(1024, 768, MENU_SCENE_TEST);
    fails += check("TEST recipe up at 1024x768", hal_menu_live() == 1);
    for (int i = 0; i < 1024 * 768; ++i) dst[i] = 0xff808080u;
    hal_menu_present(dst, 1024, 768, MENU_STAGE_PRE_FADE);
    fails += check("wrong stage draws nothing", dst[0] == 0xff808080u);
    hal_menu_present(dst, 1024, 768, MENU_STAGE_POST_FADE);
    {
        int hx, hy;
        menu_hotspot_centre(&g_drawn, 0, &hx, &hy);
        fails += check("hotspot 0 centre shows the sub colour", dst[hy * 1024 + hx] == kCol);
        const uint32_t dimmed = blend_ch(0xff808080u, 8, 0);
        fails += check("backdrop dimmed by evy 8 where no piece draws", dst[767 * 1024 + 1023] == dimmed);
        int dsx, dsy, in;
        hal_menu_touch_inverse(hx, hy, &dsx, &dsy, &in);
        const MenuHotspot &H = g_drawn.hs[0];
        fails += check("inverse of hotspot 0 centre is inside its DS box",
                       in && dsx >= H.box.x && dsx < H.box.x + H.box.w && dsy >= H.box.y && dsy < H.box.y + H.box.h);
        hal_menu_touch_inverse(0, 0, &dsx, &dsy, &in);
        fails += check("a miss reads outside and is clamped to a piece", !in && dsx >= 0 && dsy >= 0);
        hal_menu_touch_end();
        /* keyed columns: p1's first source column is keyed backdrop, so dest x0 shows the dimmed backdrop */
        const int *d = g_drawn.dest[1];
        fails += check("key_backdrop drops engine-B backdrop pixels", dst[(d[1] + d[3] / 2) * 1024 + d[0]] == dimmed);
        const int *e = g_drawn.dest[0];   /* TEST p0: s = (0,16,256,48) */
        const int bxp = e[0] + (60 * e[2]) / 256, byp = e[1] + ((40 - 16) * e[3]) / 48;
        const int gxp = e[0] + (140 * e[2]) / 256;
        fails += check("TOP_2D: BG3 cell drawn with its bm verdict applied", dst[byp * 1024 + bxp] == 0xffeeeeeeu);
        fails += check("TOP_2D: a BG0 cell is outside the piece's layer mask", dst[byp * 1024 + gxp] == dimmed);
    }
    {
        uint16_t k = MENU_PAD_DOWN;
        hal_menu_pad(&k, 0);
        fails += check("first direction press only shows the cursor, bit stripped", (k & MENU_PAD_DOWN) == 0 && g_cur.focus == 0);
        k = MENU_PAD_RIGHT;
        hal_menu_pad(&k, 0);
        fails += check("right moves focus", g_cur.focus != 0 && g_cur.focus > 0);
        k = MENU_PAD_CONFIRM;
        hal_menu_pad(&k, 0);
        int x, y, down;
        fails += check("confirm queues a press and strips the bit", (k & MENU_PAD_CONFIRM) == 0 && g_q_state == 1);
        fails += check("queue refuses a second press in flight", hal_menu_touch_queue_press(1, 1) == 0);
        int ok = hal_menu_touch_queue_poll(&x, &y, &down) && down;
        ok = ok && hal_menu_touch_queue_poll(&x, &y, &down) && down;
        ok = ok && hal_menu_touch_queue_poll(&x, &y, &down) && !down;
        ok = ok && !hal_menu_touch_queue_poll(&x, &y, &down);
        fails += check("queue: press, press, release, idle", ok);
        hal_menu_touch_real_held(1);
        fails += check("queue refuses while a real stylus is held", hal_menu_touch_queue_press(5, 5) == 0);
        hal_menu_touch_real_held(0);
        fails += check("queue accepts again once released", hal_menu_touch_queue_press(5, 5) == 1);
        hal_menu_touch_real_held(1);
        fails += check("a real stylus cancels a queued press", hal_menu_touch_queue_poll(&x, &y, &down) == 0);
        hal_menu_touch_real_held(0);
    }
    {   /* census and autocrop on the TOP_2D source */
        MenuPiece P = menu_piece_make(SRC_TOP_2D, 0, 20, 256, 40, 0, 0, 0, 0, 1, 1);
        P.layers = MENU_L_BG3;
        MenuLayout L;
        menu_layout_begin(&L, 512, 384);
        int c[4];
        menu_add_piece(&L, P);
        fails += check("census: a rect holding the whole block reads 0", menu_seam_census(&L, 0, &S, c) == 0);
        L.p[0].s.x = 0; L.p[0].s.y = 30; L.p[0].s.w = 256; L.p[0].s.h = 100;   /* y 30 cuts the block, 60 is its edge */
        fails += check("census: a cut through the block counts > 0", menu_seam_census(&L, 0, &S, c) > 0);
        DsRect r;
        L.p[0].s.y = 0; L.p[0].s.h = 192;
        fails += check("autocrop finds the opaque bbox (30,20,60,40)",
                       menu_autocrop_rect(&L.p[0], &S, &r) && r.x == 30 && r.y == 20 && r.w == 60 && r.h == 40);
        L.p[0].s = r;
        fails += check("census: the autocropped rect reads 0", menu_seam_census(&L, 0, &S, c) == 0);
    }
    {   /* ASSET1: a 3D-keyed picture piece, a column fill and an OAM-range plane */
        static int g_oam_lo = -1, g_oam_hi = -1;
        struct F {
            static const uint32_t *sub_for(uint8_t) { return 0; }
            static const uint32_t *sub_for_oam(uint8_t, int lo, int hi)
            {
                g_oam_lo = lo; g_oam_hi = hi;
                static uint32_t plane[256 * 192];
                for (int i = 0; i < 256 * 192; ++i) plane[i] = 0xff445566u;
                return plane;
            }
        };
        uint8_t *cover = (uint8_t *)calloc(1024 * 768, 1);
        uint32_t *pic2 = (uint32_t *)calloc(1024 * 768, sizeof(uint32_t));
        uint32_t *d2 = (uint32_t *)calloc(1024 * 768, sizeof(uint32_t));
        if (cover && pic2 && d2) {
            for (int i = 0; i < 1024 * 768; ++i) { pic2[i] = 0xff112233u; d2[i] = 0xff000000u; }
            for (int y = 100; y < 200; ++y) for (int x = 100; x < 200; ++x) cover[y * 1024 + x] = 1;
            MenuSources S2 = S;
            S2.pic = pic2; S2.pic_cover = cover; S2.pic_stride = 1024; S2.pic_w = 1024; S2.pic_h = 768;
            S2.pic_rx = 0; S2.pic_ry = 0; S2.pic_rw = 1024; S2.pic_rh = 768;
            MenuLayout L;
            menu_layout_begin(&L, 1024, 768);
            MenuPiece kp = menu_piece_make(SRC_TOP_PIC, 0, 0, 256, 192, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
            kp.keycover = 1;
            menu_add_piece(&L, kp);
            menu_layout_dest(&L);
            menu_piece_draw(&L, 0, &S2, d2, 1024, 768);
            fails += check("keycover: a covered pixel shows the picture", d2[150 * 1024 + 150] == 0xff112233u);
            fails += check("keycover: an uncovered pixel stays untouched (own transparency)", d2[50 * 1024 + 50] == 0xff000000u &&
                           d2[150 * 1024 + 250] == 0xff000000u && d2[250 * 1024 + 150] == 0xff000000u);
            S2.pic_cover = 0;
            for (int i = 0; i < 1024 * 768; ++i) d2[i] = 0xff000000u;
            menu_piece_draw(&L, 0, &S2, d2, 1024, 768);
            fails += check("keycover: no coverage plane draws nothing", d2[150 * 1024 + 150] == 0xff000000u);
            menu_layout_begin(&L, 1024, 768);
            MenuPiece xf = menu_piece_make(SRC_SUB, 8, 0, 1, 192, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
            xf.xfill = 1; xf.fill_w = 100; xf.bg = 1;
            menu_add_piece(&L, xf);
            menu_layout_dest(&L);
            S2.sub_for = 0;
            fails += check("xfill: dest is fill_w units wide", L.dest[0][2] == 400 && L.dest[0][3] == 768);
            for (int i = 0; i < 1024 * 768; ++i) d2[i] = 0xff000000u;
            menu_piece_draw(&L, 0, &S2, d2, 1024, 768);
            fails += check("xfill: every column of the fill reads the one source column", d2[300 * 1024] == kCol && d2[300 * 1024 + 399] == kCol &&
                           d2[300 * 1024 + 400] == 0xff000000u);
            menu_layout_begin(&L, 1024, 768);
            MenuPiece op = menu_piece_make(SRC_SUB, 0, 0, 256, 192, MENU_AX_L, MENU_AY_T, 0, 0, 1, 1);
            op.layers = MENU_L_OBJ; op.oam_on = 1; op.oam_lo = 3; op.oam_hi = 5;
            menu_add_piece(&L, op);
            menu_layout_dest(&L);
            S2.sub_for = F::sub_for; S2.sub_for_oam = F::sub_for_oam;
            g_nplanes = 0;
            menu_piece_draw(&L, 0, &S2, d2, 1024, 768);
            fails += check("oam range: the host is asked for entries 3..5 and the piece draws that plane",
                           g_oam_lo == 3 && g_oam_hi == 5 && d2[300 * 1024 + 300] == 0xff445566u);
        } else {
            fails += check("ASSET1 selftest buffers allocate", 0);
        }
        free(cover); free(pic2); free(d2);
    }
    {   /* the checker itself must be able to fail: break a good layout three ways */
        static MenuLayout B;
        menu_layout_solve(&B, &g_menu_recipes[g_menu_nrecipes - 1], 1, 1024, 768);
        Result R;
        B.p[2].dy = -80;                      /* p2 rises under p1 */
        menu_layout_dest(&B);
        check_layout(&B, &R);
        fails += check("checker: overlapping pieces are caught", !R.overlap);
        menu_layout_solve(&B, &g_menu_recipes[g_menu_nrecipes - 1], 1, 1024, 768);
        for (int d = 0; d < 4; ++d) B.hs[3].nav[d] = -1;
        check_layout(&B, &R);
        fails += check("checker: an unreachable hotspot is caught", !R.focus);
        menu_layout_solve(&B, &g_menu_recipes[g_menu_nrecipes - 1], 1, 1024, 768);
        B.p[3].dx = 80;                       /* p3 pushed off the right edge */
        menu_layout_dest(&B);
        check_layout(&B, &R);
        fails += check("checker: a piece outside the picture is caught", !R.bounds);
        menu_layout_solve(&B, &g_menu_recipes[g_menu_nrecipes - 1], 1, 1024, 768);
        B.hs[0].box.x = 250;
        check_layout(&B, &R);
        fails += check("checker: a hotspot box outside its source is caught", !R.hot);
    }
    {   /* fallback in a one-screen scene with no recipe up */
        hal_menu_frame_begin(1024, 768, 6);
        fails += check("fallback live in scene 6 (VS lobby, no recipe yet) with OneScreenMenus on", hal_menu_live() == 1 && hal_menu_fallback_frames() == 1);
        fails += check("fallback has the top backdrop and one touch panel", g_cur.np == 2 && g_cur.p[1].touch && g_cur.p[0].bg);
        hal_menu_frame_begin(1024, 768, 2);
        fails += check("a non-menu scene is not live", !hal_menu_live());
    }
    {   /* P0COREFIX rows */
        int x, y, down;
        /* back key: a host flag, not a pad bit; punch (0x0001) is the ROM's own A and is left alone */
        hal_menu_frame_begin(1024, 768, MENU_SCENE_TEST);
        hal_menu_present(dst, 1024, 768, MENU_STAGE_POST_FADE);
        while (hal_menu_touch_queue_poll(&x, &y, &down)) {}
        uint16_t k = 0x0001;
        hal_menu_pad(&k, 0);
        fails += check("back: pad bit 0x0001 alone presses nothing and is not stripped", k == 0x0001 && g_q_state == 0);
        k = 0;
        hal_menu_pad(&k, 1);
        const MenuHotspot &BH = g_cur.hs[g_cur.back];
        fails += check("back: host flag queues a press at the back hotspot centre",
                       g_cur.back >= 0 && g_q_state == 1 && g_q_x == BH.box.x + BH.box.w / 2 && g_q_y == BH.box.y + BH.box.h / 2);
        while (hal_menu_touch_queue_poll(&x, &y, &down)) {}
        /* the drawn layout is invalidated on a recipe change */
        hal_menu_frame_begin(1024, 768, 6);   /* fallback replaces test */
        int dsx = 5, dsy = 5, in = 1;
        hal_menu_touch_inverse(512, 600, &dsx, &dsy, &in);
        fails += check("stale: new recipe's first tick does not invert through old rects", !in && dsx == -1 && dsy == -1);
        hal_menu_present(dst, 1024, 768, MENU_STAGE_POST_FADE);
        hal_menu_touch_inverse(512, 600, &dsx, &dsy, &in);
        fails += check("stale: after its first draw the new layout inverts", in == 1);
        /* a queued press dies when the menu closes */
        fails += check("close: press queues", hal_menu_touch_queue_press(9, 9) == 1);
        hal_menu_frame_begin(1024, 768, 2);
        fails += check("close: the queued press is cancelled with the menu", !hal_menu_live() && hal_menu_touch_queue_poll(&x, &y, &down) == 0);
        hal_menu_touch_inverse(512, 600, &dsx, &dsy, &in);
        fails += check("close: no inverse once closed", !in);
        /* exact scale: 128 wide at 2/3 on 1024x768 is 85.33 units = 341.33 px; 127 at 1/2 is 63.5 units = 254 px */
        MenuLayout T;
        menu_layout_begin(&T, 1024, 768);
        menu_add_piece(&T, menu_piece_make(SRC_SUB, 0, 0, 128, 96, MENU_AX_L, MENU_AY_T, 0, 0, 2, 3));
        menu_add_piece(&T, menu_piece_make(SRC_SUB, 0, 0, 127, 95, MENU_AX_R, MENU_AY_B, 0, 0, 1, 2));
        menu_layout_dest(&T);
        fails += check("scale: 128x96 at 2/3 on 1024x768 is 341 or 342 by 256", (T.dest[0][2] == 341 || T.dest[0][2] == 342) && T.dest[0][3] == 256);
        fails += check("scale: 127x95 at 1/2 is 254 by 190 (47.5 units)", T.dest[1][2] == 254 && T.dest[1][3] == 190 && T.dest[1][0] + 254 == 1024 && T.dest[1][1] + 190 == 768);
        menu_layout_begin(&T, 1022, 767);
        menu_add_piece(&T, menu_piece_make(SRC_SUB, 0, 0, 256, 192, MENU_AX_C, MENU_AY_M, 0, 0, 1, 1));
        menu_layout_dest(&T);
        fails += check("odd size: a full 256x192 piece on 1022x767 stays inside", T.dest[0][0] >= 0 && T.dest[0][0] + T.dest[0][2] <= 1022 && T.dest[0][1] + T.dest[0][3] <= 767);
        /* autocrop latches survive a resize before the next draw */
        hal_menu_frame_begin(1024, 768, MENU_SCENE_TEST_CROP);
        hal_menu_present(dst, 1024, 768, MENU_STAGE_POST_FADE);
        fails += check("autocrop: latched at open to (30,20,60,40)", g_cur.p[0].s.x == 30 && g_cur.p[0].s.y == 20 && g_cur.p[0].s.w == 60 && g_cur.p[0].s.h == 40);
        hal_menu_present(dst, 800, 600, MENU_STAGE_POST_FADE);   /* the picture changed under the frame */
        fails += check("autocrop: the resize re-solve draws with the latched rect",
                       g_drawn.w == 800 && g_drawn.p[0].s.x == 30 && g_drawn.p[0].s.w == 60 && g_drawn.p[0].s.h == 40 &&
                       g_drawn.dest[0][2] >= 187 && g_drawn.dest[0][2] <= 188);
        hal_menu_frame_begin(1024, 768, 2);
    }
    hal_menu_set_enable_fn(0);
    hal_menu_set_sources(0);
    free(sub); free(top); free(pic); free(dst);
    return fails;
}

int selftest_all(void)
{
    const char *fn = getenv("SM64DS_MENU_SELFTEST_OUT");
    if (fn && *fn) g_out = fopen(fn, "w");
    int fails = run_grid();
    fails += run_live();
    char line[64];
    sprintf(line, "MENU LAYOUT SELFTEST: %s (%d failed)\n", fails ? "FAIL" : "PASS", fails);
    out(line);
    if (g_out) fclose(g_out);
    g_out = 0;
    return fails;
}

struct SelftestHook {
    SelftestHook()
    {
        const char *e = getenv("SM64DS_MENU_SELFTEST");
        if (e && *e && *e != '0') {
            const int f = selftest_all();
            fflush(stderr);
            exit(f ? 1 : 0);
        }
    }
} g_selftest_hook;

}  // namespace

int menu_selftest(void) { return selftest_all(); }
