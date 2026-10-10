// COOP LEVEL GROUPS AND OWNERS. Run mp1 wave 56, lane COOPWORLD1.
//
// Pure bookkeeping, no game and no wire in here, so it can be driven by a
// table-top test (and is). The game side is coop_world.cpp.
//
// A group is every console standing in the same (sublevel, act). Each console
// tracks the membership of ITS OWN group from three kinds of evidence: the
// 'SYNL' presence of the other seats (who is in the same level and act), the
// 1 Hz GROUP_HELLO ("I hold this group's state", the seat I believe owns it),
// and OWNER_CHANGE.
//
// OWNER. The first console in the group (the one that found nobody to ask).
// It stays owner until it leaves the group or drops out; then the LOWEST
// remaining established seat takes over and announces it. A console that is
// still loading (not established) is never an owner and never counted as one.
// Two consoles that both claim it (they entered in the same breath) settle on
// the lower seat, but only when both claims show up within kTieMs of each
// other. A claim that arrives later never displaces an owner the group
// already has (a late lower-seat claimant is not a tie), and the owner a
// joiner took its state from is never displaced while it lives. Everything the owner does is idempotent, so a moment of
// disagreement costs nothing.

#ifndef PORT_HAL_COOP_GROUPS_H
#define PORT_HAL_COOP_GROUPS_H

#include <cstdint>

namespace port {

struct CoopKey {
    int level;     // sublevel id, -1 = no group
    int act;
    CoopKey() : level(-1), act(0) {}
    CoopKey(int l, int a) : level(l), act(a) {}
    bool valid() const { return level >= 0; }
    bool operator==(const CoopKey &o) const { return level == o.level && act == o.act; }
    bool operator!=(const CoopKey &o) const { return !(*this == o); }
};

class CoopGroups {
public:
    enum { kSeats = 16, kEvidenceMs = 3500, kTieMs = 4000 };

    void reset(int my_seat);
    int  my_seat() const { return me_; }

    // MY group.
    void enter(const CoopKey &k, unsigned now);       // I changed group; not established
    void leave();                                     // I left every group
    void establish(unsigned now, int owner_hint);     // I hold the state now (hint -1 = none)
    bool established() const { return est_; }
    const CoopKey &key() const { return key_; }

    // Evidence about the others.
    void note_presence(int seat, bool present, const CoopKey &theirs, unsigned now);
    void note_hello(int seat, const CoopKey &k, bool est, int owner_claim, unsigned now);
    void note_owner_change(int seat, const CoopKey &k, unsigned now);

    // Recompute membership and owner. Call once a frame.
    void update(unsigned now);

    int      owner() const { return owner_; }                 // -1 none
    bool     i_am_owner() const { return est_ && owner_ == me_; }
    bool     is_member(int seat) const;                       // present, same key
    bool     is_established(int seat) const;                  // member whose hello says so
    // True only for the owner I believe in (a present member). A seat that merely
    // claims ownership is not trusted until update() makes it the owner. FULL bitmaps trust this.
    bool     owner_claimed_by(int seat) const;
    unsigned member_mask() const;                             // others in my group, bit = seat
    int      others() const;                                  // count of those
    int      established_count() const;                       // established others
    // True once each time I become owner by claim or failover while others are
    // here: the caller sends OWNER_CHANGE.
    bool     take_owner_announce();

private:
    struct Seat {
        bool present;
        CoopKey theirs;      // from presence
        bool hello;          // a hello for my key is on record
        bool est;
        int  claim;          // this seat said it owns the group, else -1
        unsigned hello_ms;
        unsigned claim_ms;   // when this seat's standing claim was first heard
        bool pinned;         // the owner I took my state from: nobody displaces it while it lives
    };
    int me_ = -1;
    CoopKey key_;
    bool est_ = false;
    int owner_ = -1;
    bool announce_ = false;
    unsigned own_ms_ = 0;     // when I became owner
    Seat s_[kSeats] = {};
};

}  // namespace port

#endif  // PORT_HAL_COOP_GROUPS_H
