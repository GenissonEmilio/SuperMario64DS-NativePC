/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN12MetalNetLiftD1Ev.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/actors/daObjFl_Amilift_c.cpp without the arm. MSVC emits one destructor per class and never the ROM's flat Itanium variant name, and port/hal calls that flat name from the class's vtable slot.
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN17daObjFl_Amilift_cD1Ev
/* recovered: real C++ destructor -- the compiler emits the whole body.
 * Vtable slot 16: one vtable store, the members in reverse, then ~dBgActor_c. */
#include "daObjFl_Amilift_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D1 NAME, AND MSVC ONLY MAKES ONE DESTRUCTOR.
 * It folds the Itanium D1/D0 pair into the single ??1MetalNetLift@@QAE@XZ,
 * which src/_ZN17daObjFl_Amilift_cD0Ev.cpp defines out of line for the port
 * (include/daObjFl_Amilift_c.h declares ~daObjFl_Amilift_c() with no slot under
 * _MSC_VER), so this file cannot carry the same definition: the host link
 * refuses the pair (LNK2005). It spells out, in terms of that one host symbol,
 * what the variant this file is enrolled for does: the D1 body, with no delete
 * (slot 16's caller frees). The qualified call is direct. The port's slot 16
 * for METAL_NET_LIFT (aml_d1 in hal/actor_classes_ov064_gate178.cpp) calls
 * this name. The destructor stores an MSVC vftable where the cartridge's D1
 * (0x02117978, ov064) stores _ZTV17daObjFl_Amilift_c;
 * port/tools/dtor_store_guard.py reads this arm (SRC_ARMS) and re-proves from
 * the ROM body on every build that nothing dispatches before the base
 * destructor restores a ROM-shaped table. Nothing here reaches mwccarm: it
 * builds the `#else` arm and emits the ROM bytes it always emitted. */
extern "C" daObjFl_Amilift_c *_ZN17daObjFl_Amilift_cD1Ev(daObjFl_Amilift_c *thiz)
{
    thiz->daObjFl_Amilift_c::~daObjFl_Amilift_c();  /* the D1 body, through the one host symbol */
    return thiz;
}
