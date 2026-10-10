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

#include <cmath>
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
void *_ZN8dActor_c10FindWithIDEj(unsigned id);
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
    bool cyl = true;             // SM64DS_PARTY_CYL=0: no cylinder at all (the lab's control)
    bool assist = true;          // SM64DS_PARTY_ASSIST=0: the ROM's half push only (the push rows' control)
    int pad = 8;                 // radius pad, units: see SPIKE.md / the push rows
    bool read = false;
};
Rules g_r;

void read_rules() {
    if (g_r.read) return;
    g_r.read = true;
    if (const char *e = std::getenv("SM64DS_PARTY_CYL_MASK")) g_r.masked = std::atoi(e) != 0;
    if (const char *e = std::getenv("SM64DS_PARTY_GUARD")) g_r.guard = std::atoi(e) != 0;
    if (const char *e = std::getenv("SM64DS_PARTY_PAD")) g_r.pad = std::atoi(e);
    if (const char *e = std::getenv("SM64DS_PARTY_CYL")) g_r.cyl = std::atoi(e) != 0;
    if (const char *e = std::getenv("SM64DS_PARTY_ASSIST")) g_r.assist = std::atoi(e) != 0;
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

unsigned party_flags() { return g_r.masked ? kPartyFlags : kBodyFlags; }
unsigned g_extra_vuln = 0;
unsigned party_vuln() { return g_r.masked ? (kPartyVuln | g_extra_vuln) : kBodyVuln; }

}  // namespace

// THE AUDIT. Stage::Render's CylinderClsn::Process runs inside the game tick, before
// the follower (SPIKE.md "the frame order"), so the collision list is already empty
// when this runs: the world's nodes cannot be read before they are paired. What can be
// read is what the last pair pass left on the party node itself: dCc_c::Process
// writes the partner's `flags` into the node's hitFlags (+0x20) and its owner into
// otherOwner (+0x24). A body's own cylinder carries flags 0x400008; anything else
// that is left there paired with a world node. It is counted, the first few are
// named, and the cylinder is withheld for kWithholdFrames (the world node was handed
// hitFlags 0 by that pair, which nothing treats as a hit, so the one frame it already
// had cannot have done anything; the withhold stops it recurring).
enum { kWithholdFrames = 90 };
bool g_reg_last[16];
int g_holdoff[16];

int cyl_audit(void *a) {
    read_rules();
    Stats &st = stats_rw();
    const char *node = (const char *)a + kBodyClsn;
    int walked = 0;
    for (const char *n = (const char *)data_0209cee8; n && walked < 4096;
         n = *(const char *const *)(n + 0x2c), ++walked)
        ++st.audit_nodes;                // nodes in the list when the follower runs: 0 when the pair pass already ran
    const unsigned hf = *(const unsigned *)(node + 0x20);
    const unsigned oo = *(const unsigned *)(node + 0x24);
    if (hf == 0) return 0;
    // a Player body (class 0xbf) is the expected partner whatever state flags it carries
    unsigned cls = 0xffff;
    if (oo) {
        const char *o = (const char *)_ZN8dActor_c10FindWithIDEj(oo);
        if (readable(o, 0x10)) cls = *(const unsigned short *)(o + 0xc);
    }
    if (cls == 0xbf && (hf & 0x400000u)) { ++st.pairs_body; return 0; }
    ++st.shared_nodes;
    ++st.pairs_other;
    static int said = 0;
    if (said < 12) {
        ++said;
        std::fprintf(stderr, "[partycyl] PAIRED a world cylinder: hitFlags=%08x owner uid=%u class=0x%x\n", hf, oo, cls);
    }
    return 1;
}

void set_extra_vuln(unsigned bits) { g_extra_vuln = bits; }

