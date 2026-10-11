// PARTY SHARED STARS. Run mp1 wave 56, lane PSTAR1.
//
// "In a party they will share any stars collected." A star one console picks
// up is written, quietly, into every other party member's own save. This file
// is the whole feature: the sender's observer, the receiver's rules, the queue
// and the save write. Nothing in src/ knows it exists (PORT_HOST_ABI).
//
// THE SENDER. Once a frame the observer diffs the 30 course bytes of the save
// record (data_0209cab4, 8 star bits each). Every way the game collects a star
// (course star, red coins, 100 coins, Bowser keys) ends in CollectStar, which
// is a single `|=` on those bytes, so a diff catches all of them within one
// frame without touching the matched source. Exactly ONE new bit with nothing
// cleared, in the file slot that was open a frame ago, with the body alive, is
// a collected star: it goes out as one 'SYNX' (hal/hub_presence.h, 12 bytes:
// course, star). Anything else (a file load, a slot change, several bits at
// once) just re-baselines. A star that arrived from a friend is written with
// the baseline moved in the same step, so it is never echoed.
//
// THE RECEIVER. The star lands only if ALL of these hold, in this order:
//   party room (SM64DS_PARTY=1 and the hub is a party room) and ACCEPT on;
//   the hub already checked tag, version, length, course < 30, star < 8,
//   reserved 0 (hub_party_star_validate runs again here);
//   the sender's seat is 0..3, present, and not mine;
//   it is not a repeat of the same sequence number from that seat (the sender
//   sends each star three times against packet loss, same sequence);
//   the sender's own presence ('SYNL') put it in that course within the last
//   60 s (a star for a course the sender never visited is dropped);
//   a save file is open (the record's magic is there);
//   the bit is not already set, and not already queued;
//   the limits hold: 1 per 10 s per seat, 6 per 10 min per seat, 12 per 10 min
//   for the whole room (repeats never spend them);
//   the queue (8) has room.
// It is then QUEUED and written on a SAFE frame: a level running, the pause
// and star-select and level-clear and message windows closed, no fade, no
// cutscene, no level change pending, 45 frames in a row, and 4 s after this
// console's own last star. The write is the bit, then the ROM's own
// SaveData::SaveCurrentFile, then one text line. If the open file slot is not
// the one the star arrived under, the whole queue is dropped: a friend's star
// is never written into a different save. A console that exits with a queue
// loses it (stated in the release notes): the party shares what was collected
// while everybody was online, and a member who joins later gets nothing from
// before.
//
// FORGERY. Nothing on the wire carries a total or a mask, so one message is at
// most one new bit; the limits above make 150 stars a matter of hours; each
// landed star prints a line naming its sender; both directions have a switch
// (SM64DS_PARTY_SHARE_STARS / SM64DS_PARTY_ACCEPT_STARS, default 1). The honest
// limit: a hostile member of a private room can slowly add stars. Nobody can
// prove a star was earned.
//
// KNOBS (env; all off unless named). The first two are the player's switches,
// the rest are for the proofs.
//   SM64DS_PARTY_SHARE_STARS=0|1   send my stars (default 1)
//   SM64DS_PARTY_ACCEPT_STARS=0|1  take friends' stars (default 1)
//   SM64DS_PARTY_STARS_LOG=1       one "[pstar]" line per decision
//   SM64DS_PARTY_STARS_PROBE=star:<course>:<bit>@<frame>[;...]
//       once a level is running and this console's frame counter has passed
//       <frame>: CollectStar(course, bit), then SaveCurrentFile (what the star
//       select's Save does). Stands in for walking into the star.
//   SM64DS_PARTY_STARS_HOLD=<frames>:<n>
//       force "not safe" for n frames from <frame> (a star landing during a
//       star-get / menu has to wait)
//   SM64DS_PARTY_STARS_SLOTSWAP=<frame>
//       flip the open file slot byte at that frame (the queue must drop)
//   SM64DS_PARTY_STARS_TRIGGER=<path>
//       when that file exists its first line "<course> <bit>" is collected like
//       a probe and the file is removed (a proof script sequences consoles by
//       wall clock with it)
//   SM64DS_PARTY_STARS_SKIP=<mask>
//       ignore the safe-frame conditions whose bit is set (frame_is_safe's
//       enum), to find which one blocks
//   SM64DS_PARTY_STARS_NO_LIMIT=1  MUST-FAIL control: switch the limiter off
//
// LOG LINES (stderr): "[pstar] tx ..." per star sent, "[pstar] rx seat=.. ..."
// per decision (with SM64DS_PARTY_STARS_LOG=1), "[pstar] LAND seat=.. course=..
// star=.. stars=<before>-><after> slot=.. file_ok=.. line=..", "[pstar]
// report ..." once at exit and every 10 s.

