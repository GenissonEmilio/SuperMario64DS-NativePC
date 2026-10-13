// STAR TALLY: see star_tally.h. Run mp1, lane SMALLS9.
#include "star_tally.h"

#include "comms_hub.h"
#include "coop_world.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250;
extern signed char data_0209f2f8;
extern int data_0209f220[];
void *_ZN8dActor_c4NextEPKS_(const void *prev);
int func_ov002_020e73ac(char *star);
}

namespace port {
namespace {

struct Entry { uint32_t key; uint16_t count; };

struct State {
    bool inited, enabled, log;
    unsigned calls;
    int mine;                     // my own count this session
    uint32_t key;                 // hash of my name, 0 = none
    void *seen[16]; int nseen;
    int lvl, act; bool have_lvl;
    Entry tbl[kStarTallyMax]; int ntbl;   // host: the table
    Entry rx[kStarTallyMax];  int nrx;    // guest: the last host table
    bool dirty;
    ULONGLONG last_ms;
    struct Fake { bool used, done; int n; unsigned frame; } fake[6];
};
State g;

uint32_t fnv(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) { h ^= (unsigned char)*s; h *= 16777619u; }
    return h ? h : 1u;
}

void init_once() {
    if (g.inited) return;
    g.inited = true;
    const char *e = std::getenv("SM64DS_STAR_TALLY");
    g.enabled = !(e && e[0] == '0' && e[1] == 0);
    e = std::getenv("SM64DS_STAR_TALLY_LOG");
    g.log = e && *e && !(e[0] == '0' && e[1] == 0);
    if (const char *nm = std::getenv("SM64DS_PLAYER_NAME")) if (*nm) g.key = fnv(nm);
    if (const char *f = std::getenv("SM64DS_STAR_TALLY_FAKE")) {
        const char *p = f; int k = 0;
        while (*p && k < 6) {
            int n = 0, fr = 0, used = 0;
            if (std::sscanf(p, "%d@%d%n", &n, &fr, &used) == 2 && n > 0 && fr >= 0) {
                g.fake[k] = { true, false, n, (unsigned)fr }; ++k; p += used;
            } else break;
            if (*p == ';') ++p;
        }
    }
    std::fprintf(stderr, "[startally] on: enabled=%d key=%08x\n", (int)g.enabled, g.key);
}

bool coop_now() {
    return g.enabled && comms_hub_active() && !comms_hub_arena() &&
           comms_hub_room_kind() == kHubRoomCoop && coop_world_active();
}

// sets t[key] = count (or the larger of the two when take_max)
void table_set(Entry *t, int *n, uint32_t key, int count, bool take_max) {
    for (int i = 0; i < *n; ++i)
        if (t[i].key == key) {
            if (!take_max || count > t[i].count) t[i].count = (uint16_t)count;
            return;
        }
    if (*n < kStarTallyMax) { t[*n].key = key; t[*n].count = (uint16_t)count; ++*n; }
}

int table_get(const Entry *t, int n, uint32_t key) {
    for (int i = 0; i < n; ++i) if (t[i].key == key) return t[i].count;
    return -1;
}

void add_mine(int n, const char *why) {
    g.mine += n;
    g.dirty = true;
    std::fprintf(stderr, "[startally] +%d (%s): my own count now %d, level %d act %d\n", n, why,
                 g.mine, (int)data_0209f2f8, data_0209f220[0]);
}

void scan() {
    void *me = (data_0209f250 < 16) ? data_0209f394[data_0209f250] : nullptr;
    if (!me) return;
    const int lv = data_0209f2f8, ac = data_0209f220[0];
    if (!g.have_lvl || lv != g.lvl || ac != g.act) { g.have_lvl = true; g.lvl = lv; g.act = ac; g.nseen = 0; }
    void *keep[16]; int nkeep = 0;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        if (*(const unsigned short *)(a + 0x0c) != 178) continue;
        if (*(const unsigned *)(a + 0x440) != 5u || *(void *const *)(a + 0x438) != me) continue;
        bool seen = false;
        for (int i = 0; i < g.nseen; ++i) seen = seen || g.seen[i] == a;
        if (nkeep < 16) keep[nkeep++] = a;
        if (seen) continue;
        if (func_ov002_020e73ac(a) != 0) continue;       // key / special / silver: not a counted star
        add_mine(1, "star taken by my body");
    }
    // forget stars no longer in the take state, so a new star at the same address counts again
    std::memcpy(g.seen, keep, sizeof(void *) * (size_t)nkeep);
    g.nseen = nkeep;
}

}  // namespace

