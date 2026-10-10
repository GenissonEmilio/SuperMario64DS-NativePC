// COOP SWITCHES AND LEVEL GLOBALS (phase 2). Run mp1, lane COOPSW1. See coop_switch.h.
//
// Everything here hangs off the coop world core's callbacks (coop_world.h):
// a frame hook, the level-latched hook, the established hook and the owner's
// snapshot-request hook. No game file is edited; the ROM's own entries are
// called (the switch's press, the chest's SetState) or its own fields written.
//
// PORT_HOST_ABI. Actor offsets are the ROM's (32-bit host): class +0x0c,
// placed-object slot +0x0ce, dead flags +0x0f / +0x0e.

#include "coop_switch.h"
#include "coop_world.h"
#include "coop_wire.h"
#include "comms_hub.h"
#include "hub_presence.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250;
extern int data_0209f34c;
extern unsigned char data_0209f2c0[];
extern unsigned char data_0209caa0[];
void *_ZN8dActor_c4NextEPKS_(const void *prev);
void _ZN10StarSwitch15OnGroundPoundedER8dActor_c(char *self);
}

namespace port {
namespace {

long long wall_ms() {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    return (long long)((((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000ull);
}

enum : unsigned {
    kResendMs      = 100,      // each change goes out 3 times: now, +100, +200
    kWordOwnerMs   = 1000,
    kWordGuestMs   = 3000,
    kSettleFrames  = 20,       // frames after the objects exist before the word is trusted as ours
    kGlobalsWait   = 180,      // a joiner stops waiting for the owner's globals after this many frames
    kStatesAge     = 900,      // pending object states are dropped after this many frames
    kJobMs         = 150,
};
enum : int { kPendMax = 64, kObjOutMax = 48 };

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------
bool g_env = false, g_log = false;
struct Off { bool word, press, kick, field, globals; } g_off = {};
CoopSwitchStats g_st = {};
bool g_registered = false, g_hooked = false;
int g_my = -1;

// the current group
bool     g_have_key = false;
int      g_lvl = -1, g_act = 0;
unsigned g_gen = 0;             // bumped at every level latch
unsigned g_frames = 0;          // frames since the group was established and the player existed
bool     g_new_level = true;
bool     g_armed = false;
bool     g_have_globals = false;
unsigned g_prev_word = 0;       // what the word was when I last looked
unsigned g_shadow = 0;          // the word I have announced or applied
unsigned g_counter = 0;         // my Lamport counter
SwBitModel g_bit[kSwBits];
unsigned g_word_t = 0;

struct PendBit { bool on; uint8_t val; unsigned stamp; int sent; unsigned next; };
PendBit g_pb[kSwBits];

// object state: trackers by slot, outgoing messages, pending states from the owner
struct Trk { void *actor; unsigned short cls; int last; bool valid; };
Trk g_trk[kSwSlots];
struct ObjOut { bool on; uint8_t kind; uint16_t slot, cls; uint8_t state; int32_t value; uint16_t seq; int sent; unsigned next; };
ObjOut g_oo[kObjOutMax];
uint16_t g_seq = 0;
uint16_t g_lastseq[kCoopMaxSeats][kSwSlots];
struct Pend { bool on; uint8_t kind; uint16_t slot, cls; uint8_t state; int32_t value; unsigned born; };
Pend g_pend[kPendMax];

// the owner's answers to snapshot requests
struct Job { bool on; int seat; uint32_t nonce; int sent; unsigned next; unsigned born; };
Job g_job[kCoopMaxSeats];
unsigned g_states_t = 0;

// an early GLOBALS / STATES from a sender not yet a member
struct Early { bool on; int seat; int kind; int lvl, act; unsigned char buf[sizeof(SwStatesHead) + kSwMaxStateEnt * sizeof(SwStateEntry)]; int len; };
Early g_early[2];

// proof knobs
struct Knob { unsigned frame; int bit; bool set; bool done; };
Knob g_knob[8]; int g_nknob = 0;
struct Forge { unsigned frame; char name[12]; bool done; };
Forge g_forge[8]; int g_nforge = 0;
struct PressKnob { unsigned frame; int cls; int nth; bool done; };
PressKnob g_press[4]; int g_npress = 0;
struct WatchK { int cls, off, size; };
WatchK g_watch = {-1, 0, 0};
struct WatchTrk { void *a; unsigned v; bool valid; };
WatchTrk g_wtrk[kSwSlots];
unsigned g_rec[3] = {0, 0, 0}; bool g_rec_on = false; int g_rec_n = 0;

// ---------------------------------------------------------------------------
// Env.
// ---------------------------------------------------------------------------
void read_env() {
    if (g_env) return;
    g_env = true;
    const char *l = std::getenv("SM64DS_COOP_LOG");
    g_log = l && *l && *l != '0';
    if (const char *o = std::getenv("SM64DS_SW_OFF")) {
        g_off.word = std::strstr(o, "word") != nullptr;
        g_off.press = std::strstr(o, "press") != nullptr;
        g_off.kick = std::strstr(o, "kick") != nullptr;
        g_off.field = std::strstr(o, "field") != nullptr;
        g_off.globals = std::strstr(o, "globals") != nullptr;
    }
    for (int pass = 0; pass < 2; ++pass) {
        const char *e = std::getenv(pass ? "SM64DS_SW_CLEAR" : "SM64DS_SW_SET");
        if (!e) continue;
        const char *p = e;
        while (*p && g_nknob < 8) {
            unsigned f = 0; int b = 0, used = 0;
            if (std::sscanf(p, "%u:%i%n", &f, &b, &used) < 2 || used <= 0) break;
            g_knob[g_nknob++] = {f, b, pass == 0, false};
            p += used;
            while (*p == ';' || *p == ',' || *p == ' ') ++p;
        }
    }
    if (const char *f = std::getenv("SM64DS_SW_FORGE")) {
        const char *p = f;
        while (*p && g_nforge < 8) {
            unsigned fr = 0; char nm[12] = {}; int used = 0;
            if (std::sscanf(p, "%u:%11[a-z0-9]%n", &fr, nm, &used) < 2 || used <= 0) break;
            g_forge[g_nforge].frame = fr; std::memcpy(g_forge[g_nforge].name, nm, sizeof nm);
            g_forge[g_nforge].done = false; ++g_nforge;
            p += used;
            while (*p == ',' || *p == ';' || *p == ' ') ++p;
        }
    }
    if (const char *pk = std::getenv("SM64DS_SW_PRESS")) {
        // F:cls[:nth][;F:cls[:nth]]  press the nth (lowest slot first) live actor of the class at group frame F
        const char *p = pk;
        while (*p && g_npress < 4) {
            unsigned f = 0; int c = 0, n = 0, used = 0;
            if (std::sscanf(p, "%u:%i:%i%n", &f, &c, &n, &used) < 3 || used <= 0) {
                used = 0;
                if (std::sscanf(p, "%u:%i%n", &f, &c, &used) < 2 || used <= 0) break;
            }
            g_press[g_npress++] = {f, c, n, false};
            p += used;
            while (*p == ';' || *p == ',' || *p == ' ') ++p;
        }
    }
    if (const char *w = std::getenv("SM64DS_SW_WATCH")) {
        int c = 0, o = 0, s = 0;
        if (std::sscanf(w, "%i:%i:%i", &c, &o, &s) == 3 && (s == 1 || s == 2 || s == 4) && o >= 0 && o < 0x800)
            g_watch = {c, o, s};
    }
    if (const char *r = std::getenv("SM64DS_COOP_REC")) {
        unsigned a = 0, b = 0, c = 0;
        if (std::sscanf(r, "%u:%u:%u", &a, &b, &c) == 3 && a < b && b < c) { g_rec[0] = a; g_rec[1] = b; g_rec[2] = c; g_rec_on = true; }
    }
}

// ---------------------------------------------------------------------------
// Actors.
// ---------------------------------------------------------------------------
inline unsigned short a_cls(const char *a) { return *(const unsigned short *)(a + 0x0c); }
inline short a_slot(const char *a) { return *(const short *)(a + 0xce); }
inline bool a_dead(const char *a) { return *(const unsigned char *)(a + 0x0f) || *(const unsigned char *)(a + 0x0e) == 2; }
bool is_player_ptr(const void *a) {
    for (int i = 0; i < 16; ++i) if (data_0209f394[i] == a) return true;
    return false;
}

// ---- the watch table: how to read a state and how to put one in ------------
// kind press: announce 0 -> non-zero, replay = the class's own press entry
// kind kick:  announce 0 -> 1, replay = the class's own trigger
// kind field: announce every change, replay = write the field
struct Watch {
    unsigned short cls; int kind; const char *name;
    int (*read)(const char *a);
    bool (*apply)(char *a, int state, int value);   // false = refused (not idle)
};
int rd_i32(const char *a, int off) { return *(const int *)(a + off); }
int rd_u8(const char *a, int off) { return *(const unsigned char *)(a + off); }
int r_starsw(const char *a) { return rd_i32(a, 0x340); }
bool ap_starsw(char *a, int, int) {
    if (rd_i32(a, 0x340) != 0) return false;     // the ROM's own early return makes a repeat a no-op
    _ZN10StarSwitch15OnGroundPoundedER8dActor_c(a);
    return true;
}
int r_bcsw(const char *a) { return rd_u8(a, 0x32c); }
bool ap_bcsw(char *a, int, int) {
    if (rd_u8(a, 0x32c) != 0) return false;
    *(unsigned char *)(a + 0x32c) = 1;            // what the switch's collision callback stores
    return true;
}
int r_donut(const char *a) { return rd_u8(a, 0x4ea); }
bool ap_donut(char *a, int state, int) {
    if (rd_u8(a, 0x4ea) != 0) return false;       // idle only
    *(unsigned char *)(a + 0x4ea) = (unsigned char)state;
    return true;
}
// daObjFallBlock_c family (WF 45, BBH 66, LLL 83): state at +0x33c, idle 0; the shake starts from
// case 0 of its own Behavior (ready off, timer 4, state 1), which is what the replay does.
int r_fall(const char *a) { return rd_u8(a, 0x33c); }
bool ap_fall(char *a, int, int) {
    if (rd_u8(a, 0x33c) != 0) return false;
    *(unsigned char *)(a + 0x340) = 0;
    *(unsigned short *)(a + 0x33a) = 4;
    *(unsigned char *)(a + 0x33c) = 1;
    return true;
}
// CANNON_HATCH (14): +0x32c mOpening starts the slide (what func_ov002_020bc990 stores after the sound;
// the cannon-unlocked save bit itself travels with the save record).
int r_hatch(const char *a) { return rd_u8(a, 0x32c); }
bool ap_hatch(char *a, int, int) {
    if (rd_u8(a, 0x32c) != 0) return false;
    *(unsigned char *)(a + 0x32c) = 1;
    return true;
}
// The big "?" switch (HATENA_SWITCH 26): the shared fact is bit 31 of the save record word [1]
// (the ? blocks read it); the press itself is +0x71a with the 8-frame countdown at +0x718.
int r_hatena(const char *) { return (int)((*(const unsigned *)(data_0209caa0 + 4)) >> 31); }
bool ap_hatena(char *a, int, int) {
    if ((*(const unsigned *)(data_0209caa0 + 4)) & 0x80000000u) return false;
    *(unsigned char *)(a + 0x718) = 0;
    *(unsigned char *)(a + 0x71a) = 1;
    return true;
}
int r_water(const char *a) { return rd_i32(a, 0x334); }
bool ap_water(char *a, int, int value) {
    if (rd_i32(a, 0x334) == value) return false;
    *(int *)(a + 0x334) = value;                  // the water moves toward it on its own
    return true;
}
const Watch kWatch[] = {
    {10,  kSwKindPress, "BC_SWITCH",    r_bcsw,   ap_bcsw},
    {11,  kSwKindPress, "EXCL_SWITCH",  r_starsw, ap_starsw},
    {12,  kSwKindPress, "STAR_SWITCH",  r_starsw, ap_starsw},
    {126, kSwKindKick,  "DONUT_BLOCK",  r_donut,  ap_donut},
    {14,  kSwKindKick,  "CANNON_HATCH",   r_hatch, ap_hatch},
    {26,  kSwKindKick,  "HATENA_SWITCH", r_hatena, ap_hatena},
    {45,  kSwKindKick,  "FALL_BLOCK_WF",  r_fall, ap_fall},
    {66,  kSwKindKick,  "FALL_BLOCK_BBH", r_fall, ap_fall},
    {83,  kSwKindKick,  "FALL_BLOCK_LLL", r_fall, ap_fall},
    {101, kSwKindField, "WDW_WATER",    r_water,  ap_water},
};
const Watch *find_watch(int cls) {
    for (const Watch &w : kWatch) if (w.cls == cls) return &w;
    return nullptr;
}

char *find_actor(int slot, int cls) {
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a))
        if (a_cls(a) == cls && a_slot(a) == slot && !a_dead(a) && !is_player_ptr(a)) return a;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Sending.
// ---------------------------------------------------------------------------
CoopKeyWire wkey() { CoopKeyWire k; k.level = (uint16_t)g_lvl; k.act = (uint8_t)g_act; k.reserved = 0; return k; }
bool others_here() { return (coop_group_member_mask() & ~(1u << g_my)) != 0; }

bool send_world(int sub, const void *p, int n) { return coop_send_world(sub, p, n); }
bool send_ctl(int sub, const void *p, int n) { return coop_send_ctl(sub, p, n); }

void send_bits(unsigned now) {
    SwBitEntry ent[kSwMaxBitEnt]; int idx[kSwMaxBitEnt]; int n = 0;
    for (int b = 0; b < kSwBits && n < kSwMaxBitEnt; ++b) {
        PendBit &p = g_pb[b];
        if (!p.on || (int)(now - p.next) < 0) continue;
        ent[n] = {(uint8_t)b, p.val, (uint16_t)p.stamp}; idx[n++] = b;
    }
    if (!n) return;
    unsigned char buf[sizeof(SwBitHead) + kSwMaxBitEnt * sizeof(SwBitEntry)];
    SwBitHead h; std::memset(&h, 0, sizeof h); h.key = wkey(); h.count = (uint8_t)n;
    std::memcpy(buf, &h, sizeof h); std::memcpy(buf + sizeof h, ent, (size_t)n * sizeof(SwBitEntry));
    const bool ok = others_here() && send_world(kSwWorldBit, buf, (int)sizeof h + n * (int)sizeof(SwBitEntry));
    if (ok) ++g_st.sent_bit;
    for (int i = 0; i < n; ++i) {
        PendBit &p = g_pb[idx[i]];
        if (!ok && others_here()) continue;
        if (++p.sent >= 3 || !others_here()) p.on = false; else p.next = now + kResendMs;
    }
}

void send_word(unsigned now) {
    SwWordWire w; std::memset(&w, 0, sizeof w);
    w.key = wkey(); w.word = (unsigned)data_0209f34c;
    bool any = false;
    for (int i = 0; i < kSwBits; ++i) { w.stamp[i] = (uint16_t)g_bit[i].stamp; any = any || g_bit[i].stamp; }
    g_word_t = now;
    if (!any || !others_here()) return;
    if (send_world(kSwWorldWord, &w, sizeof w)) ++g_st.sent_word;
}

void queue_obj(int kind, int slot, int cls, int state, int value, unsigned now) {
    ++g_seq; if (!g_seq) g_seq = 1;
    int free_i = -1;
    for (int i = 0; i < kObjOutMax; ++i) {      // a newer message for the same object replaces the old one
        if (g_oo[i].on && g_oo[i].kind == kind && g_oo[i].slot == slot && g_oo[i].cls == cls) { free_i = i; break; }
        if (!g_oo[i].on && free_i < 0) free_i = i;
    }
    if (free_i < 0) free_i = 0;
    g_oo[free_i] = {true, (uint8_t)kind, (uint16_t)slot, (uint16_t)cls, (uint8_t)state, (int32_t)value, g_seq, 0, now};
}

void send_objs(unsigned now) {
    for (int i = 0; i < kObjOutMax; ++i) {
        ObjOut &o = g_oo[i];
        if (!o.on || (int)(now - o.next) < 0) continue;
        if (!others_here()) { o.on = false; continue; }
        SwObjWire w; std::memset(&w, 0, sizeof w);
        w.key = wkey(); w.slot = o.slot; w.cls = o.cls; w.state = o.state; w.seq = o.seq; w.value = o.value;
        const int sub = o.kind == kSwKindPress ? kSwWorldPress : o.kind == kSwKindKick ? kSwWorldKick : kSwWorldField;
        if (send_world(sub, &w, sizeof w)) {
            if (o.kind == kSwKindPress) ++g_st.sent_press; else if (o.kind == kSwKindKick) ++g_st.sent_kick; else ++g_st.sent_field;
            if (g_log && o.sent == 0)
                std::fprintf(stderr, "[coopsw] send %s slot=%d cls=%d state=%d value=%d seq=%u wall=%lld\n",
                             o.kind == kSwKindPress ? "press" : o.kind == kSwKindKick ? "kick" : "field",
                             (int)o.slot, (int)o.cls, (int)o.state, (int)o.value, (unsigned)o.seq, wall_ms());
        }
        if (++o.sent >= 3) o.on = false; else o.next = now + kResendMs;
    }
}

// What the owner tells a joiner: the globals and every non-idle watched object.
int collect_states(SwStateEntry *out, int cap) {
    int n = 0;
    for (int s = 0; s < kSwSlots && n < cap; ++s) {
        const Trk &t = g_trk[s];
        if (!t.valid) continue;
        const Watch *w = find_watch(t.cls);
        if (!w || !t.actor) continue;
        if (w->kind == kSwKindField) { out[n++] = {(uint16_t)s, t.cls, (uint8_t)kSwKindField, 0, t.last}; continue; }
        if (t.last == 0) continue;
        out[n++] = {(uint16_t)s, t.cls, (uint8_t)w->kind, (uint8_t)(w->kind == kSwKindPress ? 1 : (t.last > 3 ? 3 : t.last)), 0};
    }
    return n;
}

void serve_job(Job &j) {
    SwGlobalsWire g; std::memset(&g, 0, sizeof g);
    g.key = wkey(); g.nonce = j.nonce; g.target = (uint8_t)j.seat; g.word = (unsigned)data_0209f34c;
    std::memcpy(g.globals, data_0209f2c0, 4);
    for (int i = 0; i < 4; ++i) if (g.globals[i] > 7) g.globals[i] = 0;
    for (int i = 0; i < kSwBits; ++i) g.stamp[i] = (uint16_t)g_bit[i].stamp;
    if (send_ctl(kSwCtlGlobals, &g, sizeof g)) ++g_st.sent_globals;
    unsigned char buf[sizeof(SwStatesHead) + kSwMaxStateEnt * sizeof(SwStateEntry)];
    SwStateEntry ent[kSwMaxStateEnt];
    const int n = collect_states(ent, kSwMaxStateEnt);
    SwStatesHead h; std::memset(&h, 0, sizeof h);
    h.key = wkey(); h.nonce = j.nonce; h.target = (uint8_t)j.seat; h.count = (uint8_t)n;
    std::memcpy(buf, &h, sizeof h); std::memcpy(buf + sizeof h, ent, (size_t)n * sizeof(SwStateEntry));
    if (send_ctl(kSwCtlStates, buf, (int)sizeof h + n * (int)sizeof(SwStateEntry))) ++g_st.sent_states;
    if (g_log && j.sent == 0)
        std::fprintf(stderr, "[coopsw] serve globals to seat %d: word=%08x f2c0=%d,%d,%d,%d states=%d wall=%lld\n",
                     j.seat, (unsigned)data_0209f34c, data_0209f2c0[0], data_0209f2c0[1], data_0209f2c0[2], data_0209f2c0[3], n, wall_ms());
}

// ---------------------------------------------------------------------------
// The word.
// ---------------------------------------------------------------------------
void log_word(const char *why) {
    if (g_log)
        std::fprintf(stderr, "[coopsw] word %s=%08x frame=%u armed=%d wall=%lld\n", why, (unsigned)data_0209f34c, g_frames, (int)g_armed, wall_ms());
}

void set_word_bit(int b, int v) {
    const unsigned m = 1u << b;
    data_0209f34c = (int)((((unsigned)data_0209f34c) & ~m) | (v ? m : 0u));
}

unsigned next_stamp() {
    if (g_counter < 0xFFF) ++g_counter;
    else if (g_log) std::fprintf(stderr, "[coopsw] stamp counter exhausted (4095 changes in this course): later changes may not win\n");
    return sw_stamp_make(g_counter, g_my);
}

// A remote bit change. Highest stamp wins; the stamp's low nibble is the seat, so no ties.
void adopt_bit(int b, int v, unsigned stamp) {
    if (!sw_adopt(g_bit[b], stamp, v)) { if (stamp != g_bit[b].stamp) ++g_st.drop_old; return; }
    if (sw_stamp_counter(stamp) > g_counter) g_counter = sw_stamp_counter(stamp);
    const int cur = (data_0209f34c >> b) & 1;
    if (cur != v) { set_word_bit(b, v); ++g_st.bits_applied; }
    g_shadow = (g_shadow & ~(1u << b)) | (v ? (1u << b) : 0u);
}

void local_word_changes(unsigned now) {
    const unsigned cur = (unsigned)data_0209f34c;
    unsigned d = cur ^ g_shadow;
    if (!d) return;
    for (int b = 0; b < kSwBits; ++b) {
        if (!((d >> b) & 1u)) continue;
        const int v = (cur >> b) & 1;
        const unsigned st = next_stamp();
        g_bit[b].stamp = st; g_bit[b].val = (uint8_t)v;
        g_pb[b] = {true, (uint8_t)v, st, 0, now};
        ++g_st.bits_local;
        if (g_log) std::fprintf(stderr, "[coopsw] local bit %d -> %d stamp=%04x wall=%lld\n", b, v, st, wall_ms());
    }
    g_shadow = cur;
    log_word("local");
}

// During the settle window the word is forced to what the group said, because
// the level's own InitResources (StarSwitch, Key, PowerStar) clear bits as they
// spawn and would otherwise undo the snapshot.
void settle_word() {
    bool changed = false;
    for (int b = 0; b < kSwBits; ++b)
        if (g_bit[b].stamp) {
            const int cur = (data_0209f34c >> b) & 1;
            if (cur != g_bit[b].val) { set_word_bit(b, g_bit[b].val); changed = true; }
        }
    g_shadow = (unsigned)data_0209f34c;
    if (changed) log_word("settle");
}

// ---------------------------------------------------------------------------
// Object states (press / kick / field).
// ---------------------------------------------------------------------------
void apply_to(Trk &t, char *a, const Watch *w, int state, int value) {
    if (w->apply(a, state, value)) {
        t.last = w->read(a);
        if (w->kind == kSwKindPress) ++g_st.press_applied; else if (w->kind == kSwKindKick) ++g_st.kick_applied; else ++g_st.field_applied;
        if (g_log)
            std::fprintf(stderr, "[coopsw] apply %s slot=%d cls=%d state=%d value=%d wall=%lld\n", w->name,
                         (int)a_slot(a), (int)a_cls(a), state, value, wall_ms());
    }
}

// Returns true when the actor was found (the entry is done, whatever its state).
bool apply_entry(int kind, int slot, int cls, int state, int value) {
    const Watch *w = find_watch(cls);
    if (!w || w->kind != kind) { ++g_st.drop_class; return true; }
    char *a = find_actor(slot, cls);
    if (!a) return false;
    Trk &t = g_trk[slot];
    if (!t.valid || t.actor != a) { t.actor = a; t.cls = (unsigned short)cls; t.last = w->read(a); t.valid = true; }
    apply_to(t, a, w, state, value);
    return true;
}

void scan_objects(unsigned now) {
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        const short slot = a_slot(a);
        if (slot < 0 || slot >= kSwSlots) continue;
        const unsigned short cls = a_cls(a);
        const Watch *w = find_watch(cls);
        if (!w || a_dead(a) || is_player_ptr(a)) continue;
        const int cur = w->read(a);
        Trk &t = g_trk[slot];
        if (!t.valid || t.actor != a || t.cls != cls) { t.actor = a; t.cls = cls; t.last = cur; t.valid = true; continue; }
        if (cur == t.last) continue;
        const int prev = t.last;
        t.last = cur;
        if (w->kind == kSwKindField) { queue_obj(kSwKindField, slot, cls, 0, cur, now); continue; }
        if (prev == 0 && cur != 0)
            queue_obj(w->kind, slot, cls, w->kind == kSwKindPress ? 1 : (cur > 3 ? 3 : cur), 0, now);
    }
}

void drain_pending(unsigned) {
    for (Pend &p : g_pend) {
        if (!p.on) continue;
        if (g_frames - p.born > kStatesAge) { p.on = false; continue; }
        if (apply_entry(p.kind, p.slot, p.cls, p.state, p.value)) { p.on = false; ++g_st.states_applied; }
    }
}

void put_pending(const SwStateEntry &e) {
    int free_i = -1;
    for (int i = 0; i < kPendMax; ++i) {
        if (g_pend[i].on && g_pend[i].kind == e.kind && g_pend[i].slot == e.slot && g_pend[i].cls == e.cls) { free_i = i; break; }
        if (!g_pend[i].on && free_i < 0) free_i = i;
    }
    if (free_i < 0) return;
    g_pend[free_i] = {true, e.kind, e.slot, e.cls, e.state, e.value, g_frames};
    ++g_st.states_pending;
}

// ---------------------------------------------------------------------------
// Receiving.
// ---------------------------------------------------------------------------
bool live_key_is(const CoopKeyWire &k) { int l = -1, a = 0; return coop_group_key(&l, &a) && (int)k.level == l && (int)k.act == a; }
bool ready_for(const CoopKeyWire &k) { return g_have_key && (int)k.level == g_lvl && (int)k.act == g_act; }
bool is_member(int seat) { return seat >= 0 && seat < kCoopMaxSeats && ((coop_group_member_mask() >> seat) & 1u); }

void apply_globals(const SwGlobalsWire &w) {
    if (g_off.globals) { ++g_st.drop_off; return; }
    std::memcpy(data_0209f2c0, w.globals, 4);
    for (int i = 0; i < kSwBits; ++i) {
        const unsigned s = w.stamp[i];
        if (!s) continue;
        adopt_bit(i, (w.word >> i) & 1u, s);
    }
    const bool first = !g_have_globals;
    g_have_globals = true;
    ++g_st.globals_applied;
    if (first) log_word("globals");
    if (g_log && first)
        std::fprintf(stderr, "[coopsw] globals applied: f2c0=%d,%d,%d,%d wall=%lld\n", data_0209f2c0[0], data_0209f2c0[1], data_0209f2c0[2], data_0209f2c0[3], wall_ms());
}

void apply_states(const unsigned char *p, int len) {
    SwStatesHead h; std::memcpy(&h, p, sizeof h);
    for (int i = 0; i < (int)h.count; ++i) {
        SwStateEntry e; std::memcpy(&e, p + sizeof h + i * sizeof e, sizeof e);
        if (e.kind == kSwKindPress && g_off.press) continue;
        if (e.kind == kSwKindKick && g_off.kick) continue;
        if (e.kind == kSwKindField && g_off.field) continue;
        put_pending(e);
    }
    (void)len;
}

void on_ctl(int seat, int sub, const unsigned char *p, int len, void *) {
    if (!coop_world_active()) return;
    if (sub == kSwCtlGlobals) {
        ++g_st.recv_globals;
        if (!sw_globals_validate(p, len)) { ++g_st.drop_bad; return; }
        SwGlobalsWire w; std::memcpy(&w, p, sizeof w);
        if (seat == g_my || w.target != g_my) return;
        if (!live_key_is(w.key)) { ++g_st.drop_group; return; }
        if (!ready_for(w.key) || !is_member(seat)) {   // still loading, or the sender is not a known member yet: keep the newest
            Early &e = g_early[0]; e.on = true; e.seat = seat; e.kind = sub; e.lvl = w.key.level; e.act = w.key.act; e.len = len; std::memcpy(e.buf, p, (size_t)len);
            return;
        }
        apply_globals(w);
        return;
    }
    if (sub == kSwCtlStates) {
        ++g_st.recv_states;
        if (!sw_states_validate(p, len)) { ++g_st.drop_bad; return; }
        SwStatesHead h; std::memcpy(&h, p, sizeof h);
        if (seat == g_my || h.target != g_my) return;
        if (!live_key_is(h.key)) { ++g_st.drop_group; return; }
        if (!ready_for(h.key) || !is_member(seat)) {
            Early &e = g_early[1]; e.on = true; e.seat = seat; e.kind = sub; e.lvl = h.key.level; e.act = h.key.act; e.len = len; std::memcpy(e.buf, p, (size_t)len);
            return;
        }
        apply_states(p, len);
        return;
    }
    ++g_st.drop_bad;
}

void on_world(int seat, int sub, const unsigned char *p, int len, void *) {
    if (!coop_world_active()) return;
    if (seat == g_my || seat < 0 || seat >= kCoopMaxSeats) return;
    switch (sub) {
    case kSwWorldBit: {
        ++g_st.recv_bit;
        if (!sw_bit_validate(p, len)) { ++g_st.drop_bad; return; }
        SwBitHead h; std::memcpy(&h, p, sizeof h);
        if (!live_key_is(h.key)) { ++g_st.drop_group; return; }
        if (!is_member(seat)) { ++g_st.drop_member; return; }
        if (g_off.word) { ++g_st.drop_off; return; }
        const unsigned before = (unsigned)data_0209f34c;
        for (int i = 0; i < (int)h.count; ++i) {
            SwBitEntry e; std::memcpy(&e, p + sizeof h + i * sizeof e, sizeof e);
            if ((e.stamp & 15u) != (unsigned)seat) { ++g_st.drop_bad; continue; }   // a stamp names its sender
            adopt_bit(e.bit, e.value, e.stamp);
        }
        if ((unsigned)data_0209f34c != before) log_word("remote");
        return;
    }
    case kSwWorldWord: {
        ++g_st.recv_word;
        if (!sw_word_validate(p, len)) { ++g_st.drop_bad; return; }
        SwWordWire w; std::memcpy(&w, p, sizeof w);
        if (!live_key_is(w.key)) { ++g_st.drop_group; return; }
        if (!is_member(seat)) { ++g_st.drop_member; return; }
        if (g_off.word) { ++g_st.drop_off; return; }
        const unsigned before = (unsigned)data_0209f34c;
        for (int i = 0; i < kSwBits; ++i)
            if (w.stamp[i]) adopt_bit(i, (w.word >> i) & 1u, w.stamp[i]);
        if ((unsigned)data_0209f34c != before) log_word("remote-full");
        return;
    }
    case kSwWorldPress: case kSwWorldKick: case kSwWorldField: {
        const int kind = sub == kSwWorldPress ? kSwKindPress : sub == kSwWorldKick ? kSwKindKick : kSwKindField;
        if (kind == kSwKindPress) ++g_st.recv_press; else if (kind == kSwKindKick) ++g_st.recv_kick; else ++g_st.recv_field;
        if (!sw_obj_validate(kind, p, len)) { ++g_st.drop_bad; return; }
        SwObjWire w; std::memcpy(&w, p, sizeof w);
        if (!live_key_is(w.key)) { ++g_st.drop_group; return; }
        if (!is_member(seat)) { ++g_st.drop_member; return; }
        if ((kind == kSwKindPress && g_off.press) || (kind == kSwKindKick && g_off.kick) || (kind == kSwKindField && g_off.field)) { ++g_st.drop_off; return; }
        uint16_t &last = g_lastseq[seat][w.slot];
        if (!sw_seq_newer(w.seq, last)) { ++g_st.drop_old; return; }
        last = w.seq;
        if (g_log)
            std::fprintf(stderr, "[coopsw] recv %s from seat %d slot=%d cls=%d state=%d value=%d wall=%lld\n",
                         kind == kSwKindPress ? "press" : kind == kSwKindKick ? "kick" : "field", seat,
                         (int)w.slot, (int)w.cls, (int)w.state, (int)w.value, wall_ms());
        // The actor may not exist yet (still loading): keep it as a pending state.
        if (!apply_entry(kind, w.slot, w.cls, kind == kSwKindField ? 0 : w.state, w.value)) {
            SwStateEntry e = {w.slot, w.cls, (uint8_t)kind, kind == kSwKindField ? (uint8_t)0 : w.state, w.value};
            put_pending(e);
        }
        return;
    }
    default:
        ++g_st.drop_bad;
    }
}

// ---------------------------------------------------------------------------
// Knobs: recorder, watch, forge, setters.
// ---------------------------------------------------------------------------
struct RecSample {
    unsigned word; unsigned char g[4]; unsigned char save[0x80];
    struct A { void *a; unsigned short cls; short slot; unsigned char b[0x330]; bool ok; };
    A act[600]; int n;
};
RecSample g_rs[3];

bool safe_copy(void *dst, const void *src, size_t n) {
    __try { std::memcpy(dst, src, n); return true; } __except (1) { return false; }
}

void rec_take(int i) {
    RecSample &r = g_rs[i];
    r.word = (unsigned)data_0209f34c;
    std::memcpy(r.g, data_0209f2c0, 4);
    if (!safe_copy(r.save, data_0209caa0, sizeof r.save)) std::memset(r.save, 0, sizeof r.save);
    r.n = 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a && r.n < 600; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        RecSample::A &q = r.act[r.n++];
        q.a = a; q.cls = a_cls(a); q.slot = a_slot(a);
        q.ok = safe_copy(q.b, a + 0xd0, sizeof q.b);
    }
}

void rec_report() {
    const RecSample &a = g_rs[0], &b = g_rs[1], &c = g_rs[2];
    std::fprintf(stderr, "[rec] word %08x %08x -> %08x\n", a.word, b.word, c.word);
    std::fprintf(stderr, "[rec] f2c0 %d,%d,%d,%d -> %d,%d,%d,%d\n", b.g[0], b.g[1], b.g[2], b.g[3], c.g[0], c.g[1], c.g[2], c.g[3]);
    for (int o = 0; o < (int)sizeof b.save; o += 4) {
        unsigned x, y, z; std::memcpy(&x, a.save + o, 4); std::memcpy(&y, b.save + o, 4); std::memcpy(&z, c.save + o, 4);
        if (y != z) std::fprintf(stderr, "[rec] save +0x%02x %08x -> %08x%s\n", o, y, z, x != y ? " (was moving)" : "");
    }
    int nchg = 0;
    for (int i = 0; i < b.n; ++i) {
        const RecSample::A &q = b.act[i];
        const RecSample::A *r = nullptr, *p = nullptr;
        for (int j = 0; j < c.n; ++j) if (c.act[j].a == q.a && c.act[j].cls == q.cls) { r = &c.act[j]; break; }
        for (int j = 0; j < a.n; ++j) if (a.act[j].a == q.a && a.act[j].cls == q.cls) { p = &a.act[j]; break; }
        if (!r) { std::fprintf(stderr, "[rec] actor cls=%d slot=%d GONE\n", (int)q.cls, (int)q.slot); ++nchg; continue; }
        if (!q.ok || !r->ok) continue;
        for (int o = 0; o < (int)sizeof q.b; o += 4) {
            unsigned y, z, x = 0; std::memcpy(&y, q.b + o, 4); std::memcpy(&z, r->b + o, 4);
            if (p && p->ok) std::memcpy(&x, p->b + o, 4);
            if (y != z) {
                const bool moving = p && p->ok && x != y;
                std::fprintf(stderr, "[rec] actor cls=%d slot=%d +0x%03x %08x -> %08x%s\n", (int)q.cls, (int)q.slot, o + 0xd0, y, z, moving ? " (was moving)" : "");
                ++nchg;
            }
        }
    }
    for (int j = 0; j < c.n; ++j) {
        bool had = false;
        for (int i = 0; i < b.n; ++i) if (b.act[i].a == c.act[j].a) { had = true; break; }
        if (!had) std::fprintf(stderr, "[rec] actor cls=%d slot=%d NEW\n", (int)c.act[j].cls, (int)c.act[j].slot);
    }
    std::fprintf(stderr, "[rec] done: %d changed words, %d actors before, %d after\n", nchg, b.n, c.n);
}

void knob_forge(const char *name, unsigned now) {
    std::fprintf(stderr, "[coopsw-knob] forge %s\n", name);
    unsigned char b[128]; std::memset(b, 0, sizeof b);
    if (!std::strcmp(name, "bit32")) {
        SwBitHead h; std::memset(&h, 0, sizeof h); h.key = wkey(); h.count = 1;
        SwBitEntry e = {32, 1, (uint16_t)sw_stamp_make(1, g_my)};
        std::memcpy(b, &h, sizeof h); std::memcpy(b + sizeof h, &e, sizeof e);
        send_world(kSwWorldBit, b, (int)(sizeof h + sizeof e));
    } else if (!std::strcmp(name, "bit5")) {            // a valid bit, but the stamp names another seat
        SwBitHead h; std::memset(&h, 0, sizeof h); h.key = wkey(); h.count = 1;
        SwBitEntry e = {5, 1, (uint16_t)sw_stamp_make(7, (g_my + 1) & 3)};
        std::memcpy(b, &h, sizeof h); std::memcpy(b + sizeof h, &e, sizeof e);
        send_world(kSwWorldBit, b, (int)(sizeof h + sizeof e));
    } else if (!std::strcmp(name, "count")) {           // says 3, one present
        SwBitHead h; std::memset(&h, 0, sizeof h); h.key = wkey(); h.count = 3;
        SwBitEntry e = {1, 1, (uint16_t)sw_stamp_make(1, g_my)};
        std::memcpy(b, &h, sizeof h); std::memcpy(b + sizeof h, &e, sizeof e);
        send_world(kSwWorldBit, b, (int)(sizeof h + sizeof e));
    } else if (!std::strcmp(name, "badkey")) {
        SwBitHead h; std::memset(&h, 0, sizeof h); h.key = wkey(); h.key.act = (uint8_t)((g_act + 1) & 7); h.count = 1;
        SwBitEntry e = {2, 1, (uint16_t)sw_stamp_make(1, g_my)};
        std::memcpy(b, &h, sizeof h); std::memcpy(b + sizeof h, &e, sizeof e);
        send_world(kSwWorldBit, b, (int)(sizeof h + sizeof e));
    } else if (!std::strcmp(name, "slot") || !std::strcmp(name, "class") || !std::strcmp(name, "state") || !std::strcmp(name, "zero")) {
        SwObjWire w; std::memset(&w, 0, sizeof w);
        w.key = wkey(); w.slot = 3; w.cls = 11; w.state = 1; w.seq = 1;
        if (!std::strcmp(name, "slot")) w.slot = 600;
        if (!std::strcmp(name, "class")) w.cls = 207;
        if (!std::strcmp(name, "state")) w.state = 9;
        if (!std::strcmp(name, "zero")) w.seq = 0;
        send_world(kSwWorldPress, &w, sizeof w);
    } else if (!std::strcmp(name, "short")) {
        send_world(kSwWorldPress, b, 9);
    } else if (!std::strcmp(name, "long")) {
        send_world(kSwWorldWord, b, 100);
    } else if (!std::strcmp(name, "water")) {            // a class-101 field value far outside any level
        SwObjWire w; std::memset(&w, 0, sizeof w);
        w.key = wkey(); w.slot = 4; w.cls = 101; w.seq = 1; w.value = 0x7ffffff0;
        send_world(kSwWorldField, &w, sizeof w);
    }
    (void)now;
}

void knob_press(PressKnob &k) {
    const Watch *w = find_watch(k.cls);
    char *c[64]; int n = 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a && n < 64; a = (char *)_ZN8dActor_c4NextEPKS_(a))
        if (a_cls(a) == k.cls && a_slot(a) >= 0 && !a_dead(a) && !is_player_ptr(a)) c[n++] = a;
    std::sort(c, c + n, [](char *x, char *y) { return a_slot(x) < a_slot(y); });
    if (!w || n == 0) { std::fprintf(stderr, "[coopsw-knob] press cls=%d: no live actor\n", k.cls); return; }
    char *a = c[k.nth % n];
    const bool did = w->apply(a, 1, 0);
    std::fprintf(stderr, "[coopsw-knob] press %s slot=%d (candidate %d of %d) did=%d wall=%lld\n", w->name, (int)a_slot(a), k.nth % n, n, (int)did, wall_ms());
}

unsigned g_rec_flags = 0; bool g_rec_flags_init = false;
void watch_record() {
    const unsigned f = *(const unsigned *)(data_0209caa0 + 4);
    if (!g_rec_flags_init) { g_rec_flags_init = true; g_rec_flags = f; return; }
    if (f != g_rec_flags) {
        if (g_log) std::fprintf(stderr, "[coopsw] record flags %08x -> %08x frame=%u wall=%lld\n", g_rec_flags, f, g_frames, wall_ms());
        g_rec_flags = f;
    }
}

void watch_field(unsigned) {
    if (g_watch.cls < 0) return;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        if ((int)a_cls(a) != g_watch.cls) continue;
        const short slot = a_slot(a);
        if (slot < 0 || slot >= kSwSlots) continue;
        unsigned v = 0;
        if (!safe_copy(&v, a + g_watch.off, (size_t)g_watch.size)) continue;
        if (g_watch.size == 1) v &= 0xff; else if (g_watch.size == 2) v &= 0xffff;
        WatchTrk &t = g_wtrk[slot];
        if (!t.valid || t.a != a) { t.a = a; t.v = v; t.valid = true; std::fprintf(stderr, "[coopsw] watch cls=%d slot=%d start=%x frame=%u wall=%lld\n", g_watch.cls, (int)slot, v, g_frames, wall_ms()); continue; }
        if (v != t.v) {
            std::fprintf(stderr, "[coopsw] watch cls=%d slot=%d %x -> %x frame=%u wall=%lld\n", g_watch.cls, (int)slot, t.v, v, g_frames, wall_ms());
            t.v = v;
        }
    }
}

