// COOP RUNTIME SPAWNS (phase 2). Run mp1, lane COOPSPAWN1. See coop_spawn.h.
//
// Everything here hangs off the coop world core's callbacks (coop_world.h): a
// frame hook, the level-latched hook, the established hook and the owner's
// snapshot-request hook. No game file is edited; the ROM's own entries are
// called (the block's state change, the star marker's spawn, the actor spawn)
// or its own counters written.
//
// All wire handlers only validate and QUEUE; every game-state change happens
// in the frame hook, after the actor frame, never inside the hub pump.
//
// PORT_HOST_ABI. Actor offsets are the ROM's (32-bit host): uniqueID +0x04,
// param +0x08, class +0x0c, position +0x5c, area +0xcc, placed slot +0x0ce,
// dead flags +0x0f / +0x0e.

#include "coop_spawn.h"
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
extern unsigned char data_0209f21c;
extern unsigned char data_0209f30c[];
void *_ZN8dActor_c4NextEPKS_(const void *prev);
char *_ZN8dActor_c10FindWithIDEj(unsigned id);
char *_ZN8dActor_c15FindWithActorIDEjPS_(unsigned id, char *prev);
char *_ZN8dActor_c5SpawnEjjRK7Vector3PK10Vector3_16as(unsigned a, unsigned b, const void *pos, const void *v16, int area, int f);
void _ZN7fBase_c18MarkForDestructionEv(char *c);
void LinkSilverStarAndStarMarker(char *a, char *b);
void func_ov002_020e7e58(char *c);
void func_ov102_02149da8(void *self, int state);
signed char NumRedCoins(void);
void _ZN10StarMarker27SpawnRedCoinStarIfNecessaryEv(void *self);
unsigned DeathTable_GetBit(int id);
}

