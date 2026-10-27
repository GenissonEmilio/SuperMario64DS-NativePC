/* coop_handoff.cpp -- THE COOP KIND OF THE LAUNCHER HAND-OFF (run after093, lane SEAMCOOP1; the one-process plan's M5b).
 *
 * The launcher hands a coop room to the running game (in\HANDOFF_CONTRACT.md v1) instead of starting a second process once the game is
 * back at its title. The registry and the title poll are hal/handoff_poll.cpp's (lane SEAMVS1; on this lane's branch a fold-dropped
 * registry, hal/handoff_poll.cpp). This file only registers the "coop" kind; the work is hal/coop_boot.cpp's
 * port_coop_handoff_apply, which does in this process what a coop child does at boot with the same names (see its banner).
 * Linked into the game windows only (walk_window, walk_window_hires), like the registry. */
#include <cstddef>

extern "C" {
/* hal/handoff_poll.h (SEAMVS1), copied verbatim */
typedef struct PortHandoffRequest {
    const char *kind;               /* "adventure", "vs", "coop" */
    const char *nonce;              /* the launcher's 16 hex digits */
    int nset;                       /* the launcher's setEnv, in file order */
    const char *const *set_name;    /* "SM64DS_<NAME>" */
    const char *const *set_value;
    int nclear;                     /* the launcher's clearEnv */
    const char *const *clear_name;
} PortHandoffRequest;

/* 0 = the running game took the match; else why not (a refusal: the response says it, the game quits with exit code 8) */
typedef const char *(*PortHandoffFn)(const PortHandoffRequest *req);

/* Register the handler of one kind (at static initialisation is fine: the table is constant-initialised). A second registration of a
   kind replaces the first. 1 = registered, 0 = refused (no kind, no handler, table full). */
int port_handoff_register(const char *kind, PortHandoffFn fn);

/* The request's value for one name of its set list, or 0 when the request does not set it. */
const char *port_handoff_value(const PortHandoffRequest *req, const char *name);

/* The environment a fresh process would be started with: every name of the clear list out of BOTH copies of the environment (the CRT's,
   which getenv reads, and the Win32 block a later restart would inherit), then every name of the set list into both. Returns how many
   names were touched. */
int port_handoff_apply_env(const PortHandoffRequest *req);

/* A handler that refuses after port_handoff_apply_env: every name of the set list out of both copies again. */
void port_handoff_unset_env(const PortHandoffRequest *req);

/* hal/coop_boot.cpp: 0 = the room is being booted in this process; else why not (nothing of the request is left applied) */
const char *port_coop_handoff_apply(int nset, const char *const *set_name, const char *const *set_value, int nclear,
                                    const char *const *clear_name);
}

namespace {
const char *coop_handoff(const PortHandoffRequest *req) {
    if (!req) return "no request";
    return port_coop_handoff_apply(req->nset, req->set_name, req->set_value, req->nclear, req->clear_name);
}
const int g_registered = port_handoff_register("coop", coop_handoff);
}  // namespace
