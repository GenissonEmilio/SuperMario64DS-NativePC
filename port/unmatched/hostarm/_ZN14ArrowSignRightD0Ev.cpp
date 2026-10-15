/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN14ArrowSignRightD0Ev.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/game/actors/d_a_obj_yajirusi.cpp without the arm. MSVC emits one destructor per class and never the ROM's flat Itanium variant name, and port/hal calls that flat name from the class's vtable slot.
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN15daObjYajirusi_cD0Ev
/* recovered: real C++ deleting destructor -- the compiler emits the whole body
 *
 * D0 is the DELETING destructor: destroy through this class and its bases --
 * which is why more than one vptr store appears -- then return the object to
 * its heap. Nobody writes that; declaring `~daObjYajirusi_c()` is enough, because mwcc
 * emits D2, D0 and D1 together and objisolate keeps the one this file is bound
 * to.
 *
 * The deallocation is an inline operator delete, which is why nothing below
 * mentions a heap.
 */
#include "daObjYajirusi_c.h"

/* THE HOST NEEDS THE ROM'S FLAT D0 NAME, AND MSVC NEVER EMITS IT. MSVC
 * folds the Itanium destructor variants into the one ~daObjYajirusi_c() it
 * emits, which the class's D1 file already defines, so compiling the
 * definition below as well would define it twice. The port builds this
 * class from the header's flat host view (its SM64DS_PLATFORM_PC branch),
 * which has no base class and so no inherited operator delete. The ROM's
 * D0 ends in dActor_c's inline delete, Memory::Deallocate on the actor heap
 * word at 0x020a0eac, and this arm makes that call by name. So it makes
 * the two calls the port's generated face for this D0 made: the D1 body,
 * called qualified so it is a direct call to the one host symbol, then
 * that deallocation. Nothing here reaches mwccarm: it builds the `#else`
 * arm and emits the ROM bytes it always emitted, and the object is
 * byte-identical either way. */
extern "C" void _ZN6Memory10DeallocateEPvPN5mHeap6Heap_tE(void *, void *);
extern "C" void *data_020a0eac;
extern "C" daObjYajirusi_c *_ZN15daObjYajirusi_cD0Ev(daObjYajirusi_c *thiz)
{
    thiz->daObjYajirusi_c::~daObjYajirusi_c();                /* the D1 body, through the one host symbol */
    _ZN6Memory10DeallocateEPvPN5mHeap6Heap_tE(thiz, data_020a0eac);   /* the actor-heap delete D0 ends with */
    return thiz;
}
