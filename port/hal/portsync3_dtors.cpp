/* hal/portsync3_dtors.cpp -- destructors main leaves out of the host build.
 *
 * PORTSYNC3 (the 2026-10-14 main merge). Main's class headers stopped defining
 * the destructors inline for MSVC, and the TUs that own them keep the definition
 * behind `#ifndef _MSC_VER` (src/engine/fader/dFader_c.cpp, src/actors/daKpa_c.cpp
 * and daStar_c.cpp are the cases here; src/actors/daDemo_c.cpp's two Ext model
 * classes and daObjYajirusi_c have no definition in src/ at all). The host
 * still needs each one: port/unmatched/hostarm/*D0Ev.cpp and the flat D0 files
 * call `this->X::~X()` qualified, and the hal tables take its address.
 *
 * Every body below is `{}` because the ROM's is: the cartridge's D1/D0 pair for
 * these classes is compiler-generated member and base teardown and nothing else
 * (the src/ definitions these stand for read `{ }` too). The compiler generates
 * the same teardown for the empty host body.
 *
 * fBase_c::~fBase_c is the non-virtual base arm: it is the ROM's D2, which
 * stores fBase_c's vtable and unlinks the two list nodes, spelled by the flat
 * function src/actors/ActorBase.cpp already defines.
 *
 * Destructor1 / Destructor0: classes whose header keeps the `_MSC_VER`
 * Destructor1()/Destructor0() virtual pair and whose TU does not define it. The
 * port seats ROM-slot destructors through hand tables, so these are reached only
 * if a compiler vtable is ever dispatched; they destroy the object and, for D0,
 * do NOT free it (the host never frees through a compiler vtable).
 */
#include "fBase_c.h"
#include "dFader_c.h"
#include "dFdBrightness_c.h"
#include "dFdColor_c.h"
#include "dFdDummy_c.h"
#include "dFdWipe_c.h"
#include "daDemo_c.h"
#include "daObjYajirusi_c.h"
#include "daObjFallBlock_c.h"
#include "daObjFl_Fall_Block_c.h"
#include "daObjKaitendai_c.h"

extern "C" fBase_c *_ZN7fBase_cD2Ev(fBase_c *self);

fBase_c::~fBase_c() { _ZN7fBase_cD2Ev(this); }

dFdWipe_c::dFdWipe_c() {}
extern "C" void *_ZTV10dFdDummy_c[12];   /* hal/scene_boot.cpp: the ROM-shaped table with the dual cdecl/thiscall slots */
/* PORTSYNC3: a dFdDummy_c built by main's code (`new dFdDummy_c`) carries the compiler's
   vtable, whose slots 3 and 4 are one-argument thiscall members. Every caller of those two
   slots passes the ROM's two words (dScene_c::BeforeBehavior through a cdecl pointer view,
   dMgState_c and ProcessKuppaScript as thiscall with two), which only the hal table's
   trampolines answer. So the object gets the hal table, exactly as the ROM constructor
   stores its own. */
dFdDummy_c::dFdDummy_c() { *(void **)(void *)this = (void *)_ZTV10dFdDummy_c; }
dFader_c::~dFader_c() {}
dFdBrightness_c::~dFdBrightness_c() {}
dFdColor_c::~dFdColor_c() {}
dFdDummy_c::~dFdDummy_c() {}
dFdWipe_c::~dFdWipe_c() {}
// Fade members the host build leaves out (src/engine/fader/dFader_c.cpp keeps them
// behind #ifndef _MSC_VER because they write the 2D engine). The host-side fade is
// hal/fader_wipes.cpp; these only fill compiler vtables, like hal/shims.cpp does.
void dFdBrightness_c::AdvanceFade() { AdvanceInterp(); }
void dFdWipe_c::AdvanceFade() {}
void dFdColor_c::AdvanceFade() { AdvanceInterp(); }  // unexercised on the measured paths; the 2D blend write stays DS-only
void dFdWipe_c::LoadAndSetFile(u16 ov0ID) { model.LoadAndSetFile(ov0ID, 0, -1); }
dExtSimpleModel_c::~dExtSimpleModel_c() {}
dExtAnmModel_c::~dExtAnmModel_c() {}
daObjYajirusi_c::~daObjYajirusi_c() {}

void daObjFallBlock_c::Destructor1() { this->daObjFallBlock_c::~daObjFallBlock_c(); }
void daObjFallBlock_c::Destructor0() { this->daObjFallBlock_c::~daObjFallBlock_c(); }
void daObjFl_Fall_Block_c::Destructor1() { this->daObjFl_Fall_Block_c::~daObjFl_Fall_Block_c(); }
void daObjFl_Fall_Block_c::Destructor0() { this->daObjFl_Fall_Block_c::~daObjFl_Fall_Block_c(); }
void daObjKaitendai_c::Destructor1() { this->daObjKaitendai_c::~daObjKaitendai_c(); }
void daObjKaitendai_c::Destructor0() { this->daObjKaitendai_c::~daObjKaitendai_c(); }
