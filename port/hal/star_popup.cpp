// See star_popup.h. Run mp1, lane STARPOP1.
#include "star_popup.h"

#include "comms_hub.h"
#include "coop_wire.h"
#include "coop_world.h"
#include "hub_presence.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250;
extern signed char data_0209f2f8;
extern int data_0209f220[];
extern signed char SUBLEVEL_LEVEL_TABLE[];
extern signed char data_0209211c;         // next level (set by SetNextLevel)
extern unsigned char data_0209f200;       // its entrance
extern unsigned char data_0209f2d8[];     // stage mode
extern unsigned char data_0209d660[];     // message window
extern unsigned char data_0209d4b0[];     // transition / hold
extern unsigned char data_0209f294[];     // star select
extern unsigned char data_0209f2c4[];     // pause menu
extern unsigned char data_0209f20c[];     // level clear
extern unsigned char data_0209b454[];     // fade / scene change flags
extern unsigned char data_0209fc48[];     // cutscene
extern unsigned char data_02092110[];     // pending level change, <0 = none
extern unsigned char data_0209f22c[];     // hold timer
void *_ZN8dActor_c4NextEPKS_(const void *prev);
int func_ov002_020e73ac(char *star);      // the ROM's star classifier
void ExitLevel(void);
void port_chat_system_line(const char *text);
}

