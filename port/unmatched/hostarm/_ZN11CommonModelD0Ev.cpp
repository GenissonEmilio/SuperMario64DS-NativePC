/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN11CommonModelD0Ev.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/engine/model/d_ext_common_model.cpp without the arm. MSVC emits one destructor per class and never the ROM's flat Itanium variant name, and port/hal calls that flat name from the class's vtable slot.
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN17dExtCommonModel_cD0Ev
/* recovered: real C++ deleting destructor -- the compiler emits the whole body
 *
 * D0 is the DELETING destructor: destroy through this class and its bases --
 * which is why more than one vptr store appears -- then return the object to
 * its heap. Nobody writes that; declaring `~dExtCommonModel_c()` is enough, because mwcc
 * emits D2, D0 and D1 together and objisolate keeps the one this file is bound
 * to.
 *
 * The deallocation is an inline operator delete, which is why nothing below
 * mentions a heap.
 */
#include "dExtCommonModel_c.h"

/* THE HOST NEEDS THE ROM'S THREE SYMBOLS, AND MSVC ONLY MAKES ONE. It folds
 * the Itanium D1/D0/D2 triple into a single `??1CommonModel@@QAE@XZ`, so the files
 * that carry this one definition would each define that same symbol and the
 * host link refuses them -- measured, LNK2005 twice over for the ModelBase
 * triple alone. The D1 file keeps the definition; this file spells out, in
 * terms of it, what the variant this file is enrolled for actually does.
 * Nothing here reaches mwccarm: it builds the `#else` arm and emits the ROM
 * bytes it always emitted, and the object is byte-identical either way. */
extern "C" dExtCommonModel_c *_ZN17dExtCommonModel_cD0Ev(dExtCommonModel_c *thiz)
{
    thiz->~dExtCommonModel_c();                /* the D1 body, through the one host symbol */
    dExtCommonModel_c::operator delete(thiz);  /* the class-specific delete D0 ends with */
    return thiz;
}