#ifndef PORT_HAL_PARTY_STARS_H
#define PORT_HAL_PARTY_STARS_H

#include <cstdint>
#include <cstring>

namespace port {

enum : int {
    kPsCourses   = 30,        // data_0209cab4 is 30 bytes
    kPsStarBits  = 8,
    kPsSeats     = 4,
    kPsQueueMax  = 8,
    kPsVisitMs   = 60000,     // the sender must have been in the course this recently
    kPsGapMs     = 10000,     // 1 per 10 s per seat
    kPsWindowMs  = 600000,    // 10 min
    kPsSeatMax   = 6,         // per seat per window
    kPsRoomMax   = 12,        // per room per window
    kPsSafeFrames = 45,       // consecutive safe frames before a write
    kPsHoldFrames = 240,      // after this console's own star
    kPsStaleMs   = 300000,    // a queued star older than this is dropped
    kPsApplyGapMs = 400,      // between two writes
    kPsSeqMemory = 8,         // sequence numbers remembered per seat
    kPsCopies    = 3,         // a star goes out this many times (same seq)
};

// Why a SYNX was not taken. Index of the counter array.
enum PsDrop : int {
    kPsOk = 0,
    kPsDropRoom,        // not a party room / env
    kPsDropAccept,      // ACCEPT switch off
    kPsDropBad,         // failed the field check
    kPsDropSeat,        // seat out of range, or mine
    kPsDropNoPres,      // sender not present / never heard a presence
    kPsDropDupSeq,      // a repeat copy of a star already seen
    kPsDropCourse,      // the sender was not in that course in the last 60 s
    kPsDropNoFile,      // no save file open here
    kPsDropHave,        // the bit is already set
    kPsDropQueued,      // already waiting in the queue
    kPsDropGap,         // 1 per 10 s per seat
    kPsDropSeatCap,     // 6 per 10 min per seat
    kPsDropRoomCap,     // 12 per 10 min per room
    kPsDropFull,        // queue full
    kPsDropSlot,        // queued, then the file slot changed
    kPsDropStale,       // queued too long
    kPsDropReasons
};

// --------------------------------------------------------------------------
// The observer's diff (pure). prev / cur are the 30 course bytes.
// --------------------------------------------------------------------------
struct PsDiff {
    int added;          // bits set now that were clear
    int cleared;        // bits clear now that were set
    int course, star;   // the first added bit (valid when added > 0)
};
inline PsDiff ps_diff(const uint8_t *prev, const uint8_t *cur) {
    PsDiff d;
    d.added = d.cleared = 0;
    d.course = d.star = -1;
    for (int c = 0; c < kPsCourses; ++c) {
        const uint8_t a = (uint8_t)(cur[c] & ~prev[c]);
        const uint8_t g = (uint8_t)(prev[c] & ~cur[c]);
        for (int b = 0; b < kPsStarBits; ++b) {
            if (a & (1u << b)) {
                if (d.added == 0) { d.course = c; d.star = b; }
                ++d.added;
            }
            if (g & (1u << b)) ++d.cleared;
        }
    }
    return d;
}

// --------------------------------------------------------------------------
// The limiter (pure). Time is the caller's millisecond clock.
// --------------------------------------------------------------------------
struct PsLimiter {
    uint32_t seat_t[kPsSeats][kPsSeatMax];   // accept times in the window
    int      seat_n[kPsSeats];
    uint32_t last[kPsSeats];
    bool     have_last[kPsSeats];
    uint32_t room_t[kPsRoomMax];
    int      room_n;