namespace port {
namespace {

long long wall_ms() {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    return (long long)((((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000ull);
}

enum : unsigned {
    kResendMs     = 100,      // every change goes out 3 times: now, +100, +200
    kSettleFrames = 20,       // frames after the group is live before the census starts
    kRedWait      = 180,      // a joiner stops waiting for the owner's red-coin sum after this many frames
    kJobMs        = 150,
    kRateWindowMs = 1000,
    kRatePerSeat  = 4,        // received SPAWNs per seat per second
    kAckAge       = 20000,
};
enum : int { kNetMax = 24, kLnkMax = 8, kInMax = 48, kBlkOutMax = 16, kAckMax = 64, kRedMax = 96, kPendSpawn = 16 };

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------
bool g_env = false, g_log = false, g_census_log = false;
struct Off { bool block, red, spawn, gone, snap; } g_off = {};
CoopSpawnStats g_st = {};
bool g_registered = false;
int g_my = -1;

bool     g_have_key = false;
int      g_lvl = -1, g_act = 0;
unsigned g_gen = 0;
unsigned g_frames = 0;
bool     g_armed = false;
bool     g_census_on = false;
unsigned g_wm = 0;                 // uid watermark: actors with a larger uid are new
unsigned g_census_lines = 0;

// blocks
struct BlkTrk { void *a; unsigned uid; int last; bool valid; };
BlkTrk g_blk[kSpSlots];
struct BlkOut { bool on; uint16_t slot, cls, seq; int sent; unsigned next; };
BlkOut g_bo[kBlkOutMax];
uint16_t g_bseq = 0;
uint16_t g_lastbseq[kCoopMaxSeats][kSpSlots];

// red coins
struct RedTrk { void *a; unsigned uid; short slot; unsigned char star; bool seen; };
RedTrk g_red[kRedMax]; int g_nred = 0;
int  g_red_prev = 0;
bool g_red_armed = false;
bool g_red_snap_have = false; int g_red_snap_sum = 0;
int g_red_star = 0;               // the star number the level's red coins carry (0 = not seen)

// announced actors
struct Net {
    bool used, remote, gone;
    uint8_t origin, kind; uint16_t counter, cls; uint32_t param; int32_t pos[3]; int8_t area;
    char *a; unsigned uid; bool seen;
    int sends; unsigned next; int gsends; unsigned gnext;
};
Net g_net[kNetMax];
uint16_t g_counter = 0;
struct Lnk { bool used, taken; char *a; unsigned uid; short mslot; bool seen; int gsends; unsigned gnext; };
Lnk g_lnk[kLnkMax];
bool g_lnk_gone[kSpSlots];
struct Ack { bool on; uint8_t origin; uint16_t counter; unsigned t; };
Ack g_ack[kAckMax];

// inbound queue (filled by the hub handlers, drained in the frame)
enum : int { kEvSpawn = 1, kEvGone = 2, kEvBlock = 3 };
struct InEv { bool on; int type, seat; unsigned born; unsigned char buf[32]; };
InEv g_in[kInMax];
struct Rate { unsigned t0; int n; };
Rate g_rate[kCoopMaxSeats];

// the owner's answers to snapshot requests
struct Job { bool on; int seat; uint32_t nonce; int sent; unsigned next; unsigned born; };
Job g_job[kCoopMaxSeats];
// an early RED from a sender not yet a member
struct EarlyRed { bool on; int seat; SpRedWire w; };
EarlyRed g_early;

// proof knobs
struct BlockKnob { unsigned frame; int cls, nth; bool done; };
BlockKnob g_kb[4]; int g_nkb = 0;
struct RedKnob { unsigned frame; int nth; bool done; };
RedKnob g_kr[10]; int g_nkr = 0;
struct StarKnob { unsigned frame; int cls; unsigned param; bool done; };
StarKnob g_ks[4]; int g_nks = 0;
struct Forge { unsigned frame; char name[12]; bool done; };
Forge g_forge[8]; int g_nforge = 0;

// ---------------------------------------------------------------------------
// Env.
// ---------------------------------------------------------------------------
void read_env() {
    if (g_env) return;
    g_env = true;
    const char *l = std::getenv("SM64DS_COOP_LOG");
    g_log = l && *l && *l != '0';
    const char *c = std::getenv("SM64DS_SPAWN_CENSUS");
    g_census_log = c && *c && *c != '0';
    if (const char *o = std::getenv("SM64DS_SPAWN_OFF")) {
        g_off.block = std::strstr(o, "block") != nullptr;
        g_off.red = std::strstr(o, "red") != nullptr;
        g_off.spawn = std::strstr(o, "spawn") != nullptr;
        g_off.gone = std::strstr(o, "gone") != nullptr;
        g_off.snap = std::strstr(o, "snap") != nullptr;
    }
    if (const char *e = std::getenv("SM64DS_SPAWN_BLOCK")) {
        const char *p = e;
        while (*p && g_nkb < 4) {
            unsigned f = 0; int cl = 0, n = 0, used = 0;
            if (std::sscanf(p, "%u:%i:%i%n", &f, &cl, &n, &used) < 3 || used <= 0) {
                used = 0; n = 0;
                if (std::sscanf(p, "%u:%i%n", &f, &cl, &used) < 2 || used <= 0) break;
            }
            g_kb[g_nkb++] = {f, cl, n, false};
            p += used;
            while (*p == ';' || *p == ',' || *p == ' ') ++p;
        }
    }
    if (const char *e = std::getenv("SM64DS_SPAWN_RED")) {
        const char *p = e;
        while (*p && g_nkr < 10) {
            unsigned f = 0; int n = 0, used = 0;
            if (std::sscanf(p, "%u:%i%n", &f, &n, &used) < 2 || used <= 0) break;
            g_kr[g_nkr++] = {f, n, false};
            p += used;
            while (*p == ';' || *p == ',' || *p == ' ') ++p;
        }
    }
    if (const char *e = std::getenv("SM64DS_SPAWN_STAR")) {
        const char *p = e;
        while (*p && g_nks < 4) {
            unsigned f = 0, pr = 0; int cl = 0, used = 0;
            if (std::sscanf(p, "%u:%i:%u%n", &f, &cl, &pr, &used) < 3 || used <= 0) break;
            g_ks[g_nks++] = {f, cl, pr, false};
            p += used;
            while (*p == ';' || *p == ',' || *p == ' ') ++p;
        }
    }
    if (const char *f = std::getenv("SM64DS_SPAWN_FORGE")) {
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
}

// ---------------------------------------------------------------------------
// Actors.
// ---------------------------------------------------------------------------
inline unsigned short a_cls(const char *a) { return *(const unsigned short *)(a + 0x0c); }
inline short a_slot(const char *a) { return *(const short *)(a + 0xce); }
inline unsigned a_uid(const char *a) { return *(const unsigned *)(a + 4); }
inline unsigned a_param(const char *a) { return *(const unsigned *)(a + 8); }
inline int a_area(const char *a) { return (signed char)*(const char *)(a + 0xcc); }
inline bool a_dead(const char *a) { return *(const unsigned char *)(a + 0x0f) || *(const unsigned char *)(a + 0x0e) == 2; }
bool is_player_ptr(const void *a) {
    for (int i = 0; i < 16; ++i) if (data_0209f394[i] == a) return true;
    return false;
}
char *my_player() {
    const int me = (int)data_0209f250;
    return (me >= 0 && me < 16) ? (char *)data_0209f394[me] : nullptr;
}
bool is_block_cls(unsigned c) { return c >= 20 && c <= 25; }
bool is_ann_cls(unsigned c) { return c == 178 || c == 179 || c == 276 || c == 277; }
bool is_star_cls(unsigned c) { return c == 178 || c == 179; }

CoopKeyWire wkey() { CoopKeyWire k; k.level = (uint16_t)g_lvl; k.act = (uint8_t)g_act; k.reserved = 0; return k; }
bool others_here() { return (coop_group_member_mask() & ~(1u << g_my)) != 0; }
bool send_world(int sub, const void *p, int n) { return coop_send_world(sub, p, n); }
bool send_ctl(int sub, const void *p, int n) { return coop_send_ctl(sub, p, n); }
bool live_key_is(const CoopKeyWire &k) { int l = -1, a = 0; return coop_group_key(&l, &a) && (int)k.level == l && (int)k.act == a; }
bool ready_for(const CoopKeyWire &k) { return g_have_key && (int)k.level == g_lvl && (int)k.act == g_act; }
bool is_member(int seat) { return seat >= 0 && seat < kCoopMaxSeats && ((coop_group_member_mask() >> seat) & 1u); }

// ---------------------------------------------------------------------------
// Acks (net ids that are gone: a late SPAWN of one is dropped).
// ---------------------------------------------------------------------------
void ack_add(int origin, int counter, unsigned now) {
    int free_i = 0; unsigned oldest = 0xFFFFFFFFu; bool any_free = false;
    for (int i = 0; i < kAckMax; ++i) {
        if (g_ack[i].on && g_ack[i].origin == origin && g_ack[i].counter == counter) return;
        if (!g_ack[i].on) { if (!any_free) { free_i = i; any_free = true; } }
        else if (!any_free && g_ack[i].t < oldest) { oldest = g_ack[i].t; free_i = i; }
    }
    g_ack[free_i] = {true, (uint8_t)origin, (uint16_t)counter, now};
}
bool ack_has(int origin, int counter) {
    for (const Ack &a : g_ack) if (a.on && a.origin == origin && a.counter == counter) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Quiet removal and spawn.
// ---------------------------------------------------------------------------
void kill_quiet(char *a) {
    const unsigned cls = a_cls(a);
    if (cls == 179) {
        char *m = _ZN8dActor_c10FindWithIDEj(*(unsigned *)(a + 0x434));
        if (m) { LinkSilverStarAndStarMarker(m, nullptr); _ZN7fBase_c18MarkForDestructionEv(m); }
    }
    if (is_star_cls(cls)) func_ov002_020e7e58(a);
    _ZN7fBase_c18MarkForDestructionEv(a);
}

char *do_spawn(const SpSpawnWire &w) {
    const int32_t pos[3] = {w.pos[0], w.pos[1], w.pos[2]};
    if (w.kind == kSpKindSilver) {
        char *m = _ZN8dActor_c5SpawnEjjRK7Vector3PK10Vector3_16as(0xb4, 0x50, pos, nullptr, w.area, -1);
        char *s = _ZN8dActor_c5SpawnEjjRK7Vector3PK10Vector3_16as(0xb3, w.param, pos, nullptr, w.area, -1);
        if (!m || !s) {
            if (m) _ZN7fBase_c18MarkForDestructionEv(m);
            if (s) _ZN7fBase_c18MarkForDestructionEv(s);
            return nullptr;
        }
        *(unsigned *)(s + 0x434) = a_uid(m);
        LinkSilverStarAndStarMarker(m, s);
        return s;
    }
    return _ZN8dActor_c5SpawnEjjRK7Vector3PK10Vector3_16as(w.cls, w.param, pos, nullptr, w.area, -1);
}

// ---------------------------------------------------------------------------
// Announced actors.
// ---------------------------------------------------------------------------
Net *net_find(int origin, int counter) {
    for (Net &n : g_net) if (n.used && n.origin == origin && n.counter == counter) return &n;
    return nullptr;
}
Net *net_alloc() {
    for (Net &n : g_net) if (!n.used) return &n;
    for (Net &n : g_net) if (n.used && n.gone) return &n;       // reuse a finished one
    return nullptr;
}

bool register_local(char *a, int cls, unsigned param, const int pos[3], int area) {
    Net *n = net_alloc();
    if (!n) { ++g_st.drop_full; return false; }
    ++g_counter; if (!g_counter) g_counter = 1;
    *n = Net();
    n->used = true; n->origin = (uint8_t)g_my; n->counter = g_counter; n->cls = (uint16_t)cls;
    n->kind = (uint8_t)(cls == 179 ? kSpKindSilver : kSpKindActor);
    n->param = param; n->pos[0] = pos[0]; n->pos[1] = pos[1]; n->pos[2] = pos[2]; n->area = (int8_t)area;
    n->a = a; n->uid = a ? a_uid(a) : 0; n->sends = 0; n->next = 0; n->seen = true;
    ++g_st.spawn_local;
    if (g_log)
        std::fprintf(stderr, "[coopspawn] announce cls=%d param=%u pos=%d,%d,%d area=%d id=%d/%d uid=%u frame=%u wall=%lld\n",
                     cls, param, pos[0] >> 12, pos[1] >> 12, pos[2] >> 12, area, g_my, (int)n->counter, n->uid, g_frames, wall_ms());
    return true;
}

void send_spawn_of(const Net &n) {
    SpSpawnWire w; std::memset(&w, 0, sizeof w);
    w.key = wkey(); w.origin = n.origin; w.kind = n.kind; w.counter = n.counter; w.cls = n.cls;
    w.area = n.area; w.param = n.param; w.pos[0] = n.pos[0]; w.pos[1] = n.pos[1]; w.pos[2] = n.pos[2];
    if (send_world(kSpWorldSpawn, &w, sizeof w)) ++g_st.sent_spawn;
}

void send_net(unsigned now) {
    for (Net &n : g_net) {
        if (!n.used) continue;
        if (!n.remote && !n.gone && (int)(now - n.next) >= 0 && n.sends < 3) {
            if (others_here()) { send_spawn_of(n); ++n.sends; n.next = now + kResendMs; }
            else n.sends = 3;
        }
        if (n.gone && n.gsends < 3 && (int)(now - n.gnext) >= 0) {
            if (others_here()) {
                SpGoneWire g; std::memset(&g, 0, sizeof g);
                g.key = wkey(); g.origin = n.origin; g.kind = kSpGoneAnn; g.id = n.counter;
                if (send_world(kSpWorldGone, &g, sizeof g)) ++g_st.sent_gone;
                ++n.gsends; n.gnext = now + kResendMs;
            } else n.gsends = 3;
        }
    }
    for (Lnk &l : g_lnk) {
        if (!l.used || !l.taken || l.gsends >= 3 || (int)(now - l.gnext) < 0) continue;
        if (!others_here()) { l.gsends = 3; continue; }
        SpGoneWire g; std::memset(&g, 0, sizeof g);
        g.key = wkey(); g.origin = 0xFF; g.kind = kSpGoneLinked; g.id = (uint16_t)l.mslot;
        if (send_world(kSpWorldGone, &g, sizeof g)) ++g_st.sent_gone;
        ++l.gsends; l.gnext = now + kResendMs;
    }
}

// A star that every console spawns natively and links to a PLACED marker is not announced.
bool linked_to_placed_marker(const char *a, short *mslot) {
    const unsigned mid = *(const unsigned *)(a + 0x434);
    if (!mid) return false;
    char *m = _ZN8dActor_c10FindWithIDEj(mid);
    if (!m || a_cls(m) != 0xb4) return false;
    const short s = a_slot(m);
    if (s < 0 || s >= kSpSlots) return false;
    *mslot = s;
    return true;
}

// A prize that comes out of a ? / ! / cap block is that player's own (the block hit is replayed on every
// console and each console spawns its own): never announce a runtime actor born at a block that is mid-hit.
bool born_at_hit_block(const char *a, const int *blk_pos, int nblk) {
    const int *p = (const int *)(a + 0x5c);
    for (int i = 0; i < nblk; ++i) {
        const long long dx = (long long)p[0] - blk_pos[i * 3 + 0], dz = (long long)p[2] - blk_pos[i * 3 + 2];
        const long long dy = (long long)p[1] - blk_pos[i * 3 + 1];
        const long long lim = 100ll << 12;
        if (dx > -lim && dx < lim && dz > -lim && dz < lim && dy > -(150ll << 12) && dy < (150ll << 12)) return true;
    }
    return false;
}

Lnk *lnk_alloc() { for (Lnk &l : g_lnk) if (!l.used) return &l; return nullptr; }

// ---------------------------------------------------------------------------
// Red coins.
// ---------------------------------------------------------------------------
int red_slots() { return data_0209f21c > 16 ? 16 : (int)data_0209f21c; }
int red_sum() {
    int s = 0;
    for (int i = 0; i < red_slots(); ++i) s += (signed char)data_0209f30c[i];
    return s;
}
// a body slot that is not mine, inside the summed range
int red_remote_slot() {
    const int me = (int)data_0209f250, n = red_slots();
    for (int i = 0; i < n; ++i) if (i != me) return i;
    return (me >= 0 && me < n) ? me : -1;      // a one-body count: the sum is all that matters
}
void red_check_star(int star_id) {
    if (NumRedCoins() != 8 || star_id <= 0) return;
    for (char *m = _ZN8dActor_c15FindWithActorIDEjPS_(0xb4, nullptr); m; m = _ZN8dActor_c15FindWithActorIDEjPS_(0xb4, m)) {
        if (*(unsigned char *)(m + 0x1d8) != 0) continue;
        if (*(unsigned char *)(m + 0x1d9) != star_id) continue;
        if (*(unsigned char *)(m + 0x1db) & 4) continue;           // its star is already out
        _ZN10StarMarker27SpawnRedCoinStarIfNecessaryEv(m);
        ++g_st.star_spawn_native;
        if (g_log)
            std::fprintf(stderr, "[coopspawn] red coin star: marker slot=%d star=%d sum=%d called the ROM's spawn frame=%u wall=%lld\n",
                         (int)a_slot(m), (int)*(unsigned char *)(m + 0x1d9), (int)NumRedCoins(), g_frames, wall_ms());
    }
}
void red_bump(int n, int star_id, const char *why) {
    const int sl = red_remote_slot();
    if (sl < 0 || n <= 0) return;
    int v = (signed char)data_0209f30c[sl] + n;
    if (v > 0x63) v = 0x63;
    data_0209f30c[sl] = (unsigned char)v;
    g_st.red_remote += (unsigned)n;
    g_red_prev = red_sum();
    if (g_log)
        std::fprintf(stderr, "[coopspawn] red coin %s +%d on body slot %d: sum=%d frame=%u wall=%lld\n", why, n, sl, (int)NumRedCoins(), g_frames, wall_ms());
    red_check_star(star_id);
}

void red_rebase() {
    g_nred = 0;
    g_red_prev = red_sum();
}

// ---------------------------------------------------------------------------
// One walk of the live actors per frame.
// ---------------------------------------------------------------------------
void scan(unsigned now) {
    // mark
    for (Net &n : g_net) n.seen = false;
    for (Lnk &l : g_lnk) l.seen = false;
    for (int i = 0; i < g_nred; ++i) g_red[i].seen = false;
    unsigned maxuid = g_wm;
    int blk_pos[8 * 3]; int nblk = 0;      // blocks mid-hit this frame

    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        if (is_player_ptr(a)) continue;
        const unsigned cls = a_cls(a);
        const short slot = a_slot(a);
        const bool dead = a_dead(a);
        const unsigned uid = a_uid(a);

        // ---- blocks -------------------------------------------------------
        if (is_block_cls(cls) && slot >= 0 && slot < kSpSlots && !dead) {
            const int st = *(const int *)(a + 0x3e8);
            if (st != 0 && nblk < 8) {
                const int *p = (const int *)(a + 0x5c);
                blk_pos[nblk * 3 + 0] = p[0]; blk_pos[nblk * 3 + 1] = p[1]; blk_pos[nblk * 3 + 2] = p[2]; ++nblk;
            }
            BlkTrk &t = g_blk[slot];
            if (!t.valid || t.a != a || t.uid != uid) { t.a = a; t.uid = uid; t.last = st; t.valid = true; }
            else if (st != t.last) {
                const int prev = t.last; t.last = st;
                if (prev == 0 && st != 0 && g_armed) {
                    ++g_bseq; if (!g_bseq) g_bseq = 1;
                    int fi = -1;
                    for (int i = 0; i < kBlkOutMax; ++i) if (!g_bo[i].on) { fi = i; break; }
                    if (fi < 0) fi = 0;
                    g_bo[fi] = {true, (uint16_t)slot, (uint16_t)cls, g_bseq, 0, now};
                    ++g_st.block_local;
                    const char *pl = my_player();
                    if (g_log)
                        std::fprintf(stderr, "[coopspawn] block hit local slot=%d cls=%u hitter=%d state=%d frame=%u wall=%lld\n",
                                     (int)slot, cls, (int)(a[0x3f2] & 0xff), st, g_frames, wall_ms());
                    (void)pl;
                }
            }
        }

        // ---- red coins (placed) -------------------------------------------
        if (cls == 289 && slot >= 0 && slot < kSpSlots && !dead) {
            bool found = false;
            for (int i = 0; i < g_nred; ++i)
                if (g_red[i].slot == slot && g_red[i].a == a && g_red[i].uid == uid) { g_red[i].seen = true; found = true; break; }
            if (!found && g_nred < kRedMax)
                g_red[g_nred++] = {a, uid, slot, *(const unsigned char *)(a + 0x3ab), true};
            const unsigned char st = *(const unsigned char *)(a + 0x3ab);
            if (st >= 1 && st <= 7) g_red_star = st;
        }

        // ---- registered stars and prizes: still here? ---------------------
        if (is_ann_cls(cls)) {
            const bool taken_now = is_star_cls(cls) && !dead && *(const unsigned *)(a + 0x440) == 5u &&
                                   is_player_ptr(*(void *const *)(a + 0x438));
            for (Net &n : g_net)
                if (n.used && !n.gone && n.a == a && n.uid == uid) { n.seen = !dead && !taken_now; }
            for (Lnk &l : g_lnk)
                if (l.used && !l.taken && l.a == a && l.uid == uid) { l.seen = !dead && !taken_now; }
        }

        // ---- new runtime actors -------------------------------------------
        if (uid > maxuid) maxuid = uid;
        if (g_census_on && uid > g_wm && !dead && slot == -1) {
            if (g_census_log && g_census_lines < 400) {
                ++g_census_lines;
                const int *p = (const int *)(a + 0x5c);
                std::fprintf(stderr, "[coopspawn] new actor cls=%u uid=%u pos=%d,%d,%d area=%d param=%u frame=%u wall=%lld\n",
                             cls, uid, p[0] >> 12, p[1] >> 12, p[2] >> 12, a_area(a), a_param(a), g_frames, wall_ms());
            }
            if (is_ann_cls(cls) && !g_off.spawn) {
                short mslot = -1;
                bool registered = false;
                for (Net &n : g_net) if (n.used && n.a == a && n.uid == uid) registered = true;
                for (Lnk &l : g_lnk) if (l.used && l.a == a && l.uid == uid) registered = true;
                if (!registered) {
                    if (cls == 178 && linked_to_placed_marker(a, &mslot)) {
                        Lnk *l = lnk_alloc();
                        if (l) {
                            *l = Lnk(); l->used = true; l->a = a; l->uid = uid; l->mslot = mslot; l->seen = true;
                            if (g_log)
                                std::fprintf(stderr, "[coopspawn] linked star cls=%u marker slot=%d uid=%u pos=%d,%d,%d frame=%u wall=%lld\n",
                                             cls, (int)mslot, uid, ((const int *)(a + 0x5c))[0] >> 12, ((const int *)(a + 0x5c))[1] >> 12,
                                             ((const int *)(a + 0x5c))[2] >> 12, g_frames, wall_ms());
                            if (g_lnk_gone[mslot] && !g_off.gone) {       // the friend already took it
                                kill_quiet(a); l->taken = true; l->gsends = 3;
                                ++g_st.gone_applied;
                                if (g_log) std::fprintf(stderr, "[coopspawn] linked star marker slot=%d was already taken: removed wall=%lld\n", (int)mslot, wall_ms());
                            }
                        }
                    } else if ((cls == 276 || cls == 277) && born_at_hit_block(a, blk_pos, nblk)) {
                        // a block's own prize: per player, not announced
                    } else {
                        const int *p = (const int *)(a + 0x5c);
                        const int pos[3] = {p[0], p[1], p[2]};
                        register_local(a, (int)cls, a_param(a), pos, a_area(a));
                    }
                }
            }
        }
    }

    // ---- gone: announced and linked ----------------------------------------
    for (Net &n : g_net) {
        if (!n.used || n.gone || n.seen) continue;
        n.gone = true; n.gsends = 0; n.gnext = now;
        if (!n.remote) {
            ++g_st.gone_local;
            if (g_log) std::fprintf(stderr, "[coopspawn] gone local cls=%u id=%d/%d frame=%u wall=%lld\n", (unsigned)n.cls, (int)n.origin, (int)n.counter, g_frames, wall_ms());
        } else {
            // a remote copy that left on this console: the others hear it (first claim wins)
            ++g_st.gone_local;
            if (g_log) std::fprintf(stderr, "[coopspawn] gone local (remote copy) cls=%u id=%d/%d frame=%u wall=%lld\n", (unsigned)n.cls, (int)n.origin, (int)n.counter, g_frames, wall_ms());
        }
        ack_add(n.origin, n.counter, now);
    }
    for (Lnk &l : g_lnk) {
        if (!l.used || l.taken || l.seen) continue;
        l.taken = true; l.gsends = 0; l.gnext = now;
        ++g_st.gone_local;
        if (g_log) std::fprintf(stderr, "[coopspawn] gone local linked star marker slot=%d frame=%u wall=%lld\n", (int)l.mslot, g_frames, wall_ms());
    }

    // ---- red coins gone ------------------------------------------------------
    if (g_red_armed) {
        int gone = 0, star = 0;
        for (int i = 0; i < g_nred;) {
            RedTrk &t = g_red[i];
            if (t.seen) { ++i; continue; }
            if (DeathTable_GetBit(t.slot) && t.star >= 1 && t.star <= 7) { ++gone; star = t.star; }
            g_red[i] = g_red[--g_nred];
        }
        const int sum = red_sum();
        const int inc = sum > g_red_prev ? sum - g_red_prev : 0;
        const int rem = gone - inc;
        if (rem > 0 && !g_off.red) red_bump(rem, star, "remote");
        else g_red_prev = sum;
        (void)star;
    } else {
        // not armed: forget the coins that went away while the level settled
        for (int i = 0; i < g_nred;) {
            if (g_red[i].seen) ++i; else g_red[i] = g_red[--g_nred];
        }
        g_red_prev = red_sum();
    }
    if (g_census_on && maxuid > g_wm) g_wm = maxuid;
}

// ---------------------------------------------------------------------------
// Inbound events (applied in the frame).
// ---------------------------------------------------------------------------
void push_in(int type, int seat, const void *p, int n, unsigned now) {
    for (InEv &e : g_in)
        if (!e.on) { e.on = true; e.type = type; e.seat = seat; e.born = now; std::memcpy(e.buf, p, (size_t)n); return; }
    ++g_st.drop_full;
}

char *find_block(int slot, int cls) {
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a))
        if (a_slot(a) == slot && a_cls(a) == (unsigned)cls && !a_dead(a) && !is_player_ptr(a)) return a;
    return nullptr;
}

void apply_block(int seat, const SpBlockWire &w) {
    char *a = find_block(w.slot, w.cls);
    if (!a) { ++g_st.block_notfound; return; }
    if (*(const int *)(a + 0x3e8) != 0) { ++g_st.block_busy; return; }
    const char *pl = my_player();
    int ch = pl ? (int)(*(const unsigned *)(pl + 8) & 0xff) : 0;
    if (ch < 0 || ch > 3) ch = 0;
    *(unsigned char *)(a + 0x3f2) = (unsigned char)ch;      // MY character takes the prize
    func_ov102_02149da8(a, 1);                               // the block's own "hit" state
    BlkTrk &t = g_blk[w.slot];
    t.a = a; t.uid = a_uid(a); t.last = *(const int *)(a + 0x3e8); t.valid = true;
    ++g_st.block_applied;
    if (g_log)
        std::fprintf(stderr, "[coopspawn] block replay slot=%d cls=%d from seat %d hitter=%d state=%d pos=%d,%d,%d frame=%u wall=%lld\n", (int)w.slot, (int)w.cls,
                     seat, ch, *(const int *)(a + 0x3e8), ((const int *)(a + 0x5c))[0] >> 12, ((const int *)(a + 0x5c))[1] >> 12, ((const int *)(a + 0x5c))[2] >> 12, g_frames, wall_ms());
}

bool near_same(const char *a, const SpSpawnWire &w) {
    const int *p = (const int *)(a + 0x5c);
    const long long lim = 24ll << 12;
    for (int i = 0; i < 3; ++i) {
        const long long d = (long long)p[i] - w.pos[i];
        if (d < -lim || d > lim) return false;
    }
    return true;
}

void apply_spawn(int seat, const SpSpawnWire &w, unsigned now) {
    if (ack_has(w.origin, w.counter)) { ++g_st.spawn_dup; return; }
    if (net_find(w.origin, w.counter)) { ++g_st.spawn_dup; return; }
    Net *n = net_alloc();
    if (!n) { ++g_st.drop_full; return; }
    // the same thing already out here (spawned natively by a replayed event): adopt it, never double it
    char *adopt = nullptr;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        if (a_cls(a) != w.cls || a_slot(a) != -1 || a_dead(a) || is_player_ptr(a)) continue;
        if ((a_param(a) & 0xff) != (w.param & 0xff) && is_star_cls(w.cls)) continue;
        bool taken = false;
        for (const Net &o : g_net) if (o.used && o.a == a) taken = true;
        for (const Lnk &o : g_lnk) if (o.used && o.a == a) taken = true;
        if (taken || !near_same(a, w)) continue;
        adopt = a; break;
    }
    char *a = adopt ? adopt : do_spawn(w);
    if (!a) { ++g_st.spawn_failed; return; }
    *n = Net();
    n->used = true; n->remote = true; n->origin = w.origin; n->kind = w.kind; n->counter = w.counter; n->cls = w.cls;
    n->param = w.param; n->pos[0] = w.pos[0]; n->pos[1] = w.pos[1]; n->pos[2] = w.pos[2]; n->area = w.area;
    n->a = a; n->uid = a_uid(a); n->seen = true; n->sends = 3;
    if (a_uid(a) > g_wm) g_wm = a_uid(a);
    if (adopt) ++g_st.spawn_adopted; else ++g_st.spawn_applied;
    if (g_log)
        std::fprintf(stderr, "[coopspawn] spawn %s cls=%d from seat %d id=%d/%d pos=%d,%d,%d uid=%u frame=%u wall=%lld\n", adopt ? "adopted" : "applied",
                     (int)w.cls, seat, (int)w.origin, (int)w.counter, w.pos[0] >> 12, w.pos[1] >> 12, w.pos[2] >> 12, n->uid, g_frames, wall_ms());
    (void)now;
}

void apply_gone(int seat, const SpGoneWire &w, unsigned now) {
    if (w.kind == kSpGoneAnn) {
        ack_add(w.origin, w.id, now);
        Net *n = net_find(w.origin, w.id);
        if (!n) { ++g_st.gone_unknown; return; }
        if (n->gone) return;
        if (n->a && n->seen) kill_quiet(n->a);
        n->gone = true; n->gsends = 3;               // heard, not re-sent: the claimer already told everyone
        ++g_st.gone_applied;
        if (g_log) std::fprintf(stderr, "[coopspawn] gone applied cls=%u id=%d/%d from seat %d frame=%u wall=%lld\n", (unsigned)n->cls, (int)w.origin, (int)w.id, seat, g_frames, wall_ms());
        return;
    }
    // linked star by marker slot
    if (w.id < kSpSlots) g_lnk_gone[w.id] = true;
    for (Lnk &l : g_lnk) {
        if (!l.used || l.mslot != (short)w.id || l.taken) continue;
        if (l.seen && l.a) kill_quiet(l.a);
        l.taken = true; l.gsends = 3;
        ++g_st.gone_applied;
        if (g_log) std::fprintf(stderr, "[coopspawn] gone applied linked star marker slot=%d from seat %d frame=%u wall=%lld\n", (int)w.id, seat, g_frames, wall_ms());
        return;
    }
    ++g_st.gone_unknown;
}

void drain_in(unsigned now) {
    for (InEv &e : g_in) {
        if (!e.on) continue;
        if ((unsigned)(now - e.born) > 8000u) { e.on = false; continue; }
        if (!g_armed && e.type != kEvGone) continue;         // a joiner waits until its level is up
        e.on = false;
        if (e.type == kEvBlock) { SpBlockWire w; std::memcpy(&w, e.buf, sizeof w); apply_block(e.seat, w); }
        else if (e.type == kEvSpawn) { SpSpawnWire w; std::memcpy(&w, e.buf, sizeof w); apply_spawn(e.seat, w, now); }
        else if (e.type == kEvGone) { SpGoneWire w; std::memcpy(&w, e.buf, sizeof w); apply_gone(e.seat, w, now); }
    }
}

// ---------------------------------------------------------------------------
// Receiving.
// ---------------------------------------------------------------------------
void apply_red_snap(const SpRedWire &w) {
    if (g_off.snap) { ++g_st.drop_off; return; }
    g_red_snap_have = true; g_red_snap_sum = w.sum; if (w.star) g_red_star = w.star;
    ++g_st.red_snap_applied;
    if (g_log) std::fprintf(stderr, "[coopspawn] red snapshot from the owner: sum=%d (mine %d) wall=%lld\n", (int)w.sum, red_sum(), wall_ms());
}

void on_ctl(int seat, int sub, const unsigned char *p, int len, void *) {
    if (!coop_world_active()) return;
    if (sub == kSpCtlRed) {
        ++g_st.recv_red;
        if (!sp_red_validate(p, len)) { ++g_st.drop_bad; return; }
        SpRedWire w; std::memcpy(&w, p, sizeof w);
        if (seat == g_my || w.target != g_my) return;
        if (!live_key_is(w.key)) { ++g_st.drop_group; return; }
        if (!ready_for(w.key) || !is_member(seat)) { g_early.on = true; g_early.seat = seat; g_early.w = w; return; }
        apply_red_snap(w);
        return;
    }
    ++g_st.drop_bad;
}

bool rate_ok(int seat) {
    const unsigned now = comms_hub_now_ms();
    Rate &r = g_rate[seat];
    if ((unsigned)(now - r.t0) >= kRateWindowMs) { r.t0 = now; r.n = 0; }
    if (r.n >= kRatePerSeat) return false;
    ++r.n;
    return true;
}

void on_world(int seat, int sub, const unsigned char *p, int len, void *) {
    if (!coop_world_active()) return;
    if (seat == g_my || seat < 0 || seat >= kCoopMaxSeats) return;
    const unsigned now = comms_hub_now_ms();
    switch (sub) {
    case kSpWorldSpawn: {
        ++g_st.recv_spawn;
        if (!sp_spawn_validate(p, len)) { ++g_st.drop_bad; return; }
        SpSpawnWire w; std::memcpy(&w, p, sizeof w);
        if (!live_key_is(w.key)) { ++g_st.drop_group; return; }
        if (!is_member(seat)) { ++g_st.drop_member; return; }
        if (g_off.spawn) { ++g_st.drop_off; return; }
        if (net_find(w.origin, w.counter) || ack_has(w.origin, w.counter)) { ++g_st.spawn_dup; return; }
        for (const InEv &e : g_in)
            if (e.on && e.type == kEvSpawn) {
                SpSpawnWire q; std::memcpy(&q, e.buf, sizeof q);
                if (q.origin == w.origin && q.counter == w.counter) { ++g_st.spawn_dup; return; }
            }
        if (!rate_ok(seat)) { ++g_st.drop_rate; return; }
        ++g_st.spawn_queued;
        push_in(kEvSpawn, seat, &w, (int)sizeof w, now);
        return;
    }
    case kSpWorldGone: {
        ++g_st.recv_gone;
        if (!sp_gone_validate(p, len)) { ++g_st.drop_bad; return; }
        SpGoneWire w; std::memcpy(&w, p, sizeof w);
        if (!live_key_is(w.key)) { ++g_st.drop_group; return; }
        if (!is_member(seat)) { ++g_st.drop_member; return; }
        if (g_off.gone) { ++g_st.drop_off; return; }
        push_in(kEvGone, seat, &w, (int)sizeof w, now);
        return;
    }
    case kSpWorldBlock: {
        ++g_st.recv_block;
        if (!sp_block_validate(p, len)) { ++g_st.drop_bad; return; }
        SpBlockWire w; std::memcpy(&w, p, sizeof w);
        if (!live_key_is(w.key)) { ++g_st.drop_group; return; }
        if (!is_member(seat)) { ++g_st.drop_member; return; }
        if (g_off.block) { ++g_st.drop_off; return; }
        uint16_t &last = g_lastbseq[seat][w.slot];
        if (!sp_seq_newer(w.seq, last)) { ++g_st.drop_old; return; }
        last = w.seq;
        push_in(kEvBlock, seat, &w, (int)sizeof w, now);
        return;
    }
    default:
        ++g_st.drop_bad;
    }
}

// ---------------------------------------------------------------------------
// Sending: blocks, the owner's join answers.
// ---------------------------------------------------------------------------
void send_blocks(unsigned now) {
    for (BlkOut &b : g_bo) {
        if (!b.on || (int)(now - b.next) < 0) continue;
        if (!others_here()) { b.on = false; continue; }
        SpBlockWire w; std::memset(&w, 0, sizeof w);
        w.key = wkey(); w.slot = b.slot; w.cls = b.cls; w.seq = b.seq;
        if (send_world(kSpWorldBlock, &w, sizeof w)) ++g_st.sent_block;
        if (++b.sent >= 3) b.on = false; else b.next = now + kResendMs;
    }
}

void serve_job(Job &j) {
    SpRedWire r; std::memset(&r, 0, sizeof r);
    r.key = wkey(); r.nonce = j.nonce; r.target = (uint8_t)j.seat;
    int s = (int)NumRedCoins(); if (s < 0) s = 0; if (s > 8) s = 8;
    r.sum = (uint8_t)s; r.star = (uint8_t)(g_red_star & 7);
    if (send_ctl(kSpCtlRed, &r, sizeof r)) ++g_st.sent_red;
    int n = 0;
    for (Net &e : g_net)
        if (e.used && !e.gone && e.seen && n < 8) { send_spawn_of(e); ++n; }
    if (g_log && j.sent == 0)
        std::fprintf(stderr, "[coopspawn] serve join to seat %d: red sum=%d live announced=%d wall=%lld\n", j.seat, s, n, wall_ms());
}

int live_class(unsigned cls) {
    int n = 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a))
        if (a_cls(a) == cls && !a_dead(a) && !is_player_ptr(a)) ++n;
    return n;
}

void stats_line() {
    if (!g_log) return;
    const CoopSpawnStats &s = g_st;
    std::fprintf(stderr,
        "[coopspawnstat] seat=%d armed=%d redsum=%d stars=%d sent spawn%llu gone%llu block%llu red%llu recv spawn%llu gone%llu block%llu red%llu "
        "block local=%llu applied=%llu busy=%llu nf=%llu spawn local=%llu applied=%llu adopted=%llu dup=%llu failed=%llu "
        "gone local=%llu applied=%llu unk=%llu red remote=%llu snap=%llu nativestar=%llu drop bad=%llu group=%llu member=%llu class=%llu old=%llu off=%llu rate=%llu full=%llu\n",
        g_my, (int)g_armed, red_sum(), live_class(178), s.sent_spawn, s.sent_gone, s.sent_block, s.sent_red, s.recv_spawn, s.recv_gone, s.recv_block, s.recv_red,
        s.block_local, s.block_applied, s.block_busy, s.block_notfound, s.spawn_local, s.spawn_applied, s.spawn_adopted, s.spawn_dup, s.spawn_failed,
        s.gone_local, s.gone_applied, s.gone_unknown, s.red_remote, s.red_snap_applied, s.star_spawn_native,
        s.drop_bad, s.drop_group, s.drop_member, s.drop_class, s.drop_old, s.drop_off, s.drop_rate, s.drop_full);
}

// ---------------------------------------------------------------------------
// Proof knobs.
// ---------------------------------------------------------------------------
void knob_block(BlockKnob &k) {
    char *c[64]; int n = 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a && n < 64; a = (char *)_ZN8dActor_c4NextEPKS_(a))
        if ((int)a_cls(a) == k.cls && a_slot(a) >= 0 && !a_dead(a) && !is_player_ptr(a)) c[n++] = a;
    std::sort(c, c + n, [](char *x, char *y) { return a_slot(x) < a_slot(y); });
    if (n == 0) { std::fprintf(stderr, "[coopspawn-knob] block cls=%d: no live block\n", k.cls); return; }
    char *a = c[k.nth % n];
    const char *pl = my_player();
    int ch = pl ? (int)(*(const unsigned *)(pl + 8) & 0xff) : 0;
    if (ch > 3) ch = 0;
    bool did = false;
    if (*(const int *)(a + 0x3e8) == 0) {
        *(unsigned char *)(a + 0x3f2) = (unsigned char)ch;
        func_ov102_02149da8(a, 1);               // what QuestionBlock::OnGroundPounded does after its checks
        did = true;
    }
    std::fprintf(stderr, "[coopspawn-knob] block hit cls=%d slot=%d (candidate %d of %d) did=%d wall=%lld\n", k.cls, (int)a_slot(a), k.nth % n, n, (int)did, wall_ms());
}

