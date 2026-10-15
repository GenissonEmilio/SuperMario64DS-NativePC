/* PORT_HOST_ABI. The fader hierarchy, force-included ahead of
 * src/_ZN8dScene_c9SetFadersEP15FaderBrightness.cpp so that translation unit
 * dispatches the ROM's vtable slots instead of MSVC's. Nothing in src/ or
 * include/ changes. Run link100, lane CRASH2.
 *
 * The mechanism is the one port/unmatched/MgSmartball_HostAbi.h already uses:
 * pre-define a header's own guard macro, so the src file still says
 * `#include "dFdBrightness_c.h"`, the include still happens, and the body is
 * skipped because the guard is already defined.
 *
 * ---- WHAT IS WRONG WITHOUT THIS ----------------------------------------
 *
 * include/Fader.h declares `virtual ~Fader()` first and then eight more
 * virtuals. mwccarm gives a virtual destructor TWO vtable entries (D1 at slot
 * 0, D0 at slot 1), so the ROM's fader tables are ten words long and
 * IsAtStart, IsAtEnd, SetToEnd and SetToStart are slots 5, 6, 8 and 9 --
 * which is what include/FaderBrightness.h's own comment says out loud:
 * "dScene_c::SetFaders dispatches slot 9 at [vt+0x24] and slot 8 at
 * [vt+0x20]". MSVC folds the destructor pair into ONE slot, so the same
 * source compiled for the host puts those four at 4, 5, 7 and 8. Every fader
 * table the port actually seats is ROM-shaped and ten words long
 * (?vt_HalFaderWipe@@6B@ in port/hal/fader_wipes.cpp, and the hosted ROM table
 * data_0208eb2c in port/hal/scene_boot.cpp), so the caller lands one slot low.
 * This is the shift recorded in the memory note sm64ds-port-msvc-dtor-slot-shift.
 *
 * MEASURED, not inferred. In the shipped artifact
 * ?SetFaders@dScene_c@@SAXPAUFaderBrightness@@@Z at 004d4010 calls [eax+0x10]
 * where it means IsAtStart. On a HalFaderWipe that word is
 * ?SetForwardTime@HalFaderWipe@@UAEHHH@Z at 00416190, a two-int __thiscall
 * member ending `ret 8` at 004161ab. Called with nothing pushed it pops eight
 * bytes of SetFaders' own frame: esp is 001af188 at the call and 001af190
 * after it. SetFaders' epilogue then pops the wrong words and returns into
 * st_binit's tail rather than to its caller, so dScene_c::ResetFadersAndSound
 * never reaches its `mov al, 1`; Stage's vtable slot 1 answers 0; the init
 * dispatcher skips slot 0, which is the level boot; port_stage_a_boot returns
 * null and port/tests/walk_window.cpp:8335 dereferences it on frame one.
 *
 * ---- WHY THIS SHAPE ----------------------------------------------------
 *
 * Only the two slots that TAKE ARGUMENTS can corrupt a frame, because only
 * they clean the stack: a caller that lands one slot low on a no-argument slot
 * gets the wrong answer and keeps its frame. So the fatal call sites are the
 * ones aiming at slot 5 (IsAtStart, landing on SetForwardTime) and slot 4
 * (SetForwardTime, landing on SetBackwardTime). SetFaders is the one such site
 * on the boot path, and it is also the only translation unit left that reaches
 * a fader through the real C++ class at all -- src/_ZN8dScene_c14BeforeBehaviorEv.cpp
 * already declares a file-local explicit-slot `FaderVTable` of cdecl function
 * pointers for exactly this reason, and says so in its own header comment.
 *
 * THIS IS A CALL-SITE FIX, NOT THE WHOLE ANSWER. The complete fix is the
 * pattern the memory note proved byte-neutral over 1,142 objects, applied to
 * include/Fader.h itself:
 *
 *     #ifdef _MSC_VER
 *         virtual void Destructor1();
 *         virtual void Destructor0();
 *     #else
 *         virtual ~Fader();
 *     #endif
 *
 * That would fix every consumer at once and is where this belongs. It is a
 * decomp-side edit, which this lane does not own, so it is written up in
 * out/CRASH2/report.md and handed over instead.
 *
 * The base is left ABSTRACT on purpose: the two destructor slots below are
 * declared and never defined, and nothing here is ever instantiated, so no
 * vtable is emitted for either class and no unresolved external appears.
 * PORTSYNC2: main folded SetFaders into src/game/stages/d_scene.cpp (compiled as
 * its hostgen copy), renamed Fader/FaderBrightness to dFader_c/dFdBrightness_c
 * with new include guards, and that TU now declares fader objects and makes
 * qualified dFdBrightness_c:: calls -- see the two shadows below.
 */
