// PARTY HITS, game side. Rules: hal/party_hits_core.h. Overview: hal/party_hits.h and
// cloud_tasks/PLAYINT1_DESIGN.md. NOT YET BUILT OR RUN ON A DEVICE (written on a Linux VM): every ROM offset
// is taken from code that already runs (party.cpp, party_cyl.cpp, vs_arena_ref.cpp apply_hit_on_me), the
// guesses are marked TUNE.
//
// PORT_HOST_ABI. Reads Player fields; writes only MY OWN body (the victim's console moves its own player, the
// attacker's console never writes a friend's body except the cosmetic scale).

#include "party_hits.h"
#include "party_hits_core.h"
#include "party.h"
#include "comms_hub.h"
#include "coop_wire.h"
#include "hub_presence.h"
#include "player_fields.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250;       // my player slot
extern signed char data_0209f2f8;         // level (sublevel id)
extern signed char data_02092120;         // area
extern int data_0209f220[];               // [0] & 0xff = act
extern unsigned char data_0209f2d8[];     // stage mode (1 = normal play)
extern unsigned char data_0209fc48[];     // (s32) cutscene
extern unsigned char data_ov002_0211013c[];   // the neutral state the ROM returns to (vs_arena_eat.cpp)
int _ZN6Player4HurtERK7Vector3j5Fix12IiEjjj(void *self, const void *pos, unsigned dmg_units, int speed,
                                           unsigned char a4, unsigned char a5, unsigned char a6);
void _ZN6Player11ChangeStateERNS_5StateE(void *self, void *state);
}
namespace port { int vs_arena_state_index(const void *body); }