// THE OTHER HALF OF THE PUSH. The ROM pushes each of two overlapping movable cylinders by half
// the overlap (src/_ZN5dCc_c7ProcessEv.cpp:156-165), which is right for two bodies on one
// console and leaves a friend's body half-way inside mine at a run: the friend's console
// pushes the friend by half as well, but only after the packets cross. Here my body is the
// only one that moves, so the half the friend's cylinder would have taken is added to the
// pushback my cylinder already carries from the pair pass (it is consumed by my next Behavior,
// func_ov002_020bf36c): my body leaves the overlap in one frame. Geometry is read from the
// node as the pair pass saw it (position, radius), not from where the body is now.
void cyl_assist(void *a) {
    read_rules();
    if (!g_r.assist || !g_r.cyl) return;
    int body = -1;
    for (int i = 0; i < 16; ++i)
        if (data_0209f394[i] == a) { body = i; break; }
    if (body < 0 || !g_reg_last[body] || g_holdoff[body] > 0) return;
    const int me = (int)data_0209f250;
    char *mb = (me >= 0 && me < 16) ? (char *)data_0209f394[me] : nullptr;
    if (!mb || mb == (char *)a) return;
    if (*(const int *)(mb + 0x37c) != 0) return;          // the Player does not integrate its position then
    const char *pn = (const char *)a + kBodyClsn;          // the party node as the pair pass saw it
    const char *mn = mb + kBodyClsn;
    const long long pr = *(const int *)(pn + 0x04), mr = *(const int *)(mn + 0x04);
    const long long ph = *(const int *)(pn + 0x08), mh = *(const int *)(mn + 0x08);
    const long long dx = (long long)*(const int *)(mn + 0x34) - *(const int *)(pn + 0x34);
    const long long dy = (long long)*(const int *)(mn + 0x38) - *(const int *)(pn + 0x38);
    const long long dz = (long long)*(const int *)(mn + 0x3c) - *(const int *)(pn + 0x3c);
    if (dy >= ph || -dy >= mh) return;                     // no vertical overlap: the pair pass skipped it too
    const double dist = std::sqrt((double)dx * (double)dx + (double)dz * (double)dz);
    const double rsum = (double)(pr + mr);
    if (dist >= rsum) return;
    double ux = -1.0, uz = 0.0;                             // the pair pass's own answer for dist == 0: me pushed -x
    if (dist >= 1.0) { ux = (double)dx / dist; uz = (double)dz / dist; }
    double half = (rsum - dist) * 0.5;
    if (half > (double)(pr + mr) * 0.5) half = (double)(pr + mr) * 0.5;
    *(int *)(mb + 0x2e0) += (int)(ux * half);
    *(int *)(mb + 0x2e8) += (int)(uz * half);
    ++stats_rw().assists;
}

