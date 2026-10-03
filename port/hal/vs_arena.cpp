// ARENA MODE, the core half, implemented. Run mp1, lane ARENACORE1 (plan A0,
// runs/mp1/out/VSSYNC1/plan.txt). The contract (env, the 'SYNS' layout, the
// calls the other arena lanes make) is hal/vs_arena.h; read it first.
//
// PORT_HOST_ABI: host plumbing below the game. Nothing in src/ knows this file
// exists. It reads the local player's own body and the comms records, writes
// the per-slot key words before the ROM builds PadData, the ROM's "my comms
// slot" byte once per level boot, and the live byte / position of a body
// whose seat has no console. Every one of those writes is gated on arena mode
// being installed, so a solo or lockstep run never reaches them.
//
// ===========================================================================
// WHY THE ARENA NEEDS SO LITTLE OF THE ROM CHANGED
// ===========================================================================
//
// A VS boot with SM64DS_VS_PLAYERS=n and no transport already seats n Yoshi
// bodies in one world and runs the match (VSSYNC1 M4). The ROM's comms read,
// src/func_0203df40.c, takes its solo arm (role byte 0) and
// src/func_0203e0ac.c copies the local key word into all four per-slot
// records, so every body walks to the one pad. Arena mode changes two things
// about that world and nothing else:
//
//   1. MY SLOT IS MY SEAT. Stage::InitResources seats the local player index
//      as data_0209f250 = func_0203da9c(), which returns data_020a0f10, the
//      ROM's "my comms slot" byte. vs_arena_seat_world writes my hub seat into
//      that byte before the level boots, the way the lockstep conductor writes
//      its slot there (hal/comms_conductor.cpp comms_wait_for_session), so the
//      camera, the HUD's "me" and the pad all follow body <seat>.
//   2. MY PAD REACHES MY SLOT ONLY. vs_arena_route_pads clears the other
//      slots' key words after the ROM's solo copy and before the ROM's own
//      PadData builder (src/func_0203bc7c.c) turns them into PadData, so the
//      held / pressed / released words of every other slot are a plain zero
//      pad, built by the ROM's own code.
//
// Nothing waits on anybody: there is no transport, so the conductor answers
// "solo" everywhere and no round is ever exchanged. That is the whole reason
// the own body has zero added input delay at any ping, and the
// SM64DS_ARENA_LATENCY readout below is how a row shows it.
//
// ===========================================================================
// THE STATE STREAM
// ===========================================================================
//
// Every frame of the VS level this console samples its own body into a
// 'SYNS' frame and sends it through the hub (30 Hz while anything a puppet
// reads changes, 10 Hz while it stands still). Received frames are drained
// once per frame into a ring per seat, newest first by the sender's frame,
// with the time the hub's I/O thread took each one off the socket. A1's
// puppets read the rings; the referee keeps its own copy in the relay.

#include "vs_arena.h"

#include "comms_hub.h"
#include "host_settings.h"
#include "player_fields.h"
#include "voice_chat.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
// The per-slot Player pointers, the local player index and the body count.
extern void *data_0209f394[];
extern unsigned char data_0209f250[];
extern unsigned char data_0209f21c;
// The game mode: 1 = VS.
extern unsigned char data_0209f2d8;
// The per-slot live bytes and the ROM's "seats lost" bits (src/func_02030b58.c
// writes both when a player's link goes; hal/star_flow.cpp vs_remove_body is
// the port's lockstep copy of the same two writes).
extern unsigned char data_0209fc5c[];
extern unsigned char data_0209fc58;
// MY COMMS SLOT (hosted by hal/actor_vtables.cpp; read by src/func_0203da9c.c).
extern unsigned char data_020a0f10[];
// The local comms record (key word at +0x0e, written from KEYINPUT by the
// ROM's comms read) and the per-slot records, 0x24 apiece (key word at +0x0e:
// src/func_0203dae4.c reads it for slot i at data_020a1162 + i*0x24).
extern unsigned char data_020a1040[];
extern unsigned char data_020a1154[];
// The 78 Player::State objects, ov002 bss order (hal/pmf3_player_states.cpp
// seats their cells; the order is src/__sinit_ov002_021019d0.c's).
extern unsigned char data_ov002_0210ffec[], data_ov002_02110004[], data_ov002_0211001c[];
extern unsigned char data_ov002_02110034[], data_ov002_0211004c[], data_ov002_02110064[];
extern unsigned char data_ov002_0211007c[], data_ov002_02110094[], data_ov002_021100ac[];
extern unsigned char data_ov002_021100c4[], data_ov002_021100dc[], data_ov002_021100f4[];
extern unsigned char data_ov002_0211010c[], data_ov002_02110124[], data_ov002_0211013c[];
extern unsigned char data_ov002_02110154[], data_ov002_0211016c[], data_ov002_02110184[];
extern unsigned char data_ov002_0211019c[], data_ov002_021101b4[], data_ov002_021101cc[];
extern unsigned char data_ov002_021101e4[], data_ov002_021101fc[], data_ov002_02110214[];
extern unsigned char data_ov002_0211022c[], data_ov002_02110244[], data_ov002_0211025c[];
extern unsigned char data_ov002_02110274[], data_ov002_0211028c[], data_ov002_021102a4[];
extern unsigned char data_ov002_021102bc[], data_ov002_021102d4[], data_ov002_021102ec[];
extern unsigned char data_ov002_02110304[], data_ov002_0211031c[], data_ov002_02110334[];
extern unsigned char data_ov002_0211034c[], data_ov002_02110364[], data_ov002_0211037c[];
extern unsigned char data_ov002_02110394[], data_ov002_021103ac[], data_ov002_021103c4[];
extern unsigned char data_ov002_021103dc[], data_ov002_021103f4[], data_ov002_0211040c[];
extern unsigned char data_ov002_02110424[], data_ov002_0211043c[], data_ov002_02110454[];
extern unsigned char data_ov002_0211046c[], data_ov002_02110484[], data_ov002_0211049c[];
extern unsigned char data_ov002_021104b4[], data_ov002_021104cc[], data_ov002_021104e4[];
extern unsigned char data_ov002_021104fc[], data_ov002_02110514[], data_ov002_0211052c[];
extern unsigned char data_ov002_02110544[], data_ov002_0211055c[], data_ov002_02110574[];
extern unsigned char data_ov002_0211058c[], data_ov002_021105a4[], data_ov002_021105bc[];
extern unsigned char data_ov002_021105d4[], data_ov002_021105ec[], data_ov002_02110604[];
extern unsigned char data_ov002_0211061c[], data_ov002_02110634[], data_ov002_0211064c[];
extern unsigned char data_ov002_02110664[], data_ov002_0211067c[], data_ov002_02110694[];
extern unsigned char data_ov002_021106ac[], data_ov002_021106c4[], data_ov002_021106dc[];
extern unsigned char data_ov002_021106f4[], data_ov002_0211070c[], data_ov002_02110724[];
}  // extern "C"

