// PRESENCE + PARTY + COOP WIRE LAYOUTS. Run mp1 wave 56, lane WIRE1.
//
// One header, no dependencies on the carrier, so the party and coop lanes can
// include it before the carrier changes land. Everything here is plain data:
// the layout structs, the encode / decode helpers and the validators the relay
// and the hub both run (the relay has the same rules in tools/relay/relay.py;
// the two are kept equal by hand and by tools/relay/test_relay_v2.py).
//
// PORT_HOST_ABI. Nothing in src/ knows this exists.
//
// ---------------------------------------------------------------------------
// HOW THIS READS THE W56 WIRE CONTRACT (decisions, because W56 gave field
// lists, not byte offsets):
//   * EVERY datagram starts with the 4-byte tag, as every other kind does
//     ('SYNL' ...). The relay classifies on the tag and the length.
//   * Byte 4 is the version, byte 5 is the SENDER SEAT, written by the relay
//     (and by the hub before it sends). That is the existing HubEnvelope
//     (comms_hub.h), unchanged, for every new kind. A payload can never claim
//     to be another seat.
//   * The "16 B" of SYNL in W56 is the list of FIELDS (ver ... level_clock =
//     16 bytes); with the tag the datagram is 20 B. The "12 B" of SYNX counts
//     the tag (7 field bytes + 4 tag + 1 pad = 12).
//   * The SYNW / SYNB "8 B common header" is placed AFTER the tag, in the
//     envelope's own slots: ver(4) seat(5) subtype(6) flags(7) seq(8..9)
//     payload_len(10..11). W56 listed the fields as ver, subtype, seat, flags;
//     seat and subtype are swapped here so that byte 5 is the stamped seat for
//     every kind. The payload starts at byte 12, so payload_len must equal
//     (datagram length - 12), not "- 8" (the 8 is the header, 4 more are the
//     tag). Minimum datagram 16 (payload >= 4), maximum 700.
// All fields are little-endian. All layouts are packed.
//
// WHICH ROM GLOBALS FEED SYNL (confirmed against the source and measured; the
// PROBE paragraph below has the numbers):
//   level  = sublevel id, data_0209f2f8 (what SYNG.level_id carries)
//   area   = data_02092120 (signed char, -1 = none; sent as 0xFF for none)
//   act    = data_0209f220[0], the mission latched at stage boot
//            (Stage::InitResources:229, `data_0209f220 = data_0209f1f0`).
//            data_0209f1f0 is only the PENDING choice staged by the star
//            select / level change: it already shows the next act while the
//            console is still in the old level, so it is NOT the act of the
//            level the player is in. 0 = no act (menu, title, hub grounds).
//
// PROBE (measured, two consoles through a local relay, SM64DS_PRESENCE_LOG=1,
// the title path): a console booted with SM64DS_STAR_FILTER=3 sent act=3 with
// pending data_0209f1f0=3, then act=3 with pending=1 (the latched mission
// held while the pending word moved), then act=1 once a warp to level 2
// latched again. Across a level 2 door whose open animation ends in
// ChangeArea the area went 0 -> 1 and the peer heard (level 2, area 1, act 1).
// So act is data_0209f220[0], not data_0209f1f0.
//
// SESSION GATES (the relay enforces them; a send into the wrong kind of room
// is dropped and counted):
//   SYNL  party, coop, world (hub)       all other seats
//   SYNX  party only                     all other seats
//   SYNH  party only                     ONE seat: byte 6 = the victim
//   SYNW  coop only                      all other seats
//   SYNB  coop only                      all other seats
// Seats are 0..3 in party and coop sessions (a seat 4+ is refused).

#ifndef PORT_HAL_HUB_PRESENCE_H
#define PORT_HAL_HUB_PRESENCE_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace port {

