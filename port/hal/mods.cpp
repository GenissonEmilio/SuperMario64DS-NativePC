/* hal/mods.cpp: mods that are folders, the list, and the files/ layer
   (run mods1, lane MODFILES1). The contract is runs/mods1/in/MODS_CONTRACT.md;
   this file is its game side for files/. textures_hd/ is applied by
   ntr/hdtex.cpp and palettes/ by hal/fs_mods.cpp, both through the list below.

   THE LIST (mods.h). Boot-latched: EnabledMods is read once. An id is skipped,
   with one stderr line saying why, when it is not [a-z0-9-] of 1..64
   characters, is a Windows reserved device name, repeats an earlier id, has no
   mods/<id>/ folder, or has a mod.json that is missing, unreadable, not a JSON
   object, has no "id" or an "id" other than the folder's, or a "kind" other
   than "files" or "code". Never a crash: nothing below aborts.

   THE FILES LAYER. A mod's files/<rom path> replaces the ROM file with that
   path for every read. The path is looked up exactly (case-insensitively for
   the loose files, whose catalog is hal/fs.cpp's files.tsv; archive members by
   their NARC's own name table, the way hal/fs.cpp's port_fs_interior_id does).
   A path that names no ROM file is ignored with a line. Two mods replacing the
   same file: the later in EnabledMods wins, and a line says so.

   It rides the hooks hal/fs.cpp already has: the FILTER (rewrites a file's
   decompressed master as it is served; a returned buffer REPLACES the stock
   one, any size) and the CLAIMS (the ids the card must pre-serve, because the
   game targets read through the ROM's own loaders and the NARC the ROM mounted;
   an archive member's NARC is rebuilt around the new member by hal/fs.cpp's
   existing code, which is why no size limit applies here either).

   WHY THIS LINK IS LAST. hal/fs_mods.cpp installs its link from an ordinary
   initializer and hal/stage_geom.cpp chains onto it from .CRT$XCV; this one is
   .CRT$XCW, after both, and calls what it chained to FIRST and then replaces
   the buffer. A mod's file sits ON TOP of the port's own mods for the same
   file, for two reasons. The port's mods (the palette settings, Loves Me, the
   stage-geometry folder) patch bytes at offsets they found in the STOCK file,
   so running them over a different file would write colours into the wrong
   place; and a player who installed a mod that replaces a file chose that file
   on purpose, which outranks a global setting. The port's mods have already
   run on the stock bytes and those bytes are simply dropped.

   A mod file that starts with "LZ77" (a file copied straight out of the
   extracted game is stored that way when the cartridge stores it that way) is
   decompressed here: the filter's contract is the DECOMPRESSED master, and
   hal/fs.cpp re-wraps the result the way the stock file is held.

   KNOWN GAP, RECORDED NOT FIXED: save states. A state saved with one mod set
   and loaded with another can mix resident files: the state holds the files
   the game had loaded (a model, an animation) as they were when it was saved,
   while the files the game loads after the restore come from the mod set the
   running game booted with. A model from one set can then meet an animation
   from the other. A future fix is a mods signature (the ordered list of ids
   with each mod's version) in the save-state header, refused or warned about
   on a load that does not match. Nothing here writes or checks one yet. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <string>
#include <vector>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "mods.h"
#include "ntr/hdtex.h"

typedef unsigned char u8;
typedef unsigned int u32;

extern "C" {
int host_setting_adventure_ghosts(void);   /* hal/host_settings.cpp: settings.json AdventureGhosts, SM64DS_ADVENTURE */
int host_setting_enabled_mods(char *bundle, size_t bcap, char *arr, size_t acap);  /* 1 got, 0 none, -1 too long */
int host_json_is_object(const char *text);
int host_json_string(const char *text, const char *key, char *out, size_t cap);
const char *port_fs_catalog_path(unsigned file_id);   /* hal/fs.cpp */
unsigned port_fs_interior_id_nocase(const char *want); /* hal/fs.cpp */
unsigned port_fs_member_claim_cap(void);              /* hal/fs.cpp */
extern unsigned (*port_fs_mod_map)(unsigned fileID);
extern u32 (*port_fs_mod_filter)(unsigned fileID, u8 **data, u32 size);
extern void (*port_fs_mod_claims)(void (*claim)(unsigned fileID));
}

