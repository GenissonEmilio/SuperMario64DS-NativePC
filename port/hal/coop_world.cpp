// COOP WORLD CORE (phase 1). Run mp1 wave 56, lane COOPWORLD1. See coop_world.h.
//
// WHAT IT KEEPS EQUAL. The death table (data_0209f4f8): three level parts of
// sixteen words, one bit per placed object that died or was collected, indexed
// by the object's spawn-order slot (actor +0x0ce). The level loader hands every
// placed record the same running slot on every console for the same (level,
// act), and dActor_c::BeforeInitResources makes any actor whose bit is up delete
// itself instead of initialising. So:
//   * a LATE JOINER that holds the group's bits BEFORE its objects spawn never
//     sees the dead things (no spawn logic of ours);
//   * a LIVE bit that arrives is set in the table and any live actor in that
//     slot is removed (reconcile, below), so a kill or pickup on one screen
//     happens on every screen.
// Runtime spawns carry slot -1 and are not covered here (phase 2 / 3).
//
// PORT_HOST_ABI. Reads the ROM's globals and the live-actor walk; nothing in
// src/ changes.

#include "coop_world.h"
#include "coop_groups.h"
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
extern signed char data_0209f2f8;
extern int data_0209f220[];
extern int data_0209f4f8[];
signed char GetLevelPart(int idx);
void *_ZN8dActor_c4NextEPKS_(const void *prev);
void _ZN8dActor_c24KillAndTrackInDeathTableEv(void *self);
}

