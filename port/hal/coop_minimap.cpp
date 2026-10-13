// COOP MINIMAP: see coop_minimap.h. Run mp1, lane SMALLS9.
#include "coop_minimap.h"
#include "../unmatched/Minimap_wide.h"
#include <cstdio>
#include <cstdlib>

extern "C" int port_adventure_ghost_seat(int slot);
extern "C" int port_adventure_ghost_drawpos(int slot, int out[3]);
extern "C" unsigned char data_0209f2d8;

namespace {

int mode_env() {
    static int v = -1;
    if (v < 0) {
        const char *e = std::getenv("SM64DS_COOP_MINIMAP");
        v = (e && e[0] == '0' && e[1] == 0) ? 0 : 1;
    }
    return v;
}

int log_env() {
    static int v = -1;
    if (v < 0) {
        const char *e = std::getenv("SM64DS_COOP_MINIMAP_LOG");
        v = (e && *e && !(e[0] == '0' && e[1] == 0)) ? 1 : 0;
    }
    return v;
}

}  // namespace

extern "C" int port_coop_minimap_ghost(int slot, int pos[3]) {
    if (!mode_env() || data_0209f2d8 == 1) return 0;     // VS draws its own
    if (port_adventure_ghost_seat(slot) < 0) return 0;   // not a hub ghost
    if (!port_adventure_ghost_drawpos(slot, pos)) return 2;
    if (log_env()) {
        static unsigned n[kMinimapWidePlayers];
        if (slot >= 0 && slot < kMinimapWidePlayers && (n[slot]++ % 120u) == 0)
            std::fprintf(stderr, "[comap] slot %d seat %d at (%d,%d,%d)\n", slot,
                         port_adventure_ghost_seat(slot), pos[0] >> 12, pos[1] >> 12,
                         pos[2] >> 12);
    }
    return 1;
}

namespace {
struct Install { Install() { g_mm_ghost_pos = port_coop_minimap_ghost; } } g_install;
}  // namespace
