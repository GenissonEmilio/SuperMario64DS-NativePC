// THE ADVENTURE HUB CARRIER, implemented. Run mp1, lane HUB1.
// The contract, the env knobs and the wire table are in comms_hub.h; the design
// is runs mp1 NETARCH1 design.txt sections 1, 3 and 6.
//
// PORT_HOST_ABI: host plumbing. Nothing in src/ knows this file exists, and
// nothing here reads or writes a byte of game state: it moves datagrams that
// other port layers build (hal/comms_sync.cpp ghosts and identity, the chat
// overlay, hal/voice_chat.cpp) and answers who is in the room.
//
// ===========================================================================
// WHY A SEPARATE CARRIER AND NOT AN "ADVENTURE MODE" OF comms_loopback.cpp
// ===========================================================================
//
// Up to 0.5.3 an adventure console opened the VS carrier and rode its aux
// channel. That carrier is a DS transport: installing it seats the DS
// multiplayer role, the conductor waits for the session before it seats a
// world, and the round exchange runs underneath even though adventure never
// consumes a round. GHOSTSCOUT measured what that costs: a 4-player adventure
// parent stalled 615 frames and "rewound" its own SOLO world 42 times, because
// one slow console held the rounds everyone was exchanging for nothing.
//
// Adventure has no shared simulation, so it needs no clock. This carrier has
// no rounds at all. It is never handed to comms_set_transport(), so
// comms_transport() stays null, and every conductor path already answers
// "solo" for a null transport: no seat request, no world-seat wait, no pump,
// no fan-out. The ROM runs the single-player game it was always running.
//
// ===========================================================================
// THE I/O THREAD
// ===========================================================================
//
// One host thread owns the socket's receive side, the relay keepalive (HELLO
// every 1 s; the relay forgets a hub member after 10 s of quiet), the 2 Hz
// ping, and the presence sweep. Callers only copy in and out of queues under
// one lock. Two reasons it is a thread and not a per-frame poll:
//   * presence and the keepalive must not depend on the game's frame rate; a
//     console sitting in a long load or a menu must keep its seat;
//   * nothing is added to the frame loop, which keeps the only walk_window.cpp
//     change this lane makes to the one install call.
// The thread never touches game memory.
//
// ws2_32 IS LoadLibrary'd, never imported (comms_loopback.cpp's banner has the
// measurement: a static ws2_32 import loses the DS fixed ranges). This file
// keeps its own small table rather than reaching into the loopback carrier's,
// so neither file can change the other's load order.

#include "comms_hub.h"

