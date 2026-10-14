// COOP MINIMAP: see coop_minimap.h. Run mp1, lane SMALLS9.
#include "coop_minimap.h"
#include "../unmatched/Minimap_wide.h"
#include <cstdio>
#include <cstdlib>

extern "C" int port_adventure_ghost_seat(int slot);
extern "C" int port_adventure_ghost_drawpos(int slot, int out[3]);
extern "C" unsigned char data_0209f2d8;
extern "C" int port_rom_frame(void);

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
    {   // proof knob SM64DS_COOP_MINIMAP_BLINK=<from>:<to>: the hook answers "not a ghost" for ROM frames
        // from..to-1, so one run holds both pictures (the icon on, then off) for the same scene
        static int b0 = -2, b1 = -2;
        if (b0 == -2) {
            b0 = b1 = -1;
            const char *e = std::getenv("SM64DS_COOP_MINIMAP_BLINK");
            int x = 0, y = 0;
            if (e && std::sscanf(e, "%d:%d", &x, &y) == 2) { b0 = x; b1 = y; }
        }
        const int f = port_rom_frame();
        if (b0 >= 0 && f >= b0 && f < b1) return 0;
    }
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

extern "C" void port_coop_minimap_trace(int slot, int gk, int x, int y, int mapid, int cur) {
    if (!log_env()) return;
    static unsigned n;
    if ((n++ % 240u) == 0)
        std::fprintf(stderr, "[comap] render slot %d answer %d icon (%d,%d) map %d current %d\n", slot, gk, x, y, mapid, cur);
}

namespace {
struct Install { Install() { g_mm_ghost_pos = port_coop_minimap_ghost; } } g_install;
}  // namespace
