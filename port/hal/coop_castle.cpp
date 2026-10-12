// COOP CASTLE: see coop_castle.h. Run mp1, lane COOPCASTLE1. PORT_HOST_ABI: reads live actors by raw offset, writes only
// the local player's position (the CHASE proof knob) and one save bit (the HUNT proof knob).

#include "coop_castle.h"
#include "coop_world.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250;
extern int data_0209caa0[];
void *_ZN8dActor_c4NextEPKS_(const void *prev);
}

namespace port {
namespace {

enum : unsigned { kClsBoo = 209, kClsMip = 187, kClsMipKey = 229 };

bool g_env = false, g_log = false, g_hunt = false;
unsigned g_shove_f = 0, g_shove_cls = 0; int g_shove_dx = 0; bool g_shove_done = true;
unsigned g_chase_f = 0, g_chase_n = 0, g_chase_cls = kClsMip, g_chase_d = 150;
bool g_established = false;
unsigned g_frames = 0;
int g_prev_keys = 0;
CoopCastleStats g_st = {};

long long wall_ms() {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    return (long long)((((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000ull);
}

void read_env() {
    if (g_env) return;
    g_env = true;
    const char *l = std::getenv("SM64DS_COOP_CASTLE_LOG");
    g_log = l && *l && *l != '0';
    const char *h = std::getenv("SM64DS_CASTLE_HUNT");
    g_hunt = h && *h && *h != '0';
    if (const char *sh = std::getenv("SM64DS_CASTLE_SHOVE")) {
        unsigned f = 0, cl = 0; int dx = 0;
        if (std::sscanf(sh, "%u:%u:%d", &f, &cl, &dx) == 3 && (cl == kClsMip || cl == kClsBoo) && dx >= -3000 && dx <= 3000) {
            g_shove_f = f; g_shove_cls = cl; g_shove_dx = dx; g_shove_done = false;
        }
    }
    if (const char *c = std::getenv("SM64DS_CASTLE_CHASE")) {
        unsigned f = 0, n = 0, cl = kClsMip, d = 150;
        if (std::sscanf(c, "%u:%u:%u:%u", &f, &n, &cl, &d) >= 2) {
            g_chase_f = f; g_chase_n = n > 6000 ? 6000 : n;
            if (cl == kClsMip || cl == kClsBoo) g_chase_cls = cl;
            if (d >= 50 && d <= 2000) g_chase_d = d;
        }
    }
}

int rd32(const char *a, int off) { int v; std::memcpy(&v, a + off, 4); return v; }

bool is_player(const void *a) {
    for (int i = 0; i < 16; ++i) if (data_0209f394[i] == a) return true;
    return false;
}
bool live(const char *a) { return !*(const unsigned char *)(a + 0x0f) && *(const unsigned char *)(a + 0x0e) != 2; }

char *nearest_cls(const char *pl, unsigned cls) {
    char *best = nullptr; double bd = 1e30;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(nullptr); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        if (*(const unsigned short *)(a + 0x0c) != cls || is_player(a) || !live(a)) continue;
        const double dx = (rd32(a, 0x5c) - rd32(pl, 0x5c)) / 4096.0, dz = (rd32(a, 0x64) - rd32(pl, 0x64)) / 4096.0;
        const double d = dx * dx + dz * dz;
        if (d < bd) { bd = d; best = a; }
    }
    return best;
}

void shove_all(unsigned cls, int dx) {
    int n = 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(nullptr); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        if (*(const unsigned short *)(a + 0x0c) != cls || is_player(a) || !live(a)) continue;
        const int nx = rd32(a, 0x5c) + (dx << 12);
        std::memcpy(a + 0x5c, &nx, 4);
        ++n;
    }
    std::fprintf(stderr, "[coopcastle-knob] shove class=%u dx=%d moved=%d wall=%lld
", cls, dx, n, wall_ms());
}

void on_latched(int, int) { g_established = false; g_frames = 0; g_shove_done = g_shove_cls == 0; g_prev_keys = 0; }
void on_established(int, int) { g_established = true; g_frames = 0; }

void on_frame(unsigned) {
    read_env();
    if (!g_established && !coop_group_established()) return;
    g_established = true;
    ++g_frames; ++g_st.frames;
    char *pl = (char *)data_0209f394[data_0209f250 & 15];
    if (g_hunt && pl && (data_0209caa0[2] & 0x20000) == 0) { data_0209caa0[2] |= 0x20000; ++g_st.hunt_sets; }
    if (g_chase_n && pl && g_frames >= g_chase_f && g_frames < g_chase_f + g_chase_n && (g_frames % 20u) == 0) {
        if (char *m = nearest_cls(pl, g_chase_cls)) {
            // stand 150 units to the +x side of the rabbit, same height: it has to run from the player
            const int off = (int)g_chase_d << 12;
            const int dx = rd32(pl, 0x5c) - rd32(m, 0x5c);
            const int sx = dx >= 0 ? 1 : -1;
            const int nx = rd32(m, 0x5c) + sx * off;
            std::memcpy(pl + 0x5c, &nx, 4);
            const int ny = rd32(m, 0x60), nz = rd32(m, 0x64);
            std::memcpy(pl + 0x60, &ny, 4); std::memcpy(pl + 0x64, &nz, 4);
            if (g_chase_cls == kClsBoo) {     // a shy Boo creeps up on a player who looks away: face away from it
                const double ddx = (rd32(m, 0x5c) - nx) / 4096.0, ddz = (rd32(m, 0x64) - nz) / 4096.0;
                const int away = ((int)std::lround(std::atan2(ddx, ddz) * 32768.0 / 3.14159265358979) + 0x8000) & 0xffff;
                const short ya = (short)away;
                std::memcpy(pl + 0x8e, &ya, 2); std::memcpy(pl + 0x94, &ya, 2);
            }
            ++g_st.chase_moves;
        }
    }
    if (!g_shove_done && g_frames >= g_shove_f) { g_shove_done = true; shove_all(g_shove_cls, g_shove_dx); }
    if ((g_frames % 60u) != 0) return;
    int boo = 0, mip = 0, keys = 0;
    char *b0 = nullptr, *m0 = nullptr;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(nullptr); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        if (is_player(a) || !live(a)) continue;
        const unsigned c = *(const unsigned short *)(a + 0x0c);
        if (c == kClsBoo) { ++boo; if (!b0) b0 = a; }
        else if (c == kClsMip) { ++mip; if (!m0) m0 = a; }
        else if (c == kClsMipKey) ++keys;
    }
    if ((unsigned long long)boo > g_st.max_boo) g_st.max_boo = (unsigned long long)boo;
    if ((unsigned long long)mip > g_st.max_mip) g_st.max_mip = (unsigned long long)mip;
    if (keys > g_prev_keys) { g_st.key_spawns += (unsigned long long)(keys - g_prev_keys); if (g_log) std::fprintf(stderr, "[coopcastle] key spawned wall=%lld keys=%d\n", wall_ms(), keys); }
    g_prev_keys = keys;
    if (!g_log) return;
    char bs[64] = "-", ms[160] = "-";
    if (b0) std::snprintf(bs, sizeof bs, "%d,%d", rd32(b0, 0x5c) >> 12, rd32(b0, 0x64) >> 12);
    if (m0) std::snprintf(ms, sizeof ms, "%d,%d st=%08x hold=%08x caught=%d", rd32(m0, 0x5c) >> 12, rd32(m0, 0x64) >> 12,
                          (unsigned)rd32(m0, 0x364), (unsigned)rd32(m0, 0x45c), (int)*(const unsigned char *)(m0 + 0x426));
    std::fprintf(stderr, "[coopcastle] wall=%lld f=%u boo=%d mip=%d key=%d b0=%s m0=%s\n", wall_ms(), g_frames, boo, mip, keys, bs, ms);
}

struct Reg {
    Reg() {
        coop_on_frame(on_frame);
        coop_on_group_latched(on_latched);
        coop_on_group_established(on_established);
    }
} g_reg;

}  // namespace

CoopCastleStats coop_castle_stats() { return g_st; }

}  // namespace port
