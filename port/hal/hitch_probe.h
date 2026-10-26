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
    HK_GPUBUILD,     // gpu_raster: CPU side of a picture (vertex build, state, texture lookups) before the draw
    HK_GPUDRAW,      // gpu_raster: the draw calls submitted
    HK_GPUWAIT,      // gpu_raster: the CPU waiting on the card (Map of the staging planes)
    HK_GPUCOPY,      // gpu_raster: the CPU copying the planes out
    HK_FLUSH,        // the frame's fflush(stdout)
    HK_SWAP,         // the ROM frame step + geometry swap at the frame boundary
    HK_MSGBM,        // message_compositor bm_fill_default (the brightness mask's default pass)
    HK_MSGRESOLVE,   // message_compositor hudelem::resolve (the widescreen element labelling), when it ran
    // TITLEPERF1: the scene tick's own blocks, for the dev-only per-section summary (SM64DS_TP_SECT=1); never printed on a hitch line
    HK_TP_TICK,      // port_scene_tick: port_actor_tick (the ROM's behaviour walk)
    HK_TP_RENDER,    // port_scene_tick: port_actor_render (the ROM's render walk, gx command decode, smoothing)
    HK_TP_CLEAR,     // port_scene_tick: gx_clear_fill
    HK_TP_GXREND,    // port_scene_tick: ntr::gx_render (the 3D picture: card build, draw, wait, copy)
    HK_TP_IP1,       // port_scene_tick: SmoothMotion hook stage 1
    HK_TP_COMPA,     // port_scene_tick: port_message_composite_engine_a
    HK_TP_SUBPRES,   // port_scene_tick: hal_sub_screen_present (the menu compose and bottom screen)
    HK_TP_IP2,       // port_scene_tick: SmoothMotion hook stage 2
    HK_TP_SOUND,     // port_scene_tick: sdat_host_tick
    HK_TP_IPA,       // SmoothMotion ip_build_a (replay + raster)
    HK_TP_IPM1,      // ip_build_b: the tick's 2D merged over the replay (p3/p2)
    HK_TP_IPRL,      // ip_build_b: hal_menu_relift (the menu's re-draw)
    HK_TP_IPM2,      // ip_build_b: the second merge (p4/p5)
    HK_COUNT
};

inline const char *const kHitchKindName[HK_COUNT] = {"tex", "cardfile", "sfpload", "tongue", "chat", "nethook", "hubpump", "ghostfollow", "gpubuild", "gpudraw", "gpuwait", "gpucopy", "flush", "swap", "msgbm", "msgresolve", "tp_tick", "tp_render", "tp_clear", "tp_gxrend", "tp_ip1", "tp_compa", "tp_subpres", "tp_ip2", "tp_sound", "tp_ipa", "tp_ipm1", "tp_iprl", "tp_ipm2"};

struct HitchCell { double ms; unsigned n; };
inline HitchCell g_hitch_cell[HK_COUNT];

inline double hitch_now_ms() {
    using clk = std::chrono::steady_clock;   // QueryPerformanceCounter underneath
    return std::chrono::duration<double, std::milli>(clk::now().time_since_epoch()).count();
}

inline void hitch_add(int kind, double ms) {
    HitchCell &c = g_hitch_cell[kind];
    c.ms += ms;
    ++c.n;
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