/* Mods are for single player only. A multiplayer session is any of: the
   launcher's lobby / VS match env (comms role and relay knobs, the VS-match
   knobs), SM64DS_PARTY set to a nonzero number (read the way comms_sync.cpp
   reads it), or the Adventure hub (settings.json AdventureGhosts, or the
   SM64DS_ADVENTURE override), which is the shared adventure with other players.
   The ONE definition: the mod list below asks it, and the plugin loader only
   sees the emptied list. host_setting_adventure_ghosts() runs the settings
   loader (load_once), which reads the file itself and never calls back into
   mods, and host_setting_enabled_mods() reads the file on its own, so there is
   no loop, and load_once's one-shot guard makes any second call a no-op. */
extern "C" int port_mods_multiplayer(void)
{
    static const char *const K[] = {
        "SM64DS_COMMS_ROLE", "SM64DS_COMMS_RELAY", "SM64DS_COMMS_HOST", "SM64DS_COMMS_CODE",
        "SM64DS_COMMS_SLOT", "SM64DS_VS_MAP", "SM64DS_VS_MODE", "SM64DS_VS_PLAYERS", 0 };
    for (int i = 0; K[i]; ++i) {
        const char *v = getenv(K[i]);
        if (v && *v) return 1;
    }
    const char *party = getenv("SM64DS_PARTY");
    if (party && atoi(party) != 0) return 1;
    return host_setting_adventure_ghosts() != 0;
}

namespace {

struct Mod {
    std::string id, folder, kind, game;
    int api;
};

std::vector<Mod> g_mods;
std::string g_bundle;
int g_latched;

bool reserved_name(const std::string &id)
{
    static const char *const R[] = { "con", "prn", "aux", "nul", 0 };
    for (int i = 0; R[i]; ++i)
        if (id == R[i]) return true;
    if (id.size() == 4 && (id.compare(0, 3, "com") == 0 || id.compare(0, 3, "lpt") == 0)
        && id[3] >= '0' && id[3] <= '9')
        return true;
    return false;
}

bool valid_id(const std::string &id, const char **why)
{
    if (id.empty() || id.size() > 64) { *why = "id is empty or longer than 64 characters"; return false; }
    for (size_t i = 0; i < id.size(); ++i) {
        const char c = id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            *why = "id has a character outside a-z 0-9 and -";
            return false;
        }
    }
    if (reserved_name(id)) { *why = "id is a reserved Windows name"; return false; }
    return true;
}

char *slurp_small(const std::string &path)
{
    FILE *f = fopen(path.c_str(), "rb");
    long n;
    char *b;
    size_t got;
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    n = ftell(f);
    if (n < 0 || n > (1 << 20)) { fclose(f); return 0; }
    fseek(f, 0, SEEK_SET);
    b = (char *)malloc((size_t)n + 1);
    if (!b) { fclose(f); return 0; }
    got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = 0;
    return b;
}

