/* hal/char_save.cpp: see char_save.h. Never a crash and never a stop: a missing,
   unreadable or hand-edited characters.json is the plain character. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "char_save.h"
#include "char_registry.h"
#include "mods.h"
#include "ntr/backup.h"

extern "C" {
int host_json_string(const char *text, const char *key, char *out, size_t cap);
extern unsigned char data_0209caa0[];
/* seams in files that sit on more targets than this one */
extern int (*port_title_file_start_hook)(int slot);
extern const char *(*port_lk7_skin_key_hook)(void);
extern int (*port_title_front_door_hook)(void);
/* SHELVED1 KeepLives: seams in src/SetPlayerGlobals.c and src/GiveLives.c (_MSC_VER arms), the setting, and the game's own globals */
extern void (*port_lives_restore_hook)(void);
extern void (*port_lives_note_hook)(int lives);
int host_setting_keep_lives(void);
extern signed char data_0209f2f4[];
extern unsigned char data_0209f2d8[];
}

namespace {

const char *const BASE_NAME[4] = { "mario", "luigi", "wario", "yoshi" };

struct Slot {
    bool unlocked;
    std::string key;    /* "" = plain */
    std::string base;   /* "" = never chosen */
    Slot() : unlocked(false) {}
};

void lives_restore(void);   /* SHELVED1, below */
Slot g_slot[3];
int g_last;
bool g_loaded;
int g_env_slot = -2;    /* SM64DS_CHAR_SLOT: -2 unread, -1 absent */
int g_file_picked = 0;  /* the title picked a save file in this process (FIELDPLAY1) */
std::string g_path;

const char *file_path()
{
    if (g_path.empty()) {
        const char *sav = ntr::backup::path();
        const char *over = getenv("SM64DS_SAVE_PATH");
        if (over && *over) {
            g_path = std::string(sav) + ".characters.json";
        } else {
            std::string d = sav;
            const size_t cut = d.find_last_of("/\\");
            g_path = (cut == std::string::npos ? std::string(".") : d.substr(0, cut)) + "/characters.json";
        }
    }
    return g_path.c_str();
}

bool key_ok(const std::string &k)
{
    if (k.size() > 80) return false;
    for (size_t i = 0; i < k.size(); ++i) {
        const char c = k[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ':')) return false;
    }
    return true;
}

void load(void)
{
    if (g_loaded) return;
    g_loaded = true;
    FILE *f = fopen(file_path(), "rb");
    if (!f) return;
    std::string t;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0 && t.size() < (256u << 10)) t.append(buf, n);
    fclose(f);
    const char *p = strstr(t.c_str(), "\"lastSlot\"");
    if (p && (p = strchr(p, ':'))) {
        const int v = atoi(p + 1);
        if (v >= 0 && v <= 2) g_last = v;
    }
    p = strstr(t.c_str(), "\"slots\"");
    if (!p) return;
    for (int i = 0; i < 3; ++i) {
        const char *a = strchr(p, '{');
        if (!a) break;
        const char *b = strchr(a, '}');
        if (!b) break;
        const std::string sub(a, (size_t)(b + 1 - a));
        char key[96] = "", base[16] = "";
        host_json_string(sub.c_str(), "character", key, sizeof key);
        host_json_string(sub.c_str(), "base", base, sizeof base);
        if (key_ok(key)) g_slot[i].key = key;
        g_slot[i].base = base;
        const char *u = strstr(sub.c_str(), "\"unlocked\"");
        if (u && (u = strchr(u, ':'))) {
            do { ++u; } while (*u == ' ');
            g_slot[i].unlocked = !strncmp(u, "true", 4);
        }
        p = b + 1;
    }
}