namespace port {
namespace {

enum Mode { kNone, kParty, kCoop };
enum : int { kKindStar = 1, kKindKey = 2 };
enum : unsigned { kFlagExits = 1u };
enum : int { kCtlStarPop = 0x1e };
enum : unsigned { kCountMs = 5000, kShowAfterMs = 2500 };
enum : int { kSeenMax = 16, kPendMax = 8, kSeqMemory = 12 };
// SYNX reserved word
enum : unsigned { kXStar = 1u, kXKey = 2u, kXNoExit = 4u };

#pragma pack(push, 1)
struct PopWire {            // SYNB 0x1e payload, 8 B
    CoopKeyWire key;        // sender's level + act
    uint8_t kind;           // 1 star, 2 key
    uint8_t flags;          // bit0 the collector exits
    uint8_t star;           // star number 0..8
    uint8_t seq;
};
#pragma pack(pop)
static_assert(sizeof(PopWire) == 8, "popup event is 8 bytes");

struct Pending { bool used; unsigned due; int left; unsigned char buf[16]; int len; bool coop; PopWire w; };
struct Fake { bool used, done; int kind, exits; unsigned frame; };
struct SeqMem { int seat; unsigned seq; };

struct State {
    bool inited, enabled, log;
    unsigned frame;
    // sender
    void *seen[kSeenMax]; int nseen;
    int  lvl, act; bool have_key;
    unsigned seq;
    Pending pend[kPendMax];
    // receiver
    SeqMem mem[kSeqMemory]; int nmem;
    bool counting; unsigned t_end; bool exit_pending; bool exit_key;
    char line[kStarPopupLines - 1][kStarPopupLineMax]; int nline;
    unsigned t_last;
    bool registered;
    StarPopupStats st;
    // knobs
    Fake fake[6];
    int hold_from, hold_n;
    unsigned last_wait_log;
    // proof knob SM64DS_STAR_POPUP_STOP: end the selftest at a chosen moment (a picture)
    int stop_kind, stop_val; bool armed, stop_flag, stop_timer; unsigned stop_at;
};
State g;

unsigned now_ms() { return (unsigned)GetTickCount64(); }
long long wall_ms() {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    return (long long)((((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000ull);
}

bool env_on(const char *name, bool dflt) {
    const char *e = std::getenv(name);
    if (!e || !*e) return dflt;
    return !(e[0] == '0' && e[1] == 0);
}

void init_once() {
    if (g.inited) return;
    g.inited = true;
    g.enabled = env_on("SM64DS_STAR_POPUP", true);
    g.log = env_on("SM64DS_STAR_POPUP_LOG", false);
    g.hold_from = -1;
    if (const char *e = std::getenv("SM64DS_STAR_POPUP_HOLD")) {
        int f = 0, n = 0;
        if (std::sscanf(e, "%d:%d", &f, &n) == 2 && f >= 0 && n > 0) { g.hold_from = f; g.hold_n = n; }
    }
    if (const char *e = std::getenv("SM64DS_STAR_POPUP_FAKE")) {
        const char *p = e; int k = 0;
        while (*p && k < 6) {
            int kd = 0, ex = 0, f = 0, used = 0;
            if (std::sscanf(p, "%d:%d@%d%n", &kd, &ex, &f, &used) == 3 && (kd == 1 || kd == 2) && f >= 0) {
                g.fake[k] = { true, false, kd, ex ? 1 : 0, (unsigned)f }; ++k; p += used;
            } else break;
            if (*p == ';') ++p;
        }
    }
    if (const char *e = std::getenv("SM64DS_STAR_POPUP_STOP")) {
        int v = 0;
        if (std::sscanf(e, "cd%d", &v) == 1) { g.stop_kind = 1; g.stop_val = v; }       // the countdown shows v
        else if (std::sscanf(e, "tx%d", &v) == 1) { g.stop_kind = 2; g.stop_val = v; }  // v ms after my own event
        else if (std::sscanf(e, "lv%d", &v) == 1) { g.stop_kind = 3; g.stop_val = v; }  // v ms after the level changed following an event
    }
    std::fprintf(stderr, "[starpop] on: enabled=%d\n", (int)g.enabled);
}

Mode mode_now() {
    if (!g.enabled) return kNone;
    if (!comms_hub_active() || comms_hub_arena() || comms_hub_state() != kHubLive) return kNone;
    const HubRoomKind k = comms_hub_room_kind();
    if (k == kHubRoomParty) {
        const char *e = std::getenv("SM64DS_PARTY");
        return (e && *e && !(e[0] == '0' && e[1] == 0)) ? kParty : kNone;
    }
    if (k == kHubRoomCoop) return coop_world_active() ? kCoop : kNone;
    return kNone;
}

int my_seat() { return comms_hub_my_seat(); }
void *my_body() {
    const int me = (int)data_0209f250;
    return (me >= 0 && me < 16) ? data_0209f394[me] : nullptr;
}
unsigned rd32(const unsigned char *p) { unsigned v; std::memcpy(&v, p, 4); return v; }

void seat_name(int seat, char *out, int cap) {
    out[0] = 0;
    unsigned char id[64];
    const int n = comms_hub_latest(kHubIdent, seat, id, (int)sizeof id, nullptr);
    if (n >= (int)sizeof(HubIdentHead)) {
        HubIdentHead h; std::memcpy(&h, id, sizeof h);
        int nl = h.name_len;
        if (nl > kHubIdentNameMax) nl = kHubIdentNameMax;
        if ((int)sizeof h + nl > n) nl = n - (int)sizeof h;
        int k = 0;
        for (int i = 0; i < nl && k < cap - 1 && k < 16; ++i) {
            const unsigned char c = id[sizeof h + i];
            if (c >= 0x20 && c < 0x7f) out[k++] = (char)c;
        }
        out[k] = 0;
    }
    if (!out[0]) std::snprintf(out, (size_t)cap, "Player %d", seat + 1);
}

// ---- the exit -------------------------------------------------------------
// func_ov002_020c94a4 (Player, after the star-get dance): sub 0 = star, sub 3 = key.
void landing_overrides(bool key) {
    if (!key) {
        if (data_0209f2f8 == 0x21) { data_0209211c = 4; data_0209f200 = 7; }
        return;
    }
    switch (data_0209f2f8) {
    case 0x24: data_0209211c = 2;    data_0209f200 = 0xa; break;
    case 0x26: data_0209211c = 4;    data_0209f200 = 9;   break;
    case 0x2d: data_0209211c = 0x32; data_0209f200 = 3;   break;
    case 0x2f: data_0209211c = 3;    data_0209f200 = 0;   break;
    case 0x31: data_0209211c = 5;    data_0209f200 = 0xb; break;
    default: break;
    }
}

// 0 = go, 1 = wait, 2 = already leaving (drop the exit)
int exit_gate(const char **why) {
    *why = "";
    if (!my_body()) { *why = "nobody"; return 1; }
    if ((signed char)data_02092110[0] >= 0) { *why = "leaving"; return 2; }
    if (g.hold_from >= 0 && (int)g.frame >= g.hold_from && (int)g.frame < g.hold_from + g.hold_n) { *why = "forced"; return 1; }
    if (data_0209f2d8[0] == 2) { *why = "mode"; return 1; }
    if (data_0209d660[0]) { *why = "message"; return 1; }
    if (rd32(data_0209d4b0)) { *why = "d4b0"; return 1; }
    if (data_0209f294[0]) { *why = "starselect"; return 1; }
    if (data_0209f2c4[0]) { *why = "pause"; return 1; }
    // data_0209f20c (level clear) is NOT a gate: the Bowser arenas hold it at 1 for the whole fight, and a level clear that
    // matters always comes with a fade, a cutscene or a pending level change, which are gates.
    if (rd32(data_0209b454)) { *why = "fade"; return 1; }
    if (rd32(data_0209fc48)) { *why = "cutscene"; return 1; }
    if (data_0209f22c[0]) { *why = "timer"; return 1; }
    return 0;
}

void clear_receiver() {
    g.counting = false; g.exit_pending = false; g.nline = 0; g.t_end = 0;
}

void toast(const char *name, int kind) {
    char t[80];
    std::snprintf(t, sizeof t, "%s collected %s", name, kind == kKindKey ? "a key" : "a star");
    port_chat_system_line(t);
    ++g.st.toast;
}

bool seen_seq(int seat, unsigned seq) {
    for (int i = 0; i < g.nmem; ++i) if (g.mem[i].seat == seat && g.mem[i].seq == seq) return true;
    if (g.nmem < kSeqMemory) { g.mem[g.nmem].seat = seat; g.mem[g.nmem].seq = seq; ++g.nmem; }
    else {
        std::memmove(g.mem, g.mem + 1, sizeof g.mem[0] * (kSeqMemory - 1));
        g.mem[kSeqMemory - 1].seat = seat; g.mem[kSeqMemory - 1].seq = seq;
    }
    return false;
}

// An event arrived from `seat`. `same` = the sender stands in my level and act.
void on_event(int seat, int kind, bool exits, bool same) {
    char name[24];
    seat_name(seat, name, sizeof name);
    ++g.st.rx;
    const unsigned now = now_ms();
    if (!same || !my_body()) {
        toast(name, kind);
        if (g.log) std::fprintf(stderr, "[starpop] rx seat=%d kind=%d exits=%d same=0 -> toast wall=%lld\n", seat, kind, (int)exits, wall_ms());
        return;
    }
    if (!exits) {
        toast(name, kind);       // the collector keeps playing: nobody exits
        if (g.log) std::fprintf(stderr, "[starpop] rx seat=%d kind=%d exits=0 same=1 -> toast, no exit wall=%lld\n", seat, kind, wall_ms());
        return;
    }
    if (g.nline < kStarPopupLines - 1) {
        std::snprintf(g.line[g.nline], kStarPopupLineMax, "%s collected the %s!", name, kind == kKindKey ? "key" : "star");
        ++g.nline;
    }
    g.t_last = now;
    ++g.st.shown;
    if (!g.counting && !g.exit_pending) {
        g.counting = true;
        g.t_end = now + kCountMs;
        g.exit_key = (kind == kKindKey);
    } else if (kind == kKindKey) {
        g.exit_key = true;       // a key anywhere in the batch: use the key landing
    }
    if (g.log) std::fprintf(stderr, "[starpop] rx seat=%d kind=%d exits=1 same=1 -> popup, countdown %s wall=%lld\n", seat, kind,
                            g.counting ? "running" : "pending exit", wall_ms());
}

// ---- wire -----------------------------------------------------------------
void queue_copies(const Pending &src) {
    for (Pending &p : g.pend) {
        if (p.used) continue;
        p = src; p.used = true; p.left = 2; p.due = now_ms() + 250;
        return;
    }
}

void send_event(Mode m, int kind, bool exits, int star) {
    g.seq = (g.seq + 1) & 0xffu;
    Pending p; std::memset(&p, 0, sizeof p);
    if (m == kCoop) {
        PopWire w; std::memset(&w, 0, sizeof w);
        w.key.level = (uint16_t)(data_0209f2f8 & 0xff); w.key.act = (uint8_t)(data_0209f220[0] & 0xf);
        w.kind = (uint8_t)kind; w.flags = exits ? kFlagExits : 0; w.star = (uint8_t)(star & 0xf); w.seq = (uint8_t)g.seq;
        p.coop = true; p.w = w;
        coop_send_ctl(kCtlStarPop, &w, sizeof w);
    } else {
        const int lv = data_0209f2f8;
        int course = (lv >= 0 && lv < 52) ? SUBLEVEL_LEVEL_TABLE[lv] : 0;
        if (course < 0 || course > 29) course = 0;
        unsigned char b[kHubPartyStarBytes];
        if (hub_party_star_encode(course, star & 7, g.seq, b) != kHubPartyStarBytes) return;
        const unsigned rsv = (kind == kKindKey ? kXKey : kXStar) | (exits ? 0u : kXNoExit);
        b[8] = (unsigned char)rsv; b[9] = 0;
        std::memcpy(p.buf, b, sizeof b); p.len = (int)sizeof b; p.coop = false;
        comms_hub_send(b, (int)sizeof b);
    }
    ++g.st.tx;
    g.armed = true;
    if (g.stop_kind == 2 && !g.stop_timer) { g.stop_timer = true; g.stop_at = now_ms() + (unsigned)g.stop_val; }
    queue_copies(p);
    std::fprintf(stderr, "[starpop] tx mode=%s kind=%d exits=%d star=%d level=%d act=%d seq=%u wall=%lld\n", m == kCoop ? "coop" : "party",
                 kind, (int)exits, star, (int)data_0209f2f8, data_0209f220[0], g.seq, wall_ms());
}

void send_copies() {
    const unsigned now = now_ms();
    for (Pending &p : g.pend) {
        if (!p.used || (int)(now - p.due) < 0) continue;
        if (p.coop) coop_send_ctl(kCtlStarPop, &p.w, sizeof p.w);
        else comms_hub_send(p.buf, p.len);
        if (--p.left <= 0) p.used = false; else p.due = now + 450;
    }
}

void on_ctl(int seat, int sub, const unsigned char *pl, int len, void *) {
    if (sub != kCtlStarPop) { ++g.st.dropped; return; }
    if (!g.enabled || mode_now() != kCoop) return;
    if (!pl || len != (int)sizeof(PopWire)) { ++g.st.dropped; return; }
    PopWire w; std::memcpy(&w, pl, sizeof w);
    if (!coop_key_ok(w.key) || (w.kind != kKindStar && w.kind != kKindKey) || (w.flags & ~kFlagExits) || w.star > 8 ||
        seat < 0 || seat >= kCoopMaxSeats || seat == my_seat()) { ++g.st.dropped; return; }
    if (seen_seq(seat, w.seq)) return;
    const bool same = my_body() && w.key.level == (uint16_t)(data_0209f2f8 & 0xff) && w.key.act == (uint8_t)(data_0209f220[0] & 0xf);
    on_event(seat, w.kind, (w.flags & kFlagExits) != 0, same);
}

void scan_local(Mode m) {
    void *me = my_body();
    if (!me) return;
    for (char *a = (char *)_ZN8dActor_c4NextEPKS_(0); a; a = (char *)_ZN8dActor_c4NextEPKS_(a)) {
        const unsigned cls = *(const unsigned short *)(a + 0x0c);
        int kind = 0, star = 0;
        bool exits = false;
        if (cls == 178) {
            if (*(const unsigned *)(a + 0x440) != 5u || *(void *const *)(a + 0x438) != me) continue;
            const int r4 = func_ov002_020e73ac(a);
            if (r4 == 2) continue;                       // silver / VS: not a collect
            star = *(const unsigned char *)(a + 0x49d);
            kind = (r4 == 3) ? kKindKey : kKindStar;
            exits = (r4 == 0 || r4 == 3);
        } else if (cls == 282) {
            if (*(const int *)(a + 0x448) != 3 || *(void *const *)(a + 0x110) != me) continue;
            kind = kKindKey; exits = true; star = 8;
        } else continue;
        bool seen = false;
        for (int i = 0; i < g.nseen; ++i) seen = seen || g.seen[i] == a;
        if (seen || g.nseen >= kSeenMax) continue;
        g.seen[g.nseen++] = a;
        send_event(m, kind, exits, star);
    }
}

void new_level_check() {
    const int lv = data_0209f2f8, ac = data_0209f220[0];
    if (!g.have_key || lv != g.lvl || ac != g.act) {
        if (g.have_key && g.armed && g.stop_kind == 3 && !g.stop_timer) { g.stop_timer = true; g.stop_at = now_ms() + (unsigned)g.stop_val; }
        g.have_key = true; g.lvl = lv; g.act = ac;
        g.nseen = 0;
        if ((g.counting || g.exit_pending || g.nline) && g.log)
            std::fprintf(stderr, "[starpop] level changed (%d/%d): popup cleared\n", lv, ac);
        clear_receiver();
    }
}

}  // namespace

bool star_popup_party_rx(int seat, const unsigned char *d, int len) {
    if (!d || len != kHubPartyStarBytes || hub_rd16(d, 8) == 0) return false;
    init_once();
    const unsigned rsv = hub_rd16(d, 8);
    if (!g.enabled || mode_now() != kParty) return true;
    if ((rsv & ~7u) || ((rsv & 3u) != kXStar && (rsv & 3u) != kXKey) || seat < 0 || seat >= 4 || seat == my_seat()) {
        ++g.st.dropped;
        return true;
    }
    if (seen_seq(seat, hub_rd16(d, 10))) return true;
    HubPresence p;
    unsigned age = 0;
    const bool pres = hub_presence_get(seat, &p, &age);
    const bool same = pres && age < 5000 && p.in_level() && my_body() && p.level == (int)data_0209f2f8 && p.act == data_0209f220[0];
    on_event(seat, (rsv & kXKey) ? kKindKey : kKindStar, (rsv & kXNoExit) == 0, same);
    return true;
}

void star_popup_view(StarPopupView *out) {
    out->n = 0;
    if (!g.counting && !g.exit_pending && g.nline == 0) return;
    const unsigned now = now_ms();
    for (int i = 0; i < g.nline && out->n < kStarPopupLines; ++i)
        std::snprintf(out->line[out->n++], kStarPopupLineMax, "%s", g.line[i]);
    if (g.counting && out->n < kStarPopupLines) {
        int left = ((int)(g.t_end - now) + 999) / 1000;
        if (left < 1) left = 1;
        if (left > 5) left = 5;
        std::snprintf(out->line[out->n++], kStarPopupLineMax, "Exiting in %d...", left);
        if (g.stop_kind == 1 && left == g.stop_val && !g.stop_timer) { g.stop_timer = true; g.stop_at = now + 150; }
    } else if (g.exit_pending && out->n < kStarPopupLines) {
        std::snprintf(out->line[out->n++], kStarPopupLineMax, "Exiting in 1...");
    }
}

StarPopupStats star_popup_stats() { return g.st; }

}  // namespace port

extern "C" void port_star_popup_frame(void) {
    using namespace port;
    if (!std::getenv("SM64DS_PARTY") && !std::getenv("SM64DS_COOP")) return;
    init_once();
    ++g.frame;
    const Mode m = mode_now();
    if (m == kNone) { if (g.counting || g.exit_pending || g.nline) clear_receiver(); return; }
    if (!g.registered && m == kCoop) {
        g.registered = coop_ctl_register(kCtlStarPop, kCtlStarPop, on_ctl, nullptr);
        std::fprintf(stderr, "[starpop] coop wire %s\n", g.registered ? "registered" : "REFUSED");
    }
    new_level_check();
    for (Fake &f : g.fake)
        if (f.used && !f.done && g.frame >= f.frame && my_body()) {
            f.done = true;
            send_event(m, f.kind, f.exits != 0, f.kind == kKindKey ? 8 : 1);
        }
    scan_local(m);
    send_copies();
    const unsigned now = now_ms();
    if (g.stop_timer && !g.stop_flag && (int)(now - g.stop_at) >= 0) {
        g.stop_flag = true;
        std::fprintf(stderr, "[starpop] STOP requested (picture) wall=%lld\n", wall_ms());
    }
    if (g.counting && (int)(now - g.t_end) >= 0) { g.counting = false; g.exit_pending = true; }
    if (g.exit_pending) {
        const char *why = "";
        const int gate = exit_gate(&why);
        if (gate == 2) {
            if (g.log) std::fprintf(stderr, "[starpop] already leaving: exit dropped wall=%lld\n", wall_ms());
            clear_receiver();
        } else if (gate == 1) {
            if (g.log && (unsigned)(now - g.last_wait_log) > 1000u) {
                g.last_wait_log = now;
                std::fprintf(stderr, "[starpop] exit waits: %s (msg=%u d4b0=%u starsel=%u pause=%u lc=%u fade=%u cut=%u tmr=%u mode=%u) wall=%lld\n", why,
                             (unsigned)data_0209d660[0], rd32(data_0209d4b0), (unsigned)data_0209f294[0], (unsigned)data_0209f2c4[0],
                             (unsigned)data_0209f20c[0], rd32(data_0209b454), rd32(data_0209fc48), (unsigned)data_0209f22c[0],
                             (unsigned)data_0209f2d8[0], wall_ms());
            }
        } else {
            landing_overrides(g.exit_key);
            const int from = data_0209f2f8;
            ExitLevel();
            ++g.st.exits;
            g.armed = true;
            std::fprintf(stderr, "[starpop] EXIT from level %d act %d key=%d (ROM override globals now %d/%u, -1/0 = default exit) wall=%lld\n", from, data_0209f220[0],
                         (int)g.exit_key, (int)data_0209211c, (unsigned)data_0209f200, wall_ms());
            g.exit_pending = false; g.nline = 0;
        }
    }
    if (!g.counting && !g.exit_pending && g.nline && (unsigned)(now - g.t_last) > kShowAfterMs + kCountMs) g.nline = 0;
}

extern "C" int port_star_popup_stop_now(void) { return port::g.stop_flag ? 1 : 0; }
