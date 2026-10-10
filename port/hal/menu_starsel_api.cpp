/* menu_starsel_api.cpp -- see menu_starsel_api.h (run menu1, lane STARHOOK1). */
#include "menu_starsel_api.h"

#include "menu_compose.h"
#include "ntr/ppu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
void *port_scene_live_object(void);
int   port_scene_live_id(void);
int   hal_present_client_to_fb(int cx, int cy, int *fx, int *fy);
}
extern "C++" {
void menu_starsel_facts(MenuFacts *f);          /* menu_recipe_starsel.cpp: reads the live object (logic thread) */
int  menu_starsel_big_rect(const MenuFacts *F, int w, int h, int star, int out[4]);
}

namespace {

const int kStarScene = 4;

/* The facts and the picture size of this tick. Where the one-screen recipe owns the screen the answer is the decision the player
   saw (the contract: no ROM read for it); otherwise (OneScreenMenus off, or a frame the decision is not live) the object's few
   fields are read here, on the logic thread, by the same function the recipe's decide step uses. */
struct Snap { MenuFacts F; int w, h; };

bool snap(Snap *s)
{
    if (port_scene_live_id() != kStarScene) return false;
    MenuDecision d;
    if (hal_menu_decision_copy(&d) && d.live && d.L.recipe && d.L.recipe->name && !strcmp(d.L.recipe->name, "starsel")) {
        s->F = d.L.facts; s->w = d.w; s->h = d.h;
        return true;
    }
    memset(&s->F, 0, sizeof s->F);
    menu_starsel_facts(&s->F);
    s->w = ntr::active_w; s->h = ntr::active_h;
    return true;
}

/* picture (framebuffer) pixel -> client pixel. hal_present_client_to_fb is the one mapper the port's clicks use
   (hal/sub_screen.cpp: the present rectangle, the letterbox and the stacked layout's top band), so the forward mapping is
   recovered from it rather than restated. Its answer is monotone in the client coordinate (affine, floored, clamped to the
   picture): the client column where it first reaches picture column 1 and column W-1 gives the picture's left edge and its
   scale; likewise for rows. Exact to one client pixel; the picture's own edges (0 and W) are the line extended. */
struct Axis { long long a; long long b; int n; bool ok; };     /* client(t) = a + (t - 1) * b / (n - 2), b = client(n-1) - client(1) */

int first_reaching(int target, int horiz)
{
    int lo = 0, hi = 1 << 15;
    int fx = 0, fy = 0;
    hal_present_client_to_fb(horiz ? hi : 0, horiz ? 0 : hi, &fx, &fy);
    if ((horiz ? fx : fy) < target) return -1;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        hal_present_client_to_fb(horiz ? mid : 0, horiz ? 0 : mid, &fx, &fy);
        if ((horiz ? fx : fy) >= target) hi = mid; else lo = mid + 1;
    }
    return lo;
}

Axis axis(int horiz, int n)
{
    Axis A = { 0, 0, n, false };
    if (n < 4) return A;
    const int c1 = first_reaching(1, horiz), c2 = first_reaching(n - 1, horiz);
    if (c1 < 0 || c2 < 0 || c2 <= c1) return A;
    A.a = c1; A.b = c2 - c1; A.ok = true;
    return A;
}

/* t in picture pixels (0..n), result client pixels, rounded to nearest */
int to_client(const Axis &A, int t)
{
    const long long num = (long long)(t - 1) * A.b;
    const long long den = A.n - 2;
    const long long q = num >= 0 ? (2 * num + den) / (2 * den) : -((2 * -num + den) / (2 * den));
    return (int)(A.a + q);
}

}  // namespace

