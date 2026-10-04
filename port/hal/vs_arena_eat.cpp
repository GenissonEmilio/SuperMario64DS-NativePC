// ARENA MODE, eating (see vs_arena_eat.h). Run mp1, lane EAT1, plan A5.
//
// PART 1: with no referee (the loopback mesh) the tongue never eats a player
//         in arena mode: Player::OnYoshiTryEat (slot 18) answers 0.
// PART 2: with the referee, eating is a conversation between two consoles.
//
//   THE EATER'S CONSOLE (E). The ROM's own tongue (St_YoshiPower_Main ->
//   func_ov002_020d6998) touches the PUPPET of the victim and asks it
//   OnYoshiTryEat (slot 18, wrapped here). On a puppet of a plain Yoshi that
//   can be eaten we send an EAT claim and answer 1: the ROM goes on and eats
//   the puppet at once (func_ov002_020d6c60: puppet +0xd0 = me, me +0x360 =
//   puppet, the puppet in St_InYoshiMouth, my mouth closing). That is the
//   instant local feel. The referee's verdict arrives one round trip later:
//   EATEN confirms; a DENY takes the puppet back out of the mouth. The ROM does
//   the rest of the eater's work (the mouth hold, spit, swallow) on my own
//   body; when my mouth lets go, SPIT / SWALLOW is claimed so the victim's
//   console lets go too. While the puppet is held it sits on my body's
//   position, as the ROM's own victim code puts a victim there.
//
//   THE VICTIM'S CONSOLE (V). The referee's EATEN event is the only thing that
//   puts MY body into a mouth: the ROM's own entry (func_ov002_020d5f34) with
//   the eater's puppet as mEatingPlayer, the puppet's +0x360 set to my body,
//   the 90-frame hold the ROM gives the eater. From then on my own
//   St_InYoshiMouth runs the ROM's code against that puppet: it follows the
//   eater, counts down, and when it breaks free it hurts the eater (here: the
//   puppet) and sets itself loose. I see my body leave the mouth by itself and
//   send ESCAPE; the referee tells the eater (RELEASED, 3), whose console
//   takes the puppet out of its mouth and gets hurt (the ROM's own Player::Hurt
//   on its own body; the star it costs is the ordinary hurt claim). A SPIT /
//   SWALLOW verdict (RELEASED 1 / 2) makes my body leave the way the ROM makes
//   it leave (func_ov002_020d5cec, or the swallow's func_ov002_020d5ab4).
//
// The tongue contact also writes the tongue's hit flag (0x8000) on the puppet's
// body cylinder, which the referee client would turn into a HIT claim; while
// eating is on, the tongue is an EAT attempt, so that bit is cleared on the
// puppets before the hit pass reads it.
//
// Nothing here runs unless arena mode is installed.

#include "vs_arena_eat.h"

#include "comms_hub.h"
#include "vs_arena.h"
#include "vs_arena_ref.h"

#include <windows.h>
#include <intrin.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace arena_wire;

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f2d8;
extern unsigned char data_ov002_0210a83c[];    // _ZTV6Player
extern unsigned char data_ov002_02110064[];    // St_InYoshiMouth
extern unsigned char data_ov002_0211013c[];    // the neutral state
extern char data_0209f49c[];
extern char data_0209f49e[];
extern char data_0209f498[];                     // Stage::CheckInput's Ctrl[4], stride 0x18
extern unsigned short data_020a0e58[];           // PadData[4]: {held, pressed} (the raw DS pad)
extern int data_020a0e5a[];                      // the split "pressed" symbol (same memory on the DS)
extern unsigned short *data_0209214c[];
char *_ZN8dActor_c10FindWithIDEj(unsigned id);       // the ROM's actor list lookup by unique id          // CheckInput's three DS bit -> Ctrl bit maps

int _ZN6Player7IsStateERNS_5StateE(void *self, void *state);
void _ZN6Player11ChangeStateERNS_5StateE(void *self, void *state);
int _ZN6Player4HurtERK7Vector3j5Fix12IiEjjj(void *self, const void *v, unsigned a,
        int knock, unsigned d, unsigned e, unsigned f);
void Vec3_RotateYAndTranslate(int *out, int *in, short angle, int *src);
int func_ov002_020d82f0(void *c);       // the ROM's "can be hurt / eaten" gate
int func_ov002_020d5f34(char *c, void *eater);  // victim: into the mouth (state, +0xd0)
int func_ov002_020d5ed0(char *c);       // victim: mouth step 1 + the 90-frame hold
int func_ov002_020d5cec(char *c);       // victim: let go (spit)
int func_ov002_020d5ab4(char *c);       // victim: let go (swallow: the egg)
void func_ov002_020d718c(char *c);      // eater: forget the mouthful
}