void save(void)
{
    const std::string path = file_path();
    const std::string tmp = path + ".tmp";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) { fprintf(stderr, "[chars] cannot write %s\n", tmp.c_str()); return; }
    fprintf(f, "{\n  \"version\": 1,\n  \"lastSlot\": %d,\n  \"slots\": [\n", g_last);
    for (int i = 0; i < 3; ++i)
        fprintf(f, "    { \"unlocked\": %s, \"character\": \"%s\", \"base\": \"%s\" }%s\n",
                g_slot[i].unlocked ? "true" : "false", g_slot[i].key.c_str(), g_slot[i].base.c_str(),
                i < 2 ? "," : "");
    fprintf(f, "  ]\n}\n");
    const int ok = fclose(f) == 0;
    if (!ok || !MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        fprintf(stderr, "[chars] could not replace %s\n", path.c_str());
        remove(tmp.c_str());
        return;
    }
    fprintf(stderr, "[chars] wrote %s (last slot %d)\n", path.c_str(), g_last);
}

int env_slot(void)
{
    if (g_env_slot == -2) {
        g_env_slot = -1;
        const char *e = getenv("SM64DS_CHAR_SLOT");
        if (e && e[0] >= '0' && e[0] <= '2' && !e[1]) g_env_slot = e[0] - '0';
    }
    return g_env_slot;
}

/* The title picked a file (hal/title_entry.cpp, once the Stage is staged and the
   picked slot is in the save object). 1 = a new process was started and this
   one must end. */
int on_file_start(int slot)
{
    if (slot < 0 || slot > 2 || port_mods_multiplayer()) return 0;
    load();
    g_file_picked = 1;
    g_env_slot = -1;                              /* the title knows the real slot */
    SetEnvironmentVariableA("SM64DS_CHAR_SLOT", 0);
    lives_restore();                              /* SHELVED1: StartFile's own 4 ran before this pick was known; KeepLives puts the kept count back */
    if (g_last != slot) { g_last = slot; save(); }
    const std::string want = g_slot[slot].key;
    int idx = -1;
    for (int i = 0; i < port_char_count(); ++i)
        if (want == port_char_key(i)) idx = i;
    const int cur = port_char_active();
    const int chr = data_0209caa0[0x41] & 3;
    fprintf(stderr, "[chars] file %c picked: saved skin '%s' (%s), running skin %s, character %s\n",
            'A' + slot, want.c_str(), idx >= 0 ? "installed" : want.empty() ? "plain" : "NOT installed, plain used",
            cur >= 0 ? port_char_key(cur) : "(none)", BASE_NAME[chr]);
    {   /* CHARLOCK1: a saved pick made on a ROM character this file has not earned (flags2 bit 0 Mario, 1 Luigi, 2 Wario: SaveData::
           IsCharacterUnlocked) is not applied: the file plays as its own character, Yoshi on a new file. The entry is kept. */
        const unsigned flags2 = *(const unsigned *)(data_0209caa0 + 8);
        for (int c = 0; c < 3; ++c)
            if (g_slot[slot].base == BASE_NAME[c] && !(flags2 & (1u << c)))
                fprintf(stderr, "[chars] file %c: the saved base '%s' is locked on this file (flags2 0x%08x); playing as %s\n",
                        'A' + slot, BASE_NAME[c], flags2, BASE_NAME[chr]);
    }
    if (idx >= 0 && port_char_base(idx) != chr)
        fprintf(stderr, "[chars] the file's character is %s, the skin is for %s: it shows when that character is played\n",
                BASE_NAME[chr], BASE_NAME[port_char_base(idx)]);
    if (idx == cur) return 0;
    {   /* SEAMSND1 (one-process plan M3b): the title has torn down and the level has not booted, so no level holds the running skin's
           files and the new one is switched in this process (hal/mods.cpp THE SKIN IN PLACE). The restart below is for what cannot be. */
        char why[400];
        if (port_char_reskin_now(idx, "file pick", 0, why, sizeof why)) return 0;
        fprintf(stderr, "[seamless] in place refused: %s; restarting\n", why);
    }
    if (getenv("SM64DS_CHAR_RELAUNCHED") || getenv("SM64DS_SCENE_FRAMES")) {
        fprintf(stderr, "[chars] a skin change at the file pick is needed but %s; carrying on with the running skin\n",
                getenv("SM64DS_CHAR_RELAUNCHED") ? "this process is already the restart" : "this is a frame-budget run");
        return 0;
    }
    if (!port_title_front_door_hook) return 0;
    if (getenv("SM64DS_CHAR_RELAUNCH_DRY")) {
        fprintf(stderr, "[chars] SM64DS_CHAR_RELAUNCH_DRY: would restart on file %c with skin '%s'\n", 'A' + slot,
                idx >= 0 ? port_char_key(idx) : "-");
        return 0;
    }
    char v[2] = { (char)('0' + slot), 0 };
    SetEnvironmentVariableA("SM64DS_BOOT_FILE", v);
    SetEnvironmentVariableA("SM64DS_CHAR_SKIN", idx >= 0 ? port_char_key(idx) : "-");
    SetEnvironmentVariableA("SM64DS_CHAR_RELAUNCHED", "1");
    if (!port_title_front_door_hook()) {
        fprintf(stderr, "[chars] could not restart for the skin (win32 %lu); carrying on with the running skin\n",
                (unsigned long)GetLastError());
        SetEnvironmentVariableA("SM64DS_BOOT_FILE", 0);
        return 0;
    }
    fprintf(stderr, "[chars] restarted for file %c with skin '%s'; this process is ending\n", 'A' + slot,
            idx >= 0 ? port_char_key(idx) : "-");
    return 1;
}

