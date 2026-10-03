// ARENA MODE, the referee client, implemented. Run mp1, lane STARREF1 (plan A3).
// Read hal/vs_arena_ref.h first for what this layer is and who calls it.
//
// ===========================================================================
// THE SHAPE OF IT
// ===========================================================================
//
// Every console runs the ROM's own VS arena: the ROM's star actors, markers,
// bodies and HUD are the picture. The referee in the relay (referee.py) decides
// what two players could disagree about. So this file never lets the ROM decide
// those things on its own; it asks the ROM what happened on THIS screen, turns
// that into a claim, and applies what the referee says came of it.
//
//   STAR ORDER    The ROM wakes a marker when its star id equals
//                 order[progress] (data_0209f344 / data_0209f208). In an arena
//                 the order is a 256-byte host buffer of 0xFF that the
//                 referee's STAR_WAKE fills one slot at a time and the progress
//                 byte is the referee's, so every console wakes the same star.
//   MARKER GLASS  A marker whose cylinder took my attack is NOT broken here: the
//                 hit is a MARKER_BREAK claim, and the glass breaks (the ROM's
//                 own StarMarker::Collect) when the referee's MARKER_BROKEN
//                 arrives, on every console, mine included. The console that
//                 broke it owns the star's flight (the ROM's own physics) and
//                 streams it as SYNO; the others place their copy of the star
//                 from those frames.
//   COLLECT       The ROM's collect (func_ov002_020e8ef0) runs at once on my
//                 screen: sparkle, sound, the Yoshi in the star pose, the ROM's
//                 own provisional state 5. That IS the "grab" on my screen. The
//                 state-5 body (dispatch entry 5, wrapped here) HOLDS the star
//                 there until the referee's verdict: taken by me = the ROM body
//                 runs on (celebration); taken by somebody else = my Yoshi is
//                 released and the star does a short fly-off to the winner.
//   LEDGER        data_0209f310 mirrors the referee's scores every frame, so a
//                 provisional +1 never survives to the HUD and no console can
//                 count a star the referee did not give it.
//   HITS          A puppet body whose cylinder took MY attack is a hit on my
//                 screen: a HIT claim, and the pair pass's flags are cleared so
//                 the ROM never resolves it by itself. The victim's own console
//                 calls the ROM's Player::Hurt on its own body when the verdict
//                 names it. The star a hit costs is the referee's SPILL event;
//                 the ROM's own drop (func_ov002_020d94cc inside St_Hurt_Main)
//                 is muted by holding my own score at zero for that call.
//   THE END       Only the referee ends a match (MATCH_OVER). The ROM's own
//                 time-up is held until it arrives.
//
// Nothing here runs unless arena mode is installed in its relay shape.

#include "vs_arena_ref.h"

#include "comms_hub.h"
#include "vs_arena.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace arena_wire;

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250[];
extern unsigned char data_0209f2d8;
extern signed char data_0209f310[];
extern unsigned char data_0209f208;
extern unsigned char *data_0209f344;
extern unsigned char data_0209f204;
extern unsigned char data_0209fc5c[];

char *_ZN8dActor_c10FindWithIDEj(unsigned id);
char *_ZN8dActor_c15FindWithActorIDEjPS_(unsigned id, char *prev);
void _ZN7fBase_c18MarkForDestructionEv(char *c);
void LinkSilverStarAndStarMarker(char *a, char *b);
void func_ov002_020e7e58(char *c);
void func_ov002_020e9448(unsigned char *p);
void _ZN10StarMarker7CollectEv(void *self);
short GetAngleToCamera(int i);
void _ZN6Player11ChangeStateERNS_5StateE(void *self, void *state);
int _ZN6Player7IsStateERNS_5StateE(void *self, void *state);
int _ZN6Player4HurtERK7Vector3j5Fix12IiEjjj(void *self, const void *v,
        unsigned a, int knock, unsigned d, unsigned e, unsigned f);
char *_ZN8dActor_c5SpawnEjjRK7Vector3PK10Vector3_16as(unsigned a, unsigned b,
        const void *pos, const void *v16, int area, int f);
void Vec3_RotateYAndTranslate(int *out, int *in, short angle, int *src);
void func_ov002_020d8d10(void *self, void *pos);
void func_020105cc(void *thiz, unsigned flags);
void func_02012790(unsigned id);
extern unsigned short data_ov002_020ff29c[];

// the star dispatch table PowerStar::Behavior reads (port/unmatched/
// PowerStar_States.cpp seats its entries; entry 5 is the collect confirm)
struct PortPmfRef { unsigned fn; int delta; };
extern PortPmfRef data_ov002_021109d8[];

// state objects: St_Hurt, St_Walk-or-Wait (the neutral state the ROM returns to
// after a no-control sequence), St_NoControl
extern unsigned char data_ov002_02110094[];
extern unsigned char data_ov002_0211013c[];
extern unsigned char data_ov002_0211022c[];
}  // extern "C"

