// THE ADVENTURE HUB CARRIER. Run mp1, lane HUB1.
//
// The transport for adventure online ("ghosts"): every console plays its OWN
// solo game from its own save, and the wire carries only presentation -- where
// my body is and what it looks like, who I am, what I said, my voice. There
// are NO ROUNDS here: no input exchange, no lockstep, nobody waits for anybody,
// people join and leave whenever they like, and a frozen console freezes only
// its own ghost on everyone else's screen.
//
// WHAT THIS IS NOT. It is not a DS transport. It never calls
// comms_set_transport(), never seats the DS multiplayer role (data_020a0f04),
// never arms the "open the radio" one-shot, and the conductor never waits on
// it. As far as the ROM can tell the game is single-player, which is exactly
// what an adventure console is. comms_loopback.cpp stays the VS carrier.
//
// PORT_HOST_ABI: host plumbing below the game, like comms_loopback.cpp. Nothing
// in src/ knows it exists. ws2_32 is LoadLibrary'd on install (the rule in
// comms_loopback.cpp's banner: a static ws2_32 import loses the DS fixed
// ranges), so a solo run never loads a socket library.
//
// ---------------------------------------------------------------------------
// SELECTED ONLY BY ENV. comms_hub_install_from_env() installs the hub when the
// adventure mode is on (SM64DS_ADVENTURE, or the AdventureGhosts setting) AND
// one of the two address modes below is named. Otherwise it installs nothing
// and returns false, and the caller goes on to comms_loopback_install_from_env()
// exactly as before (VS, and every solo path, unchanged). A VS match
// (SM64DS_VS_MAP set) is never the hub's, even with the adventure flag on, and
// an adventure console is never handed to the VS carrier; see
// comms_hub_install_from_env below.
//
//   RELAY MODE (what the launcher sets; design.txt section 3, relay v2):
//     SM64DS_COMMS_RELAY=<host[:port]>  the relay, default port 41234
//     SM64DS_COMMS_CODE=<1..8 ascii>    the room's hub code (lobby-minted)
//     SM64DS_HUB_SEAT=<0..15>           OPTIONAL pin; default 0xFF = any free
//                                       seat (the lobby plan's want_seat 255)
//   The hub sends HELLO v2 role 2 (member) and re-sends it every 1 s as the
//   keepalive; the ACK v2 names the seat the relay will stamp on everything
//   this console sends. Received datagrams carry the SENDER's seat at byte 5,
//   written by the relay; a payload can never claim to be another seat.
//
//   LOOPBACK MESH (tests only; no relay needed):
//     SM64DS_HUB_PORT=<n>   seat k binds 127.0.0.1:<n>+k, first free k in
//                           0..15; every datagram goes to the other 15 ports.
//                           The receiver stamps byte 5 from the SOURCE PORT.
//                           NO DEFAULT on purpose (derive it from your PID in a
//                           harness; never 51765, the live desk pair's port).
//
//   With the hub installed, SM64DS_COMMS_ROLE is IGNORED (logged): an
//   adventure console must never also open the VS carrier.
//
//   Test knobs (all off by default):
//     SM64DS_HUB_DELAY_MS=<n>   hold every received datagram n ms (one way)
//     SM64DS_HUB_LOSS_PCT=<n>   drop n% of received datagrams (0..90)
//     SM64DS_HUB_REPORT_S=<n>   print the report line every n s (default 10;
//                               0 = only at exit)
//
// Exit code 4: the relay answered the FIRST pairing with status 1 (room full).
// The launcher then asks the lobby for another world (design.txt section 5).
// A "full" on a later re-pair (relay restart mid-game) never exits: the player
// keeps playing solo and the report line says so.
//
// ---------------------------------------------------------------------------
// THE WIRE (design.txt section 6). Every datagram starts with the 8-byte
// envelope:  0 tag[4] | 4 version u8 | 5 seat u8 | 6 u8 | 7 u8   (little endian)
// Byte 5 is ALWAYS overwritten by the carrier/relay; senders may leave it 0xFF.
// comms_hub_send() refuses anything that is not in this table, by tag and
// exact/min/max length, so a caller can never send what the relay would drop:
//
//   kind   tag     length        version  queue at the receiver
//   ghost  'SYNG'  44            1        FIFO 64 + newest-by-seq per seat
//   ident  'SYNI'  24..64, %4    1        FIFO 32 + newest per seat
//          (length = max(24, 20 + name_len rounded up to 4); 20+name_len <= len)
//   chat   'SYNT'  12..132       1        FIFO 32 (C1 dedupes on seat,msg_id)
//   voice  'SYNV'  176 or 340    1        FIFO 32 (hal/voice_chat.h format)
//   ping   'SYNP'  12            -        hub-internal (RTT per seat, 2 Hz)
//   pong   'SYNQ'  12            -        hub-internal (byte 6 = pinger seat)
// Anything else received is dropped and counted by reason.
//
// THE ARENA SHAPE (run mp1, lane ARENACORE1; hal/vs_arena.h). A VS console in
// arena mode opens this same carrier through comms_hub_install_arena(): HELLO
// v2 role 3 (arena member), the relay's "arena" session, the REFEREE in the
// relay at seat mark 0xFE (port/tools/relay/arena_wire.h, lane REFEREE1).
// The kinds then differ by shape: 'SYNG' is adventure only, and these four
// plus two referee-clock sizes are arena only (everything else as above):
//   state  'SYNS'  48            1        FIFO 64 + newest per seat (A0)
//   object 'SYNO'  32            1        FIFO 64 (a star in flight, A3)
//   ref    'SYNR'  272           1        FIFO 32; ONLY from the relay with
//                                         byte 5 == 0xFE (the referee)
//   claim  'SYNE'  24            1        send only (to the referee)
//   rping  'SYNP'  20            2        hub-internal, byte 6 = 0xFE, 2 Hz
//   rpong  'SYNQ'  20            2        hub-internal, ONLY from 0xFE: the
//                                         referee clock (comms_hub_referee_clock)

