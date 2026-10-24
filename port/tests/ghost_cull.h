/* GHOSTPERF1: A HUB GHOST THAT CANNOT BE SEEN IS NOT SUBMITTED.
 *
 * Included from walk_window.cpp right after nametag.h, whose per-tick stash of
 * the camera (nt_stash_view) it reads. With 31 other players in the world,
 * each remote body costs a full Player::Render-sized display list (decoded
 * and transformed on the CPU), then its triangles are carried through the
 * raster, the ghost layer and every in-between picture of the smooth motion.
 * Measured on castle grounds (out/GHOSTPERF1): about 0.17 ms per body per
 * tick in the submit stage alone, whether or not the body is on screen. This
 * asks one cheap question first: is a sphere around the body inside the view
 * volume, widened on every side? If not, the draw (and everything downstream
 * of it) is skipped for the tick.
 *
 * The test is in clip space with the same matrices the name tags project
 * with, so it follows the widescreen widen. The four side planes and the
 * eye plane are tested; the far plane is not (the game's own draw distance
 * handles that). A body is kept when it is within kGhostCullSphere world units
 * of the volume, and the volume itself is opened by kGhostCullSlack of its
 * half width, so a camera turning between two ticks cannot reach a body this
 * tick skipped (the in-between pictures blend tick N-1 into tick N). SM64DS_GHOST_CULL=0
 * turns it off (the control arm of the measurement).
 */
#ifndef PORT_GHOST_CULL_H
#define PORT_GHOST_CULL_H

enum { kGhostCullSphere = 520 };           /* world units: tongue, wings, cap, a hop */
static const double kGhostCullSlack = 0.30;

static int ghost_cull_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("SM64DS_GHOST_CULL");
        on = (e && e[0] == '0' && !e[1]) ? 0 : 1;
    }
    return on;
}

static unsigned long long g_gc_seen, g_gc_culled;

/* 1 when the body at world position bp (Fix12) is certainly out of view this tick */
static int ghost_cull_outside(const int bp[3])
{
    if (!g_nt_have_view) return 0;
    static int lg = -1;
    if (lg < 0) { const char *e = getenv("SM64DS_GHOST_CULL_LOG"); lg = e && *e && *e != '0'; }
    if (lg && (g_gc_seen % 3000) == 1)
        fprintf(stderr, "[gcull] seen %llu culled %llu\n", g_gc_seen, g_gc_culled);
    const float *P = g_nt_proj;
    const bool persp = P[3] != 0.0f || P[7] != 0.0f || P[11] != 0.0f;
    if (!persp) return 0;
    if (g_nt_vp[0] != 0 || g_nt_vp[1] != 0 || g_nt_vp[2] * 3 != g_nt_vp[3] * 4)
        return 0;                          /* a split or inset view: keep it simple, draw */
    const int *m = g_nt_view;
    /* the body's middle, a little above its feet, into view space (scene units) */
    const double px = (double)((bp[0] + 4) >> 3) / 4096.0;
    const double py = (double)((bp[1] + 80 * 4096 + 4) >> 3) / 4096.0;
    const double pz = (double)((bp[2] + 4) >> 3) / 4096.0;
    const double vx = (px * m[0] + py * m[3] + pz * m[6]) / 4096.0 + m[9] / 4096.0;
    const double vy = (px * m[1] + py * m[4] + pz * m[7]) / 4096.0 + m[10] / 4096.0;
    const double vz = (px * m[2] + py * m[5] + pz * m[8]) / 4096.0 + m[11] / 4096.0;
    const double R = (double)kGhostCullSphere / 8.0;     /* world -> scene */
    const double wd = (double)g_nt_widen;
    /* clip x, y, w as linear forms (a.v + d) */
    const double xa = P[0] * wd, xb = P[4] * wd, xc = P[8] * wd, xd = P[12] * wd;
    const double ya = P[1], yb = P[5], yc = P[9], yd = P[13];
    const double wa = P[3], wb = P[7], wc = P[11], wd_ = P[15];
    const double cw = vx * wa + vy * wb + vz * wc + wd_;
    const double nw = std::sqrt(wa * wa + wb * wb + wc * wc);
    if (nw <= 0.0) return 0;
    if (cw < -R * nw) return 1;                          /* behind the eye plane */
    const double k = 1.0 + kGhostCullSlack;
    for (int s = 0; s < 2; ++s) {
        const double sg = s ? 1.0 : -1.0;
        /* plane k*w + sg*x >= 0 and k*w + sg*y >= 0 */
        const double ax = k * wa + sg * xa, bx = k * wb + sg * xb, cx2 = k * wc + sg * xc, dx = k * wd_ + sg * xd;
        const double ay = k * wa + sg * ya, by = k * wb + sg * yb, cy2 = k * wc + sg * yc, dy = k * wd_ + sg * yd;
        const double nx = std::sqrt(ax * ax + bx * bx + cx2 * cx2);
        const double ny = std::sqrt(ay * ay + by * by + cy2 * cy2);
        if (nx > 0.0 && (vx * ax + vy * bx + vz * cx2 + dx) < -R * nx) return 1;
        if (ny > 0.0 && (vx * ay + vy * by + vz * cy2 + dy) < -R * ny) return 1;
    }
    return 0;
}

#endif