void stats_line() {
    if (!g_log) return;
    const CoopSwitchStats &s = g_st;
    std::fprintf(stderr,
        "[coopswstat] seat=%d word=%08x armed=%d sent bit%llu word%llu press%llu kick%llu field%llu glob%llu states%llu "
        "recv bit%llu word%llu press%llu kick%llu field%llu glob%llu states%llu applied bits=%llu press=%llu kick=%llu field=%llu "
        "glob=%llu states=%llu pend=%llu local=%llu drop bad=%llu group=%llu member=%llu class=%llu old=%llu off=%llu\n",
        g_my, (unsigned)data_0209f34c, (int)g_armed, s.sent_bit, s.sent_word, s.sent_press, s.sent_kick, s.sent_field, s.sent_globals, s.sent_states,
        s.recv_bit, s.recv_word, s.recv_press, s.recv_kick, s.recv_field, s.recv_globals, s.recv_states,
        s.bits_applied, s.press_applied, s.kick_applied, s.field_applied, s.globals_applied, s.states_applied, s.states_pending, s.bits_local,
        s.drop_bad, s.drop_group, s.drop_member, s.drop_class, s.drop_old, s.drop_off);
}

// ---------------------------------------------------------------------------
// Level lifecycle and the frame.
// ---------------------------------------------------------------------------
void new_level() {
    Early keep[2];
    for (int i = 0; i < 2; ++i) { keep[i] = g_early[i]; if (keep[i].on && !(keep[i].lvl == g_lvl && keep[i].act == g_act)) keep[i].on = false; }
    std::memset(g_bit, 0, sizeof g_bit);
    std::memset(g_pb, 0, sizeof g_pb);
    std::memset(g_trk, 0, sizeof g_trk);
    std::memset(g_oo, 0, sizeof g_oo);
    std::memset(g_pend, 0, sizeof g_pend);
    std::memset(g_lastseq, 0, sizeof g_lastseq);
    std::memset(g_job, 0, sizeof g_job);
    std::memset(g_wtrk, 0, sizeof g_wtrk);
    g_counter = 0; g_frames = 0; g_armed = false; g_have_globals = false;
    g_shadow = (unsigned)data_0209f34c;
    g_word_t = 0;
    for (int i = 0; i < g_nknob; ++i) g_knob[i].done = false;
    for (int i = 0; i < g_nforge; ++i) g_forge[i].done = false;
    for (int i = 0; i < g_npress; ++i) g_press[i].done = false;
    g_rec_n = 0;
    g_new_level = false;
    for (int i = 0; i < 2; ++i) g_early[i] = keep[i];
}