bool dir_exists(const std::string &p)
{
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

std::string g_summary_loaded, g_summary_skipped;
int g_nskipped;

void skip(const std::string &id, const char *why)
{
    fprintf(stderr, "[mods] skipped %s: %s\n", id.c_str(), why);
    if (g_nskipped++) g_summary_skipped += ", ";
    g_summary_skipped += id + " (" + why + ")";
}

void latch(void)
{
    char bundle[1024];
    static char arr[65536];   /* about 900 ids of 64 characters; static to keep the stack small */
    const char *kill;
    if (g_latched) return;
    g_latched = 1;

    kill = getenv("SM64DS_MODS");
    if (kill && strcmp(kill, "0") == 0) {
        fprintf(stderr, "[mods] SM64DS_MODS=0: every mod is off for this run\n");
        return;
    }
    if (port_mods_multiplayer()) {
        fprintf(stderr, "[mods] multiplayer match: every mod is off\n");
        return;
    }
    const int have = host_setting_enabled_mods(bundle, sizeof bundle, arr, sizeof arr);
    g_bundle = bundle;
    if (have < 0) {
        fprintf(stderr, "[mods] EnabledMods too long (over %u characters); no mods are on this run (bundle %s)\n",
                (unsigned)(sizeof arr - 1), bundle);
        return;
    }
    if (!have) {
        /* No EnabledMods: nothing on. Said once so a support log shows it. */
        fprintf(stderr, "[mods] none enabled (bundle %s)\n", bundle);
        return;
    }

    /* Only string elements directly in the array count. Numbers, true, false
       and null are passed over; a nested array or object is passed over whole,
       with every string inside it. */
    std::vector<std::string> ids;
    {
        int depth = 0;
        for (const char *p = arr; *p; ++p) {
            if (*p == '[' || *p == '{') { ++depth; continue; }
            if (*p == ']' || *p == '}') { --depth; continue; }
            if (*p != '"') continue;
            const char *e = ++p;
            while (*e && *e != '"') e += (*e == '\\' && e[1]) ? 2 : 1;
            if (*e != '"') break;
            if (depth == 1)
                ids.push_back(std::string(p, (size_t)(e - p)));
            p = e;
        }
    }

    for (size_t i = 0; i < ids.size(); ++i) {
        const std::string &id = ids[i];
        const char *why = 0;
        if (!valid_id(id, &why)) {
            /* a bad id is never put into a path */
            skip(id.size() > 40 ? id.substr(0, 40) + "..." : id, why);
            continue;
        }
        bool dup = false;
        for (size_t k = 0; k < g_mods.size(); ++k)
            if (g_mods[k].id == id) dup = true;
        if (dup) { skip(id, "listed twice (the first entry counts)"); continue; }

        const std::string folder = g_bundle + "/mods/" + id;
        if (!dir_exists(folder)) { skip(id, "no mods/<id> folder"); continue; }
        char *text = slurp_small(folder + "/mod.json");
        if (!text) { skip(id, "mod.json missing or unreadable"); continue; }
        if (!host_json_is_object(text)) { free(text); skip(id, "mod.json is not a JSON object"); continue; }
        char jid[96] = "", kind[16] = "", game[48] = "", api[16] = "";
        host_json_string(text, "id", jid, sizeof jid);
        host_json_string(text, "kind", kind, sizeof kind);
        host_json_string(text, "game", game, sizeof game);
        host_json_string(text, "api", api, sizeof api);
        free(text);
        if (!jid[0]) { skip(id, "mod.json has no id"); continue; }
        if (id != jid) { skip(id, "mod.json id is not the folder name"); continue; }
        if (strcmp(kind, "files") && strcmp(kind, "code")) { skip(id, "mod.json kind is not files or code"); continue; }

        Mod m;
        m.id = id;
        m.folder = folder;
        m.kind = kind;
        m.game = game;
        m.api = atoi(api);
        if (!g_mods.empty()) g_summary_loaded += ", ";
        g_mods.push_back(m);
        g_summary_loaded += id + " (" + kind + ")";
    }
    fprintf(stderr, "[mods] %u loaded: %s | %d skipped%s%s\n", (unsigned)g_mods.size(),
            g_mods.empty() ? "-" : g_summary_loaded.c_str(), g_nskipped,
            g_nskipped ? ": " : "", g_summary_skipped.c_str());
    fflush(stderr);
}

/* ---- files/ ------------------------------------------------------------ */

std::unordered_map<unsigned, std::string> g_override;   /* file id -> path on disk */
int g_override_built;

std::string lower(const std::string &s)
{
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i) r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}