#ifndef PORT_HAL_COMMS_HUB_H
#define PORT_HAL_COMMS_HUB_H

#include <cstddef>

namespace port {

enum : int {
    kHubMaxSeats     = 16,
    kHubEnvelopeBytes = 8,
    kHubMaxDatagram  = 340,     // the largest kind (voice)
    kHubPresenceMs   = 3000,    // a seat unheard this long has left
};

// The four kinds a game-side layer sends and receives. Pings are the hub's own.
enum HubKind : int {
    kHubGhost = 0,   // 'SYNG'  W2 GHOSTWIRE1
    kHubIdent = 1,   // 'SYNI'  W2 GHOSTWIRE1
    kHubChat  = 2,   // 'SYNT'  C1 CHATUI1
    kHubVoice = 3,   // 'SYNV'  V1 VOICE1
    // ARENA ONLY (lane ARENACORE1; hal/vs_arena.h)
    kHubArenaState  = 4,   // 'SYNS'  A0 vs_arena.cpp (the only consumer)
    kHubArenaObject = 5,   // 'SYNO'  A3 vs_arena_ref.cpp
    kHubArenaRef    = 6,   // 'SYNR'  A3 vs_arena_ref.cpp (seat 0xFE)
    kHubArenaClaim  = 7,   // 'SYNE'  A3 sends; never received
    kHubKinds = 8,
};
enum : int { kHubRefereeSeat = 0xFE };   // byte 5 of what the referee sends

// The tags as little-endian words (the first four bytes read as a u32).
enum : unsigned {
    kHubTagGhost = 0x474e5953u,   // 'S','Y','N','G'
    kHubTagIdent = 0x494e5953u,   // 'S','Y','N','I'
    kHubTagChat  = 0x544e5953u,   // 'S','Y','N','T'
    kHubTagVoice = 0x564e5953u,   // 'S','Y','N','V'
    kHubTagPing  = 0x504e5953u,   // 'S','Y','N','P'
    kHubTagPong  = 0x514e5953u,   // 'S','Y','N','Q'
    kHubTagArenaState  = 0x534e5953u,   // 'S','Y','N','S'
    kHubTagArenaObject = 0x4f4e5953u,   // 'S','Y','N','O'
    kHubTagArenaRef    = 0x524e5953u,   // 'S','Y','N','R'
    kHubTagArenaClaim  = 0x454e5953u,   // 'S','Y','N','E'
};

// ---------------------------------------------------------------------------
// THE LAYOUTS, one definition for every lane (design.txt 6.1 - 6.3).
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct HubEnvelope {
    unsigned      tag;
    unsigned char version;
    unsigned char seat;        // carrier/relay-stamped sender seat
    unsigned char b6;
    unsigned char b7;
};

// 'SYNG' ghost frame v1, 44 bytes. 15 Hz while playing, 2 Hz with kGhostPaused.
enum : unsigned char {
    kGhostLive      = 0x01,
    kGhostGrounded  = 0x02,
    kGhostTeleport  = 0x04,    // snap, do not interpolate
    kGhostPaused    = 0x08,    // paused / in a menu
    kGhostTalking   = 0x10,    // mic gate open (V1 exports it)
};
struct HubGhostFrame {
    unsigned       tag;         // kHubTagGhost
    unsigned char  version;     // 1
    unsigned char  seat;
    unsigned char  flags;       // kGhost*
    unsigned char  character;   // 0 Mario, 1 Luigi, 2 Wario, 3 Yoshi
    unsigned short seq;         // wrap-compared, newest wins
    unsigned short level_id;    // data_0209f2f8
    int            x, y, z;     // Fix12
    short          vx, vy, vz;  // per-frame delta, clamped; 0 with teleport
    short          yaw;
    unsigned short anim_id;
    int            anim_frame;  // 20.12
    unsigned short stars;
    unsigned char  area;
    unsigned char  reserved[3]; // 0
};

// 'SYNI' identity v1, 20-byte header + name_len bytes of UTF-8, zero padded
// to a multiple of 4 and to at least 24 bytes (so a 0..4 byte name is 24). Sent on join, every 2 s, and when a new seat appears.
struct HubIdentHead {
    unsigned       tag;         // kHubTagIdent
    unsigned char  version;     // 1
    unsigned char  seat;
    unsigned char  name_len;    // 0..24
    unsigned char  character;
    unsigned       build_id;
    unsigned char  body_rgb[3];
    unsigned char  shoes_rgb[3];
    unsigned char  flags;
    unsigned char  reserved;
    // unsigned char name[name_len]; then zero pad to a multiple of 4
};

// 'SYNT' chat v1, 12-byte header + text_len bytes of UTF-8 (1..120).
struct HubChatHead {
    unsigned       tag;         // kHubTagChat
    unsigned char  version;     // 1
    unsigned char  seat;
    unsigned char  flags;       // 0
    unsigned char  text_len;    // 1..120
    unsigned short msg_id;
    unsigned short reserved;    // 0
    // unsigned char text[text_len];
};
#pragma pack(pop)
static_assert(sizeof(HubEnvelope) == 8, "envelope is 8 bytes");
static_assert(sizeof(HubGhostFrame) == 44, "SYNG v1 is 44 bytes");
static_assert(offsetof(HubGhostFrame, level_id) == 10, "SYNG level at 10");
static_assert(offsetof(HubGhostFrame, x) == 12, "SYNG x at 12");
static_assert(offsetof(HubGhostFrame, vx) == 24, "SYNG vx at 24");
static_assert(offsetof(HubGhostFrame, yaw) == 30, "SYNG yaw at 30");
static_assert(offsetof(HubGhostFrame, anim_frame) == 34, "SYNG anim at 34");
static_assert(offsetof(HubGhostFrame, area) == 40, "SYNG area at 40");
static_assert(sizeof(HubIdentHead) == 20, "SYNI head is 20 bytes");
static_assert(sizeof(HubChatHead) == 12, "SYNT head is 12 bytes");

enum : int {
    kHubIdentNameMax = 24,
    kHubChatTextMax  = 120,
};

// ---------------------------------------------------------------------------
// LIFECYCLE
// ---------------------------------------------------------------------------

// Called once from main() (tests/walk_window.cpp) BEFORE
// comms_loopback_install_from_env(). Returns true when this is an adventure
// session the hub owns, and the caller then skips the loopback install: the
// hub was installed (relay or mesh), or the console was handed the VS
// carrier's env (SM64DS_COMMS_ROLE) with no hub address and plays solo, since
// adventure never seats a DS role. Returns false for every VS match
// (SM64DS_VS_MAP set, even with the adventure flag on), every solo run, and
// offline adventure. Whether a socket is open: comms_hub_active(). Safe to
// call more than once.
bool comms_hub_install_from_env();

// THE ARENA SHAPE (hal/vs_arena.cpp calls it; nothing else should). Opens
// the hub for a VS arena console: relay (SM64DS_COMMS_RELAY + _CODE) with
// HELLO v2 role 3 asking for `want_seat` (0..15, or 0xFF any), or the test
// mesh (SM64DS_HUB_PORT, binding base+want_seat first when one is pinned).
// No adventure gate, no VS refusal: the caller decided. True when the hub owns
// the session (opened, or failed after deciding to); false only when no
// address is named at all. Safe to call more than once (first call decides).
bool comms_hub_install_arena(int want_seat);

// True when installed in the arena shape.
bool comms_hub_arena();

// True once installed (socket open), whether or not a peer has been heard.
bool comms_hub_active();

enum HubState : int {
    kHubOff = 0,          // not installed (solo or VS)
    kHubPairing = 1,      // relay: HELLO sent, no ACK yet
    kHubLive = 2,         // seat known; sending and receiving
    kHubRelayTooOld = 3,  // the relay answered as v1: adventure online needs v2
    kHubRefused = 4,      // relay status 3/5 (version mix / wrong session kind)
    kHubFull = 5,         // relay status 1 after the first pairing (solo on)
    kHubFailed = 6,       // no socket (ws2_32, bind, resolve)
};
int comms_hub_state();

// My seat, 0..15, or -1 until the relay's ACK (loopback: the bound port).
int comms_hub_my_seat();

// ---------------------------------------------------------------------------
// SEND / RECEIVE. Thread-safe; never block. The hub runs its own I/O thread
// (keepalive, pings, receive), so presence and keepalive do not depend on the
// game's frame rate or on anyone calling in.
// ---------------------------------------------------------------------------

// Send one whole datagram (envelope included) to every other seat. Refused
// (false, counted) when the hub is not live or the tag/length/version is not
// in the table above. Byte 5 is stamped with my seat before it goes out.
bool comms_hub_send(const void *datagram, int len);

// Pop the OLDEST queued datagram of `kind`. Returns its length (the whole
// datagram, envelope included, byte 5 = the sender seat) or 0 when none is
// waiting; *seat gets the sender seat. A datagram longer than `cap` is
// dropped (counted) and the next one is tried. Each kind has exactly one
// consumer lane: ghost + ident W2, chat C1, voice V1.
int comms_hub_recv(HubKind kind, void *buf, int cap, int *seat);

// The same, plus *t_ms = the comms_hub_now_ms() at which the I/O thread took
// the datagram off the socket (after any SM64DS_HUB_DELAY_MS hold).
int comms_hub_recv_t(HubKind kind, void *buf, int cap, int *seat,
                     unsigned *t_ms);

// The hub's millisecond clock (performance counter), the one recv_t stamps.
unsigned comms_hub_now_ms();

// THE REFEREE CLOCK (arena, relay mode). The hub pings the referee ('SYNP' v2,
// byte 6 = 0xFE) at 2 Hz and keeps the 8 newest pongs; the estimate comes from
// the one with the smallest round trip (half the route asymmetry of error).
// *ref_now_ms = the referee's ms right now; *rtt_ms = that sample's RTT;
// *cap_ms (optional) = the referee's back-dating cap for my seat as it last
// said. False before the first pong (and always in the mesh).
bool comms_hub_referee_clock(unsigned *ref_now_ms, int *rtt_ms,
                             int *cap_ms);

// The NEWEST datagram of `kind` from `seat` (ghost: newest by seq). Does not
// consume anything from the FIFO. Returns the length or 0; *age_ms (optional)
// gets how long ago it arrived.
int comms_hub_latest(HubKind kind, int seat, void *buf, int cap,
                     unsigned *age_ms);

// ---------------------------------------------------------------------------
// SEATS. A seat is PRESENT while any valid datagram (pings included, 2 Hz)
// from it arrived in the last kHubPresenceMs. Consumers detect joins and
// leaves by comparing join_epoch / roster_version with what they saw last, so
// any number of lanes can watch the roster without eating each other's events.
// ---------------------------------------------------------------------------
struct HubSeatInfo {
    bool     present;
    unsigned join_epoch;      // bumps every time this seat (re)joins
    unsigned heard_ms_ago;    // any kind
    unsigned ghost_ms_ago;    // newest SYNG; ~0u if none this presence
    int      rtt_ms;          // SYNP/SYNQ round trip, -1 until measured
    // From the newest SYNG (valid when has_ghost). The hub parses only the
    // fixed fields below; W2 owns everything else in the frame.
    bool     has_ghost;
    unsigned char flags;      // kGhost*
    int      level_id;
    int      area;
    int      x, y, z;         // Fix12
};
bool comms_hub_seat_info(int seat, HubSeatInfo *out);   // false: bad seat/off
unsigned comms_hub_seat_mask();        // bit k = seat k present (never mine)
unsigned comms_hub_roster_version();   // bumps on every join or leave

// ---------------------------------------------------------------------------
// COUNTERS + REPORT
// ---------------------------------------------------------------------------
struct CommsHubStats {
    bool installed;
    int  mode;                 // 0 none, 1 relay, 2 loopback mesh
    int  state;                // HubState
    int  seat;
    unsigned seat_mask;
    unsigned long long sent[kHubKinds], recvd[kHubKinds];
    unsigned long long pings_sent, pongs_sent, pongs_recvd;
    unsigned long long send_refused;      // not live, or not in the table
    unsigned long long drop_tag, drop_len, drop_version, drop_source,
                       drop_self, drop_seat, drop_queue, drop_cap, drop_test,
                       drop_stale;        // SYNG older than the newest seq
    unsigned long long hellos, acks;
    unsigned long long joins, leaves;
    // arena shape (lane ARENACORE1)
    bool arena;
    unsigned long long drop_shape;        // a kind the other shape owns
    unsigned long long ref_pings, ref_pongs;
};
CommsHubStats comms_hub_stats();
void comms_hub_report(const char *tag);   // one "[hub] ..." line to stderr

}  // namespace port

#endif  // PORT_HAL_COMMS_HUB_H