/* Disk save states carry the skin of the moment (savestate.bin + ".skin"). */
const char *state_key(void)
{
    static std::string s;
    const int a = port_char_active();
    s = a >= 0 ? port_char_key(a) : "";
    return s.c_str();
}

/* ---- SHELVED1: THE KeepLives SETTING ---------------------------------------------------------------------------------------------------
   The cartridge starts every file with 4 lives: src/SetPlayerGlobals.c writes 4 to data_0209f2f4 (the life count, GiveLives' only
   variable) when StartFile, PrepareVsMode or the port's level boot call it. With KeepLives on, each file slot's count is kept in
   lives.json beside characters.json (the cartridge save is never touched) and put back right after that 4 for a single player file.
   Both seams are null-by-default function pointers defined in the _MSC_VER arms of src/SetPlayerGlobals.c and src/GiveLives.c, so the DS
   side is unchanged. Not in VS (data_0209f2d8 != 0), not in multiplayer, not on a scripted level boot with no file. A recorded 0 (game
   over) or an unreadable file is "nothing recorded": the file starts with the DS's 4. */
int g_lives[3];
bool g_lives_loaded;

std::string lives_path()
{
    std::string p = file_path();
    const size_t cut = p.rfind("characters.json");
    if (cut != std::string::npos) return p.substr(0, cut) + "lives.json";
    return p + ".lives.json";
}

void lives_load(void)
{
    if (g_lives_loaded) return;
    g_lives_loaded = true;
    FILE *f = fopen(lives_path().c_str(), "rb");
    if (!f) return;
    char buf[512];
    const size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    const char *p = strstr(buf, "\"lives\"");
    if (!p || !(p = strchr(p, '['))) return;
    for (int i = 0; i < 3; ++i) {
        ++p;
        while (*p == ' ') ++p;
        const int v = atoi(p);
        g_lives[i] = v >= 1 && v <= 100 ? v : 0;
        p = strchr(p, ',');
        if (!p) break;
    }
}

void lives_save(void)
{
    const std::string path = lives_path();
    const std::string tmp = path + ".tmp";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) { fprintf(stderr, "[lives] cannot write %s\n", tmp.c_str()); return; }
    fprintf(f, "{\n  \"version\": 1,\n  \"lives\": [%d, %d, %d]\n}\n", g_lives[0], g_lives[1], g_lives[2]);
    const int ok = fclose(f) == 0;
    if (!ok || !MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        fprintf(stderr, "[lives] could not replace %s\n", path.c_str());
        remove(tmp.c_str());
    }
}

