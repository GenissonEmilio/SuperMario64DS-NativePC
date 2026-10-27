/* handoff_register_stub.cpp -- THE HAND-OFF REGISTRY, A STAND-IN (run after093, lane SEAMCOOP1). FOLD-DROPPED.
 *
 * The launcher hand-off (in\HANDOFF_CONTRACT.md v1) is lane SEAMVS1's: hal/handoff_poll.h + hal/handoff_poll.cpp hold the registry,
 * the title poll and the response. This branch has neither, but hal/coop_boot.cpp registers its "coop" handler through the contract's
 * API, so this file supplies that API with the SAME declarations as SEAMVS1's header (PortHandoffRequest, PortHandoffFn,
 * port_handoff_register) and a minimal title poll, so the coop handler can be proven with a hand-written request on this branch.
 *
 * GUARDED SO THE FOLD DROPS IT: the moment hal/handoff_poll.h exists in the tree (the fold brings SEAMVS1's files), this whole file
 * compiles to nothing and SEAMVS1's registry and poll are the only ones. The fold may also delete the file and its CMakeLists block.
 *
 * The stand-in poll follows the contract: only while the title is the live scene, no session is up and the Joining plate is down, every
 * 15 game frames (chained on port_sdat_frame_hook); request.json is renamed to request.taken before it is read; v != 1 or a deadline in
 * the past answers "expired"; the kind's handler answers accepted (null) or refused (its reason), and a refusal quits with exit code 8
 * after the response. The folder is <the exe's folder>/handoff, or SM64DS_HANDOFF_TEST=<a folder path> (dev builds only). */
#if defined(__has_include)
#if __has_include("handoff_poll.h")
#define PORT_HANDOFF_STUB_DROPPED 1
#endif
#endif

#ifndef PORT_HANDOFF_STUB_DROPPED

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
/* hal/handoff_poll.h (SEAMVS1), the same declarations */
typedef struct PortHandoffRequest {
    const char *kind;
    const char *nonce;
    int nset;
    const char *const *set_name;
    const char *const *set_value;
    int nclear;
    const char *const *clear_name;
} PortHandoffRequest;
typedef const char *(*PortHandoffFn)(const PortHandoffRequest *req);
int port_handoff_register(const char *kind, PortHandoffFn fn);

extern void (*port_sdat_frame_hook)(void);   /* hal/sdat/consumer.cpp */
int port_scene_live_id(void);                /* hal/scene_boot.cpp */
int port_join_plate_up(void);                /* hal/menu_join_plate.cpp */
int port_coop_session_busy(void);            /* hal/coop_boot.cpp */
}
namespace port { bool comms_hub_active(); }

namespace {

struct Reg { const char *kind; PortHandoffFn fn; };
Reg g_reg[4];                                /* constant-initialised: a registration at static init is safe */
int g_nreg;
void (*g_prev)(void) = nullptr;
unsigned g_frames;

unsigned long long unix_ms() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    const unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (t - 116444736000000000ULL) / 10000ULL;
}

