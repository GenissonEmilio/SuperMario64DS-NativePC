// THE ONLINE PING READOUT, the view half. Run mp1, lane PINGHUD1.
// What each number is and when it shows: hal/net_hud.h.

#include "hal/net_hud.h"

#include "hal/comms_hub.h"
#include "hal/comms_loopback.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <windows.h>   // GetTickCount

namespace port {
namespace {

enum : unsigned { kRefreshMs = 500, kLogEveryMs = 10000 };
enum : int { kLossWindow = 20 };   // 500 ms intervals: ten seconds
// A seat unheard this long is leaving (the hub keeps it 3 s) or frozen, not
// on a lossy path: its ghost frames come 15 times a second, so a 400 ms
// silence is six frames in a row lost, about 1 in 15000 at 20% loss.
enum : unsigned { kQuietMs = 400, kLossSpanMs = 10000 };

NetHudView g_view;          // what is on screen until the next refresh
bool       g_view_valid = false;
unsigned   g_next_ms = 0;
unsigned   g_next_log_ms = 0;
bool       g_shown = false;
int        g_kind = 0;      // 0 none, 1 room, 2 VS

// THE ROOM'S LOSS WINDOW: the pings the hub sent and the pongs it got back,
// per refresh interval, over the last ten seconds. An interval counts only
// when it was CLEAN at both ends -- somebody in the room and nobody in it
// gone quiet -- because a ping with nobody to ask is never answered, and a
// ping to a player who just left or froze is not the network losing it. It is
// held back one interval before it counts, so a pong still in flight when a
// player leaves is not read as lost either. And the newest pings of the window
// may simply not be answered YET (a pong lands up to one round trip after its
// ping, often in the next interval), so that many are left out of the count.
struct Interval { unsigned long long ping, pong; unsigned at; };
Interval g_win[kLossWindow];
int      g_win_n = 0, g_win_head = 0;
Interval g_pending;
bool     g_have_pending = false;
unsigned long long g_prev_ping = 0, g_prev_pong = 0;
bool     g_prev_valid = false, g_prev_clean = false;

void loss_reset() {
    g_win_n = g_win_head = 0;
    g_have_pending = false;
    g_prev_valid = false;
}

void loss_sample(const CommsHubStats &st, bool clean, unsigned now) {
    if (g_have_pending && clean) {
        g_win[g_win_head] = g_pending;
        g_win_head = (g_win_head + 1) % kLossWindow;
        if (g_win_n < kLossWindow) ++g_win_n;
    }
    g_have_pending = false;
    if (g_prev_valid && g_prev_clean && clean &&
        st.pings_sent >= g_prev_ping && st.pongs_recvd >= g_prev_pong) {
        g_pending.ping = st.pings_sent - g_prev_ping;
        g_pending.pong = st.pongs_recvd - g_prev_pong;
        g_pending.at = now;
        g_have_pending = true;
    }
    g_prev_ping = st.pings_sent;
    g_prev_pong = st.pongs_recvd;
    g_prev_clean = clean;
    g_prev_valid = true;
}

// Loss in whole percent over the last ten seconds, -1 until they hold four
// answerable pings. `rtt_ms` (the room's mean round trip) sizes the in-flight
// allowance: one ping, plus one per 500 ms of round trip.
int loss_pct(unsigned now, int rtt_ms) {
    unsigned long long ping = 0, pong = 0;
    for (int i = 0; i < g_win_n; ++i) {
        if ((unsigned)(now - g_win[i].at) > kLossSpanMs) continue;
        ping += g_win[i].ping;
        pong += g_win[i].pong;
    }
    const unsigned long long in_flight = 1 + (unsigned)(rtt_ms > 0 ? rtt_ms : 0) / kRefreshMs;
    if (ping < in_flight + 4) return -1;
    ping -= in_flight;
    if (pong >= ping) return 0;
    return (int)(((ping - pong) * 100ull + ping / 2) / ping);
}

void put_loss(char *out, int pct) {
    if (pct < 0) std::snprintf(out, kNetHudLineMax, "loss --");
    else         std::snprintf(out, kNetHudLineMax, "loss %d%%", pct);
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

// THE ROOM'S PING IS A MEDIAN. The hub keeps only each seat's newest round
// trip, and one frame hitch on either console (a level load, a stall) makes
// that one reading jump by a hundred milliseconds or more. So each seat's
// reading is sampled at every refresh into a ring of the last five, and the
// seat counts with the median of those: a single late pong never reaches the
// screen, a real change shows within three refreshes (1.5 s). A seat that
// leaves, or comes back as a new join, starts its ring again.
enum : int { kRttRing = 5 };
struct SeatRtt { unsigned epoch; int n, head; int v[kRttRing]; };
SeatRtt g_seat_rtt[kHubMaxSeats];

// SM64DS_NET_HUD_TRACE=1 (test only): one line per seat per refresh, the raw
// reading next to the median the readout uses.
bool trace_on() {
    static int t = -1;
    if (t < 0) t = std::getenv("SM64DS_NET_HUD_TRACE") ? 1 : 0;
    return t == 1;
}

int seat_median(int s, const HubSeatInfo &info) {
    SeatRtt &r = g_seat_rtt[s];
    if (r.epoch != info.join_epoch) {
        r.epoch = info.join_epoch;
        r.n = r.head = 0;
    }
    r.v[r.head] = info.rtt_ms;
    r.head = (r.head + 1) % kRttRing;
    if (r.n < kRttRing) ++r.n;
    int sorted[kRttRing];
    for (int i = 0; i < r.n; ++i) sorted[i] = r.v[i];
    for (int i = 1; i < r.n; ++i)            // five values: insertion sort
        for (int j = i; j > 0 && sorted[j - 1] > sorted[j]; --j) {
            const int t = sorted[j]; sorted[j] = sorted[j - 1]; sorted[j - 1] = t;
        }
    const int med = sorted[r.n / 2];
    if (trace_on())
        std::fprintf(stderr, "[nethud] trace seat=%d raw=%d median=%d\n", s,
                     info.rtt_ms, med);
    return med;
}

// THE ROOM. True when the hub is pairing or live.
bool build_room(NetHudView *v) {
    if (!comms_hub_active()) return false;
    const int state = comms_hub_state();
    if (state != kHubPairing && state != kHubLive) return false;
    const CommsHubStats st = comms_hub_stats();
    const unsigned now = GetTickCount();
    long long sum = 0;
    int n = 0;
    bool quiet = false;
    for (int s = 0; s < kHubMaxSeats; ++s) {
        if (!(st.seat_mask & (1u << s))) continue;
        HubSeatInfo info;
        if (!comms_hub_seat_info(s, &info) || !info.present) {
            g_seat_rtt[s].n = 0;
            continue;
        }
        if (info.heard_ms_ago > kQuietMs) quiet = true;
        if (info.rtt_ms < 0) continue;
        sum += seat_median(s, info);
        ++n;
    }
    loss_sample(st, st.seat_mask != 0 && !quiet, now);
    const int ping = n ? (int)((sum + n / 2) / n) : -1;
    put_ping(v->line[0], "ping", ping);
    if (st.seat_mask) {
        put_loss(v->line[1], loss_pct(now, ping));
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