void walk(const std::string &root, const std::string &rel, std::vector<std::string> &out, int depth)
{
    WIN32_FIND_DATAA fd;
    const std::string pat = root + (rel.empty() ? "" : "/" + rel) + "/*";
    HANDLE h = FindFirstFileA(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::string n = fd.cFileName;
        if (n == "." || n == "..") continue;
        const std::string r = rel.empty() ? n : rel + "/" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            /* a junction or symlink inside a mod folder could point anywhere
               on the disk; the mod's own files are real files and folders */
            fprintf(stderr, "[mods] %s/%s is a link, skipped\n", root.c_str(), r.c_str());
            continue;
        }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth < 12) walk(root, r, out, depth + 1);
        } else {
            out.push_back(r);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

void build_overrides(void)
{
    if (g_override_built) return;
    g_override_built = 1;
    latch();
    if (g_mods.empty()) return;

    std::unordered_map<std::string, unsigned> loose;
    for (unsigned id = 0; id < 4096; ++id) {
        const char *cp = port_fs_catalog_path(id);
        if (cp) loose[lower(cp)] = id;
    }
    std::unordered_map<unsigned, std::string> owner;   /* id -> mod id, for the "wins" line */
    unsigned total_members = 0;
    int warned = 0;
    for (size_t m = 0; m < g_mods.size(); ++m) {
        const std::string root = g_mods[m].folder + "/files";
        std::vector<std::string> rels;
        walk(root, "", rels, 0);
        unsigned used = 0, ignored = 0, members = 0;
        for (size_t k = 0; k < rels.size(); ++k) {
            unsigned id = 0;
            std::unordered_map<std::string, unsigned>::const_iterator it = loose.find(lower(rels[k]));
            if (it != loose.end()) id = it->second;
            else id = port_fs_interior_id_nocase(rels[k].c_str());
            if (!id) {
                fprintf(stderr, "[mods] %s: files/%s is not a ROM file, ignored\n",
                        g_mods[m].id.c_str(), rels[k].c_str());
                ++ignored;
                continue;
            }
            std::unordered_map<unsigned, std::string>::const_iterator o = owner.find(id);
            if (o != owner.end())
                fprintf(stderr, "[mods] files/%s is replaced by %s and by %s; %s wins (later in the list)\n",
                        rels[k].c_str(), o->second.c_str(), g_mods[m].id.c_str(), g_mods[m].id.c_str());
            if (id >= 0x8000 && !owner.count(id)) ++members;
            owner[id] = g_mods[m].id;
            g_override[id] = root + "/" + rels[k];
            ++used;
        }
        if (used || ignored)
            fprintf(stderr, "[mods] %s: files/ replaces %u ROM file(s)%s\n", g_mods[m].id.c_str(), used,
                    ignored ? " (some ignored, see above)" : "");
        total_members += members;
        if (!warned && total_members > port_fs_member_claim_cap()) {
            /* hal/fs.cpp holds this many archive members at once; the rest
               keep the cartridge's bytes. One line, naming the mod whose files
               went over, instead of one line per member. */
            fprintf(stderr, "[mods] WARNING: %u archive members are replaced in all, more than the %u the game can hold; "
                    "%s is the mod that went over, and its members past the limit keep the cartridge's bytes\n",
                    total_members, port_fs_member_claim_cap(), g_mods[m].id.c_str());
            warned = 1;
        }
    }
    fprintf(stderr, "[mods] files layer: %u ROM file id(s) overridden in all\n", (unsigned)g_override.size());
    fflush(stderr);
}

/* The checked LZ77 (type 0x10) decoder for a mod's file. DecompressLZ16 trusts
   its input and has no bounds, which is right for the cartridge's own files and
   wrong for a file anyone can ship. This one needs the type byte 0x10, caps the
   declared size at 64 MB, stops with a failure when the input runs out, and
   rejects a back-reference that reaches before the start of the output.
   `in` is the stream after the "LZ77" tag (the 0x10 header word first).
   Returns a malloc'd buffer and its length, or 0. */
const u32 LZ_MAX = 64u << 20;

u8 *lz77_checked(const u8 *in, size_t n, u32 *len)
{
    if (n < 4 || in[0] != 0x10) return 0;
    const u32 dec = in[1] | in[2] << 8 | (u32)in[3] << 16;
    if (dec > LZ_MAX) return 0;
    u8 *out = (u8 *)malloc(dec ? dec : 1);
    if (!out) return 0;
    size_t ip = 4;
    u32 op = 0;
    while (op < dec) {
        if (ip >= n) goto bad;
        {
            const u8 flags = in[ip++];
            for (int bit = 0; bit < 8 && op < dec; ++bit) {
                if (flags & (0x80 >> bit)) {
                    if (ip + 2 > n) goto bad;
                    const u32 b1 = in[ip], b2 = in[ip + 1];
                    ip += 2;
                    const u32 cnt = (b1 >> 4) + 3;
                    const u32 disp = ((b1 & 15) << 8 | b2) + 1;
                    if (disp > op) goto bad;
                    for (u32 k = 0; k < cnt && op < dec; ++k, ++op)
                        out[op] = out[op - disp];
                } else {
                    if (ip >= n) goto bad;
                    out[op++] = in[ip++];
                }
            }
        }
    }
    *len = dec;
    return out;
bad:
    free(out);
    return 0;
}

/* The override's DECOMPRESSED bytes, malloc'd; 0 when unreadable. */
u8 *read_override(const std::string &path, u32 *len)
{
    FILE *f = fopen(path.c_str(), "rb");
    long n;
    u8 *raw, *out;
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    n = ftell(f);
    if (n < 0) { fclose(f); return 0; }
    fseek(f, 0, SEEK_SET);
    raw = (u8 *)malloc(n ? (size_t)n : 1);
    if (!raw) { fclose(f); return 0; }
    if ((long)fread(raw, 1, (size_t)n, f) != n) { fclose(f); free(raw); return 0; }
    fclose(f);
    if (n > 8 && memcmp(raw, "LZ77", 4) == 0) {
        out = lz77_checked(raw + 4, (size_t)n - 4, len);
        free(raw);
        return out;   /* 0 for a malformed stream: the caller keeps the stock file */
    }
    *len = (u32)n;
    return raw;
}

u32 (*g_prev_filter)(unsigned, u8 **, u32);
void (*g_prev_claims)(void (*)(unsigned));
std::unordered_map<unsigned, int> g_said;

u32 mods_filter(unsigned fileID, u8 **data, u32 size)
{
    if (g_prev_filter)
        size = g_prev_filter(fileID, data, size);
    build_overrides();
    std::unordered_map<unsigned, std::string>::const_iterator it = g_override.find(fileID);
    if (it == g_override.end())
        return size;
    u32 len = 0;
    u8 *b = read_override(it->second, &len);
    if (!b) {
        if (!g_said[fileID]++)
            fprintf(stderr, "[mods] %s could not be read or decoded (bad LZ77?); file id %u keeps the stock file\n",
                    it->second.c_str(), fileID);
        return size;
    }
    if (*data) free(*data);
    *data = b;
    if (!g_said[fileID]++)
        fprintf(stderr, "[mods] serving %s for file id %u (%u bytes in place of %u)\n",
                it->second.c_str(), fileID, len, size);
    return len;
}

void mods_claims(void (*claim)(unsigned fileID))
{
    build_overrides();
    for (std::unordered_map<unsigned, std::string>::const_iterator it = g_override.begin();
         it != g_override.end(); ++it)
        claim(it->first);
    if (g_prev_claims)
        g_prev_claims(claim);
}

/* textures_hd: the i-th directory, highest priority (latest mod) first. */
int mods_tex_dir(int i, char *out, unsigned cap)
{
    latch();
    const int n = (int)g_mods.size();
    if (i < 0 || i >= n) return 0;
    const std::string d = g_mods[n - 1 - i].folder + "/textures_hd";
    if (d.size() + 1 > cap) return 0;
    memcpy(out, d.c_str(), d.size() + 1);
    return 1;
}

/* Runs after every ordinary initializer, hal/fs_mods.cpp's and (.CRT$XCV)
   hal/stage_geom.cpp's included. */
void __cdecl mods_install(void)
{
    g_prev_filter = port_fs_mod_filter;
    port_fs_mod_filter = mods_filter;
    g_prev_claims = port_fs_mod_claims;
    port_fs_mod_claims = mods_claims;
    ntr::hdtex_mod_dir_hook = mods_tex_dir;
}

}  /* namespace */

extern "C" {
int port_mods_count(void) { latch(); return (int)g_mods.size(); }
const char *port_mods_id(int i) { latch(); return i >= 0 && i < (int)g_mods.size() ? g_mods[i].id.c_str() : 0; }
const char *port_mods_folder(int i) { latch(); return i >= 0 && i < (int)g_mods.size() ? g_mods[i].folder.c_str() : 0; }
const char *port_mods_kind(int i) { latch(); return i >= 0 && i < (int)g_mods.size() ? g_mods[i].kind.c_str() : 0; }
const char *port_mods_game(int i) { latch(); return i >= 0 && i < (int)g_mods.size() ? g_mods[i].game.c_str() : 0; }
int port_mods_api(int i) { latch(); return i >= 0 && i < (int)g_mods.size() ? g_mods[i].api : 0; }
const char *port_mods_bundle(void) { latch(); return g_bundle.c_str(); }
}

#pragma section(".CRT$XCW", read)
extern "C" __declspec(allocate(".CRT$XCW"))
    void (__cdecl *port_mods_ctor)(void) = mods_install;
#if defined(_M_IX86)
#pragma comment(linker, "/include:_port_mods_ctor")
#else
#pragma comment(linker, "/include:port_mods_ctor")
#endif