namespace port {
namespace {

enum : int {
    kClassStar = 0xb2,           // PowerStar (VS)
    kClassMarker = 0xb4,         // StarMarker
    kClassPlayer = 0xbf,
    kClassHud = 0x14e,
    kMaxInst = 256,
    kMaxClaims = 16,
    kStHurt = 7, kStHurtWater = 8, kStElectrocute = 9, kStBurnFire = 10,
    kStBurnLava = 11, kStDeadPit = 13,
    kAttackMask = 0x27fe0,       // F2: the hit flags a punch / kick / pound has
    kMarkerBreakMask = 0x408000, // StarMarker::Behavior's own test
    kStarTouchMask = 0x400000,   // func_ov002_020e930c: a body touched it
    kHoldMs = 500,               // no verdict: treated as denied (4.3)
    kFlyoffFrames = 10,
    kHitRetryFrames = 8,
    kLocalTimeupGraceMs = 3000,
    kReportMs = 10000,
};

struct PClaim {
    bool used;
    unsigned short cid;
    unsigned char kind, target;
    unsigned short aux;
    unsigned stamp, my_frame, ref_frame;
    unsigned first_ms, last_ms;
    int result;                  // 0 pending, 1 named, -1 denied, -2 timeout
    int reason, winner;
    unsigned char inst;          // star instance it concerns (grab / break)
    bool resend;                 // false: no resend (QUIT after the first)
};

struct Inst {
    bool used;
    unsigned char star_id;       // the marker / star id (0..7)
    unsigned char marker;        // destination marker for a spill
    unsigned char state;         // 1 waking, 2 free, 3 taken
    unsigned char owner;         // seat that runs the flight; 0xFF none
    bool spilled;
    unsigned uid;                // the local PowerStar's actor uid, 0 none yet
    bool holding;                // my console grabbed it, verdict pending
    int result;                  // 0 pending, 1 mine, 2 lost, 3 timeout
    int winner;
    unsigned short grab_cid;
    unsigned hold_ms;
    int fly;                     // overrule fly-off frames left
    int fly_from[3], fly_to[3];
    // the flight, as streamed (owner) or as received (the others)
    bool have_pos;
    int pos[3], last_pos[3];
    unsigned rx_ms, rx_frame;
    int still;
    unsigned last_tx;
    unsigned char tx_state;
    unsigned born_ms;
    unsigned t_broken;           // referee ms (my base) the glass was broken
    bool rig_break_sent, rig_race_done;
};

struct Ref {
    bool installed = false;
    bool active = false;         // arena + relay
    bool inited = false;         // the walk-time one-shots done
    int seat = -1;
    int bodies = 0;
    VsRefStats st{};
    // the stream
    unsigned short applied = 0;
    bool adopted = false;
    unsigned snap_mask = 0;
    unsigned char ledger_live = 0;
    signed char ledger[16];
    unsigned short ledger_seq = 0;
    unsigned char match_state = 0;
    unsigned short roster = 0;
    unsigned ever_roster = 0;
    unsigned short roster_now = 0;
    unsigned hdr_t0 = 0, hdr_match_ms = 0;
    unsigned char progress = 0;
    // READY / START
    bool ready_sent = false, ready_ok = false;
    unsigned short ready_cid = 0;
    unsigned ready_last = 0;
    bool session_up = false;
    bool started = false;
    unsigned t0 = 0;             // referee ms of match frame 0, my base
    int first_to_n = 0, time_s = 0, markers = 0;
    unsigned seed = 0;
    int vs_frames = 0;
    bool hud_set = false;
    int hud_units = 0;
    // countdown
    int cd_count = -1;
    bool cd_music = false;
    // the end
    bool over = false;
    VsRefOver over_v{};
    bool local_timeup = false;
    unsigned local_timeup_ms = 0;
    bool over_printed = false;
    // claims
    PClaim claims[kMaxClaims];
    unsigned short next_cid = 1;
    // stars
    Inst inst[kMaxInst];
    unsigned char order[256];
    bool order_installed = false;
    unsigned last_post_ms = 0;
    unsigned char syno_rr = 0;
    // hits
    unsigned last_hit_claim_frame[16];
    unsigned hit_applied_frame = 0;
    int prev_state = 15;
    // wraps
    unsigned star5_orig = 0;
    unsigned hurt_orig = 0;
    VsRefEventSink sink = nullptr;
    unsigned last_report = 0;
    bool log = false;
    // lab rigs (SM64DS_ARENA_RIG)
    int rig = 0;                 // 0 off, 1 race, 2 hit
    unsigned rig_seed = 1;
    int rig_n = 0;
    unsigned rig_next = 0;       // referee ms of the next scripted step
    int rig_press_frames = 0;
};
Ref g;

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------
inline unsigned now_ms() { return comms_hub_now_ms(); }

bool env_on(const char *n) {
    const char *s = std::getenv(n);
    return s && *s && !(s[0] == '0' && s[1] == 0);
}

int env_int(const char *n, int def, int lo, int hi) {
    const char *s = std::getenv(n);
    if (!s || !*s) return def;
    char *end = nullptr;
    const long v = std::strtol(s, &end, 10);
    if (end == s || *end || v < lo || v > hi) return def;
    return (int)v;
}

inline unsigned char *u8p(char *c, int off) { return (unsigned char *)(c + off); }
inline unsigned short &u16r(char *c, int off) { return *(unsigned short *)(c + off); }
inline unsigned &u32r(char *c, int off) { return *(unsigned *)(c + off); }
inline int &s32r(char *c, int off) { return *(int *)(c + off); }
inline short &s16r(char *c, int off) { return *(short *)(c + off); }
inline unsigned actor_uid(const char *c) { return *(const unsigned *)(c + 4); }
inline unsigned actor_class(const char *c) { return *(const unsigned short *)(c + 0xc); }

char *my_body() {
    if (g.seat < 0 || g.seat >= kArenaSeats) return nullptr;
    return (char *)data_0209f394[g.seat];
}

char *body_of(int seat) {
    if (seat < 0 || seat >= kArenaSeats) return nullptr;
    return (char *)data_0209f394[seat];
}

// every live actor of one class, by the ROM's own list walk (the same one the
// ROM's FindWithActorID callers use)
template <typename F>
void each_class(unsigned cls, F f) {
    char *a = nullptr;
    for (int guard = 0; guard < 64; ++guard) {
        a = _ZN8dActor_c15FindWithActorIDEjPS_(cls, a);
        if (!a) break;
        f(a);
    }
}

char *find_class(unsigned cls, char *prev) {
    return _ZN8dActor_c15FindWithActorIDEjPS_(cls, prev);
}

// my referee-clock estimate right now, my base
bool ref_now(unsigned *out) {
    int rtt;
    return vs_arena_referee_now(out, &rtt);
}

// the referee-clock stamp of a past local instant
bool stamp_for(unsigned local_ms, unsigned *stamp) {
    unsigned r;
    if (!ref_now(&r)) return false;
    const unsigned now = now_ms();
    *stamp = r - (now - local_ms);
    return true;
}

int popcount16(unsigned m) {
    int n = 0;
    for (; m; m &= m - 1) ++n;
    return n;
}

unsigned short seq_next(unsigned short s) { return s == 0xFFFF ? 1 : (unsigned short)(s + 1); }
unsigned short seq_prev(unsigned short s) { return s <= 1 ? 0xFFFF : (unsigned short)(s - 1); }
bool seq_old(unsigned short applied, unsigned short s) {
    const unsigned short d = (unsigned short)(applied - s);
    return d < 0x8000u;           // s is at or before applied
}

// ---------------------------------------------------------------------------
// CLAIMS
// ---------------------------------------------------------------------------
PClaim *claim_by_id(unsigned short cid) {
    for (auto &c : g.claims)
        if (c.used && c.cid == cid) return &c;
    return nullptr;
}

PClaim *claim_slot() {
    for (auto &c : g.claims)
        if (!c.used) return &c;
    // recycle the oldest finished one
    PClaim *best = nullptr;
    for (auto &c : g.claims)
        if (c.result != 0 && (!best || c.first_ms < best->first_ms)) best = &c;
    return best;
}

void send_claim(PClaim &c) {
    RefClaim m;
    std::memset(&m, 0, sizeof m);
    m.tag = kTagE;
    m.version = 1;
    m.seat = 0xFF;                // the hub stamps it
    m.kind = c.kind;
    m.target = c.target;
    m.claim_id = c.cid;
    m.aux = c.aux;
    m.stamp = c.stamp;
    m.my_frame = c.my_frame;
    m.ref_frame = c.ref_frame;
    comms_hub_send(&m, (int)sizeof m);
    c.last_ms = now_ms();
}

unsigned short new_cid() {
    unsigned short c = g.next_cid;
    g.next_cid = seq_next(g.next_cid);
    return c;
}

const char *claim_name(int k) {
    static const char *n[] = {"?", "READY", "MARKER_BREAK", "GRAB", "HIT",
                              "HURT_SELF", "PIT", "QUIT", "EAT", "SPIT",
                              "SWALLOW", "ESCAPE"};
    return k >= 1 && k <= 11 ? n[k] : "?";
}

PClaim *make_claim(unsigned char kind, unsigned char target, unsigned short aux,
                  unsigned stamp, unsigned my_frame, unsigned ref_frame) {
    PClaim *c = claim_slot();
    if (!c) return nullptr;
    std::memset(c, 0, sizeof *c);
    c->used = true;
    c->cid = new_cid();
    c->kind = kind;
    c->target = target;
    c->aux = aux;
    c->stamp = stamp;
    c->my_frame = my_frame;
    c->ref_frame = ref_frame;
    c->first_ms = now_ms();
    c->resend = true;
    ++g.st.claims_sent;
    send_claim(*c);
    if (g.log)
        std::fprintf(stderr, "[vsarena-ref] claim %s id=%u target=%u aux=%u "
                     "stamp=%u my_frame=%u ref_frame=%u\n", claim_name(kind),
                     c->cid, target, aux, stamp, my_frame, ref_frame);
    return c;
}

void pump_claims() {
    const unsigned now = now_ms();
    for (auto &c : g.claims) {
        if (!c.used || c.result != 0) continue;
        if (now - c.first_ms >= (unsigned)kVerdictTimeoutMs &&
            c.kind != CLAIM_READY) {
            c.result = -2;
            continue;
        }
        if (c.resend && now - c.last_ms >= (unsigned)kClaimResendMs) {
            send_claim(c);
            ++g.st.claims_resent;
        }
    }
}

void finish_claim(unsigned short cid, int result, int reason, int winner) {
    PClaim *c = claim_by_id(cid);
    if (!c || c->result > 0) return;
    c->result = result;
    c->reason = reason;
    c->winner = winner;
    ++g.st.verdicts;
    if (result < 0) ++g.st.denied;
}

// ---------------------------------------------------------------------------
// THE ORDER BUFFER + the ROM's progress byte
// ---------------------------------------------------------------------------
void install_order() {
    if (g.order_installed) return;
    std::memset(g.order, 0xFF, sizeof g.order);
    data_0209f344 = g.order;
    g.order_installed = true;
}

// ---------------------------------------------------------------------------
// ACTOR LOOKUPS
// ---------------------------------------------------------------------------
char *marker_by_id(int id) {
    char *hit = nullptr;
    each_class(kClassMarker, [&](char *a) {
        if (!hit && *u8p(a, 0x1d9) == id && *u8p(a, 0x1d8) != 0) hit = a;
    });
    return hit;
}

int count_markers() {
    int top = -1;
    each_class(kClassMarker, [&](char *a) {
        if (*u8p(a, 0x1d8) != 0 && *u8p(a, 0x1d9) < 8 &&
            *u8p(a, 0x1d9) > top) top = *u8p(a, 0x1d9);
    });
    return top + 1;
}

char *star_by_uid(unsigned uid) {
    return uid ? _ZN8dActor_c10FindWithIDEj(uid) : nullptr;
}

bool uid_taken(unsigned uid) {
    for (auto &i : g.inst)
        if (i.used && i.uid == uid) return true;
    return false;
}

// the level-placed PowerStar of a star id, still dormant (state 9)
char *dormant_star(int star_id) {
    char *hit = nullptr;
    each_class(kClassStar, [&](char *a) {
        if (hit) return;
        if (*u8p(a, 0x49d) != star_id) return;
        if (s32r(a, 0x440) != 9) return;
        if (uid_taken(actor_uid(a))) return;
        hit = a;
    });
    return hit;
}

Inst *inst_by_uid(unsigned uid) {
    if (!uid) return nullptr;
    for (auto &i : g.inst)
        if (i.used && i.uid == uid) return &i;
    return nullptr;
}

Inst *inst_by_id(unsigned id) {
    if (id == 0 || id >= (unsigned)kMaxInst) return nullptr;
    return g.inst[id].used ? &g.inst[id] : nullptr;
}

void ensure_star_uid(Inst &i) {
    if (i.uid || i.spilled) return;
    if (char *s = dormant_star(i.star_id)) i.uid = actor_uid(s);
}

// ---------------------------------------------------------------------------
// STAR EFFECTS
// ---------------------------------------------------------------------------
void unlink_and_destroy_star(char *s) {
    if (!s) return;
    char *m = _ZN8dActor_c10FindWithIDEj(u32r(s, 0x434));
    if (m) LinkSilverStarAndStarMarker(m, nullptr);
    func_ov002_020e7e58(s);
    _ZN7fBase_c18MarkForDestructionEv(s);
}

void break_marker(int inst_id, int star_id, int breaker) {
    char *m = marker_by_id(star_id);
    if (!m) return;
    if (*u8p(m, 0x1db) & 1) return;                 // already broken
    // the ROM's star pops "away from the breaker's camera"; the flight is the
    // owner's (the breaker's console runs it), so any body will do as the hit
    // actor on the others, and mine is always a valid camera index.
    (void)breaker;
    s32r(m, 0x1d0) = (int)(size_t)my_body();
    _ZN10StarMarker7CollectEv(m);
    if (g.log)
        std::fprintf(stderr, "[vsarena-ref] marker %d broken (inst %d, breaker "
                     "seat %d)\n", star_id, inst_id, breaker);
}

// a dropped star: the ROM's own spawn (func_ov002_020d94cc's) with the
// referee's marker instead of its RNG
char *spawn_spill_star(int marker_id, const int *origin, int victim_seat) {
    int pos[3] = { origin[0], origin[1] + 0x82000, origin[2] };
    char *vb = body_of(victim_seat);
    const int area = vb ? (signed char)*u8p(vb, 0xcc) : 0;
    char *s = _ZN8dActor_c5SpawnEjjRK7Vector3PK10Vector3_16as(
        kClassStar, 0x10, pos, nullptr, area, -1);
    if (!s) return nullptr;
    if (vb) {
        s16r(s, 0x92) = s16r(vb, 0x92);
        s16r(s, 0x94) = s16r(vb, 0x94);
        s16r(s, 0x96) = s16r(vb, 0x96);
        s32r(s, 0x98) = s32r(vb, 0x98);
    }
    char *m = marker_by_id(marker_id);
    if (m) {
        u32r(s, 0x434) = actor_uid(m);
        LinkSilverStarAndStarMarker(m, s);
    }
    u16r(s, 0x4a2) |= 0x40;
    return s;
}

// ---------------------------------------------------------------------------
// EVENTS
// ---------------------------------------------------------------------------
const char *reason_name(int r) {
    static const char *n[] = {"?", "STALE", "GONE", "LOST", "RANGE", "STATE",
                              "INVULN", "MISMATCH", "UNSUPPORTED", "BAD",
                              "TOO_OLD", "SELF"};
    return r >= 1 && r <= 11 ? n[r] : "?";
}

void session_up(const char *why) {
    if (g.session_up) return;
    g.session_up = true;
    std::fprintf(stderr, "[vsarena] session up players=%d (%s)\n",
                 g.roster ? popcount16(g.roster) : g.bodies, why);
    std::fflush(stderr);
}

void on_start(const RefEvent &e) {
    if (g.started || g.over) return;
    g.started = true;
    g.t0 = e.t;
    g.progress = 0;
    g.first_to_n = e.b;
    g.time_s = e.c;
    g.roster = (unsigned short)e.d;
    g.markers = e.e;
    g.seed = (unsigned)e.x;
    ++g.st.events_applied;
    session_up("START");
    std::fprintf(stderr, "[vsarena] START t0=%u first_to=%d time=%ds roster=0x%04x "
                 "markers=%d seed=%u map=%d\n", e.t, e.b, e.c, e.d, e.e,
                 (unsigned)e.x, e.y);
}

void on_wake(const RefEvent &e) {
    Inst *i = &g.inst[e.a ? e.a : 1];
    if (e.a == 0) return;
    std::memset(i, 0, sizeof *i);
    i->used = true;
    i->star_id = e.b;
    i->marker = e.b;
    i->state = 1;
    i->owner = 0xFF;
    i->born_ms = now_ms();
    if (e.c < sizeof g.order) {
        g.order[e.c] = e.b;
        g.progress = (unsigned char)e.c;
        data_0209f208 = (unsigned char)e.c;
    }
    ensure_star_uid(*i);
    std::fprintf(stderr, "[vsarena-ref] STAR_WAKE inst=%d star=%d progress=%d\n",
                 e.a, e.b, e.c);
}

void on_marker_broken(const RefEvent &e) {
    Inst *i = inst_by_id(e.a);
    if (!i) {
        // a console that missed the wake: make the entry from the event
        if (e.a == 0) return;
        i = &g.inst[e.a];
        std::memset(i, 0, sizeof *i);
        i->used = true;
        i->star_id = e.b;
        i->marker = e.b;
    }
    i->state = 2;
    i->owner = e.seat;
    i->born_ms = now_ms();
    i->t_broken = e.t;
    ensure_star_uid(*i);
    break_marker(e.a, e.b, e.seat);
    if (e.seat == g.seat) finish_claim(e.c, 1, 0, 0xFF);
}

void on_star_taken(const RefEvent &e) {
    Inst *i = inst_by_id(e.a);
    ++g.st.stars_taken;
    g.progress = (unsigned char)e.e;
    data_0209f208 = (unsigned char)e.e;
    if (e.seat == g.seat) finish_claim(e.c, 1, 0, 0xFF);
    std::fprintf(stderr, "[vsarena-ref] STAR_TAKEN inst=%d winner=%d score=%d "
                 "claim=%d progress=%d stamp=%u\n", e.a, e.seat, e.b, e.c, e.e,
                 e.t);
    if (!i) return;
    i->state = 3;
    if (i->holding) {
        if (e.seat == g.seat) { i->result = 1; }
        else { i->result = 2; i->winner = e.seat; }
        return;
    }
    // everybody else: the star is gone
    if (char *s = star_by_uid(i->uid)) unlink_and_destroy_star(s);
}

void on_deny(const RefEvent &e) {
    if (e.seat != g.seat) return;
    const int kind = e.a, reason = e.b, cid = e.c, winner = e.d;
    std::fprintf(stderr, "[vsarena-ref] DENY kind=%d reason=%s(%d) claim=%d "
                 "winner=%d target=%d\n", kind, reason_name(reason), reason, cid,
                 winner, e.e);
    finish_claim((unsigned short)cid, -1, reason, winner);
    if (kind == CLAIM_GRAB)
        for (auto &i : g.inst)
            if (i.used && i.holding && i.grab_cid == cid) {
                i.result = 2;
                i.winner = winner;
            }
    if (kind == CLAIM_HIT) ++g.st.hits_denied;
}

void on_accepted(const RefEvent &e) {
    if (e.seat != g.seat) return;
    finish_claim((unsigned short)e.c, 1, 0, 0xFF);
    if (e.a == CLAIM_READY) {
        g.ready_ok = true;
        session_up("READY accepted");
    }
}

void apply_hit_on_me(const RefEvent &e);
void apply_hit_effects(const RefEvent &e) {
    ++g.st.hits_landed;
    if (e.seat == g.seat) apply_hit_on_me(e);
    if (e.a == g.seat) finish_claim(e.c, 1, 0, 0xFF);
    std::fprintf(stderr, "[vsarena-ref] HIT victim=%d attacker=%d trade=%d "
                 "claim=%d flags=0x%x spilled=%d\n", e.seat, e.a, e.b, e.c,
                 (unsigned)e.d << 5, e.e);
}

void on_spill(const RefEvent &e) {
    ++g.st.spills;
    Inst *i = &g.inst[e.a ? e.a : 1];
    if (e.a == 0) return;
    std::memset(i, 0, sizeof *i);
    i->used = true;
    i->spilled = true;
    i->star_id = 0xFF;
    i->marker = e.b;
    i->state = 2;
    i->born_ms = now_ms();
    // who streams this flight: the seat that lost the star; a leaver's stars
    // are launched by the lowest seat left in the match
    int owner = e.seat;
    if (e.c == 2 || e.seat == 0xFF) {
        owner = 0xFF;
        for (int k = 0; k < kArenaSeats; ++k)
            if ((g.roster >> k & 1u) && k != e.seat) { owner = k; break; }
    }
    i->owner = (unsigned char)owner;
    const int origin[3] = { e.x, e.y, e.z };
    char *s = spawn_spill_star(e.b, origin, e.seat);
    if (s) i->uid = actor_uid(s);
    std::fprintf(stderr, "[vsarena-ref] SPILL inst=%d seat=%d marker=%d cause=%d "
                 "score=%d owner=%d spawned=%d\n", e.a, e.seat, e.b, e.c, e.d,
                 owner, s ? 1 : 0);
}

void on_match_over(const RefEvent &e) {
    if (g.over) return;
    g.over = true;
    g.over_v.reason = e.a;
    g.over_v.winners = e.b;
    g.over_v.winners_mask = e.c;
    g.over_v.roster_mask = g.roster_now;
    g.over_v.seen_mask = (int)(g.ever_roster | g.roster_now);
    std::memcpy(g.over_v.scores, g.ledger, 16);
    g.over_v.seq = e.seq;
    g.over_v.local_timeout = false;
    std::fprintf(stderr, "[vsarena] MATCH OVER ref reason=%d winners=0x%04x "
                 "count=%d scores=%d,%d,%d,%d seq=%u\n", e.a, e.c, e.b,
                 g.ledger[0], g.ledger[1], g.ledger[2], g.ledger[3], e.seq);
}

void on_star_state(const RefEvent &e) {
    // snapshot record: instance e.a is in state e.b (1 waking, 2 free)
    if (e.a == 0) return;
    Inst *i = &g.inst[e.a];
    if (i->used) return;
    std::memset(i, 0, sizeof *i);
    i->used = true;
    i->state = e.b;
    i->marker = (unsigned char)e.c;
    i->star_id = (unsigned char)e.d;
    i->owner = e.seat;
    if (e.b == 1 && g.progress < sizeof g.order) g.order[g.progress] = (unsigned char)e.d;
    ensure_star_uid(*i);
}

void apply_event(const RefEvent &e, bool directed) {
    switch (e.type) {
    case EV_START:          on_start(e); break;
    case EV_STAR_WAKE:      on_wake(e); break;
    case EV_MARKER_BROKEN:  on_marker_broken(e); break;
    case EV_STAR_TAKEN:     on_star_taken(e); break;
    case EV_DENY:           on_deny(e); break;
    case EV_HIT:            apply_hit_effects(e); break;
    case EV_CLASH:
        if (e.a == g.seat) finish_claim(e.c, 1, 0, 0xFF);
        std::fprintf(stderr, "[vsarena-ref] CLASH victim=%d attacker=%d\n",
                     e.seat, e.a);
        break;
    case EV_SPILL:          on_spill(e); break;
    case EV_MATCH_OVER:     on_match_over(e); break;
    case EV_LEAVE:
        std::fprintf(stderr, "[vsarena-ref] LEAVE seat=%d why=%d spilled=%d\n",
                     e.seat, e.a, e.d);
        break;
    case EV_JOIN:
        std::fprintf(stderr, "[vsarena-ref] JOIN seat=%d mid=%d\n", e.seat, e.a);
        break;
    case EV_STAR_STATE:     on_star_state(e); break;
    case EV_ACCEPTED:       on_accepted(e); break;
    case EV_EATEN:
    case EV_RELEASED:
        if (e.a == g.seat && e.type == EV_EATEN) finish_claim(e.c, 1, 0, 0xFF);
        break;
    default: break;
    }
    ++g.st.events_applied;
    if (g.sink) g.sink(&e, g.seat, directed);
}

void apply_frame(const RefFrame &f) {
    ++g.st.frames_rx;
    if (f.n_events > kSynrEvents) return;
    g.match_state = f.match_state;
    g.hdr_t0 = f.t0;
    g.hdr_match_ms = f.match_ms;
    if (!g.started || !g.roster) g.roster = f.roster;
    g.roster_now = f.roster;
    g.ever_roster |= f.roster;
    // the ledger: the newest header wins
    if (!g.ledger_live || !seq_old(f.newest_seq, g.ledger_seq) ||
        f.newest_seq == g.ledger_seq) {
        std::memcpy(g.ledger, f.scores, 16);
        g.ledger_seq = f.newest_seq;
        g.ledger_live = 1;
    }
    const bool snapshot = (f.flags & SYNR_F_SNAPSHOT) != 0;
    // directed records (seq 0): DENY, ACCEPTED, snapshot records
    for (int k = f.n_events - 1; k >= 0; --k)
        if (f.ev[k].seq == 0) apply_event(f.ev[k], true);
    if (snapshot) {
        const int idx = (f.part >> 4) & 0xF, cnt = f.part & 0xF;
        if (cnt) {
            g.snap_mask |= 1u << idx;
            if (g.snap_mask == (1u << cnt) - 1u) {
                g.snap_mask = 0;
                g.applied = f.newest_seq;
                g.adopted = true;
                ++g.st.gaps;
                std::fprintf(stderr, "[vsarena-ref] snapshot applied: seq now %u\n",
                             g.applied);
            }
        }
        return;
    }
    // the stream: oldest first
    int oldest = -1;
    for (int k = f.n_events - 1; k >= 0; --k)
        if (f.ev[k].seq != 0) { oldest = k; break; }
    if (oldest < 0) return;
    if (!g.adopted) {
        // the first frame this console sees: take the stream from its oldest
        // event (anything before is another seat's JOIN)
        g.applied = seq_prev(f.ev[oldest].seq);
        g.adopted = true;
    }
    bool progress = true;
    while (progress) {
        progress = false;
        for (int k = f.n_events - 1; k >= 0; --k) {
            const RefEvent &e = f.ev[k];
            if (e.seq == 0) continue;
            if (e.seq == seq_next(g.applied)) {
                apply_event(e, false);
                g.applied = e.seq;
                progress = true;
            } else if (seq_old(g.applied, e.seq)) {
                if (e.seq != g.applied) ++g.st.events_dup;
            }
        }
    }
    if (!seq_old(g.applied, f.newest_seq)) {
        ++g.st.gaps;     // events between were missed: the referee resends
    }
    g.st.applied_seq = g.applied;
}

void process_synr() {
    unsigned char buf[kSynrBytes];
    int seat;
    unsigned t;
    int n;
    unsigned short before = g.applied;
    while ((n = comms_hub_recv_t(kHubArenaRef, buf, (int)sizeof buf, &seat,
                                 &t)) > 0) {
        if (n != kSynrBytes) continue;
        RefFrame f;
        std::memcpy(&f, buf, sizeof f);
        if (f.tag != kTagR || f.version != 1 || f.seat != kRefereeMark) continue;
        apply_frame(f);
    }
    if (g.applied != before || (g.adopted && !g.ready_sent)) {
        vs_arena_set_ack(g.applied);
        vs_arena_send_now();
    }
}

// ---------------------------------------------------------------------------
// READY
// ---------------------------------------------------------------------------
void read_hud_clock() {
    char *hud = find_class(kClassHud, nullptr);
    if (!hud) return;
    unsigned short &units = u16r(hud, 0x60);
    if (!g.hud_set) {
        g.hud_set = true;
        const int ov = env_int("SM64DS_ARENA_TIME_S", 0, 3, 3600);
        if (ov) {
            units = (unsigned short)((ov + 2) / 3);
            std::fprintf(stderr, "[vsarena] SM64DS_ARENA_TIME_S=%d: the match "
                         "clock starts at %u units (3 s each)\n", ov,
                         (unsigned)units);
        }
    }
    g.hud_units = units;
}

void maybe_send_ready() {
    if (g.ready_ok || g.started || g.over) return;
    if (g.vs_frames < 8) return;
    char *me = my_body();
    if (!me) return;
    read_hud_clock();
    const unsigned now = now_ms();
    if (!g.ready_sent) {
        const int markers = count_markers();
        if (markers <= 0) return;
        const int map = env_int("SM64DS_VS_MAP", 0, 0, 15);
        const int cap = env_int("SM64DS_VS_PLAYERS", g.bodies, 1, 16);
        const int first_to = env_int("SM64DS_VS_STAR_TARGET", 0, 0, 99);
        const int time_s = g.hud_units ? g.hud_units * 3 : 0;
        const unsigned digest = (unsigned)std::strtoul(
            std::getenv("SM64DS_SESSION_DIGEST") ? std::getenv("SM64DS_SESSION_DIGEST") : "0",
            nullptr, 10);
        const unsigned rules = (unsigned)first_to | (0u << 8) | ((unsigned)cap << 16) |
                               ((unsigned)(markers > 8 ? 8 : markers) << 24);
        PClaim *c = make_claim(CLAIM_READY, (unsigned char)map,
                              (unsigned short)(time_s > 3600 ? 3600 : time_s),
                              rules, digest, 0);
        if (!c) return;
        g.ready_sent = true;
        g.ready_cid = c->cid;
        g.ready_last = now;
        std::fprintf(stderr, "[vsarena] READY sent: map=%d time=%ds first_to=%d "
                     "capacity=%d markers=%d digest=%u\n", map, time_s, first_to,
                     cap, markers, digest);
        return;
    }
    PClaim *c = claim_by_id(g.ready_cid);
    if (c && c->result < 0 && now - g.ready_last >= 2000) {
        // denied (mismatch, or the referee had not met us yet): ask again
        g.ready_last = now;
        c->result = 0;
        c->first_ms = now;
        send_claim(*c);
    }
}

// ---------------------------------------------------------------------------
// THE STARS, once a frame
// ---------------------------------------------------------------------------
unsigned my_frame_now() { return vs_arena_stats().frame; }

void neutralise_foreign_star_touches() {
    char *me = my_body();
    each_class(kClassStar, [&](char *a) {
        const unsigned flags = u32r(a, 0x130);
        if (!(flags & (kStarTouchMask | 0x8000))) return;
        char *o = _ZN8dActor_c10FindWithIDEj(u32r(a, 0x134));
        if (!o || o == me || actor_class(o) != (unsigned)kClassPlayer) return;
        u32r(a, 0x130) &= ~(unsigned)(kStarTouchMask | 0x8000);
        u32r(a, 0x134) = 0;
    });
}

void marker_pass() {
    char *me = my_body();
    each_class(kClassMarker, [&](char *m) {
        const unsigned char state = *u8p(m, 0x1d8);
        if (state == 0 || state == 2) return;
        const unsigned flags = u32r(m, 0xf4);
        const unsigned other = u32r(m, 0xf8);
        if (!other || !(flags & kMarkerBreakMask)) return;
        char *o = _ZN8dActor_c10FindWithIDEj(other);
        // never let the ROM break the glass itself: the referee orders it
        u32r(m, 0xf4) = 0;
        u32r(m, 0xf8) = 0;
        if (!o || o != me) return;
        if (!(*u8p(m, 0x1db) & 2)) return;          // not awake
        if (*u8p(m, 0x1db) & 1) return;             // already broken
        const int id = *u8p(m, 0x1d9);
        for (int k = 1; k < kMaxInst; ++k) {
            Inst &i = g.inst[k];
            if (!i.used || i.state != 1 || i.star_id != id) continue;
            // one claim per marker until a verdict (the resend carries it)
            for (auto &c : g.claims)
                if (c.used && c.kind == CLAIM_MARKER_BREAK && c.inst == k &&
                    c.result == 0) return;
            unsigned stamp;
            if (!stamp_for(g.last_post_ms ? g.last_post_ms : now_ms(), &stamp))
                return;
            const unsigned short yaw = (unsigned short)GetAngleToCamera(g.seat);
            PClaim *c = make_claim(CLAIM_MARKER_BREAK, (unsigned char)k, yaw,
                                  stamp, my_frame_now(), 0);
            if (c) c->inst = (unsigned char)k;
            return;
        }
    });
}

void puppet_stars() {
    for (int k = 1; k < kMaxInst; ++k) {
        Inst &i = g.inst[k];
        if (!i.used || i.state != 2) continue;
        if (i.owner == g.seat || i.holding || i.fly > 0) continue;
        if (!i.have_pos) continue;
        char *s = star_by_uid(i.uid);
        if (!s) continue;
        const int st = s32r(s, 0x440);
        if (st == 5 || st == 6 || st == 7 || st == 10) continue;   // collected here
        s32r(s, 0x5c) = i.pos[0];
        s32r(s, 0x60) = i.pos[1];
        s32r(s, 0x64) = i.pos[2];
        s32r(s, 0x98) = 0;
        s32r(s, 0xa8) = 0;
    }
}

// the star state-5 body, wrapped (dispatch entry 5)
void star5_orig_call(char *c) {
    ((void (__fastcall *)(void *, void *))(size_t)g.star5_orig)(c, nullptr);
}

void release_my_yoshi() {
    char *me = my_body();
    if (!me) return;
    if (_ZN6Player7IsStateERNS_5StateE(me, data_ov002_0211022c))
        _ZN6Player11ChangeStateERNS_5StateE(me, data_ov002_0211013c);
}

void star5_arena(char *c) {
    Inst *i = inst_by_uid(actor_uid(c));
    char *me = my_body();
    if (!i || !i->holding || (char *)(size_t)u32r(c, 0x438) != me) {
        star5_orig_call(c);
        return;
    }
    if (i->result == 0 && now_ms() - i->hold_ms >= (unsigned)kHoldMs) {
        i->result = 3;                          // no verdict in time: denied
        std::fprintf(stderr, "[vsarena-ref] grab of inst %d: no verdict in %d ms; "
                     "treated as denied\n", (int)(i - g.inst), kHoldMs);
    }
    if (i->result == 0) {
        // THE HOLD: the ROM's own waiting arm (state 5, the Yoshi still in the
        // star pose): the star rides over my head and nothing is decided
        int *src = (int *)(me + 0x5c);
        s32r(c, 0x5c) = src[0];
        s32r(c, 0x60) = src[1] + 0xc8000;
        s32r(c, 0x64) = src[2];
        s32r(c, 0x4b8) = 0;
        s32r(c, 0x4b4) = 0;
        return;
    }
    if (i->result == 1) {
        i->holding = false;
        vs_arena_set_star_lock(false);
        star5_orig_call(c);                     // the ROM's state 5 -> 6
        return;
    }
    // OVERRULED (or timed out). The Yoshi goes back to its pad, the star flies
    // to the winner's head and goes out; with no winner named it is tossed back
    // onto the field the way the ROM's own failure arm does.
    if (i->fly == 0) {
        release_my_yoshi();
        vs_arena_set_star_lock(false);
        char *wb = i->winner >= 0 && i->winner < kArenaSeats ? body_of(i->winner) : nullptr;
        if (wb && i->winner != g.seat) {
            i->fly = kFlyoffFrames;
            for (int k = 0; k < 3; ++k) {
                i->fly_from[k] = s32r(c, 0x5c + 4 * k);
                i->fly_to[k] = s32r(wb, 0x5c + 4 * k);
            }
            i->fly_to[1] += 0xc8000;
            s32r(c, 0x128) |= 1;                // its cylinder off
            std::fprintf(stderr, "[vsarena-ref] OVERRULE: star %d goes to seat %d\n",
                         (int)(i - g.inst), i->winner);
        } else {
            i->holding = false;
            s32r(c, 0x440) = 8;
            s32r(c, 0xa8) = 0x20000;
            s32r(c, 0x98) = 0xc000;
            func_ov002_020e9448((unsigned char *)c);
            s32r(c, 0x128) &= ~1;
            std::fprintf(stderr, "[vsarena-ref] DENIED grab of star %d (reason %s): "
                         "tossed back\n", (int)(i - g.inst), "n/a");
            return;
        }
    }
    if (i->fly > 0) {
        --i->fly;
        const int total = kFlyoffFrames;
        const int done = total - i->fly;
        for (int k = 0; k < 3; ++k) {
            const long long d = (long long)i->fly_to[k] - i->fly_from[k];
            s32r(c, 0x5c + 4 * k) = i->fly_from[k] + (int)(d * done / total);
        }
        if (i->fly == 0) {
            i->holding = false;
            unlink_and_destroy_star(c);
        }
    }
}

void __fastcall vs_ref_star5_face(void *self, void *) {
    star5_arena((char *)self);
}

// ---------------------------------------------------------------------------
// MY BODY'S EDGES
// ---------------------------------------------------------------------------
void grab_detect() {
    char *me = my_body();
    if (!me) return;
    for (int k = 1; k < kMaxInst; ++k) {
        Inst &i = g.inst[k];
        if (!i.used || i.state != 2 || i.holding || !i.uid) continue;
        char *s = star_by_uid(i.uid);
        if (!s) continue;
        if (s32r(s, 0x440) != 5) continue;
        if ((char *)(size_t)u32r(s, 0x438) != me) continue;
        // the ROM's collect ran on my screen: that is the touch
        const unsigned touch_ms = g.last_post_ms ? g.last_post_ms : now_ms();
        unsigned stamp;
        if (!stamp_for(touch_ms, &stamp)) {
            // no referee clock: nothing can be claimed; give the star back
            std::fprintf(stderr, "[vsarena-ref] grab without a referee clock\n");
            continue;
        }
        PClaim *c = make_claim(CLAIM_GRAB, (unsigned char)k, 0, stamp,
                              my_frame_now(), 0);
        if (!c) continue;
        c->inst = (unsigned char)k;
        i.holding = true;
        i.result = 0;
        i.grab_cid = c->cid;
        i.hold_ms = now_ms();
        vs_arena_set_star_lock(true);
        std::fprintf(stderr, "[vsarena-ref] TOUCH inst=%d seat=%d touch_ms=%u "
                     "frame=%u stamp=%u\n", k, g.seat, touch_ms, my_frame_now(),
                     stamp);
    }
}

void syno_send() {
    char *me = my_body();
    (void)me;
    // one object frame a frame at most (the relay's per-kind cap is 35 / s)
    for (int n = 0; n < kMaxInst; ++n) {
        const int k = 1 + (g.syno_rr + n) % (kMaxInst - 1);
        Inst &i = g.inst[k];
        if (!i.used || i.state != 2 || i.owner != g.seat || i.holding) continue;
        char *s = star_by_uid(i.uid);
        if (!s) continue;
        const int st = s32r(s, 0x440);
        if (st == 5 || st == 6 || st == 7 || st == 10) continue;
        const int pos[3] = { s32r(s, 0x5c), s32r(s, 0x60), s32r(s, 0x64) };
        const unsigned fr = my_frame_now();
        const bool moved = !i.have_pos || pos[0] != i.last_pos[0] ||
                           pos[1] != i.last_pos[1] || pos[2] != i.last_pos[2];
        if (moved) i.still = 0; else ++i.still;
        const bool due = moved || i.still <= 3 || (fr - i.last_tx) >= 30;
        if (!due) continue;
        ObjFrame o;
        std::memset(&o, 0, sizeof o);
        o.tag = kTagO;
        o.version = 1;
        o.seat = 0xFF;
        o.obj_class = 1;
        o.obj_id = (unsigned char)k;
        o.frame = fr;
        for (int a = 0; a < 3; ++a) {
            o.pos[a] = pos[a];
            const int d = i.have_pos ? (pos[a] - i.last_pos[a]) >> 6 : 0;
            o.vel[a] = (short)(d > 32767 ? 32767 : d < -32767 ? -32767 : d);
        }
        o.state = moved ? 1 : 2;
        o.flags = 0;
        if (comms_hub_send(&o, (int)sizeof o)) ++g.st.syno_tx;
        for (int a = 0; a < 3; ++a) i.last_pos[a] = pos[a];
        i.have_pos = true;
        i.last_tx = fr;
        g.syno_rr = (unsigned char)(k % (kMaxInst - 1));
        return;
    }
}

void syno_recv() {
    unsigned char buf[64];
    int seat;
    unsigned t;
    int n;
    while ((n = comms_hub_recv_t(kHubArenaObject, buf, (int)sizeof buf, &seat,
                                 &t)) > 0) {
        if (n != kSynoBytes) continue;
        ObjFrame o;
        std::memcpy(&o, buf, sizeof o);
        if (o.tag != kTagO || o.version != 1 || o.obj_class != 1) continue;
        Inst *i = inst_by_id(o.obj_id);
        if (!i || i->state != 2) continue;
        if (i->owner != 0xFF && i->owner != (unsigned char)seat) continue;
        if (i->owner == g.seat) continue;
        const int lim = 32768 << 12;
        if (o.pos[0] > lim || o.pos[0] < -lim || o.pos[1] > lim || o.pos[1] < -lim ||
            o.pos[2] > lim || o.pos[2] < -lim) continue;
        if (i->have_pos && o.frame <= i->rx_frame && i->rx_frame - o.frame < 300)
            continue;
        i->pos[0] = o.pos[0]; i->pos[1] = o.pos[1]; i->pos[2] = o.pos[2];
        i->have_pos = true;
        i->rx_ms = t;
        i->rx_frame = o.frame;
        ++g.st.syno_rx;
    }
}

// ---------------------------------------------------------------------------
// HITS
// ---------------------------------------------------------------------------
unsigned drawn_frame(int seat) {
    ArenaSeatView v;
    if (!vs_arena_seat_view(seat, &v) || !v.has_state) return 0;
    long long t = (long long)now_ms() - v.clock_offset_ms - 50;
    long long f = t * 30 / 1000;
    ArenaSample s[kArenaRing];
    const int n = vs_arena_samples(seat, s, kArenaRing);
    if (n > 0) {
        const long long newest = s[0].s.frame, oldest = s[n - 1].s.frame;
        if (f > newest) f = newest;
        if (f < oldest) f = oldest;
    }
    return f < 0 ? 0 : (unsigned)f;
}

void hit_detect() {
    char *me = my_body();
    if (!me) return;
    const unsigned fr = my_frame_now();
    for (int k = 0; k < g.bodies && k < kArenaSeats; ++k) {
        if (k == g.seat) continue;
        char *b = body_of(k);
        if (!b) continue;
        const unsigned flags = u32r(b, 0x2f4) & kAttackMask;
        const unsigned other = u32r(b, 0x2f8);
        if (!other || !flags) continue;
        // never let the ROM resolve a hit on a puppet by itself
        u32r(b, 0x2f4) = 0;
        u32r(b, 0x2f8) = 0;
        char *o = _ZN8dActor_c10FindWithIDEj(other);
        if (o != me) continue;
        if ((flags & ~0x1000u) == 0) continue;
        if (vs_arena_body_held(k)) continue;
        if (fr - g.last_hit_claim_frame[k] < (unsigned)kHitRetryFrames &&
            g.last_hit_claim_frame[k]) continue;
        unsigned stamp;
        if (!stamp_for(g.last_post_ms ? g.last_post_ms : now_ms(), &stamp)) continue;
        PClaim *c = make_claim(CLAIM_HIT, (unsigned char)k,
                              (unsigned short)((flags >> 5) & 0xFFFF), stamp,
                              fr, drawn_frame(k));
        if (c) {
            g.last_hit_claim_frame[k] = fr ? fr : 1;
            ++g.st.hits_claimed;
        }
    }
}

void apply_hit_on_me(const RefEvent &e) {
    char *me = my_body();
    if (!me) return;
    const unsigned flags = (unsigned)e.d << 5;
    const unsigned flag = flags & 0x380;
    if (flag) { *u8p(me, 0x6de) = 1; *u8p(me, 0x6df) = 0; }
    int v[3] = { e.x, e.y, e.z };
    char *ab = body_of(e.a);
    const unsigned param = ab ? (unsigned)s32r(ab, 8) : 3u;
    // the ROM's own damage, from the attacker's position
    const int hr = _ZN6Player4HurtERK7Vector3j5Fix12IiEjjj(
        me, v, 1u, 0, 0u, (param + 1) & 0xffu, 1u);
    g.hit_applied_frame = my_frame_now();
    if (!hr) {
        std::fprintf(stderr, "[vsarena-ref] HIT verdict: Player::Hurt refused on my "
                     "body (already hurt?)\n");
        return;
    }
    // ... and the knockback, as func_ov002_020d8360 does after Hurt
    int tr[3] = { 0, 0x50000, 0x64000 };
    int out[3], cp[3];
    Vec3_RotateYAndTranslate(out, (int *)(me + 0x5c), s16r(me, 0x8e), tr);
    cp[0] = out[0]; cp[1] = out[1]; cp[2] = out[2];
    func_ov002_020d8d10(me, cp);
    func_020105cc(me, flags);
    unsigned idx = (unsigned short)(s32r(me, 8) + (int)(param << 2));
    if (*u8p(me, 0x6f9)) idx = 0x11;
    const int t98 = (int)data_ov002_020ff29c[idx] << 3;
    s32r(me, 0x98) = t98;
    *u8p(me, 0x6de) = 1;
    *u8p(me, 0x6df) = 0;
    if (flag) {
        s32r(me, 0xa8) = t98 + 0x20000;
        s32r(me, 0x98) = s32r(me, 0x98) >> 1;
    } else {
        s32r(me, 0xa8) = 0x18000;
    }
}

// St_Hurt's Main, wrapped: the ROM drops a star from inside it (func_ov002_020d94cc
// reads my carried count and refuses at zero); in an arena the referee already
// spilled it, so the count reads zero for that one call.
int __fastcall vs_ref_hurt_main_face(void *self, void *edx) {
    char *b = (char *)self;
    if (!g.active || b != my_body())
        return ((int (__fastcall *)(void *, void *))(size_t)g.hurt_orig)(self, edx);
    const int idx = *u8p(b, 0x6d8);
    const signed char keep = idx < 16 ? data_0209f310[idx] : 0;
    if (idx < 16) data_0209f310[idx] = 0;
    const int r = ((int (__fastcall *)(void *, void *))(size_t)g.hurt_orig)(self, edx);
    if (idx < 16) data_0209f310[idx] = keep;
    return r;
}

void my_state_edges() {
    char *me = my_body();
    if (!me || !g.started || g.over) return;
    int st = vs_arena_state_index(me);
    if (st == 0xFF) return;
    const bool hurt_now = st == kStHurt || st == kStHurtWater ||
                          st == kStElectrocute || st == kStBurnFire ||
                          st == kStBurnLava;
    const bool hurt_before = g.prev_state == kStHurt || g.prev_state == kStHurtWater ||
                             g.prev_state == kStElectrocute ||
                             g.prev_state == kStBurnFire || g.prev_state == kStBurnLava;
    const unsigned fr = my_frame_now();
    if (hurt_now && !hurt_before && fr > g.hit_applied_frame + 6u) {
        // a hazard (chomp, iron ball, lava): always accepted, costs one star
        unsigned stamp;
        if (stamp_for(g.last_post_ms ? g.last_post_ms : now_ms(), &stamp)) {
            make_claim(CLAIM_HURT_SELF, 0xFF, 3, stamp, fr, 0);
            std::fprintf(stderr, "[vsarena-ref] hazard hurt (state %d): HURT_SELF\n", st);
        }
    }
    if (st == kStDeadPit && g.prev_state != kStDeadPit) {
        unsigned stamp;
        if (stamp_for(g.last_post_ms ? g.last_post_ms : now_ms(), &stamp)) {
            make_claim(CLAIM_PIT, 0xFF, 0, stamp, fr, 0);
            std::fprintf(stderr, "[vsarena-ref] pit fall: PIT (sanctions the respawn)\n");
        }
    }
    g.prev_state = st;
}

// ---------------------------------------------------------------------------
// QUIT
// ---------------------------------------------------------------------------
void quit_at_exit() {
    if (!g.active || !g.started || g.over || g.seat < 0) return;
    unsigned stamp = 0;
    stamp_for(now_ms(), &stamp);
    PClaim *c = make_claim(CLAIM_QUIT, 0xFF, 0, stamp, my_frame_now(), 0);
    if (c) {
        for (int i = 0; i < 3; ++i) { ::Sleep(15); send_claim(*c); }
    }
    std::fprintf(stderr, "[vsarena-ref] QUIT sent at exit\n");
}

// ---------------------------------------------------------------------------
// LAB RIGS (SM64DS_ARENA_RIG=race:<seed> | hit:<seed>). Scripted consoles for the
// proof rows: no human, no pad tape for the interesting moments. They move the
// body and press the pad at referee-clock instants every console agrees on;
// the ROM and the referee do everything else.
//   race  two consoles (seats 0 and 1): seat 0 breaks each marker (a claim, no
//         punch), and 4.5 s after the glass breaks both bodies are put on the
//         star at referee instants a scripted gap apart (-125..+125 ms).
//   hit   seat 0 punches seat 1's standing body: valid hits, then invalid ones
//         (a claim from out of reach, a repeat inside the hit immunity).
// ---------------------------------------------------------------------------
extern "C" char data_0209f49c[];
extern "C" char data_0209f49e[];

unsigned rig_rand(unsigned seed, unsigned n) {
    unsigned x = seed * 2654435761u + n * 40503u + 12345u;
    x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; x *= 3266489917u; x ^= x >> 16;
    return x;
}

void rig_parse() {
    const char *e = std::getenv("SM64DS_ARENA_RIG");
    if (!e || !*e) return;
    if (std::strncmp(e, "race", 4) == 0) g.rig = 1;
    else if (std::strncmp(e, "hit", 3) == 0) g.rig = 2;
    const char *c = std::strchr(e, ':');
    if (c) g.rig_seed = (unsigned)std::strtoul(c + 1, nullptr, 10);
    if (g.rig)
        std::fprintf(stderr, "[rig] SM64DS_ARENA_RIG=%s: scripted %s rig, seed %u\n",
                     e, g.rig == 1 ? "contention" : "hit", g.rig_seed);
}

void rig_press(int slot, unsigned short mask) {
    *(short *)(data_0209f49c + slot * 0x18) |= (short)mask;
    *(short *)(data_0209f49e + slot * 0x18) |= (short)mask;
}

void rig_teleport(char *me, const int *to) {
    int *p = (int *)(me + 0x5c);
    p[0] = to[0]; p[1] = to[1]; p[2] = to[2];
    s32r(me, 0x98) = 0;
    s32r(me, 0xa8) = 0;
}

void rig_race() {
    if (!g.started || g.over || g.seat > 1) return;
    unsigned r;
    if (!ref_now(&r)) return;
    char *me = my_body();
    if (!me) return;
    for (int k = 1; k < kMaxInst; ++k) {
        Inst &i = g.inst[k];
        if (!i.used || i.spilled) continue;
        if (i.state == 1 && g.seat == 0 && !i.rig_break_sent &&
            now_ms() - i.born_ms > 2600 && g.vs_frames > 120) {
            // the breaker: a claim straight to the referee (the glass breaks on
            // the event, everywhere)
            unsigned stamp;
            if (!stamp_for(now_ms(), &stamp)) continue;
            PClaim *c = make_claim(CLAIM_MARKER_BREAK, (unsigned char)k,
                                   (unsigned short)GetAngleToCamera(g.seat), stamp,
                                   my_frame_now(), 0);
            if (c) { c->inst = (unsigned char)k; i.rig_break_sent = true; }
            std::fprintf(stderr, "[rig] seat 0 breaks the marker of inst %d\n", k);
        }
        if (i.state == 2 && i.t_broken && !i.rig_race_done) {
            char *s = star_by_uid(i.uid);
            if (!s) continue;
            const int gap = (int)(rig_rand(g.rig_seed, (unsigned)k + 101u * g.rig_n) % 251u) - 125;
            const int off = g.seat == 0 ? (gap < 0 ? -gap : 0) : (gap > 0 ? gap : 0);
            const unsigned at = i.t_broken + 4500u + (unsigned)off;
            if ((int)(r - at) < 0) continue;
            const int to[3] = { s32r(s, 0x5c), s32r(s, 0x60), s32r(s, 0x64) };
            rig_teleport(me, to);
            i.rig_race_done = true;
            ++g.rig_n;
            std::fprintf(stderr, "[rig] race inst=%d seat=%d gap=%d off=%d at_ref=%u "
                         "local_ms=%u star=(%d,%d,%d)\n", k, g.seat, gap, off, r,
                         now_ms(), to[0] >> 12, to[1] >> 12, to[2] >> 12);
        }
    }
}

void rig_hit() {
    if (!g.started || g.over || g.seat > 1) return;
    unsigned r;
    if (!ref_now(&r)) return;
    char *me = my_body();
    if (!me) return;
    if (g.rig_next == 0) g.rig_next = g.t0 + 6000u;
    if (g.rig_press_frames > 0) {
        --g.rig_press_frames;
        rig_press(g.seat, 0x0002);
    }
    if (g.seat != 0 || (int)(r - g.rig_next) < 0) return;
    char *vb = body_of(1);
    if (!vb) return;
    const int cycle = g.rig_n % 3;
    const unsigned fr = my_frame_now();
    if (cycle == 0) {
        // a real punch: stand 90 units behind the victim, face it, press B
        int to[3] = { s32r(vb, 0x5c), s32r(vb, 0x60), s32r(vb, 0x64) - (90 << 12) };
        rig_teleport(me, to);
        s16r(me, 0x8e) = 0;               // facing +Z, toward the victim
        s16r(me, 0x94) = 0;
        g.rig_press_frames = 3;
        std::fprintf(stderr, "[rig] hit cycle %d: valid punch (B pressed, %u)\n",
                     g.rig_n, r);
        g.rig_next = r + 2500u;
    } else if (cycle == 1) {
        // out of reach: a claim with nothing under it (the referee checks the
        // two bodies' own reported positions)
        unsigned stamp;
        if (stamp_for(now_ms(), &stamp)) {
            PClaim *c = make_claim(CLAIM_HIT, 1, 0x40 >> 5, stamp, fr, drawn_frame(1));
            std::fprintf(stderr, "[rig] hit cycle %d: invalid claim sent (id %u)\n",
                         g.rig_n, c ? c->cid : 0u);
        }
        g.rig_next = r + 2500u;
    } else {
        // inside the hit immunity of the last landed hit
        unsigned stamp;
        if (stamp_for(now_ms(), &stamp)) {
            PClaim *c = make_claim(CLAIM_HIT, 1, 0x40 >> 5, stamp, fr, drawn_frame(1));
            std::fprintf(stderr, "[rig] hit cycle %d: repeat claim sent (id %u)\n",
                         g.rig_n, c ? c->cid : 0u);
        }
        g.rig_next = r + 3500u;
    }
    ++g.rig_n;
}

void rig_step() {
    if (g.rig == 1) rig_race();
    else if (g.rig == 2) rig_hit();
}

// ---------------------------------------------------------------------------
// ONE-SHOTS AT THE FIRST ARENA FRAME
// ---------------------------------------------------------------------------
void init_once() {
    if (g.inited) return;
    g.inited = true;
    install_order();
    // dispatch entry 5 (the collect confirm window)
    if (!g.star5_orig) {
        g.star5_orig = data_ov002_021109d8[5].fn;
        data_ov002_021109d8[5].fn = (unsigned)(size_t)&vs_ref_star5_face;
    }
    // St_Hurt's Main (the cell at +8 of the state object)
    if (!g.hurt_orig) {
        unsigned *cell = (unsigned *)(data_ov002_02110094 + 8);
        g.hurt_orig = cell[0];
        cell[0] = (unsigned)(size_t)&vs_ref_hurt_main_face;
    }
    std::atexit(quit_at_exit);
    rig_parse();
    std::fprintf(stderr, "[vsarena] referee client up: seat %d, %d bodies, order "
                 "buffer installed, collect + hurt seams wrapped\n", g.seat,
                 g.bodies);
}

void report_maybe() {
    const unsigned now = now_ms();
    if (!g.last_report) g.last_report = now;
    if (now - g.last_report >= (unsigned)kReportMs) {
        g.last_report = now;
        vs_ref_report("periodic");
    }
}

}  // namespace

// ===========================================================================
// PUBLIC
// ===========================================================================
bool vs_ref_active() {
    if (g.installed) return g.active;
    if (!vs_arena_active()) return false;
    const CommsHubStats h = comms_hub_stats();
    if (!h.installed) return false;
    g.installed = true;
    g.active = h.mode == 1;           // relay mode only: the mesh has no referee
    g.log = env_on("SM64DS_ARENA_LOG");
    for (int k = 0; k < 16; ++k) g.ledger[k] = 0;
    std::fprintf(stderr, "[vsarena] referee client %s (hub mode %d)\n",
                 g.active ? "ACTIVE: the referee in the relay decides stars, hits, "
                            "the clock and the winner"
                          : "inert: no relay, no referee (loopback mesh)", h.mode);
    return g.active;
}

void vs_ref_pre_walk(int frame) {
    (void)frame;
    if (!vs_ref_active() || data_0209f2d8 != 1) return;
    const ArenaStats s = vs_arena_stats();
    if (s.seat < 0) return;
    g.seat = s.seat;
    g.bodies = s.bodies;
    ++g.vs_frames;
    init_once();
    process_synr();
    syno_recv();
    pump_claims();
    maybe_send_ready();
    if (g.over) return;
    // the ledger and the progress byte are the referee's
    if (g.ledger_live) {
        for (int k = 0; k < 16; ++k)
            if (g.started || g.ledger[k] == 0) data_0209f310[k] = g.ledger[k];
    }
    if (g.started) data_0209f208 = g.progress;
    for (int k = 1; k < kMaxInst; ++k)
        if (g.inst[k].used && g.inst[k].state == 1) ensure_star_uid(g.inst[k]);
    if (g.started) {
        neutralise_foreign_star_touches();
        marker_pass();
        puppet_stars();
        hit_detect();
        rig_step();
    }
}

void vs_ref_post_walk(int frame) {
    (void)frame;
    if (!vs_ref_active() || data_0209f2d8 != 1 || !g.inited) return;
    if (g.started && !g.over) {
        if (g.ledger_live)
            for (int k = 0; k < 16; ++k) data_0209f310[k] = g.ledger[k];
        grab_detect();
        syno_send();
        my_state_edges();
    }
    g.last_post_ms = now_ms();
    report_maybe();
}

bool vs_ref_countdown(int *count, int *f304, int *beep, int *music) {
    *beep = 0;
    *music = 0;
    if (!vs_ref_active() || data_0209f2d8 != 1 || !g.inited) return false;
    if (g.over) return false;
    if (!g.started) {
        *count = 3;
        *f304 = 0x2a;
        return true;
    }
    unsigned r;
    if (!ref_now(&r)) {
        *count = 3;
        *f304 = 0x2a;
        return true;
    }
    const int left = (int)(g.t0 - r);             // ms to the match's frame 0
    int c;
    if (left > 2000) c = 3;
    else if (left > 1000) c = 2;
    else if (left > 0) c = 1;
    else c = 0;
    if (c != g.cd_count) {
        if (c == 0) { *beep = 0x2a; *music = 0x4d; }
        else *beep = 0x2b;
        g.cd_count = c;
    }
    *count = c;
    *f304 = c != 0 ? 0x2a : (left > -1000 ? 0x28 : 0);
    if (c == 0 && left <= -1000) return false;     // the START glyph is done
    return true;
}

void vs_ref_note_local_timeup(int frame) {
    (void)frame;
    if (!g.active || g.local_timeup) return;
    g.local_timeup = true;
    g.local_timeup_ms = now_ms();
}

bool vs_ref_match_over(VsRefOver *out) {
    if (!g.active) return false;
    if (!g.over && g.local_timeup &&
        now_ms() - g.local_timeup_ms >= (unsigned)kLocalTimeupGraceMs) {
        g.over = true;
        g.over_v = VsRefOver();
        g.over_v.reason = OVER_TIME_UP;
        g.over_v.roster_mask = g.roster_now ? g.roster_now : g.roster;
        g.over_v.seen_mask = (int)(g.ever_roster | g.roster);
        std::memcpy(g.over_v.scores, g.ledger, 16);
        int top = -128;
        for (int k = 0; k < 16; ++k)
            if ((g.roster >> k & 1u) && g.ledger[k] > top) top = g.ledger[k];
        for (int k = 0; k < 16; ++k)
            if ((g.roster >> k & 1u) && g.ledger[k] == top) {
                g.over_v.winners_mask |= 1 << k;
                ++g.over_v.winners;
            }
        g.over_v.local_timeout = true;
        std::fprintf(stderr, "[vsarena] connection lost: no MATCH OVER from the "
                     "referee %d ms after the clock ran out; ending on my last "
                     "ledger\n", kLocalTimeupGraceMs);
    }
    if (!g.over) return false;
    if (out) *out = g.over_v;
    return true;
}

bool vs_ref_ledger(signed char out[16]) {
    if (!g.active || !g.ledger_live) return false;
    std::memcpy(out, g.ledger, 16);
    return true;
}

unsigned short vs_ref_claim(unsigned char kind, unsigned char target,
                            unsigned short aux, unsigned ref_frame,
                            unsigned stamp, unsigned my_frame) {
    if (!g.active) return 0;
    if (!stamp) {
        if (!stamp_for(g.last_post_ms ? g.last_post_ms : now_ms(), &stamp)) return 0;
    }
    PClaim *c = make_claim(kind, target, aux, stamp,
                          my_frame ? my_frame : my_frame_now(), ref_frame);
    return c ? c->cid : 0;
}

int vs_ref_claim_result(unsigned short cid, int *reason, int *winner) {
    PClaim *c = claim_by_id(cid);
    if (!c) return -2;
    if (reason) *reason = c->reason;
    if (winner) *winner = c->winner;
    return c->result;
}

void vs_ref_set_event_sink(VsRefEventSink sink) { g.sink = sink; }

bool vs_ref_stamp_for(unsigned local_ms, unsigned *stamp) {
    return g.active && stamp_for(local_ms, stamp);
}

VsRefStats vs_ref_stats() {
    VsRefStats s = g.st;
    s.active = g.active;
    s.started = g.started;
    s.over = g.over;
    s.applied_seq = g.applied;
    return s;
}

void vs_ref_report(const char *tag) {
    if (!g.active) return;
    std::fprintf(stderr, "[vsarena-ref] %s seat=%d started=%d over=%d seq=%u "
                 "frames=%llu events=%llu dup=%llu gaps=%llu claims=%llu resent=%llu "
                 "verdicts=%llu denied=%llu taken=%llu hits=%llu/%llu/%llu spills=%llu "
                 "syno=%llu/%llu ledger=%d,%d,%d,%d progress=%u\n", tag ? tag : "",
                 g.seat, g.started, g.over, (unsigned)g.applied, g.st.frames_rx,
                 g.st.events_applied, g.st.events_dup, g.st.gaps, g.st.claims_sent,
                 g.st.claims_resent, g.st.verdicts, g.st.denied, g.st.stars_taken,
                 g.st.hits_claimed, g.st.hits_landed, g.st.hits_denied, g.st.spills,
                 g.st.syno_tx, g.st.syno_rx, g.ledger[0], g.ledger[1], g.ledger[2],
                 g.ledger[3], (unsigned)g.progress);
}

}  // namespace port
