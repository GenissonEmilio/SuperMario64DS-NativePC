/* hal/ov_reset.cpp -- every load of an overlay starts that overlay's own
   objects from their start-up image, as the DS does (run hunt8, lane OVLRESET1).

   WHAT THE DS DOES. FS_LoadOverlay copies the overlay's .data from the cart and
   zero-fills its .bss (func_0205e0b0 -> func_0205a588(base + size, 0, bssSize));
   the overlay's static initialisers then run. So an object a class's
   translation unit owns starts every visit as the build made it. This port
   compiles those TUs into the exe once, so the objects live for the process and
   a second visit to the same overlay used to start from the first visit's end
   state (a counter still counting, a cache still warm, a flag still set).

   WHAT THIS DOES. port/tools/ovreset.py routes every file-scope and
   function-local object of an overlay's TUs (the ones the overlays' delinks.txt files
   gives .data or .bss) into one linker group per overlay. On the FIRST load of
   an overlay in the process this copies that group; on every later load it
   copies the group back, before anything of the overlay runs. The first load is
   taken as it stands: by then every one-time bring-up the port does for the
   overlay (the held-back initialisers of port_ov_sinit_late, the registry and
   state-table seats of port_stage_a2_seat) has run, so the image already holds
   what the initialisers built, and copying it back equals the DS clearing .bss
   and running the initialisers again, without a second run of initialisers that
   register themselves in global lists.

   WHERE THE DS LOADS. port_ov_load(n) is called at the places the DS loads
   overlay n: LoadOverlay (hal/scene_boot.cpp: the scene overlays through
   func_0201a798), the level entry (port_ov_level_load: LoadLevelOverlays' own
   sequence, the object overlays through the ROM's LoadOrUnloadObjectOverlays),
   and the minigame scene spawn (port_scene_mg_prepare). The ROM decides when a
   scene overlay is loaded again (func_0201a694 skips a scene whose overlay is
   already resident), so a retry that stays in the overlay is not reset.

   SM64DS_OVL_RESET=0 turns this off (the once-per-process behaviour);
   =2 only reports what a restore would have changed; SM64DS_OVL_RESET_LOG=1
   lists the changed address runs of every restore. */
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "hal/ov_reset.h"

extern "C" {
extern int overlay_2, overlay_3, overlay_4, overlay_5, overlay_6, overlay_7, overlay_75;   /* hal/boot_globals.cpp */
extern int overlay_60, overlay_64, overlay_66, overlay_98, overlay_100, overlay_102;      /* hal/scene_boot.cpp */
extern int data_020758c8[];                                                                /* level -> level overlay */
}

/* The ROM's own object-overlay walk (C++ linkage, the matched TU). */
void LoadOrUnloadObjectOverlays(void (*fn)(int), int idx);

namespace {

struct Range {
    const unsigned char *b;
    size_t n;
    unsigned char *snap;
};

struct Ov {
    bool seen;
    unsigned loads;
    Range r[2];
};

Ov g_ov[128];

int mode(void)
{
    static int m = -1;
    if (m < 0) {
        const char *e = std::getenv("SM64DS_OVL_RESET");
        m = !e || !*e ? 1 : std::atoi(e);
    }
    return m;
}

int logging(void)
{
    static int on = -1;
    if (on < 0)
        on = std::getenv("SM64DS_OVL_RESET_LOG") != 0;
    return on;
}

const PortOvResetEnt *find(int ov)
{
    for (const PortOvResetEnt *e = port_ovreset_tab; e->ov >= 0; ++e)
        if (e->ov == ov)
            return e;
    return 0;
}

}  /* namespace */

/* An overlay id as the DS passes it: the number, or (the ADDRESS is the id on
   the DS) the host address of the placeholder int hal/scene_boot.cpp and
   hal/boot_globals.cpp hold for it. -1 when it is neither. */
