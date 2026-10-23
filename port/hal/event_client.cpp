// HOST EVENTS, the game's side. See hal/event_client.h for the whole picture.
// Run hunt8, lane EVENTS1. PROTOTYPE.
//
// One frame hook (port_event_frame) does everything: it takes events from the
// hub (hub_on_kind, delivered by comms_hub_pump inside port_hub_presence_frame
// just before this runs), keeps the list of what is live against the relay's
// clock, applies each effect to this console, and ends it when its time is up.
//
// THE CLOCK. Every datagram carries the time REMAINING, not a start time, so
// no two machines have to agree on a wall clock: a console sets "this ends at
// now + remaining" each second and the relay's own countdown is the truth. The
// error is one network trip (tens of ms) either way.
//
// THE GAME TICK. Chaos counts game ticks (Stage::Behavior calls, 60 a second
// on the DS, whatever the picture rate), not frames, so its two seconds are
// two seconds of game time at 60, 144 or 240 fps, and a pause or a menu that
// stops the game stops the count.

#include "event_client.h"
#include "comms_hub.h"
#include "hub_presence.h"
#include "host_settings.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
extern void *data_0209f394[];
extern unsigned char data_0209f250;
extern unsigned char data_0209caa0[];     // the save data in memory (flags1 at +4)
extern signed char data_0209f2f8;
extern signed char data_02092120;
extern short data_02092144[];
extern unsigned char data_0209f294[];     // star select
extern unsigned char data_0209f2c4[];     // pause menu
extern unsigned char data_0209d660[];     // message window
extern unsigned char data_0209fc48[];     // cutscene
extern unsigned char data_0209b454[];     // fade / scene change
extern unsigned char data_0209f2d8[];     // stage mode (0 adventure, 1 VS, 2 script)
extern unsigned char data_02075768[];     // the course sound rows: [level*3+2] = the level song, signed
extern unsigned short data_ov002_020ff170[];   // per-character speed factor, 1.0 = 0x1000
unsigned port_stage_behavior_calls(void);
unsigned char NumStars(void);
int _ZN6Player4HealEi(void *player, int amt);
int _ZN6Player4HurtERK7Vector3j5Fix12IiEjjj(void *self, const void *pos, unsigned dmg_units, int speed,
                                           unsigned char a4, unsigned char a5, unsigned char a6);
void _ZN8Particle6System9NewSimpleEj5Fix12IiES2_S2_(unsigned id, int x, int y, int z);
void func_0201267c(unsigned id, const void *pos);       // the bob-omb's explosion sound
void *port_debug_spawn_at(unsigned id, unsigned param, int x, int y, int z, int yaw, int area);
void _ZN5Sound22LoadAndSetMusic_Layer3Ej(unsigned musicId);
void _ZN5Sound22StopLoadedMusic_Layer3Ev(void);
}