int star_tally_mine() { return g.mine; }
void star_tally_poke() { g.dirty = true; g.last_ms = 0; }

int star_tally_wire_value(int total) {
    init_once();
    if (!coop_now()) return total;
    ++g.calls;
    for (State::Fake &f : g.fake)
        if (f.used && !f.done && g.calls >= f.frame) { f.done = true; add_mine(f.n, "fake"); }
    scan();
    // a returning console takes the count the host kept
    if (g.key && g.nrx) {
        const int t = table_get(g.rx, g.nrx, g.key);
        if (t > g.mine) {
            std::fprintf(stderr, "[startally] restored %d from the host (was %d)\n", t, g.mine);
            g.mine = t;
        }
    }
    if (g.log && (g.calls % 600u) == 0)
        std::fprintf(stderr, "[startally] mine=%d total=%d\n", g.mine, total);
    return g.mine > 65535 ? 65535 : g.mine;
}

int star_tally_take_outgoing(bool host, uint8_t *out) {
    init_once();
    if (!coop_now()) return 0;
    const ULONGLONG now = GetTickCount64();
    if (host && g.key) {
        const int before = table_get(g.tbl, g.ntbl, g.key);
        table_set(g.tbl, &g.ntbl, g.key, g.mine, true);
        if (before < g.mine) g.dirty = true;
    }
    if (!g.dirty && now - g.last_ms < 3000) return 0;
    if (g.last_ms && now - g.last_ms < 150) return 0;
    std::memset(out, 0, kStarTallyPayloadMax);
    int n = 0;
    if (host) {
        out[0] = 1;
        for (int i = 0; i < g.ntbl; ++i) {
            const uint32_t k = g.tbl[i].key; const uint16_t c = g.tbl[i].count;
            std::memcpy(out + 4 + 8 * n, &k, 4); std::memcpy(out + 8 + 8 * n, &c, 2); ++n;
        }
    } else if (g.key) {
        out[0] = 0;
        const uint32_t k = g.key; const uint16_t c = (uint16_t)g.mine;
        std::memcpy(out + 4, &k, 4); std::memcpy(out + 8, &c, 2); n = 1;
    }
    g.dirty = false; g.last_ms = now;
    if (n == 0) return 0;
    out[1] = (uint8_t)n;
    return 4 + 8 * n;
}

bool star_tally_on_payload(bool host, int seat, const uint8_t *pl, int len) {
    init_once();
    if (!pl || len < 4 || pl[0] > 1 || pl[1] > kStarTallyMax || pl[2] || pl[3] || len != 4 + 8 * pl[1]) return false;
    for (int i = 0; i < pl[1]; ++i) {            // validate every entry first
        uint32_t k; uint16_t z;
        std::memcpy(&k, pl + 4 + 8 * i, 4); std::memcpy(&z, pl + 10 + 8 * i, 2);
        if (!k || z) return false;
    }
    if (!coop_now()) return true;
    const bool from_host = pl[0] == 1;
    if (from_host == host) return true;          // a host table is for guests, a guest entry for the host
    if (host) {
        for (int i = 0; i < pl[1]; ++i) {
            uint32_t k; uint16_t c;
            std::memcpy(&k, pl + 4 + 8 * i, 4); std::memcpy(&c, pl + 8 + 8 * i, 2);
            const int before = table_get(g.tbl, g.ntbl, k);
            table_set(g.tbl, &g.ntbl, k, c, true);
            if (before < (int)c) {
                g.dirty = true;
                if (g.log) std::fprintf(stderr, "[startally] host table: seat %d key %08x -> %d\n", seat, k, (int)c);
            }
        }
    } else {
        g.nrx = 0;
        for (int j = 0; j < pl[1]; ++j) {
            uint32_t kk; uint16_t cc;
            std::memcpy(&kk, pl + 4 + 8 * j, 4); std::memcpy(&cc, pl + 8 + 8 * j, 2);
            table_set(g.rx, &g.nrx, kk, cc, false);
        }
    }
    return true;
}

}  // namespace port
