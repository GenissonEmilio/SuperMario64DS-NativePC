/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/_ZN13BigBrickBlock11OnAttacked1ER8dActor_c.cpp, which carried this under #ifdef _MSC_VER. The
 * 2026-10 main merge took main's src/, which folded that code into
 * src/game/actors/d_a_obj_block_l.cpp without the arm. Main's body still takes a bare return; out of an int function, which MSVC refuses (C2561), so main's definition must be parked in the hostgen copy and this one used instead. Needs /wd4716 on this source (falls off the end on purpose).
 * Only the host arm is here, retargeted to main's names; mwccarm never
 * compiles this file, so nothing here moves a ROM byte. Comments below that
 * name old per-function src/ files describe the pre-merge tree. */
// host arm for ROM symbol _ZN13daObjBlockL_c11OnAttacked1ER8dActor_c
#include "daObjBlockL_c.h"
#include "dActor_c.h"

/* daObjBlockL_c::OnAttacked1 -- vtable slot 22, ov002 0x020b37ec. Was a plain
 * C recovery (func_ov002_020b37ec.c) with a single `void *this` parameter --
 * `other` was never read, so the pre-migration stub dropped it from the
 * declared prototype without changing the compiled bytes. The real override
 * keeps `other` in the signature (include/dActor_c.h slot 22) but still never
 * touches it.
 *
 * `actorID` is fBase_c's own field. The single call is this class's own Kill
 * (slot 31), reached through an unqualified virtual call -- the same
 * indirect dispatch the pre-migration `*(int*)((*(int*)c)+0x7c)` vtable read
 * reproduced by hand.
 *
 * `int b = (actorID == 0x11);` materializes the bool the same way the
 * pre-migration recovery did -- load-bearing (cpp-bool-widening-cast). */
int daObjBlockL_c::OnAttacked1(dActor_c &other)
{
    int b = (actorID == 0x11);
    /* THE HOST SPELLS THE EARLY EXIT AS THE ABSENCE OF THE CALL. mwccarm
     * accepts a valueless `return` in an int function; MSVC refuses it
     * (C2561) and no option reaches it. The ROM sets no return value on
     * either path (0x020b3810 returns with r0 still the receiver, the Kill
     * path with whatever Kill left), so the faithful host shape is a body
     * that reaches its closing brace with nothing to return, the shape
     * daObjTatefuda_c::OnAttacked1 and daObjHatenaBlock_c::OnAttacked1 are seated in;
     * port/CMakeLists.txt puts /wd4716 on this source for it. Nothing here
     * reaches mwccarm: it builds the #else arm, byte-identical. */
    if (!b) {
        Kill();
    }
}