namespace port {
namespace {

enum : unsigned {
    kHelloMs      = 1000,
    kFullMs       = 1000,
    kEnterWaitMs  = 400,     // no answer in this long = first in
    kEnterResend  = 120,
    kSnapResend   = 250,
    kSnapGiveUpMs = 5000,
    kPresenceMs   = 4000,
    kReconcileEvery = 20,
};
enum : int { kOutMax = 256, kSnapChunks = 2 };

struct Out { uint16_t slot; uint8_t sent; unsigned next; };
struct Job { bool on; uint32_t nonce; CoopKey key; unsigned start, last; };
struct Reg { int lo, hi; CoopSubFn fn; void *user; };

CoopGroups g_grp;
CoopWorldStats g_st = {};
bool g_env_read = false, g_on = false, g_log = false, g_sync = true;
bool g_handlers = false;
int  g_my = -1;
unsigned g_frame = 0, g_est_frames = 0;

CoopKey  g_key;
int      g_part = -1;
unsigned g_prev[kCoopWords];       // shadow of my part: what I have already announced or taken
unsigned g_remote[kCoopWords];     // bits that came from other consoles
bool     g_recon_dirty = false;
unsigned g_last_bits_hash = 0;

Out      g_out[kOutMax];
int      g_out_n = 0;

unsigned g_nonce = 0;              // my enter counter
bool     g_snap_waiting = false;   // the latch hook is waiting for the answer
bool     g_snap_done = false;
uint8_t  g_snap_total = 0;
unsigned g_snap_mask = 0;
unsigned char g_snap_buf[kCoopSnapChunkMax];
int      g_snap_owner = -1;
unsigned g_ack_last = 0;
Job      g_job[CoopGroups::kSeats];

unsigned g_hello_t = 0, g_full_t = 0;
unsigned g_seq_ctl = 0, g_seq_world = 0;
Reg      g_reg_ctl[4], g_reg_world[4];
int      g_nreg_ctl = 0, g_nreg_world = 0;

// proof knobs
struct KillKnob { unsigned frame; int id; int n; bool done; };
KillKnob g_kill[8]; int g_nkill = 0;
unsigned g_goto_frame = 0; int g_goto_id = -1; bool g_goto_done = false;
unsigned g_forge_frame = 0; char g_forge_name[16]; bool g_forge_done = false;

void read_env() {
    if (g_env_read) return;
    g_env_read = true;
    const char *e = std::getenv("SM64DS_COOP");
    g_on = e && *e && *e != '0';
    const char *l = std::getenv("SM64DS_COOP_LOG");
    g_log = l && *l && *l != '0';
    const char *s = std::getenv("SM64DS_COOP_SYNC");
    g_sync = !(s && *s == '0');
    if (const char *k = std::getenv("SM64DS_COOP_KILL")) {
        const char *p = k;
        while (*p && g_nkill < 8) {
            unsigned f = 0; int id = 0, n = 0, used = 0;
            if (std::sscanf(p, "%u:%i:%i%n", &f, &id, &n, &used) >= 3 && used > 0) {
                g_kill[g_nkill++] = {f, id, n, false};
                p += used;
            }
            while (*p == ';' || *p == ',' || *p == ' ') ++p;
            if (used <= 0) break;
        }
    }
    if (const char *g = std::getenv("SM64DS_COOP_GOTO")) {
        unsigned f = 0; int id = 0;
        if (std::sscanf(g, "%u:%i", &f, &id) == 2) { g_goto_frame = f; g_goto_id = id; }
    }
    if (const char *f = std::getenv("SM64DS_COOP_FORGE")) {
        unsigned fr = 0; char nm[16] = {};
        if (std::sscanf(f, "%u:%15s", &fr, nm) == 2) {
            g_forge_frame = fr;
            std::memcpy(g_forge_name, nm, sizeof g_forge_name);
            g_forge_name[15] = 0;
        }
    }
}

unsigned *tbl(int part) { return reinterpret_cast<unsigned *>(data_0209f4f8) + part * kCoopWords; }

int part_of_level(int level) {
    if (level < 0 || level > 127) return -1;
    const int p = (int)GetLevelPart(level);
    return (p >= 0 && p <= 2) ? p : -1;
}

int popcnt(unsigned v) { int n = 0; for (; v; v &= v - 1) ++n; return n; }

void stats_line();

bool is_player_ptr(const void *a) {
    for (int i = 0; i < 16; ++i)
        if (data_0209f394[i] == a) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Sending.
// ---------------------------------------------------------------------------
bool send_kind(uint32_t tag, int sub, const void *payload, int plen) {
    unsigned char buf[kHubCoopMaxDatagram];
    unsigned &seq = (tag == kHubTagCoopWorld) ? g_seq_world : g_seq_ctl;
    const int n = hub_coop_encode(tag, sub, ++seq, payload, plen, buf, (int)sizeof buf);
    if (n <= 0) return false;
    return comms_hub_send(buf, n);
}

CoopKeyWire wire_key(const CoopKey &k) {
    CoopKeyWire w; w.level = (uint16_t)k.level; w.act = (uint8_t)k.act; w.reserved = 0;
    return w;
}
bool key_from_wire(const CoopKeyWire &w, CoopKey *k) {
    if (!coop_key_ok(w)) return false;
    *k = CoopKey(w.level, w.act);
    return true;
}

void send_enter() {
    CoopEnterWire w; w.key = wire_key(g_key); w.nonce = g_nonce;
    if (send_kind(kHubTagCoopCtl, kCtlLevelEnter, &w, sizeof w)) ++g_st.sent_enter;
}
void send_hello(unsigned now) {
    CoopHelloWire w; std::memset(&w, 0, sizeof w);
    w.key = wire_key(g_key);
    w.established = g_grp.established() ? 1 : 0;
    w.owner = g_grp.owner() >= 0 ? (uint8_t)g_grp.owner() : (uint8_t)kCoopNoSeat;
    w.rank = (uint8_t)std::min(16, g_grp.established_count() + 1);
    if (send_kind(kHubTagCoopCtl, kCtlGroupHello, &w, sizeof w)) { ++g_st.sent_hello; g_hello_t = now; }
}
void send_owner_change() {
    CoopOwnerWire w; std::memset(&w, 0, sizeof w);
    w.key = wire_key(g_key); w.owner = (uint8_t)g_my;
    if (send_kind(kHubTagCoopCtl, kCtlOwnerChange, &w, sizeof w)) ++g_st.sent_owner;
}
void send_ack(int owner, uint32_t nonce) {
    CoopAckWire w; std::memset(&w, 0, sizeof w);
    w.key = wire_key(g_key); w.nonce = nonce; w.owner = (uint8_t)owner;
    if (send_kind(kHubTagCoopCtl, kCtlSnapAck, &w, sizeof w)) ++g_st.sent_ack;
}
void send_full(unsigned now) {
    if (g_part < 0) return;
    CoopFullWire w; w.key = wire_key(g_key);
    std::memcpy(w.words, tbl(g_part), sizeof w.words);
    if (send_kind(kHubTagCoopWorld, kWorldDeathFull, &w, sizeof w)) { ++g_st.sent_full; g_full_t = now; }
}

void serve_snapshot(int seat, const Job &j) {
    if (g_part < 0) return;
    unsigned char buf[sizeof(CoopSnapWire) + kCoopSnapChunkMax];
    const unsigned *t = tbl(g_part);
    const int clen = kCoopSnapChunkMax / kSnapChunks;
    for (int n = 0; n < kSnapChunks; ++n) {
        CoopSnapWire h; std::memset(&h, 0, sizeof h);
        h.target = (uint8_t)seat; h.n = (uint8_t)n; h.total = (uint8_t)kSnapChunks;
        h.key = wire_key(g_key); h.nonce = j.nonce;
        h.level_clock = 0; h.rng_seed = 0;   // phase 2
        std::memcpy(buf, &h, sizeof h);
        std::memcpy(buf + sizeof h, reinterpret_cast<const unsigned char *>(t) + n * clen, (size_t)clen);
        if (send_kind(kHubTagCoopCtl, kCtlSnapPart, buf, (int)sizeof h + clen)) ++g_st.sent_snap;
    }
}

// ---------------------------------------------------------------------------
// Applying remote bits.
// ---------------------------------------------------------------------------
void merge_remote_word(int w, unsigned bits) {
    unsigned *t = tbl(g_part);
    const unsigned fresh = bits & ~t[w];
    if (!fresh) return;
    t[w] |= fresh;
    g_prev[w] |= fresh;          // not mine: never announced back
    g_remote[w] |= fresh;
    g_st.bits_applied += (unsigned)popcnt(fresh);
    g_recon_dirty = true;
}

bool for_me(const CoopKey &k) {
    return g_part >= 0 && g_key.valid() && k == g_key;
}

// Remove the live actors whose slot a remote bit names. The ROM's own
// kill-and-track (bit already up, actor marked for destruction): no effects
// play on this console, no counters move.
int reconcile() {
    int kills = 0;
    bool any = false;
    for (int w = 0; w < kCoopWords; ++w) any = any || g_remote[w];
    if (!any) return 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a;
         a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        const short slot = *(const short *)(a + 0xce);
        if (slot < 0 || slot >= kCoopSlots) continue;
        if (!((g_remote[slot >> 5] >> (slot & 31)) & 1u)) continue;
        if (*(const unsigned char *)(a + 0x0f) || *(const unsigned char *)(a + 0x0e) == 2) continue;
        if (is_player_ptr(a)) continue;
        _ZN8dActor_c24KillAndTrackInDeathTableEv(a);
        ++kills;
        if (g_log)
            std::fprintf(stderr, "[coop] reconcile kill slot=%d actor=%u\n", (int)slot,
                         (unsigned)*(const unsigned short *)(a + 0x0c));
    }
    g_st.reconcile_kills += (unsigned)kills;
    return kills;
}

// ---------------------------------------------------------------------------
// Handlers.
// ---------------------------------------------------------------------------
bool seat_ok(int seat) {
    if (seat < 0 || seat >= kCoopSeats || seat == g_my) { ++g_st.drop_seat; return false; }
    return true;
}

void on_ctl(int seat, const unsigned char *d, int len, void *) {
    if (!coop_world_active()) { ++g_st.drop_not_coop; return; }
    if (!g_sync) { ++g_st.drop_sync_off; return; }
    if (!hub_coop_validate(kHubTagCoopCtl, d, len)) { ++g_st.drop_bad; return; }
    if (!seat_ok(seat)) return;
    const int sub = d[6];
    const unsigned char *p = d + kHubCoopHeaderBytes;
    const int pl = len - kHubCoopHeaderBytes;
    const unsigned now = comms_hub_now_ms();
    CoopKey k;
    switch (sub) {
    case kCtlGroupHello: {
        CoopHelloWire w;
        if (!coop_hello_validate(p, pl)) { ++g_st.drop_bad; return; }
        std::memcpy(&w, p, sizeof w);
        key_from_wire(w.key, &k);
        ++g_st.recv_hello;
        g_grp.note_hello(seat, k, w.established != 0, w.owner == kCoopNoSeat ? -1 : (int)w.owner, now);
        return;
    }
    case kCtlLevelEnter: {
        CoopEnterWire w;
        if (!coop_enter_validate(p, pl)) { ++g_st.drop_bad; return; }
        std::memcpy(&w, p, sizeof w);
        key_from_wire(w.key, &k);
        ++g_st.recv_enter;
        if (!for_me(k)) { ++g_st.drop_group; return; }
        if (!g_grp.i_am_owner()) return;       // the owner answers
        Job &j = g_job[seat];
        j.on = true; j.nonce = w.nonce; j.key = k; j.start = now; j.last = now;
        serve_snapshot(seat, j);
        ++g_st.snaps_served;
        return;
    }
    case kCtlSnapPart: {
        CoopSnapWire w;
        if (!coop_snap_validate(p, pl)) { ++g_st.drop_bad; return; }
        std::memcpy(&w, p, sizeof w);
        ++g_st.recv_snap;
        if (w.target != g_my) { ++g_st.drop_target; return; }
        key_from_wire(w.key, &k);
        if (!for_me(k)) { ++g_st.drop_group; return; }
        if (w.nonce != g_nonce) { ++g_st.drop_nonce; return; }
        if (g_snap_done) {                      // a resend after we already have it: re-ack, rate limited
            if ((unsigned)(now - g_ack_last) >= 100u) { send_ack(seat, g_nonce); g_ack_last = now; }
            return;
        }
        if (g_snap_total && g_snap_total != w.total) { ++g_st.drop_bad; return; }
        g_snap_total = w.total;
        const int clen = kCoopSnapChunkMax / w.total;
        std::memcpy(g_snap_buf + w.n * clen, p + sizeof w, (size_t)clen);
        g_snap_mask |= 1u << w.n;
        if (g_snap_mask == (1u << w.total) - 1u) {
            g_snap_done = true;
            g_snap_owner = seat;
            send_ack(seat, g_nonce);
            g_ack_last = now;
            if (!g_snap_waiting) {              // the hook gave up already: take it as a live merge
                const unsigned *s = reinterpret_cast<const unsigned *>(g_snap_buf);
                for (int i = 0; i < kCoopWords; ++i) merge_remote_word(i, s[i]);
                ++g_st.snaps_applied;
            }
        }
        return;
    }
    case kCtlSnapAck: {
        CoopAckWire w;
        if (!coop_ack_validate(p, pl)) { ++g_st.drop_bad; return; }
        std::memcpy(&w, p, sizeof w);
        ++g_st.recv_ack;
        key_from_wire(w.key, &k);
        if (g_job[seat].on && g_job[seat].nonce == w.nonce && g_job[seat].key == k)
            g_job[seat].on = false;
        return;
    }
    case kCtlOwnerChange: {
        CoopOwnerWire w;
        if (!coop_owner_validate(p, pl)) { ++g_st.drop_bad; return; }
        std::memcpy(&w, p, sizeof w);
        key_from_wire(w.key, &k);
        ++g_st.recv_owner;
        if (w.owner != seat) { ++g_st.drop_bad; return; }   // only a seat can name itself
        g_grp.note_owner_change(seat, k, now);
        return;
    }
    default:
        for (int i = 0; i < g_nreg_ctl; ++i)
            if (sub >= g_reg_ctl[i].lo && sub <= g_reg_ctl[i].hi) {
                g_reg_ctl[i].fn(seat, sub, p, pl, g_reg_ctl[i].user);
                return;
            }
        ++g_st.drop_unknown_sub;
    }
}

void on_world(int seat, const unsigned char *d, int len, void *) {
    if (!coop_world_active()) { ++g_st.drop_not_coop; return; }
    if (!g_sync) { ++g_st.drop_sync_off; return; }
    if (!hub_coop_validate(kHubTagCoopWorld, d, len)) { ++g_st.drop_bad; return; }
    if (!seat_ok(seat)) return;
    const int sub = d[6];
    const unsigned char *p = d + kHubCoopHeaderBytes;
    const int pl = len - kHubCoopHeaderBytes;
    CoopKey k;
    if (sub == kWorldDeathDelta) {
        if (!coop_delta_validate(p, pl)) { ++g_st.drop_bad; return; }
        CoopDeltaWire w; std::memcpy(&w, p, sizeof w);
        key_from_wire(w.key, &k);
        ++g_st.recv_delta;
        if (!for_me(k)) { ++g_st.drop_group; return; }
        for (int i = 0; i < (int)w.count; ++i) {
            uint16_t s; std::memcpy(&s, p + sizeof w + 2 * i, 2);
            merge_remote_word(s >> 5, 1u << (s & 31));
        }
        return;
    }
    if (sub == kWorldDeathFull) {
        if (!coop_full_validate(p, pl)) { ++g_st.drop_bad; return; }
        CoopFullWire w; std::memcpy(&w, p, sizeof w);
        key_from_wire(w.key, &k);
        ++g_st.recv_full;
        if (!for_me(k)) { ++g_st.drop_group; return; }
        for (int i = 0; i < kCoopWords; ++i) merge_remote_word(i, w.words[i]);
        return;
    }
    for (int i = 0; i < g_nreg_world; ++i)
        if (sub >= g_reg_world[i].lo && sub <= g_reg_world[i].hi) {
            g_reg_world[i].fn(seat, sub, p, pl, g_reg_world[i].user);
            return;
        }
    ++g_st.drop_unknown_sub;
}

void ensure_handlers() {
    if (g_handlers) return;
    hub_on_kind(kHubCoopCtl, on_ctl, nullptr);
    hub_on_kind(kHubCoopWorld, on_world, nullptr);
    g_handlers = true;
    g_my = comms_hub_my_seat();
    g_grp.reset(g_my);
    if (g_log) std::atexit([] { stats_line(); });
    std::fprintf(stderr, "[coop] world core on: seat %d, sync %s\n", g_my, g_sync ? "on" : "OFF (control)");
}

// ---------------------------------------------------------------------------
// Presence -> membership.
// ---------------------------------------------------------------------------
void refresh_presence(unsigned now) {
    for (int s = 0; s < kCoopSeats; ++s) {
        if (s == g_my) continue;
        HubPresence p; unsigned age = 0;
        const bool have = hub_presence_get(s, &p, &age) && age < kPresenceMs && p.in_level();
        g_grp.note_presence(s, have, have ? CoopKey(p.level, p.act) : CoopKey(), now);
    }
}

void dump_bits(const char *why) {
    if (!g_log || g_part < 0) return;
    const unsigned *t = tbl(g_part);
    int n = 0;
    char hex[kCoopWords * 9 + 1]; hex[0] = 0;
    char *o = hex;
    for (int i = 0; i < kCoopWords; ++i) { n += popcnt(t[i]); o += std::sprintf(o, "%08x", t[i]); if (i + 1 < kCoopWords) *o++ = ' '; }
    std::fprintf(stderr, "[coopbits] %s level=%d act=%d part=%d n=%d bits=%s\n", why, g_key.level, g_key.act, g_part, n, hex);
}

void join_group(const CoopKey &nk) {
    const unsigned now = comms_hub_now_ms();
    g_grp.leave();
    g_out_n = 0;
    std::memset(g_remote, 0, sizeof g_remote);
    std::memset(g_job, 0, sizeof g_job);
    g_snap_waiting = false; g_snap_done = false; g_snap_total = 0; g_snap_mask = 0; g_snap_owner = -1;
    g_recon_dirty = false;
    g_est_frames = 0;
    for (int i = 0; i < g_nkill; ++i) g_kill[i].done = false;
    g_goto_done = false;
    g_key = nk;
    g_part = part_of_level(nk.level);
    g_grp.enter(nk, now);
    ++g_nonce;
    if (g_part < 0) {   // not a level the table covers: stay out of any group
        g_key = CoopKey();
        return;
    }
    comms_hub_pump();
    refresh_presence(now);
    const int peers = g_grp.others();
    int hint = -1;
    if (peers > 0) {
        g_snap_waiting = true;
        const unsigned t0 = comms_hub_now_ms();
        unsigned last_send = 0;
        send_enter(); last_send = t0;
        while ((unsigned)(comms_hub_now_ms() - t0) < kEnterWaitMs && !g_snap_done) {
            ::Sleep(2);
            comms_hub_pump();
            const unsigned n2 = comms_hub_now_ms();
            if ((unsigned)(n2 - last_send) >= kEnterResend) { send_enter(); last_send = n2; }
        }
        g_snap_waiting = false;
        if (g_snap_done) {
            unsigned *t = tbl(g_part);
            const unsigned *s = reinterpret_cast<const unsigned *>(g_snap_buf);
            for (int i = 0; i < kCoopWords; ++i) {
                t[i] = s[i] | g_remote[i];     // the group's state, plus anything heard while waiting
                g_remote[i] |= s[i];           // survivors of the spawn get reconciled
            }
            hint = g_snap_owner;
            ++g_st.snaps_applied;
            g_recon_dirty = true;
            std::fprintf(stderr, "[coop] joined level %d act %d: snapshot from seat %d applied before spawn "
                         "(%d ms)\n", nk.level, nk.act, g_snap_owner,
                         (int)(comms_hub_now_ms() - t0));
        } else {
            ++g_st.snap_timeouts;
            std::fprintf(stderr, "[coop] joined level %d act %d: no snapshot in %d ms, first in\n",
                         nk.level, nk.act, (int)kEnterWaitMs);
        }
    } else {
        ++g_st.first_in;
        std::fprintf(stderr, "[coop] joined level %d act %d: nobody here, first in\n", nk.level, nk.act);
    }
    std::memcpy(g_prev, tbl(g_part), sizeof g_prev);
    g_grp.establish(comms_hub_now_ms(), hint);
    g_grp.update(comms_hub_now_ms());
    g_hello_t = 0; g_full_t = comms_hub_now_ms();
    dump_bits("established");
}

// ---------------------------------------------------------------------------
// Proof knobs (inert unless named).
// ---------------------------------------------------------------------------
void knob_kill(const KillKnob &k) {
    // Nearest to the local player first (a proof wants the kill on screen).
    struct C { short slot; char *a; long long d2; };
    C c[128]; int n = 0;
    const char *pl = (const char *)data_0209f394[data_0209f250 & 15];
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a && n < 128; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        if (*(const unsigned short *)(a + 0x0c) != (unsigned)k.id) continue;
        const short slot = *(const short *)(a + 0xce);
        if (slot < 0 || slot >= kCoopSlots) continue;
        if (*(const unsigned char *)(a + 0x0f) || *(const unsigned char *)(a + 0x0e) == 2) continue;
        if (is_player_ptr(a)) continue;
        long long d2 = 0;
        if (pl) for (int q = 0; q < 3; ++q) {
            const long long dd = (long long)*(const int *)(a + 0x5c + 4 * q) - *(const int *)(pl + 0x5c + 4 * q);
            d2 += (dd >> 8) * (dd >> 8);
        }
        c[n++] = {slot, a, d2};
    }
    std::sort(c, c + n, [](const C &x, const C &y) { return x.d2 != y.d2 ? x.d2 < y.d2 : x.slot < y.slot; });
    int killed = 0;
    for (int i = 0; i < n && killed < k.n; ++i, ++killed) {
        std::fprintf(stderr, "[coop-knob] kill class=%d slot=%d\n", k.id, (int)c[i].slot);
        _ZN8dActor_c24KillAndTrackInDeathTableEv(c[i].a);
    }
    std::fprintf(stderr, "[coop-knob] kill class=%d asked %d did %d (live candidates %d)\n", k.id, k.n, killed, n);
}

void knob_goto(int id) {
    void *pl = data_0209f394[data_0209f250 & 15];
    if (!pl) return;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        if (*(const unsigned short *)(a + 0x0c) != (unsigned)id || is_player_ptr(a)) continue;
        if (*(const unsigned char *)(a + 0x0f) || *(const unsigned char *)(a + 0x0e) == 2) continue;
        std::memcpy((char *)pl + 0x5c, a + 0x5c, 12);
        std::fprintf(stderr, "[coop-knob] goto class=%d slot=%d\n", id, (int)*(const short *)(a + 0xce));
        return;
    }
    std::fprintf(stderr, "[coop-knob] goto class=%d: no live actor\n", id);
}

