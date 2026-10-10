// PARTY BODY CYLINDER + THE SPIKE'S LAB. See hal/party.h and out/PBODY1/SPIKE.md.
//
// A party body's Behavior never runs on this console, so nothing registers its
// body cylinder (Player::Behavior ends with dCc_c::Clear then Update on +0x2d4).
// This does both for it, once a frame, from the follower, after the pose and
// before Stage::Render's CylinderClsn::Process pairs the list (the same two ROM
// calls hal/vs_arena.cpp's puppet makes; this file owns its own copy).
//
// THE MASK. dCc_c::Process (src/_ZN5dCc_c7ProcessEv.cpp) pairs two cylinders only
// when (A.flags & B.vuln) | (A.vuln & B.flags) != 0, writes each one's `flags`
// into the other's hitFlags, and pushes both apart by half the overlap unless one
// has flag bit 2 or 4. A Player body is flags 0x400008, vuln 0x04f3ffe0
// (src/func_ov002_020e63a4.c). The party cylinder is flags 0, vuln 0x00400000:
//   me (0x400008) & party vuln (0x400000) = 0x400000  -> paired with my body, pushed
//   party flags (0)          & anything vuln          -> 0
//   party vuln (0x400000)    & a world cylinder flags -> 0 unless that cylinder
//                                                         carries the player bit
// and the audit below checks the last line on the live list, every frame, for
// every cylinder it is about to pair. Whatever it pairs with is handed hitFlags 0,
// the value nothing treats as a hit. The party cylinder is withheld for the frame
// if the audit finds a non-player node that would pair.
//
// Knobs (lab; off by default):
//   SM64DS_PARTY_CYL_MASK=0   MUST-FAIL: the cylinder carries a real body's flags /
//                             vuln (0x400008 / 0x04f3ffe0), as if no mask existed
//   SM64DS_PARTY_GUARD=0      MUST-FAIL: no audit, never withheld
//   SM64DS_PARTY_PAD=<units>  radius pad added to the 40-unit body radius
//   SM64DS_PARTY_LAB=sweep[:start[:step[:max]]]   the spike: after `start` follower
//                             passes (default 300) park the first party body over
//                             every actor of the level in turn (`step` passes each,
//                             default 8; at most `max` actors, default 400), print
//                             a [partylab] line per actor and a summary
//   SM64DS_PARTY_LAB=far      the control: the same sweep's metrics with the body
//                             left far away
//   SM64DS_PARTY_LAB=at:<x>,<y>,<z>   park the first party body at that world point
//                             (units) for the whole run

#include "party.h"
#include "player_fields.h"
#include "comms_hub.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
// the collision list: a body whose Behavior is skipped registers its own
// cylinder, the two calls Player::Behavior ends with
void _ZN5dCc_c5ClearEv(void *self);
void _ZN5dCc_c6UpdateEv(void *self);
void _ZN10dCcAcPos_c21SetPosRelativeToActorERK7Vector3(void *self, const void *v);
extern void *data_0209cee8;     // the head of the active collision list
extern void *data_0209f394[];
extern unsigned char data_0209f250;
extern unsigned char data_0209f21c;
extern signed char data_0209f2f8;
void *_ZN8dActor_c4NextEPKS_(const void *prev);
short NumCoins(void);
unsigned char NumStars(void);
signed char NumRedCoins(void);
}

