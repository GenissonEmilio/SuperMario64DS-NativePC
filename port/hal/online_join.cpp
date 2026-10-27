/* ONLINE JOIN: see hal/online_join.h for the contract. */
#include "online_join.h"

#include <windows.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ntr/backup.h"
#include "host_settings.h"
#include "comms_hub.h"   /* SEAMHUB1: the session record and the teardown */

int out_volume_pct(void);   /* hal/sdat/out_win.cpp: the master volume this process plays at right now (0..100) */

extern "C" {
extern int (*port_title_front_door_hook)(void);   /* hal/title_entry.cpp; tests/walk_window.cpp seats it: start the game again at its front door */
extern int (*port_online_join_tick_hook)(void);   /* hal/title_entry.cpp; seated below */
void port_join_plate_raise_black(void);            /* hal/menu_join_plate.cpp (JOINWIN1) */
}

namespace port {
namespace {

const unsigned kPollMs = 250;
const unsigned kWaitMs = 20000;

OnlineJoinState g_state = kJoinIdle;
std::string g_sentence;
unsigned long long g_stamp = 0;
int g_file = 0;
ULONGLONG g_t0 = 0, g_last_poll = 0;

unsigned long long now_ms() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return t / 10000ull - 11644473600000ull;
}

std::string folder() {
    std::string d = ntr::backup::path();
    const size_t cut = d.find_last_of("/\\");
    d = (cut == std::string::npos ? std::string(".") : d.substr(0, cut)) + "/online";
    return d;
}
std::string req_path() { return folder() + "/join_request.json"; }
std::string plan_path() { return folder() + "/join_plan.json"; }

bool read_file(const std::string &p, std::string *out) {
    FILE *f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    char buf[4096];
    size_t n = std::fread(buf, 1, sizeof buf - 1, f);
    std::fclose(f);
    buf[n] = 0;
    *out = buf;
    return true;
}

/* tiny extraction, enough for the flat files the launcher writes; a value with a backslash is refused */
bool jfind(const std::string &t, const char *key, size_t *at) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = t.find(k);
    if (p == std::string::npos) return false;
    p += k.size();
    while (p < t.size() && (t[p] == ' ' || t[p] == ':' || t[p] == '\t')) ++p;
    *at = p;
    return p < t.size();
}
bool jstr(const std::string &t, const char *key, std::string *v) {
    size_t p;
    if (!jfind(t, key, &p) || t[p] != '"') return false;
    size_t e = t.find('"', p + 1);
    if (e == std::string::npos) return false;
    *v = t.substr(p + 1, e - p - 1);
    return v->find('\\') == std::string::npos;
}
bool jnum(const std::string &t, const char *key, long long *v) {
    size_t p;
    if (!jfind(t, key, &p)) return false;
    char *end = nullptr;
    long long x = std::strtoll(t.c_str() + p, &end, 10);
    if (end == t.c_str() + p) return false;
    *v = x;
    return true;
}
bool jbool(const std::string &t, const char *key, bool *v) {
    size_t p;
    if (!jfind(t, key, &p)) return false;
    if (t.compare(p, 4, "true") == 0) { *v = true; return true; }
    if (t.compare(p, 5, "false") == 0) { *v = false; return true; }
    return false;
}

bool printable(const std::string &s, size_t lo, size_t hi, bool allow_space = false) {
    if (s.size() < lo || s.size() > hi) return false;
    const unsigned char first = allow_space ? 0x20 : 0x21;
    for (char c : s) if ((unsigned char)c < first || (unsigned char)c > 0x7E || c == '"') return false;
    return true;
}
bool hex6(const std::string &s) {
    if (s.size() != 6) return false;
    for (char c : s) if (!std::isxdigit((unsigned char)c)) return false;
    return true;
}

void fail(const char *s) { g_state = kJoinFailed; g_sentence = s; }

