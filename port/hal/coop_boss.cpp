// COOP BOSSES. Run mp1, lane BOSSCORE1. See coop_boss.h for the design.
//
// PORT_HOST_ABI. Reads and writes live boss actors by raw offset (the established convention in port/hal); nothing in
// src/ changes. Every received value is range-checked before it is written.

#include "coop_boss.h"
#include "coop_target.h"
#include "coop_world.h"
#include "coop_groups.h"
#include "coop_wire.h"
#include "comms_hub.h"

#include "dActor_c.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250;
extern short data_02092144[];
void *_ZN8dActor_c4NextEPKS_(const void *prev);
int KingBobOmb_SetState(void *c, void *state);
// bossdesc1: Chief Chilly's state records (16 x 0x10 at ov073 0x02123320), his ROM transition, Wiggler's ROM transition, ModelAnim::SetAnim
int ChiefChilly_ChangeState(void *c, void *state);
void func_ov034_021125b8(void *c, int state);
int _ZN9ModelAnim7SetAnimEP8BCA_Filei5Fix12IiEj(void *anim, void *file, int a, int speed, unsigned int b);
extern char data_ov073_02123320[], data_ov073_02123330[], data_ov073_02123340[], data_ov073_02123350[], data_ov073_02123360[],
            data_ov073_02123370[], data_ov073_02123380[], data_ov073_02123390[], data_ov073_021233a0[], data_ov073_021233b0[],
            data_ov073_021233c0[], data_ov073_021233d0[], data_ov073_021233e0[], data_ov073_021233f0[], data_ov073_02123400[],
            data_ov073_02123410[];
extern char data_ov078_02126ffc[], data_ov078_0212700c[], data_ov078_0212701c[], data_ov078_0212702c[], data_ov078_0212703c[],
            data_ov078_0212704c[], data_ov078_0212705c[], data_ov078_0212706c[], data_ov078_0212707c[], data_ov078_0212708c[],
            data_ov078_0212709c[], data_ov078_021270ac[], data_ov078_021270bc[], data_ov078_021270cc[], data_ov078_021270dc[],
            data_ov078_021270ec[], data_ov078_021270fc[], data_ov078_0212710c[];
}