void knob_red(RedKnob &k) {
    char *c[64]; int n = 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a && n < 64; a = (char *)_ZN8dActor_c4NextEPKS_(a))
        if (a_cls(a) == 289 && a_slot(a) >= 0 && !a_dead(a) && !is_player_ptr(a)) c[n++] = a;
    std::sort(c, c + n, [](char *x, char *y) { return a_slot(x) < a_slot(y); });
    char *pl = my_player();
    if (!pl || n == 0) { std::fprintf(stderr, "[coopspawn-knob] red %d: no live red coin\n", k.nth); return; }
    char *a = c[k.nth % n];
    std::memcpy(pl + 0x5c, a + 0x5c, 12);
    std::fprintf(stderr, "[coopspawn-knob] red coin goto slot=%d (candidate %d of %d) pos=%d,%d,%d wall=%lld\n", (int)a_slot(a), k.nth % n, n,
                 ((const int *)(a + 0x5c))[0] >> 12, ((const int *)(a + 0x5c))[1] >> 12, ((const int *)(a + 0x5c))[2] >> 12, wall_ms());
}

void knob_star(StarKnob &k) {
    char *pl = my_player();
    if (!pl) return;
    const int *p = (const int *)(pl + 0x5c);
    const int pos[3] = {p[0] + (160 << 12), p[1] + (80 << 12), p[2]};
    char *a = _ZN8dActor_c5SpawnEjjRK7Vector3PK10Vector3_16as((unsigned)k.cls, k.param, pos, nullptr, a_area(pl), -1);
    std::fprintf(stderr, "[coopspawn-knob] spawn cls=%d param=%u -> %p uid=%u pos=%d,%d,%d wall=%lld\n", k.cls, k.param, (void *)a, a ? a_uid(a) : 0u,
                 pos[0] >> 12, pos[1] >> 12, pos[2] >> 12, wall_ms());
}