#include "host_settings.h"   // adventure_ghost_mode(), host_setting_voice_enabled

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace port {
namespace {

// ---------------------------------------------------------------------------
// ws2_32 by hand
// ---------------------------------------------------------------------------
struct HubWs {
    int    (WSAAPI *WSAStartup)(WORD, LPWSADATA);
    SOCKET (WSAAPI *socket)(int, int, int);
    int    (WSAAPI *WSAIoctl)(SOCKET, DWORD, LPVOID, DWORD, LPVOID, DWORD,
                              LPDWORD, LPWSAOVERLAPPED,
                              LPWSAOVERLAPPED_COMPLETION_ROUTINE);
    int    (WSAAPI *ioctlsocket)(SOCKET, long, u_long *);
    int    (WSAAPI *setsockopt)(SOCKET, int, int, const char *, int);
    int    (WSAAPI *bind)(SOCKET, const struct sockaddr *, int);
    int    (WSAAPI *sendto)(SOCKET, const char *, int, int,
                            const struct sockaddr *, int);
    int    (WSAAPI *recvfrom)(SOCKET, char *, int, int, struct sockaddr *,
                              int *);
    int    (WSAAPI *select)(int, fd_set *, fd_set *, fd_set *,
                            const struct timeval *);
    int    (WSAAPI *closesocket)(SOCKET);
    int    (WSAAPI *WSAGetLastError)(void);
    unsigned long (WSAAPI *inet_addr)(const char *);
    struct hostent *(WSAAPI *gethostbyname)(const char *);
};
HubWs WS;

bool ws_up() {
    static int state = 0;   // 0 untried, 1 up, -1 refused
    if (state) return state > 0;
    state = -1;
    HMODULE m = ::LoadLibraryA("ws2_32.dll");
    if (!m) {
        std::fprintf(stderr, "[hub] ws2_32.dll would not load; adventure "
                     "online is off, the game plays solo\n");
        return false;
    }
    bool ok = true;
#define HUB_WS(n) \
    *(FARPROC *)&WS.n = ::GetProcAddress(m, #n); if (!WS.n) ok = false;
    HUB_WS(WSAStartup) HUB_WS(socket) HUB_WS(WSAIoctl) HUB_WS(ioctlsocket)
    HUB_WS(setsockopt) HUB_WS(bind) HUB_WS(sendto) HUB_WS(recvfrom)
    HUB_WS(select) HUB_WS(closesocket) HUB_WS(WSAGetLastError)
    HUB_WS(inet_addr) HUB_WS(gethostbyname)
#undef HUB_WS
    if (!ok) {
        std::fprintf(stderr, "[hub] ws2_32.dll is missing an entry point; "
                     "adventure online is off\n");
        return false;
    }
    WSADATA wsa;
    if (WS.WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::fprintf(stderr, "[hub] WSAStartup failed; adventure online is "
                     "off\n");
        return false;
    }
    state = 1;   // never WSACleanup'd: process exit does it, and the VS
                 // carrier may share Winsock in the same process
    return true;
}

inline unsigned short hton16(unsigned short v) {
    return (unsigned short)((v >> 8) | (v << 8));
}

// Milliseconds from the performance counter: GetTickCount steps in ~16 ms,
// which is a whole ghost frame and too coarse for an RTT readout.
unsigned now_ms() {
    static LARGE_INTEGER f = {};
    if (!f.QuadPart) ::QueryPerformanceFrequency(&f);
    LARGE_INTEGER c;
    ::QueryPerformanceCounter(&c);
    return (unsigned)((unsigned long long)c.QuadPart * 1000ull /
                      (unsigned long long)f.QuadPart);
}

int env_int(const char *name, int def, int lo, int hi) {
    const char *s = std::getenv(name);
    if (!s || !*s) return def;
    char *end = nullptr;
    const long v = std::strtol(s, &end, 10);
    if (end == s || v < lo || v > hi) {
        std::fprintf(stderr, "[hub] %s=%s is out of range %d..%d; using %d\n",
                     name, s, lo, hi, def);
        return def;
    }
    return (int)v;
}

bool parse_host_port(const char *s, int default_port, sockaddr_in *out) {
    char host[256];
    int port = default_port;
    const char *colon = std::strrchr(s, ':');
    const size_t hlen = colon ? (size_t)(colon - s) : std::strlen(s);
    if (hlen == 0 || hlen >= sizeof host) return false;
    std::memcpy(host, s, hlen);
    host[hlen] = 0;
    if (colon) {
        port = std::atoi(colon + 1);
        if (port <= 0 || port > 65535) return false;
    }
    std::memset(out, 0, sizeof *out);
    out->sin_family = AF_INET;
    out->sin_port = hton16((unsigned short)port);
    const unsigned long v = WS.inet_addr(host);
    if (v != INADDR_NONE) { out->sin_addr.s_addr = v; return true; }
    struct hostent *he = WS.gethostbyname(host);
    if (he && he->h_addrtype == AF_INET && he->h_addr_list &&
        he->h_addr_list[0]) {
        std::memcpy(&out->sin_addr.s_addr, he->h_addr_list[0], 4);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// THE KIND TABLE. One validator for both directions, so the hub can never
// send what it would refuse to receive (or what relay v2 would drop).
// ---------------------------------------------------------------------------
enum : int { kKindPing = kHubKinds, kKindPong = kHubKinds + 1,
             kKindNone = -1 };

enum Why : int { kOk = 0, kBadTag, kBadLen, kBadVersion };

Why classify(const unsigned char *p, int len, int *kind) {
    *kind = kKindNone;
    if (len < kHubEnvelopeBytes) return kBadTag;
    unsigned tag;
    std::memcpy(&tag, p, 4);
    switch (tag) {
    case kHubTagGhost:
        *kind = kHubGhost;
        if (len != (int)sizeof(HubGhostFrame)) return kBadLen;
        if (p[4] != 1) return kBadVersion;
        return kOk;
    case kHubTagIdent: {
        *kind = kHubIdent;
        if (len < 24 || len > 64 || (len & 3)) return kBadLen;
        if (p[4] != 1) return kBadVersion;
        const int name_len = p[6];
        if (name_len > kHubIdentNameMax ||
            (int)sizeof(HubIdentHead) + name_len > len) return kBadLen;
        return kOk;
    }
    case kHubTagChat: {
        *kind = kHubChat;
        if (len < 12 || len > 12 + kHubChatTextMax) return kBadLen;
        if (p[4] != 1) return kBadVersion;
        const int text_len = p[7];
        if (text_len < 1 || text_len > kHubChatTextMax ||
            (int)sizeof(HubChatHead) + text_len != len) return kBadLen;
        return kOk;
    }
    case kHubTagVoice:
        *kind = kHubVoice;
        if (len != 176 && len != 340) return kBadLen;
        if (p[4] != 1) return kBadVersion;
        return kOk;
    case kHubTagPing:
        *kind = kKindPing;
        if (len != 12) return kBadLen;
        if (p[4] != 0) return kBadVersion;   // SyncPingV1.kind = ping
        return kOk;
    case kHubTagPong:
        *kind = kKindPong;
        if (len != 12) return kBadLen;
        if (p[4] != 1) return kBadVersion;   // SyncPingV1.kind = pong
        return kOk;
    default:
        return kBadTag;
    }
}

// ---------------------------------------------------------------------------
// STATE. Everything below g_lock is guarded by it.
// ---------------------------------------------------------------------------
enum : int { kModeNone = 0, kModeRelay = 1, kModeMesh = 2 };

const int kFifoDepth[kHubKinds] = { 64, 32, 32, 32 };
enum : int { kFifoMaxDepth = 64 };

struct Msg {
    unsigned t;        // arrival ms
    int len;
    int seat;
    unsigned char b[kHubMaxDatagram];
};
struct Fifo {
    Msg slot[kFifoMaxDepth];
    int head, count;
};
struct Seat {
    bool present;
    unsigned join_epoch;
    unsigned heard;               // ms of the newest valid datagram
    unsigned ghost_t;
    bool has_ghost;
    unsigned short ghost_seq;
    int rtt_ms;
    int latest_len[kHubKinds];
    unsigned latest_t[kHubKinds];
    unsigned char latest[kHubKinds][kHubMaxDatagram];
};

SRWLOCK g_lock = SRWLOCK_INIT;
bool g_installed = false;
int g_mode = kModeNone;
int g_state = kHubOff;
int g_seat = -1;
SOCKET g_sock = INVALID_SOCKET;
sockaddr_in g_relay = {};
int g_mesh_base = 0;
unsigned char g_code[8] = {};
int g_want_seat = 0xFF;
bool g_ever_paired = false;
bool g_exit_full = false;   // set under the lock, acted on outside it
volatile LONG g_stop = 0;   // process exit: the I/O thread returns
HANDLE g_thread = nullptr;
unsigned g_last_ack = 0;
unsigned g_roster_version = 0;
int g_ping_rr = 0;          // the seat the last ping asked to answer
Fifo g_fifo[kHubKinds];
Seat g_seats[kHubMaxSeats];
CommsHubStats g_st = {};
unsigned long long g_drop_stale = 0;

// test knobs
int g_delay_ms = 0, g_loss_pct = 0, g_report_s = 10;
unsigned g_rng = 0x2545F491u;

struct Held { unsigned due; int len; sockaddr_in from; unsigned char b[kHubMaxDatagram]; };
enum : int { kHeldRing = 512 };
Held g_held[kHeldRing];   // I/O thread only
int g_held_head = 0, g_held_count = 0;

struct Lock {
    Lock() { ::AcquireSRWLockExclusive(&g_lock); }
    ~Lock() { ::ReleaseSRWLockExclusive(&g_lock); }
};

const char *state_name(int s) {
    switch (s) {
    case kHubOff: return "off";
    case kHubPairing: return "pairing";
    case kHubLive: return "live";
    case kHubRelayTooOld: return "relay-too-old";
    case kHubRefused: return "refused";
    case kHubFull: return "full";
    case kHubFailed: return "failed";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// SENDING (any thread). Destinations are copied under the lock; the sendto
// calls happen outside it.
// ---------------------------------------------------------------------------
void raw_send_to(const unsigned char *p, int len, const sockaddr_in &to) {
    WS.sendto(g_sock, (const char *)p, len, 0, (const sockaddr *)&to,
              (int)sizeof to);
}

sockaddr_in mesh_addr(int seat) {
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_port = hton16((unsigned short)(g_mesh_base + seat));
    a.sin_addr.s_addr = 0x0100007Fu;   // 127.0.0.1, network order
    return a;
}

// to_seat < 0: everyone else. Caller holds no lock.
void send_out(const unsigned char *p, int len, int to_seat, int mode, int me,
              const sockaddr_in &relay) {
    if (mode == kModeRelay) {
        raw_send_to(p, len, relay);   // the relay routes (pong: byte 6)
        return;
    }
    if (to_seat >= 0) { raw_send_to(p, len, mesh_addr(to_seat)); return; }
    for (int k = 0; k < kHubMaxSeats; ++k)
        if (k != me) raw_send_to(p, len, mesh_addr(k));
}

void send_hello() {
    unsigned char h[16] = { 'S', 'M', 'R', 'C', 2, 2, 0, 0 };
    h[6] = (unsigned char)g_want_seat;
    h[7] = host_setting_voice_enabled() ? 1 : 0;
    std::memcpy(h + 8, g_code, 8);
    raw_send_to(h, 16, g_relay);
    ++g_st.hellos;
}

// THE PING ASKS ONE SEAT TO ANSWER, in rotation (byte 6; 0xFF = nobody).
// Every ping still reaches everyone (it is the presence heartbeat), but only
// the named seat echoes it. An echo-from-everyone ping is fine at 2 players
// and breaks at 16: fifteen peers pinging at 2 Hz would make every seat send
// 30 pongs a second, five times relay v2's 6-per-second pong cap, so most RTT
// samples would die at the relay. Rotating keeps each seat near 2 pongs a
// second at any room size; a seat's RTT refreshes every (peers / 2) seconds.
void send_ping(int me, int mode, const sockaddr_in &relay, int ask) {
    unsigned char p[12] = { 'S', 'Y', 'N', 'P', 0, 0, 0xFF, 0 };
    p[5] = (unsigned char)me;
    p[6] = (unsigned char)(ask >= 0 ? ask : 0xFF);
    const unsigned t = now_ms();
    std::memcpy(p + 8, &t, 4);
    send_out(p, 12, -1, mode, me, relay);
}

// ---------------------------------------------------------------------------
// RECEIVING (I/O thread)
// ---------------------------------------------------------------------------
void seat_join(int s, unsigned now) {
    Seat &z = g_seats[s];
    z.present = true;
    ++z.join_epoch;
    z.heard = now;
    z.has_ghost = false;
    z.rtt_ms = -1;
    for (int k = 0; k < kHubKinds; ++k) z.latest_len[k] = 0;
    ++g_roster_version;
    ++g_st.joins;
    std::fprintf(stderr, "[hub] seat %d joined (epoch %u, roster %u)\n", s,
                 z.join_epoch, g_roster_version);
}

void seat_leave(int s, const char *why) {
    Seat &z = g_seats[s];
    z.present = false;
    z.has_ghost = false;
    for (int k = 0; k < kHubKinds; ++k) z.latest_len[k] = 0;
    ++g_roster_version;
    ++g_st.leaves;
    std::fprintf(stderr, "[hub] seat %d left (%s, roster %u)\n", s, why,
                 g_roster_version);
}

void fifo_push(int kind, const unsigned char *p, int len, int seat,
               unsigned now) {
    Fifo &f = g_fifo[kind];
    const int depth = kFifoDepth[kind];
    if (f.count == depth) {            // newest wins: drop the oldest
        f.head = (f.head + 1) % depth;
        --f.count;
        ++g_st.drop_queue;
    }
    Msg &m = f.slot[(f.head + f.count) % depth];
    m.t = now;
    m.len = len;
    m.seat = seat;
    std::memcpy(m.b, p, (size_t)len);
    ++f.count;
}

// A datagram that passed the source check; `seat` is the stamped sender.
void accept_datagram(unsigned char *p, int len, int seat, unsigned now,
                     int *pong_to, unsigned char *pong) {
    int kind;
    const Why w = classify(p, len, &kind);
    if (w == kBadTag) { ++g_st.drop_tag; return; }
    if (w == kBadLen) { ++g_st.drop_len; return; }
    if (w == kBadVersion) { ++g_st.drop_version; return; }
    if (seat < 0 || seat >= kHubMaxSeats) { ++g_st.drop_seat; return; }
    if (seat == g_seat) { ++g_st.drop_self; return; }
    p[5] = (unsigned char)seat;   // the carrier's stamp is the only truth

    Seat &z = g_seats[seat];
    if (!z.present) seat_join(seat, now);
    z.heard = now;

    if (kind == kKindPing) {
        if (p[6] != (unsigned char)g_seat) return;   // not asked this time
        // Echo to the pinger only: 'SYNQ', kind 1, from me, to it, its time.
        std::memcpy(pong, p, 12);
        pong[3] = 'Q';
        pong[4] = 1;
        pong[5] = (unsigned char)g_seat;
        pong[6] = (unsigned char)seat;
        *pong_to = seat;
        return;
    }
    if (kind == kKindPong) {
        if (p[6] != (unsigned char)g_seat) { ++g_st.drop_seat; return; }
        unsigned t;
        std::memcpy(&t, p + 8, 4);
        const unsigned rtt = now - t;
        if (rtt < 60000u) z.rtt_ms = (int)rtt;
        ++g_st.pongs_recvd;
        return;
    }
    if (kind == kHubGhost) {
        unsigned short seq;
        std::memcpy(&seq, p + 8, 2);
        // Late or duplicate: newest wins. Only within 16 frames (about 1 s at
        // 15 Hz, far past any real reordering), though: a console that
        // restarts and lands on the same seat before its old presence lapses
        // starts again from seq 0, and a plain wrap compare would hold every
        // frame it sends as "stale" until it passed the old count (measured:
        // a restart 4 s in lost its first 52 frames with a 64-frame window).
        // A jump back past the window is a new sender.
        const short d = (short)(seq - z.ghost_seq);
        if (z.has_ghost && d <= 0 && d > -16) {
            ++g_drop_stale;
            return;
        }
        z.has_ghost = true;
        z.ghost_seq = seq;
        z.ghost_t = now;
    }
    std::memcpy(z.latest[kind], p, (size_t)len);
    z.latest_len[kind] = len;
    z.latest_t[kind] = now;
    ++g_st.recvd[kind];
    fifo_push(kind, p, len, seat, now);
}

// Returns the stamped seat, or -1 when the source is not one we expect.
int source_seat(const unsigned char *p, int len, const sockaddr_in &from) {
    if (g_mode == kModeRelay) {
        if (from.sin_addr.s_addr != g_relay.sin_addr.s_addr ||
            from.sin_port != g_relay.sin_port) return -1;
        return len > 5 ? p[5] : -1;   // relay v2 stamped it
    }
    if (from.sin_addr.s_addr != 0x0100007Fu) return -1;
    const int port = hton16(from.sin_port);
    const int s = port - g_mesh_base;
    return (s >= 0 && s < kHubMaxSeats) ? s : -1;
}

void on_ack(const unsigned char *p, unsigned now) {
    ++g_st.acks;
    if (std::memcmp(p + 8, g_code, 8) != 0) return;   // not our code
    const int ver = p[4], status = p[5];
    if (ver != 2) {
        if (g_state != kHubRelayTooOld)
            std::fprintf(stderr, "[hub] the relay answered as version %d: "
                         "adventure online needs relay v2. Playing solo; the "
                         "hub keeps asking once a second.\n", ver);
        g_state = kHubRelayTooOld;
        return;
    }
    switch (status) {
    case 0: {
        const int seat = p[6];
        if (seat >= kHubMaxSeats) { ++g_st.drop_seat; return; }
        if (g_state != kHubLive || seat != g_seat)
            std::fprintf(stderr, "[hub] paired: relay v2 hub, seat %d%s\n",
                         seat, g_ever_paired ? " (re-paired)" : "");
        if (g_seat >= 0 && seat != g_seat && g_seats[seat].present)
            seat_leave(seat, "that seat is mine now");
        g_seat = seat;
        g_state = kHubLive;
        g_ever_paired = true;
        g_last_ack = now;
        return;
    }
    case 1:
        if (!g_ever_paired) {
            g_exit_full = true;
            return;
        }
        if (g_state != kHubFull)
            std::fprintf(stderr, "[hub] the room filled while re-pairing; "
                         "playing solo and asking again once a second\n");
        g_state = kHubFull;
        return;
    case 4:
        std::fprintf(stderr, "[hub] seat %d is taken; asking for any free "
                     "seat\n", g_want_seat);
        g_want_seat = 0xFF;
        return;
    default:
        if (g_state != kHubRefused)
            std::fprintf(stderr, "[hub] the relay refused the hub HELLO "
                         "(status %d: %s). Playing solo.\n", status,
                         status == 3 ? "this code is a v1 session"
                         : status == 5 ? "this code is a VS session"
                         : "bad request");
        g_state = kHubRefused;
        return;
    }
}

void presence_sweep(unsigned now) {
    for (int s = 0; s < kHubMaxSeats; ++s)
        if (g_seats[s].present && now - g_seats[s].heard > kHubPresenceMs)
            seat_leave(s, "unheard for 3 s");
}

void report_locked(const char *tag) {
    const unsigned now = now_ms();
    unsigned mask = 0;
    int n = 0;
    for (int s = 0; s < kHubMaxSeats; ++s)
        if (g_seats[s].present) { mask |= 1u << s; ++n; }
    char rtt[16 * 8] = "";
    int o = 0;
    for (int s = 0; s < kHubMaxSeats && o < (int)sizeof rtt - 8; ++s)
        if (g_seats[s].present)
            o += std::snprintf(rtt + o, sizeof rtt - (size_t)o, "%s%d:%d",
                               o ? "," : "", s, g_seats[s].rtt_ms);
    std::fprintf(stderr,
                 "[hub] %s t=%u mode=%s state=%s seat=%d mask=0x%04x peers=%d "
                 "sent g/i/t/v=%llu/%llu/%llu/%llu recvd=%llu/%llu/%llu/%llu "
                 "ping=%llu pong=%llu/%llu rtt=[%s] refused=%llu "
                 "drop tag/len/ver/src/self/seat/queue/cap/stale/test="
                 "%llu/%llu/%llu/%llu/%llu/%llu/%llu/%llu/%llu/%llu "
                 "joins=%llu leaves=%llu hello=%llu ack=%llu\n",
                 tag, now,
                 g_mode == kModeRelay ? "relay" : g_mode == kModeMesh ? "mesh"
                                                                      : "none",
                 state_name(g_state), g_seat, mask, n,
                 g_st.sent[0], g_st.sent[1], g_st.sent[2], g_st.sent[3],
                 g_st.recvd[0], g_st.recvd[1], g_st.recvd[2], g_st.recvd[3],
                 g_st.pings_sent, g_st.pongs_sent, g_st.pongs_recvd, rtt,
                 g_st.send_refused, g_st.drop_tag, g_st.drop_len,
                 g_st.drop_version, g_st.drop_source, g_st.drop_self,
                 g_st.drop_seat, g_st.drop_queue, g_st.drop_cap, g_drop_stale,
                 g_st.drop_test, g_st.joins, g_st.leaves, g_st.hellos,
                 g_st.acks);
}

// One datagram off the wire (or out of the test hold ring).
void process(unsigned char *p, int len, const sockaddr_in &from,
             unsigned now) {
    int pong_to = -1;
    unsigned char pong[12];
    int mode = kModeNone, me = -1;
    sockaddr_in relay = {};
    {
        Lock l;
        if (g_mode == kModeRelay && len == 16 && std::memcmp(p, "SMRA", 4) == 0
            && from.sin_addr.s_addr == g_relay.sin_addr.s_addr
            && from.sin_port == g_relay.sin_port) {
            on_ack(p, now);
            if (!g_exit_full) return;
        } else {
            const int seat = source_seat(p, len, from);
            if (seat < 0) { ++g_st.drop_source; return; }
            if (g_state != kHubLive) { ++g_st.drop_source; return; }
            accept_datagram(p, len, seat, now, &pong_to, pong);
            mode = g_mode; me = g_seat; relay = g_relay;
            if (pong_to >= 0) ++g_st.pongs_sent;
        }
    }
    if (g_exit_full) {
        // THE FIRST PAIRING met a full room (design.txt section 5): exit 4 so
        // the launcher asks the lobby for another world. Never after a pairing
        // has succeeded: that player is mid-game and keeps playing solo.
        std::fprintf(stderr, "[hub] the relay says this room is full. Exiting "
                     "with code 4 so the launcher can ask for another "
                     "room.\n");
        std::fflush(stderr);
        std::fflush(stdout);
        ::ExitProcess(4);
    }
    if (pong_to >= 0) send_out(pong, 12, pong_to, mode, me, relay);
}

DWORD WINAPI hub_thread(void *) {
    unsigned last_hello = 0, last_ping = 0, last_report = now_ms();
    bool first_hello = true;
    unsigned char buf[2048];
    for (;;) {
        if (::InterlockedCompareExchange(&g_stop, 0, 0)) return 0;
        const unsigned now = now_ms();
        int mode, me, state;
        sockaddr_in relay;
        {
            Lock l;
            mode = g_mode; me = g_seat; state = g_state; relay = g_relay;
            if (mode == kModeRelay) {
                const unsigned every = (state == kHubRefused) ? 5000u : 1000u;
                if (first_hello || now - last_hello >= every) {
                    send_hello();
                    last_hello = now;
                    first_hello = false;
                }
                if (state == kHubLive && now - g_last_ack > 5000u) {
                    std::fprintf(stderr, "[hub] no ACK from the relay for 5 s;"
                                 " re-pairing\n");
                    g_state = kHubPairing;
                    state = g_state;
                }
            }
            presence_sweep(now);
            if (g_report_s > 0 && now - last_report >= (unsigned)g_report_s * 1000u) {
                report_locked("periodic");
                last_report = now;
            }
        }
        if (state == kHubLive && now - last_ping >= 500u) {
            int ask = -1;
            {
                Lock l;
                for (int i = 1; i <= kHubMaxSeats && ask < 0; ++i) {
                    const int s = (g_ping_rr + i) % kHubMaxSeats;
                    if (g_seats[s].present && s != me) ask = s;
                }
                if (ask >= 0) g_ping_rr = ask;
                ++g_st.pings_sent;
                last_ping = now;
            }
            send_ping(me, mode, relay, ask);
        }

        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(g_sock, &rd);
        timeval tv = { 0, 5000 };   // 5 ms: keeps the delay knob honest
        const int r = WS.select(0, &rd, nullptr, nullptr, &tv);
        if (r > 0) {
            for (;;) {
                sockaddr_in from = {};
                int fl = (int)sizeof from;
                const int n = WS.recvfrom(g_sock, (char *)buf, (int)sizeof buf,
                                          0, (sockaddr *)&from, &fl);
                if (n <= 0) {
                    if (n < 0 && WS.WSAGetLastError() == WSAEMSGSIZE) {
                        Lock l;           // bigger than any kind: refuse it
                        ++g_st.drop_cap;
                        continue;
                    }
                    break;                // WSAEWOULDBLOCK: drained
                }
                const unsigned t = now_ms();
                if (n > kHubMaxDatagram) {
                    Lock l;
                    ++g_st.drop_cap;
                    continue;
                }
                if (g_loss_pct > 0) {
                    g_rng = g_rng * 1664525u + 1013904223u;
                    if ((int)((g_rng >> 8) % 100u) < g_loss_pct) {
                        Lock l;
                        ++g_st.drop_test;
                        continue;
                    }
                }
                if (g_delay_ms > 0) {
                    if (g_held_count == kHeldRing) {
                        Lock l;
                        ++g_st.drop_test;
                        continue;
                    }
                    Held &h = g_held[(g_held_head + g_held_count) % kHeldRing];
                    h.due = t + (unsigned)g_delay_ms;
                    h.len = n;
                    h.from = from;
                    std::memcpy(h.b, buf, (size_t)n);
                    ++g_held_count;
                    continue;
                }
                process(buf, n, from, t);
            }
        }
        while (g_held_count > 0) {
            Held &h = g_held[g_held_head];
            const unsigned t = now_ms();
            if ((int)(t - h.due) < 0) break;
            process(h.b, h.len, h.from, t);
            g_held_head = (g_held_head + 1) % kHeldRing;
            --g_held_count;
        }
    }
}

bool open_socket_mesh() {
    SOCKET s = WS.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;
    BOOL excl = TRUE;   // two consoles can never share a seat's port
    WS.setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&excl,
                  (int)sizeof excl);
    for (int k = 0; k < kHubMaxSeats; ++k) {
        sockaddr_in a = mesh_addr(k);
        if (WS.bind(s, (const sockaddr *)&a, (int)sizeof a) == 0) {
            g_sock = s;
            g_seat = k;
            return true;
        }
    }
    WS.closesocket(s);
    std::fprintf(stderr, "[hub] mesh: every seat port %d..%d is taken\n",
                 g_mesh_base, g_mesh_base + kHubMaxSeats - 1);
    return false;
}

bool open_socket_relay() {
    SOCKET s = WS.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in a = {};
    a.sin_family = AF_INET;   // any address, any port: the NAT picks
    if (WS.bind(s, (const sockaddr *)&a, (int)sizeof a) != 0) {
        WS.closesocket(s);
        return false;
    }
    g_sock = s;
    return true;
}

void finish_socket() {
    u_long nb = 1;
    WS.ioctlsocket(g_sock, FIONBIO, &nb);
    // An ICMP "port unreachable" (a mesh seat nobody holds, a relay restart)
    // must not surface as WSAECONNRESET on the next receive.
    BOOL off = FALSE;
    DWORD got = 0;
    WS.WSAIoctl(g_sock, SIO_UDP_CONNRESET, &off, (DWORD)sizeof off, nullptr,
                0, &got, nullptr, nullptr);
    int big = 256 * 1024;   // 16 seats of voice arrive in bursts
    WS.setsockopt(g_sock, SOL_SOCKET, SO_RCVBUF, (const char *)&big,
                  (int)sizeof big);
}

// AT PROCESS EXIT THE I/O THREAD STOPS FIRST. exit() runs this, then tears
// down stdio, and only then does ExitProcess end the other threads. A hub
// thread still printing "seat k left" into that window read a closed stderr:
// 2 of 16 probes in the first 16-seat mesh rung died 0xC0000005 AFTER their
// last line. So: raise the flag, wait for the thread (it checks every 5 ms,
// bounded at 500 ms), then print the exit line from here.
void report_at_exit() {
    ::InterlockedExchange(&g_stop, 1);
    if (g_thread) ::WaitForSingleObject(g_thread, 500);
    comms_hub_report("exit");
}

}  // namespace

// ===========================================================================
// PUBLIC
// ===========================================================================
bool comms_hub_install_from_env() {
    static bool tried = false;
    static bool owned = false;   // the answer: true = skip the VS carrier
    if (tried) return owned;
    tried = true;

    if (!adventure_ghost_mode()) return false;
    // A VS MATCH IS NEVER THE HUB'S, whatever the adventure flag says. The
    // launcher names the VS map for every VS match and never for adventure,
    // so SM64DS_VS_MAP is the one knob that cannot be a stale setting: a
    // hand-edited settings.json with AdventureGhosts on must not let the hub
    // take a VS match's relay code and keep the VS carrier from installing.
    // Declined silently; the VS carrier logs its own install.
    if (const char *vs = std::getenv("SM64DS_VS_MAP"))
        if (*vs) return false;

    const char *relay = std::getenv("SM64DS_COMMS_RELAY");
    const char *code = std::getenv("SM64DS_COMMS_CODE");
    const char *mesh = std::getenv("SM64DS_HUB_PORT");
    const bool want_relay = relay && *relay;
    const bool want_mesh = !want_relay && mesh && *mesh;
    if (!want_relay && !want_mesh) {
        // ADVENTURE NEVER SEATS A DS ROLE, and that holds on this path too.
        // An adventure console handed the VS carrier's direct env (a role and
        // a port, no relay, no hub port) used to open that carrier and seat
        // roles 1 and 2: the old 0.5.3 adventure transport, joiner without a
        // save included. It plays solo instead, and says why once.
        if (const char *role = std::getenv("SM64DS_COMMS_ROLE")) {
            std::fprintf(stderr, "[hub] adventure mode with SM64DS_COMMS_ROLE="
                         "%s but no hub address (SM64DS_COMMS_RELAY or "
                         "SM64DS_HUB_PORT): the VS carrier is not opened for "
                         "adventure, so this console plays solo\n", role);
            owned = true;
            return true;
        }
        return false;   // adventure, offline: nothing to open, nothing to skip
    }
    owned = true;

    if (const char *role = std::getenv("SM64DS_COMMS_ROLE"))
        std::fprintf(stderr, "[hub] SM64DS_COMMS_ROLE=%s is ignored: an "
                     "adventure console opens the hub, never the VS "
                     "carrier\n", role);

    // FROM HERE ON THE HUB OWNS THE SESSION even if it fails to open: a
    // failed hub must not fall through to the VS carrier and seat a DS role.
    g_installed = true;
    g_state = kHubFailed;
    g_delay_ms = env_int("SM64DS_HUB_DELAY_MS", 0, 0, 2000);
    g_loss_pct = env_int("SM64DS_HUB_LOSS_PCT", 0, 0, 90);
    g_report_s = env_int("SM64DS_HUB_REPORT_S", 10, 0, 3600);
    g_rng ^= (unsigned)::GetCurrentProcessId() * 2654435761u;

    if (!ws_up()) return true;

    if (want_relay) {
        g_mode = kModeRelay;
        const size_t cl = code ? std::strlen(code) : 0;
        bool code_ok = cl >= 1 && cl <= 8;
        for (size_t i = 0; code_ok && i < cl; ++i)
            if ((unsigned char)code[i] < 0x21 || (unsigned char)code[i] > 0x7E)
                code_ok = false;
        if (!code_ok) {
            std::fprintf(stderr, "[hub] SM64DS_COMMS_CODE must be 1..8 "
                         "printable ASCII characters; adventure online is "
                         "off\n");
            return true;
        }
        std::memcpy(g_code, code, cl);
        if (!parse_host_port(relay, 41234, &g_relay)) {
            std::fprintf(stderr, "[hub] could not resolve the relay '%s'; "
                         "adventure online is off\n", relay);
            return true;
        }
        g_want_seat = env_int("SM64DS_HUB_SEAT", 0xFF, 0, 0xFF);
        if (g_want_seat >= kHubMaxSeats) g_want_seat = 0xFF;
        if (!open_socket_relay()) {
            std::fprintf(stderr, "[hub] no UDP socket; adventure online is "
                         "off\n");
            return true;
        }
        g_state = kHubPairing;
    } else {
        g_mode = kModeMesh;
        g_mesh_base = env_int("SM64DS_HUB_PORT", 0, 1024, 65535 - kHubMaxSeats);
        if (!g_mesh_base) return true;
        if (g_mesh_base <= 51765 && 51765 < g_mesh_base + kHubMaxSeats)
            std::fprintf(stderr, "[hub] WARNING: mesh ports %d..%d include "
                         "51765\n", g_mesh_base,
                         g_mesh_base + kHubMaxSeats - 1);
        if (!open_socket_mesh()) return true;
        g_state = kHubLive;
    }
    finish_socket();

    HANDLE th = ::CreateThread(nullptr, 64 * 1024, hub_thread, nullptr, 0,
                               nullptr);
    if (!th) {
        std::fprintf(stderr, "[hub] could not start the I/O thread; "
                     "adventure online is off\n");
        WS.closesocket(g_sock);
        g_sock = INVALID_SOCKET;
        g_state = kHubFailed;
        return true;
    }
    g_thread = th;   // kept open: report_at_exit waits on it
    std::atexit(report_at_exit);
    if (g_mode == kModeRelay) {
        char cs[9] = {};
        std::memcpy(cs, g_code, 8);
        std::fprintf(stderr, "[hub] installed: adventure hub through the "
                     "relay %s, code %s, want seat %d. No DS session is "
                     "seated; this console plays its own game.\n", relay, cs,
                     g_want_seat);
    } else {
        std::fprintf(stderr, "[hub] installed: adventure hub, loopback mesh "
                     "base %d, seat %d. No DS session is seated; this console "
                     "plays its own game.\n", g_mesh_base, g_seat);
    }
    return true;
}

bool comms_hub_active() {
    Lock l;
    return g_installed && g_sock != INVALID_SOCKET;
}

int comms_hub_state() {
    Lock l;
    return g_installed ? g_state : kHubOff;
}

int comms_hub_my_seat() {
    Lock l;
    return (g_state == kHubLive) ? g_seat : -1;
}

bool comms_hub_send(const void *datagram, int len) {
    if (!datagram || len <= 0 || len > kHubMaxDatagram) {
        Lock l;
        ++g_st.send_refused;
        return false;
    }
    unsigned char p[kHubMaxDatagram];
    std::memcpy(p, datagram, (size_t)len);
    int kind;
    int mode, me;
    sockaddr_in relay;
    {
        Lock l;
        if (!g_installed || g_state != kHubLive ||
            classify(p, len, &kind) != kOk || kind >= kHubKinds) {
            ++g_st.send_refused;
            return false;
        }
        p[5] = (unsigned char)g_seat;
        mode = g_mode; me = g_seat; relay = g_relay;
        ++g_st.sent[kind];
    }
    send_out(p, len, -1, mode, me, relay);
    return true;
}

int comms_hub_recv(HubKind kind, void *buf, int cap, int *seat) {
    if (kind < 0 || kind >= kHubKinds || !buf) return 0;
    Lock l;
    Fifo &f = g_fifo[kind];
    const int depth = kFifoDepth[kind];
    while (f.count > 0) {
        Msg &m = f.slot[f.head];
        f.head = (f.head + 1) % depth;
        --f.count;
        if (m.len > cap) { ++g_st.drop_cap; continue; }
        std::memcpy(buf, m.b, (size_t)m.len);
        if (seat) *seat = m.seat;
        return m.len;
    }
    return 0;
}

int comms_hub_latest(HubKind kind, int seat, void *buf, int cap,
                     unsigned *age_ms) {
    if (kind < 0 || kind >= kHubKinds || seat < 0 || seat >= kHubMaxSeats ||
        !buf) return 0;
    Lock l;
    const Seat &z = g_seats[seat];
    const int len = z.latest_len[kind];
    if (!z.present || len <= 0 || len > cap) return 0;
    std::memcpy(buf, z.latest[kind], (size_t)len);
    if (age_ms) *age_ms = now_ms() - z.latest_t[kind];
    return len;
}

bool comms_hub_seat_info(int seat, HubSeatInfo *out) {
    if (!out || seat < 0 || seat >= kHubMaxSeats) return false;
    Lock l;
    if (!g_installed) return false;
    const Seat &z = g_seats[seat];
    const unsigned now = now_ms();
    std::memset(out, 0, sizeof *out);
    out->present = z.present;
    out->join_epoch = z.join_epoch;
    out->heard_ms_ago = z.present ? now - z.heard : ~0u;
    out->rtt_ms = z.present ? z.rtt_ms : -1;
    out->has_ghost = z.present && z.has_ghost;
    out->ghost_ms_ago = out->has_ghost ? now - z.ghost_t : ~0u;
    if (out->has_ghost) {
        HubGhostFrame g;
        std::memcpy(&g, z.latest[kHubGhost], sizeof g);
        out->flags = g.flags;
        out->level_id = g.level_id;
        out->area = g.area;
        out->x = g.x; out->y = g.y; out->z = g.z;
    }
    return true;
}

unsigned comms_hub_seat_mask() {
    Lock l;
    unsigned m = 0;
    for (int s = 0; s < kHubMaxSeats; ++s)
        if (g_seats[s].present) m |= 1u << s;
    return m;
}

unsigned comms_hub_roster_version() {
    Lock l;
    return g_roster_version;
}

CommsHubStats comms_hub_stats() {
    Lock l;
    CommsHubStats s = g_st;
    s.installed = g_installed;
    s.mode = g_mode;
    s.state = g_installed ? g_state : kHubOff;
    s.seat = g_seat;
    s.seat_mask = 0;
    for (int k = 0; k < kHubMaxSeats; ++k)
        if (g_seats[k].present) s.seat_mask |= 1u << k;
    s.drop_stale = g_drop_stale;
    return s;
}

void comms_hub_report(const char *tag) {
    Lock l;
    if (!g_installed) return;
    report_locked(tag ? tag : "report");
}

}  // namespace port
