/* hal/portsync3_manual.cpp -- the hand-written leftovers of the 2026-10-14 main
 * sync. Every block says what it stands for and what is deliberately inert.
 */
#include "fBase_c.h"
#include "daDemo_c.h"
#include "dBgCh_Lin.h"
#include <cstdio>

/* ---- loud stub for ROM bodies the host has no definition for --------------
 * port_unmatched_hit prints once per site. Used where a compiler vtable lists
 * a virtual whose ROM body has no config name and no host copy; the port seats
 * hand tables for these objects, so a hit means the compiler vtable was
 * dispatched, which is a finding, not a state to hide. */
static void port_unmatched_hit(const char *what)
{
    static int said[64];
    static const char *seen[64];
    for (int i = 0; i < 64; ++i) {
        if (seen[i] == what) {
            if (said[i]) return;
            said[i] = 1;
            break;
        }
        if (!seen[i]) { seen[i] = what; said[i] = 1; break; }
    }
    std::fprintf(stderr, "  [portsync3] UNMATCHED virtual entered: %s\n", what);
    std::fflush(stderr);
}

/* ---- the daDemo_c model helpers (src/actors/daDemo_c.cpp) ------------------
 * dExtModel_c and its two subclasses are cutscene-only; the ROM bodies sit at
 * anonymous vtable slots (config names only the RTTI). */
dExtModel_c::~dExtModel_c() {}
int dExtSimpleModel_c::DoSetFile(char *, int, int) { port_unmatched_hit("dExtSimpleModel_c::DoSetFile"); return 0; }
void dExtSimpleModel_c::UpdateVerts() { port_unmatched_hit("dExtSimpleModel_c::UpdateVerts"); }
void dExtSimpleModel_c::Virtual10(Matrix4x3 &) { port_unmatched_hit("dExtSimpleModel_c::Virtual10"); }
void dExtSimpleModel_c::Render(const Vector3 *) { port_unmatched_hit("dExtSimpleModel_c::Render"); }
void dExtAnmModel_c::UpdateVerts() { port_unmatched_hit("dExtAnmModel_c::UpdateVerts"); }
void dExtAnmModel_c::Virtual10(Matrix4x3 &) { port_unmatched_hit("dExtAnmModel_c::Virtual10"); }
void dExtAnmModel_c::Render(const Vector3 *) { port_unmatched_hit("dExtAnmModel_c::Render"); }
void dExtAnmModel_c::Virtual18(u32, const Vector3 *) { port_unmatched_hit("dExtAnmModel_c::Virtual18"); }

/* ---- dBgCh_Lin::GetClsnPos, the flat spelling ------------------------------
 * func_02008cb4.c calls the ROM's struct-return convention (return slot first,
 * receiver second); main's member returns by value. */
extern "C" void _ZN9dBgCh_Lin10GetClsnPosEv(Vector3 *ret, void *self)
{
    *ret = ((dBgCh_Lin *)self)->GetClsnPos();
}

/* ---- port_vs_body_0211478c -------------------------------------------------
 * hal/vs_data_patch.inc points the VS lobby's .data record at this name. The
 * old build compiled ov075's own body under it with a -D rename (the window
 * address 0x0211478c is also a WorkElevator vtable word in another overlay).
 * Main names the body UnknownVsPlayer::func_ov075_0211478c, so the rename is
 * unnecessary: the flat face the ledger now carries (faces_sync.txt, R row) is
 * the entry, and this name aliases it. */
#pragma comment(linker, "/alternatename:_port_vs_body_0211478c=__ZN15UnknownVsPlayer19func_ov075_0211478cEv")

/* ---- fBase_c::Process, the flat spelling ----------------------------------
 * func_0204322c.cpp calls it by its Itanium name with the three member
 * pointers as eight-byte pairs on the stack (the port compiles /vmg /vmm so an
 * MSVC pointer to member is the same eight bytes). facegen refuses a
 * pointer-to-member parameter; the face is written by hand. */
extern "C" int _ZN7fBase_c7ProcessEMS_FivEMS_FbvEMS_FvjE(fBase_c *self,
    fBase_c::ProcessFunction action, fBase_c::BeforeProcessFunction before,
    fBase_c::AfterProcessFunction after)
{
    return self->Process(action, before, after);
}

/* ---- dBgCh_Actr::GetWallResult / GetFloorResult, the flat spellings -------
 * Both are inline members of include/dBgCh_Actr.h now (they return the sphere
 * collider's result pointer), so there is no out-of-line member to reach; the
 * flat names the ROM-shaped callers spell are defined here from the header. */
#include "dBgCh_Actr.h"
extern "C" int _ZNK10dBgCh_Actr13GetWallResultEv(const void *self)
{ return (int)(size_t)((const dBgCh_Actr *)self)->GetWallResult(); }
extern "C" int _ZNK10dBgCh_Actr14GetFloorResultEv(const void *self)
{ return (int)(size_t)((const dBgCh_Actr *)self)->GetFloorResult(); }

/* ---- daDonketu_c::CleanupResources / Render --------------------------------
 * include/daDonketu_c.h re-declares both so the class names them, but the ROM
 * gives daDonketu_c no body of its own: its vtable slots 3 and 9 point at
 * daOts_c's (the header comment records the addresses). MSVC needs a definition
 * for the declared override, and the faithful one is the inherited call. */
#include "daDonketu_c.h"
int daDonketu_c::CleanupResources() { return daOts_c::CleanupResources(); }
int daDonketu_c::Render() { return daOts_c::Render(); }
