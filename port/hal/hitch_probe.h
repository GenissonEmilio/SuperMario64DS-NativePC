// HITCH9C (run mp1): named cost counters for the hitch log (tests/perf_trace.inc, "[hitch]" lines).
//
// A game frame that takes far longer than its neighbours is a "hitch". perf_trace.inc times the frame loop's own stages
// (ROM tick, camera, raster, overlay, present, sound, pace). The work that hides INSIDE those stages (a texture upload
// the first time a model is drawn, a file read the first time an animation is asked for, the network hooks, the chat
// painter) is timed here, by name, with one HITCH_SCOPE(kind) around the call. The loop reads and clears the table at
// the end of each frame and prints the entries that cost something on a frame that hitched.
//
// Cost when nothing hitches: two QueryPerformanceCounter reads per scope (tens of nanoseconds) and an add.
// Header only (C++17 inline variables) so every target that includes it needs no extra source file.
// PORT_HOST_ABI: host-only, touches no game state.
#ifndef PORT_HAL_HITCH_PROBE_H
#define PORT_HAL_HITCH_PROBE_H

#include <chrono>
#include <cstdint>

namespace port {

enum HitchKind {
    HK_TEX = 0,      // gpu_raster upload_texture: first draw of a texture (premultiply + mip build + upload)
    HK_CARDFILE,     // fs_names img_file: a file read from disk on its first touch
    HK_SFPLOAD,      // fs Load seam (host SharedFilePtr::Load): miss path
    HK_TONGUE,       // ghost_tongue: the ROM call that attaches / detaches a ghost's tongue head
    HK_CHAT,         // chat_host_frame: the chat painter
    HK_NETHOOK,      // the per-frame MP hooks (ghost hold, hub presence, coop world, party stars / hits, star popup)
    HK_HUBPUMP,      // comms_hub_pump: the datagram handlers run from the game thread
    HK_GHOSTFOLLOW,  // comms_sync gw_follow: the adventure ghosts' pose follower
    HK_COUNT
};

inline const char *const kHitchKindName[HK_COUNT] = {"tex", "cardfile", "sfpload", "tongue", "chat", "nethook", "hubpump", "ghostfollow"};

struct HitchCell { double ms; unsigned n; };
inline HitchCell g_hitch_cell[HK_COUNT];

inline double hitch_now_ms() {
    using clk = std::chrono::steady_clock;   // QueryPerformanceCounter underneath
    return std::chrono::duration<double, std::milli>(clk::now().time_since_epoch()).count();
}

struct HitchScope {
    int kind;
    double t0;
    explicit HitchScope(int k) : kind(k), t0(hitch_now_ms()) {}
    ~HitchScope() {
        HitchCell &c = g_hitch_cell[kind];
        c.ms += hitch_now_ms() - t0;
        ++c.n;
    }
};

}  // namespace port

#define HITCH_CAT2(a, b) a##b
#define HITCH_CAT(a, b) HITCH_CAT2(a, b)
#define HITCH_SCOPE(kind) ::port::HitchScope HITCH_CAT(hitch_scope_, __LINE__)((::port::kind))

#endif
