/* hal/char_registry.cpp: see char_registry.h. Never a crash: a bad character
   block is skipped with one stderr line saying why. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "char_registry.h"
#include "mods.h"
#include "char_save.h"

extern "C" {
int host_json_string(const char *text, const char *key, char *out, size_t cap);
}

namespace {

struct Chr {
    std::string mod, key, full, name;
    int base;
    std::string disk[4];   /* body, head with cap, head without cap, metal */
};

std::vector<Chr> g_chr;
int g_active = -2;
std::vector<std::pair<std::string, std::string> > g_skin;   /* rom path, disk path */

const char *const BASE_NAME[4] = { "mario", "luigi", "wario", "yoshi" };

/* The ROM files a skin replaces, per base: body, head with cap, head without
   cap, metal body (only Wario has a loose metal model). Same files as
   hal/fs_mods.cpp's PC_FILES. */
const char *const ROM_FILES[4][4] = {
    { "data/player/mario_model.bmd", "data/player/mario_head_cap.bmd", "data/player/mario_head_nocap.bmd", 0 },
    { "data/player/luigi_model.bmd", "data/player/luigi_head_cap.bmd", "data/player/luigi_head_nocap.bmd", 0 },
    { "data/player/wario_model.bmd", "data/player/wario_head_cap.bmd", "data/player/wario_head_nocap.bmd",
      "data/player/wario_metal_model.bmd" },
    { "data/player/yoshi_model.bmd", "data/player/yoshi_head.bmd", "data/player/yoshi_head_fill.bmd", 0 },
};
const char *const FIELD[4] = { "model", "headCap", "headNoCap", "metal" };

bool plain_key(const std::string &k)
{
    if (k.empty() || k.size() > 32) return false;
    for (size_t i = 0; i < k.size(); ++i) {
        const char c = k[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

/* a path inside the mod folder: relative, plain, no .. */
bool rel_ok(const char *r)
{
    const size_t n = strlen(r);
    if (n == 0 || n > 120 || r[0] == '/' || r[0] == '.') return false;
    for (size_t i = 0; i < n; ++i) {
        const char c = r[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '/'))
            return false;
    }
    if (strstr(r, "..") || strstr(r, "//")) return false;
    return true;
}

bool file_ok(const std::string &p)
{
    const DWORD a = GetFileAttributesA(p.c_str());
    if (a == INVALID_FILE_ATTRIBUTES || (a & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) return false;
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &fa)) return false;
    return fa.nFileSizeHigh == 0 && fa.nFileSizeLow >= 0x40 && fa.nFileSizeLow <= (8u << 20);
}

/* the text of the "character" object in mod.json, braces included; empty if none */
std::string character_block(const char *json)
{
    const char *p = strstr(json, "\"character\"");
    if (!p) return std::string();
    p += 11;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != ':') return std::string();
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != '{') return std::string();
    int depth = 0;
    const char *s = p;
    for (; *p; ++p) {
        if (*p == '"') {
            ++p;
            while (*p && *p != '"') p += (*p == '\\' && p[1]) ? 2 : 1;
            if (!*p) break;
        } else if (*p == '{') {
            ++depth;
        } else if (*p == '}') {
            if (--depth == 0) return std::string(s, (size_t)(p + 1 - s));
        }
    }
    return std::string();
}

void resolve_active(void)
{
    if (g_active != -2) return;
    g_active = -1;
    /* CHARSAVE1: SM64DS_CHAR_SKIN wins ("-" = the plain character on purpose);
       without it the pick saved in save/characters.json is the skin. */
    char saved[96] = "";
    const char *e = getenv("SM64DS_CHAR_SKIN");
    const bool from_env = e && *e;
    if (from_env) {
        if (!strcmp(e, "-")) return;
    } else {
        if (!port_char_save_boot_pick(saved, sizeof saved) || !saved[0]) return;
        e = saved;
    }
    if (port_mods_multiplayer()) {
        fprintf(stderr, "[chars] multiplayer session: the base character is forced, the %s skin is ignored\n",
                from_env ? "SM64DS_CHAR_SKIN" : "saved");
        return;
    }
    int hit = -1, n = 0;
    for (size_t i = 0; i < g_chr.size(); ++i)
        if (g_chr[i].full == e || g_chr[i].key == e) { hit = (int)i; ++n; }
    if (n != 1) {
        fprintf(stderr, "[chars] %s=%s: %s; the plain character is used\n", from_env ? "SM64DS_CHAR_SKIN" : "saved character", e,
                n ? "that key is ambiguous, use modid:key" : "no such character is installed and on");
        return;
    }
    g_active = hit;
    const Chr &c = g_chr[(size_t)hit];
    for (int k = 0; k < 4; ++k)
        if (!c.disk[k].empty() && ROM_FILES[c.base][k])
            g_skin.push_back(std::make_pair(std::string(ROM_FILES[c.base][k]), c.disk[k]));
    fprintf(stderr, "[chars] skin active: %s (%s) on base %s, %u file(s)\n", c.full.c_str(), c.name.c_str(),
            BASE_NAME[c.base], (unsigned)g_skin.size());
}

}  // namespace