/* Apply a validated plan: the same names the launcher's BuildAdventureEnv sets, then the front door. */
bool apply_plan(const std::string &t, int server) {
    std::string relay, code, kind, name, color, shoes;
    long long seat = 255;
    jstr(t, "relay", &relay); jstr(t, "code", &code); jstr(t, "kind", &kind);
    jnum(t, "seat", &seat);
    bool has_name = jstr(t, "name", &name);
    const bool has_cols = jstr(t, "color", &color) && jstr(t, "shoes", &shoes);
    bool ok = printable(relay, 1, 253) && printable(code, 1, 8) && kind == "world" &&
              ((seat >= 0 && seat < 32) || seat == 255);
    /* The player's nickname may hold spaces ("Mario Fan"; the launcher keeps 0x20..0x7E). Leading and trailing spaces are
       trimmed; a name of only spaces is treated as no name (the tag falls back to PLAYER n). Relay and code stay strict. */
    if (has_name) {
        size_t b = name.find_first_not_of(' '), e = name.find_last_not_of(' ');
        name = (b == std::string::npos) ? std::string() : name.substr(b, e - b + 1);
        if (name.empty()) has_name = false;
        else if (!printable(name, 1, 24, true)) ok = false;
    }
    if (has_cols && !(hex6(color) && hex6(shoes))) ok = false;
    if (!ok) { fail("The launcher's answer looked wrong. Try again."); return false; }
    if (!port_title_front_door_hook) { fail("This game cannot join from here."); return false; }

    SetEnvironmentVariableA("SM64DS_ADVENTURE", "1");
    SetEnvironmentVariableA("SM64DS_COMMS_RELAY", relay.c_str());
    SetEnvironmentVariableA("SM64DS_COMMS_CODE", code.c_str());
    SetEnvironmentVariableA("SM64DS_ROOM_KIND", "world");
    SetEnvironmentVariableA("SM64DS_VOICE_ALLOWED", "0");
    if (seat != 255) { char b[8]; std::snprintf(b, sizeof b, "%lld", seat); SetEnvironmentVariableA("SM64DS_HUB_SEAT", b); }
    else SetEnvironmentVariableA("SM64DS_HUB_SEAT", nullptr);
    if (server > 0 && server < 1000) { char w[8]; std::snprintf(w, sizeof w, "%d", server); SetEnvironmentVariableA("SM64DS_WORLD_NO", w); }   /* Rich Presence shows the number */
    if (has_name) SetEnvironmentVariableA("SM64DS_PLAYER_NAME", name.c_str());
    if (has_cols) { SetEnvironmentVariableA("SM64DS_PLAYER_COLOR", color.c_str()); SetEnvironmentVariableA("SM64DS_PLAYER_SHOES", shoes.c_str()); }
    /* PROFILE1: the player's own pick from the in-game popup wins over what the launcher / lobby sent: the saved name is the one every other console
       reads in the identity message, and the saved colour is exported as the same six-hex-digit pair the launcher's own start uses
       (hal/comms_sync.cpp gw_rgb_env), so this player's Yoshi and the figure every other console draws for them show it. Nothing saved (the popup
       always runs before a pick joins, so this is a launcher-started game's own path) leaves the plan's values as they were. */
    {
        const char *own = host_setting_online_name();
        if (own[0]) SetEnvironmentVariableA("SM64DS_PLAYER_NAME", own);
        unsigned body = 0, shoes = 0;
        if (host_setting_online_colours(&body, &shoes)) {
            char b6[8], s6[8];
            std::snprintf(b6, sizeof b6, "%06X", body & 0xFFFFFFu);
            std::snprintf(s6, sizeof s6, "%06X", shoes & 0xFFFFFFu);
            SetEnvironmentVariableA("SM64DS_PLAYER_COLOR", b6);
            SetEnvironmentVariableA("SM64DS_PLAYER_SHOES", s6);
            std::fprintf(stderr, "[join] own profile: name \"%s\" body %s shoes %s\n", own, b6, s6);
        }
    }
#ifndef PORT_ROM_CLEAN
    SetEnvironmentVariableA("SM64DS_JOIN_TEST", nullptr);
#endif
    char v[2] = { (char)('0' + g_file), 0 };
    SetEnvironmentVariableA("SM64DS_BOOT_FILE", v);
    /* JOINFLOW1: THE VOLUME GOES WITH THE PLAYER. The launcher starts a game with SM64DS_VOLUME set to the launcher's own volume, and that variable
       outranks settings.json inside the game for the whole life of the process. A volume the player turned in the in-game settings page since then
       is live in this process (and saved), so the restarted game, which inherits the launcher's number, played at the old volume until the file next
       changed. It is handed the number this process is playing at now. */
    {
        char vol[8];
        std::snprintf(vol, sizeof vol, "%d", out_volume_pct());
        SetEnvironmentVariableA("SM64DS_VOLUME", vol);
    }
    /* JOINFLOW1: this start is a join successor. The restarted game reads (and clears) SM64DS_JOIN_QUIET and runs its quiet entry: a plain "Joining" plate
       over every picture until the adventure is entered (hal/menu_join_plate.cpp). The value is the server number (0 = not known). Games the launcher's own
       Multiplayer window starts never carry it, so they are untouched. */
    {
        char q[8];
        std::snprintf(q, sizeof q, "%d", (server > 0 && server < 1000) ? server : 0);
        SetEnvironmentVariableA("SM64DS_JOIN_QUIET", q);
    }
#ifndef PORT_ROM_CLEAN
    /* SM64DS_JOINFLOW_OFFSCREEN=<bmp name with %d>: the restarted game (only) reads back every picture it presents, from its first one (proof knob, compiled out of the kit) */
    if (const char *dump = std::getenv("SM64DS_JOINFLOW_OFFSCREEN")) {
        SetEnvironmentVariableA("SM64DS_PRESENT_OFFSCREEN", "1");
        SetEnvironmentVariableA("SM64DS_PRESENT_OFFSCREEN_EVERY", "1");
        SetEnvironmentVariableA("SM64DS_PRESENT_OFFSCREEN_BMP", dump);
    }
#endif
    std::fprintf(stderr, "[join] plan applied for server %d, file %d: starting the game again at its front door with the hub environment\n", server, g_file);
    std::fflush(stderr);
#ifndef PORT_ROM_CLEAN
    /* SM64DS_JOIN_TEST_RELAUNCH_FAIL=1: act as if the restart was refused (drill for the clean-up below; compiled out of the kit) */
    const bool relaunched = std::getenv("SM64DS_JOIN_TEST_RELAUNCH_FAIL") ? false : port_title_front_door_hook() != 0;
#else
    const bool relaunched = port_title_front_door_hook() != 0;
#endif
    if (!relaunched) {
        SetEnvironmentVariableA("SM64DS_BOOT_FILE", nullptr);
        /* back to a clean solo process: the hub names set above must not steer the solo file select (online_wanted reads them) */
#ifndef PORT_ROM_CLEAN
        /* SM64DS_JOIN_TEST_RELAUNCH_FAIL=2: keep the names set (replays the behaviour before this clean-up, for the before/after proof) */
        const char *drill = std::getenv("SM64DS_JOIN_TEST_RELAUNCH_FAIL");
        if (!(drill && drill[0] == '2'))
#endif
        online_join_clear_env();
#ifndef PORT_ROM_CLEAN
        char w32[16] = "";
        const DWORD wn = GetEnvironmentVariableA("SM64DS_COMMS_RELAY", w32, sizeof w32);
        std::fprintf(stderr, "[join] restart refused: COMMS_RELAY seen by the process (getenv) %s, by the system block %s"
                            "\n", std::getenv("SM64DS_COMMS_RELAY") ? "SET" : "unset", wn ? "SET" : "unset");
#endif
        fail("The game could not restart to join. Try again.");
        return false;
    }
    g_state = kJoinEntering;
    return true;
}

