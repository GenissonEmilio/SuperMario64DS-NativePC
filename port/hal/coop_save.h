// COOP SAVE: the host's file, the guests' temp copies, the join fetch and the
// merge. Run mp1 wave 56, lane COOPSAVE1. Wire: hal/coop_save_wire.h. Fence
// (coop vs normal images): ntr/backup.cpp section 6.
//
// A coop room has ONE save file, on the host's PC (<install>\save\coop\
// <name>.coopsav, with <name>.json beside it). Guests run on a throwaway image
// in %TEMP% that is deleted at exit. Progress flows: a guest's save bits go to
// the host (SYNB 0x21 from_host=0), the host ORs them into its own record,
// saves with the ROM's own SaveData::SaveFile, and rebroadcasts the merged
// record (from_host=1) at once and every 2 s; guests OR it into theirs.
//
// ENV (the W56 launch contract): SM64DS_COOP=1 (all), SM64DS_COOP_HOST=1 (host
// only), SM64DS_COOP_SAVE=<full path of the host's .coopsav>, and
// SM64DS_COOP_SAVE_NAME=<display name>. Other keys read here, for proofs only:
// SM64DS_COOPSAVE_LOG=1 (no effect today, events always log),
// SM64DS_COOPSAVE_PROBE, SM64DS_COOPSAVE_FORGE (see coop_save.cpp).
//
// ONE FRAME HOOK: port_coop_save_frame() in tests/walk_window.cpp, next to
// port_hub_presence_frame().
//
// SYNB IS SHARED. hub_on_kind keeps ONE handler per kind, and the SYNB
// sub-type ranges belong to three lanes. This file registers a handler for
// kHubCoopCtl unless the fold installs a dispatcher: then call
// port::coop_save_set_external_dispatch(true) before the first frame and have
// the dispatcher call port::coop_save_handle() for sub-types 0x20-0x2f.
#ifndef PORT_HAL_COOP_SAVE_H
#define PORT_HAL_COOP_SAVE_H

#include <stdint.h>

namespace port {

// Is this process a coop room member / the host?
bool coop_save_active();
bool coop_save_is_host();

// HOST: make sure <name>.coopsav (an all-0xFF image: a new file) and
// <name>.json exist. Idempotent; true when the file is ready. A host that
// passes a path which is not a *.coopsav gets false (the backup fence refuses
// it with exit 8 on the first card access anyway).
bool coop_save_prepare();

// GUEST, before the title reads its files: ask the host for its record and
// seed the temp image's slot 0 with it. False on timeout (the caller decides
// the exit; W56 proposes code 6). A host (or a non-coop process) returns true
// at once. Pumps the hub itself while waiting.
bool coop_save_fetch(int timeout_ms);

// A SYNB datagram (whole, with the 12-byte head). True when it was in this
// lane's sub-type range (0x20-0x2f), whether or not it was accepted.
bool coop_save_handle(int seat, const unsigned char *data, int len);
void coop_save_set_external_dispatch(bool on);

struct CoopSaveStats {
    unsigned req_sent, req_recv;
    unsigned rec_sent_guest, rec_sent_host;
    unsigned rec_recv_guest, rec_recv_host;
    unsigned merged_bits;        // bits newly set by a merge (host or guest)
    unsigned saves_written;      // SaveFile calls this lane made
    unsigned dropped_bad;        // malformed
    unsigned dropped_role;       // a record for the other role
    unsigned dropped_subtype;    // 0x20-0x2f but not 0x20 / 0x21
    unsigned fetch_ok, fetch_timeout;
};
CoopSaveStats coop_save_stats();

}  // namespace port

extern "C" void port_coop_save_frame(void);
extern "C" void port_coop_save_boot(void);

#endif  // PORT_HAL_COOP_SAVE_H