namespace port {

bool event_decode(const unsigned char *d, int len, EventWire *out) {
    if (!d || len != 96) return false;
    if (std::memcmp(d, "SYNZ", 4) != 0 || d[4] != 1) return false;
    const int type = d[6];
    if (type < 1 || type > 6) return false;
    const int tlen = d[22];
    if (tlen > 70) return false;
    std::memcpy(&out->id, d + 8, 4);
    std::memcpy(&out->remain_ms, d + 12, 4);
    std::memcpy(&out->total_ms, d + 16, 4);
    out->type = type;
    out->extra = d[7];
    out->arg = (int)d[20] | ((int)d[21] << 8);
    for (int i = 0; i < tlen; ++i) {
        const unsigned char c = d[24 + i];
        out->text[i] = (c >= 32 && c < 127) ? (char)c : '?';
    }
    out->text[tlen] = 0;
    return true;
}

namespace {

enum : int { kMsg = 1, kMusic = 2, kMod = 3, kClear = 4, kLeader = 5, kWinner = 6 };
enum : int { kModSpeed = 1, kModOneHit = 2, kModChaos = 3, kModRace = 4, kModCastle = 5 };
enum : int { kRecs = 8, kDead = 16 };
enum : int { kChaosTicks = 120, kChaosCooldown = 240, kRaceStars = 50 };
// The player is "in control" this many game ticks after the level is up (the opening camera is over), and has moved or 900 ticks went by.
enum : int { kReadyTicks = 300, kReadyForceTicks = 900 };
enum : unsigned { kAnnounceMs = 6000, kWinMs = 10000 };

struct Rec {
    bool     used;
    bool     applied;
    unsigned id;
    int      type, arg, extra;
    char     text[73];
    unsigned start_ms, end_ms, total_ms;
};

Rec      g_rec[kRecs];
unsigned g_dead[kDead];
int      g_dead_n = 0;
unsigned g_clear_id = 0;
bool     g_registered = false;
int      g_off = -1;               // SM64DS_EVENTS=0
int      g_log = -1;
EventStats g_st = {};

// effect state
bool           g_speed_on = false;
int            g_speed_idx = -1;
unsigned short g_speed_orig = 0, g_speed_set = 0;
bool           g_music_on = false;
int            g_hp_prev = -99;
unsigned       g_tick_prev = 0;
bool           g_tick_seeded = false;
int            g_still = 0, g_cool = 0, g_boom_pending = 0;
int            g_px = 0, g_py = 0, g_pz = 0;
bool           g_pos_have = false;
bool           g_castle = false;   // a castle-unlocked event is live (the door hooks read it)
unsigned       g_win_id = 0;       // the relay's winner record we already showed
char           g_win_name[40] = {};
int            g_win_stars = 0;
unsigned       g_win_until = 0;    // 0 = no banner
int            g_lead_stars = -1;  // the relay's last word on who leads (-1 none)
char           g_lead_name[40] = {};
unsigned       g_lead_id = 0;
// "in control": the level has been up for a while, the player has moved (or a long time passed), no cutscene / menu / fade
int            g_up_ticks = 0, g_up_key = -1;
bool           g_moved = false;
int            g_sx = 0, g_sy = 0, g_sz = 0;
unsigned       g_log_tick = 0;
unsigned       g_frame = 0;

#ifndef PORT_ROM_CLEAN
// proof-only (compiled out of the kit)
struct Fake { int frame; int type, arg, secs; char text[73]; bool done; };
Fake g_fake[8];
#endif
int  g_nfake = -1;

// SM64DS_EVENTS=0, the kill switch (kept in the kit)
bool env_on() {
    if (g_off < 0) {
        const char *e = std::getenv("SM64DS_EVENTS");
        g_off = (e && e[0] == '0') ? 1 : 0;
        if (g_off) std::fprintf(stderr, "[event] SM64DS_EVENTS=0: host events are off in this game\n");
    }
    return g_off == 0;
}
// the kill switch AND the player's own "Online events" switch
bool enabled() { return env_on() && host_setting_online_events() != 0; }
bool logging() {
    if (g_log < 0) {
        const char *e = std::getenv("SM64DS_EVENT_LOG");
        g_log = (e && *e && *e != '0') ? ((*e == '2') ? 2 : 1) : 0;
    }
    return g_log >= 1;
}
unsigned log_every() { return g_log == 2 ? 5u : 30u; }

char *my_body() {
    const int me = (int)data_0209f250;
    if (me < 0 || me >= 32) return nullptr;
    return (char *)data_0209f394[me];
}
int rd32(const char *b, int off) { int v; std::memcpy(&v, b + off, 4); return v; }

// The game is in a state where standing still means nothing (the star_popup
// exit gate's list).
unsigned busy_mask() {
    return (data_0209f294[0] ? 1u : 0u) | (data_0209f2c4[0] ? 2u : 0u) | (data_0209d660[0] ? 4u : 0u) | (data_0209fc48[0] ? 8u : 0u) |
           (data_0209b454[0] ? 16u : 0u) | (data_0209f2d8[0] == 2 ? 32u : 0u);
}
bool busy() { return busy_mask() != 0; }

bool dead_id(unsigned id) {
    for (int i = 0; i < g_dead_n && i < kDead; ++i) if (g_dead[i] == id) return true;
    return false;
}
void note_dead(unsigned id) {
    g_dead[g_dead_n % kDead] = id;
    ++g_dead_n;
}

const char *mod_name(int arg, int extra, char *buf, int cap) {
    switch (arg) {
    case kModSpeed:  return "TRIPLE SPEED";
    case kModOneHit: return "ONE HIT MODE";
    case kModChaos:  return "CHAOS MODE";
    case kModCastle: return "CASTLE UNLOCKED";
    case kModRace:
        std::snprintf(buf, (size_t)cap, "RACE TO %d STARS", extra > 0 ? extra : kRaceStars);
        return buf;
    }
    return "EVENT";
}

// ---- effects -------------------------------------------------------------------------------------------------------

void speed_apply(bool on) {
    char *b = my_body();
    if (on && b) {
        const int idx = rd32(b, 8) & 0xff;
        if (idx > 7) return;
        const unsigned short v = data_ov002_020ff170[idx];
        if (g_speed_on && idx == g_speed_idx && v == g_speed_set) return;
        if (!(g_speed_on && idx == g_speed_idx)) g_speed_orig = v;
        g_speed_idx = idx;
        g_speed_set = (unsigned short)(g_speed_orig * 3u);
        data_ov002_020ff170[idx] = g_speed_set;
        if (!g_speed_on)
            std::fprintf(stderr, "[event] triple speed ON: character %d factor 0x%04x -> 0x%04x\n", idx,
                         (unsigned)g_speed_orig, (unsigned)g_speed_set);
        g_speed_on = true;
    } else if (!on && g_speed_on) {
        if (g_speed_idx >= 0 && data_ov002_020ff170[g_speed_idx] == g_speed_set)
            data_ov002_020ff170[g_speed_idx] = g_speed_orig;
        std::fprintf(stderr, "[event] triple speed OFF: character %d factor back to 0x%04x\n", g_speed_idx,
                     (unsigned)g_speed_orig);
        g_speed_on = false;
    }
}

void music_apply(bool on, int sublevel) {
    if (on && !g_music_on) {
        if (sublevel < 0 || sublevel >= 0x34) return;
        const int bgm = (signed char)data_02075768[sublevel * 3 + 2];
        if (bgm < 0) {
            std::fprintf(stderr, "[event] music: sublevel %d has no song of its own\n", sublevel);
            return;
        }
        _ZN5Sound22LoadAndSetMusic_Layer3Ej((unsigned)bgm);
        g_music_on = true;
        std::fprintf(stderr, "[event] music ON: sublevel %d song %d (layer 3)\n", sublevel, bgm);
    } else if (!on && g_music_on) {
        _ZN5Sound22StopLoadedMusic_Layer3Ev();
        g_music_on = false;
        std::fprintf(stderr, "[event] music OFF: the level's own song is back\n");
    }
}

void explode(char *b) {
    int pos[3] = { rd32(b, 0x5c), rd32(b, 0x60), rd32(b, 0x64) };
    const int camsp[3] = { rd32(b, 0x74), rd32(b, 0x78), rd32(b, 0x7c) };
    // the bob-omb's own explosion: five particle effects over the head and its sound
    for (unsigned id = 0x14; id <= 0x18; ++id)
        _ZN8Particle6System9NewSimpleEj5Fix12IiES2_S2_(id, pos[0], pos[1] + 0x64000, pos[2]);
    func_0201267c(0x10c, camsp);
    // a coin out of his head (the coin's own launch behaviour 2, as the bob-omb's coin)
    const void *coin = port_debug_spawn_at(0x120, 2, pos[0], pos[1] + 0x78000, pos[2], 0, (int)data_02092120);
    // damage no health meter survives: the ordinary death, and with it the Bowser iris-out
    _ZN6Player4HurtERK7Vector3j5Fix12IiEjjj(b, pos, 16u, 0xc000, 1, 0, 1);
    ++g_st.explosions;
    g_boom_pending = 4;
    std::fprintf(stderr, "[event] CHAOS: mario explodes at (%d,%d,%d) coin=%s hp=%d\n", pos[0] >> 12, pos[1] >> 12,
                 pos[2] >> 12, coin ? "out" : "refused", (int)(signed char)(data_02092144[data_0209f250 & 15] >> 8));
}

void kill_now(char *b, const char *why) {
    const int hp = (int)(signed char)(data_02092144[data_0209f250 & 15] >> 8);
    if (hp <= 0) return;
    _ZN6Player4HealEi(b, -(hp * 0x100));
    ++g_st.kills;
    std::fprintf(stderr, "[event] %s: hp %d -> %d\n", why, hp,
                 (int)(signed char)(data_02092144[data_0209f250 & 15] >> 8));
}

// ---- the list ------------------------------------------------------------------------------------------------------

Rec *find_rec(unsigned id) {
    for (auto &r : g_rec) if (r.used && r.id == id) return &r;
    return nullptr;
}

void end_rec(Rec &r, const char *why) {
    std::fprintf(stderr, "[event] id %u type %d arg %d ends (%s)\n", r.id, r.type, r.arg, why);
    if (r.type == kMod && r.arg == kModSpeed) speed_apply(false);
    if (r.type == kMod && r.arg == kModOneHit) g_hp_prev = -99;
    if (r.type == kMod && r.arg == kModChaos) { g_still = 0; g_pos_have = false; }
    if (r.type == kMod && r.arg == kModCastle && g_castle) {
        g_castle = false;
        std::fprintf(stderr, "[event] castle unlocked OFF: the save's own doors (save flags1 %08x)\n", *(unsigned *)(data_0209caa0 + 4));
    }
    if (r.type == kMusic) music_apply(false, 0);
    note_dead(r.id);
    r.used = false;
    ++g_st.ended;
}

void end_all(const char *why) {
    for (auto &r : g_rec) if (r.used) end_rec(r, why);
    speed_apply(false);
    music_apply(false, 0);
    g_castle = false;
}

// The player turned Online events off (or the world is gone): undo every effect but do NOT remember the records as finished, so turning
// the switch back on picks up what still runs from the relay's next repeat.
void drop_all(const char *why) {
    bool any = false;
    for (auto &r : g_rec) any = any || r.used;
    for (auto &r : g_rec) {
        if (!r.used) continue;
        if (r.type == kMod && r.arg == kModOneHit) g_hp_prev = -99;
        if (r.type == kMod && r.arg == kModChaos) { g_still = 0; g_pos_have = false; }
        r.used = false;
    }
    speed_apply(false);
    music_apply(false, 0);
    g_castle = false;
    g_win_until = 0;
    g_lead_stars = -1;
    if (any) std::fprintf(stderr, "[event] events dropped (%s)\n", why);
}

void put_event(const EventWire &w, unsigned now) {
    if (w.type == kClear) {
        if (w.id != g_clear_id) {
            g_clear_id = w.id;
            std::fprintf(stderr, "[event] CLEAR from the host (id %u)\n", w.id);
            end_all("host cleared");
        }
        return;
    }
    if (w.type == kLeader) {
        // the relay's word on who leads the race (every few seconds)
        if (w.arg != g_lead_stars || std::strcmp(g_lead_name, w.text) != 0)
            std::fprintf(stderr, "[event] RACE LEADER (relay word, record %u): %s with %d stars\n", w.id, w.text, w.arg);
        g_lead_id = w.id;
        g_lead_stars = w.arg;
        std::snprintf(g_lead_name, sizeof g_lead_name, "%s", w.text);
        return;
    }
    if (w.type == kWinner) {
        // the relay's word on who won (decided once, repeated until the race record runs out): the ONLY way a console shows a winner
        if (w.id == g_win_id) return;
        g_win_id = w.id;
        g_win_stars = w.arg;
        std::snprintf(g_win_name, sizeof g_win_name, "%s", w.text[0] ? w.text : "SOMEONE");
        g_win_until = now + (w.remain_ms ? w.remain_ms : 1000u);
        for (auto &x : g_rec) if (x.used && x.type == kMod && x.arg == kModRace) end_rec(x, "race decided");
        g_lead_stars = -1;
        std::fprintf(stderr, "[event] RACE WINNER (relay word, record %u): %s with %d stars, shown for %u ms\n", w.id, g_win_name,
                     w.arg, w.remain_ms);
        return;
    }
    if (dead_id(w.id)) return;
    Rec *r = find_rec(w.id);
    if (!r) {
        for (auto &x : g_rec) if (!x.used) { r = &x; break; }
        if (!r) { ++g_st.dropped_bad; return; }
        std::memset(r, 0, sizeof *r);
        r->used = true;
        r->id = w.id;
        r->type = w.type;
        r->arg = w.arg;
        r->extra = w.extra;
        std::memcpy(r->text, w.text, sizeof r->text);
        r->start_ms = now;
        r->total_ms = w.total_ms;
        std::fprintf(stderr, "[event] NEW id %u type %d arg %d for %u ms%s%s\n", w.id, w.type, w.arg, w.remain_ms,
                     w.text[0] ? " text: " : "", w.text);
        ++g_st.applied;
    }
    r->end_ms = now + w.remain_ms;     // the relay's countdown is the clock
}

void on_event(int, const unsigned char *d, int len, void *) {
    EventWire w;
    if (!event_decode(d, len, &w)) { ++g_st.dropped_bad; return; }
    ++g_st.rx;
    if (!enabled()) return;      // the relay handed it out; this console's own switch says no
    put_event(w, comms_hub_now_ms());
}

#ifdef PORT_ROM_CLEAN
void read_fakes() { g_nfake = 0; }
#else
void read_fakes() {
    g_nfake = 0;
    const char *e = std::getenv("SM64DS_EVENT_FAKE");
    if (!e) return;
    char buf[512];
    std::strncpy(buf, e, sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    for (char *tok = std::strtok(buf, ";"); tok && g_nfake < 8; tok = std::strtok(nullptr, ";")) {
        Fake &f = g_fake[g_nfake];
        std::memset(&f, 0, sizeof f);
        char text[80] = {};
        const int n = std::sscanf(tok, "%d:%d:%d:%d:%72[^\n]", &f.frame, &f.type, &f.arg, &f.secs, text);
        if (n >= 4) {
            std::strncpy(f.text, text, 72);
            ++g_nfake;
        }
    }
}
#endif

#ifndef PORT_ROM_CLEAN
// proof knob (dev builds only): SM64DS_EVENT_TESTHURT=<tick>[,<tick>...] hurts the local body by one wedge at those game ticks, or
// SM64DS_EVENT_TESTHURT=file:<path> one wedge each time the harness creates that file. A control for one hit mode: without the modifier
// health goes 8 -> 7, with it the console dies. It runs BEFORE the kill switch and the Online events switch are looked at, so a console
// that takes no events can be hurt the same way as one that does.
void testhurt_poll() {
    static unsigned th[4]; static int nth = -1, thi = 0;
    static char hurt_file[260] = {};
    static unsigned prev = 0; static bool seeded = false;
    if (nth < 0) {
        nth = 0;
        if (const char *e = std::getenv("SM64DS_EVENT_TESTHURT")) {
            if (std::strncmp(e, "file:", 5) == 0) {
                std::strncpy(hurt_file, e + 5, sizeof hurt_file - 1);
            } else {
                char buf[128]; std::strncpy(buf, e, sizeof buf - 1); buf[sizeof buf - 1] = 0;
                for (char *t = std::strtok(buf, ","); t && nth < 4; t = std::strtok(nullptr, ",")) th[nth++] = (unsigned)std::atoi(t);
            }
        }
    }
    if (nth == 0 && !hurt_file[0]) return;
    char *b = my_body();
    const unsigned ticks = port_stage_behavior_calls();
    const unsigned dt = seeded ? ticks - prev : 0;
    prev = ticks;
    seeded = true;
    if (!b || dt == 0) return;
    const int hp = (int)(signed char)(data_02092144[data_0209f250 & 15] >> 8);
    bool hurt_now = false;
    if (thi < nth && ticks >= th[thi]) { ++thi; hurt_now = true; }
    if (hurt_file[0] && hp > 0 && GetFileAttributesA(hurt_file) != INVALID_FILE_ATTRIBUTES) { DeleteFileA(hurt_file); hurt_now = true; }
    if (!hurt_now) return;
    int pos[3] = { rd32(b, 0x5c), rd32(b, 0x60) + 0x1000, rd32(b, 0x64) };
    const int r = _ZN6Player4HurtERK7Vector3j5Fix12IiEjjj(b, pos, 1u, 0x8000, 1, 0, 1);
    std::fprintf(stderr, "[event] TESTHURT at tick %u: Hurt returned %d, hp %d -> %d\n", ticks, r, hp,
                 (int)(signed char)(data_02092144[data_0209f250 & 15] >> 8));
}
#endif

bool hub_ok() {
    return comms_hub_active() && !comms_hub_arena() && comms_hub_state() == kHubLive &&
           comms_hub_room_kind() == kHubRoomWorld;
}

}  // namespace

EventStats event_stats() { return g_st; }

// "m:ss" of the time a record has left
static int secs_left(const Rec &r, unsigned now) { return (int)((r.end_ms - now) / 1000u) + 1; }

void event_view(EventView *out) {
    out->n = 0;
    if (!enabled()) return;
    const unsigned now = comms_hub_now_ms();
    auto add = [&](const char *s, int style) {
        if (out->n >= kEventLines) return;
        std::snprintf(out->line[out->n], kEventLineMax, "%s", s);
        out->style[out->n] = style;
        ++out->n;
    };
    // the race winner: the relay's word, the same text on every console
    if (g_win_until != 0 && (int)(now - g_win_until) < 0) {
        char line[80];
        std::snprintf(line, sizeof line, "%s WINS THE RACE", g_win_name);
        add(line, 0);
    }
    // the newest message first, then one line per modifier
    const Rec *msg = nullptr;
    for (const auto &r : g_rec) if (r.used && r.type == kMsg && (!msg || r.start_ms > msg->start_ms)) msg = &r;
    if (msg) add(msg->text, 0);
    for (const auto &r : g_rec) {
        if (!r.used) continue;
        char line[80], nb[32];
        if (r.type == kMod && now - r.start_ms < kAnnounceMs) {
            const unsigned tot = (r.total_ms + 500) / 1000;
            const char *nm = mod_name(r.arg, r.extra, nb, sizeof nb);
            if (tot >= 120 && tot % 60 == 0)
                std::snprintf(line, sizeof line, "%s EVENT: %u MINUTES", nm, tot / 60);
            else if (tot == 60)
                std::snprintf(line, sizeof line, "%s EVENT: 1 MINUTE", nm);
            else
                std::snprintf(line, sizeof line, "%s EVENT: %u SECONDS", nm, tot);
            add(line, 0);
        }
    }
    for (const auto &r : g_rec) {
        if (!r.used) continue;
        const int left = secs_left(r, now);
        char line[80], nb[32];
        if (r.type == kMod) {
            const char *nm = mod_name(r.arg, r.extra, nb, sizeof nb);
            if (r.arg == kModRace) {
                if (g_lead_stars >= 0)
                    std::snprintf(line, sizeof line, "%s  you %d  %s %d  %d:%02d", nm, (int)NumStars(), g_lead_name, g_lead_stars,
                                  left / 60, left % 60);
                else
                    std::snprintf(line, sizeof line, "%s  you %d  %d:%02d", nm, (int)NumStars(), left / 60, left % 60);
            } else {
                std::snprintf(line, sizeof line, "%s  %d:%02d", nm, left / 60, left % 60);
            }
            add(line, 1);
        } else if (r.type == kMusic) {
            std::snprintf(line, sizeof line, "EVENT SONG  %d:%02d", left / 60, left % 60);
            add(line, 2);
        }
    }
}

}  // namespace port

extern "C" int port_event_castle_live(void) { return port::g_castle ? 1 : 0; }

extern "C" void port_event_frame(void) {
    using namespace port;
    ++g_frame;
#ifndef PORT_ROM_CLEAN
    testhurt_poll();
#endif
    if (!env_on()) { g_castle = false; return; }
    if (std::getenv("SM64DS_EVENT_LOG") && (g_frame == 1 || g_frame % 600 == 0))
        std::fprintf(stderr, "[event] hook alive: call %u body=%p hub=%d fakes=%d setting=%d\n", g_frame, (void *)my_body(),
                     comms_hub_active() ? 1 : 0, g_nfake, host_setting_online_events());

    if (comms_hub_active() && !g_registered) {
        hub_on_kind(kHubEvent, on_event, nullptr);
        g_registered = true;
    }

    // the player's own switch: off = nothing applied, nothing shown, nothing remembered as finished
    if (!host_setting_online_events()) {
        drop_all("Online events is off");
        return;
    }

    // fake events (the single-console knob, dev builds only)
    if (g_nfake < 0) read_fakes();
    const unsigned now = comms_hub_now_ms();
    bool fake_live = false;
#ifndef PORT_ROM_CLEAN
    for (int i = 0; i < g_nfake; ++i) {
        Fake &f = g_fake[i];
        fake_live = true;
        if (!f.done && (int)g_frame >= f.frame) {
            f.done = true;
            EventWire w;
            std::memset(&w, 0, sizeof w);
            w.id = 9000u + (unsigned)i;
            w.type = f.type;
            w.arg = f.arg;
            w.total_ms = w.remain_ms = (unsigned)f.secs * 1000u;
            std::memcpy(w.text, f.text, sizeof w.text);
            put_event(w, now);
        }
    }
#endif

    const bool live = hub_ok();
    if (!live && !fake_live) {
        bool any = false;
        for (auto &r : g_rec) any = any || r.used;
        if (any || g_speed_on || g_music_on || g_castle) end_all("the world is gone");
        g_win_until = 0;
        g_lead_stars = -1;
        return;
    }

    // time's up
    for (auto &r : g_rec)
        if (r.used && (int)(now - r.end_ms) >= 0) end_rec(r, "time is up");
    if (g_win_until != 0 && (int)(now - g_win_until) >= 0) g_win_until = 0;

    char *b = my_body();
    const bool in_level = b != nullptr;

    bool want_speed = false, want_onehit = false, want_chaos = false, want_castle = false;
    const Rec *music = nullptr;
    for (const auto &r : g_rec) {
        if (!r.used) continue;
        if (r.type == kMod) {
            want_speed  = want_speed  || r.arg == kModSpeed;
            want_onehit = want_onehit || r.arg == kModOneHit;
            want_chaos  = want_chaos  || r.arg == kModChaos;
            want_castle = want_castle || r.arg == kModCastle;
        } else if (r.type == kMusic) {
            music = &r;
        }
    }

    // castle unlocked: the door hooks (hal/event_castle.cpp) ask port_event_castle_live; nothing is written to the save
    if (want_castle != g_castle) {
        std::fprintf(stderr, "[event] castle unlocked %s (save flags1 %08x)\n", want_castle ? "ON: every star and key door opens" : "OFF: the save's own doors", *(unsigned *)(data_0209caa0 + 4));
        g_castle = want_castle;
    }

    // triple speed: re-asserted every frame (an overlay reload puts the table back)
    if (in_level && want_speed) speed_apply(true);
    else if (!want_speed) speed_apply(false);

    // music: starts in a level, ends with its record
    if (music && in_level && !g_music_on) music_apply(true, music->arg);

    const unsigned ticks = port_stage_behavior_calls();
    unsigned dt = 0;
    if (g_tick_seeded) dt = ticks - g_tick_prev;
    g_tick_prev = ticks;
    g_tick_seeded = true;

    if (!in_level) {
        g_up_ticks = 0;
        g_up_key = -1;
        g_moved = false;
    }

    if (in_level) {
        const int hp = (int)(signed char)(data_02092144[data_0209f250 & 15] >> 8);

        // "in control": the level is up (not the opening camera), nothing modal, and the player moved at least once (or a long time went
        // by). A level change, a death and a respawn all take the body away for a while, which starts this over.
        const int key = ((int)data_0209f2f8 << 8) | (int)(unsigned char)data_02092120;
        const int x0 = rd32(b, 0x5c), y0 = rd32(b, 0x60), z0 = rd32(b, 0x64);
        if (key != g_up_key) { g_up_key = key; g_up_ticks = 0; g_moved = false; }
        if (dt > 0 && !busy()) g_up_ticks += (int)dt;
        // "moved" is judged from where the player stood when the opening time ran out, so the first fall onto the floor does not count
        if (g_up_ticks < kReadyTicks || busy()) { g_moved = false; g_sx = x0; g_sy = y0; g_sz = z0; }
        {
            const int dx = x0 - g_sx, dy = y0 - g_sy, dz = z0 - g_sz;
            if ((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + (dz < 0 ? -dz : dz) > 0x20000) g_moved = true;
        }
        const bool ready = g_up_ticks >= kReadyTicks && (g_moved || g_up_ticks >= kReadyForceTicks) && !busy() && hp > 0;

        if (logging() && dt > 0 && (ticks - g_log_tick) >= log_every()) {
            g_log_tick = ticks;
            std::fprintf(stderr, "[event] me tick=%u pos=(%d,%d,%d) hspeed=%d hp=%d ready=%d up=%d busy=%x\n", ticks, rd32(b, 0x5c), rd32(b, 0x60),
                         rd32(b, 0x64), rd32(b, 0x98), hp, ready ? 1 : 0, g_up_ticks, busy_mask());
        }


        // one hit: any drop in health is the end of it (only once the player is in control)
        if (want_onehit) {
            if (ready && g_hp_prev != -99 && hp > 0 && hp < g_hp_prev) kill_now(b, "ONE HIT");
            g_hp_prev = ready ? hp : -99;
        }

        // an explosion that did not take (invincible frames): finish it
        if (g_boom_pending > 0) {
            --g_boom_pending;
            if (g_boom_pending == 0 && hp > 0) kill_now(b, "CHAOS finish");
        }

        // chaos: two seconds of game time without moving, never before the player is in control
        if (want_chaos && dt > 0) {
            const int x = rd32(b, 0x5c), y = rd32(b, 0x60), z = rd32(b, 0x64);
            if (g_cool > 0) { g_cool -= (int)dt; g_still = 0; }
            else if (!ready) { g_still = 0; }
            else {
                const int dx = x - g_px, dy = y - g_py, dz = z - g_pz;
                const int moved = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + (dz < 0 ? -dz : dz);
                if (!g_pos_have || moved > 0x1000) g_still = 0;     // more than one unit this tick
                else g_still += (int)dt;
                if (g_still >= kChaosTicks) {
                    explode(b);
                    g_still = 0;
                    g_cool = kChaosCooldown;
                }
            }
            g_px = x; g_py = y; g_pz = z; g_pos_have = true;
        }
    }
}
