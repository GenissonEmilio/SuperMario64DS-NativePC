/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN13BigBrickBlock11OnAttacked2ER8dActor_c.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/actors/daObjBlockL_c.cpp without the arm. Main's body still takes a bare return; out of an int function, which MSVC refuses (C2561), so main's definition must be parked in the hostgen copy and this one used instead. Needs /wd4716 on this source (falls off the end on purpose).
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN13daObjBlockL_c11OnAttacked2ER8dActor_c
#include "daObjBlockL_c.h"
#include "dActor_c.h"

/* daObjBlockL_c::OnAttacked2 -- vtable slot 23, ov002 0x020b3788.
 * include/dActor_c.h's own slot 23 supplies the signature, `virtual int
 * OnAttacked2(dActor_c &other)`.
 *
 * `actorID` and `other.param1` are fBase_c's own fields. Both branches call
 * this class's own Kill (slot 31) through an unqualified virtual call -- the
 * same indirect dispatch the pre-migration `(*(Vt**)c)->fn(c)` read
 * reproduced by hand.
 *
 * `int b = (actorID == 0x11);` materializes the bool the same way the
 * pre-migration recovery did -- load-bearing (cpp-bool-widening-cast). */
int daObjBlockL_c::OnAttacked2(dActor_c &other)
{
    int b = (actorID == 0x11);
    /* THE HOST SPELLS THE EARLY EXITS AS THE ABSENCE OF THE CALL. mwccarm
     * accepts a valueless `return` in an int function; MSVC refuses it
     * (C2561) and no option reaches it. The ROM sets no return value on any
     * path, so the faithful host shape is a body that reaches its closing
     * brace with nothing to return, the shape OnAttacked1 (slot 22) is seated
     * in; port/CMakeLists.txt puts /wd4716 on this source for it. Both Kill
     * calls stay where the ROM has them. Nothing here reaches mwccarm: it
     * builds the #else arm, byte-identical. */
    if (b) {
        if (other.param1 == 2) {
            Kill();
        }
    } else {
        Kill();
    }
}