int tick_hook() {
#ifndef PORT_ROM_CLEAN
    /* SM64DS_JOIN_TEST=<file>: write the request at tick 300 (proof knob, compiled out of the kit; the child clears it) */
    static int ticks = 0;
    static int fired = 0;
    if (!fired) {
        const char *e = std::getenv("SM64DS_JOIN_TEST");
        if (e && *e && ++ticks == 300) {
            fired = 1;
            const int f = (e[0] >= '0' && e[0] <= '2') ? e[0] - '0' : 0;
            online_join_begin(f, "JoinTest");
        }
    }
#endif
    if (g_state == kJoinWaiting) {
#ifndef PORT_ROM_CLEAN
        /* SM64DS_JOIN_TEST_CANCEL=<n>: cancel the wait n ticks after the request (proof knob, compiled out of the kit) */
        static int since = 0;
        ++since;
        const char *c = std::getenv("SM64DS_JOIN_TEST_CANCEL");
        if (c && *c && since == std::atoi(c)) { online_join_cancel(); return 0; }
#endif
        online_join_step();
    }
    /* JOINWIN1: the successor is started. This window goes black for its last pictures (one more tick is drawn black before this process
       stops), so the player sees the file select, then black, then the level: the successor is black from its first picture too. */
    if (g_state == kJoinEntering) {
        static int black_ticks = 0;
        if (black_ticks++ == 0) { port_join_plate_raise_black(); return 0; }
        return 1;
    }
    return 0;
}
const int g_seated = (port_online_join_tick_hook = tick_hook, 1);

}  // namespace

