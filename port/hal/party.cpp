// PARTY BODIES: membership by seat, the SYNL gate, the solid flag per body. See
// hal/party.h. The cylinder and the spike's lab live in hal/party_cyl.cpp.
//
// Knobs (all off by default; the first four are the proof's):
//   SM64DS_PARTY=1            this console is in a party room (the launcher sets it)
//   SM64DS_PARTY_MEMBERS=1,2  lab: only these SEATS are party, the rest stay ghosts
//   SM64DS_PARTY_GATE=0       lab: ignore SYNL (the ghost frames alone decide)
//   SM64DS_PARTY_LOG=1        a [party] line every 60 follower passes, and the
//                             gate / cylinder events
//   SM64DS_PARTY_SEP=1        a [partysep] line per solid body per frame (the
//                             separation to my body, for the push rows)

#include "party.h"
#include "hub_presence.h"
#include "comms_hub.h"
#include "player_fields.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250;
extern unsigned char data_0209f21c;
extern signed char data_0209f2f8;
extern signed char data_02092120;
}

namespace port {
namespace party {
namespace {

enum { kMaxBodies = 16 };

int g_read = 0;
int g_all = 0;                 // SM64DS_PARTY=1
bool g_have_list = false;
unsigned g_mask = 0;           // SM64DS_PARTY_MEMBERS, seats
int g_gate_on = 1;
int g_log = 0;
int g_sep = 0;
bool g_solid[kMaxBodies];
Stats g_st = {};
unsigned g_pass = 0;

void read_env() {
    if (g_read) return;
    g_read = 1;
    if (const char *e = std::getenv("SM64DS_PARTY")) g_all = std::atoi(e) != 0;
    if (const char *e = std::getenv("SM64DS_PARTY_MEMBERS")) {
        g_have_list = true;
        for (const char *q = e; *q;) {
            char *end;
            const long v = std::strtol(q, &end, 10);
            if (end == q) break;
            if (v >= 0 && v < 32) g_mask |= (1u << (unsigned)v);
            q = *end == ',' ? end + 1 : end;
        }
    }
    if (const char *e = std::getenv("SM64DS_PARTY_GATE")) g_gate_on = std::atoi(e) != 0;
    if (const char *e = std::getenv("SM64DS_PARTY_LOG")) g_log = std::atoi(e);
    if (const char *e = std::getenv("SM64DS_PARTY_SEP")) g_sep = std::atoi(e);
}

}  // namespace

bool active() {
    read_env();
    return g_all != 0 || g_have_list;
}

bool seat_is_party(int seat) {
    read_env();
    if (seat < 0 || seat >= kHubMaxSeats) return false;
    if (seat == comms_hub_my_seat()) return false;
    if (g_have_list) return ((g_mask >> seat) & 1u) != 0;
    return g_all != 0;
}

int gate(int seat) {
    read_env();
    if (!g_gate_on) return kGateOk;
    HubPresence p;
    unsigned age = 0;
    // No word from him yet (or none for 3 s): his ghost frames decide, as they do
    // for an adventure ghost. The relay sends SYNL on every change and at 2 Hz.
    if (!hub_presence_get(seat, &p, &age) || age > 3000u) return kGateOk;
    if (!p.in_level() || p.in_menu() || p.in_cutscene()) {
        ++g_st.gate_hide;
        return kGateHide;
    }
    if (p.level != (int)(unsigned char)data_0209f2f8) {
        ++g_st.gate_level;
        return kGateLevel;
    }
    if (p.area != (int)data_02092120) {
        ++g_st.gate_area;
        return kGateArea;
    }
    return kGateOk;
}

void clear(int body) {
    if (body >= 0 && body < kMaxBodies) g_solid[body] = false;
    lab_tick(body);
}

bool body_solid(int body) {
    return body >= 0 && body < kMaxBodies && g_solid[body];
}

bool follow(void *a, int body, int seat) {
    read_env();
    if (!a || body < 0 || body >= kMaxBodies) return false;
    lab_before_register(a, body);
    g_solid[body] = true;
    ++g_st.solid_frames;
    ++g_pass;
    const bool reg = cyl_register(a);
    if (g_sep > 0) {
        const int me = (int)data_0209f250;
        void *mb = (me >= 0 && me < kMaxBodies) ? data_0209f394[me] : nullptr;
        if (mb) {
            const long long dx = (long long)(*player::pos_x(a) - *player::pos_x(mb));
            const long long dz = (long long)(*player::pos_z(a) - *player::pos_z(mb));
            const long long dy = (long long)(*player::pos_y(a) - *player::pos_y(mb));
            // horizontal distance in whole units (Fix12 >> 12), integer sqrt
            long long d2 = (dx >> 4) * (dx >> 4) + (dz >> 4) * (dz >> 4);   // (Fix12 / 16)^2
            long long r = 0, lo = 0, hi = 1LL << 31;
            while (lo < hi) {
                const long long mid = (lo + hi + 1) / 2;
                if (mid * mid <= d2) lo = mid; else hi = mid - 1;
            }
            r = lo;                                  // Fix12 / 16
            std::fprintf(stderr, "[partysep] t=%u seat=%d body=%d horiz=%d.%02d dy=%d me=(%d,%d,%d) "
                         "p=(%d,%d,%d) reg=%d push=(%d,%d,%d) hit=%x own=%u\n", comms_hub_now_ms(), seat, body,
                         (int)(r * 16 / 4096), (int)((r * 16 * 100 / 4096) % 100), (int)(dy >> 12),
                         *player::pos_x(mb) >> 12, *player::pos_y(mb) >> 12, *player::pos_z(mb) >> 12,
                         *player::pos_x(a) >> 12, *player::pos_y(a) >> 12, *player::pos_z(a) >> 12,
                         reg ? 1 : 0,
                         *(const int *)((const char *)mb + 0x2e0), *(const int *)((const char *)mb + 0x2e4),
                         *(const int *)((const char *)mb + 0x2e8), *(const unsigned *)((const char *)mb + 0x2f4),
                         *(const unsigned *)((const char *)mb + 0x2f8));
        }
    }
    if (g_log > 0 && (g_pass % 60u) == 0)
        std::fprintf(stderr, "[party] t=%u seat=%d body=%d solid_frames=%llu registers=%llu guard_skips=%llu "
                     "gate(hide=%llu level=%llu area=%llu) listnodes_at_follow=%llu pairs_body=%llu pairs_other=%llu\n",
                     comms_hub_now_ms(), seat, body, g_st.solid_frames, g_st.registers, g_st.guard_skips,
                     g_st.gate_hide, g_st.gate_level, g_st.gate_area, g_st.audit_nodes, g_st.pairs_body,
                     g_st.pairs_other);
    return reg;
}

const Stats &stats() { return g_st; }

Stats &stats_rw() { return g_st; }

int log_level() { read_env(); return g_log; }

}  // namespace party
}  // namespace port

extern "C" int port_party_member(int slot) {
    return port::party::body_solid(slot) ? 1 : 0;
}