extern "C" int port_ov_id_to_num(int id)
{
    if (id >= 0 && id < 128)
        return id;
    const int *k[] = {&overlay_2, &overlay_3, &overlay_4, &overlay_5, &overlay_6, &overlay_7,
                      &overlay_75, &overlay_60, &overlay_64, &overlay_66, &overlay_98,
                      &overlay_100, &overlay_102};
    const int n[] = {2, 3, 4, 5, 6, 7, 75, 60, 64, 66, 98, 100, 102};
    for (unsigned i = 0; i < sizeof n / sizeof n[0]; ++i)
        if (id == (int)(size_t)k[i])
            return n[i];
    return -1;
}

/* Overlay ov is being loaded now. Returns 1 when its objects were put back to
   the start-up image (so a caller that keeps one-time bring-up of its own for
   the overlay knows to run it again), else 0. */
extern "C" int port_ov_load(int ov)
{
    if (mode() == 0 || ov < 0 || ov >= 128)
        return 0;
    const PortOvResetEnt *e = find(ov);
    if (!e)
        return 0;
    Ov &o = g_ov[ov];
    ++o.loads;
    const unsigned char *rb[2] = {(const unsigned char *)e->db + 1, (const unsigned char *)e->sb + 1};
    const unsigned char *re[2] = {(const unsigned char *)e->de, (const unsigned char *)e->se};
    if (!o.seen) {
        o.seen = true;
        size_t tot = 0;
        for (int i = 0; i < 2; ++i) {
            o.r[i].b = rb[i];
            o.r[i].n = re[i] > rb[i] ? (size_t)(re[i] - rb[i]) : 0;
            tot += o.r[i].n;
            if (o.r[i].n) {
                o.r[i].snap = (unsigned char *)std::malloc(o.r[i].n);
                if (o.r[i].snap)
                    std::memcpy(o.r[i].snap, o.r[i].b, o.r[i].n);
            }
        }
        if (logging())
            std::fprintf(stderr, "[ovreset] ov%03d load #1: start-up image taken, %zu bytes\n", ov, tot);
        return 0;
    }
    size_t diff = 0, tot = 0;
    for (int i = 0; i < 2; ++i) {
        Range &r = o.r[i];
        if (!r.n || !r.snap)
            continue;
        tot += r.n;
        const unsigned char *cur = r.b;
        size_t runs = 0;
        for (size_t k = 0; k < r.n; ++k) {
            if (cur[k] == r.snap[k])
                continue;
            size_t s = k;
            while (k + 1 < r.n && cur[k + 1] != r.snap[k + 1])
                ++k;
            diff += k - s + 1;
            if (logging() && runs < 60) {
                std::fprintf(stderr, "[ovreset]   ov%03d %s +%zx..+%zx (%p) changed\n", ov,
                             i ? "dsstate" : "data", s, k, (const void *)(cur + s));
                ++runs;
            }
        }
        if (mode() == 1)
            std::memcpy((void *)r.b, r.snap, r.n);
    }
    std::fprintf(stderr, "[ovreset] ov%03d load #%u: %s %zu bytes, %zu differed from the start-up image\n",
                 ov, o.loads, mode() == 1 ? "restored" : "would restore", tot, diff);
    return mode() == 1;
}

/* LoadOrUnloadObjectOverlays' callback. */
extern "C" void port_ov_load_cb(int id)
{
    int n = port_ov_id_to_num(id);
    if (n >= 0)
        port_ov_load(n);
}

/* Stage::InitResources' LoadLevelOverlays(level), src/_Z17LoadLevelOverlaysi.cpp:
   the level's own overlay, then the object overlays the ROM's walk names. */
extern "C" void port_ov_level_load(int level)
{
    if (mode() == 0 || level < 0 || level >= 52)
        return;
    int ov = data_020758c8[level];
    if (ov != -1)
        port_ov_load(ov);
    LoadOrUnloadObjectOverlays(port_ov_load_cb, level);
}