/* Every name apply_plan sets (SM64DS_VOLUME aside, which is always handed over as the live volume). */
static const char *const k_join_env[] = { "SM64DS_ADVENTURE", "SM64DS_COMMS_RELAY", "SM64DS_COMMS_CODE", "SM64DS_ROOM_KIND",
    "SM64DS_VOICE_ALLOWED", "SM64DS_HUB_SEAT", "SM64DS_PLAYER_NAME", "SM64DS_PLAYER_COLOR", "SM64DS_PLAYER_SHOES", "SM64DS_WORLD_NO",
    "SM64DS_JOIN_QUIET", "SM64DS_BOOT_FILE" };

/* JOINFLOW1: take every name a join sets out of this process again (the restart was refused, or the player cancelled on the Joining plate): the
   solo file select, the mod rule and the hub all read these. One list for both clean-up paths. */
void online_join_clear_env() {
    for (const char *n : k_join_env) SetEnvironmentVariableA(n, nullptr);
}

/* JOINWIN1: LEAVING AN ONLINE SESSION BY ITS QUIT ROW IS AN OFFLINE START. The level-clear menu's Save and Quit and the pause menu's quit rows end
   the session through the front door (tests/walk_window.cpp port_front_end_quit_poll), and the restarted game inherits this process's block. In a
   join successor (or a launcher-started online game) that block still holds the hub: the restart came up as an Adventure Online title with every
   mod off and the hub installed again (the player's report: "all my settings and mods turned off"). Every name apply_plan sets (k_join_env above)
   and every online session name the launcher's own start sets (its AlwaysClear and lobby names) goes, so the restart reads the player's own
   settings.json and mods like any offline boot. The volume is handed over as this process plays it now, the way apply_plan does. A solo process
   holds none of these names: nothing is printed and nothing changes for it. */
/* The launcher's own online session names (its AlwaysClear and lobby names). SEAMHUB1 moved the list out of
   port_online_quit_offline_env so the in-place leave clears exactly the names the restart clears. */
static const char *const k_session_env[] = {
    "SM64DS_COMMS_HOST", "SM64DS_COMMS_ROLE", "SM64DS_COMMS_SLOT", "SM64DS_COMMS_PORT", "SM64DS_COMMS_INPUT_DELAY",
    "SM64DS_HUB_PORT", "SM64DS_HUB_DELAY_MS", "SM64DS_PARTY", "SM64DS_PARTY_SHARE_STARS", "SM64DS_NETMODE",
    "SM64DS_COOP", "SM64DS_COOP_HOST", "SM64DS_COOP_SAVE", "SM64DS_COOP_SAVE_NAME", "SM64DS_COOP_TP",
    "SM64DS_ROOM_COLLISION", "SM64DS_ROOM_INTERACT", "SM64DS_ROOM_DAMAGE", "SM64DS_ROOM_TP", "SM64DS_ROOM_STARSHARE",
    "SM64DS_VS_PLAYERS", "SM64DS_VS_NAMES", "SM64DS_VS_COLORS", "SM64DS_VS_NETMODE", "SM64DS_SESSION_DIGEST",
    "SM64DS_SKIP_MENU",
    /* JOINWIN2: the launcher's VS child also carries the arena and its star target (hal/comms_hub.cpp and hal/comms_loopback.cpp read
       both); a VS_MAP left set makes the restart a VS boot instead of the title */
    "SM64DS_VS_MAP", "SM64DS_VS_STAR_TARGET" };

