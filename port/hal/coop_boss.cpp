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

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250;
void *_ZN8dActor_c4NextEPKS_(const void *prev);
int KingBobOmb_SetState(void *c, void *state);
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
enum : uint8_t { kPlain = 0, kStatePtr = 1, kVol = 2 };   // kVol: changes every frame, never a reason to publish by itself
enum : int { kLoOff = 0x100, kHiOff = 0x520 };
struct BField { uint16_t off; uint8_t size; uint8_t kind; };
struct BDesc {
    uint16_t cls;
    const char *name;
    uint8_t nf;
    BField f[kBossMaxFields];
    int8_t hp;        // index of the HP field (min wins), -1 none
    int8_t state;     // index of the state field, -1 none
    uint16_t hit_off; // 0, or the offset of a byte the class's own handler turns into one HP (the proof knob sets it)
};

// Whomp King (daBtn_c, shared Whomp class, king flag +0x414): include/Whomp.h, src/func_ov079_02124b08.c / 02124ed4.c:
// mState +0x3b0, HP +0x401 (3 for the king), the hit flag +0x402, the sub-state +0x40c, the flat-on-the-ground timer
// +0x3fc (u16), the state frame counter +0x100 (u16), +0x407, and the animation cursor in the ModelAnim's Animation.
const BDesc kDescs[] = {
    { 165, "WHOMP_KING", 7,
      { { 0x3b0, 4, kPlain }, { 0x401, 1, kPlain }, { 0x40c, 1, kPlain }, { 0x407, 1, kPlain }, { 0x3fc, 2, kVol },
        { 0x100, 2, kVol }, { 0x324, 4, kVol }, { 0, 0, 0 } },
      1, 0, 0x402 },
    // King Bob-omb (daBombking_c): state record pointer +0x420 (mapped to a table id), HP mHealth +0x500 (3), mPhase +0x4a0,
    // the two timers +0x504 / +0x505, mAnimSpeed +0x4fc, +0x499.
    { 189, "KING_BOB_OMB", 7,
      { { 0x420, 4, kStatePtr }, { 0x500, 4, kPlain }, { 0x4a0, 4, kPlain }, { 0x504, 1, kVol }, { 0x505, 1, kVol },
        { 0x4fc, 4, kPlain }, { 0x499, 1, kPlain }, { 0, 0, 0 } },
      1, 0, 0 },
};
const int kNumDescs = (int)(sizeof kDescs / sizeof kDescs[0]);

const BDesc *desc_of(unsigned cls) {
    for (int i = 0; i < kNumDescs; ++i) if (kDescs[i].cls == cls) return &kDescs[i];
    return nullptr;
}

bool field_ok(const BField &f) {
    if (f.size != 1 && f.size != 2 && f.size != 4) return false;
    if (f.off < kLoOff || (int)f.off + f.size > kHiOff) return false;
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
struct HitKnob { unsigned tick; bool done; bool armed; int hp0; int slot; };
HitKnob g_hit[4]; int g_nhit = 0;
struct ForgeKnob { unsigned tick; char name[12]; bool done; };
ForgeKnob g_forge[8]; int g_nforge = 0;
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
    if (f.size == 1) v &= 0xffu; else if (f.size == 2) v &= 0xffffu;
    std::memcpy(a + f.off, &v, f.size);
}

