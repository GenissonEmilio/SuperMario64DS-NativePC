// ARENA MODE, eating (see vs_arena_eat.h). Run mp1, lane EAT1, plan A5.
//
// PART 1: the tongue never eats a player in arena mode. Player::OnYoshiTryEat
// (vtable slot 18) is the question the Yoshi tongue asks of whatever it aims at
// (func_ov002_020d708c) and touches (func_ov002_020d6998); answering 0 is the
// ROM's own refusal path, the one an uneatable object takes. The tongue's hit
// flag (0x8000) on a puppet's body cylinder is cleared too, so a refused tongue
// is no HIT claim either. Outside arena mode the slot is the ROM's own.

#include "vs_arena_eat.h"

#include "vs_arena.h"
#include "vs_arena_ref.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f2d8;
extern unsigned char data_ov002_0210a83c[];    // _ZTV6Player
extern char data_0209f498[];                   // Stage::CheckInput's Ctrl[4], stride 0x18
extern unsigned short data_020a0e58[];         // PadData[4]: {held, pressed} (the raw DS pad)
extern int data_020a0e5a[];                    // the split "pressed" symbol
extern unsigned short *data_0209214c[];
char *_ZN8dActor_c10FindWithIDEj(unsigned id);       // the ROM's actor list lookup by unique id        // CheckInput's DS bit -> Ctrl bit maps
}

