/* HOST ARM, recovered by PORTSYNC2.
 * Old path: src/_ZN17BowserPuzzlePieceD1Ev.cpp (its #ifdef _MSC_VER arm). The
 * 2026-10 main merge folded that file into src/actors/daObjFl_Coin_c.cpp and
 * renamed the class BowserPuzzlePiece -> daObjFl_Puzzle_c (flat D1
 * _ZN17BowserPuzzlePieceD1Ev -> _ZN16daObjFl_Puzzle_cD1Ev, same ROM address).
 * Main's TU carries an _MSC_VER arm for the flat D0 only. mwccarm never saw
 * this arm, so nothing here moves a ROM byte.
 *
 * WHY IT EXISTS. The host needs the ROM's flat D1 name and MSVC never emits it:
 * ~daObjFl_Puzzle_c() is defined in the class body, and the port's slot 16 for
 * this class (port/hal/actor_classes_bowserpuzzle.cpp) calls the flat name.
 * This defines it as the qualified, direct call that makes MSVC emit the body.
 * QUALIFIED on purpose: `self->~daObjFl_Puzzle_c()` would be a virtual call
 * through the port's ROM-shaped table at slot 16, straight back into this
 * symbol. The inline chain MSVC emits stores an MSVC vftable
 * (??_7dBgActor_c@@6B@, measured) where the cartridge's D1 (0x02118b50, ov064)
 * stores _ZTV16daObjFl_Puzzle_c; port/tools/dtor_store_guard.py re-proves from
 * the ROM body that nothing dispatches before the base destructor restores a
 * ROM-shaped table. Only the class and flat names changed from the old arm. */
#include "daObjFl_Puzzle_c.h"

// @symbol _ZN16daObjFl_Puzzle_cD1Ev
extern "C" void _ZN16daObjFl_Puzzle_cD1Ev(void *self)
{ ((daObjFl_Puzzle_c *)self)->daObjFl_Puzzle_c::~daObjFl_Puzzle_c(); }
