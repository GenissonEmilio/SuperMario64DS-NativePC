// See coop_groups.h. Pure logic; no game, no wire.

#include "coop_groups.h"

#include <cstring>

namespace port {

void CoopGroups::reset(int my_seat) {
    me_ = my_seat;
    key_ = CoopKey();
    est_ = false;
    owner_ = -1;
    announce_ = false;
    std::memset(s_, 0, sizeof s_);
    for (int i = 0; i < kSeats; ++i) s_[i].claim = -1;
}

void CoopGroups::enter(const CoopKey &k, unsigned) {
    key_ = k;
    est_ = false;
    owner_ = -1;
    announce_ = false;
    // Any hello on record was for the group I came from.
    for (int i = 0; i < kSeats; ++i) {
        s_[i].hello = false;
        s_[i].est = false;
        s_[i].claim = -1;
    }
}

void CoopGroups::leave() {
    enter(CoopKey(), 0);
}

void CoopGroups::establish(unsigned now, int owner_hint) {
    if (!key_.valid()) return;
    est_ = true;
    owner_ = -1;
    if (owner_hint >= 0 && owner_hint < kSeats && owner_hint != me_ &&
        is_member(owner_hint)) {
        owner_ = owner_hint;
        s_[owner_hint].hello = true;
        s_[owner_hint].est = true;
        s_[owner_hint].claim = owner_hint;
        s_[owner_hint].hello_ms = now;
    }
    // owner_ stays -1 without a hint: update() picks me when I am alone.
}

void CoopGroups::note_presence(int seat, bool present, const CoopKey &theirs,
                               unsigned) {
    if (seat < 0 || seat >= kSeats || seat == me_) return;
    Seat &s = s_[seat];
    s.present = present;
    s.theirs = present ? theirs : CoopKey();
    if (!present || s.theirs != key_) {
        s.hello = false;
        s.est = false;
        s.claim = -1;
    }
}

void CoopGroups::note_hello(int seat, const CoopKey &k, bool est,
                            int owner_claim, unsigned now) {
    if (seat < 0 || seat >= kSeats || seat == me_ || k != key_) return;
    Seat &s = s_[seat];
    s.hello = true;
    s.est = est;
    s.claim = (est && owner_claim == seat) ? seat : -1;   // only a seat's claim about itself counts
    s.hello_ms = now;
}

void CoopGroups::note_owner_change(int seat, const CoopKey &k, unsigned now) {
    if (seat < 0 || seat >= kSeats || seat == me_ || k != key_) return;
    Seat &s = s_[seat];
    s.hello = true;
    s.est = true;
    s.claim = seat;
    s.hello_ms = now;
}

bool CoopGroups::is_member(int seat) const {
    if (seat < 0 || seat >= kSeats || seat == me_ || !key_.valid()) return false;
    return s_[seat].present && s_[seat].theirs == key_;
}

bool CoopGroups::is_established(int seat) const {
    return is_member(seat) && s_[seat].hello && s_[seat].est;
}

bool CoopGroups::owner_claimed_by(int seat) const {
    if (!est_ || seat < 0 || seat >= kSeats || seat == me_) return false;
    if (seat == owner_) return is_member(seat);
    return is_established(seat) && s_[seat].claim == seat;
}

unsigned CoopGroups::member_mask() const {
    unsigned m = 0;
    for (int i = 0; i < kSeats; ++i)
        if (is_member(i)) m |= 1u << i;
    return m;
}

int CoopGroups::others() const {
    int n = 0;
    for (int i = 0; i < kSeats; ++i) n += is_member(i) ? 1 : 0;
    return n;
}

int CoopGroups::established_count() const {
    int n = 0;
    for (int i = 0; i < kSeats; ++i) n += is_established(i) ? 1 : 0;
    return n;
}

void CoopGroups::update(unsigned now) {
    if (!key_.valid()) return;
    // Hello evidence goes stale: an established member that stopped saying so.
    for (int i = 0; i < kSeats; ++i) {
        Seat &s = s_[i];
        if (s.hello && (unsigned)(now - s.hello_ms) > (unsigned)kEvidenceMs) {
            s.hello = false;
            s.est = false;
            s.claim = -1;
        }
    }
    if (!est_) { owner_ = -1; return; }

    // The owner I believe in, still a candidate?
    const bool owner_ok = (owner_ == me_) ||
                          (owner_ >= 0 && is_established(owner_));
    if (owner_ok) {
        // Two claimants: the lower seat keeps it.
        for (int i = 0; i < kSeats; ++i) {
            if (!is_established(i) || s_[i].claim != i) continue;
            if (i < owner_) owner_ = i;
            break;
        }
        return;
    }

    // Nobody, or a gone owner: a standing claim first, else the lowest
    // established seat, counting myself.
    int pick = -1;
    for (int i = 0; i < kSeats; ++i)
        if (is_established(i) && s_[i].claim == i) { pick = i; break; }
    if (pick < 0) {
        pick = me_;
        for (int i = 0; i < me_; ++i)
            if (is_established(i)) { pick = i; break; }
    }
    const int was = owner_;
    owner_ = pick;
    if (owner_ == me_ && was != me_ && others() > 0) announce_ = true;
}

bool CoopGroups::take_owner_announce() {
    const bool a = announce_;
    announce_ = false;
    return a;
}

}  // namespace port