extern "C" {

void port_char_offer(const char *mod_id, const char *folder, const char *mod_json)
{
    const std::string blk = character_block(mod_json);
    if (blk.empty()) return;
    char key[64] = "", base[16] = "", name[96] = "";
    host_json_string(blk.c_str(), "key", key, sizeof key);
    host_json_string(blk.c_str(), "base", base, sizeof base);
    host_json_string(blk.c_str(), "name", name, sizeof name);
    if (!plain_key(key)) { fprintf(stderr, "[chars] %s: character key missing or not a-z 0-9 - _; ignored\n", mod_id); return; }
    Chr c;
    c.base = -1;
    for (int b = 0; b < 4; ++b)
        if (!strcmp(base, BASE_NAME[b])) c.base = b;
    if (c.base < 0) { fprintf(stderr, "[chars] %s:%s: base must be mario, luigi, wario or yoshi; ignored\n", mod_id, key); return; }
    c.mod = mod_id;
    c.key = key;
    c.full = std::string(mod_id) + ":" + key;
    c.name = name[0] ? name : key;
    for (size_t i = 0; i < g_chr.size(); ++i)
        if (g_chr[i].full == c.full) { fprintf(stderr, "[chars] %s: listed twice; ignored\n", c.full.c_str()); return; }
    if (g_chr.size() >= 12) { fprintf(stderr, "[chars] %s: more than 12 characters; ignored\n", c.full.c_str()); return; }
    for (int k = 0; k < 4; ++k) {
        char rel[160] = "";
        if (!host_json_string(blk.c_str(), FIELD[k], rel, sizeof rel)) {
            if (k < 3) { fprintf(stderr, "[chars] %s: character.%s is required; ignored\n", c.full.c_str(), FIELD[k]); return; }
            continue;
        }
        if (k == 3 && !ROM_FILES[c.base][3]) {
            fprintf(stderr, "[chars] %s: character.metal is only used with base wario; ignored\n", c.full.c_str());
            continue;
        }
        if (!rel_ok(rel)) { fprintf(stderr, "[chars] %s: character.%s is not a plain path inside the mod; ignored\n", c.full.c_str(), FIELD[k]); return; }
        const std::string disk = std::string(folder) + "/" + rel;
        if (!file_ok(disk)) { fprintf(stderr, "[chars] %s: %s is missing or not a usable file; ignored\n", c.full.c_str(), rel); return; }
        c.disk[k] = disk;
    }
    g_chr.push_back(c);
    fprintf(stderr, "[chars] registered %s (%s) base %s, dormant\n", c.full.c_str(), c.name.c_str(), BASE_NAME[c.base]);
}

int port_char_count(void) { return (int)g_chr.size(); }
const char *port_char_key(int i) { return i >= 0 && i < (int)g_chr.size() ? g_chr[(size_t)i].full.c_str() : 0; }
const char *port_char_name(int i) { return i >= 0 && i < (int)g_chr.size() ? g_chr[(size_t)i].name.c_str() : 0; }
int port_char_base(int i) { return i >= 0 && i < (int)g_chr.size() ? g_chr[(size_t)i].base : -1; }

int port_char_active(void)
{
    port_mods_count();   /* latches the mod list, which registers the characters */
    resolve_active();
    return g_active;
}

int port_char_skin_file(int k, const char **rom, const char **disk)
{
    port_char_active();
    if (k < 0 || k >= (int)g_skin.size()) return 0;
    *rom = g_skin[(size_t)k].first.c_str();
    *disk = g_skin[(size_t)k].second.c_str();
    return 1;
}

}