extern "C" int port_online_quit_offline_env(void)
{
    struct List { const char *const *names; size_t count; };
    const List lists[2] = { { k_join_env, sizeof k_join_env / sizeof *k_join_env },
                            { k_session_env, sizeof k_session_env / sizeof *k_session_env } };
#ifndef PORT_ROM_CLEAN
    /* SM64DS_JOINWIN_KEEP_ENV=1: keep every name (replays the behaviour before this clean-up, for the before/after proof; compiled out of the kit) */
    if (const char *keep = std::getenv("SM64DS_JOINWIN_KEEP_ENV")) {
        if (keep[0] == '1') {
            std::fprintf(stderr, "[join] SM64DS_JOINWIN_KEEP_ENV: the restart keeps this process's online names (the behaviour before JOINWIN1)\n");
            return 0;
        }
    }
#endif
    std::string cleared;
    int n = 0;
    char v[8];
    for (const List &l : lists) {
        for (size_t i = 0; i < l.count; ++i) {
            SetLastError(0);
            if (!GetEnvironmentVariableA(l.names[i], v, sizeof v) && GetLastError() == ERROR_ENVVAR_NOT_FOUND) continue;
            SetEnvironmentVariableA(l.names[i], nullptr);
            if (n++) cleared += ' ';
            cleared += l.names[i] + 7;   /* without the SM64DS_ prefix */
        }
    }
    if (!n) return 0;
    std::snprintf(v, sizeof v, "%d", out_volume_pct());
    SetEnvironmentVariableA("SM64DS_VOLUME", v);
    std::fprintf(stderr, "[join] leaving the online session: the restart is an offline start (%d online name(s) cleared: %s; volume %s)\n",
                 n, cleared.c_str(), v);
    std::fflush(stderr);
    return n;
}

/* ==== SEAMHUB1: LEAVING AN ONLINE WORLD IN PLACE (run after093; the one-process plan's M4a) ====================================
   Save and Quit (or a pause quit row) out of an Adventure Online world went back to the solo title by starting the game again
   (port_online_quit_offline_env + the front door): the hub, the mod rule, the online save and the player's online name and colours
   are all chosen when a process starts. SEAMQUIT1 made the offline quit an in-place scene change, the cartridge's own; this does the
   same for the online world, in two halves.

   AT THE QUIT (tests/walk_window.cpp port_front_end_quit_poll asks port_online_leave_refusal, then calls port_online_leave_begin): the
   save is already written (the save menu writes before it asks for scene 1), so the save medium goes back to the single player save
   at once (ntr::backup::select_online, NAME1's run-time switch) and the title the ROM builds next reads its files from it. The quit
   poll then leaves the scene-1 request in place, as for an offline quit, and the ROM's fade and the level-to-scene crossing take the
   game to the title. The hub keeps running through the fade, so the ghosts stay drawn until the Stage and every body are gone.

   AT THE TITLE (the first game frame with the title live; port_sdat_frame_hook, one call per game frame in every loop, chained):
     1. the hub is torn down (comms_hub_teardown: the I/O thread stopped and joined, the socket closed, every seat and queue emptied,
        the install latch forgotten); the ghost layer, the hub body count and presence go back to a process that never joined
        (comms_sync_leave_reset, comms_conductor_leave_reset, hub_presence_leave_reset), with the ROM's body count and input gate
        words put back to the solo values;
     2. the session record is cleared for good (hal/comms_hub.h), and every online name this process holds goes from BOTH copies of the
        environment (the CRT's, which getenv reads, through _putenv_s; the Win32 block a restart would inherit): the same list the
        restart clears (k_join_env + k_session_env). The Adventure flag's env latch is read again (host_setting_adventure_env_reread),
        so the multiplayer mod rule and adventure_ghost_mode() answer as a solo boot does;
     3. the mods are latched again to the player's offline list (hal/mods.cpp port_mods_relatch_after_level, which also counts the
        files a level read as held: SEAMMODS1 note c);
     4. one line says what is now in effect: the save medium, the identity a fresh read gives, the lobby colours, the Loves Me
        setting, the Adventure flag and the mod rule.
   ANYTHING THAT CANNOT BE DONE IN PLACE TAKES THE RESTART, AS BEFORE, saying why in one line ("[seamless] in place refused: <why>;
   restarting"): at the quit a VS / arena / direct-carrier / party / coop session, the lobby colours (read once by hal/fs_mods.cpp), a
   scene-booted process or a loaded code mod; at the title a hub thread that does not stop or a relatch the mods layer refuses (a code
   mod or a held file on the offline list, a change in hd_ui / sound / character mods). The title-time restart goes through
   port_online_leave_restart_hook (tests/walk_window.cpp: the front door, the hand-off wait, the process ends). The join itself still
   restarts (M4b). */
}  // namespace port