void knob_forge(const char *name) {
    std::fprintf(stderr, "[coopspawn-knob] forge %s\n", name);
    unsigned char b[128]; std::memset(b, 0, sizeof b);
    SpSpawnWire s; std::memset(&s, 0, sizeof s);
    s.key = wkey(); s.origin = (uint8_t)g_my; s.kind = kSpKindActor; s.counter = 9000 % 60000; s.cls = 178; s.area = 0; s.param = 0;
    s.pos[0] = 0; s.pos[1] = 0; s.pos[2] = 0;
    if (!std::strcmp(name, "cls")) { s.cls = 207; send_world(kSpWorldSpawn, &s, sizeof s); }              // a Cannon: not on the list
    else if (!std::strcmp(name, "cls2")) { s.cls = 200; send_world(kSpWorldSpawn, &s, sizeof s); }       // a Goomba
    else if (!std::strcmp(name, "kind")) { s.kind = 3; send_world(kSpWorldSpawn, &s, sizeof s); }
    else if (!std::strcmp(name, "param")) { s.param = 0x12345678u; send_world(kSpWorldSpawn, &s, sizeof s); }
    else if (!std::strcmp(name, "counter")) { s.counter = 0; send_world(kSpWorldSpawn, &s, sizeof s); }
    else if (!std::strcmp(name, "pos")) { s.pos[1] = 0x7ffffff0; send_world(kSpWorldSpawn, &s, sizeof s); }
    else if (!std::strcmp(name, "origin")) { s.origin = 77; send_world(kSpWorldSpawn, &s, sizeof s); }
    else if (!std::strcmp(name, "resv")) { s.reserved = 1; send_world(kSpWorldSpawn, &s, sizeof s); }
    else if (!std::strcmp(name, "short")) { send_world(kSpWorldSpawn, b, 9); }
    else if (!std::strcmp(name, "long")) { send_world(kSpWorldSpawn, b, 100); }
    else if (!std::strcmp(name, "key")) { s.key.act = (uint8_t)((g_act + 1) & 7); send_world(kSpWorldSpawn, &s, sizeof s); }
    else if (!std::strcmp(name, "goodstar")) { send_world(kSpWorldSpawn, &s, sizeof s); }                 // valid: the control that the forge shape is right
    else if (!std::strcmp(name, "gone")) {
        SpGoneWire g; std::memset(&g, 0, sizeof g); g.key = wkey(); g.origin = 99; g.kind = kSpGoneAnn; g.id = 5;
        send_world(kSpWorldGone, &g, sizeof g);
    } else if (!std::strcmp(name, "goneslot")) {
        SpGoneWire g; std::memset(&g, 0, sizeof g); g.key = wkey(); g.origin = 0xFF; g.kind = kSpGoneLinked; g.id = 700;
        send_world(kSpWorldGone, &g, sizeof g);
    } else if (!std::strcmp(name, "blockslot")) {
        SpBlockWire w; std::memset(&w, 0, sizeof w); w.key = wkey(); w.slot = 600; w.cls = 21; w.seq = 1;
        send_world(kSpWorldBlock, &w, sizeof w);
    } else if (!std::strcmp(name, "blockcls")) {
        SpBlockWire w; std::memset(&w, 0, sizeof w); w.key = wkey(); w.slot = 3; w.cls = 207; w.seq = 1;
        send_world(kSpWorldBlock, &w, sizeof w);
    } else if (!std::strcmp(name, "redsum")) {
        SpRedWire r; std::memset(&r, 0, sizeof r); r.key = wkey(); r.nonce = 1; r.target = (uint8_t)(g_my ^ 1); r.sum = 9; r.star = 1;
        send_ctl(kSpCtlRed, &r, sizeof r);
    }
}

