// GHOSTDOOR1 (run mp1, wave 56): the door half of the hub ghost layer. See
// ghost_door.h for the wire meaning. The sender reads its own body and the
// door it is using; the watcher plays that door's MODEL animation, nothing
// else.
//
// WHY ONLY THE MODEL. The ROM's door runs on the local player alone:
// Door::Behavior -> func_ov100_02145370 reads data_0209f394[data_0209f250]
// and hands THAT body to the door's state callbacks, which call
// Player::OpenBigDoor, SetRealCharacter, ChangeArea and ShowArea. Pointed at a
// ghost's door those would move the watcher (his area, his character, his
// camera). The door's drawn pose is a pure function of its Animation cursor
// (ModelAnim::Render -> UpdateVerts -> UpdateBones at currFrame), so writing
// the cursor is the whole visual, with no game logic run at all.
#include "ghost_door.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
void *_ZN8dActor_c4NextEPKS_(const void *prev);
extern int _ZN6Player7IsStateERNS_5StateE(void *p, void *st);
extern char data_ov002_0211022c;      // Player's no-control state
extern signed char data_02092120;     // the area shown (-1 none)
extern signed char data_0209f2f8;     // the level
}

namespace port {
namespace ghostdoor {
namespace {

enum {
    kDoorActorId = 353,                   // g_profile_DOOR (ov100), the plain door
    kCursor      = 0x12c,                 // Door: ModelAnim 0xd4 + Animation 0x50 + currFrame 8
    kNumFrames   = 0x128,                 // ...numFrames << 12, flags in the top two bits
    kNode        = 0x140,                 // the state callback node
    kSearchR     = 300 << 12,             // a door this near the body (x, z)
    kSearchY     = 220 << 12,
    kMaxSeats    = 16,
};

Stats g_stats;

inline int rd(const void *p, int off) { return *(const int *)((const char *)p + off); }
inline unsigned char rb(const void *p, int off) {
    return *((const unsigned char *)p + off);
}

bool is_door(const void *a) {
    return *(const unsigned short *)((const char *)a + 0x0c) == kDoorActorId;
}

// The nearest door actor to (x, y, z), or null. Horizontal distance first;
// ties by the first found.
void *nearest_door(int x, int y, int z) {
    void *best = 0;
    long long bd = 0;
    for (void *a = _ZN8dActor_c4NextEPKS_(0); a; a = _ZN8dActor_c4NextEPKS_(a)) {
        if (!is_door(a)) continue;
        const long long dx = (long long)(rd(a, 0x5c) - x),
                        dz = (long long)(rd(a, 0x64) - z);
        const int dy = rd(a, 0x60) - y;
        if (dx > kSearchR || dx < -kSearchR || dz > kSearchR || dz < -kSearchR ||
            dy > kSearchY || dy < -kSearchY)
            continue;
        const long long d = (dx >> 4) * (dx >> 4) + (dz >> 4) * (dz >> 4);
        if (!best || d < bd) { best = a; bd = d; }
    }
    return best;
}

bool door_alive(const void *d) {
    for (void *a = _ZN8dActor_c4NextEPKS_(0); a; a = _ZN8dActor_c4NextEPKS_(a))
        if (a == d) return true;
    return false;
}

int door_frames(const void *d) {
    return (rd(d, kNumFrames) & ~(int)0xc0000000) >> 12;
}

}  // namespace

const Stats &stats() { return g_stats; }

unsigned char pack_area(int area) {
    if (area < 0) return 0;
    if (area >= 14) return 15;
    return (unsigned char)(area + 1);
}

unsigned char sender_kind(const void *body) {
    if (!body) return kDoorNone;
    const char *c = (const char *)body;
    if (!_ZN6Player7IsStateERNS_5StateE((void *)body, &data_ov002_0211022c))
        return kDoorNone;
    switch (rb(c, 0x70a)) {                 // mNoCtrlKind
    case 7:                                  return kDoorPlain;
    case 0xa: case 0xb: case 0xe:            return kDoorTalk;
    case 0x10: case 0x11:                    return kDoorBig;
    default: break;
    }
    if (rb(c, 0x6e3) == 0x13 && rb(c, 0x70b) != 0)   // the big-door step
        return kDoorBig;
    return kDoorNone;
}

unsigned char sender_frame(const void *body) {
    if (!body) return kDoorFrameNone;
    const void *d = nearest_door(rd(body, 0x5c), rd(body, 0x60), rd(body, 0x64));
    if (!d) return kDoorFrameNone;
    int f = rd(d, kCursor) >> 12;
    if (f < 0) f = 0;
    if (f > 254) f = 254;
    return (unsigned char)f;
}

bool sender_hidden(const void *body) {
    if (!body) return false;
    const char *c = (const char *)body;
    if (rb(c, 0x6f5) == 0) return true;                          // mOpacity
    if (*(const unsigned *)(c + 0xb0) & 0x10u) return true;      // held / eaten
    return false;
}

void sender_probe(const void *body, unsigned frame, unsigned char kind) {
    static int on = -1;
    if (on < 0) on = std::getenv("SM64DS_GHOST_DOOR_PROBE") ? 1 : 0;
    if (!on || !body) return;
    static int last_level = -2, last_area = -2;
    static unsigned last_node = 0xffffffffu;
    static int last_cur = -999;
    if ((int)data_0209f2f8 != last_level || (int)data_02092120 != last_area) {
        last_level = data_0209f2f8;
        last_area = data_02092120;
        int n = 0;
        for (void *a = _ZN8dActor_c4NextEPKS_(0); a; a = _ZN8dActor_c4NextEPKS_(a)) {
            if (!is_door(a)) continue;
            std::fprintf(stderr,
                         "[gwdoors] level %d area %d door %d @%p pos=%d,%d,%d "
                         "yaw=%d variant=%u areas=%d,%d cursor=%d frames=%d "
                         "node=%08x\n",
                         (int)data_0209f2f8, (int)data_02092120, n++, a,
                         rd(a, 0x5c) >> 12, rd(a, 0x60) >> 12, rd(a, 0x64) >> 12,
                         (int)*(const short *)((const char *)a + 0x8e),
                         (unsigned)rd(a, 8), (int)*(const signed char *)((const char *)a + 0x8c),
                         (int)*(const signed char *)((const char *)a + 0x90),
                         rd(a, kCursor) >> 12, door_frames(a),
                         (unsigned)rd(a, kNode));
        }
        std::fprintf(stderr, "[gwdoors] level %d area %d: %d doors\n",
                     (int)data_0209f2f8, (int)data_02092120, n);
    }
    const void *d = nearest_door(rd(body, 0x5c), rd(body, 0x60), rd(body, 0x64));
    const unsigned node = d ? (unsigned)rd(d, kNode) : 0u;
    const int cur = d ? rd(d, kCursor) >> 12 : -1;
    if (kind || node != last_node || cur != last_cur) {
        last_node = node;
        last_cur = cur;
        std::fprintf(stderr,
                     "[gwdoor] f=%u kind=%u noctrl=%u step=%u big=%u pos=%d,%d,%d "
                     "yaw=%d anim=%u door=%p cursor=%d/%d node=%08x\n",
                     frame, (unsigned)kind, (unsigned)rb(body, 0x70a),
                     (unsigned)rb(body, 0x6e3), (unsigned)rb(body, 0x70b),
                     rd(body, 0x5c), rd(body, 0x60), rd(body, 0x64),
                     (int)*(const short *)((const char *)body + 0x8e),
                     (unsigned)(rd(body, 0x63c) >> 2), d, cur,
                     d ? door_frames(d) : -1, node);
    }
}

// ---------------------------------------------------------------------------
// The watcher.
namespace {
struct Drive {
    void *door;       // the door this seat is driving, or null
    int   orig;       // its cursor before the first write (Fix12)
    int   written;    // the cursor as last written
    bool  yielded;    // the local game moved it: leave it alone until released
};
Drive g_drive[kMaxSeats];

void give_back(Drive &d) {
    if (d.door && door_alive(d.door) && !d.yielded &&
        rd(d.door, kCursor) == d.written)
        *(int *)((char *)d.door + kCursor) = d.orig;
    d.door = 0;
    d.yielded = false;
}
}  // namespace

void watcher_release(int seat) {
    if (seat < 0 || seat >= kMaxSeats) return;
    Drive &d = g_drive[seat];
    if (!d.door && !d.yielded) return;
    give_back(d);
    ++g_stats.released;
}

void watcher_apply(int seat, unsigned char kind, unsigned char frame,
                   const int pos[3]) {
    if (seat < 0 || seat >= kMaxSeats) return;
    Drive &d = g_drive[seat];
    if (kind == kDoorNone || kind >= kDoorKinds || frame == kDoorFrameNone) {
        if (kind != kDoorNone) ++g_stats.no_door;
        watcher_release(seat);
        return;
    }
    if (d.door && !door_alive(d.door)) {           // a level change took it
        d.door = 0;
        d.yielded = false;
    }
    if (d.yielded) return;
    if (!d.door) {
        d.door = nearest_door(pos[0], pos[1], pos[2]);
        if (!d.door) { ++g_stats.no_door; return; }
        d.orig = rd(d.door, kCursor);
        d.written = d.orig;
    } else if (rd(d.door, kCursor) != d.written) {
        /* the local game (or another ghost's drive) moved it: the local
           player is using this door himself. Not ours to write. */
        d.yielded = true;
        ++g_stats.yielded;
        return;
    }
    int f = (int)frame;
    const int n = door_frames(d.door);
    if (n > 0 && f >= n) f = n - 1;
    d.written = f << 12;
    *(int *)((char *)d.door + kCursor) = d.written;
    ++g_stats.applied;
}

}  // namespace ghostdoor
}  // namespace port