namespace ntr { namespace backup { void select_online(int on); bool online_selected(); } }
extern "C" {
extern void (*port_sdat_frame_hook)(void);                    /* hal/sdat/consumer.cpp: one call per game frame */
int port_scene_live_id(void);                                 /* hal/scene_boot.cpp: 1 = the title is the live scene */
int port_mods_relatch_after_level(const char *reason);        /* hal/mods.cpp */
int port_mods_multiplayer(void);
int port_mods_count(void);
const char *port_mods_id(int i);
const char *port_mods_kind(int i);
void host_setting_adventure_env_reread(void);                 /* hal/host_settings.cpp */
int host_setting_lovesme_character(void);
int host_setting_voice_enabled(void);
void port_join_settings_rearm(void);                          /* hal/menu_join_plate.cpp */
int (*port_online_leave_restart_hook)(void) = 0;              /* tests/walk_window.cpp seats it */
}

namespace port {
struct CommsTransport;
const CommsTransport *comms_transport();   /* hal/comms_seam.h: the DS session carrier, null on the hub */
namespace {
int g_leave = 0;                      /* 0 none, 1 asked at the quit, 2 done at the title */
int g_leave_frame = 0;
void (*g_leave_prev_hook)(void) = 0;

/* a name set in either copy of the environment */
bool env_any(const char *n)
{
    const char *c = std::getenv(n);
    if (c && *c) return true;
    char v[4];
    return GetEnvironmentVariableA(n, v, sizeof v) > 0;
}

void leave_restart(const char *why)
{
    std::fprintf(stderr, "[seamless] in place refused: %s; restarting\n", why);
    std::fflush(stderr);
    if (!port_online_leave_restart_hook || !port_online_leave_restart_hook()) {
        std::fprintf(stderr, "[seamless] the restart could not be started (win32 %lu); this process stays on the title\n",
                     (unsigned long)GetLastError());
        std::fflush(stderr);
    }
}

void leave_at_title()
{
    g_leave = 2;
    port_sdat_frame_hook = g_leave_prev_hook;
    std::fprintf(stderr, "[seamless] the title is up (quit at f%d): leaving the online session in this process\n", g_leave_frame);
    if (!comms_hub_teardown("leaving the online world in place")) {
        leave_restart("the hub's I/O thread did not stop");
        return;
    }
    comms_sync_leave_reset();
    comms_conductor_leave_reset();
    hub_presence_leave_reset();
    const int fields = online_session_clear();
    std::string cleared;
    int names = 0;
    for (int pass = 0; pass < 2; ++pass) {
        const char *const *l = pass ? k_session_env : k_join_env;
        const size_t n = pass ? sizeof k_session_env / sizeof *k_session_env : sizeof k_join_env / sizeof *k_join_env;
        for (size_t i = 0; i < n; ++i) {
            if (!env_any(l[i])) continue;
            SetEnvironmentVariableA(l[i], nullptr);
            _putenv_s(l[i], "");
            if (names++) cleared += ' ';
            cleared += l[i] + 7;
        }
    }
    host_setting_adventure_env_reread();
    std::fprintf(stderr, "[seamless] session record cleared (%d field(s)); %d online name(s) cleared from both copies of the "
                 "environment: %s\n", fields, names, names ? cleared.c_str() : "-");
    std::fflush(stderr);
    if (!port_mods_relatch_after_level("leaving the online session")) {
        leave_restart("the mods layer refused the switch back to the offline list");
        return;
    }
    char id[200];
    comms_sync_identity_text(id, sizeof id);
    const int lm = host_setting_lovesme_character();
    std::fprintf(stderr, "[seamless] now solo: save medium %s; identity %s; lobby colours %s; Loves Me %d (%s); adventure %d; "
                 "multiplayer mod rule %d; %d mod(s) on; hub %s\n",
                 ntr::backup::online_selected() ? "THE ONLINE SAVE" : "the single player save", id,
                 env_any("SM64DS_VS_COLORS") ? "SET" : "none (SM64DS_VS_COLORS unset, as a solo boot)",
                 lm, lm ? "settings.json, read the same as a solo boot" : "Yoshi, the ROM", adventure_ghost_mode() ? 1 : 0,
                 port_mods_multiplayer(), port_mods_count(), comms_hub_active() ? "STILL ACTIVE" : "off");
    std::fflush(stderr);
    port_join_settings_rearm();
}

void leave_frame()
{
    if (g_leave_prev_hook) g_leave_prev_hook();
    if (g_leave == 1 && port_scene_live_id() == 1) leave_at_title();
}
}  // namespace
}  // namespace port

