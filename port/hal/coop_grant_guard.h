// COOP GRANT GUARD (run mp1, lane MPSAFE1). Pure: no Windows, no ROM, no globals, so a host proof can
// compile and drive it alone.
//
// WHY. The coop save record (coop_save_wire.h) can only ever OR bits in, so a peer cannot REMOVE or
// reset anything. It could still GRANT: a hostile peer sends a record with every mergeable bit set
// and the host's shared file then holds every star, key and door (and a hostile "host" record did the
// same to a guest and, through the guest's honest resend, to the real host). There are no accounts, so
// "is this peer allowed" cannot be asked. This guard asks the questions a machine can answer:
//   1. STARS (the 30 course bytes): a star bit is believed only for a course the sender was SEEN in
//      (its SYNL presence) in the last kGgVisitMs, and only a few new stars per minute per sender.
//      A friend who really took a star was in that course a moment ago; a forged record was not.
//   2. OTHER BITS (keys, door and switch flags, cannons): a burst allowance per sender that refills
//      slowly. A boss win sets a handful of bits at once; a forged "every bit" record sets dozens.
// A record that fails is dropped WHOLE (never half applied) and counted. The sender's real progress
// is not lost: guests resend a record every 1.5 s until the host's record covers it, so a record that
// failed only on the allowance is accepted when the allowance has refilled.
// Not applied to the one-time fetch of the host record at a guest's boot (the guest has nothing to
// lose yet) and not to a host record rebroadcast of OTHER players' stars (no visit test, allowance only).
#ifndef PORT_HAL_COOP_GRANT_GUARD_H
#define PORT_HAL_COOP_GRANT_GUARD_H

#include <stdint.h>
#include <string.h>

#include "coop_save_wire.h"

namespace port {

enum : int {
    kGgSeats = 16,
    kGgCourses = 30,
    kGgStarBurst = 6,            // new stars a sender may add at once
    kGgStarRefillMs = 10000,     // then one more star per this
    kGgBitBurst = 48,            // new non-star bits at once (a boss win is a handful)
    kGgBitRefillMs = 2000,       // then one more per this
    kGgVisitMs = 180000,         // the sender must have been in the star's course this recently
};

enum GgVerdict : int { kGgOk = 0, kGgBadSeat, kGgNotThere, kGgStarCap, kGgBitCap, kGgVerdicts };

inline const char *gg_name(int v) {
    static const char *const n[kGgVerdicts] = {"ok", "bad_seat", "star_not_visited", "star_cap", "bit_cap"};
    return (v >= 0 && v < kGgVerdicts) ? n[v] : "?";
}

// What `src` would newly set in `dst` (mergeable bits only): stars (the 30 course bytes at +0x14) and
// everything else. course_new[c] is non-zero for each course that gains a star.
inline void gg_count_new(const uint8_t *dst, const uint8_t *src, int *stars, int *other,
                         uint8_t *course_new /* kGgCourses */) {
    const uint8_t *mk = coop_mono_mask();
    *stars = 0; *other = 0;
    if (course_new) memset(course_new, 0, kGgCourses);
    for (int i = 0; i < kCoopRecBytes; ++i) {
        const uint8_t add = (uint8_t)(src[i] & mk[i] & (uint8_t)~dst[i]);
        if (!add) continue;
        int n = 0;
        for (uint8_t b = add; b; b = (uint8_t)(b & (b - 1))) ++n;
        if (i >= 0x14 && i < 0x14 + kGgCourses) {
            *stars += n;
            if (course_new) course_new[i - 0x14] = 1;
        } else {
            *other += n;
        }
    }
}

struct CoopGrantGuard {
    struct Bucket { uint32_t milli; uint32_t last; bool init; };
    Bucket star[kGgSeats], bit[kGgSeats];

    void reset() { memset(this, 0, sizeof *this); }
    void reset_seat(int s) {
        if (s >= 0 && s < kGgSeats) { memset(&star[s], 0, sizeof star[s]); memset(&bit[s], 0, sizeof bit[s]); }
    }
    static void refill(Bucket &b, uint32_t now, int burst, uint32_t refill_ms) {
        const uint32_t cap = (uint32_t)burst * 1000u;
        if (!b.init) { b.init = true; b.milli = cap; b.last = now; return; }
        const uint32_t dt = (uint32_t)(now - b.last);
        b.last = now;
        const uint64_t add = (uint64_t)dt * 1000u / refill_ms;
        const uint64_t m = (uint64_t)b.milli + add;
        b.milli = m > cap ? cap : (uint32_t)m;
    }
    // `visited[c]` non-zero when the sender was seen in course c recently; null = skip the visit test
    // (the allowance still applies). Consumes the allowance only on kGgOk.
    GgVerdict admit(int seat, const uint8_t *dst, const uint8_t *src, uint32_t now, const uint8_t *visited) {
        if (seat < 0 || seat >= kGgSeats) return kGgBadSeat;
        int stars, other;
        uint8_t cn[kGgCourses];
        gg_count_new(dst, src, &stars, &other, cn);
        if (stars == 0 && other == 0) return kGgOk;
        if (visited) {
            for (int c = 0; c < kGgCourses; ++c)
                if (cn[c] && !visited[c]) return kGgNotThere;
        }
        Bucket &bs = star[seat], &bb = bit[seat];
        refill(bs, now, kGgStarBurst, kGgStarRefillMs);
        refill(bb, now, kGgBitBurst, kGgBitRefillMs);
        if ((uint32_t)stars * 1000u > bs.milli) return kGgStarCap;
        if ((uint32_t)other * 1000u > bb.milli) return kGgBitCap;
        bs.milli -= (uint32_t)stars * 1000u;
        bb.milli -= (uint32_t)other * 1000u;
        return kGgOk;
    }
};

}  // namespace port

#endif  // PORT_HAL_COOP_GRANT_GUARD_H