namespace port {
namespace party_hits {
namespace {

using namespace phit;

enum : unsigned {
    kVertSpeed = 0xa8, kHorzSpeed = 0x98,       // apply_hit_on_me writes both after Hurt (vs_arena_ref.cpp)
    kNoControl = 0x709, kInMouth = 0xd0,        // my_body_may_act reads both
    kYawAngle = 0x8e,
    kBounceUp = 0x30000,                        // TUNE: the attacker's hop off a friend's head (Fix12 / frame)
    kSendGapFrames = 30,                        // the attacker's own re-claim gap per victim (< relay cap 10/s)
};

struct G {
    bool read = false, installed = false, on = false, announced = false;
    int log = 0;
    Judge judge;
    EatHold hold;
    EatClaimBook book;
    unsigned frame = 0;
    unsigned send_frame[kHubPartySeats] = {};
    unsigned counter = 0;
    int prev_state = 0xFF, state = 0xFF;
    unsigned in_state = 0;
    // a believed claim waits here for the frame hook (the pump runs earlier in the frame)
    bool pend = false; Claim pend_claim; Effect pend_fx;
    // squash shown on the friend I stomped
    unsigned squash_until[kHubPartySeats] = {};
    // lab
    char forge_kind = 0; int forge_units = 0; bool forged = false;
    Stats st = {};
} g;

void read_env() {
    if (g.read) return;
    g.read = true;
    g.judge.tog.interact = env_flag(std::getenv("SM64DS_ROOM_INTERACT"), true);
    g.judge.tog.damage = env_flag(std::getenv("SM64DS_ROOM_DAMAGE"), false);
    if (const char *e = std::getenv("SM64DS_PHIT_LOG")) g.log = std::atoi(e);
    Rules &r = g.judge.rules;
    r.check_reach = !env_flag(std::getenv("SM64DS_PHIT_NOREACH"), false);
    r.check_level = !env_flag(std::getenv("SM64DS_PHIT_NOLEVEL"), false);
    r.check_cooldown = !env_flag(std::getenv("SM64DS_PHIT_NOCOOLDOWN"), false);
    r.check_pos = !env_flag(std::getenv("SM64DS_PHIT_NOPOS"), false);
    r.check_busy = !env_flag(std::getenv("SM64DS_PHIT_NOBUSY"), false);
    if (const char *e = std::getenv("SM64DS_PHIT_LAB")) {
        char kind[16] = {0}; int units = 0;
        if (std::sscanf(e, "forge:%15[a-z]:%d", kind, &units) == 2) {
            g.forge_units = units;
            g.forge_kind = !std::strcmp(kind, "melee") ? 'm' : !std::strcmp(kind, "pound") ? 'p' :
                           !std::strcmp(kind, "stomp") ? 's' : !std::strcmp(kind, "eat") ? 'e' : 0;
        }
    }
}

char *my_body() {
    const int me = (int)data_0209f250;
    return (me >= 0 && me < 16) ? (char *)data_0209f394[me] : nullptr;
}
int u(int fix12) { return fix12 >> 12; }

Place my_place() {
    Place p; p.level = (int)(unsigned char)data_0209f2f8; p.area = (int)data_02092120; p.act = data_0209f220[0] & 0xff;
    return p;
}

// Can my body be hit / held right now? (the same set of reasons vs_arena_ref.cpp's my_body_may_act and the
// referee's _hit use, plus a level in transition)
bool me_busy(const char *me) {
    const int st = port::vs_arena_state_index(me);
    if (st != 0xFF && state_blocks_hit(st)) return true;
    if (*(const unsigned short *)(me + player::kInvincibleTimer) > 0) return true;
    if (*(const unsigned char *)(me + 0x708) || *(const unsigned char *)(me + kNoControl)) return true;
    if (*(void *const *)(me + kInMouth)) return true;
    if (data_0209f2d8[0] != 1) return true;
    if (*(const int *)data_0209fc48 != 0) return true;
    return false;
}

bool see(int seat, Seen *s) {
    HubPresence p; unsigned age = 0;
    s->have_presence = hub_presence_get(seat, &p, &age);
    if (s->have_presence) {
        s->presence_age_ms = age;
        s->in_level = p.in_level() && !p.in_menu() && !p.in_cutscene();
        s->at.level = p.level; s->at.area = p.area; s->at.act = p.act;
    }
    HubSeatInfo si;
    s->have_ghost = comms_hub_seat_info(seat, &si) && si.has_ghost;
    if (s->have_ghost) { s->ghost_age_ms = si.ghost_ms_ago; s->x = u(si.x); s->y = u(si.y); s->z = u(si.z); }
    return s->have_presence;
}

// ---- transport ------------------------------------------------------------------------------------
bool send_wire(int to_seat, unsigned ikind, int yaw, int x, int y, int z) {
    const unsigned seq = pack_seq(ikind, ++g.counter);
    unsigned char d[64];
    const int n = hub_party_hit_encode(to_seat, kHitShove, seq, yaw, x << 12, y << 12, z << 12, d);
    if (!n) return false;
    if (comms_hub_room_kind() == kHubRoomCoop) {
        // coop rooms do not route SYNH (relay: party only): the same 24 bytes ride a SYNB sub-type
        unsigned char c[64];
        const int m = hub_coop_encode(kHubTagCoopCtl, kCoopCtlSubtype, seq, d, n, c, (int)sizeof c);
        return m && comms_hub_send(c, m);
    }
    return comms_hub_send(d, n);
}

// ---- victim side ----------------------------------------------------------------------------------
void log_verdict(const Claim &c, Verdict v) {
    ++g.st.claims_recv;
    if (v != kOk) { ++g.st.refused; ++g.st.refused_by[v & 15]; }
    if (g.log > 0 || v == kOk)
        std::fprintf(stderr, "[phit] %s ik=%u from seat %d (claimed %d,%d,%d) -> %s\n", v == kOk ? "APPLY" : "REFUSE", c.ikind,
                     c.attacker, c.x, c.y, c.z, verdict_name(v));
}

void handle_claim(int attacker, unsigned ikind, unsigned counter, int yaw, int x, int y, int z) {
    char *me = my_body();
    if (!me) return;
    Claim c;
    c.attacker = attacker; c.victim = comms_hub_my_seat(); c.ikind = ikind; c.counter = counter; c.yaw = yaw;
    c.x = u(x); c.y = u(y); c.z = u(z);
    Seen s;
    see(attacker, &s);
    Me m;
    m.at = my_place(); m.x = u(*player::pos_x(me)); m.y = u(*player::pos_y(me)); m.z = u(*player::pos_z(me));
    m.busy = me_busy(me);
    m.eaten = g.hold.active();
    Effect fx;
    const Verdict v = g.judge.judge(c, s, m, g.frame, &fx);
    log_verdict(c, v);
    if (v != kOk || g.pend) return;      // (one believed claim per frame; the cooldown makes a second one rare)
    g.pend = true; g.pend_claim = c; g.pend_fx = fx;
}

void on_hit(int seat, const unsigned char *d, int len, void *) {
    if (!g.on || len != (int)sizeof(HubPartyHitWire)) return;
    HubPartyHitWire w; std::memcpy(&w, d, sizeof w);
    const unsigned ik = seq_ikind(w.seq), ctr = seq_counter(w.seq);
    if (ikind_is_reply(ik)) {
        if (ik == kEatAck && g.book.on_ack(seat, g.frame)) std::fprintf(stderr, "[phit] EAT ack from seat %d\n", seat);
        else if (ik == kEatRelease && g.book.on_release(seat)) std::fprintf(stderr, "[phit] EAT release from seat %d\n", seat);
        return;
    }
    handle_claim(seat, ik, ctr, w.yaw, w.x, w.y, w.z);
}

// Apply a believed shove / stomp to my own body: the ROM's own hurt entry with zero damage, then the
// knock speeds re-asserted the way apply_hit_on_me does after Hurt.
void apply_effect(char *me, const Claim &c, const Effect &fx) {
    int v[3] = { c.x << 12, c.y << 12, c.z << 12 };
    // a5 = 0: Hurt adds 2 to the damage when a5 != 0 (the arena's variant select), which would make a "no damage" shove cost 2
    const int ok = _ZN6Player4HurtERK7Vector3j5Fix12IiEjjj(me, v, (unsigned)fx.damage_units, fx.speed, 0, 0, 0);
    if (!ok) { ++g.st.refused; return; }
    *(int *)(me + kHorzSpeed) = fx.speed;
    *(int *)(me + kVertSpeed) = fx.hop;
    ++g.st.applied;
}

void eat_begin(char *me, const Claim &c) {
    g.hold.begin(c.attacker, c.yaw, g.frame);
    *(unsigned char *)(me + kNoControl) = 1;           // TUNE: the flag my_body_may_act reads
    ++g.st.eats_began;
    send_wire(c.attacker, kEatAck, 0, 0, 0, 0);
    std::fprintf(stderr, "[phit] EAT hold begins, eater seat %d\n", c.attacker);
}

void eat_end(char *me, bool spit) {
    const int eater = g.hold.eater();
    *(unsigned char *)(me + kNoControl) = 0;
    _ZN6Player11ChangeStateERNS_5StateE(me, data_ov002_0211013c);
    if (spit) {
        *(short *)(me + kYawAngle) = (short)g.hold.spit_yaw();
        *(int *)(me + kHorzSpeed) = 0x20000;           // TUNE: ~kEatSpitUnits over the next frames
        *(int *)(me + kVertSpeed) = 0x18000;
        ++g.st.eats_spat;
    } else {
        ++g.st.eats_aborted;
    }
    g.hold.force_end();
    send_wire(eater, kEatRelease, 0, 0, 0, 0);
    std::fprintf(stderr, "[phit] EAT %s (eater seat %d)\n", spit ? "spat out" : "released early", eater);
}

void eat_tick(char *me) {
    if (!g.hold.active()) return;
    const int eater = g.hold.eater();
    Seen s;
    see(eater, &s);
    EatInput in;
    in.toggle_on = g.judge.tog.interact;
    const Place mp = my_place();
    in.eater_present = s.have_presence && s.in_level && s.presence_age_ms <= (unsigned)kPresenceMaxAgeMs &&
                       s.at.level == mp.level && s.at.area == mp.area && s.have_ghost && s.ghost_age_ms <= (unsigned)kGhostMaxAgeMs;
    in.me_dead_or_menu = data_0209f2d8[0] != 1;
    const EatStep step = g.hold.tick(g.frame, in);
    if (step == kEatHolding) {
        int ox, oy, oz;
        EatHold::mouth_pos(s.x, s.y, s.z, g.hold.spit_yaw(), &ox, &oy, &oz);
        *player::pos_x(me) = ox << 12; *player::pos_y(me) = oy << 12; *player::pos_z(me) = oz << 12;
        *(int *)(me + kHorzSpeed) = 0; *(int *)(me + kVertSpeed) = 0;
    } else if (step == kEatSpit) {
        eat_end(me, true);
    } else if (step == kEatAbort) {
        eat_end(me, false);
    }
}

// ---- attacker side --------------------------------------------------------------------------------
void track_pose(const char *me) {
    const int st = port::vs_arena_state_index(me);
    if (st != g.state) { g.prev_state = g.state; g.state = st; g.in_state = 0; } else { ++g.in_state; }
}

void attacker_scan(char *me) {
    Pose p;
    p.state = g.state; p.prev_state = g.prev_state; p.frames_in_state = g.in_state;
    p.vy = *(const int *)(me + kVertSpeed);
    int ik = attack_for_pose(p);
    const int mx = u(*player::pos_x(me)), my = u(*player::pos_y(me)), mz = u(*player::pos_z(me));
    const int yaw = *(const short *)(me + kYawAngle) & 0xFFFF;
    const bool lab = g.forge_kind && !g.forged;
    if (lab) ik = g.forge_kind == 'm' ? kMelee : g.forge_kind == 'p' ? kPound : g.forge_kind == 's' ? kStomp : kEatClaim;
    if (ik < 0 || (ik == kEatClaim && g.book.open())) return;
    for (int seat = 0; seat < kHubPartySeats; ++seat) {
        if (seat == comms_hub_my_seat() || !party::seat_is_party(seat) || party::gate(seat) != party::kGateOk) continue;
        if (g.send_frame[seat] && g.frame - g.send_frame[seat] < (unsigned)kSendGapFrames) continue;
        Seen s;
        if (!see(seat, &s) || !s.have_ghost) continue;
        if (lab) {
            // LAB: forge a claim from `units` away from the friend, whatever the real positions are
            send_wire(seat, (unsigned)ik, yaw, s.x + g.forge_units, s.y, s.z);
            g.forged = true; g.send_frame[seat] = g.frame ? g.frame : 1;
            std::fprintf(stderr, "[phit] LAB forged ik=%d claim %d units from seat %d\n", ik, g.forge_units, seat);
            return;
        }
        if (!attacker_contact((unsigned)ik, mx, my, mz, yaw, s.x, s.y, s.z)) continue;
        send_wire(seat, (unsigned)ik, yaw, mx, my, mz);
        g.send_frame[seat] = g.frame ? g.frame : 1;
        ++g.st.claims_sent;
        if (ik == kEatClaim) g.book.sent(seat, g.frame);
        if (ik == kStomp) {
            *(int *)(me + kVertSpeed) = kBounceUp;             // my own bounce, whatever the friend's console decides
            g.squash_until[seat] = g.frame + 20;
            ++g.st.bounces;
        }
        if (g.log > 0) std::fprintf(stderr, "[phit] claim sent ik=%d -> seat %d (my %d,%d,%d; his %d,%d,%d)\n", ik, seat, mx, my, mz, s.x, s.y, s.z);
        return;                                                // one claim per frame
    }
}

}  // namespace

bool active() { read_env(); return g.on; }

void on_coop_ctl(int seat, const unsigned char *pl, int len) {
    if (!g.on || !pl || len < (int)sizeof(HubPartyHitWire)) return;
    HubPartyHitWire w; std::memcpy(&w, pl, sizeof w);
    if (w.tag != kHubTagPartyHit || w.version != 1 || w.victim != (unsigned char)comms_hub_my_seat() || seat == w.victim) return;
    const unsigned ik = seq_ikind(w.seq), ctr = seq_counter(w.seq);
    if (ikind_is_reply(ik)) {
        if (ik == kEatAck) g.book.on_ack(seat, g.frame); else g.book.on_release(seat);
        return;
    }
    handle_claim(seat, ik, ctr, w.yaw, w.x, w.y, w.z);
}

// coop_world's SYNB dispatcher hands over the payload after the 12-byte head.
static void coop_sub(int seat, int, const unsigned char *pl, int len, void *) { on_coop_ctl(seat, pl, len); }

int squash_scale_for_seat(int seat) {
    if (seat < 0 || seat >= kHubPartySeats || !g.on || g.frame >= g.squash_until[seat]) return 0x1000;
    return 0x800;                                              // TUNE: half height
}

const Stats &stats() { return g.st; }

}  // namespace party_hits
}  // namespace port