void knob_forge(const char *name) {
    unsigned char b[64]; std::memset(b, 0, sizeof b);
    CoopKeyWire k = wire_key(g_key);
    std::fprintf(stderr, "[coop-knob] forge %s\n", name);
    if (!std::strcmp(name, "oob")) {          // a slot past the end of the part
        CoopDeltaWire w; std::memset(&w, 0, sizeof w); w.key = k; w.count = 2;
        std::memcpy(b, &w, sizeof w); uint16_t s[2] = {5, 600}; std::memcpy(b + sizeof w, s, 4);
        send_kind(kHubTagCoopWorld, kWorldDeathDelta, b, (int)sizeof w + 4);
    } else if (!std::strcmp(name, "group")) { // right shape, someone else's level
        CoopDeltaWire w; std::memset(&w, 0, sizeof w); w.key = k; w.key.act = (uint8_t)((k.act + 1) & 7); w.count = 1;
        std::memcpy(b, &w, sizeof w); uint16_t s = 7; std::memcpy(b + sizeof w, &s, 2);
        send_kind(kHubTagCoopWorld, kWorldDeathDelta, b, (int)sizeof w + 2);
    } else if (!std::strcmp(name, "count")) { // count says 5, one slot present
        CoopDeltaWire w; std::memset(&w, 0, sizeof w); w.key = k; w.count = 5;
        std::memcpy(b, &w, sizeof w); uint16_t s = 7; std::memcpy(b + sizeof w, &s, 2);
        send_kind(kHubTagCoopWorld, kWorldDeathDelta, b, (int)sizeof w + 2);
    } else if (!std::strcmp(name, "snapbad")) { // chunk 3 of 2
        CoopSnapWire w; std::memset(&w, 0, sizeof w); w.target = (uint8_t)(g_my ^ 1); w.n = 3; w.total = 2; w.key = k;
        std::memcpy(b, &w, sizeof w);
        send_kind(kHubTagCoopCtl, kCtlSnapPart, b, (int)sizeof w + 32);
    } else if (!std::strcmp(name, "snapgroup")) { // a whole valid snapshot for another group, aimed at the other seat
        CoopSnapWire w; std::memset(&w, 0, sizeof w); w.target = (uint8_t)(g_my ^ 1); w.n = 0; w.total = 2; w.key = k;
        w.key.level = (uint16_t)((k.level + 1) & 0x7f);
        std::memcpy(b, &w, sizeof w);
        send_kind(kHubTagCoopCtl, kCtlSnapPart, b, (int)sizeof w + 32);
    } else if (!std::strcmp(name, "sub")) {    // a SYNB sub-type nobody owns
        send_kind(kHubTagCoopCtl, 0x1e, b, 4);
    } else if (!std::strcmp(name, "trunc")) {  // a delta cut short (head says 3 slots, 1 follows)
        CoopDeltaWire w; std::memset(&w, 0, sizeof w); w.key = k; w.count = 3;
        std::memcpy(b, &w, sizeof w);
        send_kind(kHubTagCoopWorld, kWorldDeathDelta, b, (int)sizeof w + 2);
    }
}