namespace port {
namespace {

enum : int {
    kSlotOnYoshiTryEat = 18,     // dActor_c's first own virtual (src/_ZN6Player13OnYoshiTryEatEv.cpp)
    kPadSlotBytes = 0x18,
    kTongueFlag = 0x8000,        // the tongue's hit flag on a body cylinder
    kBodyClsnHit = 0x2f4,        // the pair pass's hit flags on a body (+0x2d4 + 0x20)
};

struct Eat {
    bool wrapped = false;
    bool log = false;
    unsigned long long asked = 0, refused = 0, rig_tongues = 0, tongue_bits = 0;
    unsigned orig = 0;                 // the face the slot held before us
    int rig = 0;                       // 1 tongue
    unsigned rig_frames = 0;           // arena frames the rig has run
    int rig_press = 0;
    int rig_pin = 0;
    char *mouth_ptr = nullptr;         // what my mouth held last frame, and its unique id
    unsigned mouth_uid = 0;
    unsigned long long mouth_cleared = 0;
};
Eat g;

int &s32r(char *b, int off) { return *(int *)(b + off); }
short &s16r(char *b, int off) { return *(short *)(b + off); }

// Slot 18 of the Player's vtable, wrapped. __thiscall as a __fastcall face: the
// receiver in ecx, a dummy edx, no stack arguments.
int __fastcall eat_face(void *self, void *edx) {
    if (!vs_arena_active())
        return ((int (__fastcall *)(void *, void *))(size_t)g.orig)(self, edx);
    ++g.asked;
    ++g.refused;
    if (g.log || g.refused <= 3)
        std::fprintf(stderr, "[eat] tongue on a player (body %p): refused, the ROM's "
                     "own refusal path (n=%llu)\n", self, g.refused);
    return 0;
}

void wrap_once() {
    if (g.wrapped) return;
    void **vt = (void **)data_ov002_0210a83c;
    g.orig = (unsigned)(size_t)vt[kSlotOnYoshiTryEat];
    if (!g.orig) return;           // the slot is not seated yet
    vt[kSlotOnYoshiTryEat] = (void *)&eat_face;
    g.wrapped = true;
    g.log = std::getenv("SM64DS_EAT_LOG") && *std::getenv("SM64DS_EAT_LOG") == '1';
    const char *r = std::getenv("SM64DS_EAT_RIG");
    if (r && std::strncmp(r, "tongue", 6) == 0) g.rig = 1;
    if (r && std::strncmp(r, "stalemouth", 10) == 0) g.rig = 3;
    std::fprintf(stderr, "[eat] arena eating: Player::OnYoshiTryEat wrapped (the tongue "
                 "never eats a player)%s\n", g.rig ? ", rig: tongue" : "");
}

// A refused tongue is a harmless one: the tongue's hit flag on a puppet's body
// cylinder must not become a HIT claim either (the referee client reads the
// pair pass's flags in port_vs_match_end_hold, after vs_eat_before_walk).
void clear_tongue_bits() {
    const ArenaStats st = vs_arena_stats();
    const int me = vs_arena_my_seat();
    for (int k = 0; k < st.bodies && k < kArenaSeats; ++k) {
        if (k == me || !data_0209f394[k]) continue;
        unsigned &f = *(unsigned *)((char *)data_0209f394[k] + kBodyClsnHit);
        if (f & kTongueFlag) { f &= ~(unsigned)kTongueFlag; ++g.tongue_bits; }
    }
}

// THE MOUTH MUST NEVER HOLD A DEAD ACTOR. The Yoshi's eating states call a
// virtual on whatever Player +0x360 (mObjInMouth) names (func_ov002_020d7430,
// St_Swallow, St_YoshiPower_Main). In an arena the actors around a body are
// replaced under it by the referee's events (a star taken and woken again, a
// marker broken), and a body that leaves and comes back is re-seated: a stale
// pointer there is a call through freed memory (an access violation seen on a
// staying console of a 4-player match after another console crashed and
// rejoined). So once a frame, before the walk, a mouthful that is not one of
// the seat bodies must still be the live actor it was: found in the ROM's own
// list by its unique id, and the same id as last frame. Otherwise the pointer
// is dropped (the eating state finds an empty mouth, which it handles).
void sanitize_mouth() {
    const int me = vs_arena_my_seat();
    if (me < 0 || me >= kArenaSeats) return;
    char *b = (char *)data_0209f394[me];
    if (!b) return;
    char *m = *(char **)(b + 0x360);
    if (!m) { g.mouth_ptr = nullptr; return; }
    const ArenaStats st = vs_arena_stats();
    for (int k = 0; k < st.bodies && k < kArenaSeats; ++k)
        if (data_0209f394[k] == m) { g.mouth_ptr = nullptr; return; }   // a seat body: permanent
    const unsigned uid = *(unsigned *)(m + 4);
    const bool changed = m == g.mouth_ptr && uid != g.mouth_uid;
    if (changed || !uid || _ZN8dActor_c10FindWithIDEj(uid) != m) {
        *(char **)(b + 0x360) = nullptr;
        g.mouth_ptr = nullptr;
        ++g.mouth_cleared;
        std::fprintf(stderr, "[eat] f%u: my mouth held %p (uid %u), which is no longer a live "
                     "actor: dropped\n", vs_arena_frame(), (void *)m, uid);
        return;
    }
    g.mouth_ptr = m;
    g.mouth_uid = uid;
}

// A press of the ROM's own button words (Ctrl bit 0x0001 = attack) goes in as the
// RAW DS pad bit whose remap gives it, into PadData before the actor walk:
// Stage::Behavior (CheckInput) rebuilds the Ctrl block from PadData at the top of
// the walk, so a write into the Ctrl words never reaches a body. `first` = the
// edge (the pressed word) too.
void rig_press(int slot, unsigned short ctrl_mask, bool first) {
    if (slot < 0 || slot > 3) return;
    const unsigned char mode = *(unsigned char *)(data_0209f498 + slot * kPadSlotBytes + 0x16);
    const unsigned short *map = data_0209214c[mode < 3 ? mode : 0];
    unsigned short ds = 0;
    for (int j = 0; j < 16; ++j)
        if (map[j] & ctrl_mask) ds |= (unsigned short)(1u << j);
    data_020a0e58[slot * 2] |= ds;
    if (first) {
        data_020a0e58[slot * 2 + 1] |= ds;
        *(unsigned short *)((char *)data_020a0e5a + slot * 4) |= ds;
    }
}

// SM64DS_EAT_RIG=tongue: each of seats 0 and 1 stands 40 units behind the other's
// puppet, faces it and uses the tongue (the attack button) once per 120 arena
// frames, seat 1 half a cycle after seat 0. The stance is held for the first 25
// frames (the selftest's own pad tape walks and turns the body).
void rig_tongue() {
    const int me = vs_arena_my_seat();
    if (data_0209f2d8 != 1 || me < 0 || me > 1) return;
    if (!vs_ref_stats().started) return;    // the referee's START (the countdown is over)
    const int other = 1 - me;
    ++g.rig_frames;
    char *mb = (char *)data_0209f394[me];
    char *vb = (char *)data_0209f394[other];
    if (!mb || !vb) return;
    ArenaSeatView sv;
    if (!vs_arena_seat_view(other, &sv) || !sv.has_state) return;
    if (g.rig_pin > 0) {
        --g.rig_pin;
        int *p = (int *)(mb + 0x5c);
        p[0] = s32r(vb, 0x5c);
        p[2] = s32r(vb, 0x64) - (40 << 12);
        s32r(mb, 0x98) = 0;
        s16r(mb, 0x8e) = 0;
        s16r(mb, 0x94) = 0;
    }
    if (g.rig_press > 0) {
        rig_press(me, 0x0001, g.rig_press == 3);
        --g.rig_press;
        return;
    }
    if (g.rig_frames < 240) return;
    if ((g.rig_frames + (me ? 60u : 0u)) % 120u != 0) return;
    int *p = (int *)(mb + 0x5c);
    p[0] = s32r(vb, 0x5c);
    p[1] = s32r(vb, 0x60);
    p[2] = s32r(vb, 0x64) - (40 << 12);
    s32r(mb, 0x98) = 0;
    s32r(mb, 0xa8) = 0;
    s16r(mb, 0x8e) = 0;
    s16r(mb, 0x94) = 0;
    g.rig_press = 3;
    g.rig_pin = 25;
    ++g.rig_tongues;
    std::fprintf(stderr, "[eat] rig: tongue %llu (40 units behind the puppet of seat %d)\n",
                 g.rig_tongues, other);
}

unsigned char g_stale_block[0x400];     // zeroed: a "freed" actor (no vtable)

// SM64DS_EAT_RIG=stalemouth: every 120 arena frames, once the match runs, my mouth is
// pointed at a dead block (what a replaced star or a re-seated body leaves behind). The
// guard in sanitize_mouth runs right after in the same call, before the walk.
void rig_stale() {
    const int me = vs_arena_my_seat();
    if (data_0209f2d8 != 1 || me < 0 || me >= kArenaSeats || !vs_ref_stats().started) return;
    ++g.rig_frames;
    char *b = (char *)data_0209f394[me];
    if (!b || g.rig_frames < 120 || g.rig_frames % 120 != 0) return;
    *(char **)(b + 0x360) = (char *)g_stale_block;
    ++g.rig_tongues;
    std::fprintf(stderr, "[eat] rig: my mouth now names a dead block (%p)\n", (void *)g_stale_block);
}

void report_at_exit() {
    std::fprintf(stderr, "[eat] exit asked=%llu refused=%llu rig_tongues=%llu "
                 "tongue_bits_cleared=%llu mouth_cleared=%llu\n", g.asked, g.refused,
                 g.rig_tongues, g.tongue_bits, g.mouth_cleared);
}

}  // namespace

void vs_eat_before_walk(int frame) {
    (void)frame;
    if (!vs_arena_active()) return;
    if (!g.wrapped) {
        wrap_once();
        static bool reg = false;
        if (g.wrapped && !reg) { reg = true; std::atexit(report_at_exit); }
    }
    if (!g.wrapped) return;
    if (g.rig == 3) rig_stale();
    if (data_0209f2d8 == 1) { sanitize_mouth(); clear_tongue_bits(); }
    if (g.rig == 1) rig_tongue();
}

VsEatStats vs_eat_stats() {
    VsEatStats s;
    s.wrapped = g.wrapped;
    s.asked = g.asked;
    s.refused = g.refused;
    s.rig_tongues = g.rig_tongues;
    return s;
}

}  // namespace port