extern "C" {

int hal_menu_starsel_up(void) { return port_scene_live_id() == kStarScene ? 1 : 0; }

int hal_menu_starsel_count(void)
{
    Snap s;
    if (!snap(&s) || !s.F.starsel_valid) return 0;
    return s.F.starsel_n > 8 ? 8 : s.F.starsel_n;
}

int hal_menu_starsel_selected(void)
{
    Snap s;
    if (!snap(&s) || !s.F.starsel_valid) return -1;
    const int cur = s.F.spare[0] - 1;
    return (cur >= 0 && cur < s.F.starsel_n && cur < 8) ? cur : -1;
}

int hal_menu_starsel_star_rect_pic(int star, int *x, int *y, int *w, int *h)
{
    Snap s;
    int r[4];
    if (!snap(&s) || !menu_starsel_big_rect(&s.F, s.w, s.h, star, r)) return 0;
    if (x) *x = r[0];
    if (y) *y = r[1];
    if (w) *w = r[2];
    if (h) *h = r[3];
    return 1;
}

int hal_menu_starsel_star_rect(int star, int *x, int *y, int *w, int *h)
{
    Snap s;
    int r[4];
    if (!snap(&s) || !menu_starsel_big_rect(&s.F, s.w, s.h, star, r)) return 0;
    const Axis X = axis(1, ntr::active_w), Y = axis(0, ntr::active_h);
    if (!X.ok || !Y.ok) return 0;
    const int cx0 = to_client(X, r[0]), cx1 = to_client(X, r[0] + r[2]);
    const int cy0 = to_client(Y, r[1]), cy1 = to_client(Y, r[1] + r[3]);
    if (x) *x = cx0;
    if (y) *y = cy0;
    if (w) *w = cx1 - cx0;
    if (h) *h = cy1 - cy0;
    return 1;
}

#ifndef PORT_ROM_CLEAN
void hal_menu_starsel_debug_draw(unsigned *px, int stride)
{
    static int on = -1, last_sig;
    if (on < 0) { const char *e = getenv("SM64DS_STARSEL_RECT_DEBUG"); on = (e && *e && *e != '0') ? 1 : 0; }
    if (!on || !px || !hal_menu_starsel_up()) return;
    const int n = hal_menu_starsel_count(), cur = hal_menu_starsel_selected();
    int sig = n * 100 + cur + 1;
    for (int i = 0; i < n; ++i) {
        int x, y, w, h;
        if (!hal_menu_starsel_star_rect_pic(i, &x, &y, &w, &h)) continue;
        sig = sig * 31 + x;
        const unsigned col = (i == cur) ? 0xFFFF0000u : 0xFF00C000u;
        for (int t = 0; t < 2; ++t) {
            for (int k = 0; k <= w; ++k) {
                const int xx = x + k;
                if (xx < 0 || xx >= ntr::active_w) continue;
                if (y + t >= 0 && y + t < ntr::active_h) px[(size_t)(y + t) * stride + xx] = col;
                if (y + h - t >= 0 && y + h - t < ntr::active_h) px[(size_t)(y + h - t) * stride + xx] = col;
            }
            for (int k = 0; k <= h; ++k) {
                const int yy = y + k;
                if (yy < 0 || yy >= ntr::active_h) continue;
                if (x + t >= 0 && x + t < ntr::active_w) px[(size_t)yy * stride + x + t] = col;
                if (x + w - t >= 0 && x + w - t < ntr::active_w) px[(size_t)yy * stride + x + w - t] = col;
            }
        }
    }
    if (sig != last_sig) {
        last_sig = sig;
        for (int i = 0; i < n; ++i) {
            int x, y, w, h, cx = -1, cy = -1, cw = -1, ch = -1, fx = -1, fy = -1;
            hal_menu_starsel_star_rect_pic(i, &x, &y, &w, &h);
            if (hal_menu_starsel_star_rect(i, &cx, &cy, &cw, &ch)) hal_present_client_to_fb(cx + cw / 2, cy + ch / 2, &fx, &fy);
            fprintf(stderr, "[starsel-rect] star %d/%d sel %d pic(%d,%d,%d,%d) client(%d,%d,%d,%d) client-centre->pic(%d,%d) pic-centre(%d,%d)\n",
                    i, n, cur, x, y, w, h, cx, cy, cw, ch, fx, fy, x + w / 2, y + h / 2);
        }
    }
}
#endif

}  // extern "C"