void census_line() {
    // Placed actors by class (those with a death-table slot): the proof reads
    // class ids for its kill / goto knobs from this.
    int cnt[1024]; std::memset(cnt, 0, sizeof cnt);
    int slotted = 0, total = 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        ++total;
        const short slot = *(const short *)(a + 0xce);
        if (slot < 0 || slot >= kCoopSlots) continue;
        const unsigned id = *(const unsigned short *)(a + 0x0c);
        ++slotted;
        if (id < 1024) ++cnt[id];
    }
    std::fprintf(stderr, "[coop] census level=%d act=%d actors=%d slotted=%d classes:", g_key.level, g_key.act, total, slotted);
    for (int i = 0; i < 1024; ++i) if (cnt[i]) std::fprintf(stderr, " %d:%d", i, cnt[i]);
    std::fprintf(stderr, "
");
}

void stats_line() {
    if (!g_log) return;
    const CoopWorldStats &s = g_st;
    std::fprintf(stderr,
        "[coopstat] seat=%d owner=%d est=%d members=%u sent h%llu e%llu s%llu a%llu o%llu d%llu f%llu "
        "recv h%llu e%llu s%llu a%llu o%llu d%llu f%llu applied=%llu recon=%llu local=%llu served=%llu "
        "snaps=%llu firstin=%llu timeouts=%llu drop bad=%llu group=%llu target=%llu nonce=%llu sub=%llu "
        "notcoop=%llu seat=%llu syncoff=%llu\n",
        g_my, g_grp.owner(), (int)g_grp.established(), g_grp.member_mask(),
        s.sent_hello, s.sent_enter, s.sent_snap, s.sent_ack, s.sent_owner, s.sent_delta, s.sent_full,
        s.recv_hello, s.recv_enter, s.recv_snap, s.recv_ack, s.recv_owner, s.recv_delta, s.recv_full,
        s.bits_applied, s.reconcile_kills, s.local_bits, s.snaps_served, s.snaps_applied, s.first_in,
        s.snap_timeouts, s.drop_bad, s.drop_group, s.drop_target, s.drop_nonce, s.drop_unknown_sub,
        s.drop_not_coop, s.drop_seat, s.drop_sync_off);
}