void on_latched(int level, int act) {
    if (!coop_world_active()) return;
    read_env();
    g_my = comms_hub_my_seat();
    g_lvl = level; g_act = act; g_have_key = true;
    ++g_gen;
    new_level();
    if (g_log) std::fprintf(stderr, "[coopsw] latched level=%d act=%d word=%08x wall=%lld\n", level, act, (unsigned)data_0209f34c, wall_ms());
}

void on_established(int level, int act) {
    if (!coop_world_active()) return;
    g_lvl = level; g_act = act; g_have_key = true;
}

void on_snapshot_request(int seat, unsigned nonce) {
    if (!coop_world_active() || seat < 0 || seat >= kCoopMaxSeats || seat == g_my) return;
    Job &j = g_job[seat];
    j.on = true; j.seat = seat; j.nonce = nonce; j.sent = 0; j.next = 0; j.born = comms_hub_now_ms();
}

void on_member_gone(int seat) {
    if (seat >= 0 && seat < kCoopMaxSeats) { std::memset(g_lastseq[seat], 0, sizeof g_lastseq[seat]); g_job[seat].on = false; }
}

void register_handlers() {
    if (g_registered) return;
    g_registered = true;
    const bool a = coop_ctl_register(kSwCtlGlobals, kSwCtlLast, on_ctl, nullptr);
    const bool b = coop_world_register(kSwWorldBit, kSwWorldLast, on_world, nullptr);
    std::fprintf(stderr, "[coopsw] switch module on: ctl %s, world %s\n", a ? "ok" : "REFUSED", b ? "ok" : "REFUSED");
}