// The tags as little-endian words (the first four bytes read as a u32).
enum : uint32_t {
    kHubTagPresence  = 0x4c4e5953u,   // 'S','Y','N','L'
    kHubTagPartyStar = 0x584e5953u,   // 'S','Y','N','X'
    kHubTagPartyHit  = 0x484e5953u,   // 'S','Y','N','H'
    kHubTagCoopWorld = 0x574e5953u,   // 'S','Y','N','W'
    kHubTagCoopCtl   = 0x424e5953u,   // 'S','Y','N','B'
};

// Session kinds a room can be (what the relay's HELLO v2 asked for). The
// carrier derives it from the environment (SM64DS_ROOM_KIND), see
// hub_room_kind() in comms_hub.h.
enum HubRoomKind : int {
    kHubRoomWorld = 0,    // public / code world: ghosts only (the old hub)
    kHubRoomParty = 1,    // seats 0..3, shared stars, shoves
    kHubRoomCoop  = 2,    // seats 0..3, synced world
};

enum : int {
    kHubPartySeats = 4,            // party and coop rooms hold seats 0..3
    kHubPresenceBytes = 20,        // SYNL, whole datagram
    kHubPartyStarBytes = 12,       // SYNX
    kHubPartyHitBytes = 24,        // SYNH
    kHubCoopHeaderBytes = 12,      // tag + the 8-byte common header
    kHubCoopMinDatagram = 16,
    kHubCoopMaxDatagram = 700,
    kHubPresenceHz = 2,            // steady send rate, plus on every change
};

#pragma pack(push, 1)

// ---------------------------------------------------------------------------
// 'SYNL' presence, 20 B fixed, version 1. Sent at 2 Hz and on any change of
// level / area / act / character / flags, in party, coop and world rooms.
// ---------------------------------------------------------------------------
enum : uint8_t {
    kPresInLevel    = 0x01,   // a course / area is running (not title / menu)
    kPresMenu       = 0x02,   // menu, pause or star select
    kPresCutscene   = 0x04,
    kPresPartyRoom  = 0x08,   // sender is in a party room
    kPresCoopRoom   = 0x10,   // sender is in a coop room
    kPresFlagsMask  = 0x1f,   // anything else = invalid, dropped
};
enum : uint8_t { kPresAreaNone = 0xFF };

struct HubPresenceWire {
    uint32_t tag;            // 0  kHubTagPresence
    uint8_t  version;        // 4  1
    uint8_t  seat;           // 5  stamped by the relay / hub
    uint16_t level;          // 6  sublevel id, data_0209f2f8
    uint8_t  area;           // 8  data_02092120, 0xFF = none
    uint8_t  act;            // 9  mission / star number, data_0209f220[0]; 0 = none
    uint8_t  character;      // 10 0 Mario, 1 Luigi, 2 Wario, 3 Yoshi
    uint8_t  flags;          // 11 kPres*
    uint16_t stars_total;    // 12
    uint16_t seq;            // 14 wrap-compared, newest wins
    uint32_t level_clock;    // 16 coop phase 2; 0 until then
};

// ---------------------------------------------------------------------------
// 'SYNX' party star, 12 B fixed, version 1. "I collected this star": the
// receivers write it to their own save quietly. Party rooms only.
// ---------------------------------------------------------------------------
struct HubPartyStarWire {
    uint32_t tag;            // 0  kHubTagPartyStar
    uint8_t  version;        // 4  1
    uint8_t  seat;           // 5  stamped
    uint8_t  course;         // 6  0..29
    uint8_t  star;           // 7  0..7
    uint16_t reserved;       // 8  0 = star to save; 1 star / 2 key / +4 no exit = popup event (STARPOP1); other bits dropped
    uint16_t seq;            // 10
};