/* the file this process is playing, when KeepLives applies to it; -1 when it does not */
int lives_slot(void)
{
    if (!host_setting_keep_lives() || port_mods_multiplayer()) return -1;
    if (data_0209f2d8[0] != 0) return -1;                  /* VS */
    if (!port_char_save_file_known()) return -1;           /* a scripted level boot: no file */
    const int slot = port_char_save_slot();
    return slot >= 0 && slot <= 2 ? slot : -1;
}

void lives_restore(void)
{
    const int slot = lives_slot();
    if (slot < 0) return;
    lives_load();
    const int v = g_lives[slot];
    if (v < 1) {
        fprintf(stderr, "[lives] file %c: nothing kept, the DS's %d\n", 'A' + slot, (int)data_0209f2f4[0]);
        return;
    }
    fprintf(stderr, "[lives] file %c: %d lives kept from the last session (the DS would start with %d)\n", 'A' + slot, v, (int)data_0209f2f4[0]);
    data_0209f2f4[0] = (signed char)v;
}

void lives_note(int lives)
{
    const int slot = lives_slot();
    if (slot < 0) return;
    lives_load();
    const int v = lives >= 1 && lives <= 100 ? lives : 0;
    if (g_lives[slot] == v) return;
    g_lives[slot] = v;
    lives_save();
    fprintf(stderr, "[lives] file %c: %d lives written\n", 'A' + slot, v);
}


int seat(void)
{
    port_title_file_start_hook = on_file_start;
    port_lives_restore_hook = lives_restore;
    port_lives_note_hook = lives_note;
    port_lk7_skin_key_hook = state_key;
    return 1;
}
const int g_seated = seat();

}  // namespace

extern "C" {

int port_char_save_boot_pick(char *out, size_t cap)
{
    load();
    int slot = env_slot();
    if (slot < 0) slot = g_last;
    if (slot < 0 || slot > 2) return 0;
    const Slot &s = g_slot[slot];
    if (s.base.empty() && s.key.empty()) return 0;
    if (s.key.size() + 1 > cap) return 0;
    strcpy(out, s.key.c_str());
    return 1;
}

int port_char_save_slot(void)
{
    const int e = env_slot();
    if (e >= 0) return e;
    const int s = data_0209caa0[0x328];
    return s >= 0 && s <= 2 ? s : 0;
}

void port_char_save_record(int slot, const char *key, int base)
{
    if (slot < 0 || slot > 2 || port_mods_multiplayer()) return;
    load();
    g_slot[slot].key = key && key_ok(key) ? key : "";
    g_slot[slot].base = BASE_NAME[base & 3];
    g_last = slot;
    save();
}

int port_char_save_unlocked(int slot)
{
    if (slot < 0 || slot > 2) return 0;
    load();
    return g_slot[slot].unlocked ? 1 : 0;
}

void port_char_save_set_unlocked(int slot, int on)
{
    if (slot < 0 || slot > 2 || port_mods_multiplayer()) return;
    load();
    g_slot[slot].unlocked = on != 0;
    if (g_slot[slot].base.empty() && g_slot[slot].key.empty()) g_slot[slot].base = BASE_NAME[data_0209caa0[0x41] & 3];
    save();
}

const char *port_char_save_key(int slot)
{
    if (slot < 0 || slot > 2) return "";
    load();
    return g_slot[slot].key.c_str();
}

int port_char_save_file_known(void)
{
    return g_file_picked || env_slot() >= 0;
}

void port_char_save_child_env(int slot, const char *key)
{
    char v[2] = { (char)('0' + (slot < 0 || slot > 2 ? 0 : slot)), 0 };
    SetEnvironmentVariableA("SM64DS_CHAR_SLOT", v);
    SetEnvironmentVariableA("SM64DS_CHAR_SKIN", key && *key ? key : "-");
    /* FIELDPLAY1: a restart for a costume change must come back with the file the player is on (stars, keys, doors), not the default open
       file. Only when this process really is playing a file (the title picked it, or it is itself such a restart); a scripted level boot has no
       file and must never read slot A. The child reads it once and clears this. */
    SetEnvironmentVariableA("SM64DS_CHAR_RELOAD", port_char_save_file_known() ? "1" : 0);
}

}