namespace port {
namespace {

// ---------------------------------------------------------------------------
// THE STATE INDEX. Player+0x370 points at one of the 78 objects above.
// ---------------------------------------------------------------------------
const unsigned char *const kStateObjects[kArenaStates] = {
    data_ov002_0210ffec, data_ov002_02110004, data_ov002_0211001c,
    data_ov002_02110034, data_ov002_0211004c, data_ov002_02110064,
    data_ov002_0211007c, data_ov002_02110094, data_ov002_021100ac,
    data_ov002_021100c4, data_ov002_021100dc, data_ov002_021100f4,
    data_ov002_0211010c, data_ov002_02110124, data_ov002_0211013c,
    data_ov002_02110154, data_ov002_0211016c, data_ov002_02110184,
    data_ov002_0211019c, data_ov002_021101b4, data_ov002_021101cc,
    data_ov002_021101e4, data_ov002_021101fc, data_ov002_02110214,
    data_ov002_0211022c, data_ov002_02110244, data_ov002_0211025c,
    data_ov002_02110274, data_ov002_0211028c, data_ov002_021102a4,
    data_ov002_021102bc, data_ov002_021102d4, data_ov002_021102ec,
    data_ov002_02110304, data_ov002_0211031c, data_ov002_02110334,
    data_ov002_0211034c, data_ov002_02110364, data_ov002_0211037c,
    data_ov002_02110394, data_ov002_021103ac, data_ov002_021103c4,
    data_ov002_021103dc, data_ov002_021103f4, data_ov002_0211040c,
    data_ov002_02110424, data_ov002_0211043c, data_ov002_02110454,
    data_ov002_0211046c, data_ov002_02110484, data_ov002_0211049c,
    data_ov002_021104b4, data_ov002_021104cc, data_ov002_021104e4,
    data_ov002_021104fc, data_ov002_02110514, data_ov002_0211052c,
    data_ov002_02110544, data_ov002_0211055c, data_ov002_02110574,
    data_ov002_0211058c, data_ov002_021105a4, data_ov002_021105bc,
    data_ov002_021105d4, data_ov002_021105ec, data_ov002_02110604,
    data_ov002_0211061c, data_ov002_02110634, data_ov002_0211064c,
    data_ov002_02110664, data_ov002_0211067c, data_ov002_02110694,
    data_ov002_021106ac, data_ov002_021106c4, data_ov002_021106dc,
    data_ov002_021106f4, data_ov002_0211070c, data_ov002_02110724,
};

int state_index_of(const void *body) {
    if (!body) return 0xFF;
    const void *st = player::state_ptr(body);
    for (int i = 0; i < kArenaStates; ++i)
        if (st == (const void *)kStateObjects[i]) return i;
    return 0xFF;
}

enum : int {
    kStHurt = 7, kStHurtWater = 8,            // state_table.json
    kRecStride = 0x24, kRecKeys = 0x0e,       // per-slot comms record
    kNarrowSlots = 4,                         // the ROM's record copy width
    kGraceFrames = 150,                       // 5 s: a seat never heard by
                                              // then has its body held
    kParkUp = 200000 << 12,                   // DROPOUT1's parking spot
    kParkSide = 100000 << 12,
    kTeleportH = 200 << 12,                   // a step no state can make
    kTeleportV = 300 << 12,                   // (M2: 156 / 185 max)
    kRestartBack = 300,                       // a frame jump back this far
                                              // is a restarted sender
    kOffsetBuckets = 8,                       // 8 one-second minima
    kReportEveryMs = 10000,
};

struct SeatRing {
    ArenaSample s[kArenaRing];    // s[0] newest
    int n;
    unsigned epoch;               // the hub join_epoch the ring belongs to
    unsigned samples, rejected;
    int bucket_off[kOffsetBuckets];
    unsigned bucket_sec[kOffsetBuckets];
    bool ever_present;
    bool held;
    int held_pos[3];
};

struct Arena {
    bool installed = false;
    bool hub = false;             // the hub opened (relay or mesh)
    int want_seat = 0xFF;
    int seat = -1;
    int bodies = 0;
    unsigned frame = 0;           // my match frame (SYNS frame)
    bool frame_started = false;
    unsigned start_tick = 0;      // vs_arena_tick count at the first VS frame
    // sender
    ArenaState last{};            // the last SYNS sent
    bool have_last = false;
    int prev_pos[3] = {0, 0, 0};
    bool have_prev = false;
    unsigned since_send = 0;
    bool send_now = false;
    unsigned char sfx[2] = {0, 0};
    int last_state = 15;          // St_Wait until the body has a state
    int nsfx = 0;
    unsigned short ack = 0;
    bool star_lock = false;
    // receive
    SeatRing ring[kArenaSeats];
    unsigned roster_mask = 0, roster_version = 0;
    unsigned last_report = 0;
    ArenaStats st{};
    bool log = false;
};
Arena g;
CRITICAL_SECTION g_cs;            // the rings: A1 / A3 read them from the
bool g_cs_up = false;             // game thread too, but keep it honest

struct Guard {
    Guard() { if (g_cs_up) ::EnterCriticalSection(&g_cs); }
    ~Guard() { if (g_cs_up) ::LeaveCriticalSection(&g_cs); }
};

int env_int(const char *name, int def, int lo, int hi) {
    const char *s = std::getenv(name);
    if (!s || !*s) return def;
    char *end = nullptr;
    const long v = std::strtol(s, &end, 10);
    if (end == s || *end || v < lo || v > hi) {
        std::fprintf(stderr, "[vsarena] %s=%s is out of range %d..%d; using %d\n",
                     name, s, lo, hi, def);
        return def;
    }
    return (int)v;
}

bool env_on(const char *name) {
    const char *s = std::getenv(name);
    return s && *s && !(s[0] == '0' && s[1] == 0);
}

// SEAT PIN, first one present wins (vs_arena.h): the launcher's go plan
// already names role and slot for every member, so it pins the seat with no
// new knob. 0xFF = any free seat.
int seat_pin_from_env(const char **why) {
    *why = "no pin: any free seat";
    if (const char *s = std::getenv("SM64DS_HUB_SEAT"))
        if (*s) { *why = "SM64DS_HUB_SEAT"; return env_int("SM64DS_HUB_SEAT", 0xFF, 0, 15); }
    if (const char *s = std::getenv("SM64DS_COMMS_SLOT"))
        if (*s) { *why = "SM64DS_COMMS_SLOT"; return env_int("SM64DS_COMMS_SLOT", 0xFF, 0, 15); }
    if (const char *r = std::getenv("SM64DS_COMMS_ROLE"))
        if (std::strcmp(r, "parent") == 0) { *why = "SM64DS_COMMS_ROLE=parent"; return 0; }
    return 0xFF;
}

inline unsigned short *slot_keys(int slot) {
    return (unsigned short *)(data_020a1154 + slot * kRecStride + kRecKeys);
}

inline int clamp16(int v) {
    return v > 32767 ? 32767 : v < -32767 ? -32767 : v;
}

int body_seat(const void *ptr) {
    if (!ptr) return -1;
    for (int k = 0; k < kArenaSeats && k < g.bodies; ++k)
        if (data_0209f394[k] == ptr) return k;
    return -1;
}

// ---------------------------------------------------------------------------
// SAMPLING MY BODY into a SYNS frame (vs_arena.h for every field's source).
// ---------------------------------------------------------------------------
void sample_body(void *body, ArenaState *s) {
    const char *b = (const char *)body;
    std::memset(s, 0, sizeof *s);
    s->tag = kArenaTagState;
    s->version = 1;
    s->seat = (unsigned char)g.seat;
    s->character = player::character(body);
    s->frame = g.frame;
    s->x = *player::pos_x(body);
    s->y = *player::pos_y(body);
    s->z = *player::pos_z(body);
    s->yaw = *player::facing(body);
    unsigned short anim = player::anim_id(body);
    s->anim_id = anim < kArenaAnimLimit ? anim : 0;
    const int cur = player::anim_frame(body);
    s->anim_cursor = (unsigned short)((cur < 0 ? 0 : cur) >> 8);
    // NEVER 0xFF ON THE WIRE: the referee drops a SYNS whose state is not one
    // of the 78 (referee.py check_state), so a body between states (no state
    // object yet at level entry) repeats the last one it had.
    int st = state_index_of(body);
    if (st == 0xFF) st = g.last_state;
    else g.last_state = st;
    s->state_idx = (unsigned char)st;
    s->state_step = *(const unsigned char *)(b + 0x6e3);
    // CLAMPED TO 36, the VS hurt window (plan 2.1): the referee drops a SYNS
    // with more (referee.py check_state), and nothing reads past "still
    // invincible" anyway.
    const unsigned inv = player::invincible_timer(body);
    s->invuln = (unsigned char)(inv > kArenaInvulnMax ? kArenaInvulnMax : inv);
    unsigned char pw = 0;
    if (*(const unsigned short *)(b + 0x6ce) & 0x1000) pw |= kArenaFire;
    if (b[0x6ff]) pw |= kArenaWings;
    if (b[0x6f9]) pw |= kArenaMetal;
    if (b[0x6fb]) pw |= kArenaVanish;
    if (b[0x703]) pw |= kArenaMega;
    if (b[0x71a]) pw |= kArenaNoCap;
    s->power = pw;
    // held: who is in my mouth (mObjInMouth +0x360), else anything I carry
    // (mHeldObj +0x358); eaten_by: whose mouth I am in (mEatingPlayer +0xd0)
    void *mouth = *(void *const *)(b + 0x360);
    void *carry = *(void *const *)(b + 0x358);
    void *eater = *(void *const *)(b + 0xd0);
    const int ms = body_seat(mouth);
    s->held = ms >= 0 ? (unsigned char)(ms + 1)
            : (mouth || carry) ? (unsigned char)kArenaHeldObject : 0;
    const int es = body_seat(eater);
    s->eaten_by = es >= 0 ? (unsigned char)(es + 1) : 0;
    unsigned char fl = 0;
    if (g.seat >= 0 && data_0209fc5c[g.seat]) fl |= kArenaLive;
    if (player::on_ground(body)) fl |= kArenaGrounded;
    if (voice_talking()) fl |= kArenaTalking;
    if (st == kStHurt || st == kStHurtWater || b[0x708]) fl |= kArenaInHurt;
    if (b[0x709]) fl |= kArenaNoControl;
    if (g.star_lock) fl |= kArenaStarLock;
    // THE STEP since the previous frame, in 1/64 unit (Fix12 >> 6). A step no
    // state can make in one frame is a teleport: snap, no velocity.
    if (g.have_prev) {
        const int dx = s->x - g.prev_pos[0];
        const int dy = s->y - g.prev_pos[1];
        const int dz = s->z - g.prev_pos[2];
        const int ax = dx < 0 ? -dx : dx, az = dz < 0 ? -dz : dz;
        const int ay = dy < 0 ? -dy : dy;
        if (ax > kTeleportH || az > kTeleportH || ay > kTeleportV) {
            fl |= kArenaTeleport;
        } else {
            s->vx = (short)clamp16(dx >> 6);
            s->vy = (short)clamp16(dy >> 6);
            s->vz = (short)clamp16(dz >> 6);
        }
    }
    s->flags = fl;
    s->ack = g.ack;
    for (int i = 0; i < g.nsfx && i < 2; ++i) s->sfx[i] = g.sfx[i];
    g.prev_pos[0] = s->x; g.prev_pos[1] = s->y; g.prev_pos[2] = s->z;
    g.have_prev = true;
}

// What a puppet reads changed since the last SYNS sent (the cursor and the
// invincibility countdown tick every frame and are left out on purpose).
bool changed(const ArenaState &a, const ArenaState &b) {
    return a.x != b.x || a.y != b.y || a.z != b.z || a.yaw != b.yaw ||
           a.anim_id != b.anim_id || a.state_idx != b.state_idx ||
           a.state_step != b.state_step || a.flags != b.flags ||
           a.power != b.power || a.held != b.held ||
           a.eaten_by != b.eaten_by || a.character != b.character ||
           a.ack != b.ack || (a.invuln == 0) != (b.invuln == 0) ||
           a.sfx[0] != 0;
}

void send_state(void *body) {
    ArenaState s;
    sample_body(body, &s);
    const bool due = !g.have_last || g.send_now || changed(s, g.last) ||
                     g.since_send + 1 >= 3;
    g.nsfx = 0;
    g.sfx[0] = g.sfx[1] = 0;
    if (!due) { ++g.since_send; return; }
    const bool ok = comms_hub_send(&s, (int)sizeof s);
    if (ok) ++g.st.sent; else ++g.st.send_refused;
    g.last = s;
    g.have_last = true;
    g.since_send = 0;
    g.send_now = false;
    if (g.log)
        std::fprintf(stderr, "[vsarena] tx f=%u seat=%d pos=%d,%d,%d v=%d,%d,%d "
                     "yaw=%d anim=%u cur=%u st=%u step=%u fl=%02x pw=%02x "
                     "held=%u eaten=%u inv=%u%s\n", s.frame, g.seat, s.x, s.y,
                     s.z, s.vx, s.vy, s.vz, s.yaw, s.anim_id, s.anim_cursor,
                     s.state_idx, s.state_step, s.flags, s.power, s.held,
                     s.eaten_by, s.invuln, ok ? "" : " REFUSED");
}

// ---------------------------------------------------------------------------
// RECEIVING
// ---------------------------------------------------------------------------
void reset_ring(SeatRing &r, unsigned epoch) {
    r.n = 0;
    r.epoch = epoch;
    r.samples = r.rejected = 0;
    for (int i = 0; i < kOffsetBuckets; ++i) r.bucket_sec[i] = ~0u;
}

void ingest(int seat, const ArenaState &in, unsigned t) {
    if (seat < 0 || seat >= kArenaSeats || seat == g.seat) {
        ++g.st.rejected;
        return;
    }
    ArenaState s = in;
    s.seat = (unsigned char)seat;   // the carrier's stamp is the only truth
    SeatRing &r = g.ring[seat];
    HubSeatInfo hi;
    const unsigned epoch = comms_hub_seat_info(seat, &hi) ? hi.join_epoch : 0;
    if (epoch != r.epoch) reset_ring(r, epoch);
    if (!arena_state_valid(s)) {
        ++r.rejected;
        ++g.st.rejected;
        return;
    }
    if (r.n > 0) {
        const unsigned newest = r.s[0].s.frame;
        if (s.frame <= newest) {
            if (newest - s.frame > (unsigned)kRestartBack) {
                reset_ring(r, epoch);       // that console started again
            } else {
                ++r.rejected;
                ++g.st.out_of_order;
                return;
            }
        }
    }
    const int keep = r.n < kArenaRing ? r.n : kArenaRing - 1;
    std::memmove(&r.s[1], &r.s[0], sizeof(ArenaSample) * (size_t)keep);
    r.s[0].s = s;
    r.s[0].recv_ms = t;
    r.n = keep + 1;
    ++r.samples;
    ++g.st.recvd;
    r.ever_present = true;
    // the clock offset's lower envelope, one minimum per second over 8 s
    const int off = (int)(t - (unsigned)((unsigned long long)s.frame * 1000ull / 30ull));
    const unsigned sec = t / 1000u;
    const int bi = (int)(sec % kOffsetBuckets);
    if (r.bucket_sec[bi] != sec) { r.bucket_sec[bi] = sec; r.bucket_off[bi] = off; }
    else if (off < r.bucket_off[bi]) r.bucket_off[bi] = off;
    if (g.log)
        std::fprintf(stderr, "[vsarena] rx seat=%d f=%u t=%u pos=%d,%d,%d "
                     "v=%d,%d,%d yaw=%d anim=%u st=%u fl=%02x\n", seat,
                     s.frame, t, s.x, s.y, s.z, s.vx, s.vy, s.vz, s.yaw,
                     s.anim_id, s.state_idx, s.flags);
}

void drain() {
    unsigned char buf[64];
    int seat = -1;
    unsigned t = 0;
    int n;
    Guard gd;
    while ((n = comms_hub_recv_t(kHubArenaState, buf, (int)sizeof buf, &seat,
                                 &t)) > 0) {
        if (n != (int)sizeof(ArenaState)) { ++g.st.rejected; continue; }
        ArenaState s;
        std::memcpy(&s, buf, sizeof s);
        ingest(seat, s, t);
    }
}

// ---------------------------------------------------------------------------
// BODIES OF ABSENT SEATS (vs_arena.h). The two writes the cartridge makes
// for a lost player (src/func_02030b58.c: its "lost" bit for seats 0..3 and
// the live byte), plus DROPOUT1's parking spot so the body is nobody's
// nearest player and off the bottom-screen map.
// ---------------------------------------------------------------------------
void hold_body(int k, const char *why) {
    SeatRing &r = g.ring[k];
    if (r.held) return;
    char *c = (char *)data_0209f394[k];
    if (k < 4) data_0209fc58 = (unsigned char)(data_0209fc58 | (1u << k));
    data_0209fc5c[k] = 0;
    r.held = true;
    ++g.st.holds;
    if (!c) {
        std::fprintf(stderr, "[vsarena] f%u seat %d held (%s): no body\n",
                     g.frame, k, why);
        return;
    }
    int *pos = (int *)(c + 0x5c);
    r.held_pos[0] = pos[0]; r.held_pos[1] = pos[1]; r.held_pos[2] = pos[2];
    pos[0] += kParkSide;
    pos[1] += kParkUp;
    pos[2] += kParkSide;
    *(int *)(c + 0x98) = 0;                  // horizontal speed
    *(int *)(c + 0xa8) = 0;                  // vertical speed
    std::fprintf(stderr, "[vsarena] f%u seat %d held (%s): body out of play at "
                 "(%d,%d,%d), parked\n", g.frame, k, why, r.held_pos[0] >> 12,
                 r.held_pos[1] >> 12, r.held_pos[2] >> 12);
}

void release_body(int k) {
    SeatRing &r = g.ring[k];
    if (!r.held) return;
    char *c = (char *)data_0209f394[k];
    if (c) {
        int *pos = (int *)(c + 0x5c);
        pos[0] = r.held_pos[0]; pos[1] = r.held_pos[1]; pos[2] = r.held_pos[2];
    }
    if (k < 4) data_0209fc58 = (unsigned char)(data_0209fc58 & ~(1u << k));
    data_0209fc5c[k] = 1;
    r.held = false;
    ++g.st.releases;
    std::fprintf(stderr, "[vsarena] f%u seat %d back: body in play again\n",
                 g.frame, k);
}

void roster() {
    const unsigned mask = comms_hub_seat_mask() & 0xFFFFu;
    if (mask != g.roster_mask) {
        std::fprintf(stderr, "[vsarena] f%u roster 0x%04x -> 0x%04x (me %d)\n",
                     g.frame, g.roster_mask, mask, g.seat);
        g.roster_mask = mask;
        ++g.roster_version;
    }
    const unsigned since = g.frame;
    for (int k = 0; k < g.bodies && k < kArenaSeats; ++k) {
        if (k == g.seat) continue;
        SeatRing &r = g.ring[k];
        const bool present = (mask >> k) & 1u;
        if (present) {
            r.ever_present = true;
            release_body(k);
        } else if (r.ever_present) {
            hold_body(k, "its console left");
        } else if (since >= (unsigned)kGraceFrames) {
            hold_body(k, "no console in this seat");
        }
    }
}

// ---------------------------------------------------------------------------
// THE LATENCY READOUT (SM64DS_ARENA_LATENCY=1). Works in every VS netmode so
// solo, lockstep and arena can be compared on the same rung: the frame my
// local key word first carries A (the ROM's own comms read filled
// data_020a1040 from KEYINPUT this frame) and the first frame my own body's
// state object changes after it. Read only.
// ---------------------------------------------------------------------------
struct Lat {
    int on = -1;
    unsigned tick = 0;
    unsigned short prev_keys = 0;
    int prev_state = 0xFF;        // my body's state at the end of last frame
    bool armed = false;
    unsigned press = 0;
    int base_state = 0xFF;
} g_lat;

void lat_answer(int me, unsigned at, int from, int to) {
    std::fprintf(stderr, "[vsarena-lat] press A at tick %u, my body (slot %d) "
                 "answered at tick %u: %u frame(s) (state %d -> %d) "
                 "netmode=%s\n", g_lat.press, me, at, at - g_lat.press, from,
                 to, g.installed ? "arena" : "not-arena");
}

// The edge and the answer are both read at the END of a frame (this runs
// after the actor tick), so a press the body answers in the same frame's tick
// reads 0 frames: the state then differs from the one it had at the end of
// the previous frame.
void latency_probe() {
    if (g_lat.on < 0) g_lat.on = env_on("SM64DS_ARENA_LATENCY") ? 1 : 0;
    if (!g_lat.on) return;
    ++g_lat.tick;
    if (data_0209f2d8 != 1) return;
    const int me = (int)data_0209f250[0];
    if (me < 0 || me >= kArenaSeats) return;
    void *body = data_0209f394[me];
    const unsigned short keys =
        *(const unsigned short *)(data_020a1040 + kRecKeys);
    const unsigned short edge = (unsigned short)(keys & ~g_lat.prev_keys);
    g_lat.prev_keys = keys;
    if (!body) return;
    const int st = state_index_of(body);
    if (g_lat.armed) {
        if (st != g_lat.base_state) {
            lat_answer(me, g_lat.tick, g_lat.base_state, st);
            g_lat.armed = false;
        } else if (g_lat.tick - g_lat.press > 30) {
            std::fprintf(stderr, "[vsarena-lat] press A at tick %u: no answer "
                         "from my body (slot %d) in 30 frames (state %d)\n",
                         g_lat.press, me, st);
            g_lat.armed = false;
        }
    } else if (edge & 0x0001u) {
        g_lat.press = g_lat.tick;
        if (st != g_lat.prev_state) {
            lat_answer(me, g_lat.tick, g_lat.prev_state, st);
        } else {
            g_lat.armed = true;
            g_lat.base_state = st;
        }
    }
    g_lat.prev_state = st;
}

void report_line(const char *tag) {
    char rings[16 * 24] = "";
    int o = 0;
    for (int k = 0; k < kArenaSeats && o < (int)sizeof rings - 24; ++k) {
        const SeatRing &r = g.ring[k];
        if (!r.samples && !r.held) continue;
        o += std::snprintf(rings + o, sizeof rings - (size_t)o,
                           "%s%d:%u/%u%s", o ? "," : "", k, r.samples,
                           r.rejected, r.held ? "H" : "");
    }
    std::fprintf(stderr, "[vsarena] %s f=%u seat=%d bodies=%d hub=%d sent=%llu "
                 "refused=%llu recvd=%llu rejected=%llu ooo=%llu "
                 "roster=0x%04x holds=%llu releases=%llu rings=[%s]\n",
                 tag, g.frame, g.seat, g.bodies, comms_hub_state(),
                 g.st.sent, g.st.send_refused, g.st.recvd, g.st.rejected,
                 g.st.out_of_order, g.roster_mask, g.st.holds,
                 g.st.releases, rings);
}

void report_at_exit() { report_line("exit"); }

}  // namespace

// ===========================================================================
// PUBLIC
// ===========================================================================
bool arena_state_valid(const ArenaState &s) {
    if (s.tag != kArenaTagState || s.version != 1) return false;
    if (s.seat >= kArenaSeats) return false;
    const int lim = kArenaPosLimit << 12;
    if (s.x > lim || s.x < -lim || s.y > lim || s.y < -lim || s.z > lim ||
        s.z < -lim) return false;
    if (s.anim_id >= kArenaAnimLimit) return false;
    if (s.state_idx >= kArenaStates && s.state_idx != 0xFF) return false;
    if (s.invuln > kArenaInvulnMax) return false;
    if (s.character > 7) return false;
    if (s.held > kArenaSeats && s.held < kArenaHeldObject) return false;
    if (s.eaten_by > kArenaSeats) return false;
    if (s.power & 0xC0) return false;
    if (s.reserved[0] || s.reserved[1]) return false;
    return true;
}

bool vs_arena_requested() {
    const char *nm = std::getenv("SM64DS_VS_NETMODE");
    if (!nm || !*nm || std::strcmp(nm, "lockstep") == 0) return false;
    if (std::strcmp(nm, "arena") != 0) {
        static bool said = false;
        if (!said) {
            said = true;
            std::fprintf(stderr, "[vsarena] SM64DS_VS_NETMODE=%s is not a "
                         "netmode (arena or lockstep); lockstep it is\n", nm);
        }
        return false;
    }
    const char *vs = std::getenv("SM64DS_VS_MAP");
    if (!vs || !*vs) {
        static bool said = false;
        if (!said) {
            said = true;
            std::fprintf(stderr, "[vsarena] SM64DS_VS_NETMODE=arena without "
                         "SM64DS_VS_MAP: arena mode is VS only; ignored\n");
        }
        return false;
    }
    return true;
}

bool vs_arena_install_from_env() {
    static bool tried = false;
    if (tried) return g.installed;
    tried = true;
    if (!vs_arena_requested()) return false;
    ::InitializeCriticalSection(&g_cs);
    g_cs_up = true;
    for (int k = 0; k < kArenaSeats; ++k) reset_ring(g.ring[k], 0);

    // AN ARENA CONSOLE IS NEVER AN ADVENTURE CONSOLE. The adventure ghost
    // layer (hal/comms_sync.cpp, hal/player_bridges.cpp) keys on
    // adventure_ghost_mode() && comms_hub_active(), and the hub is active
    // here: a player whose settings file has AdventureGhosts on would hand
    // the arena's bodies to the ghost follower. The settings read honours
    // SM64DS_ADVENTURE=0 as "off" and latches it at its first call.
    ::_putenv_s("SM64DS_ADVENTURE", "0");
    if (adventure_ghost_mode())
        std::fprintf(stderr, "[vsarena] WARNING: adventure mode was latched ON "
                     "before the arena install; the ghost layer may touch "
                     "arena bodies\n");

    g.bodies = env_int("SM64DS_VS_PLAYERS", 0, 1, kArenaSeats);
    if (!g.bodies) {
        g.bodies = 4;
        std::fprintf(stderr, "[vsarena] SM64DS_VS_PLAYERS is not set; the arena "
                     "boots 4 bodies\n");
    }
    const char *why = "";
    g.want_seat = seat_pin_from_env(&why);
    // THE VS CARRIER MUST NEVER OPEN IN AN ARENA PROCESS, and main() is not
    // its only installer: the VS env boot (hal/scene_vs_menu.cpp) calls
    // comms_loopback_install_from_env() again, and that installer keys on
    // SM64DS_COMMS_ROLE alone. Measured on the first build: the arena joiner
    // seated a lockstep child there, was "retired" by a host it never had and
    // exited 5. The role has done its one arena job (the seat pin, read
    // above), so it leaves this process's environment here.
    if (const char *role = std::getenv("SM64DS_COMMS_ROLE")) {
        std::fprintf(stderr, "[vsarena] SM64DS_COMMS_ROLE=%s pins the seat "
                     "only; removed from the environment so no VS carrier "
                     "opens in arena mode\n", role);
        ::_putenv_s("SM64DS_COMMS_ROLE", "");
    }
    g.log = env_on("SM64DS_ARENA_LOG");
    g.installed = true;
    g.hub = comms_hub_install_arena(g.want_seat);
    std::fprintf(stderr, "[vsarena] installed: arena mode, %d bodies, seat pin "
                 "%d (%s), hub %s. No DS transport, no rounds: my body takes "
                 "my pad with no added delay.\n", g.bodies, g.want_seat, why,
                 g.hub ? "open" : "NOT OPEN (no SM64DS_COMMS_RELAY or "
                                  "SM64DS_HUB_PORT): this console plays the "
                                  "arena alone");
    std::atexit(report_at_exit);
    return true;
}

bool vs_arena_active() { return g.installed; }

void vs_arena_seat_world() {
    if (!g.installed) return;
    int seat = comms_hub_my_seat();
    if (seat < 0 && g.hub && comms_hub_active()) {
        const int wait_ms = env_int("SM64DS_ARENA_PAIR_MS", 8000, 0, 60000);
        const unsigned t0 = comms_hub_now_ms();
        while (seat < 0 && comms_hub_now_ms() - t0 < (unsigned)wait_ms) {
            ::Sleep(4);
            seat = comms_hub_my_seat();
        }
        std::fprintf(stderr, "[vsarena] the relay %s after %u ms\n",
                     seat >= 0 ? "named my seat" : "did not name a seat",
                     comms_hub_now_ms() - t0);
    }
    if (seat < 0) {
        seat = g.want_seat < kArenaSeats ? g.want_seat : 0;
        std::fprintf(stderr, "[vsarena] no seat from the hub; taking seat %d "
                     "(%s)\n", seat, g.want_seat < kArenaSeats ? "the pin"
                                                              : "default");
    }
    if (seat >= g.bodies)
        std::fprintf(stderr, "[vsarena] ERROR: seat %d has no body (the arena "
                     "boots %d); this console drives nothing\n", seat,
                     g.bodies);
    if (g.seat >= 0 && g.seat != seat)
        std::fprintf(stderr, "[vsarena] my seat moved %d -> %d at a level "
                     "boot\n", g.seat, seat);
    g.seat = seat;
    data_020a0f10[0] = (unsigned char)seat;
    std::fprintf(stderr, "[vsarena] seated: seat %d is my body (data_020a0f10 = "
                 "%d), %d bodies\n", seat, seat, g.bodies);
}

int vs_arena_my_seat() { return g.installed ? g.seat : -1; }

void vs_arena_route_pads() {
    if (!g.installed || g.seat < 0) return;
    for (int i = 0; i < kNarrowSlots; ++i)
        if (i != g.seat) *slot_keys(i) = 0;
}

void vs_arena_before_walk(int frame) {
    (void)frame;
    if (!g.installed) return;
    // THE PUPPET HOLD (A1 PUPPET1) hangs here: the other seats' bodies are
    // held and posed before the actor walk ticks them.
}

void vs_arena_tick(int frame) {
    (void)frame;
    latency_probe();
    if (!g.installed || g.seat < 0) return;
    if (data_0209f2d8 != 1) return;          // the VS level only
    if (!g.frame_started) {
        g.frame_started = true;
        std::fprintf(stderr, "[vsarena] first VS frame: seat %d, %d bodies "
                     "(ROM count %d, me %d)\n", g.seat, g.bodies,
                     (int)data_0209f21c, (int)data_0209f250[0]);
    } else {
        ++g.frame;
    }
    void *body = (g.seat < kArenaSeats) ? data_0209f394[g.seat] : nullptr;
    if (body) send_state(body);
    drain();
    roster();
    const unsigned now = comms_hub_now_ms();
    if (!g.last_report) g.last_report = now;
    if (now - g.last_report >= (unsigned)kReportEveryMs) {
        g.last_report = now;
        report_line("periodic");
    }
}

unsigned vs_arena_frame() { return g.have_last ? g.last.frame : 0; }

int vs_arena_samples(int seat, ArenaSample *out, int cap) {
    if (!g.installed || !out || cap <= 0 || seat < 0 || seat >= kArenaSeats ||
        seat == g.seat) return 0;
    Guard gd;
    const SeatRing &r = g.ring[seat];
    const int n = r.n < cap ? r.n : cap;
    std::memcpy(out, r.s, sizeof(ArenaSample) * (size_t)n);
    return n;
}

bool vs_arena_seat_view(int seat, ArenaSeatView *out) {
    if (!out || seat < 0 || seat >= kArenaSeats) return false;
    std::memset(out, 0, sizeof *out);
    if (!g.installed) return false;
    HubSeatInfo hi;
    if (comms_hub_seat_info(seat, &hi)) {
        out->present = hi.present;
        out->join_epoch = hi.join_epoch;
        out->rtt_ms = hi.rtt_ms;
    } else {
        out->rtt_ms = -1;
    }
    Guard gd;
    const SeatRing &r = g.ring[seat];
    out->has_state = r.n > 0 && r.epoch == out->join_epoch;
    out->samples = r.samples;
    out->rejected = r.rejected;
    out->body_held = r.held;
    if (r.n > 0) {
        out->newest_frame = r.s[0].s.frame;
        out->newest_age_ms = comms_hub_now_ms() - r.s[0].recv_ms;
        const unsigned now_sec = comms_hub_now_ms() / 1000u;
        bool have = false;
        int best = 0;
        for (int i = 0; i < kOffsetBuckets; ++i) {
            if (r.bucket_sec[i] == ~0u || now_sec - r.bucket_sec[i] >= 8u)
                continue;
            if (!have || r.bucket_off[i] < best) { best = r.bucket_off[i]; have = true; }
        }
        out->clock_offset_ms = have ? best : 0;
    }
    return true;
}

unsigned vs_arena_roster_mask() { return g.roster_mask; }
unsigned vs_arena_roster_version() { return g.roster_version; }

void vs_arena_note_sfx(unsigned id) {
    if (!g.installed || !id || id > 255 || g.nsfx >= 2) return;
    g.sfx[g.nsfx++] = (unsigned char)id;
}
void vs_arena_set_ack(unsigned short seq) { g.ack = seq; }
void vs_arena_set_star_lock(bool on) { g.star_lock = on; }
void vs_arena_send_now() { g.send_now = true; }

bool vs_arena_referee_now(unsigned *ref_ms, int *rtt_ms) {
    if (!g.installed) return false;
    return comms_hub_referee_clock(ref_ms, rtt_ms, nullptr);
}

int vs_arena_state_index(const void *body) { return state_index_of(body); }

bool vs_arena_body_held(int seat) {
    return g.installed && seat >= 0 && seat < kArenaSeats && g.ring[seat].held;
}

ArenaStats vs_arena_stats() {
    ArenaStats s = g.st;
    s.installed = g.installed;
    s.seat = g.seat;
    s.bodies = g.bodies;
    s.frame = g.frame;
    return s;
}

void vs_arena_report(const char *tag) {
    if (!g.installed) return;
    report_line(tag ? tag : "report");
}

}  // namespace port