namespace port {
namespace {

enum : int { kSeats = 4, kSlots = 512, kPayloadMax = kHubCoopMaxDatagram - kHubCoopHeaderBytes };

// ---------------------------------------------------------------------------
// The boss rows. A field is {offset, size, kind}; the kinds say how a value crosses the wire.
// ---------------------------------------------------------------------------
enum : uint8_t { kPlain = 0, kStatePtr = 1, kVol = 2,   // kVol: changes every frame, never a reason to publish by itself
                 // bossdesc1 kinds:
                 kChillyPtr = 4,    // Chief Chilly's state record pointer (+0x37c), mapped to a table id; set through ChiefChilly_ChangeState
                 kAlive = 8,        // a read-only "HP" for a boss without one: 1 while the int state at the field is not 4 (dead) or 5 (gone)
                 kMaxUp = 16,       // a counter that only goes up (0xff = "not used" is never touched)
                 kBullyState = 32,  // Big Bully's int state (+0x398): written with the ROM's animation for that state
                 kWigState = 64 };  // Wiggler's state index (+0x8c4): set through func_ov034_021125b8
enum : int { kLoOff = 0x100, kHiOff = 0x900 };
struct BField { uint16_t off; uint8_t size; uint8_t kind; };
struct BDesc {
    uint16_t cls;
    const char *name;
    uint8_t nf;
    BField f[kBossMaxFields];
    int8_t hp;        // index of the HP field (min wins), -1 none
    int8_t state;     // index of the state field, -1 none
    int8_t pos;       // index of the first of three position fields (x, y, z), -1 none
    uint16_t pl[2];   // offsets of player pointers the class's states dereference (a copy that is put into such a state must hold one)
    uint16_t hit_who; // 0, or the offset where the ROM keeps the pointer of the player that hit (the proof knob must set it too)
    uint16_t hit_off; // 0, or the offset of a byte the class's own handler turns into one HP (the proof knob sets it)
};

// Whomp King (daBtn_c, shared Whomp class, king flag +0x414): include/Whomp.h, src/func_ov079_02124b08.c / 02124ed4.c:
// mState +0x3b0, HP +0x401 (3 for the king), the hit flag +0x402, the sub-state +0x40c, the flat-on-the-ground timer
// +0x3fc (u16), the state frame counter +0x100 (u16), +0x407, and the animation cursor in the ModelAnim's Animation.
const BDesc kDescs[] = {
    { 165, "WHOMP_KING", 14,
      { { 0x3b0, 4, kPlain }, { 0x401, 1, kPlain }, { 0x40c, 1, kPlain }, { 0x407, 1, kPlain }, { 0x3fc, 2, kVol },
        { 0x100, 2, kVol }, { 0x324, 4, kVol }, { 0x5c, 4, kVol }, { 0x60, 4, kVol }, { 0x64, 4, kVol },
        { 0x98, 4, kVol }, { 0x9c, 4, kVol }, { 0xa0, 4, kVol }, { 0xa8, 4, kVol } },
      1, 0, 7, { 0x3ac, 0 }, 0x3ac, 0x402 },
    // King Bob-omb (daBombking_c): state record pointer +0x420 (mapped to a table id), HP mHealth +0x500 (3), mPhase +0x4a0,
    // the two timers +0x504 / +0x505, mAnimSpeed +0x4fc, +0x499.
    { 189, "KING_BOB_OMB", 14,
      { { 0x420, 4, kStatePtr }, { 0x500, 4, kPlain }, { 0x4a0, 4, kPlain }, { 0x504, 1, kVol }, { 0x505, 1, kVol },
        { 0x4fc, 4, kPlain }, { 0x499, 1, kPlain }, { 0x5c, 4, kVol }, { 0x60, 4, kVol }, { 0x64, 4, kVol },
        { 0x98, 4, kVol }, { 0x9c, 4, kVol }, { 0xa0, 4, kVol }, { 0xa8, 4, kVol } },
      1, 0, 7, { 0x430, 0 }, 0, 0 },
    // ---- bossdesc1 (run mp1, lane BOSSDESC1). Offsets from include/ChiefChilly.h, daOts_c.h / BigBully.h, daTrs_c.h, Wiggler.h and
    // the state functions read for each (see out/BOSSDESC1/notes). Console-local pointers are never fields: they sit in pl[].
    // Chief Chilly (class 218, daKing_Donketu_c): state record pointer +0x37c, knock-downs left mHitsRemaining +0x4cb (3 -> 0, taken
    // when he slides off the edge: func_ov073_02120c7c), sub-state +0x4c8, flags +0x4ca, waypoint cursor +0x4c4, turn target +0x4c6,
    // phase counter +0x4b4. +0x3e4 is the player he talks to (console-local). His reward is a KEY (class 282), spawned by each console.
    { 218, "CHIEF_CHILLY", 14,
      { { 0x37c, 4, kChillyPtr }, { 0x4cb, 1, kPlain }, { 0x4c8, 1, kPlain }, { 0x4ca, 1, kPlain }, { 0x4c4, 1, kPlain },
        { 0x4c6, 2, kVol }, { 0x4b4, 4, kVol }, { 0x5c, 4, kVol }, { 0x60, 4, kVol }, { 0x64, 4, kVol },
        { 0x98, 4, kVol }, { 0x9c, 4, kVol }, { 0xa0, 4, kVol }, { 0xa8, 4, kVol } },
      1, 0, 7, { 0x3e4, 0 }, 0, 0 },
    // Big Bully (class 216, daBDonketu_c on daOts_c): no HP. He is killed by the lava (state 4) and the star is spawned from there.
    // field 0 = the int state +0x398, field 1 = "alive" derived from it, then the bullies-killed counter +0x3fe (the three small
    // bullies he spawns die on each console, or by the death list: only goes up), the bounce count +0x3f9, and the death point
    // +0x3a8 / +0x3ac / +0x3b0 (the particles of the dying state read it).
    { 216, "BIG_BULLY", 14,
      { { 0x398, 4, kBullyState }, { 0x398, 4, kAlive }, { 0x3fe, 1, kMaxUp }, { 0x3f9, 1, kVol }, { 0x3a8, 4, kPlain },
        { 0x3ac, 4, kPlain }, { 0x3b0, 4, kPlain }, { 0x5c, 4, kVol }, { 0x60, 4, kVol }, { 0x64, 4, kVol },
        { 0x98, 4, kVol }, { 0x9c, 4, kVol }, { 0xa0, 4, kVol }, { 0xa8, 4, kVol } },
      1, 0, 7, { 0, 0 }, 0, 0 },
    // Big Boo (class 210, daTrs_c, types 12..15): state byte +0x5cc, hits left +0x5ca (3 -> 0, taken in state 3 when the timer is 0),
    // sub-state +0x5ce, +0x5c9, timer +0x5c2, +0x5d2, scale +0x584. +0x488 / +0x484 are the players he talks to (console-local).
    // NOT synced on purpose: +0x5c8 (opacity) and the flags word +0x5d4 (camera / area visibility): those key on the LOCAL camera.
    { 210, "BIG_BOO", 14,
      { { 0x5cc, 1, kPlain }, { 0x5ca, 1, kPlain }, { 0x5ce, 1, kPlain }, { 0x5c9, 1, kPlain }, { 0x5c2, 2, kVol },
        { 0x5d2, 1, kPlain }, { 0x584, 4, kPlain }, { 0x5c, 4, kVol }, { 0x60, 4, kVol }, { 0x64, 4, kVol },
        { 0x98, 4, kVol }, { 0x9c, 4, kVol }, { 0xa0, 4, kVol }, { 0xa8, 4, kVol } },
      1, 0, 7, { 0x488, 0x484 }, 0, 0 },
    // Wiggler (class 248): state index +0x8c4 (set through func_ov034_021125b8, whose enter half takes a health point in state 3),
    // hits left +0x8db (4 -> 1, then state 7 is his death), angry +0x8de, falling +0x8df, dying +0x8e0, timer +0x8da, turn target
    // +0x8d8. +0x8c8 is the player he talks to; +0x8e1 (drive the camera) stays local. The body chain is rebuilt from the head.
    { 248, "WIGGLER", 14,
      { { 0x8c4, 4, kWigState }, { 0x8db, 1, kPlain }, { 0x8de, 1, kPlain }, { 0x8df, 1, kPlain }, { 0x8e0, 1, kPlain },
        { 0x8da, 1, kVol }, { 0x8d8, 2, kVol }, { 0x5c, 4, kVol }, { 0x60, 4, kVol }, { 0x64, 4, kVol },
        { 0x98, 4, kVol }, { 0x9c, 4, kVol }, { 0xa0, 4, kVol }, { 0xa8, 4, kVol } },
      1, 0, 7, { 0x8c8, 0 }, 0, 0 },
};
const int kNumDescs = (int)(sizeof kDescs / sizeof kDescs[0]);

const BDesc *desc_of(unsigned cls) {
    for (int i = 0; i < kNumDescs; ++i) if (kDescs[i].cls == cls) return &kDescs[i];
    return nullptr;
}

bool field_ok(const BField &f) {
    if (f.size != 1 && f.size != 2 && f.size != 4) return false;
    const bool low_ok = f.off == 0x5c || f.off == 0x60 || f.off == 0x64 || f.off == 0x98 || f.off == 0x9c || f.off == 0xa0 || f.off == 0xa8;
    if (!low_ok && (f.off < kLoOff || (int)f.off + f.size > kHiOff)) return false;
    return (f.off % f.size) == 0;
}

// King Bob-omb's state records, by ROM address (the host's symbols are the same records the ROM code names).
char *const *kbo_states(int *n) {
    static char *const k[] = {
        data_ov078_02126ffc, data_ov078_0212700c, data_ov078_0212701c, data_ov078_0212702c, data_ov078_0212703c, data_ov078_0212704c,
        data_ov078_0212705c, data_ov078_0212706c, data_ov078_0212707c, data_ov078_0212708c, data_ov078_0212709c, data_ov078_021270ac,
        data_ov078_021270bc, data_ov078_021270cc, data_ov078_021270dc, data_ov078_021270ec, data_ov078_021270fc, data_ov078_0212710c };
    *n = (int)(sizeof k / sizeof k[0]);
    return k;
}
uint32_t kbo_ptr_to_id(uint32_t ptr) {
    int n; char *const *k = kbo_states(&n);
    for (int i = 0; i < n; ++i) if ((uint32_t)(uintptr_t)k[i] == ptr) return (uint32_t)i;
    return 0xffu;
}
void *kbo_id_to_ptr(uint32_t id) {
    int n; char *const *k = kbo_states(&n);
    return id < (uint32_t)n ? (void *)k[id] : nullptr;
}

// Chief Chilly's sixteen state records (data_ov073_02123320 + 0x10 * i).
char *const *chilly_states(int *n) {
    static char *const k[] = {
        data_ov073_02123320, data_ov073_02123330, data_ov073_02123340, data_ov073_02123350, data_ov073_02123360, data_ov073_02123370,
        data_ov073_02123380, data_ov073_02123390, data_ov073_021233a0, data_ov073_021233b0, data_ov073_021233c0, data_ov073_021233d0,
        data_ov073_021233e0, data_ov073_021233f0, data_ov073_02123400, data_ov073_02123410 };
    *n = (int)(sizeof k / sizeof k[0]);
    return k;
}
uint32_t chilly_ptr_to_id(uint32_t ptr) {
    int n; char *const *k = chilly_states(&n);
    for (int i = 0; i < n; ++i) if ((uint32_t)(uintptr_t)k[i] == ptr) return (uint32_t)i;
    return 0xffu;
}
void *chilly_id_to_ptr(uint32_t id) {
    int n; char *const *k = chilly_states(&n);
    return id < (uint32_t)n ? (void *)k[id] : nullptr;
}

// Big Bully (daOts_c): the int state at +0x398 (0 wait, 1 run, 2 knocked back, 3 turned at a cliff, 4 dying, 5 gone). The ROM sets
// the animation at each transition (src/actors/daOts_c.cpp); a copy that is put into a state gets the same animation and a fresh
// state timer. Death is final: a copy that is dying or gone is never pulled back.
void bully_enter(char *a, uint32_t st) {
    if (st > 5) return;
    int32_t cur; std::memcpy(&cur, a + 0x398, 4);
    if (cur == (int32_t)st || cur == 4 || cur == 5) return;
    const int32_t v = (int32_t)st;
    std::memcpy(a + 0x398, &v, 4);
    const uint16_t zero = 0; std::memcpy(a + 0x100, &zero, 2);
    static const int kFile[5] = { 4, 4, 2, 3, 1 };
    static const int kSpeed[5] = { 0x1000, 0x2000, 0x1000, 0x1000, 0x1000 };
    if (st < 5) {
        char *cfg = nullptr; std::memcpy(&cfg, a + 0x330, sizeof cfg);
        if (cfg) {
            char *sf = nullptr; std::memcpy(&sf, cfg + 4 * kFile[st], sizeof sf);
            void *anim = nullptr; if (sf) std::memcpy(&anim, sf + 4, sizeof anim);
            if (anim) _ZN9ModelAnim7SetAnimEP8BCA_Filei5Fix12IiEj(a + 0x110, anim, 0, kSpeed[st], 0);
        }
    }
}

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------
struct Rx { bool valid, fresh, first_done; unsigned t; uint16_t seq; uint8_t rflags; int from; uint32_t w[kBossMaxFields]; };
struct Boss {
    char *a;
    const BDesc *d;
    uint32_t seen;
    bool known;                    // first sight done
    int32_t synced_hp;             // HP as of the last publish / apply: a lower local HP is a hit by my player
    uint32_t last_w[kBossMaxFields]; bool lw_valid;
    unsigned pub_t;
    unsigned sample_t;
    int mism;                      // consecutive state mismatches against the authority's record
    int last_auth; unsigned auth_t;
    bool announced_dead;
    bool heard;                    // an authority record was ever applied here
    Rx rx;
};

CoopBossStats g_st = {};
bool g_env = false, g_on = true, g_log = false;
struct HitKnob { unsigned tick; bool done; bool armed; bool lethal; int hp0; int slot; };
HitKnob g_hit[4]; int g_nhit = 0;
struct ForgeKnob { unsigned tick; char name[12]; bool done; };
ForgeKnob g_forge[8]; int g_nforge = 0;
unsigned g_stand_tick = 0, g_stand_every = 0; int g_stand_dist = 0; bool g_stand_done = true, g_god = false;
bool g_regd = false;
int  g_my = -1;
CoopKey g_key;
unsigned g_ticks = 0, g_seq = 0;
Boss g_boss[kSlots];
unsigned g_claim_until[kSeats];     // wall ms until which a seat's claim holds
unsigned g_tok_t = 0; double g_tokens = 6.0;
unsigned g_stat_t = 0;

long long wall_ms() {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    return (long long)((((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000ull);
}
inline int32_t rd32(const char *a, int off) { int32_t v; std::memcpy(&v, a + off, 4); return v; }
inline int16_t rd16(const char *a, int off) { int16_t v; std::memcpy(&v, a + off, 2); return v; }

uint32_t read_field(const char *a, const BField &f) {
    uint32_t v = 0;
    std::memcpy(&v, a + f.off, f.size);
    if (f.kind & kStatePtr) v = kbo_ptr_to_id(v);
    else if (f.kind & kChillyPtr) v = chilly_ptr_to_id(v);
    else if (f.kind & kAlive) v = (v == 4u || v == 5u) ? 0u : 1u;
    return v;
}
void write_field(char *a, const BDesc &d, int idx, uint32_t v) {
    const BField &f = d.f[idx];
    if (!field_ok(f)) return;
    if (f.kind & kStatePtr) {
        void *p = kbo_id_to_ptr(v);
        if (!p) return;
        KingBobOmb_SetState(a, p);          // the ROM's own transition, with its enter code
        return;
    }
    if (f.kind & kChillyPtr) {
        void *p = chilly_id_to_ptr(v);
        if (!p) return;
        ChiefChilly_ChangeState(a, p);      // the ROM's own transition (his enter code runs)
        return;
    }
    if (f.kind & kAlive) return;            // derived, never written: the state field carries it
    if (f.kind & kWigState) { if (v <= 8u) func_ov034_021125b8(a, (int)v); return; }
    if (f.kind & kBullyState) { bully_enter(a, v); return; }
    if (f.kind & kMaxUp) {
        uint32_t cur = 0; std::memcpy(&cur, a + f.off, f.size);
        const uint32_t unused = f.size == 1 ? 0xffu : (f.size == 2 ? 0xffffu : 0xffffffffu);
        if (cur == unused || v == unused || v <= cur) return;
    }
    if (f.size == 1) v &= 0xffu; else if (f.size == 2) v &= 0xffffu;
    std::memcpy(a + f.off, &v, f.size);
}

// Sane values only: a state of Whomp King is a small index; HP is small.
bool value_ok(const BDesc &d, int idx, uint32_t v) {
    if (d.pos >= 0 && idx >= d.pos && idx < d.pos + 3) { const long long lim = 40000LL * 4096; return (int32_t)v > -lim && (int32_t)v < lim; }
    if (d.cls == 165) {
        if (idx == 0) return v < 16;                 // mState indexes the 12-cell handler table
        if (idx == 1) return v <= 8;                 // HP
        if (idx == 2) return v <= 32;                // sub-state
        return true;
    }
    if (d.cls == 189) {
        if (idx == 0) return v < 18;                 // a table id
        if (idx == 1) return v <= 8;                 // mHealth
        return true;
    }
    if (d.cls == 218) {
        if (idx == 0) return v < 16;                 // one of the sixteen state records
        if (idx == 1) return v <= 3;                 // knock-downs left
        if (idx == 2) return v <= 8;                 // sub-state
        return true;
    }
    if (d.cls == 216) {
        if (idx == 0) return v <= 5;                 // int state
        if (idx == 1) return v <= 1;                 // alive
        if (idx == 2) return v <= 4 || v == 0xffu;   // bullies killed (0xff: none to kill)
        return true;
    }
    if (d.cls == 210) {
        if (idx == 0) return v <= 8;                 // state byte
        if (idx == 1) return v <= 3;                 // hits left
        if (idx == 2) return v <= 16;                // sub-state
        return true;
    }
    if (d.cls == 248) {
        if (idx == 0) return v <= 8;                 // state index
        if (idx == 1) return v <= 4;                 // hits left
        return true;
    }
    return true;
}

void read_env() {
    if (g_env) return;
    g_env = true;
    auto flag = [](const char *n, bool dflt) { const char *e = std::getenv(n); return (e && *e) ? *e != '0' : dflt; };
    g_on = flag("SM64DS_COOP_BOSS", true);
    g_log = flag("SM64DS_COOP_BOSS_LOG", false);
    if (const char *h = std::getenv("SM64DS_COOP_BOSS_HIT")) {
        const char *p = h;
        while (*p && g_nhit < 4) {
            unsigned fr = 0; int used = 0;
            if (std::sscanf(p, "%u%n", &fr, &used) < 1 || used <= 0) break;
            const bool lethal = std::strncmp(p + used, ":lethal", 7) == 0;
            if (lethal) used += 7;
            g_hit[g_nhit].lethal = lethal;
            g_hit[g_nhit].tick = fr; g_hit[g_nhit].done = false; g_hit[g_nhit].armed = false; g_hit[g_nhit].hp0 = 0; g_hit[g_nhit].slot = -1; ++g_nhit;
            p += used; while (*p == ',' || *p == ';' || *p == ' ') ++p;
        }
    }
    if (const char *st = std::getenv("SM64DS_COOP_BOSS_STAND")) {
        unsigned f = 0, ev = 0; int d = 0;
        const int n = std::sscanf(st, "%u:%i:%u", &f, &d, &ev);
        if (n >= 2) { g_stand_tick = f; g_stand_dist = d; g_stand_done = false; g_stand_every = n >= 3 ? ev : 0; }
    }
    g_god = flag("SM64DS_COOP_BOSS_GOD", false);
    if (const char *f = std::getenv("SM64DS_COOP_BOSS_FORGE")) {
        const char *p = f;
        while (*p && g_nforge < 8) {
            unsigned fr = 0; char nm[12] = {}; int used = 0;
            if (std::sscanf(p, "%u:%11[a-z]%n", &fr, nm, &used) < 2 || used <= 0) break;
            g_forge[g_nforge].tick = fr; std::memcpy(g_forge[g_nforge].name, nm, sizeof nm); g_forge[g_nforge].done = false;
            ++g_nforge; p += used; while (*p == ',' || *p == ';' || *p == ' ') ++p;
        }
    }
    std::fprintf(stderr, "[cbs] boss sync %s%s\n", g_on ? "on" : "OFF (control)", g_log ? " log" : "");
}

bool is_player_ptr(const void *a) {
    for (int i = 0; i < 16; ++i) if (data_0209f394[i] == a) return true;
    return false;
}

// A state that was never entered on this copy finds its player pointers empty (Whomp King's defeat talk dereferences +0x3ac,
// King Bob-omb's +0x430): they are given the local player's body, so the dialogue and camera run for the player at this console.
void give_player_ptrs(Boss &b) {
    const int me = (int)data_0209f250;
    if (me < 0 || me >= 16 || !data_0209f394[me]) return;
    for (int k = 0; k < 2; ++k) {
        const uint16_t off = b.d->pl[k];
        if (!off) continue;
        void *cur = nullptr; std::memcpy(&cur, b.a + off, sizeof cur);
        if (!cur || !is_player_ptr(cur)) { void *mine = data_0209f394[me]; std::memcpy(b.a + off, &mine, sizeof mine); }
    }
}

CoopKeyWire wire_key() {
    CoopKeyWire w; w.level = (uint16_t)g_key.level; w.act = (uint8_t)g_key.act; w.reserved = 0;
    return w;
}

bool take_token(unsigned now) {
    if (g_tok_t == 0) g_tok_t = now;
    const unsigned dt = now - g_tok_t; g_tok_t = now;
    g_tokens = (std::min)(8.0, g_tokens + dt * 0.012);
    if (g_tokens < 1.0) return false;
    g_tokens -= 1.0;
    return true;
}

// The seat whose copy of this boss is the right one.
int auth_for(int slot, unsigned wnow) {
    for (int s = 0; s < kSeats; ++s)
        if ((int)(g_claim_until[s] - wnow) > 0 && (s == g_my || ((coop_group_member_mask() >> s) & 1u))) return s;
    int s = coop_target_seat_of_slot(slot);
    if (s >= 0) return s;
    const int o = coop_group_owner();
    return o >= 0 ? o : g_my;
}

int rec_bytes(const BDesc &d) { return (int)sizeof(BossRec) + 4 * d.nf; }

int fill_rec(unsigned char *out, int slot, const Boss &b, uint8_t rflags) {
    BossRec r; std::memset(&r, 0, sizeof r);
    r.slot = (uint16_t)slot; r.cls = b.d->cls; r.nf = b.d->nf; r.rflags = rflags; r.seq = (uint16_t)++g_seq;
    std::memcpy(out, &r, sizeof r);
    for (int k = 0; k < b.d->nf; ++k) {
        const uint32_t v = read_field(b.a, b.d->f[k]);
        std::memcpy(out + sizeof r + 4 * k, &v, 4);
    }
    return rec_bytes(*b.d);
}

void send_batch(int sub, uint8_t flags, const unsigned char *recs, int nrecs, int bytes) {
    unsigned char buf[kPayloadMax];
    BossHead h; std::memset(&h, 0, sizeof h);
    h.key = wire_key(); h.count = (uint8_t)nrecs; h.flags = flags;
    std::memcpy(buf, &h, sizeof h);
    std::memcpy(buf + sizeof h, recs, (size_t)bytes);
    if (coop_send_world(sub, buf, (int)sizeof h + bytes)) {
        if (sub == kWorldBossHit) ++g_st.hits_sent; else { ++g_st.sent; g_st.recs_sent += (unsigned)nrecs; }
    }
}

// ---------------------------------------------------------------------------
// Scan, publish, apply.
// ---------------------------------------------------------------------------
int g_nb = 0;
void scan() {
    for (int i = 0; i < kSlots; ++i) g_boss[i].a = nullptr;
    g_nb = 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        const unsigned id = *(const unsigned short *)(a + 0x0c);
        const BDesc *d = desc_of(id);
        if (!d) continue;
        const short slot = rd16(a, 0xce);
        if (slot < 0 || slot >= kSlots || is_player_ptr(a)) continue;
        if (id == 165 && *(const unsigned char *)(a + 0x414) == 0) continue;   // a plain Whomp that shares the class body
        Boss &b = g_boss[slot];
        if (b.a) continue;
        if (b.d != d) { b = Boss(); b.d = d; }
        b.a = a; ++b.seen; ++g_nb;
    }
    for (int i = 0; i < kSlots; ++i) if (!g_boss[i].a) { g_boss[i].seen = 0; g_boss[i].known = false; g_boss[i].lw_valid = false; g_boss[i].heard = false; g_boss[i].rx.valid = false; g_boss[i].rx.first_done = false; }
}

int hp_of(const Boss &b) { return (int)read_field(b.a, b.d->f[b.d->hp]); }
uint32_t state_of(const Boss &b) { return read_field(b.a, b.d->f[b.d->state]); }

void log_sample(int slot, const Boss &b, int auth) {
    char line[320]; int n = std::snprintf(line, sizeof line, "[cbs] w=%lld t=%u slot=%d cls=%u pos=%d,%d,%d auth=%d me=%d hp=%d st=%u",
                                          wall_ms(), g_ticks, slot, (unsigned)b.d->cls, rd32(b.a, 0x5c) >> 12, rd32(b.a, 0x60) >> 12, rd32(b.a, 0x64) >> 12,
                                          auth, g_my, hp_of(b), state_of(b));
    for (int k = 2; k < b.d->nf && n < (int)sizeof line - 20; ++k) n += std::snprintf(line + n, sizeof line - n, " f%d=%u", k, read_field(b.a, b.d->f[k]));
    n += std::snprintf(line + n, sizeof line - n, " tgt=%d", coop_target_seat_of_slot(slot));
    std::fprintf(stderr, "%s\n", line);
}

// After a state was forced onto a copy (bossdesc1). Big Boo's hit state takes its health point only on the first frame of the state
// (timer +0x100 == 0, src/unnamed/ov063/func_ov063_021177b0.c); the authority has taken it already and the merged HP is the truth, so a
// forced copy skips that frame.
void post_force(Boss &b) {
    if (b.d->cls == 210) {
        uint16_t t = 0; std::memcpy(&t, b.a + 0x100, 2);
        if (t == 0) { t = 1; std::memcpy(b.a + 0x100, &t, 2); }
    }
}

void apply_rx(int slot, Boss &b, unsigned now) {
    Rx &x = b.rx;
    if (!x.valid || !x.fresh) return;
    x.fresh = false;
    const BDesc &d = *b.d;
    ++g_st.recs_rx;
    // HP only goes down.
    const int lhp = hp_of(b);
    const int rhp = (int)x.w[d.hp];
    if (rhp >= 0 && rhp < lhp) {
        write_field(b.a, d, d.hp, (uint32_t)rhp);
        b.synced_hp = rhp;
        ++g_st.applied_hp;
        if (g_log) std::fprintf(stderr, "[cbs] apply-hp slot=%d hp %d->%d from seat %d w=%lld\n", slot, lhp, rhp, x.from, wall_ms());
    } else if (b.synced_hp > rhp && rhp >= 0) {
        b.synced_hp = rhp;
    }
    // counters that only go up (Big Bully's bullies killed): write_field refuses anything that is not larger
    for (int k = 0; k < d.nf; ++k)
        if ((d.f[k].kind & kMaxUp) && value_ok(d, k, x.w[k])) write_field(b.a, d, k, x.w[k]);
    // The state: forced on the first record this console ever takes, otherwise after two in a row that disagree.
    const uint32_t ls = state_of(b), rs = x.w[d.state];
    bool force = false;
    // King Bob-omb's two states in which HE holds a body (706c, 707c): the held pointer is a console-local one, so those states
    // are never forced onto a copy; the pose still follows through the mover path.
    const bool carry_state = d.cls == 189 && (rs == 7 || rs == 8);
    if (ls != rs) { ++b.mism; ++g_st.state_mismatch; } else b.mism = 0;
    if (!b.heard) force = ls != rs;
    else if (b.mism >= 2) force = true;
    if (carry_state) force = false;
    if (force) {
        // everything except the HP (already min-merged); the state field goes first so its enter code runs on the old fields
        // The state's enter code can have side effects of its own (King Bob-omb's hurt states take a health point): the HP
        // stays what the min-merge above made it.
        const int hp_keep = hp_of(b);
        give_player_ptrs(b);
        for (int k = 0; k < d.nf; ++k) if (k != d.hp && value_ok(d, k, x.w[k])) write_field(b.a, d, k, x.w[k]);
        write_field(b.a, d, d.hp, (uint32_t)hp_keep);
        post_force(b);
        b.synced_hp = hp_keep;
        b.mism = 0;
        ++g_st.applied_state;
        if (g_log) std::fprintf(stderr, "[cbs] apply-state slot=%d st %u->%u hp=%d from seat %d heard=%d w=%lld\n", slot, ls, rs, hp_of(b), x.from, (int)b.heard, wall_ms());
    }
    // The pose: the mover path carries it, but its heartbeat is 1.5 s and a copy that falls or slides in between (Whomp King's
    // slam, a forced state) is far off by then. A record this fresh that disagrees by more than 80 units puts the copy back,
    // with the speeds so it does not drift again.
    if (d.pos >= 0 && (unsigned)(now - x.t) < 300u && !(x.rflags & 1)) {
        double dd = 0;
        for (int k = 0; k < 3; ++k) { const double e = ((double)rd32(b.a, d.f[d.pos + k].off) - (double)(int32_t)x.w[d.pos + k]) / 4096.0; dd += e * e; }
        if (std::sqrt(dd) > 80.0) {
            for (int k = d.pos; k < d.nf; ++k) if (value_ok(d, k, x.w[k])) write_field(b.a, d, k, x.w[k]);
            ++g_st.applied_fields;
            if (g_log) std::fprintf(stderr, "[cbs] apply-pos slot=%d off by %.0f units from seat %d w=%lld\n", slot, std::sqrt(dd), x.from, wall_ms());
        }
    }
    b.heard = true;
    b.auth_t = now;
}

void process(unsigned now) {
    const unsigned wnow = comms_hub_now_ms();
    unsigned char recs[kPayloadMax]; int nrec = 0, bytes = 0;
    for (int slot = 0; slot < kSlots; ++slot) {
        Boss &b = g_boss[slot];
        if (!b.a) continue;
        const BDesc &d = *b.d;
        if (!b.known) { b.known = true; b.synced_hp = hp_of(b); }
        // King Bob-omb carried by MY player (state 707c with my body as the holder): the console that carries him is the one that is right.
        bool claiming = false;
        if (d.cls == 189) {
            const uint32_t st = state_of(b);
            char *holder = nullptr; std::memcpy(&holder, b.a + 0x494, sizeof holder);
            const int me = (int)data_0209f250;
            if ((st == 7 || st == 8) && holder && me >= 0 && me < 16 && holder == (char *)data_0209f394[me]) {
                if ((int)(g_claim_until[g_my] - wnow) <= 0) {
                    ++g_st.claims;
                    if (g_log) std::fprintf(stderr, "[cbs] claim slot=%d st=%u holder=me w=%lld\n", slot, st, wall_ms());
                }
                g_claim_until[g_my] = wnow + 3000u;
            }
            claiming = (int)(g_claim_until[g_my] - wnow) > 0;
        }
        const int auth = auth_for(slot, wnow);
        const bool mine = auth == g_my;
        // a hit that landed on my copy: HP below what was last synced
        const int hp = hp_of(b);
        if (hp < b.synced_hp) {
            ++g_st.hit_local;
            if (g_log) std::fprintf(stderr, "[cbs] hit slot=%d hp %d->%d (auth %d me %d) w=%lld\n", slot, b.synced_hp, hp, auth, g_my, wall_ms());
            b.synced_hp = hp;
            if (!mine && g_on) {
                unsigned char one[64];
                const int n = fill_rec(one, slot, b, 0);
                send_batch(kWorldBossHit, 1, one, 1, n);
            }
            b.lw_valid = false;                 // publish at once when I am the authority
        }
        if (g_on) apply_rx(slot, b, now);
        if (g_log && (unsigned)(now - b.sample_t) >= 500u) { b.sample_t = now; log_sample(slot, b, auth); }
        if (!b.announced_dead && hp_of(b) == 0 && g_log) {
            b.announced_dead = true;
            std::fprintf(stderr, "[cbs] defeated slot=%d cls=%u hp=0 st=%u w=%lld\n", slot, (unsigned)d.cls, state_of(b), wall_ms());
        }
        if (b.last_auth != auth) {
            if (g_log) std::fprintf(stderr, "[cbs] authority slot=%d %d->%d w=%lld\n", slot, b.last_auth, auth, wall_ms());
            b.last_auth = auth; b.auth_t = now; b.mism = 0;
        }
        if (!mine || !g_on) continue;
        // I am the authority: publish when anything changed, and as a heartbeat
        uint32_t w[kBossMaxFields]; bool changed = !b.lw_valid;
        for (int k = 0; k < d.nf; ++k) { w[k] = read_field(b.a, d.f[k]); if (b.lw_valid && w[k] != b.last_w[k] && !(d.f[k].kind & kVol)) changed = true; }
        if (changed || (unsigned)(now - b.pub_t) >= (unsigned)kBossHeartbeatMs) {
            if (bytes + rec_bytes(d) <= kPayloadMax - (int)sizeof(BossHead) && nrec < kBossMaxRecs) {
                bytes += fill_rec(recs + bytes, slot, b, claiming ? 2 : 0); ++nrec;
                std::memcpy(b.last_w, w, sizeof w); b.lw_valid = true; b.pub_t = now;
            }
        }
    }
    if (nrec > 0 && take_token(now)) send_batch(kWorldBossState, 0, recs, nrec, bytes);
}

// SM64DS_COOP_BOSS_HIT=F[,F]: from boss tick F, on THIS console, hit the first boss the ROM's own way. Where the class has a
// hit flag (Whomp King) the flag is set every tick until the boss's own handler has taken it (it only does in its vulnerable
// state), so the HP drop is the game's; where it has none the HP is lowered directly (the sync path is what is under test).
bool bossdesc1_knob(Boss &b, HitKnob &k);
void do_hit_knob(HitKnob &k) {
    if (!k.armed) {
        for (int slot = 0; slot < kSlots; ++slot) {
            Boss &b = g_boss[slot];
            if (!b.a || hp_of(b) <= 0) continue;
            if (k.lethal && b.d->hit_off && hp_of(b) > 1) { write_field(b.a, *b.d, b.d->hp, 1u); b.synced_hp = 1; }   // the next hit is the last one, the ROM's own way
            k.armed = true; k.slot = slot; k.hp0 = hp_of(b);
            std::fprintf(stderr, "[cbs-knob] hit armed slot=%d cls=%u hp=%d w=%lld\n", slot, (unsigned)b.d->cls, k.hp0, wall_ms());
            break;
        }
        if (!k.armed) return;
    }
    Boss &b = g_boss[k.slot];
    if (!b.a) { k.done = true; return; }
    if (hp_of(b) < k.hp0) {
        k.done = true;
        std::fprintf(stderr, "[cbs-knob] hit taken slot=%d hp %d->%d st=%u w=%lld\n", k.slot, k.hp0, hp_of(b), state_of(b), wall_ms());
        return;
    }
    if (b.d->hit_off) {
        if (b.d->hit_who && data_0209f394[data_0209f250 & 15]) { void *me = data_0209f394[data_0209f250 & 15]; std::memcpy(b.a + b.d->hit_who, &me, sizeof me); }
        b.a[b.d->hit_off] = 1;
    } else if (b.d->cls == 189) {
        // King Bob-omb takes a hit by entering state 02126ffc: its enter code (func_ov078_021243c0) is the health point.
        if (k.lethal && hp_of(b) > 1) { write_field(b.a, *b.d, b.d->hp, 1u); k.hp0 = 1; }
        give_player_ptrs(b);          // the hit state reads the player it was hit by (+0x430)
        KingBobOmb_SetState(b.a, kbo_id_to_ptr(0));
    } else if (bossdesc1_knob(b, k)) {
    } else { write_field(b.a, *b.d, b.d->hp, (uint32_t)(k.hp0 - 1)); }
}

// bossdesc1: the proof's "a hit from this console" for the four families, each the ROM's own way as far as it can be made without
// a player pushing: Chilly goes through the knock-down code of func_ov073_02120c7c (one fewer, then the hit or the defeat state);
// Big Bully falls into the lava (the dying state with the death point that func_ov064_02116220 sets); Big Boo enters its hit state
// with the timer at 0 so the ROM's own handler takes the health point; Wiggler enters the hit state (whose enter half takes it) or,
// on the last point, the death state. Returns true when the class is one of the four.
bool bossdesc1_knob(Boss &b, HitKnob &k) {
    char *a = b.a;
    const unsigned cls = b.d->cls;
    if (cls != 218 && cls != 216 && cls != 210 && cls != 248) return false;
    give_player_ptrs(b);
    if (cls == 218) {
        uint8_t hp = (uint8_t)a[0x4cb];
        if (k.lethal && hp > 1) { a[0x4cb] = 1; hp = 1; }
        a[0x4cb] = (char)(hp - 1);
        ChiefChilly_ChangeState(a, chilly_id_to_ptr(hp - 1 ? 10u : 11u));
    } else if (cls == 216) {
        int32_t x = rd32(a, 0x5c), y = rd32(a, 0x60) + 0x5000, z = rd32(a, 0x64);
        std::memcpy(a + 0x3a8, &x, 4); std::memcpy(a + 0x3ac, &y, 4); std::memcpy(a + 0x3b0, &z, 4);
        bully_enter(a, 4u);
    } else if (cls == 210) {
        if (k.lethal && (uint8_t)a[0x5ca] > 1) a[0x5ca] = 1;
        a[0x5cc] = 3; const uint16_t zero = 0; std::memcpy(a + 0x100, &zero, 2);
    } else {
        if (k.lethal && (uint8_t)a[0x8db] > 1) a[0x8db] = 1;
        func_ov034_021125b8(a, (uint8_t)a[0x8db] > 1 ? 3 : 7);
    }
    k.done = true;
    std::fprintf(stderr, "[cbs-knob] hit made slot=%d cls=%u hp=%u st=%u w=%lld\n", k.slot, cls, (unsigned)hp_of(b), state_of(b), wall_ms());
    return true;
}

void do_forge(const char *name) {
    int slot = -1;
    for (int i = 0; i < kSlots; ++i) if (g_boss[i].a) { slot = i; break; }
    if (slot < 0) { std::fprintf(stderr, "[cbs-knob] forge %s: no boss\n", name); return; }
    const Boss &b = g_boss[slot];
    unsigned char buf[kPayloadMax]; std::memset(buf, 0, sizeof buf);
    BossHead h; std::memset(&h, 0, sizeof h); h.key = wire_key(); h.count = 1;
    unsigned char rec[64]; int rn = fill_rec(rec, slot, b, 0);
    BossRec r; std::memcpy(&r, rec, sizeof r);
    int sub = kWorldBossState;
    int len;
    if (!std::strcmp(name, "short")) { len = (int)sizeof h + rn - 5; }
    else if (!std::strcmp(name, "long")) { len = (int)sizeof h + rn + 3; }
    else {
        if (!std::strcmp(name, "count")) h.count = 9;
        else if (!std::strcmp(name, "cls")) r.cls = 200;
        else if (!std::strcmp(name, "slot")) r.slot = 600;
        else if (!std::strcmp(name, "nf")) r.nf = (uint8_t)(r.nf + 1);
        else if (!std::strcmp(name, "key")) h.key.act = (uint8_t)((g_key.act + 1) & 7);
        else if (!std::strcmp(name, "flags")) h.flags = 1;
        else if (!std::strcmp(name, "rflags")) r.rflags = 0x80;
        else if (!std::strcmp(name, "hp")) { const uint32_t v = 200; std::memcpy(rec + sizeof r + 4 * b.d->hp, &v, 4); }
        else if (!std::strcmp(name, "state")) { const uint32_t v = 99; std::memcpy(rec + sizeof r + 4 * b.d->state, &v, 4); }
        else if (!std::strcmp(name, "hit")) { sub = kWorldBossHit; h.flags = 1; }
        else if (!std::strcmp(name, "notboss")) { r.cls = 164; }
        else return;
        std::memcpy(rec, &r, sizeof r);
        len = (int)sizeof h + rn;
    }
    std::fprintf(stderr, "[cbs-knob] forge %s\n", name);
    std::memcpy(buf, &h, sizeof h); std::memcpy(buf + sizeof h, rec, (size_t)rn);
    if (len < 1) len = 1;
    coop_send_world(sub, buf, len);
}

// ---------------------------------------------------------------------------
// ONE STAR FOR ALL. Every console runs the boss's defeat code and each would spawn the star (dActor_c::UntrackAndSpawnStar,
// src/_ZN8dActor_c19UntrackAndSpawnStarERajRK7Vector3h.cpp: UntrackStar, then Spawn(0xb2, ...)); COOPSPAWN1 announces each one and
// the others spawn it too, so a console ended with two. The replacement keeps the ROM's two lines for everything except a boss
// whose authority is another console: that copy retires its star marker and spawns nothing, the authority's star arrives
// through COOPSPAWN1's announcement.
// ---------------------------------------------------------------------------
bool boss_class_for_star(unsigned cls) {
    if (desc_of(cls)) return true;
    return cls == 176 || cls == 198 || cls == 210 || cls == 216 || cls == 218 || cls == 248 || cls == 279;
}

bool star_is_mine(dActor_c *self) {
    if (!g_on || !coop_world_active() || !coop_group_established()) return true;
    const unsigned cls = *(const unsigned short *)((const char *)self + 0x0c);
    if (!boss_class_for_star(cls)) return true;
    const short slot = rd16((const char *)self, 0xce);
    if (slot < 0 || slot >= kSlots) return true;
    if (g_my < 0) g_my = comms_hub_my_seat();
    const int auth = desc_of(cls) ? auth_for(slot, comms_hub_now_ms()) : coop_target_seat_of_slot(slot);
    if (auth < 0) return true;
    return auth == g_my;
}

dActor_c *__fastcall star_replacement(dActor_c *self, void * /*edx*/, s8 *trackStarID, u32 starID, const Vector3 *spawnPos, u8 how) {
    self->UntrackStar(*trackStarID);
    if (!star_is_mine(self)) {
        ++g_st.stars_suppressed;
        if (g_log) std::fprintf(stderr, "[cbs] star suppressed: this console is not the authority of cls=%u (the authority's star comes by announcement) w=%lld\n",
                                (unsigned)*(const unsigned short *)((const char *)self + 0x0c), wall_ms());
        return nullptr;
    }
    ++g_st.stars_spawned;
    return self->Spawn(0xb2, starID | ((u32)how << 4), *spawnPos, 0, ((const s8 *)self)[0xcc], -1);
}

bool g_star_patched = false;
void install_star_patch() {
    if (g_star_patched) return;
    g_star_patched = true;
    dActor_c *(dActor_c::*pmf)(s8 &, u32, const Vector3 &, u8) = &dActor_c::UntrackAndSpawnStar;
    void *from = nullptr; std::memcpy(&from, &pmf, sizeof from);
    DWORD old = 0;
    if (!VirtualProtect(from, 8, PAGE_EXECUTE_READWRITE, &old)) { std::fprintf(stderr, "[cbs] cannot patch UntrackAndSpawnStar\n"); return; }
    unsigned char b[5]; b[0] = 0xE9;
    const int rel = (int)((unsigned char *)&star_replacement - (unsigned char *)from - 5);
    std::memcpy(b + 1, &rel, 4);
    std::memcpy(from, b, 5);
    VirtualProtect(from, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), from, 8);
    std::fprintf(stderr, "[cbs] UntrackAndSpawnStar replaced at %p\n", from);
}

void on_frame(unsigned now) {
    read_env();
    if (!coop_world_active()) return;
    if (!g_regd) { g_regd = true; g_my = comms_hub_my_seat(); }
    install_star_patch();
    int lv = -1, ac = 0;
    if (!coop_group_established() || !coop_group_key(&lv, &ac)) return;
    const CoopKey nk(lv, ac);
    if (nk != g_key) {
        for (int i = 0; i < kSlots; ++i) g_boss[i] = Boss();
        for (int s = 0; s < kSeats; ++s) g_claim_until[s] = 0;
        g_key = nk;
    }
    ++g_ticks; ++g_st.ticks;
    scan();
    g_st.bosses = (unsigned long long)g_nb;
    if (g_god) { const int me = (int)data_0209f250 & 15; data_02092144[me] = (short)((data_02092144[me] & 0xff) | (8 << 8)); }
    if (!g_stand_done && g_ticks >= g_stand_tick && g_nb > 0) {
        for (int slot = 0; slot < kSlots; ++slot) {
            Boss &b = g_boss[slot];
            const int me = (int)data_0209f250;
            if (!b.a || me < 0 || me >= 16 || !data_0209f394[me]) continue;
            char *pl = (char *)data_0209f394[me];
            int32_t x = rd32(b.a, 0x5c) + (g_stand_dist << 12), y = rd32(b.a, 0x60) + 0x10000, z = rd32(b.a, 0x64);
            std::memcpy(pl + 0x5c, &x, 4); std::memcpy(pl + 0x60, &y, 4); std::memcpy(pl + 0x64, &z, 4);
            const int32_t zero = 0; std::memcpy(pl + 0x98, &zero, 4);
            if (g_stand_every) g_stand_tick = g_ticks + g_stand_every; else g_stand_done = true;
            std::fprintf(stderr, "[cbs-knob] stand local player %d units from boss slot=%d cls=%u at %d,%d,%d w=%lld\n", g_stand_dist, slot, (unsigned)b.d->cls, x >> 12, y >> 12, z >> 12, wall_ms());
            break;
        }
    }
    if (g_nb > 0) {
        process(now);
        for (int i = 0; i < g_nhit; ++i)
            if (!g_hit[i].done && g_ticks >= g_hit[i].tick) do_hit_knob(g_hit[i]);
        for (int i = 0; i < g_nforge; ++i)
            if (!g_forge[i].done && g_ticks >= g_forge[i].tick) { g_forge[i].done = true; do_forge(g_forge[i].name); }
    }
    if (g_log && g_nb > 0 && (unsigned)(now - g_stat_t) >= 4000u) {
        g_stat_t = now;
        const CoopBossStats &s = g_st;
        std::fprintf(stderr, "[cbsstat] seat=%d bosses=%llu sent=%llu recs=%llu hits_sent=%llu rx=%llu rrx=%llu hits_rx=%llu app st%llu hp%llu mism%llu hit_local=%llu "
                     "hit_adopted=%llu drop bad%llu grp%llu mem%llu auth%llu cls%llu off%llu seat%llu\n",
                     g_my, s.bosses, s.sent, s.recs_sent, s.hits_sent, s.rx, s.recs_rx, s.hits_rx, s.applied_state, s.applied_hp, s.state_mismatch,
                     s.hit_local, s.hit_adopted, s.drop_bad, s.drop_group, s.drop_member, s.drop_notauth, s.drop_cls, s.drop_off, s.drop_seat);
    }
}

struct Registrar {
    Registrar() { coop_on_frame(on_frame); }
} g_registrar;

}  // namespace

// ---------------------------------------------------------------------------
// Public.
// ---------------------------------------------------------------------------
CoopBossStats coop_boss_stats() { return g_st; }

bool coop_boss_validate(const void *payload, int len) {
    if (!payload || len < (int)sizeof(BossHead) + (int)sizeof(BossRec)) return false;
    const unsigned char *p = static_cast<const unsigned char *>(payload);
    BossHead h; std::memcpy(&h, p, sizeof h);
    if (!coop_key_ok(h.key) || h.flags > 1 || h.reserved != 0) return false;
    if (h.count < 1 || h.count > kBossMaxRecs) return false;
    int off = (int)sizeof h;
    for (int i = 0; i < (int)h.count; ++i) {
        if (off + (int)sizeof(BossRec) > len) return false;
        BossRec r; std::memcpy(&r, p + off, sizeof r);
        if (r.slot >= kSlots || (r.rflags & ~0x03) != 0) return false;
        const BDesc *d = desc_of(r.cls);
        if (!d || r.nf != d->nf) return false;
        off += (int)sizeof r + 4 * r.nf;
        if (off > len) return false;
    }
    return off == len;
}

void coop_boss_on_world(int seat, int sub, const unsigned char *p, int len) {
    read_env();
    if (!g_on) { ++g_st.drop_off; return; }
    if (seat < 0 || seat >= kSeats || seat == g_my) { ++g_st.drop_seat; return; }
    if (sub != kWorldBossState && sub != kWorldBossHit) { ++g_st.drop_bad; return; }
    if (!coop_boss_validate(p, len)) { ++g_st.drop_bad; return; }
    BossHead h; std::memcpy(&h, p, sizeof h);
    if ((sub == kWorldBossHit) != (h.flags == 1) || (sub == kWorldBossHit && h.count != 1)) { ++g_st.drop_bad; return; }
    ++g_st.rx;
    if (!g_key.valid() || h.key.level != (uint16_t)g_key.level || h.key.act != (uint8_t)g_key.act) { ++g_st.drop_group; return; }
    if (!((coop_group_member_mask() >> seat) & 1u)) { ++g_st.drop_member; return; }
    const unsigned now = comms_hub_now_ms();
    int off = (int)sizeof h;
    for (int i = 0; i < (int)h.count; ++i) {
        BossRec r; std::memcpy(&r, p + off, sizeof r);
        const BDesc *d = desc_of(r.cls);
        uint32_t w[kBossMaxFields]; std::memset(w, 0, sizeof w);
        std::memcpy(w, p + off + sizeof r, 4u * r.nf);
        off += (int)sizeof r + 4 * r.nf;
        Boss &b = g_boss[r.slot];
        if (!b.a || b.d != d) { ++g_st.drop_cls; continue; }
        bool ok = true;
        for (int k = 0; k < d->nf; ++k) if (!value_ok(*d, k, w[k])) ok = false;
        if (!ok) { ++g_st.drop_bad; continue; }
        const int auth = auth_for(r.slot, now);
        if (sub == kWorldBossHit) {
            ++g_st.hits_rx;
            // the hitter's copy was hit: take its HP at once everywhere; the authority takes its state whole
            const int lhp = (int)read_field(b.a, d->f[d->hp]);
            if ((int)w[d->hp] < lhp) {
                write_field(b.a, *d, d->hp, w[d->hp]);
                b.synced_hp = (int)w[d->hp];
                ++g_st.applied_hp;
                if (g_log) std::fprintf(stderr, "[cbs] hit-rx slot=%d hp %d->%u from seat %d (auth %d) w=%lld\n", (int)r.slot, lhp, w[d->hp], seat, auth, wall_ms());
            }
            if (auth == g_my) {
                const int hp_keep = (int)read_field(b.a, d->f[d->hp]);
                give_player_ptrs(b);
                for (int k = 0; k < d->nf; ++k) if (k != d->hp) write_field(b.a, *d, k, w[k]);
                write_field(b.a, *d, d->hp, (uint32_t)hp_keep);
                post_force(b);
                b.synced_hp = hp_keep;
                ++g_st.hit_adopted;
                b.lw_valid = false;
            }
            continue;
        }
        if (r.rflags & 2) {                      // a claim: that seat's copy is carried by its player
            g_claim_until[seat] = now + 3000u; ++g_st.claims;
        } else if (seat != auth) {
            // a late record from the authority before this one, within two seconds, still counts
            if (!(b.last_auth == seat && (unsigned)(now - b.auth_t) < 2000u)) { ++g_st.drop_notauth; continue; }
        }
        b.rx.valid = true; b.rx.fresh = true; b.rx.t = now; b.rx.seq = r.seq; b.rx.rflags = r.rflags; b.rx.from = seat;
        std::memcpy(b.rx.w, w, sizeof w);
    }
}

}  // namespace port