// ---------------------------------------------------------------------------
// 'SYNH' party hit, 24 B fixed, version 1. ROUTED TO ONE SEAT: byte 6 names
// the victim and the relay delivers it to that seat only. Byte 5 is the
// attacker (stamped). Party rooms only. PHIT1 fills in the semantics; the
// carrier only checks the ranges below.
// ---------------------------------------------------------------------------
enum : uint8_t {
    kHitShove = 0,    // push + short stun, no damage
    kHitKinds = 1,    // 0..kHitKinds-1 are valid today; more may be added
};
struct HubPartyHitWire {
    uint32_t tag;            // 0  kHubTagPartyHit
    uint8_t  version;        // 4  1
    uint8_t  attacker;       // 5  stamped by the relay (the sender seat)
    uint8_t  victim;         // 6  0..3, never equal to the attacker
    uint8_t  hit_kind;       // 7  kHit*
    uint16_t seq;            // 8
    int16_t  yaw;            // 10 attacker yaw, the game's 16-bit angle
    int32_t  x, y, z;        // 12 attacker position, Fix12
};

// ---------------------------------------------------------------------------
// 'SYNW' coop world and 'SYNB' coop control: common 12-byte head (tag + the
// 8-byte header), then payload_len bytes. Sub-type ranges are claimed in
// W56: SYNW 0x00-0x7f COOPWORLD1; SYNB 0x00-0x1f COOPWORLD1, 0x20-0x2f
// COOPSAVE1, 0x30-0x3f COOPBOOT1. The carrier checks the header only; unknown
// sub-types are the consumer's to drop and count.
// ---------------------------------------------------------------------------
struct HubCoopHead {
    uint32_t tag;            // 0  kHubTagCoopWorld or kHubTagCoopCtl
    uint8_t  version;        // 4  1
    uint8_t  seat;           // 5  stamped
    uint8_t  subtype;        // 6
    uint8_t  flags;          // 7  0 (non-zero = dropped)
    uint16_t seq;            // 8
    uint16_t payload_len;    // 10 must equal datagram length - 12
    // uint8_t payload[payload_len];
};
#pragma pack(pop)

static_assert(sizeof(HubPresenceWire) == 20, "SYNL is 20 bytes");
static_assert(sizeof(HubPartyStarWire) == 12, "SYNX is 12 bytes");
static_assert(sizeof(HubPartyHitWire) == 24, "SYNH is 24 bytes");
static_assert(sizeof(HubCoopHead) == 12, "SYNW / SYNB head is 12 bytes");
static_assert(offsetof(HubPresenceWire, level) == 6, "SYNL level at 6");
static_assert(offsetof(HubPresenceWire, level_clock) == 16, "SYNL clock at 16");
static_assert(offsetof(HubPartyHitWire, victim) == 6, "SYNH victim at 6");
static_assert(offsetof(HubCoopHead, payload_len) == 10, "payload_len at 10");

// ---------------------------------------------------------------------------
// The decoded presence a consumer reads (HubPresence), the accessor, and the
// pure helpers. Wire structs are never handed to consumers.
// ---------------------------------------------------------------------------
struct HubPresence {
    int      seat;           // 0..3
    int      level;          // sublevel id
    int      area;           // -1 = none, else 0..254
    int      act;            // 0 = none, else the star number
    int      character;      // 0..3
    unsigned flags;          // kPres*
    int      stars_total;
    unsigned seq;
    uint32_t level_clock;
    bool in_level() const { return (flags & kPresInLevel) != 0; }
    bool in_menu() const { return (flags & kPresMenu) != 0; }
    bool in_cutscene() const { return (flags & kPresCutscene) != 0; }
};

// The newest SYNL heard from `seat` (0..15). False when none has arrived this
// presence of that seat. *age_ms (optional) = how long ago it arrived. Safe
// from any thread. Implemented in hub_presence.cpp.
bool hub_presence_get(int seat, HubPresence *out, unsigned *age_ms = nullptr);

// The presence I send: the game fills it in once a frame (the one hook) and
// the sender decides when a datagram is due. Same struct as received.
// hub_presence_publish() copies it; the sender thread-safely sends on change
// and at 2 Hz.
void hub_presence_publish(const HubPresence &mine);

// Counters for the report line and the proofs.
struct HubPresenceStats {
    unsigned long long sent, recvd, drop_bad, drop_stale;
};
HubPresenceStats hub_presence_stats();