void on_frame(unsigned now) {
    read_env();
    if (!coop_world_active()) return;
    register_handlers();
    g_my = comms_hub_my_seat();
    int lv = -1, ac = 0;
    if (!coop_group_key(&lv, &ac) || !coop_group_established()) return;
    if (!g_have_key || lv != g_lvl || ac != g_act) {          // a level change that missed the latch
        g_lvl = lv; g_act = ac; g_have_key = true; ++g_gen; new_level();
    }
    const int me = (int)data_0209f250;
    if (me < 0 || me >= 16 || !data_0209f394[me]) return;
    ++g_frames;
    if (g_frames == 1) g_shadow = (unsigned)data_0209f34c;

    // early arrivals from a sender that was not yet a member
    for (Early &e : g_early) {
        if (!e.on || !is_member(e.seat)) continue;
        e.on = false;
        if (e.lvl != g_lvl || e.act != g_act) continue;
        if (e.kind == kSwCtlGlobals) { SwGlobalsWire w; std::memcpy(&w, e.buf, sizeof w); apply_globals(w); }
        else apply_states(e.buf, e.len);
    }

    // arm: the word is the group's once the joiner has been told (or nobody could tell it)
    if (!g_armed) {
        const bool alone_or_owner = coop_group_i_am_owner();
        const bool ready = (g_have_globals || alone_or_owner || g_frames >= kGlobalsWait) && g_frames >= kSettleFrames;
        if (g_have_globals) settle_word();
        if (ready) { g_armed = true; g_shadow = (unsigned)data_0209f34c; if (g_log) std::fprintf(stderr, "[coopsw] armed frame=%u word=%08x have_globals=%d wall=%lld\n", g_frames, g_shadow, (int)g_have_globals, wall_ms()); }
    } else {
        local_word_changes(now);
    }
    if (g_frames >= 3) scan_objects(now);
    if (g_frames >= 3 && (g_frames % 15u) == 0) drain_pending(now);

    // proof setters (they do what a ROM setter does)
    for (int i = 0; i < g_nknob; ++i)
        if (!g_knob[i].done && g_armed && g_frames >= g_knob[i].frame) {
            g_knob[i].done = true;
            if (g_knob[i].bit >= 0 && g_knob[i].bit < 32) set_word_bit(g_knob[i].bit, g_knob[i].set ? 1 : 0);
            std::fprintf(stderr, "[coopsw-knob] %s bit %d wall=%lld\n", g_knob[i].set ? "set" : "clear", g_knob[i].bit, wall_ms());
            log_word("knob");
        }
    for (int i = 0; i < g_nforge; ++i)
        if (!g_forge[i].done && g_armed && g_frames >= g_forge[i].frame) { g_forge[i].done = true; knob_forge(g_forge[i].name, now); }
    for (int i = 0; i < g_npress; ++i)
        if (!g_press[i].done && g_armed && g_frames >= g_press[i].frame) { g_press[i].done = true; knob_press(g_press[i]); }
    watch_record();
    watch_field(now);
    if (g_rec_on) {
        for (int i = 0; i < 3; ++i) if (g_rec_n == i && g_frames >= g_rec[i]) { rec_take(i); ++g_rec_n; if (i == 2) rec_report(); }
    }

    // outgoing
    send_bits(now);
    send_objs(now);
    if (g_armed && (unsigned)(now - g_word_t) >= (coop_group_i_am_owner() ? kWordOwnerMs : kWordGuestMs)) send_word(now);
    if (coop_group_i_am_owner())
        for (Job &j : g_job) {
            if (!j.on || (int)(now - j.next) < 0) continue;
            if (!is_member(j.seat)) { if ((unsigned)(now - j.born) > 5000u) j.on = false; continue; }
            serve_job(j);
            ++j.sent;
            if (j.sent >= 4) j.on = false; else j.next = now + (j.sent == 1 ? kJobMs : 300u);
        }
    if (g_log && (g_frames % 120u) == 0) stats_line();
}

struct Reg {
    Reg() {
        coop_on_frame(on_frame);
        coop_on_group_latched(on_latched);
        coop_on_group_established(on_established);
        coop_on_snapshot_request(on_snapshot_request);
        coop_on_member_gone(on_member_gone);
    }
} g_reg;

}  // namespace

CoopSwitchStats coop_switch_stats() { return g_st; }

}  // namespace port