/* 0 when this process is not an online session at all (the caller keeps its own answer); else 1, with *why 0 when the quit may
   leave in place, or the reason it may not. */
extern "C" int port_online_leave_refusal(const char **why)
{
    using namespace port;
    static char buf[200];
    *why = 0;
    const bool online = comms_hub_active() || online_session_get(kSessAdventure) || online_session_get(kSessRelay) ||
                        online_session_get(kSessHubPort);
    if (!online) return 0;
    static const char *const k_carrier[] = { "SM64DS_VS_MAP", "SM64DS_VS_PLAYERS", "SM64DS_VS_NETMODE", "SM64DS_COMMS_ROLE",
        "SM64DS_COMMS_HOST", "SM64DS_COMMS_SLOT", "SM64DS_NETMODE", "SM64DS_PARTY", "SM64DS_COOP", "SM64DS_COOP_HOST" };
    for (const char *n : k_carrier)
        if (env_any(n)) {
            std::snprintf(buf, sizeof buf, "a VS, direct-carrier, party or coop session (%s is set)", n);
            *why = buf;
            return 1;
        }
    if (comms_hub_arena()) { *why = "a VS arena session"; return 1; }
    if (comms_hub_active() && comms_hub_room_kind() != kHubRoomWorld) { *why = "a party or coop room"; return 1; }
    if (comms_transport()) { *why = "a DS session transport is installed"; return 1; }
    if (env_any("SM64DS_VS_COLORS")) { *why = "the lobby colours were read once at boot (SM64DS_VS_COLORS)"; return 1; }
    if (env_any("SM64DS_SCENE")) { *why = "a scene-booted process"; return 1; }
    for (int i = 0; i < port_mods_count(); ++i) {
        const char *k = port_mods_kind(i);
        if (k && !std::strcmp(k, "code")) {
            std::snprintf(buf, sizeof buf, "a code mod is loaded (%s) and a plugin cannot be unloaded", port_mods_id(i));
            *why = buf;
            return 1;
        }
    }
    {   /* voice chat keeps its own device and thread (hal/voice_chat.cpp); this leave does not tear them down */
        const char *va = online_session_get(kSessVoiceAllowed);
        if (host_setting_voice_enabled() && !(va && !std::strcmp(va, "0"))) { *why = "voice chat is on for this session"; return 1; }
    }
    if (!port_online_leave_restart_hook) { *why = "this build has no restart for a refusal at the title"; return 1; }
    if (g_leave) { *why = "a leave is already under way"; return 1; }
    return 1;
}

extern "C" void port_online_leave_begin(int frame)
{
    using namespace port;
    g_leave = 1;
    g_leave_frame = frame;
    ntr::backup::select_online(0);
    comms_hub_reports_off();
    g_leave_prev_hook = port_sdat_frame_hook;
    port_sdat_frame_hook = leave_frame;
    std::fprintf(stderr, "[seamless] f%d leaving the online world in this process: the hub stays up through the fade, the title "
                 "tears it down\n", frame);
    std::fflush(stderr);
}