namespace port {
namespace {

enum : int {
    kSlotOnYoshiTryEat = 18,     // dActor_c's first own virtual
    kPadSlotBytes = 0x18,
    kMouthObj = 0x360,           // mObjInMouth: the player in MY mouth
    kEatenBy = 0xd0,             // mEatingPlayer: whose mouth I am in
    kHoldTimer = 0x6c6,          // the eater's mouth hold (0x5a = 90 frames)
    kStYoshiPower = 3,
    kStSwallow = 4,
    kTongueFlag = 0x8000,        // the tongue's hit flag on a body cylinder
    kBodyClsnHit = 0x2f4,        // the pair pass's hit flags on a body (+0x2d4 + 0x20)
    kSeatNone = -1,
    kWaitVerdictMs = 700,
    kSilentMs = 2000,            // a partner's SYNS silent this long: its console is gone (a leave
                                 // and a fast rejoin inside the referee's presence window shows no leave)
    kRetryFrames = 45,
};

struct EatE {                    // I am the eater
    bool active = false;
    int k = kSeatNone;           // the victim's seat
    unsigned short cid = 0;      // the EAT claim
    int phase = 0;               // 0 waiting for the verdict, 1 EATEN
    unsigned t0_ms = 0;
    unsigned epoch = 0;          // the victim seat's join epoch when I ate it
};
struct EatV {                    // I am the victim
    bool active = false;
    int e = kSeatNone;           // the eater's seat
    unsigned frame0 = 0;
    unsigned absent = 0;         // frames the eater's seat has been missing
    unsigned epoch = 0;          // the eater seat's join epoch when I was eaten
};

struct Eat {
    bool wrapped = false;
    bool log = false;
    bool sink_set = false;
    unsigned orig = 0;           // the face the slot held before us
    char *mouth_ptr = nullptr;   // what my mouth held last frame, and its unique id
    unsigned mouth_uid = 0;
    unsigned long long mouth_cleared = 0;
    EatE e;
    EatV v;
    unsigned cool_until = 0;     // no new eat attempt before this arena frame
    // lab rig
    int rig = 0;                 // 1 tongue only, 2 eat
    int rig_var = 0;             // 0 hold (the eater's own timer spits), 1 spit, 2 swallow, 3 struggle (the victim mashes), 4 stay (the eater's timer never ends)
    int rig_press_at = 8;
    unsigned rig_frames = 0;
    int rig_press = 0;
    unsigned rig_hold = 0;
    int rig_pin = 0;
    bool vp_set = false;
    int vp[3] = {0, 0, 0};
    int probe = 0;               // frames of tongue probe lines left (SM64DS_EAT_LOG)
    unsigned short rig_mask = 0;
    // counters
    unsigned long long asked = 0, refused = 0, rig_tongues = 0;
    unsigned long long claims = 0, confirmed = 0, denied = 0, unwound = 0;
    unsigned long long victim_eaten = 0, victim_released = 0, escapes_sent = 0,
                       spit_sent = 0, swallow_sent = 0, eater_hurt = 0,
                       eater_dropped = 0, forced_release = 0, tongue_bits = 0;
};
Eat g;

int &s32r(char *b, int off) { return *(int *)(b + off); }
short &s16r(char *b, int off) { return *(short *)(b + off); }
char *&ptr(char *b, int off) { return *(char **)(b + off); }

char *my_body() {
    const int s = vs_arena_my_seat();
    return s >= 0 && s < kArenaSeats ? (char *)data_0209f394[s] : nullptr;
}
char *body_of(int seat) {
    return seat >= 0 && seat < kArenaSeats ? (char *)data_0209f394[seat] : nullptr;
}
int seat_of_body(const char *b) {
    if (!b) return kSeatNone;
    const ArenaStats st = vs_arena_stats();
    for (int k = 0; k < st.bodies && k < kArenaSeats; ++k)
        if (data_0209f394[k] == b) return k;
    return kSeatNone;
}
bool part2() {
    static int off = -1;                // SM64DS_EAT_PART2=0: Part 1's behaviour only
    if (off < 0) { const char *e = std::getenv("SM64DS_EAT_PART2"); off = e && *e == '0'; }
    return !off && vs_ref_active();
}

unsigned drawn_frame(int seat) {      // the victim frame I am drawing (A3's rule)
    ArenaSeatView v;
    if (!vs_arena_seat_view(seat, &v) || !v.has_state) return 0;
    long long t = (long long)comms_hub_now_ms() - v.clock_offset_ms - 50;
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

// the victim's last reported state says it can be eaten (the referee checks the
// same things against the drawn frame)
bool seat_eatable(int seat) {
    ArenaSample s[2];
    if (vs_arena_samples(seat, s, 1) < 1) return false;
    const ArenaState &a = s[0].s;
    if (a.invuln > 0 || (a.flags & (kArenaInHurt | kArenaNoControl)) ||
        (a.power & (kArenaMega | kArenaVanish | kArenaMetal)) || a.eaten_by)
        return false;
    return (a.flags & kArenaLive) != 0;
}

// ---------------------------------------------------------------------------
// THE EATER'S SIDE
// ---------------------------------------------------------------------------
// Take the puppet out of my mouth without ceremony: its mouth state is left
// (the ROM's own cleanup restores its collision), my mouthful forgotten.
void e_drop_puppet(const char *why) {
    char *me = my_body();
    char *pb = body_of(g.e.k);
    if (pb) {
        s32r(pb, kEatenBy) = 0;
        if (_ZN6Player7IsStateERNS_5StateE(pb, data_ov002_02110064))
            _ZN6Player11ChangeStateERNS_5StateE(pb, data_ov002_0211013c);
    }
    if (me && pb && ptr(me, kMouthObj) == pb) func_ov002_020d718c(me);
    ++g.eater_dropped;
    std::fprintf(stderr, "[eat] E: puppet of seat %d out of my mouth (%s)\n", g.e.k, why);
}

void e_hurt_self(const char *why) {
    char *me = my_body();
    if (!me) return;
    // the ROM's own victim code does this to the eater when the victim breaks
    // free: Hurt from a point in front of the eater
    int tr[3] = { 0, 0x64000, 0x64000 };
    int hv[3];
    Vec3_RotateYAndTranslate(hv, (int *)(me + 0x5c), s16r(me, 0x8e), tr);
    const int r = _ZN6Player4HurtERK7Vector3j5Fix12IiEjjj(
        me, hv, 1u, 0, 0u, (unsigned)(s32r(me, 8) + 1) & 0xffu, 1u);
    ++g.eater_hurt;
    std::fprintf(stderr, "[eat] E: hurt by the victim breaking free (%s): Player::Hurt "
                 "%s\n", why, r ? "applied" : "refused");
}

void e_finish() {
    g.e = EatE();
    g.cool_until = vs_arena_frame() + kRetryFrames;
}

void e_monitor() {
    if (!g.e.active) return;
    char *me = my_body();
    char *pb = body_of(g.e.k);
    if (!me || !pb) { e_finish(); return; }
    const bool holding = ptr(me, kMouthObj) == pb && ptr(pb, kEatenBy) == me;
    if (g.e.phase == 0) {
        int reason = 0, winner = 0;
        const int r = vs_ref_claim_result(g.e.cid, &reason, &winner);
        const unsigned age = comms_hub_now_ms() - g.e.t0_ms;
        if (r == 1) {
            g.e.phase = 1;
            ++g.confirmed;
            std::fprintf(stderr, "[eat] E: EAT verdict EATEN for seat %d after %u ms\n",
                         g.e.k, age);
        } else if (r == -1 || (r == -2 && age > (unsigned)kWaitVerdictMs)) {
            ++g.denied;
            std::fprintf(stderr, "[eat] E: EAT claim %u for seat %d %s (reason %d, %u ms): "
                         "the puppet comes out of my mouth\n", g.e.cid, g.e.k,
                         r == -1 ? "denied" : "unanswered", reason, age);
            if (holding || ptr(pb, kEatenBy) == me) { e_drop_puppet("denied"); ++g.unwound; }
            e_finish();
            return;
        }
    }
    if (!(vs_arena_roster_mask() >> g.e.k & 1u) || vs_arena_body_held(g.e.k)) {
        e_drop_puppet("victim left");
        e_finish();
        return;
    }
    {
        // the victim's console left and came back before the referee noticed (its seat was
        // reclaimed inside the presence window), or went quiet: the referee still has it in my mouth
        ArenaSeatView sv;
        if (vs_arena_seat_view(g.e.k, &sv) && (sv.join_epoch != g.e.epoch || sv.newest_age_ms > (unsigned)kSilentMs)) {
            e_drop_puppet("the victim's console re-joined or fell silent");
            if (g.e.phase == 1) vs_ref_claim(CLAIM_SPIT, (unsigned char)g.e.k, 0, 0, 0, 0);
            e_finish();
            return;
        }
    }
    if (!holding && g.e.phase == 1) {
        // my mouth let go: the ROM's spit or swallow, or my own hurt
        const bool sw = vs_arena_state_index(me) == kStSwallow;
        if (_ZN6Player7IsStateERNS_5StateE(pb, data_ov002_02110064))
            _ZN6Player11ChangeStateERNS_5StateE(pb, data_ov002_0211013c);
        const unsigned short c = vs_ref_claim(sw ? CLAIM_SWALLOW : CLAIM_SPIT,
                                              (unsigned char)g.e.k, 0, 0, 0, 0);
        if (sw) ++g.swallow_sent; else ++g.spit_sent;
        std::fprintf(stderr, "[eat] E: my mouth let go of seat %d: %s claim %u\n", g.e.k,
                     sw ? "SWALLOW" : "SPIT", c);
        e_finish();
    }
}

// after the actor walk: keep the held puppet on my body
void e_after_walk() {
    if (!g.e.active) return;
    char *me = my_body();
    char *pb = body_of(g.e.k);
    if (!me || !pb) return;
    if (ptr(me, kMouthObj) != pb || ptr(pb, kEatenBy) != me) return;
    if (vs_arena_state_index(me) == kStYoshiPower) return;   // the mouth anim places it
    s32r(pb, 0x5c) = s32r(me, 0x5c);
    s32r(pb, 0x60) = s32r(me, 0x60);
    s32r(pb, 0x64) = s32r(me, 0x64);
}

// The ROM's own gate for eating a player (func_ov002_020d82f0) reads the
// victim's invincible timer, mega / vanish bits and its hurt and no-control
// bytes. A puppet's Behavior never runs, so the two bytes a countdown or a hurt
// left on it would stay for good: the victim's own last SYNS (seat_eatable, run
// first) is the truth, and the puppet's copy is brought in line before the ROM
// asks.
bool puppet_gate(char *pb) {
    *(unsigned char *)(pb + 0x708) = 0;      // mIsTakingDamage
    *(unsigned char *)(pb + 0x709) = 0;      // mIsNoControl
    *(unsigned short *)(pb + 0x73c) = 0;
    *(unsigned char *)(pb + 0x703) = 0;      // mega
    return func_ov002_020d82f0(pb) != 0;
}

// OnYoshiTryEat, the tongue's question. __thiscall as a __fastcall face.
int __fastcall eat_face(void *selfv, void *edx) {
    char *self = (char *)selfv;
    if (!vs_arena_active())
        return ((int (__fastcall *)(void *, void *))(size_t)g.orig)(selfv, edx);
    ++g.asked;
    char *me = my_body();
    const int k = seat_of_body(self);
    if (g.log && g.asked <= 12) {
        const char *base = (const char *)::GetModuleHandleA(nullptr);
        std::fprintf(stderr, "[eat] ask #%llu f=%u from rva %08x (module %p): self=%p seat=%d me_state=%d "
                     "step=%d my+338=%u my+334=%08x\n", g.asked, vs_arena_frame(),
                     (unsigned)((const char *)_ReturnAddress() - base), (const void *)base, selfv, k,
                     me ? vs_arena_state_index(me) : -1, me ? (int)*(unsigned char *)(me + 0x6e3) : -1,
                     me ? *(unsigned *)(me + 0x338) : 0u, me ? *(unsigned *)(me + 0x334) : 0u);
    }
    // The tongue asks this slot for two different things: while it picks what to
    // aim at (func_ov002_020d708c, at the tongue's start, nothing touched yet) and
    // when the tongue's attack cylinder is on this very actor (func_ov002_020d6998,
    // my cylinder's otherOwner +0x338 = its uid). The first is "can it be
    // eaten": answered from the victim's own last SYNS. Only the second is a claim.
    if (me && self == (char *)ptr(me, kMouthObj) && part2()) {
        // my eating step asking about the mouthful it already holds: the ROM's answer
        return ((int (__fastcall *)(void *, void *))(size_t)g.orig)(selfv, edx);
    }
    if (!me || self == me || k == kSeatNone) {
        ++g.refused;
        return 0;
    }
    if (s32r(self, 8) != 3) {
        // a capped player: the ROM's own answer (it aims at it and the tongue bounces)
        return 1;
    }
    if (part2()) {
        const unsigned uid = *(unsigned *)(self + 4);
        const bool touching = uid && *(unsigned *)(me + 0x338) == uid;
        const bool eatable = seat_eatable(k);
        if (!touching) return eatable ? 1 : 0;
        if (eatable && !g.e.active && vs_arena_frame() >= g.cool_until &&
            ptr(me, kMouthObj) == nullptr && ptr(self, kMouthObj) == nullptr &&
            ptr(self, kEatenBy) == nullptr && puppet_gate(self)) {
            // my_frame = the last frame I sent: the tongue has been out for a frame
            // or more (St_YoshiPower in the SYNS before it), and that frame is not
            // yet the one the eating itself marks no-control (the referee refuses
            // a claimant whose frame says it is no longer in control)
            const unsigned short cid = vs_ref_claim(CLAIM_EAT, (unsigned char)k, 0,
                                                    drawn_frame(k), 0, vs_arena_frame());
            if (cid) {
                g.e.active = true;
                g.e.k = k;
                g.e.cid = cid;
                g.e.phase = 0;
                g.e.t0_ms = comms_hub_now_ms();
                ArenaSeatView sv;
                g.e.epoch = vs_arena_seat_view(k, &sv) ? sv.join_epoch : 0;
                ++g.claims;
                std::fprintf(stderr, "[eat] E: tongue on the puppet of seat %d: EAT claim %u "
                             "sent, eating it now\n", k, cid);
                return 1;                  // the ROM eats the puppet (d6c60)
            }
        }
    }
    ++g.refused;
    if (g.log || g.refused <= 3)
        std::fprintf(stderr, "[eat] tongue on a player (body %p, seat %d): refused, the "
                     "ROM's own refusal path (n=%llu)\n", selfv, k, g.refused);
    if (g.log && me && self != me && k != kSeatNone) {
        ArenaSample sm[2];
        const int ns = vs_arena_samples(k, sm, 1);
        std::fprintf(stderr, "[eat] why: part2=%d param=%d e.active=%d frame=%u cool=%u my_mouth=%p "
                     "its_mouth=%p its_eater=%p d82f0=%d eatable=%d samples=%d invuln=%u "
                     "flags=%02x power=%02x eaten_by=%u\n", part2() ? 1 : 0, s32r(self, 8),
                     g.e.active ? 1 : 0, vs_arena_frame(), g.cool_until, (void *)ptr(me, kMouthObj),
                     (void *)ptr(self, kMouthObj), (void *)ptr(self, kEatenBy),
                     func_ov002_020d82f0(self), seat_eatable(k) ? 1 : 0, ns,
                     ns ? (unsigned)sm[0].s.invuln : 0u, ns ? (unsigned)sm[0].s.flags : 0u,
                     ns ? (unsigned)sm[0].s.power : 0u, ns ? (unsigned)sm[0].s.eaten_by : 0u);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// THE VICTIM'S SIDE
// ---------------------------------------------------------------------------
// The eater's puppet is never ticked, so the two bytes my mouth state reads on
// it (the ROM stops the hold's countdown while the eater is hurt or has no
// control) come from the eater's own last SYNS flags.
void mirror_eater_flags(char *ep, int eater) {
    ArenaSample sm[2];
    const bool have = vs_arena_samples(eater, sm, 1) >= 1;
    const unsigned fl = have ? sm[0].s.flags : 0u;
    *(unsigned char *)(ep + 0x708) = (fl & kArenaInHurt) ? 1 : 0;
    *(unsigned char *)(ep + 0x709) = (fl & kArenaNoControl) ? 1 : 0;
}

void v_enter(int eater, const RefEvent &ev) {
    char *me = my_body();
    char *ep = body_of(eater);
    if (!me || !ep || g.v.active) return;
    s32r(me, kEatenBy) = 0;
    ptr(ep, kMouthObj) = me;                     // eater +0x360 = me
    if (!func_ov002_020d5f34(me, ep)) {          // +0xd0 = eater, my mouth state
        ptr(ep, kMouthObj) = nullptr;
        std::fprintf(stderr, "[eat] V: EATEN by seat %d but the ROM refused to put me in "
                     "the mouth\n", eater);
        return;
    }
    // the ROM's hold: the eater's timer is 0x5a, and in a lockstep world BOTH the
    // eater's own Behavior and my mouth state count it down, so it runs out in
    // about 45 frames; here only my mouth state counts the puppet's copy
    *(unsigned short *)(ep + kHoldTimer) = 0x2d;
    mirror_eater_flags(ep, eater);
    func_ov002_020d5ed0(me);                     // mouth step 1: follow, count down
    g.v.active = true;
    g.v.e = eater;
    g.v.frame0 = vs_arena_frame();
    {
        ArenaSeatView sv;
        g.v.epoch = vs_arena_seat_view(eater, &sv) ? sv.join_epoch : 0;
    }
    g.v.absent = 0;
    ++g.victim_eaten;
    std::fprintf(stderr, "[eat] V: eaten by seat %d (event seq %u): my body is in its "
                 "mouth\n", eater, (unsigned)ev.seq);
}

void v_release(int how, const char *why) {
    char *me = my_body();
    char *ep = body_of(g.v.e);
    if (me && _ZN6Player7IsStateERNS_5StateE(me, data_ov002_02110064) &&
        ptr(me, kEatenBy) != nullptr) {
        if (how == 2) func_ov002_020d5ab4(me);
        else func_ov002_020d5cec(me);
    }
    if (ep && ptr(ep, kMouthObj) == me) ptr(ep, kMouthObj) = nullptr;
    ++g.victim_released;
    std::fprintf(stderr, "[eat] V: out of the mouth of seat %d (%s, how %d)\n", g.v.e, why, how);
    g.v = EatV();
}

void v_monitor() {
    if (!g.v.active) return;
    char *me = my_body();
    char *ep = body_of(g.v.e);
    if (!me || !ep) { g.v = EatV(); return; }
    mirror_eater_flags(ep, g.v.e);
    if (g.log && (vs_arena_frame() - g.v.frame0) % 10 == 0)
        std::fprintf(stderr, "[eat] V: hold f+%u timer=%u step=%d eater hurt/nocontrol=%d/%d\n",
                     vs_arena_frame() - g.v.frame0, (unsigned)*(unsigned short *)(ep + kHoldTimer),
                     (int)*(unsigned char *)(me + 0x6e3), (int)*(unsigned char *)(ep + 0x708),
                     (int)*(unsigned char *)(ep + 0x709));
    if (ptr(me, kEatenBy) == nullptr) {
        // my own St_InYoshiMouth broke free (the ROM's hold ran out) and hurt
        // the eater: tell the referee
        const unsigned short c = vs_ref_claim(CLAIM_ESCAPE, (unsigned char)g.v.e, 0, 0, 0, 0);
        ++g.escapes_sent;
        std::fprintf(stderr, "[eat] V: broke free of seat %d after %u frames: ESCAPE claim %u\n",
                     g.v.e, vs_arena_frame() - g.v.frame0, c);
        if (ptr(ep, kMouthObj) == me) ptr(ep, kMouthObj) = nullptr;
        g.v = EatV();
        return;
    }
    {
        // the eater's console left and came back before the referee noticed: whatever now
        // sits in that seat holds nobody; the referee still has me in its mouth, so tell it
        ArenaSeatView sv;
        if (vs_arena_seat_view(g.v.e, &sv) && (sv.join_epoch != g.v.epoch || sv.newest_age_ms > (unsigned)kSilentMs)) {
            vs_ref_claim(CLAIM_ESCAPE, (unsigned char)g.v.e, 0, 0, 0, 0);
            v_release(4, "the eater's console re-joined or fell silent");
            return;
        }
    }
    // the eater's seat gone: the referee says so with a leave; do not wait on it
    if (!(vs_arena_roster_mask() >> g.v.e & 1u) || vs_arena_body_held(g.v.e)) {
        if (++g.v.absent > 90) { ++g.forced_release; v_release(1, "eater's seat gone"); }
    } else {
        g.v.absent = 0;
    }
}

// ---------------------------------------------------------------------------
// REFEREE EVENTS (one sink, after the referee client applied its own part)
// ---------------------------------------------------------------------------
void on_event(const RefEvent *ev, int my_seat, bool directed) {
    if (directed || !ev) return;
    if (ev->type == EV_EATEN) {
        if (ev->seat == my_seat) v_enter(ev->a, *ev);
        return;
    }
    if (ev->type != EV_RELEASED) return;
    const int victim = ev->seat, eater = ev->a, how = ev->b;
    if (victim == my_seat) {
        if (how == 3) return;                       // my own escape, already applied
        if (g.v.active && g.v.e == eater)
            v_release(how, how == 4 ? "the eater left" : how == 2 ? "swallowed" : "spat out");
        return;
    }
    if (eater == my_seat && g.e.active && g.e.k == victim) {
        if (how == 3) {
            e_drop_puppet("it broke free");
            e_hurt_self("escape verdict");
        } else if (how == 4) {
            e_drop_puppet("the victim left");
        }
        e_finish();
    }
}

// ---------------------------------------------------------------------------
// WRAP + LAB RIG
// ---------------------------------------------------------------------------
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
    else if (r && std::strncmp(r, "eat", 3) == 0) {
        g.rig = 2;
        if (std::strstr(r, "spit")) g.rig_var = 1;
        else if (std::strstr(r, "struggle")) g.rig_var = 3;
        else if (std::strstr(r, "stay")) g.rig_var = 4;
        else if (std::strstr(r, "swallow")) g.rig_var = 2;
        const char *c = std::strchr(r, '@');
        if (c) g.rig_press_at = std::atoi(c + 1);
    }
    if (r && std::strncmp(r, "stalemouth", 10) == 0) g.rig = 3;
    std::fprintf(stderr, "[eat] arena eating: Player::OnYoshiTryEat wrapped (%s)%s\n",
                 part2() ? "the referee decides" : "no referee: the tongue never eats a player",
                 g.rig ? ", rig on" : "");
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

// A press of the ROM's own button words (Ctrl bits: 0x0001 attack, 0x0400 the
// swallow key) goes in as the RAW DS pad bits whose remap gives it, into PadData
// before the actor walk: Stage::Behavior (CheckInput) rebuilds the Ctrl block
// from PadData at the top of the walk, so a write into the Ctrl words never
// reaches a body. `first` = the edge (the pressed word) too.
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
    if (g.log)
        std::fprintf(stderr, "[eat] press ctrl=%04x ds=%04x slot=%d first=%d (mode %d) pad %04x/%04x\n",
                     (unsigned)ctrl_mask, (unsigned)ds, slot, first ? 1 : 0, (int)mode,
                     (unsigned)data_020a0e58[slot * 2], (unsigned)data_020a0e58[slot * 2 + 1]);
}

// SM64DS_EAT_RIG=tongue | eat[:hold|spit|swallow][@frames]: seat 0 stands 60
// units behind seat 1's puppet, faces it and uses the tongue (the attack
// button) once per cycle. tongue: every 120 arena frames. eat: every 420; after
// `@frames` (default 8) of a full, still mouth seat 0 spits (attack again) or swallows (X); hold lets
// the victim break free by itself. Seat 1 stands still.
void rig_step() {
    const int me = vs_arena_my_seat();
    if (data_0209f2d8 != 1 || me < 0 || me > 1) return;
    if (g.rig == 2 && me != 0) {
        // the eat rig's victim stands where it first stood (the selftest's pad tape
        // would walk it away): pinned for the 40 frames before and 30 after each
        // tongue, and whenever it is not in a mouth or hurt
        char *vb2 = (char *)data_0209f394[1];
        if (!vb2 || !vs_ref_stats().started) return;
        // STRUGGLE: the ROM's hold counts down by the number of buttons the victim
        // presses a frame (func_ov002_020beb38); rig_var 3 mashes four every frame
        if (g.rig_var == 3 && g.v.active) {
            rig_press(1, 0x0001 | 0x0002 | 0x0400 | 0x0800, true);
        }
        ++g.rig_frames;
        if (!g.vp_set) {
            for (int i = 0; i < 3; ++i) g.vp[i] = *(int *)(vb2 + 0x5c + 4 * i);
            g.vp_set = true;
        }
        const unsigned cf2 = g.rig_frames % 420u;
        const bool window = cf2 >= 380u || cf2 <= 30u;
        if (window && !g.v.active && ptr(vb2, kEatenBy) == nullptr && vs_arena_state_index(vb2) != 7) {
            for (int i = 0; i < 3; ++i) *(int *)(vb2 + 0x5c + 4 * i) = g.vp[i];
            s32r(vb2, 0x98) = 0;
        }
        return;
    }
    if (!vs_ref_stats().started) return;    // the referee's START (the countdown is over)
    const int other = 1 - me;
    ++g.rig_frames;
    char *mb = (char *)data_0209f394[me];
    char *vb = (char *)data_0209f394[other];
    if (!mb || !vb) return;
    ArenaSeatView sv;
    if (!vs_arena_seat_view(other, &sv) || !sv.has_state) return;
    // hold the stance for the first frames of the tongue (the selftest's own pad
    // tape walks and turns the body): 40 units behind the puppet, facing it
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
        rig_press(me, g.rig_mask, g.rig_press == 3);
        --g.rig_press;
        return;
    }
    const unsigned cycle = g.rig == 1 ? 120u : 420u;
    if (g.rig_frames < 240) return;
    // the tongue rig runs on both seats, half a cycle apart
    const unsigned cf = (g.rig_frames + (g.rig == 1 && me ? 60u : 0u)) % cycle;
    if (cf == 0) {
        int *p = (int *)(mb + 0x5c);
        p[0] = s32r(vb, 0x5c);
        p[1] = s32r(vb, 0x60);
        p[2] = s32r(vb, 0x64) - (40 << 12);
        s32r(mb, 0x98) = 0;
        s32r(mb, 0xa8) = 0;
        s16r(mb, 0x8e) = 0;
        s16r(mb, 0x94) = 0;
        g.rig_mask = 0x0001;
        g.rig_press = 3;
        g.rig_pin = 25;
        ++g.rig_tongues;
        g.probe = 40;
        std::fprintf(stderr, "[eat] rig: tongue %llu (40 units behind the puppet of seat %d)\n",
                     g.rig_tongues, other);
    }
    // spit / swallow (rig_var 1 / 2): once the verdict is in and the mouth is full and still,
    // wait `@frames` (default 8) and press
    // rig_var 4 (stay): the eater's own mouth timer never runs out, so a row can end the
    // mouthful by the victim or the eater leaving
    if (g.rig == 2 && g.rig_var == 4 && g.e.active && ptr(mb, kMouthObj) == body_of(g.e.k))
        *(unsigned short *)(mb + kHoldTimer) = 0x5a;
    if (g.rig == 2 && (g.rig_var == 1 || g.rig_var == 2) && g.e.active && g.e.phase == 1) {
        char *pb = body_of(g.e.k);
        if (pb && ptr(mb, kMouthObj) == pb && vs_arena_state_index(mb) != kStYoshiPower) {
            if (++g.rig_hold == (unsigned)g.rig_press_at) {
                g.rig_mask = g.rig_var == 1 ? 0x0001 : 0x0400;
                g.rig_press = 3;
                std::fprintf(stderr, "[eat] rig: %s (mouth full and still for %u frames)\n",
                             g.rig_var == 1 ? "spit" : "swallow", g.rig_hold);
            }
        }
    } else {
        g.rig_hold = 0;
    }
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
    std::fprintf(stderr, "[eat] exit asked=%llu refused=%llu claims=%llu confirmed=%llu "
                 "denied=%llu unwound=%llu | v_eaten=%llu v_released=%llu v_escapes=%llu "
                 "v_forced=%llu | e_spit=%llu e_swallow=%llu e_hurt=%llu e_dropped=%llu | "
                 "rig_tongues=%llu tongue_bits_cleared=%llu | still_eating=%d still_eaten=%d mouth_cleared=%llu\n",
                 g.asked, g.refused, g.claims,
                 g.confirmed, g.denied, g.unwound, g.victim_eaten, g.victim_released,
                 g.escapes_sent, g.forced_release, g.spit_sent, g.swallow_sent, g.eater_hurt,
                 g.eater_dropped, g.rig_tongues, g.tongue_bits, g.e.active ? 1 : 0,
                 g.v.active ? 1 : 0, g.mouth_cleared);
}

// SM64DS_EAT_LOG=1: what my tongue and the other seat's puppet look like for the
// 40 frames after the rig presses (state, attack cylinder's hit words, the puppet's
// body cylinder words before they are cleared, the distance)
void probe_tongue() {
    if (g.probe <= 0 || !g.log) return;
    --g.probe;
    const int me = vs_arena_my_seat();
    const int other = me == 0 ? 1 : 0;
    char *mb = body_of(me);
    char *vb = body_of(other);
    if (!mb || !vb) return;
    const long dx = (long)(s32r(vb, 0x5c) - s32r(mb, 0x5c)) >> 12;
    const long dz = (long)(s32r(vb, 0x64) - s32r(mb, 0x64)) >> 12;
    std::fprintf(stderr, "[eat] probe f=%u state=%d step=%d yaw=%d d=(%ld,%ld) atk=%08x/%u "
                 "body_hit=%08x/%u pad=%04x\n", vs_arena_frame(), vs_arena_state_index(mb),
                 (int)*(unsigned char *)(mb + 0x6e3), (int)s16r(mb, 0x8e), dx, dz,
                 *(unsigned *)(mb + 0x334), *(unsigned *)(mb + 0x338),
                 *(unsigned *)(vb + kBodyClsnHit), *(unsigned *)(vb + 0x2f8),
                 (unsigned)*(unsigned short *)(data_0209f49e + me * kPadSlotBytes));
}

// the tongue is an EAT attempt: its hit flag on a puppet's body never becomes a
// HIT claim (the referee client reads the flags after this runs)
void clear_tongue_bits() {
    const ArenaStats st = vs_arena_stats();
    const int me = vs_arena_my_seat();
    for (int k = 0; k < st.bodies && k < kArenaSeats; ++k) {
        if (k == me) continue;
        char *b = body_of(k);
        if (!b) continue;
        unsigned &f = *(unsigned *)(b + kBodyClsnHit);
        if (f & kTongueFlag) { f &= ~(unsigned)kTongueFlag; ++g.tongue_bits; }
    }
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
    if (!g.wrapped || data_0209f2d8 != 1) return;
    if (part2()) {
        if (!g.sink_set) { g.sink_set = true; vs_ref_set_event_sink(on_event); }
        e_monitor();
        v_monitor();
    }
    probe_tongue();
    if (g.rig == 3) rig_stale();
    if (data_0209f2d8 == 1) sanitize_mouth();
    clear_tongue_bits();             // part 1: a refused or an eating tongue is never a HIT
    if (g.rig) rig_step();
}

void vs_eat_after_walk(int frame) {
    (void)frame;
    if (!g.wrapped || !part2()) return;
    e_after_walk();
}

VsEatStats vs_eat_stats() {
    VsEatStats s;
    s.wrapped = g.wrapped;
    s.asked = g.asked;
    s.refused = g.refused;
    s.rig_tongues = g.rig_tongues;
    s.claims = g.claims;
    s.confirmed = g.confirmed;
    s.victim_eaten = g.victim_eaten;
    return s;
}

}  // namespace port
