/* coop_handoff.cpp -- THE COOP KIND OF THE LAUNCHER HAND-OFF (run after093, lane SEAMCOOP1; the one-process plan's M5b).
 *
 * The launcher hands a coop room to the running game (in\HANDOFF_CONTRACT.md v1) instead of starting a second process once the game is
 * back at its title. The registry and the title poll are hal/handoff_poll.cpp's (lane SEAMVS1; on this lane's branch a fold-dropped
 * stand-in, hal/handoff_register_stub.cpp). This file only registers the "coop" kind; the work is hal/coop_boot.cpp's
 * port_coop_handoff_apply, which does in this process what a coop child does at boot with the same names (see its banner).
 * Linked into the game windows only (walk_window, walk_window_hires), like the registry. */
#include <cstddef>

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