std::string folder() {
#ifndef PORT_ROM_CLEAN
    if (const char *t = std::getenv("SM64DS_HANDOFF_TEST"))
        if (std::strchr(t, '/') || std::strchr(t, (char)92)) return t;
#endif
    char p[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s(p, n);
    const size_t at = s.find_last_of("/\\");
    return (at == std::string::npos ? std::string(".") : s.substr(0, at)) + "\\handoff";
}

/* ---- a minimal JSON reader for the request's one object ---- */
struct Js {
    const char *p, *e;
    void ws() { while (p < e && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p; }
    bool lit(char c) { ws(); if (p < e && *p == c) { ++p; return true; } return false; }
    bool str(std::string *out) {
        ws();
        if (p >= e || *p != '"') return false;
        ++p;
        out->clear();
        while (p < e && *p != '"') {
            char c = *p++;
            if (c == (char)92 && p < e) {
                const char x = *p++;
                if (x == 'n') c = '\n'; else if (x == 't') c = '\t'; else if (x == 'r') c = '\r';
                else if (x == 'u') { c = '?'; for (int i = 0; i < 4 && p < e; ++i) ++p; }
                else c = x;
            }
            *out += c;
        }
        if (p >= e) return false;
        ++p;
        return true;
    }
    bool num(long long *out) {
        ws();
        char *end = nullptr;
        const long long v = std::strtoll(p, &end, 10);
        if (end == p) return false;
        p = end;
        *out = v;
        return true;
    }
    bool skip() {   /* any value */
        ws();
        if (p >= e) return false;
        std::string s;
        long long n;
        if (*p == '"') return str(&s);
        if (*p == '{' || *p == '[') {
            const char open = *p, close = open == '{' ? '}' : ']';
            int depth = 0;
            while (p < e) {
                if (*p == '"') { if (!str(&s)) return false; continue; }
                if (*p == open) ++depth;
                if (*p == close && --depth == 0) { ++p; return true; }
                ++p;
            }
            return false;
        }
        if (num(&n)) return true;
        while (p < e && *p != ',' && *p != '}' && *p != ']') ++p;
        return true;
    }
};

struct Req {
    long long v = 0, deadline = 0;
    std::string nonce, kind;
    std::vector<std::string> sn, sv, cn;
};

bool parse(const std::string &t, Req *r) {
    Js j{t.data(), t.data() + t.size()};
    if (!j.lit('{')) return false;
    while (true) {
        std::string k;
        if (!j.str(&k) || !j.lit(':')) return false;
        if (k == "v") { if (!j.num(&r->v)) return false; }
        else if (k == "deadline_ms") { if (!j.num(&r->deadline)) return false; }
        else if (k == "nonce") { if (!j.str(&r->nonce)) return false; }
        else if (k == "kind") { if (!j.str(&r->kind)) return false; }
        else if (k == "set") {
            if (!j.lit('{')) return false;
            if (!j.lit('}')) {
                do {
                    std::string n, v;
                    if (!j.str(&n) || !j.lit(':') || !j.str(&v)) return false;
                    r->sn.push_back(n); r->sv.push_back(v);
                } while (j.lit(','));
                if (!j.lit('}')) return false;
            }
        } else if (k == "clear") {
            if (!j.lit('[')) return false;
            if (!j.lit(']')) {
                do {
                    std::string n;
                    if (!j.str(&n)) return false;
                    r->cn.push_back(n);
                } while (j.lit(','));
                if (!j.lit(']')) return false;
            }
        } else if (!j.skip()) return false;
        if (j.lit(',')) continue;
        return j.lit('}');
    }
}

void respond(const std::string &dir, const std::string &nonce, const char *result, const char *why) {
    std::string body = "{ \"v\": 1, \"nonce\": \"" + nonce + "\", \"result\": \"" + result + "\"";
    if (why) {
        body += ", \"why\": \"";
        for (const char *c = why; *c; ++c) { if (*c == '"' || *c == (char)92) body += (char)92; body += *c; }
        body += "\"";
    }
    body += " }\n";
    const std::string tmp = dir + "\\response.json.tmp", fin = dir + "\\response.json";
    if (std::FILE *f = std::fopen(tmp.c_str(), "wb")) {
        std::fwrite(body.data(), 1, body.size(), f);
        std::fclose(f);
        MoveFileExA(tmp.c_str(), fin.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
    std::fprintf(stderr, "[handoff] response %s (nonce %s)%s%s\n", result, nonce.c_str(), why ? ": " : "", why ? why : "");
    std::fflush(stderr);
}

void poll_once() {
    const std::string dir = folder();
    const std::string rq = dir + "\\request.json", taken = dir + "\\request.taken";
    if (GetFileAttributesA(rq.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    if (!MoveFileExA(rq.c_str(), taken.c_str(), MOVEFILE_REPLACE_EXISTING)) return;
    std::string t;
    if (std::FILE *f = std::fopen(taken.c_str(), "rb")) {
        char b[4096];
        size_t n;
        while ((n = std::fread(b, 1, sizeof b, f)) > 0) t.append(b, n);
        std::fclose(f);
    }
    Req r;
    const bool ok = parse(t, &r);
    std::fprintf(stderr, "[handoff] request taken (stand-in poll): kind %s, nonce %s, %d set, %d clear%s\n", r.kind.c_str(),
                 r.nonce.c_str(), (int)r.sn.size(), (int)r.cn.size(), ok ? "" : " (NOT PARSED)");
    std::fflush(stderr);
    if (!ok || r.v != 1 || (unsigned long long)r.deadline < unix_ms()) {
        respond(dir, r.nonce, "expired", nullptr);
        DeleteFileA(taken.c_str());
        return;
    }
    PortHandoffFn fn = nullptr;
    for (int i = 0; i < g_nreg; ++i)
        if (!std::strcmp(g_reg[i].kind, r.kind.c_str())) fn = g_reg[i].fn;
    std::vector<const char *> sn, sv, cn;
    for (size_t i = 0; i < r.sn.size(); ++i) { sn.push_back(r.sn[i].c_str()); sv.push_back(r.sv[i].c_str()); }
    for (const std::string &c : r.cn) cn.push_back(c.c_str());
    PortHandoffRequest q = { r.kind.c_str(), r.nonce.c_str(), (int)sn.size(), sn.data(), sv.data(), (int)cn.size(), cn.data() };
    const char *why = fn ? fn(&q) : "no handler is registered for this kind";
    DeleteFileA(taken.c_str());
    if (!why) { respond(dir, r.nonce, "accepted", nullptr); return; }
    respond(dir, r.nonce, "refused", why);
    std::fprintf(stderr, "[handoff] refused: %s; the launcher starts a fresh game\n", why);
    std::fflush(stderr);
    ExitProcess(8);
}

void frame_hook() {
    if (g_prev) g_prev();
    if (++g_frames % 15) return;
    if (port_scene_live_id() != 1 || port::comms_hub_active() || port_join_plate_up() || port_coop_session_busy()) return;
    poll_once();
}

const int g_seated = (g_prev = port_sdat_frame_hook, port_sdat_frame_hook = frame_hook, 1);

}  // namespace

extern "C" int port_handoff_register(const char *kind, PortHandoffFn fn) {
    if (!kind || !*kind || !fn) return 0;
    for (int i = 0; i < g_nreg; ++i)
        if (!std::strcmp(g_reg[i].kind, kind)) { g_reg[i].fn = fn; return 1; }
    if (g_nreg >= (int)(sizeof g_reg / sizeof *g_reg)) return 0;
    g_reg[g_nreg].kind = kind;
    g_reg[g_nreg].fn = fn;
    ++g_nreg;
    return 1;
}

#endif  /* PORT_HANDOFF_STUB_DROPPED */