// ---------------------------------------------------------------------------
// Validators + encode / decode. All bounds-checked: they read only `len`
// bytes and never trust a field.
// ---------------------------------------------------------------------------
inline uint32_t hub_rd32(const void *p, size_t off) {
    uint32_t v; std::memcpy(&v, static_cast<const uint8_t *>(p) + off, 4);
    return v;
}
inline uint16_t hub_rd16(const void *p, size_t off) {
    uint16_t v; std::memcpy(&v, static_cast<const uint8_t *>(p) + off, 2);
    return v;
}

// SYNL: exact length, tag, version 1, level any u16 (the consumer judges the
// level), character 0..3, flags inside the mask. The seat byte is not judged
// (0xFF before the carrier stamps it, 0..15 after).
inline bool hub_presence_validate(const void *d, int len) {
    if (!d || len != (int)sizeof(HubPresenceWire)) return false;
    HubPresenceWire w; std::memcpy(&w, d, sizeof w);
    if (w.tag != kHubTagPresence || w.version != 1) return false;
    if (w.character > 3) return false;
    if (w.flags & ~(uint8_t)kPresFlagsMask) return false;
    return true;
}
inline bool hub_presence_decode(const void *d, int len, HubPresence *out) {
    if (!hub_presence_validate(d, len) || !out) return false;
    HubPresenceWire w; std::memcpy(&w, d, sizeof w);
    out->seat = w.seat;
    out->level = w.level;
    out->area = (w.area == kPresAreaNone) ? -1 : (int)w.area;
    out->act = w.act;
    out->character = w.character;
    out->flags = w.flags;
    out->stars_total = w.stars_total;
    out->seq = w.seq;
    out->level_clock = w.level_clock;
    return true;
}
// Fills the 20 bytes (seat byte left 0xFF; the carrier stamps it). Returns 20,
// or 0 when a field is out of range.
inline int hub_presence_encode(const HubPresence &p, void *out20) {
    if (!out20) return 0;
    if (p.character < 0 || p.character > 3) return 0;
    if (p.flags & ~(unsigned)kPresFlagsMask) return 0;
    if (p.level < 0 || p.level > 0xFFFF) return 0;
    if (p.act < 0 || p.act > 255) return 0;
    if (p.area < -1 || p.area > 254) return 0;
    if (p.stars_total < 0 || p.stars_total > 0xFFFF) return 0;
    HubPresenceWire w;
    std::memset(&w, 0, sizeof w);
    w.tag = kHubTagPresence;
    w.version = 1;
    w.seat = 0xFF;
    w.level = (uint16_t)p.level;
    w.area = (p.area < 0) ? (uint8_t)kPresAreaNone : (uint8_t)p.area;
    w.act = (uint8_t)p.act;
    w.character = (uint8_t)p.character;
    w.flags = (uint8_t)p.flags;
    w.stars_total = (uint16_t)p.stars_total;
    w.seq = (uint16_t)p.seq;
    w.level_clock = p.level_clock;
    std::memcpy(out20, &w, sizeof w);
    return (int)sizeof w;
}

// SYNX: exact length, tag, version 1, course 0..29, star 0..7, reserved 0..7
// (0 = a star to save; non-zero = a popup event, hal/star_popup.h, STARPOP1).
inline bool hub_party_star_validate(const void *d, int len) {
    if (!d || len != (int)sizeof(HubPartyStarWire)) return false;
    HubPartyStarWire w; std::memcpy(&w, d, sizeof w);
    return w.tag == kHubTagPartyStar && w.version == 1 &&
           w.course <= 29 && w.star <= 7 && (w.reserved & ~(uint16_t)7) == 0;
}
inline int hub_party_star_encode(int course, int star, unsigned seq,
                                 void *out12) {
    if (!out12 || course < 0 || course > 29 || star < 0 || star > 7) return 0;
    HubPartyStarWire w;
    std::memset(&w, 0, sizeof w);
    w.tag = kHubTagPartyStar; w.version = 1; w.seat = 0xFF;
    w.course = (uint8_t)course; w.star = (uint8_t)star; w.seq = (uint16_t)seq;
    std::memcpy(out12, &w, sizeof w);
    return (int)sizeof w;
}