// ---------------------------------------------------------------------------
// Level lifecycle and the frame.
// ---------------------------------------------------------------------------
void new_level() {
    EarlyRed keep = g_early;
    if (keep.on && !(keep.w.key.level == g_lvl && keep.w.key.act == g_act)) keep.on = false;
    std::memset(g_blk, 0, sizeof g_blk);
    std::memset(g_bo, 0, sizeof g_bo);
    std::memset(g_lastbseq, 0, sizeof g_lastbseq);
    std::memset(g_net, 0, sizeof g_net);
    std::memset(g_lnk, 0, sizeof g_lnk);
    std::memset(g_lnk_gone, 0, sizeof g_lnk_gone);
    std::memset(g_ack, 0, sizeof g_ack);
    std::memset(g_in, 0, sizeof g_in);
    std::memset(g_job, 0, sizeof g_job);
    std::memset(g_rate, 0, sizeof g_rate);
    g_nred = 0; g_red_prev = 0; g_red_armed = false; g_red_snap_have = false; g_red_snap_sum = 0; g_red_star = 0;
    g_frames = 0; g_armed = false; g_census_on = false; g_wm = 0; g_census_lines = 0;
    for (int i = 0; i < g_nkb; ++i) g_kb[i].done = false;
    for (int i = 0; i < g_nkr; ++i) g_kr[i].done = false;
    for (int i = 0; i < g_nks; ++i) g_ks[i].done = false;
    for (int i = 0; i < g_nforge; ++i) g_forge[i].done = false;
    g_early = keep;
}

