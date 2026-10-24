/* ONLINE JOIN: see hal/online_join.h for the contract. */
#include "online_join.h"

#include <windows.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ntr/backup.h"

extern "C" {
extern int (*port_title_front_door_hook)(void);   /* hal/title_entry.cpp; tests/walk_window.cpp seats it: start the game again at its front door */
extern int (*port_online_join_tick_hook)(void);   /* hal/title_entry.cpp; seated below */
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

bool printable(const std::string &s, size_t lo, size_t hi) {
    if (s.size() < lo || s.size() > hi) return false;
    for (char c : s) if ((unsigned char)c < 0x21 || (unsigned char)c > 0x7E || c == '"') return false;
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
    const bool has_name = jstr(t, "name", &name);
    const bool has_cols = jstr(t, "color", &color) && jstr(t, "shoes", &shoes);
    bool ok = printable(relay, 1, 253) && printable(code, 1, 8) && kind == "world" &&
              ((seat >= 0 && seat < 32) || seat == 255);
    if (has_name && !printable(name, 1, 24)) ok = false;
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
    if (has_name) SetEnvironmentVariableA("SM64DS_PLAYER_NAME", name.c_str());
    if (has_cols) { SetEnvironmentVariableA("SM64DS_PLAYER_COLOR", color.c_str()); SetEnvironmentVariableA("SM64DS_PLAYER_SHOES", shoes.c_str()); }
    SetEnvironmentVariableA("SM64DS_SKIP_INTRO", "1");
    SetEnvironmentVariableA("SM64DS_JOIN_TEST", nullptr);
    char v[2] = { (char)('0' + g_file), 0 };
    SetEnvironmentVariableA("SM64DS_BOOT_FILE", v);
    std::fprintf(stderr, "[join] plan applied for server %d, file %d: starting the game again at its front door with the hub environment\n", server, g_file);
    std::fflush(stderr);
    if (!port_title_front_door_hook()) {
        SetEnvironmentVariableA("SM64DS_BOOT_FILE", nullptr);
        fail("The game could not restart to join. Try again.");
        return false;
    }
    g_state = kJoinEntering;
    return true;
}

int tick_hook() {
    /* SM64DS_JOIN_TEST=<file>: write the request at tick 300 (proof knob; the child clears it) */
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
    /* SM64DS_JOIN_TEST_CANCEL=<n>: cancel the wait n ticks after the request (the cancel row) */
    static int since = 0;
    if (g_state == kJoinWaiting) {
        ++since;
        const char *c = std::getenv("SM64DS_JOIN_TEST_CANCEL");
        if (c && *c && since == std::atoi(c)) { online_join_cancel(); return 0; }
        online_join_step();
    }
    return g_state == kJoinEntering;
}
const int g_seated = (port_online_join_tick_hook = tick_hook, 1);

}  // namespace

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
    if (n.empty()) n = "Player";
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
    std::fprintf(stderr, "[join] request written: file %d, stamp %llu\n", file, g_stamp);
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
        fail("No launcher answered. Start the game from the launcher to play online.");
        std::fprintf(stderr, "[join] timed out after 20 s: %s\n", g_sentence.c_str());
        std::fflush(stderr);
    }
    return g_state;
}

void online_join_cancel() {
    if (g_state == kJoinWaiting) {
        DeleteFileA(req_path().c_str());
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