// SYNH: exact length, tag, version 1, victim 0..3 and not the attacker (byte 5,
// 0xFF until stamped), hit_kind known.
inline bool hub_party_hit_validate(const void *d, int len) {
    if (!d || len != (int)sizeof(HubPartyHitWire)) return false;
    HubPartyHitWire w; std::memcpy(&w, d, sizeof w);
    if (w.tag != kHubTagPartyHit || w.version != 1) return false;
    if (w.victim >= kHubPartySeats) return false;
    if (w.attacker == w.victim) return false;   // 0xFF = not stamped yet
    if (w.hit_kind >= kHitKinds) return false;
    return true;
}
// The victim is checked against 0..3 only (the attacker byte is still 0xFF
// before the carrier stamps it).
inline int hub_party_hit_encode(int victim, int hit_kind, unsigned seq,
                                int yaw, int x, int y, int z, void *out24) {
    if (!out24 || victim < 0 || victim >= kHubPartySeats) return 0;
    if (hit_kind < 0 || hit_kind >= (int)kHitKinds) return 0;
    HubPartyHitWire w;
    std::memset(&w, 0, sizeof w);
    w.tag = kHubTagPartyHit; w.version = 1; w.attacker = 0xFF;
    w.victim = (uint8_t)victim; w.hit_kind = (uint8_t)hit_kind;
    w.seq = (uint16_t)seq; w.yaw = (int16_t)yaw;
    w.x = x; w.y = y; w.z = z;
    std::memcpy(out24, &w, sizeof w);
    return (int)sizeof w;
}

// SYNW / SYNB head: length 16..700, tag is `tag`, version 1, flags 0,
// payload_len == len - 12. The seat is stamped, so it is not judged here.
inline bool hub_coop_validate(uint32_t tag, const void *d, int len) {
    if (!d || len < kHubCoopMinDatagram || len > kHubCoopMaxDatagram)
        return false;
    HubCoopHead h; std::memcpy(&h, d, sizeof h);
    if (h.tag != tag || h.version != 1 || h.flags != 0) return false;
    if ((int)h.payload_len != len - kHubCoopHeaderBytes) return false;
    return true;
}
// Builds a SYNW / SYNB datagram: head + payload. Returns the length or 0
// (payload too big / too small: the minimum datagram is 16, so a payload under
// 4 bytes is padded with zeros).
inline int hub_coop_encode(uint32_t tag, int subtype, unsigned seq,
                           const void *payload, int payload_len, void *out,
                           int cap) {
    if (!out || payload_len < 0 || (payload_len > 0 && !payload)) return 0;
    if (tag != kHubTagCoopWorld && tag != kHubTagCoopCtl) return 0;
    if (subtype < 0 || subtype > 255) return 0;
    int plen = payload_len < 4 ? 4 : payload_len;
    int total = kHubCoopHeaderBytes + plen;
    if (total > kHubCoopMaxDatagram || total > cap) return 0;
    uint8_t *o = static_cast<uint8_t *>(out);
    std::memset(o, 0, (size_t)total);
    HubCoopHead h;
    std::memset(&h, 0, sizeof h);
    h.tag = tag; h.version = 1; h.seat = 0xFF; h.subtype = (uint8_t)subtype;
    h.seq = (uint16_t)seq; h.payload_len = (uint16_t)plen;
    std::memcpy(o, &h, sizeof h);
    if (payload_len > 0) std::memcpy(o + kHubCoopHeaderBytes, payload,
                                     (size_t)payload_len);
    return total;
}

// Wrap-compared newest-wins for the u16 sequence numbers on these kinds.
inline bool hub_seq_newer(unsigned a, unsigned b) {
    return (uint16_t)((uint16_t)a - (uint16_t)b) != 0 &&
           (uint16_t)((uint16_t)a - (uint16_t)b) < 0x8000u;
}

}  // namespace port

#endif  // PORT_HAL_HUB_PRESENCE_H