    void reset() { std::memset(this, 0, sizeof *this); }
    static int prune(uint32_t *t, int n, uint32_t now) {
        int k = 0;
        for (int i = 0; i < n; ++i)
            if ((uint32_t)(now - t[i]) < (uint32_t)kPsWindowMs) t[k++] = t[i];
        return k;
    }
    // kPsOk, or the reason. Does NOT record; call take() after the queue push.
    PsDrop check(int seat, uint32_t now) {
        if (seat < 0 || seat >= kPsSeats) return kPsDropSeat;
        if (have_last[seat] && (uint32_t)(now - last[seat]) < (uint32_t)kPsGapMs)
            return kPsDropGap;
        seat_n[seat] = prune(seat_t[seat], seat_n[seat], now);
        if (seat_n[seat] >= kPsSeatMax) return kPsDropSeatCap;
        room_n = prune(room_t, room_n, now);
        if (room_n >= kPsRoomMax) return kPsDropRoomCap;
        return kPsOk;
    }
    void take(int seat, uint32_t now) {
        if (seat < 0 || seat >= kPsSeats) return;
        seat_n[seat] = prune(seat_t[seat], seat_n[seat], now);
        if (seat_n[seat] < kPsSeatMax) seat_t[seat][seat_n[seat]++] = now;
        room_n = prune(room_t, room_n, now);
        if (room_n < kPsRoomMax) room_t[room_n++] = now;
        last[seat] = now;
        have_last[seat] = true;
    }
};

// --------------------------------------------------------------------------
// Where each seat has been recently (pure). Fed from the seat's presence.
// --------------------------------------------------------------------------
struct PsVisits {
    uint32_t t[kPsSeats][kPsCourses];
    bool     seen[kPsSeats][kPsCourses];
    void reset() { std::memset(this, 0, sizeof *this); }
    void reset_seat(int s) {
        if (s >= 0 && s < kPsSeats) {
            std::memset(t[s], 0, sizeof t[s]);
            std::memset(seen[s], 0, sizeof seen[s]);
        }
    }
    void note(int seat, int course, uint32_t now) {
        if (seat < 0 || seat >= kPsSeats || course < 0 || course >= kPsCourses)
            return;
        t[seat][course] = now;
        seen[seat][course] = true;
    }
    bool within(int seat, int course, uint32_t now, uint32_t ms) const {
        if (seat < 0 || seat >= kPsSeats || course < 0 || course >= kPsCourses)
            return false;
        return seen[seat][course] && (uint32_t)(now - t[seat][course]) <= ms;
    }
};

// The sublevel id -> course through the ROM's SUBLEVEL_LEVEL_TABLE (52
// entries, -1 = none). Pure: the caller passes the table.
inline int ps_course_of_level(const signed char *table, int level) {
    if (!table || level < 0 || level >= 52) return -1;
    const int c = table[level];
    return (c >= 0 && c < kPsCourses) ? c : -1;
}

// --------------------------------------------------------------------------
// The queue (pure).
// --------------------------------------------------------------------------
struct PsStar {
    int      seat, course, star, slot;
    uint32_t t;
};
struct PsQueue {
    PsStar e[kPsQueueMax];
    int    n;
    void reset() { n = 0; }
    bool has(int course, int star) const {
        for (int i = 0; i < n; ++i)
            if (e[i].course == course && e[i].star == star) return true;
        return false;
    }
    bool push(const PsStar &s) {
        if (n >= kPsQueueMax) return false;
        e[n++] = s;
        return true;
    }
    PsStar pop() {
        const PsStar s = e[0];
        for (int i = 1; i < n; ++i) e[i - 1] = e[i];
        --n;
        return s;
    }
};

// --------------------------------------------------------------------------
// Sequence memory per seat (pure): true when `seq` was seen from `seat`
// already, else it is remembered.
// --------------------------------------------------------------------------
struct PsSeqMemory {
    uint16_t s[kPsSeats][kPsSeqMemory];
    uint8_t  n[kPsSeats];
    uint8_t  head[kPsSeats];
    void reset() { std::memset(this, 0, sizeof *this); }
    bool seen_or_add(int seat, unsigned seq) {
        if (seat < 0 || seat >= kPsSeats) return true;
        for (int i = 0; i < n[seat]; ++i)
            if (s[seat][i] == (uint16_t)seq) return true;
        s[seat][head[seat]] = (uint16_t)seq;
        head[seat] = (uint8_t)((head[seat] + 1) % kPsSeqMemory);
        if (n[seat] < kPsSeqMemory) ++n[seat];
        return false;
    }
};

// --------------------------------------------------------------------------
// Counters, for the report line and the proofs.
// --------------------------------------------------------------------------
struct PartyStarsStats {
    unsigned long long sent_stars;     // distinct stars announced
    unsigned long long sent_datagrams; // including the extra copies
    unsigned long long recv;           // every SYNX handed to the handler
    unsigned long long queued, landed;
    unsigned long long observed_multi; // several bits in one frame (re-baseline)
    unsigned long long rebaselines;
    unsigned long long dropped[kPsDropReasons];
    unsigned long long hold_waits;     // frames a queued star waited on an unsafe frame
};
PartyStarsStats party_stars_stats();

// Mine / my room, for the proofs.
bool party_stars_enabled();

}  // namespace port

#endif  // PORT_HAL_PARTY_STARS_H