void on_latched(int level, int act) {
    if (!coop_world_active()) return;
    read_env();
    g_my = comms_hub_my_seat();
    g_lvl = level; g_act = act; g_have_key = true;
    ++g_gen;
    new_level();
    if (g_log) std::fprintf(stderr, "[coopspawn] latched level=%d act=%d wall=%lld\n", level, act, wall_ms());
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
    if (seat >= 0 && seat < kCoopMaxSeats) {
        std::memset(g_lastbseq[seat], 0, sizeof g_lastbseq[seat]);
        g_job[seat].on = false;
    }
}

void register_handlers() {
    if (g_registered) return;
    g_registered = true;
    const bool a = coop_ctl_register(kSpCtlRed, kSpCtlLast, on_ctl, nullptr);
    const bool b = coop_world_register(kSpWorldSpawn, kSpWorldLast, on_world, nullptr);
    std::fprintf(stderr, "[coopspawn] spawn module on: ctl %s, world %s\n", a ? "ok" : "REFUSED", b ? "ok" : "REFUSED");
}

void on_frame(unsigned now) {
    read_env();
    if (!coop_world_active()) return;
    register_handlers();
    g_my = comms_hub_my_seat();
    int lv = -1, ac = 0;
    if (!coop_group_key(&lv, &ac) || !coop_group_established()) return;
    if (!g_have_key || lv != g_lvl || ac != g_act) {        // a level change that missed the latch
        g_lvl = lv; g_act = ac; g_have_key = true; ++g_gen; new_level();
    }
    if (!my_player()) return;
    ++g_frames;

    // the owner's red-coin sum, from a sender that was not yet a member
    if (g_early.on && is_member(g_early.seat)) {
        g_early.on = false;
        if (g_early.w.key.level == g_lvl && g_early.w.key.act == g_act) apply_red_snap(g_early.w);
    }

    // arm: the level has settled and a joiner has been told (or nobody could tell it)
    if (!g_armed && g_frames >= kSettleFrames &&
        (g_red_snap_have || coop_group_i_am_owner() || g_frames >= kRedWait)) {
        g_armed = true;
        if (g_red_snap_have) {
            const int have = red_sum();
            if (g_red_snap_sum > have) red_bump(g_red_snap_sum - have, g_red_star, "snapshot");
        }
        red_rebase();
        g_red_armed = true;
        if (g_log) std::fprintf(stderr, "[coopspawn] armed frame=%u redsum=%d snap=%d wall=%lld\n", g_frames, red_sum(), g_red_snap_have ? g_red_snap_sum : -1, wall_ms());
    }
    if (!g_census_on && g_frames >= kSettleFrames) {         // everything the level placed or started with is not "new"
        g_census_on = true;
        unsigned m = 0;
        for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) if (a_uid(a) > m) m = a_uid(a);
        g_wm = m;
    }

    if (g_frames >= 3) scan(now);
    drain_in(now);

    // proof knobs
    for (int i = 0; i < g_nkb; ++i)
        if (!g_kb[i].done && g_armed && g_frames >= g_kb[i].frame) { g_kb[i].done = true; knob_block(g_kb[i]); }
    for (int i = 0; i < g_nkr; ++i)
        if (!g_kr[i].done && g_armed && g_frames >= g_kr[i].frame) { g_kr[i].done = true; knob_red(g_kr[i]); }
    for (int i = 0; i < g_nks; ++i)
        if (!g_ks[i].done && g_armed && g_frames >= g_ks[i].frame) { g_ks[i].done = true; knob_star(g_ks[i]); }
    for (int i = 0; i < g_nforge; ++i)
        if (!g_forge[i].done && g_armed && g_frames >= g_forge[i].frame) { g_forge[i].done = true; knob_forge(g_forge[i].name); }

    // outgoing
    send_blocks(now);
    send_net(now);
    if (coop_group_i_am_owner())
        for (Job &j : g_job) {
            if (!j.on || (int)(now - j.next) < 0) continue;
            if (!is_member(j.seat)) { if ((unsigned)(now - j.born) > 5000u) j.on = false; continue; }
            serve_job(j);
            ++j.sent;
            if (j.sent >= 3) j.on = false; else j.next = now + (j.sent == 1 ? kJobMs : 400u);
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

CoopSpawnStats coop_spawn_stats() { return g_st; }

bool coop_spawn_announce(void *local_actor, int cls, unsigned param, const int pos[3], int area) {
    if (!coop_world_active() || !coop_group_established() || !g_have_key || g_my < 0) return false;
    if (!is_ann_cls((unsigned)cls) || !pos || !sp_param_ok(cls, param) || area < -1 || area > 7) return false;
    for (int i = 0; i < 3; ++i) if (!sp_pos_ok(pos[i])) return false;
    if (local_actor) {
        char *a = (char *)local_actor;
        for (const Net &n : g_net) if (n.used && n.a == a && n.uid == a_uid(a)) return true;     // already announced
    }
    return register_local((char *)local_actor, cls, param, pos, area);
}

void *coop_spawn_and_announce(int cls, unsigned param, const int pos[3], int area) {
    if (!pos) return nullptr;
    char *a = _ZN8dActor_c5SpawnEjjRK7Vector3PK10Vector3_16as((unsigned)cls, param, pos, nullptr, area, -1);
    if (!a) return nullptr;
    if (cls == 179) {
        // a Silver Star needs its marker, the way the brick block makes the pair
        char *m = _ZN8dActor_c5SpawnEjjRK7Vector3PK10Vector3_16as(0xb4, 0x50, pos, nullptr, area, -1);
        if (m) { *(unsigned *)(a + 0x434) = a_uid(m); LinkSilverStarAndStarMarker(m, a); }
    }
    coop_spawn_announce(a, cls, param, pos, area);
    return a;
}

}  // namespace port