namespace port {

bool online_join_begin(int file, const char *nick) {
    if (file < 0 || file > 2) file = 0;
    g_file = file;
    g_sentence.clear();
    CreateDirectoryA(folder().c_str(), nullptr);
    DeleteFileA(plan_path().c_str());
    DeleteFileA(req_path().c_str());
    std::string n;
    for (const char *p = nick ? nick : ""; *p && n.size() < 16; ++p)
        if ((unsigned char)*p >= 0x20 && (unsigned char)*p < 0x7F && *p != '"' && *p != '\\' && *p != ',') n += *p;
    /* An empty nick stays empty: the launcher then fills in the nickname the player set there (JoinHelper.HandleAsync). */
    g_stamp = now_ms();
    char buf[256];
    std::snprintf(buf, sizeof buf, "{\"v\":1,\"mode\":\"adventure\",\"file\":%d,\"nick\":\"%s\",\"stamp\":%llu}\n", file, n.c_str(), g_stamp);
    const std::string tmp = req_path() + ".tmp";
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f) { fail("The game could not ask for a server. Check that its save folder can be written."); return false; }
    std::fputs(buf, f);
    std::fclose(f);
    if (!MoveFileExA(tmp.c_str(), req_path().c_str(), MOVEFILE_REPLACE_EXISTING)) {
        fail("The game could not ask for a server. Check that its save folder can be written.");
        return false;
    }
    g_state = kJoinWaiting;
    g_t0 = GetTickCount64();
    g_last_poll = 0;
    std::fprintf(stderr, "[join] request written: file %d, nick \"%s\", stamp %llu\n", file, n.c_str(), g_stamp);
    std::fflush(stderr);
    return true;
}

OnlineJoinState online_join_step() {
    if (g_state != kJoinWaiting) return g_state;
    const ULONGLONG now = GetTickCount64();
    if (now - g_last_poll < kPollMs) return g_state;
    g_last_poll = now;
    std::string t;
    if (read_file(plan_path(), &t)) {
        long long stamp = 0;
        if (jnum(t, "stamp", &stamp) && (unsigned long long)stamp == g_stamp) {
            DeleteFileA(plan_path().c_str());
            bool ok = false;
            long long server = 0;
            jnum(t, "server", &server);
            if (jbool(t, "ok", &ok) && ok) {
                apply_plan(t, (int)server);
            } else {
                std::string why;
                if (!jstr(t, "why", &why) || why.empty() || why.size() > 160) why = "No server could be found.";
                /* the lobby's refusal of the name itself (the launcher's bad_nick sentence): say where to fix it */
                else if (why.compare(0, 10, "That name ") == 0 && why.find("lobby will not take") != std::string::npos)
                    why = "The lobby refused that nickname. Set one in the launcher's settings.";
                fail(why.c_str());
            }
            std::fprintf(stderr, "[join] plan %s\n", g_state == kJoinEntering ? "accepted" : g_sentence.c_str());
            std::fflush(stderr);
            return g_state;
        }
        /* stale plan (another stamp): ignored, left for the next begin to replace */
    }
    if (now - g_t0 >= kWaitMs) {
        DeleteFileA(req_path().c_str());
        DeleteFileA(plan_path().c_str());
        fail("No launcher answered. Start the game from the launcher to play online.");
        std::fprintf(stderr, "[join] timed out after 20 s: %s\n", g_sentence.c_str());
        std::fflush(stderr);
    }
    return g_state;
}

void online_join_cancel() {
    if (g_state == kJoinWaiting) {
        DeleteFileA(req_path().c_str());
        DeleteFileA(plan_path().c_str());
        g_state = kJoinCancelled;
        std::fprintf(stderr, "[join] cancelled\n");
        std::fflush(stderr);
    }
}

OnlineJoinState online_join_state() { return g_state; }
const char *online_join_sentence() { return g_sentence.c_str(); }
int online_join_seconds_left() {
    if (g_state != kJoinWaiting) return 0;
    const ULONGLONG el = GetTickCount64() - g_t0;
    return el >= kWaitMs ? 0 : (int)((kWaitMs - el + 999) / 1000);
}

}  // namespace port
