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
