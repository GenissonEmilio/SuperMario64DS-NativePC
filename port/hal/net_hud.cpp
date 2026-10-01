// THE ONLINE PING READOUT, the view half. Run mp1, lane PINGHUD1.
// What each number is and when it shows: hal/net_hud.h.

#include "hal/net_hud.h"

#include "hal/comms_hub.h"
#include "hal/comms_loopback.h"

#include <cstdio>
#include <cstring>

#include <windows.h>   // GetTickCount

namespace port {
namespace {

enum : unsigned { kRefreshMs = 500, kLogEveryMs = 10000 };
enum : int { kLossWindow = 20 };   // 500 ms intervals: ten seconds

NetHudView g_view;          // what is on screen until the next refresh
bool       g_view_valid = false;
unsigned   g_next_ms = 0;
unsigned   g_next_log_ms = 0;
bool       g_shown = false;
int        g_kind = 0;      // 0 none, 1 room, 2 VS

// THE ROOM'S LOSS WINDOW. One entry per refresh interval in which somebody
// was in the room at both ends (a ping with nobody to ask is never answered
// and is not a loss): the pings the hub sent and the pongs it got back.
struct Interval { unsigned long long ping, pong; };
Interval g_win[kLossWindow];
int      g_win_n = 0, g_win_head = 0;
unsigned long long g_prev_ping = 0, g_prev_pong = 0;
bool     g_prev_valid = false, g_prev_peers = false;

void loss_reset() {
    g_win_n = g_win_head = 0;
    g_prev_valid = false;
}

void loss_sample(const CommsHubStats &st) {
    const bool peers = st.seat_mask != 0;
    if (g_prev_valid && peers && g_prev_peers &&
        st.pings_sent >= g_prev_ping && st.pongs_recvd >= g_prev_pong) {
        g_win[g_win_head].ping = st.pings_sent - g_prev_ping;
        g_win[g_win_head].pong = st.pongs_recvd - g_prev_pong;
        g_win_head = (g_win_head + 1) % kLossWindow;
        if (g_win_n < kLossWindow) ++g_win_n;
    }
    g_prev_ping = st.pings_sent;
    g_prev_pong = st.pongs_recvd;
    g_prev_peers = peers;
    g_prev_valid = true;
}

// Loss in tenths of a percent, -1 until the window holds four pings.
int loss_tenths() {
    unsigned long long ping = 0, pong = 0;
    for (int i = 0; i < g_win_n; ++i) { ping += g_win[i].ping; pong += g_win[i].pong; }
    if (ping < 4) return -1;
    if (pong >= ping) return 0;
    return (int)(((ping - pong) * 1000ull + ping / 2) / ping);
}

void put_loss(char *out, int tenths) {
    if (tenths < 0)
        std::snprintf(out, kNetHudLineMax, "loss --");
    else if (tenths < 100)
        std::snprintf(out, kNetHudLineMax, "loss %d.%d%%", tenths / 10, tenths % 10);
    else
        std::snprintf(out, kNetHudLineMax, "loss %d%%", (tenths + 5) / 10);
}

void put_ping(char *out, const char *label, int ms) {
    if (ms < 0) std::snprintf(out, kNetHudLineMax, "%s --", label);
    else        std::snprintf(out, kNetHudLineMax, "%s %d ms", label, ms);
}

int popcount16(unsigned m) {
    int n = 0;
    for (; m; m &= m - 1) ++n;
    return n;
}

// THE ROOM. True when the hub is pairing or live.
bool build_room(NetHudView *v) {
    if (!comms_hub_active()) return false;
    const int state = comms_hub_state();
    if (state != kHubPairing && state != kHubLive) return false;
    const CommsHubStats st = comms_hub_stats();
    loss_sample(st);
    long long sum = 0;
    int n = 0;
    for (int s = 0; s < kHubMaxSeats; ++s) {
        if (!(st.seat_mask & (1u << s))) continue;
        HubSeatInfo info;
        if (!comms_hub_seat_info(s, &info) || !info.present || info.rtt_ms < 0)
            continue;
        sum += info.rtt_ms;
        ++n;
    }
    put_ping(v->line[0], "ping", n ? (int)((sum + n / 2) / n) : -1);
    if (st.seat_mask) {
        put_loss(v->line[1], loss_tenths());
        v->n = 2;
    } else {
        v->n = 1;
    }
    return true;
}

// THE VS MATCH. True while the lockstep carrier has two or more live players.
bool build_vs(NetHudView *v) {
    const CommsLoopbackStats s = comms_loopback_stats();
    if (!s.installed || (s.role != 1 && s.role != 2)) return false;
    if (popcount16((unsigned)s.live_mask & 0xFFFFu) < 2) return false;
    if (s.role == 2) {   // a joiner: its own round trip, then the delay
        put_ping(v->line[0], "ping", s.handshake_rtt_ms);
        std::snprintf(v->line[1], kNetHudLineMax, "delay %d", s.input_delay);
        v->n = 2;
        return true;
    }
    // the host: the delay, then the slowest joiner
    std::snprintf(v->line[0], kNetHudLineMax, "delay %d", s.input_delay);
    int worst = -1, worst_slot = -1;
    for (int k = 1; k < 16; ++k) {
        if (!((unsigned)s.live_mask & (1u << k))) continue;
        const int r = comms_loopback_child_rtt_ms(k);
        if (r > worst) { worst = r; worst_slot = k; }
    }
    if (worst >= 0)
        std::snprintf(v->line[1], kNetHudLineMax, "worst %d ms P%d", worst,
                      worst_slot + 1);
    else
        std::snprintf(v->line[1], kNetHudLineMax, "worst --");
    v->n = 2;
    return true;
}

}  // namespace

void net_hud_view(NetHudView *out) {
    if (!out) return;
    const unsigned now = GetTickCount();
    if (!g_view_valid || (unsigned)(now - g_next_ms) < 0x80000000u) {
        NetHudView v;
        std::memset(&v, 0, sizeof v);
        int kind = 0;
        if (build_room(&v)) kind = 1;
        else if (build_vs(&v)) kind = 2;
        else v.n = 0;
        if (kind != 1 && g_kind == 1) loss_reset();
        // ONE LINE IN THE LOG when the readout appears or goes, and every ten
        // seconds while it shows, so a player's log carries what he saw.
        const bool shown = v.n > 0;
        if (shown != g_shown ||
            (shown && (unsigned)(now - g_next_log_ms) < 0x80000000u)) {
            if (shown)
                std::fprintf(stderr, "[nethud] %s: %s | %s\n",
                             g_shown ? "showing" : "shown",
                             v.line[0], v.n > 1 ? v.line[1] : "-");
            else
                std::fprintf(stderr, "[nethud] hidden\n");
            g_next_log_ms = now + kLogEveryMs;
        }
        g_shown = shown;
        g_kind = kind;
        g_view = v;
        g_view_valid = true;
        g_next_ms = now + kRefreshMs;
    }
    *out = g_view;
}

}  // namespace port