#ifndef PORT_FADER_HOSTABI_H
#define PORT_FADER_HOSTABI_H

#include "types.h"

/* shadows include/dFader_c.h (was include/Fader.h; main's rename, same class
   at the same addresses: _ZTV8dFader_c is data_0208eafc). PORTSYNC2: the guard
   is main's DFADER_C_H now -- the old FADER_H guard no longer matched anything,
   so the real header was read as well. */
#define DFADER_C_H
struct dFader_c {
    Fix12i currInterp;  /* 0x04 -- current fade level, 0..0x1000 */
    Fix12i speed;       /* 0x08 -- per-frame delta; sign selects the target */

    /* The ROM's destructor PAIR, spelled as two ordinary virtuals so MSVC
       gives them the two slots mwccarm gives them and everything below keeps
       its ROM slot number. */
    virtual void Destructor1();                     /* slot 0 */
    virtual void Destructor0();                     /* slot 1 */

    virtual void AdvanceFade() = 0;                 /* slot 2 */
    virtual int  SetBackwardTime(u32 frames) = 0;   /* slot 3 */
    virtual int  SetForwardTime(u32 frames) = 0;    /* slot 4 */
    virtual int  IsAtStart() = 0;                   /* slot 5 */
    virtual int  IsAtEnd() = 0;                     /* slot 6 */
    virtual int  IsBetweenStartAndEnd() = 0;        /* slot 7 */
    virtual void SetToEnd() = 0;                    /* slot 8 */
    virtual void SetToStart() = 0;                  /* slot 9 */
};

/* shadows include/dFdBrightness_c.h (was include/FaderBrightness.h; main's
   rename, _ZTV15dFdBrightness_c is data_0208eacc). The NAME and the `struct`
   keyword are what MSVC's mangling encodes, so
   ?SetFaders@dScene_c@@SAXPAUdFdBrightness_c@@@Z is unchanged.
   PORTSYNC2: main's TU now names this class's own overrides -- qualified,
   non-virtual calls such as `data_0209f5d0.dFdBrightness_c::SetForwardTime(0x10)`
   -- and declares fader OBJECTS (`extern dFdBrightness_c data_0209f5d0;`,
   `extern dFdColor_c data_0209f5e8;`), which an abstract class cannot have.
   So the eight overrides are declared here, with main's signatures: they keep
   slots 2..9 (an override takes its base's slot), the qualified calls bind to
   ?X@dFdBrightness_c@@... as on the ARM, and the class is concrete. The
   constructor is main's own; include/dFdColor_c.h defines dFdColor_c's inline
   constructor through it. Both are inline and the TU constructs no fader, so
   still no vtable is emitted here. */
#define DFDBRIGHTNESS_C_H
struct dFdBrightness_c : dFader_c {
    dFdBrightness_c(Fix12i initial = 0x1000);

    virtual void AdvanceFade();                 /* slot 2 */
    virtual int  SetBackwardTime(u32 frames);   /* slot 3 */
    virtual int  SetForwardTime(u32 frames);    /* slot 4 */
    virtual int  IsAtStart();                   /* slot 5 */
    virtual int  IsAtEnd();                     /* slot 6 */
    virtual int  IsBetweenStartAndEnd();        /* slot 7 */
    virtual void SetToEnd();                    /* slot 8 */
    virtual void SetToStart();                  /* slot 9 */
};
inline dFdBrightness_c::dFdBrightness_c(Fix12i initial) { currInterp = initial; speed = 0; }

#endif
