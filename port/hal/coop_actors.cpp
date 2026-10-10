// COOP PHASE 2: LIVE ENEMIES AND MOVING PLATFORMS KEPT IN STEP. Run mp1, lane COOPMOVE1. See coop_actors.h.
//
// PORT_HOST_ABI. Reads and writes live actors by raw offset (the established convention in port/hal); nothing in
// src/ changes. Every write is range-checked: an offset is accepted only inside 0xd0..0x480 and aligned to its
// width, a position only inside +-kMoverMaxUnits world units, and only after the record passed the validator.

#include "coop_actors.h"
#include "coop_world.h"
#include "coop_groups.h"
#include "coop_wire.h"
#include "comms_hub.h"
#include "hub_presence.h"

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
}

namespace port {
namespace {

// ---------------------------------------------------------------------------
// Profiles.
// ---------------------------------------------------------------------------
enum : unsigned { kFPhase = 1u };
struct Field { uint16_t off; uint8_t size; };
struct Prof {
    uint16_t id;
    const char *name;
    uint16_t period, band, snap;
    uint8_t flags, nf;
    Field f[kMoverMaxFields];
    uint16_t hold;          // 0 = none; else a pointer word that is non-zero while something carries the actor
};

#define ROW(id, name, period, band, snap, flags, nf, ...) { id, name, period, band, snap, flags, nf, { __VA_ARGS__ } },
Prof g_prof[256] = {
#include "coop_actor_profiles.inc"
};
#undef ROW
int g_nprof = 0;
short g_prof_of[1024];      // class id -> index into g_prof, -1 none
char  g_prof_name[256][40]; // names of rows added from the profile file

enum : int {
    kSlots = 512, kSeats = 4,
    kFieldLo = 0xd0, kFieldHi = 0x480,
    kStaleMs = 3000, kHeartbeatMs = 1500, kDwellMs = 1000, kRxKeepMs = 4000,
    kPayloadMax = kHubCoopMaxDatagram - kHubCoopHeaderBytes,
};

bool field_ok(const Field &f) {
    if (f.size != 1 && f.size != 2 && f.size != 4) return false;
    if (f.off < kFieldLo || (int)f.off + f.size > kFieldHi) return false;
    return (f.off % f.size) == 0;
}

void prof_index() {
    for (int i = 0; i < 1024; ++i) g_prof_of[i] = -1;
    g_nprof = 0;
    while (g_nprof < 256 && g_prof[g_nprof].id != 0) ++g_nprof;
    for (int i = 0; i < g_nprof; ++i) {
        Prof &p = g_prof[i];
        bool ok = p.id < 1024 && p.nf <= kMoverMaxFields;
        for (int k = 0; ok && k < p.nf; ++k) ok = field_ok(p.f[k]);
        if (!ok) { std::fprintf(stderr, "[coopmove] profile row %d (class %d) rejected\n", i, (int)p.id); p.id = 0; continue; }
        g_prof_of[p.id] = (short)i;
    }
    // classes a player can pick up: their pose is the carrier's, not the Behavior's
    static const struct { uint16_t id, off; } kHold[] = { { 206, 0x390 } };
    for (const auto &h : kHold)
        if (h.id < 1024 && g_prof_of[h.id] >= 0 && g_prof[g_prof_of[h.id]].hold == 0) g_prof[g_prof_of[h.id]].hold = h.off;
}

// SM64DS_COOP_MOVE_PROFILE=<file>: lines `id period band snap flags nf off:size ...`, '#' comments.
void load_profile_file(const char *path) {
    FILE *fp = std::fopen(path, "r");
    if (!fp) { std::fprintf(stderr, "[coopmove] profile file %s: cannot open\n", path); return; }
    char line[512];
    int added = 0;
    while (std::fgets(line, sizeof line, fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        int id, period, band, snap, flags, nf, used = 0;
        if (std::sscanf(line, "%i %i %i %i %i %i%n", &id, &period, &band, &snap, &flags, &nf, &used) < 6) continue;
        if (id <= 0 || id >= 1024 || nf < 0 || nf > kMoverMaxFields || period < 20 || period > 5000) continue;
        Prof np; std::memset(&np, 0, sizeof np);
        np.id = (uint16_t)id; np.period = (uint16_t)period; np.band = (uint16_t)band; np.snap = (uint16_t)snap;
        np.flags = (uint8_t)flags; np.nf = (uint8_t)nf;
        const char *p = line + used;
        bool ok = true;
        for (int k = 0; k < nf; ++k) {
            int off, sz, u2 = 0;
            if (std::sscanf(p, " %i:%i%n", &off, &sz, &u2) < 2) { ok = false; break; }
            p += u2;
            np.f[k].off = (uint16_t)off; np.f[k].size = (uint8_t)sz;
            if (!field_ok(np.f[k])) { ok = false; break; }
        }
        if (ok) if (const char *h = std::strstr(p, "hold=")) np.hold = (uint16_t)std::strtoul(h + 5, nullptr, 0);
        if (!ok) { std::fprintf(stderr, "[coopmove] profile file: bad row for class %d\n", id); continue; }
        int at = -1;
        for (int i = 0; i < g_nprof; ++i) if (g_prof[i].id == id) at = i;
        if (at < 0) {
            if (g_nprof >= 255) continue;
            at = g_nprof++;
            std::snprintf(g_prof_name[at], sizeof g_prof_name[at], "FILE_%d", id);
            np.name = g_prof_name[at];
        } else np.name = g_prof[at].name;
        g_prof[at] = np;
        g_prof_of[id] = (short)at;
        ++added;
    }
    std::fclose(fp);
    std::fprintf(stderr, "[coopmove] profile file %s: %d rows applied\n", path, added);
}

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------
struct Slot {
    char    *a;               // the local actor in this slot this tick, or null
    short    prof;
    unsigned seen;            // consecutive ticks the actor has been seen
    int      owner;           // -1 unknown, my seat, or a remote seat
    unsigned owner_t;         // when it last changed
    unsigned last_pub;
    bool     lvalid;
    int32_t  lx, ly, lz;
    uint32_t lf[kMoverMaxFields];
    bool     heard;           // a remote record for this slot was ever taken
};
struct Rx {
    bool     valid, fresh, snap;
    unsigned t;
    uint16_t cls, cd;
    int32_t  x, y, z;
    int16_t  ax, ay, az;
    uint8_t  rflags, nf;
    uint32_t f[kMoverMaxFields];
};
struct Job { bool on; int passes; unsigned next; int cursor; };

CoopMoveStats g_st = {};
bool g_env = false, g_log = false, g_act = true, g_snap_on = true, g_hyst = true, g_stale_on = true;
unsigned g_sample_every = 0;
unsigned g_dump_at[4]; int g_ndump = 0;
struct ForgeKnob { unsigned tick; char name[12]; bool done; };
ForgeKnob g_forge[8]; int g_nforge = 0;

bool g_regd = false;
int  g_my = -1;
CoopKey g_key;
unsigned g_lf = 0;                // ticks since this group key
unsigned g_spawn_lf = 0; bool g_spawned = false;
unsigned g_ents = 0;
Slot g_slot[kSlots];
Rx   g_rx[kSlots][kSeats];
Job  g_job[kSeats];
unsigned g_req_nonce = 0, g_req_last = 0; int g_req_tries = 0; bool g_req_done = false;
unsigned long long g_snap_base = 0;
bool g_hold = false;              // a joiner publishes nothing until the group's snapshot is in (or it gave up)
double g_tokens = 6.0; unsigned g_tok_t = 0;
unsigned g_stat_t = 0;
int g_hp_prev = -99;

long long wall_ms() {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    return (long long)((((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000ull);
}

void read_env() {
    if (g_env) return;
    g_env = true;
    auto flag = [](const char *n, bool dflt) { const char *e = std::getenv(n); return (e && *e) ? *e != '0' : dflt; };
    g_log = flag("SM64DS_COOP_MOVE_LOG", false);
    g_act = flag("SM64DS_COOP_ACT", true);
    g_snap_on = flag("SM64DS_COOP_MOVE_SNAP", true);
    g_hyst = flag("SM64DS_COOP_MOVE_HYST", true);
    g_stale_on = flag("SM64DS_COOP_MOVE_STALE", true);
    if (const char *s = std::getenv("SM64DS_COOP_MOVE_SAMPLE")) g_sample_every = (unsigned)std::strtoul(s, nullptr, 0);
    if (const char *d = std::getenv("SM64DS_COOP_MOVE_DUMP")) {
        const char *p = d;
        while (*p && g_ndump < 4) {
            char *end = nullptr;
            const unsigned long v = std::strtoul(p, &end, 0);
            if (end == p) break;
            g_dump_at[g_ndump++] = (unsigned)v;
            p = end;
            while (*p == ',' || *p == ';' || *p == ' ') ++p;
        }
    }
    if (const char *f = std::getenv("SM64DS_COOP_MOVE_FORGE")) {
        const char *p = f;
        while (*p && g_nforge < 8) {
            unsigned fr = 0; char nm[12] = {}; int used = 0;
            if (std::sscanf(p, "%u:%11[a-z]%n", &fr, nm, &used) < 2 || used <= 0) break;
            g_forge[g_nforge].tick = fr;
            std::memcpy(g_forge[g_nforge].name, nm, sizeof nm);
            g_forge[g_nforge].done = false;
            ++g_nforge;
            p += used;
            while (*p == ',' || *p == ';' || *p == ' ') ++p;
        }
    }
    prof_index();
    if (const char *pf = std::getenv("SM64DS_COOP_MOVE_PROFILE")) if (*pf) load_profile_file(pf);
    std::fprintf(stderr, "[coopmove] phase 2 %s: %d classes%s%s%s%s\n", g_act ? "on" : "OFF (control)", g_nprof,
                 g_snap_on ? "" : ", snapshot OFF", g_hyst ? "" : ", hysteresis OFF", g_stale_on ? "" : ", stale-owner expiry OFF",
                 g_log ? ", log" : "");
}

// ---------------------------------------------------------------------------
// Raw actor access.
// ---------------------------------------------------------------------------
inline int32_t rd32(const char *a, int off) { int32_t v; std::memcpy(&v, a + off, 4); return v; }
inline int16_t rd16(const char *a, int off) { int16_t v; std::memcpy(&v, a + off, 2); return v; }
inline void wr32(char *a, int off, int32_t v) { std::memcpy(a + off, &v, 4); }
inline void wr16(char *a, int off, int16_t v) { std::memcpy(a + off, &v, 2); }

bool is_player_ptr(const void *a) {
    for (int i = 0; i < 16; ++i)
        if (data_0209f394[i] == a) return true;
    return false;
}

char *local_player() {
    const int me = (int)data_0209f250;
    if (me < 0 || me >= 16) return nullptr;
    return (char *)data_0209f394[me];
}

double dist3(const char *a, int32_t x, int32_t y, int32_t z) {
    const double dx = ((double)rd32(a, 0x5c) - (double)x) / 4096.0;
    const double dy = ((double)rd32(a, 0x60) - (double)y) / 4096.0;
    const double dz = ((double)rd32(a, 0x64) - (double)z) / 4096.0;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Dead, dying, being eaten or being carried: the actor's position is not its Behavior's business.
bool skip_state(const char *a, const Prof &p) {
    if (p.hold && p.hold + 4 <= 0x480 && rd32(a, p.hold) != 0) return true;
    if (*(const unsigned char *)(a + 0x0f) || *(const unsigned char *)(a + 0x0e) == 2) return true;
    const unsigned fl = (unsigned)rd32(a, 0xb0);
    if (fl & 0x60000u) return true;                       // yoshi mouth states
    return false;
}
bool frozen_state(const char *a) {
    const unsigned fl = (unsigned)rd32(a, 0xb0);
    return (fl & 0x10000u) && (fl & 0x38u);               // Behavior suppressed while culled
}

uint32_t read_field(const char *a, const Field &f) {
    uint32_t v = 0;
    std::memcpy(&v, a + f.off, f.size);                   // little-endian host: the low bytes
    return v;
}
void write_field(char *a, const Field &f, uint32_t v) {
    if (!field_ok(f)) return;
    std::memcpy(a + f.off, &v, f.size);
}

int clampi(long long v, int lo, int hi) { return (int)(v < lo ? lo : v > hi ? hi : v); }

// ---------------------------------------------------------------------------
// Sending.
// ---------------------------------------------------------------------------
CoopKeyWire wire_key() {
    CoopKeyWire w; w.level = (uint16_t)g_key.level; w.act = (uint8_t)g_key.act; w.reserved = 0;
    return w;
}

void refill(unsigned now) {
    if (g_tok_t == 0) g_tok_t = now;
    const unsigned dt = now - g_tok_t;
    g_tok_t = now;
    g_tokens = (std::min)(6.0, g_tokens + dt * 0.020);      // 20 datagrams per second, burst 6
}

int fill_rec(unsigned char *out, int slot, const Slot &s, unsigned my_cd, bool frozen) {
    const Prof &p = g_prof[s.prof];
    const char *a = s.a;
    MoverRec r; std::memset(&r, 0, sizeof r);
    r.slot = (uint16_t)slot;
    r.cls = (uint16_t)p.id;
    r.cd = (uint16_t)(my_cd > 65535u ? 65535u : my_cd);
    r.x = rd32(a, 0x5c); r.y = rd32(a, 0x60); r.z = rd32(a, 0x64);
    r.ax = rd16(a, 0x8c); r.ay = rd16(a, 0x8e); r.az = rd16(a, 0x90);
    r.rflags = frozen ? 1 : 0;
    r.nf = p.nf;
    std::memcpy(out, &r, sizeof r);
    for (int k = 0; k < p.nf; ++k) {
        const uint32_t v = read_field(a, p.f[k]);
        std::memcpy(out + sizeof r + 4 * k, &v, 4);
    }
    return (int)sizeof r + 4 * p.nf;
}

int rec_bytes(int prof) { return (int)sizeof(MoverRec) + 4 * g_prof[prof].nf; }

// ---------------------------------------------------------------------------
// Receiving.
// ---------------------------------------------------------------------------
bool prof_known(unsigned cls, int nf) {
    return cls < 1024 && g_prof_of[cls] >= 0 && g_prof[g_prof_of[cls]].nf == nf;
}

void on_world(int seat, int sub, const unsigned char *p, int len, void *);
void on_ctl(int seat, int sub, const unsigned char *p, int len, void *);

void ensure_registered() {
    if (g_regd) return;
    g_regd = true;
    g_my = comms_hub_my_seat();
    coop_world_register(kWorldMoverBatch, 0x1f, on_world);
    coop_ctl_register(kCtlMoverSnapReq, 0x0b, on_ctl);
}

bool member_ok(int seat) {
    return seat >= 0 && seat < kSeats && seat != g_my && ((coop_group_member_mask() >> seat) & 1u);
}

void on_world(int seat, int sub, const unsigned char *p, int len, void *) {
    if (!g_act) { ++g_st.drop_off; return; }
    if (seat < 0 || seat >= kSeats || seat == g_my) { ++g_st.drop_seat; return; }
    if (sub != kWorldMoverBatch) { ++g_st.drop_bad; return; }
    if (!coop_mover_batch_validate(p, len)) { ++g_st.drop_bad; return; }
    MoverBatchHead h; std::memcpy(&h, p, sizeof h);
    if (h.pver != kMoverProfileVer) { ++g_st.drop_ver; return; }
    ++g_st.batches_rx;
    if (!g_key.valid() || h.key.level != (uint16_t)g_key.level || h.key.act != (uint8_t)g_key.act) { ++g_st.drop_group; return; }
    if (!member_ok(seat)) { ++g_st.drop_member; return; }
    if ((h.flags & 1) && !g_snap_on) return;
    const unsigned now = comms_hub_now_ms();
    int off = (int)sizeof h;
    for (int i = 0; i < (int)h.count; ++i) {
        MoverRec r; std::memcpy(&r, p + off, sizeof r);
        Rx &x = g_rx[r.slot][seat];
        x.valid = true; x.fresh = true; x.snap = (h.flags & 1) != 0; x.t = now;
        x.cls = r.cls; x.cd = r.cd; x.x = r.x; x.y = r.y; x.z = r.z;
        x.ax = r.ax; x.ay = r.ay; x.az = r.az; x.rflags = r.rflags; x.nf = r.nf;
        std::memcpy(x.f, p + off + sizeof r, 4u * r.nf);
        off += (int)sizeof r + 4 * r.nf;
        ++g_st.recs_rx;
        g_slot[r.slot].heard = true;
    }
}

void on_ctl(int seat, int sub, const unsigned char *p, int len, void *) {
    if (!g_act) { ++g_st.drop_off; return; }
    if (seat < 0 || seat >= kSeats || seat == g_my) { ++g_st.drop_seat; return; }
    if (sub != kCtlMoverSnapReq || !coop_mover_req_validate(p, len)) { ++g_st.drop_bad; return; }
    MoverSnapReq q; std::memcpy(&q, p, sizeof q);
    if (!g_snap_on) return;
    if (!g_key.valid() || q.key.level != (uint16_t)g_key.level || q.key.act != (uint8_t)g_key.act) { ++g_st.drop_group; return; }
    if (!member_ok(seat)) { ++g_st.drop_member; return; }
    if (!coop_group_established()) return;
    Job &j = g_job[seat];
    j.on = true; j.passes = 2; j.next = 0; j.cursor = 0;
    ++g_st.req_served;
}

// ---------------------------------------------------------------------------
// Ownership: the member closest to an actor publishes it (PLAN 1.3). Pure.
// ---------------------------------------------------------------------------
int decide_owner(int cur, int me, unsigned my_cd, bool cur_fresh, unsigned cur_cd, int best, unsigned best_cd, bool hyst) {
    const double F = hyst ? 1.5 : 1.0;
    if (cur >= 0 && cur != me && !cur_fresh) cur = -1;           // the owner went silent
    if (best < 0 && (cur < 0 || cur == me)) return me;           // nobody else claims it
    if (cur == me) {
        if (best >= 0) {
            if (best_cd * F < (double)my_cd) return best;
            if (!(my_cd * F < (double)best_cd) && best < me) return best;   // a tie: the lower seat
        }
        return me;
    }
    if (cur >= 0) {                                              // a fresh remote owner
        if (my_cd * F < (double)cur_cd) return me;
        if (best >= 0 && best != cur && best_cd * F < (double)cur_cd) return best;
        return cur;
    }
    // no owner yet, a remote claim exists
    if (my_cd * F < (double)best_cd) return me;
    if (best_cd * F < (double)my_cd) return best;
    return best < me ? best : me;
}

// ---------------------------------------------------------------------------
// Applying one record.
// ---------------------------------------------------------------------------
int ang_err(int16_t a, int16_t b) {
    int d = (int)(uint16_t)(a - b);
    if (d > 0x8000) d = 0x10000 - d;
    return d;
}

void apply_rec(int slot, Slot &s, Rx &x) {
    const Prof &p = g_prof[s.prof];
    char *a = s.a;
    if (x.cls != p.id || x.nf != p.nf) { ++g_st.cls_mismatch; return; }
    if (skip_state(a, p)) { ++g_st.skipped_state; return; }
    const double err = dist3(a, x.x, x.y, x.z);
    const bool force = x.snap;
    if (!force && err > 6000.0) { ++g_st.drop_far; return; }
    if (force && err > 20000.0) { ++g_st.drop_far; return; }
    const int aerr = (std::max)({ ang_err(rd16(a, 0x8c), x.ax), ang_err(rd16(a, 0x8e), x.ay), ang_err(rd16(a, 0x90), x.az) });
    const bool phase = (p.flags & kFPhase) && p.nf > 0;
    if (!force && !phase && err <= p.band && aerr <= 0x400) { ++g_st.in_band; return; }
    const bool write = force || phase || err > p.snap || aerr > 0x2000;
    if (write) {
        wr32(a, 0x5c, clampi(x.x, -kMoverMaxUnits * 4096, kMoverMaxUnits * 4096));
        wr32(a, 0x60, clampi(x.y, -kMoverMaxUnits * 4096, kMoverMaxUnits * 4096));
        wr32(a, 0x64, clampi(x.z, -kMoverMaxUnits * 4096, kMoverMaxUnits * 4096));
        wr16(a, 0x8c, x.ax); wr16(a, 0x8e, x.ay); wr16(a, 0x90, x.az);
        for (int k = 0; k < p.nf; ++k) write_field(a, p.f[k], x.f[k]);
        ++g_st.applied_write;
        if (force) ++g_st.snap_applied;
    } else {
        // inside the snap radius: close a quarter of the gap per record
        wr32(a, 0x5c, rd32(a, 0x5c) + (int32_t)(((long long)x.x - rd32(a, 0x5c)) / 4));
        wr32(a, 0x60, rd32(a, 0x60) + (int32_t)(((long long)x.y - rd32(a, 0x60)) / 4));
        wr32(a, 0x64, rd32(a, 0x64) + (int32_t)(((long long)x.z - rd32(a, 0x64)) / 4));
        const int16_t cy = rd16(a, 0x8e);
        const int16_t dy = (int16_t)(x.ay - cy);
        if (ang_err(cy, x.ay) > 0x400) wr16(a, 0x8e, (int16_t)(cy + dy / 4));
        ++g_st.applied_ease;
    }
    s.lvalid = false;           // the actor was moved under the publisher's cache
    if (g_log && (g_st.applied_write + g_st.applied_ease) <= 40)
        std::fprintf(stderr, "[coopmove] apply slot=%d cls=%d err=%.1f %s%s wall=%lld\n", slot, (int)p.id, err,
                     write ? "write" : "ease", force ? " snap" : "", wall_ms());
}

// ---------------------------------------------------------------------------
// The per-tick work.
// ---------------------------------------------------------------------------
void reset_level() {
    std::memset(g_slot, 0, sizeof g_slot);
    for (int i = 0; i < kSlots; ++i) { g_slot[i].owner = -1; g_slot[i].prof = -1; }
    std::memset(g_rx, 0, sizeof g_rx);
    std::memset(g_job, 0, sizeof g_job);
    g_lf = 0; g_spawned = false; g_spawn_lf = 0; g_ents = 0;
    g_req_done = false; g_req_tries = 0; g_req_last = 0; g_req_nonce = 0; g_snap_base = g_st.snap_applied; g_hold = g_snap_on;
    for (int i = 0; i < g_nforge; ++i) g_forge[i].done = false;
}

void scan_ents() {
    for (int i = 0; i < kSlots; ++i) { g_slot[i].a = nullptr; }
    unsigned n = 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        const unsigned id = *(const unsigned short *)(a + 0x0c);
        if (id >= 1024 || g_prof_of[id] < 0) continue;
        const short slot = rd16(a, 0xce);
        if (slot < 0 || slot >= kSlots) continue;
        if (is_player_ptr(a)) continue;
        Slot &s = g_slot[slot];
        if (s.a) continue;                                    // two actors in one slot: the first wins
        if (s.prof != g_prof_of[id]) {                        // a different class (or first sight): start clean
            s.prof = g_prof_of[id]; s.seen = 0; s.lvalid = false; s.owner = -1; s.last_pub = 0;
        }
        s.a = a;
        ++s.seen;
        ++n;
    }
    for (int i = 0; i < kSlots; ++i) if (!g_slot[i].a) g_slot[i].seen = 0;
    g_ents = n;
    g_st.ents = n;
}

bool alive_rx(const Rx &x, unsigned now) {
    return x.valid && (!g_stale_on || (unsigned)(now - x.t) <= (unsigned)kStaleMs);
}

void send_batch(bool snap, const unsigned char *recs, int nrecs, int bytes) {
    unsigned char buf[kPayloadMax];
    MoverBatchHead h; std::memset(&h, 0, sizeof h);
    h.key = wire_key(); h.flags = snap ? 1 : 0; h.count = (uint8_t)nrecs; h.pver = kMoverProfileVer;
    std::memcpy(buf, &h, sizeof h);
    std::memcpy(buf + sizeof h, recs, (size_t)bytes);
    if (coop_send_world(kWorldMoverBatch, buf, (int)sizeof h + bytes)) {
        if (snap) { ++g_st.snap_batches_sent; g_st.snap_recs_sent += (unsigned)nrecs; }
        else { ++g_st.batches_sent; g_st.recs_sent += (unsigned)nrecs; }
    }
}

// Ownership, apply, publish: one pass over the slots that exist here.
void process(unsigned now) {
    char *pl = local_player();
    if (!pl) return;
    refill(now);
    unsigned char recs[kPayloadMax]; int nrec = 0, bytes = 0;
    struct Due { int slot; unsigned age; };
    Due due[kSlots]; int ndue = 0;
    for (int slot = 0; slot < kSlots; ++slot) {
        Slot &s = g_slot[slot];
        if (!s.a) continue;
        const Prof &p = g_prof[s.prof];
        const unsigned my_cd = (unsigned)(std::min)(65535.0, dist3(pl, rd32(s.a, 0x5c), rd32(s.a, 0x60), rd32(s.a, 0x64)));
        // 1. who owns this actor
        int best = -1; unsigned best_cd = 0xffffffffu; unsigned cur_cd = 0xffffffffu; bool cur_fresh = false;
        for (int q = 0; q < kSeats; ++q) {
            if (q == g_my) continue;
            const Rx &x = g_rx[slot][q];
            if (!alive_rx(x, now) || x.snap) continue;
            if (q == s.owner) { cur_fresh = true; cur_cd = x.cd; }
            if (best < 0 || x.cd < best_cd) { best = q; best_cd = x.cd; }
        }
        const int was = s.owner;
        int nown = decide_owner(s.owner, g_my, my_cd, cur_fresh, cur_cd, best, best_cd, g_hyst);
        if (g_hyst && was != nown && was >= 0 && (unsigned)(now - s.owner_t) < (unsigned)kDwellMs && (was == g_my || cur_fresh))
            nown = was;                                            // dwell: no flip within a second
        if (nown != was) {
            s.owner = nown; s.owner_t = now;
            if (was >= 0) {
                ++g_st.owner_changes;
                if (g_log && g_st.owner_changes <= 400)
                    std::fprintf(stderr, "[coopmove] owner slot=%d cls=%d %d->%d mycd=%u wall=%lld\n", slot, (int)p.id, was, nown, my_cd, wall_ms());
            }
        }
        // 2. apply what the owner (or a snapshot) sent
        for (int q = 0; q < kSeats; ++q) {
            if (q == g_my) continue;
            Rx &x = g_rx[slot][q];
            if (!x.valid || !x.fresh) continue;
            if ((unsigned)(now - x.t) > (unsigned)kRxKeepMs) { x.fresh = false; continue; }
            if (x.snap) {
                if (s.seen < 2) continue;                           // let the object finish its first tick
            } else if (s.owner != q) { x.fresh = false; continue; }
            apply_rec(slot, s, x);
            x.fresh = false;
        }
        // 3. publish if I own it
        if (!g_act) continue;
        if (!g_hold && s.owner == g_my && s.seen >= 3 && !skip_state(s.a, p) && !frozen_state(s.a)) {
            const unsigned age = now - s.last_pub;
            if (age >= p.period) {
                bool changed = !s.lvalid || rd32(s.a, 0x5c) != s.lx || rd32(s.a, 0x60) != s.ly || rd32(s.a, 0x64) != s.lz;
                for (int k = 0; !changed && k < p.nf; ++k) changed = read_field(s.a, p.f[k]) != s.lf[k];
                if (changed || age >= (unsigned)kHeartbeatMs) { due[ndue].slot = slot; due[ndue].age = age; ++ndue; }
            }
        }
    }
    // publish, most overdue first
    std::sort(due, due + ndue, [](const Due &a, const Due &b) { return a.age > b.age; });
    int i = 0;
    while (i < ndue) {
        nrec = 0; bytes = 0;
        int first = i;
        while (i < ndue && nrec < kMoverMaxRecs && bytes + rec_bytes(g_slot[due[i].slot].prof) + (int)sizeof(MoverBatchHead) <= kPayloadMax) {
            Slot &s = g_slot[due[i].slot];
            const unsigned my_cd = (unsigned)(std::min)(65535.0, dist3(pl, rd32(s.a, 0x5c), rd32(s.a, 0x60), rd32(s.a, 0x64)));
            bytes += fill_rec(recs + bytes, due[i].slot, s, my_cd, false);
            ++nrec; ++i;
        }
        if (nrec == 0) { ++i; continue; }
        if (g_tokens < 1.0) { ++g_st.throttled; break; }          // the rest wait for the next tick
        g_tokens -= 1.0;
        send_batch(false, recs, nrec, bytes);
        for (int k = first; k < i; ++k) {
            Slot &s = g_slot[due[k].slot];
            s.last_pub = now;
            s.lvalid = true; s.lx = rd32(s.a, 0x5c); s.ly = rd32(s.a, 0x60); s.lz = rd32(s.a, 0x64);
            for (int f = 0; f < g_prof[s.prof].nf; ++f) s.lf[f] = read_field(s.a, g_prof[s.prof].f[f]);
        }
    }
    // snapshot replies
    for (int q = 0; q < kSeats; ++q) {
        Job &j = g_job[q];
        if (!j.on) continue;
        if (j.next && (int)(now - j.next) < 0) continue;
        if (!member_ok(q)) { j.on = false; continue; }
        int emitted = 0;
        while (j.cursor < kSlots && emitted < 3) {
            nrec = 0; bytes = 0;
            const int c0 = j.cursor;
            while (j.cursor < kSlots && nrec < kMoverMaxRecs) {
                Slot &s = g_slot[j.cursor];
                const int slot = j.cursor;
                bool mine = s.a && s.seen >= 3 && !skip_state(s.a, g_prof[s.prof]) && s.owner == g_my && (s.heard || coop_group_i_am_owner());
                if (mine && bytes + rec_bytes(s.prof) + (int)sizeof(MoverBatchHead) > kPayloadMax) break;
                ++j.cursor;
                if (!mine) continue;
                const unsigned my_cd = (unsigned)(std::min)(65535.0, dist3(pl, rd32(s.a, 0x5c), rd32(s.a, 0x60), rd32(s.a, 0x64)));
                bytes += fill_rec(recs + bytes, slot, s, my_cd, frozen_state(s.a));
                ++nrec;
            }
            if (nrec > 0) {
                if (g_tokens < 1.0) { j.cursor = c0; ++g_st.throttled; break; }
                g_tokens -= 1.0;
                send_batch(true, recs, nrec, bytes);
                ++emitted;
            }
        }
        if (j.cursor >= kSlots) {
            if (--j.passes > 0) { j.cursor = 0; j.next = now + 250; } else j.on = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Late-join request.
// ---------------------------------------------------------------------------
void maybe_request(unsigned now) {
    if (!g_snap_on || !g_act || g_req_done || !g_spawned) return;
    if (g_lf < g_spawn_lf + 1) return;
    if (coop_group_member_mask() == 0) { g_req_done = true; g_hold = false; return; }   // alone: nobody to ask
    // any snapshot record taken counts as an answer
    if (g_st.snap_applied > g_snap_base) { g_req_done = true; g_hold = false; return; }
    if (g_req_tries >= 5 && (unsigned)(now - g_req_last) >= 300u) { g_req_done = true; g_hold = false; return; }
    if (g_req_tries > 0 && (unsigned)(now - g_req_last) < 300u) return;
    MoverSnapReq q; std::memset(&q, 0, sizeof q);
    q.key = wire_key(); q.nonce = ++g_req_nonce;
    if (coop_send_ctl(kCtlMoverSnapReq, &q, (int)sizeof q)) {
        ++g_st.req_sent; ++g_req_tries; g_req_last = now;
        if (g_log) std::fprintf(stderr, "[coopmove] snapshot request %d wall=%lld\n", g_req_tries, wall_ms());
    }
}

// ---------------------------------------------------------------------------
// Proof knobs.
// ---------------------------------------------------------------------------
void do_sample() {
    char line[8192]; int n = std::snprintf(line, sizeof line, "[mvs] w=%lld lf=%u seat=%d", wall_ms(), g_lf, g_my);
    for (int slot = 0; slot < kSlots && n < (int)sizeof line - 80; ++slot) {
        const Slot &s = g_slot[slot];
        if (!s.a) continue;
        n += std::snprintf(line + n, sizeof line - n, " %d:%d:%d:%d:%d:%d:%d", slot, (int)g_prof[s.prof].id,
                           rd32(s.a, 0x5c) >> 12, rd32(s.a, 0x60) >> 12, rd32(s.a, 0x64) >> 12,
                           (int)(uint16_t)rd16(s.a, 0x8e), (int)(uint16_t)rd16(s.a, 0x90));
    }
    std::fprintf(stderr, "%s\n", line);
}

void do_dump(unsigned at) {
    for (int slot = 0; slot < kSlots; ++slot) {
        const Slot &s = g_slot[slot];
        if (!s.a) continue;
        char line[kFieldHi * 2 + 96]; int n = std::snprintf(line, sizeof line, "[mvd] lv=%d lf=%u slot=%d cls=%d w=", g_key.level, at, slot, (int)g_prof[s.prof].id);
        for (int off = kFieldLo; off < kFieldHi; off += 4) n += std::snprintf(line + n, sizeof line - n, "%08x", (unsigned)rd32(s.a, off));
        std::fprintf(stderr, "%s\n", line);
    }
}

void do_forge(const char *name) {
    unsigned char b[kPayloadMax]; std::memset(b, 0, sizeof b);
    MoverBatchHead h; std::memset(&h, 0, sizeof h);
    h.key = wire_key(); h.count = 1; h.pver = kMoverProfileVer;
    MoverRec r; std::memset(&r, 0, sizeof r);
    int slot = -1, cls = 0;
    for (int i = 0; i < kSlots; ++i) if (g_slot[i].a) { slot = i; cls = g_prof[g_slot[i].prof].id; break; }
    if (slot < 0) { std::fprintf(stderr, "[coopmove-knob] forge %s: no actor\n", name); return; }
    r.slot = (uint16_t)slot; r.cls = (uint16_t)cls; r.cd = 5;
    r.x = rd32(g_slot[slot].a, 0x5c); r.y = rd32(g_slot[slot].a, 0x60); r.z = rd32(g_slot[slot].a, 0x64);
    r.nf = g_prof[g_slot[slot].prof].nf;
    int len = (int)sizeof h + (int)sizeof r + 4 * r.nf;
    bool snapreq = false;
    if (!std::strcmp(name, "cls")) r.cls = (uint16_t)(cls == 200 ? 206 : 200);
    else if (!std::strcmp(name, "slot")) r.slot = 600;
    else if (!std::strcmp(name, "far")) r.x = 100 * 4096 * 1000;                 // 100000 units
    else if (!std::strcmp(name, "nf")) r.nf = (uint8_t)(r.nf + 1);
    else if (!std::strcmp(name, "pver")) h.pver = 9;
    else if (!std::strcmp(name, "key")) h.key.act = (uint8_t)((g_key.act + 1) & 7);
    else if (!std::strcmp(name, "bits")) h.reserved = 1;
    else if (!std::strcmp(name, "short")) len -= 6;
    else if (!std::strcmp(name, "long")) len += 5;
    else if (!std::strcmp(name, "req")) snapreq = true;
    else return;
    std::fprintf(stderr, "[coopmove-knob] forge %s\n", name);
    if (snapreq) {
        MoverSnapReq q; std::memset(&q, 0, sizeof q); q.key = wire_key(); q.key.reserved = 1;   // reserved byte set: invalid
        coop_send_ctl(kCtlMoverSnapReq, &q, (int)sizeof q);
        return;
    }
    std::memcpy(b, &h, sizeof h);
    std::memcpy(b + sizeof h, &r, sizeof r);
    if (len > kPayloadMax) len = kPayloadMax;
    coop_send_world(kWorldMoverBatch, b, len > 0 ? len : 1);
}

// Proof line: the local player lost health. Enemies hurt players in coop (the ROM's own damage is never touched here);
// the line names the nearest synced actor and who owns it, so a row can show a non-owner console taking the hit once.
void watch_hurt(char *pl) {
    const int hp = (int)(signed char)(data_02092144[data_0209f250 & 15] >> 8);
    if (g_hp_prev != -99 && hp < g_hp_prev) {
        int bs = -1; double bd = 1e18;
        for (int i = 0; i < kSlots; ++i)
            if (g_slot[i].a) {
                const double d = dist3(g_slot[i].a, rd32(pl, 0x5c), rd32(pl, 0x60), rd32(pl, 0x64));
                if (d < bd) { bd = d; bs = i; }
            }
        std::fprintf(stderr, "[coopmove] hurt hp %d->%d nearest slot=%d cls=%d dist=%.0f owner=%d me=%d wall=%lld\n", g_hp_prev, hp, bs,
                     bs >= 0 ? (int)g_prof[g_slot[bs].prof].id : -1, bd < 1e17 ? bd : -1.0, bs >= 0 ? g_slot[bs].owner : -1, g_my, wall_ms());
    }
    g_hp_prev = hp;
}

void stats_line() {
    const CoopMoveStats &s = g_st;
    std::fprintf(stderr,
        "[movestat] seat=%d ents=%llu sent b%llu r%llu sb%llu sr%llu req%llu served%llu rx b%llu r%llu applied w%llu e%llu band%llu "
        "skip%llu clsmis%llu snapapp%llu noent%llu owner_ch%llu stale%llu thr%llu drop bad%llu group%llu member%llu ver%llu far%llu off%llu seat%llu\n",
        g_my, s.ents, s.batches_sent, s.recs_sent, s.snap_batches_sent, s.snap_recs_sent, s.req_sent, s.req_served,
        s.batches_rx, s.recs_rx, s.applied_write, s.applied_ease, s.in_band, s.skipped_state, s.cls_mismatch, s.snap_applied,
        s.no_ent, s.owner_changes, s.owner_stale, s.throttled, s.drop_bad, s.drop_group, s.drop_member, s.drop_ver, s.drop_far,
        s.drop_off, s.drop_seat);
}

// ---------------------------------------------------------------------------
// Hooks from coop_world.
// ---------------------------------------------------------------------------
void on_frame(unsigned now) {
    read_env();
    if (!coop_world_active()) return;
    ensure_registered();
    int lv = -1, ac = 0;
    if (!coop_group_established() || !coop_group_key(&lv, &ac)) return;
    char *pl = local_player();
    if (!pl) return;
    const CoopKey nk(lv, ac);
    if (nk != g_key) { reset_level(); g_key = nk; }
    ++g_lf; ++g_st.ticks;
    scan_ents();
    if (!g_spawned && g_ents > 0) { g_spawned = true; g_spawn_lf = g_lf; }
    maybe_request(now);
    process(now);
    if (g_log) watch_hurt(pl);
    if (g_spawned) {
        const unsigned since = g_lf - g_spawn_lf;
        if (g_sample_every && (since % g_sample_every) == 0) do_sample();
        for (int i = 0; i < g_ndump; ++i) if (since == g_dump_at[i]) do_dump(since);
        for (int i = 0; i < g_nforge; ++i)
            if (!g_forge[i].done && since >= g_forge[i].tick) { g_forge[i].done = true; do_forge(g_forge[i].name); }
    }
    if (g_log && (unsigned)(now - g_stat_t) >= 4000u) { g_stat_t = now; stats_line(); }
}

void on_member_gone(int seat) {
    if (seat < 0 || seat >= kSeats || !g_stale_on) return;     // (the stale control keeps a dead owner)
    for (int i = 0; i < kSlots; ++i) {
        g_rx[i][seat].valid = false; g_rx[i][seat].fresh = false;
        if (g_slot[i].owner == seat) { g_slot[i].owner = -1; g_slot[i].owner_t = 0; }
    }
    g_job[seat].on = false;
    if (g_log) std::fprintf(stderr, "[coopmove] seat %d gone, its actors are up for adoption wall=%lld\n", seat, wall_ms());
}

struct Registrar {
    Registrar() {
        coop_on_frame(on_frame);
        coop_on_member_gone(on_member_gone);
    }
} g_registrar;

}  // namespace

// ---------------------------------------------------------------------------
// Public.
// ---------------------------------------------------------------------------
CoopMoveStats coop_move_stats() { return g_st; }

bool coop_mover_req_validate(const void *p, int len) {
    if (!p || len != (int)sizeof(MoverSnapReq)) return false;
    MoverSnapReq q; std::memcpy(&q, p, sizeof q);
    return coop_key_ok(q.key);
}

bool coop_mover_batch_validate(const void *payload, int len) {
    if (!payload || len < (int)sizeof(MoverBatchHead) + (int)sizeof(MoverRec) || len > kPayloadMax) return false;
    const unsigned char *p = static_cast<const unsigned char *>(payload);
    MoverBatchHead h; std::memcpy(&h, p, sizeof h);
    if (!coop_key_ok(h.key) || h.flags > 1 || h.reserved != 0) return false;
    if (h.count < 1 || h.count > kMoverMaxRecs) return false;
    if (g_nprof == 0) prof_index();
    int off = (int)sizeof h;
    for (int i = 0; i < (int)h.count; ++i) {
        if (off + (int)sizeof(MoverRec) > len) return false;
        MoverRec r; std::memcpy(&r, p + off, sizeof r);
        if (r.slot >= kSlots || r.rflags > 1) return false;
        if (!prof_known(r.cls, r.nf)) return false;
        const long long lim = (long long)kMoverMaxUnits * 4096;
        if (r.x < -lim || r.x > lim || r.y < -lim || r.y > lim || r.z < -lim || r.z > lim) return false;
        off += (int)sizeof r + 4 * r.nf;
        if (off > len) return false;
    }
    return off == len;
}

}  // namespace port