extern "C" void port_party_hits_frame(void) {
    using namespace port;
    using namespace port::party_hits;
    read_env();
    if (!g.installed) {
        /* comms_hub_room_kind() reads kHubRoomWorld until the hub is live, so
           deciding earlier would leave interactions off for the whole run.
           Re-evaluate every frame and latch only once the hub is live. */
        if (!comms_hub_active() || comms_hub_state() != kHubLive) return;
        g.installed = true;
        const HubRoomKind rk = comms_hub_room_kind();
        g.on = g.judge.tog.interact && (rk == kHubRoomParty || rk == kHubRoomCoop);
        g.judge.tog.room = rk == kHubRoomParty || rk == kHubRoomCoop;
        if (rk == kHubRoomParty) hub_on_kind(kHubPartyHit, on_hit, nullptr);
        if (rk == kHubRoomCoop && g.on) {              // hook 3: coop receives claims on SYNB 0x41
            const bool ok = coop_ctl_register(kCoopCtlSubtype, kCoopCtlSubtype, coop_sub, nullptr);
            std::fprintf(stderr, "[phit] coop SYNB 0x%02x %s\n", (unsigned)kCoopCtlSubtype, ok ? "registered" : "REGISTER FAILED");
        }
        std::fprintf(stderr, "[phit] %s (room %d, interact %d, damage %d)\n", g.on ? "on" : "off: not a party/coop room or interactions off",
                     (int)rk, g.judge.tog.interact ? 1 : 0, g.judge.tog.damage ? 1 : 0);
    }
    if (!g.on) return;
    ++g.frame;
    char *me = my_body();
    if (!me) return;
    track_pose(me);
    g.book.tick(g.frame);
    if (g.pend) {
        g.pend = false;
        if (g.pend_fx.eat) eat_begin(me, g.pend_claim); else apply_effect(me, g.pend_claim, g.pend_fx);
    }
    eat_tick(me);
    if (!g.hold.active()) attacker_scan(me);
}