namespace port {
namespace party {
namespace {

enum : unsigned {
    kBodyClsn = 0x2d4,        // mdCcAcPos_c
    kAttackClsn = 0x314,
    kBodyClsnOn = 0x713,
    kPartyFlags = 0x00000000u,
    kPartyVuln = 0x00400000u,
    kBodyFlags = 0x00400008u,         // a real Player body cylinder (the MUST-FAIL arm)
    kBodyVuln = 0x04f3ffe0u,
    kRadius = 0x28000,                // 40 units, Fix12
    kHeight = 0x96000,                // 150 units
};

struct Rules {
    bool masked = true;
    bool guard = true;
    int pad = 0;
    bool read = false;
};
Rules g_r;

void read_rules() {
    if (g_r.read) return;
    g_r.read = true;
    if (const char *e = std::getenv("SM64DS_PARTY_CYL_MASK")) g_r.masked = std::atoi(e) != 0;
    if (const char *e = std::getenv("SM64DS_PARTY_GUARD")) g_r.guard = std::atoi(e) != 0;
    if (const char *e = std::getenv("SM64DS_PARTY_PAD")) g_r.pad = std::atoi(e);
}

// dCc_c::Unlink (src/_ZN5dCc_c6UnlinkEv.cpp, which has no flat host name): take a
// node out of the active list, a no-op for one that is not in it.
void unlink_node(char *node) {
    char *&prev = *(char **)(node + 0x28);
    char *&next = *(char **)(node + 0x2c);
    if (prev) *(char **)(prev + 0x2c) = next;
    else if ((char *)data_0209cee8 == node) data_0209cee8 = next;
    if (next) *(char **)(next + 0x28) = prev;
    prev = 0;
    next = 0;
}

bool readable(const void *p, unsigned n) {
    return p && !::IsBadReadPtr(p, n);
}

// Is `node` one of the cylinders of a Player body (mine, or a party body's)?
bool player_node(const char *node) {
    const int nb = (int)data_0209f21c < 16 ? (int)data_0209f21c : 16;
    for (int i = 0; i < nb; ++i) {
        const char *b = (const char *)data_0209f394[i];
        if (!b) continue;
        if (node >= b + 0x100 && node < b + 0x360) return true;   // the body's own cylinders
    }
    return false;
}

void describe(const char *n, unsigned fl, unsigned vu) {
    unsigned cls = 0xffff, uid = 0;
    const char *owner = readable(n + 0x30, 4) ? *(const char *const *)(n + 0x30) : nullptr;
    if (readable(owner, 0x10)) {
        cls = *(const unsigned short *)(owner + 0xc);
        uid = *(const unsigned *)(owner + 4);
    }
    std::fprintf(stderr, "[partycyl] SHARED node=%p flags=%08x vuln=%08x owner_class=0x%x uid=%u\n",
                 (const void *)n, fl, vu, cls, uid);
}

unsigned party_flags() { return g_r.masked ? kPartyFlags : kBodyFlags; }
unsigned party_vuln() { return g_r.masked ? kPartyVuln : kBodyVuln; }

}  // namespace

// How many nodes of the live list would pair with the party cylinder. A Player
// body's own cylinders are expected to (that is the push) and are not counted.
int cyl_audit(void *) {
    read_rules();
    Stats &st = stats_rw();
    const unsigned pf = party_flags(), pv = party_vuln();
    int shared = 0, walked = 0;
    for (const char *n = (const char *)data_0209cee8; n && walked < 4096;
         n = *(const char *const *)(n + 0x2c), ++walked) {
        ++st.audit_nodes;
        if (player_node(n)) continue;
        const unsigned fl = *(const unsigned *)(n + 0x18);
        const unsigned vu = *(const unsigned *)(n + 0x1c);
        if (((pf & vu) | (pv & fl)) != 0) {
            ++shared;
            ++st.shared_nodes;
            static int said = 0;
            if (said < 12) { ++said; describe(n, fl, vu); }
        }
    }
    return shared;
}

bool cyl_register(void *a) {
    read_rules();
    Stats &st = stats_rw();
    char *c = (char *)a;
    char *node = c + kBodyClsn;
    if (g_r.guard && cyl_audit(a) > 0) {
        // withheld: make sure no stale link of this node survives
        unlink_node(node);
        unlink_node(c + kAttackClsn);
        ++st.guard_skips;
        return false;
    }
    *(int *)(node + 0x04) = kRadius + g_r.pad * 0x1000;
    *(int *)(node + 0x08) = kHeight;
    unsigned *fl = (unsigned *)(node + 0x18);
    *fl = party_flags();                          // bit 0 clear: not disabled
    *(unsigned *)(node + 0x1c) = party_vuln();
    *(unsigned char *)(c + kBodyClsnOn) = 1;
    static const int zero[3] = {0, 0, 0};
    _ZN10dCcAcPos_c21SetPosRelativeToActorERK7Vector3(node, zero);
    // Unlink first: Process empties the list every frame, and a frame that did
    // not run it must not leave this node in the list when Update runs again (a
    // node linked to itself loops the pair pass for ever).
    unlink_node(node);
    _ZN5dCc_c5ClearEv(node);
    _ZN5dCc_c6UpdateEv(node);
    // the attack cylinder: off. Bit 0 of its flags makes Update return, and nothing
    // calls Update on it; it is also taken out of the list.
    unlink_node(c + kAttackClsn);
    *(unsigned *)(c + kAttackClsn + 0x18) |= 1u;
    ++st.registers;
    return true;
}

// ---------------------------------------------------------------------------
// THE LAB
// ---------------------------------------------------------------------------
namespace {

enum Lab { kLabOff = 0, kLabSweep, kLabFar, kLabAt };

struct Target {
    unsigned uid;
    unsigned cls;
    int x, y, z;
};
struct Metrics {
    int coins, stars, red, actors, level;
    bool alive;
};

struct LabState {
    bool read = false;
    Lab mode = kLabOff;
    unsigned start = 300, step = 8, maxt = 400;
    int at[3] = {0, 0, 0};
    int driver = -1;
    unsigned calls = 0;
    bool snapped = false;
    int nt = 0;
    Target t[600];
    Metrics m_prev;
    bool have_prev = false;
    int changed = 0;
    int done = 0;
    unsigned changed_cls[64];
    int nchanged_cls = 0;
};
LabState g_lab;

void lab_read() {
    if (g_lab.read) return;
    g_lab.read = true;
    const char *e = std::getenv("SM64DS_PARTY_LAB");
    if (!e || !*e) return;
    if (!std::strncmp(e, "sweep", 5) || !std::strncmp(e, "far", 3)) {
        g_lab.mode = (e[0] == 's') ? kLabSweep : kLabFar;
        const char *q = std::strchr(e, ':');
        unsigned v[3] = {300, 8, 400};
        for (int i = 0; i < 3 && q; ++i) {
            v[i] = (unsigned)std::atoi(q + 1);
            q = std::strchr(q + 1, ':');
        }
        g_lab.start = v[0]; g_lab.step = v[1] ? v[1] : 8; g_lab.maxt = v[2] > 600 ? 600 : v[2];
    } else if (!std::strncmp(e, "at:", 3)) {
        g_lab.mode = kLabAt;
        std::sscanf(e + 3, "%d,%d,%d", &g_lab.at[0], &g_lab.at[1], &g_lab.at[2]);
    }
}

bool is_body(const void *a) {
    const int nb = (int)data_0209f21c < 16 ? (int)data_0209f21c : 16;
    for (int i = 0; i < nb; ++i)
        if (data_0209f394[i] == a) return true;
    return false;
}

Metrics sample(unsigned watch_uid) {
    Metrics m;
    m.coins = NumCoins();
    m.stars = NumStars();
    m.red = NumRedCoins();
    m.level = (int)(unsigned char)data_0209f2f8;
    m.actors = 0;
    m.alive = false;
    int guard = 0;
    for (const char *a = (const char *)_ZN8dActor_c4NextEPKS_(0); a && guard < 8192;
         a = (const char *)_ZN8dActor_c4NextEPKS_(a), ++guard) {
        ++m.actors;
        if (watch_uid && *(const unsigned *)(a + 4) == watch_uid) m.alive = true;
    }
    return m;
}

void snapshot_targets() {
    g_lab.nt = 0;
    int guard = 0;
    for (const char *a = (const char *)_ZN8dActor_c4NextEPKS_(0);
         a && guard < 8192 && g_lab.nt < (int)g_lab.maxt;
         a = (const char *)_ZN8dActor_c4NextEPKS_(a), ++guard) {
        if (is_body(a)) continue;
        Target &t = g_lab.t[g_lab.nt++];
        t.uid = *(const unsigned *)(a + 4);
        t.cls = *(const unsigned short *)(a + 0xc);
        t.x = *(const int *)(a + 0x5c);
        t.y = *(const int *)(a + 0x60);
        t.z = *(const int *)(a + 0x64);
    }
    std::fprintf(stderr, "[partylab] sweep begins: %d actors, step %u passes, level %d\n", g_lab.nt,
                 g_lab.step, (int)(unsigned char)data_0209f2f8);
}

void lab_note_class(unsigned cls) {
    for (int i = 0; i < g_lab.nchanged_cls; ++i)
        if (g_lab.changed_cls[i] == cls) return;
    if (g_lab.nchanged_cls < 64) g_lab.changed_cls[g_lab.nchanged_cls++] = cls;
}

}  // namespace

void lab_before_register(void *a, int body) {
    lab_read();
    if (g_lab.mode == kLabOff || !a) return;
    if (g_lab.driver < 0) g_lab.driver = body;
    if (body != g_lab.driver) return;
    const unsigned n = g_lab.calls++;
    int *px = player::pos_x(a), *py = player::pos_y(a), *pz = player::pos_z(a);
    if (g_lab.mode == kLabAt) {
        *px = g_lab.at[0] << 12; *py = g_lab.at[1] << 12; *pz = g_lab.at[2] << 12;
        return;
    }
    if (n < g_lab.start) return;
    if (!g_lab.snapped) {
        g_lab.snapped = true;
        snapshot_targets();
        g_lab.m_prev = sample(g_lab.nt ? g_lab.t[0].uid : 0);
        g_lab.have_prev = true;
    }
    const unsigned rel = n - g_lab.start;
    const int k = (int)(rel / g_lab.step);
    const unsigned phase = rel % g_lab.step;
    if (k >= g_lab.nt) {
        if (!g_lab.done) {
            g_lab.done = 1;
            std::fprintf(stderr, "[partylab] DONE mode=%s targets=%d changed=%d classes=[",
                         g_lab.mode == kLabSweep ? "sweep" : "far", g_lab.nt, g_lab.changed);
            for (int i = 0; i < g_lab.nchanged_cls; ++i)
                std::fprintf(stderr, "%s0x%x", i ? "," : "", g_lab.changed_cls[i]);
            std::fprintf(stderr, "] guard_skips=%llu registers=%llu shared_nodes=%llu\n",
                         stats().guard_skips, stats().registers, stats().shared_nodes);
        }
        *px = 0x7fffff00 & ~0xfff; *py = 0; *pz = 0;     // park far
        return;
    }
    if (phase == 0 && k > 0) {
        // what the previous target's dwell changed, read before the body moves on
        const Target &pt = g_lab.t[k - 1];
        const Metrics m = sample(pt.uid);
        const Metrics &p = g_lab.m_prev;
        const bool ch = m.coins != p.coins || m.stars != p.stars || m.red != p.red ||
                        m.actors != p.actors || m.level != p.level || !m.alive;
        if (ch) {
            ++g_lab.changed;
            lab_note_class(pt.cls);
        }
        std::fprintf(stderr, "[partylab] k=%d cls=0x%x uid=%u pos=(%d,%d,%d) coins %d>%d stars %d>%d red %d>%d "
                     "actors %d>%d level %d>%d alive=%d %s\n", k - 1, pt.cls, pt.uid, pt.x >> 12, pt.y >> 12,
                     pt.z >> 12, p.coins, m.coins, p.stars, m.stars, p.red, m.red, p.actors, m.actors,
                     p.level, m.level, m.alive ? 1 : 0, ch ? "CHANGED" : "-");
        g_lab.m_prev = sample(g_lab.t[k].uid);
    }
    if (g_lab.mode == kLabFar) {
        *px = 0x40000000; *py = 0; *pz = 0x40000000;
        return;
    }
    // three altitudes in turn, so a cylinder that sits above or below its actor's
    // origin is crossed too
    static const int alt[3] = {0, -80, 60};
    const Target &t = g_lab.t[k];
    *px = t.x; *pz = t.z;
    *py = t.y + (alt[(phase / 2) % 3] << 12);
}

}  // namespace party
}  // namespace port