void tick_est(unsigned now) {
    ++g_est_frames;
    // 1. what I killed or collected since last frame
    if (g_sync) {
        unsigned *t = tbl(g_part);
        for (int w = 0; w < kCoopWords; ++w) {
            unsigned nb = t[w] & ~g_prev[w];
            while (nb) {
                const unsigned bit = nb & (~nb + 1u);
                nb &= nb - 1;
                int b = 0; while (!((bit >> b) & 1u)) ++b;
                if (g_out_n < kOutMax) {
                    g_out[g_out_n].slot = (uint16_t)(w * 32 + b);
                    g_out[g_out_n].sent = 0;
                    g_out[g_out_n].next = now;
                    ++g_out_n;
                }
                ++g_st.local_bits;
                if (g_log) std::fprintf(stderr, "[coop] local bit slot=%d\n", w * 32 + b);
            }
            g_prev[w] = t[w];
        }
        // 2. announce them (three tries each: now, +120 ms, +360 ms)
        unsigned char buf[sizeof(CoopDeltaWire) + 2 * kCoopMaxDelta];
        uint16_t slots[kCoopMaxDelta]; int idx[kCoopMaxDelta]; int n = 0;
        for (int i = 0; i < g_out_n && n < kCoopMaxDelta; ++i)
            if ((int)(now - g_out[i].next) >= 0) { slots[n] = g_out[i].slot; idx[n++] = i; }
        if (n > 0 && g_grp.others() > 0) {
            CoopDeltaWire h; std::memset(&h, 0, sizeof h); h.key = wire_key(g_key); h.count = (uint8_t)n;
            std::memcpy(buf, &h, sizeof h);
            std::memcpy(buf + sizeof h, slots, (size_t)n * 2);
            if (send_kind(kHubTagCoopWorld, kWorldDeathDelta, buf, (int)sizeof h + 2 * n)) {
                ++g_st.sent_delta;
                for (int i = 0; i < n; ++i) {
                    Out &o = g_out[idx[i]];
                    ++o.sent;
                    o.next = now + (o.sent == 1 ? 120u : 240u);
                }
            }
        }
        // retire the finished ones (compact)
        int k = 0;
        for (int i = 0; i < g_out_n; ++i) if (g_out[i].sent < 3) g_out[k++] = g_out[i];
        if (g_grp.others() == 0) k = 0;      // nobody to tell: the owner-less table is the whole truth
        g_out_n = k;
    }
    // 3. hello, owner full resend, owner announcement
    if ((unsigned)(now - g_hello_t) >= kHelloMs) send_hello(now);
    g_grp.update(now);
    if (g_grp.take_owner_announce()) { send_owner_change(); send_full(now); send_hello(now); }
    if (g_grp.i_am_owner() && g_grp.others() > 0 && (unsigned)(now - g_full_t) >= kFullMs) send_full(now);
    // 4. snapshots I owe
    for (int s = 0; s < CoopGroups::kSeats; ++s) {
        Job &j = g_job[s];
        if (!j.on) continue;
        if (!g_grp.i_am_owner() || !g_grp.is_member(s) || (unsigned)(now - j.start) > kSnapGiveUpMs || j.key != g_key) { j.on = false; continue; }
        if ((unsigned)(now - j.last) >= kSnapResend) { serve_snapshot(s, j); j.last = now; }
    }
    // 5. reconcile
    if (g_recon_dirty || (g_est_frames % kReconcileEvery) == 0) {
        g_recon_dirty = false;
        reconcile();
    }
    // 6. proof knobs
    for (int i = 0; i < g_nkill; ++i)
        if (!g_kill[i].done && g_est_frames >= g_kill[i].frame) { g_kill[i].done = true; knob_kill(g_kill[i]); }
    if (g_goto_id >= 0 && !g_goto_done && g_est_frames >= g_goto_frame) { g_goto_done = true; knob_goto(g_goto_id); }
    if (g_forge_frame && !g_forge_done && g_est_frames >= g_forge_frame) { g_forge_done = true; knob_forge(g_forge_name); }
    // 7. log
    if (g_log && g_est_frames == 40) census_line();
    if (g_log) {
        const unsigned *t = tbl(g_part);
        unsigned h = 2166136261u;
        for (int i = 0; i < kCoopWords; ++i) { h ^= t[i]; h *= 16777619u; }
        if (h != g_last_bits_hash) { g_last_bits_hash = h; dump_bits("changed"); }
        if ((g_est_frames % 120) == 0) stats_line();
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Public.
// ---------------------------------------------------------------------------
bool coop_world_active() {
    read_env();
    if (!g_on) return false;
    return comms_hub_active() && !comms_hub_arena() && comms_hub_state() == kHubLive &&
           comms_hub_room_kind() == kHubRoomCoop;
}

CoopWorldStats coop_world_stats() { return g_st; }

static bool register_range(Reg *arr, int *n, int lo, int hi, int mine_lo, int mine_hi,
                           CoopSubFn fn, void *user) {
    if (!fn || lo < 0 || hi > 255 || lo > hi) return false;
    if (!(hi < mine_lo || lo > mine_hi)) return false;
    for (int i = 0; i < *n; ++i)
        if (!(hi < arr[i].lo || lo > arr[i].hi)) return false;
    if (*n >= 4) return false;
    arr[(*n)++] = {lo, hi, fn, user};
    return true;
}
bool coop_ctl_register(int lo, int hi, CoopSubFn fn, void *user) {
    return register_range(g_reg_ctl, &g_nreg_ctl, lo, hi, kCtlGroupHello, kCtlOwnerChange, fn, user);
}
bool coop_world_register(int lo, int hi, CoopSubFn fn, void *user) {
    return register_range(g_reg_world, &g_nreg_world, lo, hi, kWorldDeathDelta, kWorldDeathFull, fn, user);
}
bool coop_send_ctl(int subtype, const void *payload, int payload_len) {
    return coop_world_active() && send_kind(kHubTagCoopCtl, subtype, payload, payload_len);
}
bool coop_send_world(int subtype, const void *payload, int payload_len) {
    return coop_world_active() && send_kind(kHubTagCoopWorld, subtype, payload, payload_len);
}

}  // namespace port

extern "C" void port_coop_world_level_latched(void) {
    using namespace port;
    if (!coop_world_active()) return;
    ensure_handlers();
    join_group(CoopKey((int)data_0209f2f8, data_0209f220[0] & 0xff));
}

extern "C" void port_coop_world_frame(void) {
    using namespace port;
    if (!coop_world_active()) return;
    ensure_handlers();
    ++g_frame;
    const unsigned now = comms_hub_now_ms();
    refresh_presence(now);
    const int me = (int)data_0209f250;
    const bool in_level = me >= 0 && me < 16 && data_0209f394[me] != nullptr;
    if (in_level) {
        // A boot that did not come through the latch (a direct level boot, or a
        // missed hook): join now. The snapshot, if one comes, merges live.
        const CoopKey rk((int)data_0209f2f8, data_0209f220[0] & 0xff);
        if (!g_key.valid() || rk != g_key) join_group(rk);
    }
    g_grp.update(now);
    if (in_level && g_key.valid() && g_part >= 0 && g_grp.established()) tick_est(now);
}
