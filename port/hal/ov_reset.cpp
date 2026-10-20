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
   The minigame pair ov004 / ov006 is the exception that proves it: the port brings those up
   lazily after their first load (constructors, then the state-table seats), so
   their image is taken again once that is done (port_ov_retake, from LoadOverlay)
   and nothing re-runs on a later visit (run hunt8, lane LOVESME2).

   WHERE THE DS LOADS. port_ov_load(n) is called at the places the DS loads
   overlay n: LoadOverlay (hal/scene_boot.cpp: the scene overlays through
   func_0201a798), the level entry (port_ov_level_load: LoadLevelOverlays' own
   sequence, the object overlays through the ROM's LoadOrUnloadObjectOverlays),
   and the minigame scene spawn (port_scene_mg_prepare). The ROM decides when a
   scene overlay is loaded again (func_0201a694 skips a scene whose overlay is
   already resident), so a retry that stays in the overlay is not reset.

   WHY ONLY THE TU OBJECTS AND THE GUARD WORDS, NOT THE WHOLE MOUNT. The overlay's
   per-symbol and whole-image mounts (tools/ovdata.py, build/port/host-src/ovNNN_*.c)
   are the rest of its .data/.bss. Restoring them as well was tried and rejected:
   the level overlays bring their mount up lazily (rebasing patch passes, sinits,
   seats run after the first load), so a first-load copy of a mount holds the
   un-rebased ROM words, and putting those back after the first visit crashed the
   ending row (ov009). One mounted word is a different case: the guard of a
   function-local static of a TU (data_ovNNN_XXXXXXXX |= 1). The object is
   restored with the TU, so its guard has to go back with it or the one-time
   construction is skipped and the object stays at its start-up zero (daDoor_c's
   75-unit offset, ov100). Guards are plain bss flags no bring-up writes, so the
   generator lists them (ovreset.py guards()) and they are restored from their
   first-load copy like the rest.

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
    Range *m;           /* the mounted data arrays (tools/ovdata.py rows), one Range each */
    unsigned nm;
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
    const unsigned char *rb[2] = {e->db ? (const unsigned char *)e->db + 1 : 0,
                                  e->sb ? (const unsigned char *)e->sb + 1 : 0};
    const unsigned char *re[2] = {(const unsigned char *)e->de, (const unsigned char *)e->se};
    if (!o.seen) {
        o.seen = true;
        size_t tot = 0;
        for (int i = 0; i < 2; ++i) {
            o.r[i].b = rb[i];
            o.r[i].n = rb[i] && re[i] > rb[i] ? (size_t)(re[i] - rb[i]) : 0;
            tot += o.r[i].n;
            if (o.r[i].n) {
                o.r[i].snap = (unsigned char *)std::malloc(o.r[i].n);
                if (o.r[i].snap)
                    std::memcpy(o.r[i].snap, o.r[i].b, o.r[i].n);
            }
        }
        /* The mounted guard words (see the header comment): the arrays of the overlay's mount that a
           function-local static of one of the TUs above is guarded by. */
        const PortOvMRow *tabs[2] = {e->ms, e->mw};
        const unsigned cnt[2] = {e->msn ? *e->msn : 0u, e->mwn ? *e->mwn : 0u};
        if (e->nguards)
            o.m = (Range *)std::calloc(e->nguards, sizeof(Range));
        for (unsigned g = 0; g < e->nguards && o.m; ++g) {
            bool found = false;
            for (int t = 0; t < 2 && !found; ++t)
                for (unsigned i = 0; i < cnt[t]; ++i)
                    if (tabs[t][i].p == e->guards[g]) {
                        Range &m = o.m[o.nm];
                        m.b = tabs[t][i].p;
                        m.n = tabs[t][i].n;
                        m.snap = (unsigned char *)std::malloc(m.n);
                        if (m.snap) {
                            std::memcpy(m.snap, m.b, m.n);
                            ++o.nm;
                            tot += m.n;
                        }
                        found = true;
                        break;
                    }
            if (!found && logging())
                std::fprintf(stderr, "[ovreset] ov%03d guard %p is not the start of a mounted array, left alone\n",
                             ov, (const void *)e->guards[g]);
        }
        if (logging())
            std::fprintf(stderr, "[ovreset] ov%03d load #1: start-up image taken, %zu bytes (%u mounted guard words)\n", ov, tot, o.nm);
        return 0;
    }
    size_t diff = 0, tot = 0, after = 0;
    for (int i = 0; i < 2 + (int)o.nm; ++i) {
        Range &r = i < 2 ? o.r[i] : o.m[i - 2];
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
                std::fprintf(stderr, "[ovreset]   ov%03d %s +%zx..+%zx (%p) changed, now", ov,
                             i == 0 ? "data" : i == 1 ? "dsstate" : "guard", s, k, (const void *)(cur + s));
                for (size_t q = s; q <= k && q < s + 16; ++q)
                    std::fprintf(stderr, " %02x", cur[q]);
                std::fprintf(stderr, ", start-up");
                for (size_t q = s; q <= k && q < s + 16; ++q)
                    std::fprintf(stderr, " %02x", r.snap[q]);
                std::fprintf(stderr, "\n");
                ++runs;
            }
        }
        if (mode() == 1)
            std::memcpy((void *)r.b, r.snap, r.n);
        if (logging())
            for (size_t k = 0; k < r.n; ++k)
                after += cur[k] != r.snap[k];
    }
    std::fprintf(stderr, "[ovreset] ov%03d load #%u: %s %zu bytes, %zu differed from the start-up image%s",
                 ov, o.loads, mode() == 1 ? "restored" : "would restore", tot, diff, logging() ? "" : "\n");
    if (logging())
        std::fprintf(stderr, ", %zu differ after this load\n", after);
    return mode() == 1;
}


/* LOVESME2. Take the overlay's start-up image again, from what the objects hold NOW. For an overlay whose
   one-time bring-up (constructors, state-table seats) runs after its first load, so the image taken in
   port_ov_load is the one from before it. Calling this once the bring-up is done makes every later restore put
   the bring-up back with the rest, as the DS's own load plus static initialisers leaves the overlay. */
extern "C" void port_ov_retake(int ov)
{
    if (mode() == 0 || ov < 0 || ov >= 128)
        return;
    Ov &o = g_ov[ov];
    if (!o.seen)
        return;
    size_t tot = 0;
    for (int i = 0; i < 2 + (int)o.nm; ++i) {
        Range &r = i < 2 ? o.r[i] : o.m[i - 2];
        if (r.n && r.snap) {
            std::memcpy(r.snap, r.b, r.n);
            tot += r.n;
        }
    }
    if (logging())
        std::fprintf(stderr, "[ovreset] ov%03d start-up image retaken after its bring-up, %zu bytes\n", ov, tot);
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