bool cyl_register(void *a, int seat) {
    read_rules();
    Stats &st = stats_rw();
    char *c = (char *)a;
    char *node = c + kBodyClsn;
    if (!g_r.cyl) { unlink_node(node); unlink_node(c + kAttackClsn); return false; }
    int body = -1;
    for (int i = 0; i < 16; ++i)
        if (data_0209f394[i] == a) { body = i; break; }
    if (body >= 0 && g_reg_last[body] && g_r.guard && cyl_audit(a) > 0) g_holdoff[body] = kWithholdFrames;
    if (body >= 0 && g_holdoff[body] > 0) {
        --g_holdoff[body];
        g_reg_last[body] = false;
        unlink_node(node);
        unlink_node(c + kAttackClsn);
        ++st.guard_skips;
        return false;
    }
    if (body >= 0) g_reg_last[body] = true;
    *(int *)(node + 0x04) = kRadius + g_r.pad * 0x1000;
    *(int *)(node + 0x08) = kHeight;
    unsigned *fl = (unsigned *)(node + 0x18);
    *fl = party_flags();                          // bit 0 clear: not disabled
    *(unsigned *)(node + 0x1c) = party_vuln();
    *(unsigned char *)(c + kBodyClsnOn) = 1;
    static const int zero[3] = {0, 0, 0};
    _ZN10dCcAcPos_c21SetPosRelativeToActorERK7Vector3(node, zero);
    // Two bodies on exactly the same spot (a shared entrance) are pushed the same way by the pair
    // pass on both consoles (its answer for a zero distance is +x) and would drift together. One
    // unit of offset, opposite on the two consoles, breaks the tie: the lower seat's body goes one
    // way, the higher the other.
    *(int *)(node + 0x34) += ((seat > comms_hub_my_seat()) ? 1 : -1) * 0x1000;
    // Unlink first: a frame that did not run the pair pass must not leave this node in
    // the list when Update runs again (a node linked to itself loops the pair pass for
    // ever). Unlink of an unlinked node is a no-op.
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

enum Lab { kLabOff = 0, kLabSweep, kLabFar, kLabNone, kLabAt };

struct Target {
    unsigned uid;
    unsigned cls;
    int x, y, z;
};
enum { kSnapMax = 1024 };
struct Snap {
    int n;
    unsigned uid[kSnapMax];
    unsigned short cls[kSnapMax];
    int pos[kSnapMax][3];
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
    Snap snap_prev;
    bool have_prev = false;
    int changed = 0;
    int done = 0;
    unsigned changed_cls[64];
    int nchanged_cls = 0;
    bool have_pos = false;      // this pass has a position for the driver body
    int pos[3] = {0, 0, 0};
};
LabState g_lab;

void lab_read() {
    if (g_lab.read) return;
    g_lab.read = true;
    const char *e = std::getenv("SM64DS_PARTY_LAB");
    if (!e || !*e) return;
    if (!std::strncmp(e, "sweep", 5) || !std::strncmp(e, "far", 3) || !std::strncmp(e, "none", 4)) {
        g_lab.mode = (e[0] == 's') ? kLabSweep : (e[0] == 'f') ? kLabFar : kLabNone;
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

Metrics sample(unsigned watch_uid, Snap *snap = nullptr) {
    Metrics m;
    if (snap) snap->n = 0;
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
        if (snap && snap->n < kSnapMax) {
            snap->uid[snap->n] = *(const unsigned *)(a + 4);
            snap->cls[snap->n] = *(const unsigned short *)(a + 0xc);
            snap->pos[snap->n][0] = *(const int *)(a + 0x5c) >> 12;
            snap->pos[snap->n][1] = *(const int *)(a + 0x60) >> 12;
            snap->pos[snap->n][2] = *(const int *)(a + 0x64) >> 12;
            ++snap->n;
        }
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

// what appeared and what went between two snapshots (uids are unique per actor), with where
void snap_diff(const Snap &a, const Snap &b, char *out, int cap) {
    int o = 0;
    out[0] = 0;
    for (int i = 0; i < a.n && o < cap - 48; ++i) {
        bool found = false;
        for (int j = 0; j < b.n; ++j) if (b.uid[j] == a.uid[i]) { found = true; break; }
        if (!found)
            o += std::snprintf(out + o, (size_t)(cap - o), " -0x%x/%u@(%d,%d,%d)", a.cls[i], a.uid[i],
                               a.pos[i][0], a.pos[i][1], a.pos[i][2]);
    }
    for (int j = 0; j < b.n && o < cap - 48; ++j) {
        bool found = false;
        for (int i = 0; i < a.n; ++i) if (a.uid[i] == b.uid[j]) { found = true; break; }
        if (!found)
            o += std::snprintf(out + o, (size_t)(cap - o), " +0x%x/%u@(%d,%d,%d)", b.cls[j], b.uid[j],
                               b.pos[j][0], b.pos[j][1], b.pos[j][2]);
    }
}

void lab_note_class(unsigned cls) {
    for (int i = 0; i < g_lab.nchanged_cls; ++i)
        if (g_lab.changed_cls[i] == cls) return;
    if (g_lab.nchanged_cls < 64) g_lab.changed_cls[g_lab.nchanged_cls++] = cls;
}

const char *lab_mode_name() {
    return g_lab.mode == kLabSweep ? "sweep" : g_lab.mode == kLabFar ? "far" : g_lab.mode == kLabNone ? "none" : "at";
}

}  // namespace

// Once per follower pass, from party::clear() of the first remote body: the lab's own
// clock. It does not depend on a party body being shown (the control row has none).
void lab_tick(int body) {
    lab_read();
    if (g_lab.mode == kLabOff) return;
    if (g_lab.driver < 0) g_lab.driver = body;
    if (body != g_lab.driver) return;
    g_lab.have_pos = false;
    const unsigned n = g_lab.calls++;
    if (g_lab.mode == kLabAt) {
        g_lab.pos[0] = g_lab.at[0] << 12; g_lab.pos[1] = g_lab.at[1] << 12; g_lab.pos[2] = g_lab.at[2] << 12;
        g_lab.have_pos = true;
        return;
    }
    if (n < g_lab.start) return;
    if (!g_lab.snapped) {
        g_lab.snapped = true;
        snapshot_targets();
        g_lab.m_prev = sample(g_lab.nt ? g_lab.t[0].uid : 0, &g_lab.snap_prev);
        g_lab.have_prev = true;
    }
    const unsigned rel = n - g_lab.start;
    const int k = (int)(rel / g_lab.step);
    const unsigned phase = rel % g_lab.step;
    if (k >= g_lab.nt) {
        if (!g_lab.done) {
            g_lab.done = 1;
            std::fprintf(stderr, "[partylab] DONE mode=%s targets=%d changed=%d classes=[", lab_mode_name(),
                         g_lab.nt, g_lab.changed);
            for (int i = 0; i < g_lab.nchanged_cls; ++i)
                std::fprintf(stderr, "%s0x%x", i ? "," : "", g_lab.changed_cls[i]);
            std::fprintf(stderr, "] guard_skips=%llu registers=%llu pairs_other=%llu\n",
                         stats().guard_skips, stats().registers, stats().pairs_other);
        }
        g_lab.pos[0] = 0x40000000; g_lab.pos[1] = 0; g_lab.pos[2] = 0x40000000;
        g_lab.have_pos = true;                              // far
        return;
    }
    if (phase == 0 && k > 0) {
        // what the previous target's dwell changed, read before the body moves on
        const Target &pt = g_lab.t[k - 1];
        Snap now_snap;
        const Metrics m = sample(pt.uid, &now_snap);
        const Metrics &p = g_lab.m_prev;
        char diff[240];
        snap_diff(g_lab.snap_prev, now_snap, diff, (int)sizeof diff);
        const bool ch = m.coins != p.coins || m.stars != p.stars || m.red != p.red ||
                        m.actors != p.actors || m.level != p.level || !m.alive;
        if (ch) {
            ++g_lab.changed;
            lab_note_class(pt.cls);
        }
        std::fprintf(stderr, "[partylab] k=%d cls=0x%x uid=%u pos=(%d,%d,%d) coins %d>%d stars %d>%d red %d>%d "
                     "actors %d>%d level %d>%d alive=%d %s%s\n", k - 1, pt.cls, pt.uid, pt.x >> 12, pt.y >> 12,
                     pt.z >> 12, p.coins, m.coins, p.stars, m.stars, p.red, m.red, p.actors, m.actors,
                     p.level, m.level, m.alive ? 1 : 0, ch ? "CHANGED" : "-", diff);
        g_lab.m_prev = sample(g_lab.t[k].uid, &g_lab.snap_prev);
    }
    if (g_lab.mode == kLabNone) return;                    // the control: sample, never place
    if (g_lab.mode == kLabFar) {
        g_lab.pos[0] = 0x40000000; g_lab.pos[1] = 0; g_lab.pos[2] = 0x40000000;
        g_lab.have_pos = true;
        return;
    }
    // three altitudes in turn, so a cylinder that sits above or below its actor's
    // origin is crossed too
    static const int alt[3] = {0, -80, 60};
    const Target &t = g_lab.t[k];
    g_lab.pos[0] = t.x; g_lab.pos[2] = t.z;
    g_lab.pos[1] = t.y + (alt[(phase / 2) % 3] << 12);
    g_lab.have_pos = true;
}

// Called by party::follow() on a shown solid body: put it where the lab wants it.
void lab_before_register(void *a, int body) {
    lab_read();
    if (g_lab.mode == kLabOff || !a || body != g_lab.driver || !g_lab.have_pos) return;
    *player::pos_x(a) = g_lab.pos[0];
    *player::pos_y(a) = g_lab.pos[1];
    *player::pos_z(a) = g_lab.pos[2];
}

}  // namespace party
}  // namespace port