// Sane values only: a state of Whomp King is a small index; HP is small.
bool value_ok(const BDesc &d, int idx, uint32_t v) {
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
            g_hit[g_nhit].tick = fr; g_hit[g_nhit].done = false; g_hit[g_nhit].armed = false; g_hit[g_nhit].hp0 = 0; g_hit[g_nhit].slot = -1; ++g_nhit;
            p += used; while (*p == ',' || *p == ';' || *p == ' ') ++p;
        }
    }
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
        for (int k = 0; k < d.nf; ++k) if (k != d.hp && value_ok(d, k, x.w[k])) write_field(b.a, d, k, x.w[k]);
        b.mism = 0;
        ++g_st.applied_state;
        if (g_log) std::fprintf(stderr, "[cbs] apply-state slot=%d st %u->%u hp=%d from seat %d heard=%d w=%lld\n", slot, ls, rs, hp_of(b), x.from, (int)b.heard, wall_ms());
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
        const int auth = auth_for(slot, wnow);
        const bool mine = auth == g_my;
        // a hit that landed on my copy: HP below what was last synced
        const int hp = hp_of(b);
        if (hp < b.synced_hp) {
            ++g_st.hit_local;
            if (g_log) std::fprintf(stderr, "[cbs] hit slot=%d hp %d->%d (auth %d me %d) w=%lld\n", slot, b.synced_hp, hp, auth, g_my, wall_ms());
            b.synced_hp = hp;
            if (!mine) {
                unsigned char one[64];
                const int n = fill_rec(one, slot, b, 0);
                send_batch(kWorldBossHit, 1, one, 1, n);
            }
            b.lw_valid = false;                 // publish at once when I am the authority
        }
        apply_rx(slot, b, now);
        if (g_log && (unsigned)(now - b.sample_t) >= 500u) { b.sample_t = now; log_sample(slot, b, auth); }
        if (!b.announced_dead && hp_of(b) == 0 && g_log) {
            b.announced_dead = true;
            std::fprintf(stderr, "[cbs] defeated slot=%d cls=%u hp=0 st=%u w=%lld\n", slot, (unsigned)d.cls, state_of(b), wall_ms());
        }
        if (b.last_auth != auth) {
            if (g_log) std::fprintf(stderr, "[cbs] authority slot=%d %d->%d w=%lld\n", slot, b.last_auth, auth, wall_ms());
            b.last_auth = auth; b.auth_t = now; b.mism = 0;
        }
        if (!mine) continue;
        // I am the authority: publish when anything changed, and as a heartbeat
        uint32_t w[kBossMaxFields]; bool changed = !b.lw_valid;
        for (int k = 0; k < d.nf; ++k) { w[k] = read_field(b.a, d.f[k]); if (b.lw_valid && w[k] != b.last_w[k] && !(d.f[k].kind & kVol)) changed = true; }
        if (changed || (unsigned)(now - b.pub_t) >= (unsigned)kBossHeartbeatMs) {
            if (bytes + rec_bytes(d) <= kPayloadMax - (int)sizeof(BossHead) && nrec < kBossMaxRecs) {
                bytes += fill_rec(recs + bytes, slot, b, 0); ++nrec;
                std::memcpy(b.last_w, w, sizeof w); b.lw_valid = true; b.pub_t = now;
            }
        }
    }
    if (nrec > 0 && take_token(now)) send_batch(kWorldBossState, 0, recs, nrec, bytes);
}

// SM64DS_COOP_BOSS_HIT=F[,F]: from boss tick F, on THIS console, hit the first boss the ROM's own way. Where the class has a
// hit flag (Whomp King) the flag is set every tick until the boss's own handler has taken it (it only does in its vulnerable
// state), so the HP drop is the game's; where it has none the HP is lowered directly (the sync path is what is under test).
void do_hit_knob(HitKnob &k) {
    if (!k.armed) {
        for (int slot = 0; slot < kSlots; ++slot) {
            Boss &b = g_boss[slot];
            if (!b.a || hp_of(b) <= 0) continue;
            k.armed = true; k.slot = slot; k.hp0 = hp_of(b);
            std::fprintf(stderr, "[cbs-knob] hit armed slot=%d cls=%u hp=%d w=%lld
", slot, (unsigned)b.d->cls, k.hp0, wall_ms());
            break;
        }
        if (!k.armed) return;
    }
    Boss &b = g_boss[k.slot];
    if (!b.a) { k.done = true; return; }
    if (hp_of(b) < k.hp0) {
        k.done = true;
        std::fprintf(stderr, "[cbs-knob] hit taken slot=%d hp %d->%d st=%u w=%lld
", k.slot, k.hp0, hp_of(b), state_of(b), wall_ms());
        return;
    }
    if (b.d->hit_off) b.a[b.d->hit_off] = 1;
    else { write_field(b.a, *b.d, b.d->hp, (uint32_t)(k.hp0 - 1)); }
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

void on_frame(unsigned now) {
    read_env();
    if (!coop_world_active()) return;
    if (!g_regd) { g_regd = true; g_my = comms_hub_my_seat(); }
    int lv = -1, ac = 0;
    if (!coop_group_established() || !coop_group_key(&lv, &ac)) return;
    const CoopKey nk(lv, ac);
    if (nk != g_key) {
        for (int i = 0; i < kSlots; ++i) g_boss[i] = Boss();
        for (int s = 0; s < kSeats; ++s) g_claim_until[s] = 0;
        g_key = nk;
    }
    ++g_ticks; ++g_st.ticks;
    if (!g_on) return;
    scan();
    g_st.bosses = (unsigned long long)g_nb;
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
                for (int k = 0; k < d->nf; ++k) if (k != d->hp) write_field(b.a, *d, k, w[k]);
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
