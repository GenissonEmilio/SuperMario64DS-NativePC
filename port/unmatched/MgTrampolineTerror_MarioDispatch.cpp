/* PORT_HOST_ABI.  The 3D-MARIO record's member-pointer wall -- the SECOND
 * field-held pmf in dScMgTrampoline2_c's closure, and the one a `::*` sweep
 * finds.  Run mg11, lane TTE.  actor id 0x181 = scene 385.
 *
 * ---- 1. WHERE IT SITS ----------------------------------------------------
 *
 * The factory builds TEN records of 0x1d0 bytes at this+0x5ddc (element ctor
 * TrmpItem_Construct, element dtor Trmp2_DestructJumperModels -- the ROM's own r3 and
 * [sp] columns, see unmatched/MgTrampolineTerror_Factory.cpp).  Each record
 * carries an mwcc member pointer at OFFSET 0, and two TUs dispatch it:
 *
 *     src/TrmpLine_Update.cpp   struct C { PMF pmf; };  (c->*(c->pmf))();
 *     src/TrmpItem_Update.cpp   PMF *pp = (PMF *)c;  (((C *)c)->**pp)();
 *
 * Both are real `T::*` declarations, so the prescribed `::*` sweep finds them.
 * THIS LANE RAN THAT SWEEP OVER ALL 216 TUs of its closure -- the 204 slice
 * lines PLUS the 12 TUs it host-copies or repairs, which are not slice lines and
 * would otherwise never be swept -- and it returns exactly THREE files: these
 * two and src/minigames/d_s_mg_trampoline2.cpp (the class's own vtable slot 6,
 * unmatched/MgTrampolineTerror_StateDispatch.cpp).
 *
 * THE FOURTH WALL IS NOT ONE OF THEM, and saying it was is the error the mg11
 * review caught. src/Trmp2Mario_Update.cpp declares no `T::*` anywhere; it
 * open-codes the decode in plain ints, which is precisely why
 * unmatched/MgTrampolineTerror_SubDispatch.cpp exists and why a RUN was needed
 * to convict it. The lane's sweep tool reports PMF-SHAPED files -- its `::*` arm
 * and its open-coded arm added together -- and reading that total as the `::*`
 * answer put a file inside the detector it is invisible to. FOUR WALLS, THREE
 * FOUND BY THE SWEEP, ONE FOUND BY A RUN.
 *
 * IT WAS FOUND BY A RUN BEFORE THE SWEEP WAS RUN, which is the honest order and
 * the reason the sweep is now written down.  A 3000-frame RENDERED boot of
 * scene 385 faulted with
 *
 *     FAULT code c0000005 at +0x00093129 accessing 3083e230
 *       TrmpItem_Update
 *       TrmpItem_UpdateAll        (walks data_ov006_0214082c, stride 0x1d0)
 *       func_ov006_02123cb4        (this class's PLAY state)
 *       _ZN18dScMgTrampoline2_c8BehaviorEv / tte_beh
 *
 * -- MSVC's four-byte member pointer read half of one eight-byte record and
 * dispatched on it.
 *
 * ---- 2. THE PAIR UNIVERSE, BOUNDED FROM THE ROM'S OWN RUN -----------------
 *
 * Sixteen writers name their source and every one of them is a named mwcc pair
 * in ONE CONTIGUOUS RUN of ov006 .data, 0x0213b31c .. 0x0213b39c, read out of
 * extracted/overlays/overlay_0006.bin at base 0x020bfec0.  A sample of the
 * writers, each `*(struct S *)c = <pair>`:
 *
 *     src/TrmpItem_SetMotion8.c  pmf_TrmpItem_Motion8   {0x020cd9b0, 0}
 *     src/TrmpItem_ProjectToScreen.c  pmf_TrmpItem_GrowScale   {0x020cd744, 0}
 *     src/TrmpItem_SetMotion0.c  pmf_TrmpItem_Motion0   {0x020cdf1c, 0}
 *     src/TrmpItem_SetMotion1.c  pmf_TrmpItem_Motion1   {0x020cdea0, 0}
 *     src/TrmpItem_SetMotion2.c  pmf_TrmpItem_Motion2   {0x020cde4c, 0}
 *     src/TrmpLine_SetStateFlex.c  pmf_TrmpLine_StateFlex   {0x020cfa44, 0}
 *     src/TrmpItem_SetMotion7.c  pmf_TrmpItem_Motion7   {0x020cd864, 0}
 *     src/TrmpLine_SetStateSettle.c  pmf_TrmpLine_StateSettle   {0x020cf820, 0}
 *     src/TrmpLine_SetStateFade.c  pmf_TrmpLine_StateFade   {0x020cf790, 0}
 *     src/TrmpItem_SetMotion4.c  pmf_TrmpItem_Motion4   {0x020cdc8c, 0}
 *     src/TrmpItem_SetMotion6.c  pmf_TrmpItem_Motion6   {0x020cdc38, 0}
 *     src/TrmpItem_SetMotion5.c  pmf_TrmpItem_Motion5   {0x020cdaec, 0}
 *     src/TrmpItem_SetMotion3.c  pmf_TrmpItem_Motion3   {0x020cdd08, 0}
 *
 * The run's ENDS are where it stops being pairs: 0x0213b314 is the ASCII word
 * 0x676e6972 ("ring") and 0x0213b3a4 is a five-word record ({0x1e, 0x0212e05c,
 * 0x0212e054, 0x0212e130, 1, 0x0212e094}) that is plainly a table of pointers
 * and counts.  Every adjustment word inside the run is ZERO.
 *
 * FOURTEEN DISTINCT CODE WORDS, all with a matched src TU and a delinks block;
 * all fourteen are switched below and their TUs are slice lines.  Anything not
 * covered falls through to the framework, which prints the address on an
 * UNHANDLED line and counts it -- a run with zero UNHANDLED is the evidence,
 * this header is not.
 *
 * ---- 3. THE STORED PAIR IS NOT REWRITTEN ---------------------------------
 *
 * Routing happens at the two dispatch SITES.  The pairs stay the ROM's own
 * words, for the reason unmatched/MgMemory2_FieldPmf.cpp section 3 gives and
 * port/mg_fanout_costs.txt section 4 derives from the flower: sixteen writers
 * copy these records by value and at least one consumer in this family compares
 * a stored pair word for word (src/Trmp2Mario_BounceOffLineA.cpp does it one record
 * class over), so host addresses in the .data would make a comparison
 * permanently false while the dispatch itself looked fine.
 *
 * ---- 4. THE TWO HOST COPIES ARE THEIR src TUs WITH THE DISPATCH ROUTED ----
 *
 * TrmpLine_Update is one statement and the statement is the dispatch.
 * TrmpItem_Update keeps every other line verbatim -- the AddVec3 on
 * c+8 / c+0x2c, the Animation::Advance at c+0x194 and the nine field copies, in
 * src's order and at src's offsets.  Both src TUs are OUT of
 * port/slice_tte.txt: listing either would be an LNK2005.
 */

#include <cstdio>
#include <cstdlib>   /* run link100 lane FWD gate 3: the seat aborts on wrong bytes */

struct MgPmf { unsigned code; int adj; };

extern "C" {

void port_mg_call0(void *self, unsigned code, int adj);

/* the fourteen distinct code words in 0x0213b31c..0x0213b39c */
void TrmpItem_GrowScale(void *c);
void TrmpItem_Motion7(void *c);
void TrmpItem_Motion8(void *c);
void TrmpItem_Motion5(void *c);
void TrmpItem_Motion6(void *c);
void TrmpItem_Motion4(void *c);
void TrmpItem_Motion3(void *c);
void TrmpItem_Motion2(void *c);
void TrmpItem_Motion1(void *c);
void TrmpItem_Motion0(void *c);
void TrmpItem_StateAppear(void *c);
void TrmpLine_StateFade(void *c);
void TrmpLine_StateSettle(void *c);
void TrmpLine_StateFlex(void *c);

/* src/TrmpItem_Update.cpp's own callees, spelled as that file spells them */
void AddVec3(void *a, void *b, void *c);
void _ZN15dExtFrameCtrl_c7AdvanceEv(void *anim);

/* run link100 lane FWD gate 3: both are src TUs on port/slice_fwd.txt now,
   not definitions in this file. The declarations stay because the class's
   census and its factory both name them. */
void TrmpItem_Update(char *c);
void TrmpLine_Update(void *c);

}  /* extern "C" */

static unsigned g_tte_mario_calls, g_tte_mario_routed;

static int tte_mario_try(void *self, unsigned code)
{
    switch (code) {
    case 0x020cd744u: TrmpItem_GrowScale(self); return 1;
    case 0x020cd864u: TrmpItem_Motion7(self); return 1;
    case 0x020cd9b0u: TrmpItem_Motion8(self); return 1;
    case 0x020cdaecu: TrmpItem_Motion5(self); return 1;
    case 0x020cdc38u: TrmpItem_Motion6(self); return 1;
    case 0x020cdc8cu: TrmpItem_Motion4(self); return 1;
    case 0x020cdd08u: TrmpItem_Motion3(self); return 1;
    case 0x020cde4cu: TrmpItem_Motion2(self); return 1;
    case 0x020cdea0u: TrmpItem_Motion1(self); return 1;
    case 0x020cdf1cu: TrmpItem_Motion0(self); return 1;
    case 0x020cdf3cu: TrmpItem_StateAppear(self); return 1;
    case 0x020cf790u: TrmpLine_StateFade(self); return 1;
    case 0x020cf820u: TrmpLine_StateSettle(self); return 1;
    case 0x020cfa44u: TrmpLine_StateFlex(self); return 1;
    default:                                     return 0;
    }
}

extern "C" void port_mg_tte_mario_call(void *self, unsigned code, int adj)
{
    ++g_tte_mario_calls;
    if (code != 0 && adj == 0 && tte_mario_try(self, code)) {
        ++g_tte_mario_routed;
        return;
    }
    port_mg_call0(self, code, adj);
}

extern "C" void port_mg_tte_mario_counts(unsigned *calls, unsigned *routed)
{
    if (calls)  *calls  = g_tte_mario_calls;
    if (routed) *routed = g_tte_mario_routed;
}


/* ==== RUN link100, LANE FWD gate 3: THE SEVENTEEN PAIRS ARE SEATED ========
 *
 * The two host copies below are retired and src/TrmpLine_Update.cpp and
 * src/TrmpItem_Update.cpp dispatch the field themselves. Both emit
 *     mov ecx, [obj+4] / mov eax, [obj] / add ecx, obj / jmp-or-call eax
 * -- receiver in ecx, ARITY ZERO, nothing pushed (read off their own /FAsc
 * listings, runs/link100/out/FWD/emit_gate3_out.txt; 020cf758 tail-JUMPS and
 * 020cea2c CALLS, and one set of zero-argument __fastcall faces is right for
 * both). So the field has to hold a host address, which means the .data pairs
 * the sixteen writers copy from have to hold one.
 *
 * THE UNIVERSE IS BOUNDED, which is what lane MGWRITER left open. Over this
 * class's own code block 020cd744..020d1008 -- containing all fourteen code
 * words, both dispatch sites (020cea2c, 020cf758) and both record walkers
 * (020cedf0 at stride 0x1d0, 020d0ac0 at stride 0x32c) -- every `load`
 * relocation in ov006 landing in the block is followed by a zero word in
 * exactly SEVENTEEN places, and all seventeen are the run 0213b31c..0213b39c.
 * Nothing else in ov006 can hand either field an address in this class.
 * runs/link100/out/FWD/sweep_tramp_narrow.txt is the sweep; the wider sweep
 * from 020cc198 in sweep_tramp.txt shows the sibling class's own run next door
 * and is kept as the control.
 *
 * ONE FACE PER CODE WORD, NOT PER PAIR. Three of the seventeen are BY-VALUE
 * COMPARISON SENTINELS -- src/TrmpItem_HitTest2D.c compares the object's
 * stored pair against pmf_TrmpItem_StateAppear and _0213b334 word for word, and
 * src/TrmpItem_ReflectOffLine.cpp against _0213b33c -- and each holds the same code
 * word as a WRITTEN pair (b324 and b344 both hold 020cdf3c; b334, b33c and b32c
 * all hold 020cd744). A face per pair would make those comparisons permanently
 * false while the dispatch still looked fine, which is section 3's own warning
 * come true. A face per code word moves both sides of every comparison
 * together. That is the reverse of the rule PMFB5 wrote for TABLES, where two
 * cells holding one word had to stay distinguishable; the difference is that
 * nothing compares a table cell by value.
 *
 * THE COUNTERS MOVE WITH THE DISPATCH. g_tte_mario_calls and
 * g_tte_mario_routed are what hal/scene_mg_trampoline2.cpp:721 prints, and the
 * switch below is no longer reached from inside this file, so each face bumps
 * both. A counter left behind a seat reads zero forever and nothing looks
 * wrong -- lane PMFB3's tapped-pad trap.
 * ======================================================================== */
#define TTE_FACE(sym)                                                     \
    static void __fastcall tte_face_##sym(void *self, void *dead_edx)     \
    {                                                                     \
        (void)dead_edx;                                                   \
        ++g_tte_mario_calls;                                              \
        ++g_tte_mario_routed;                                             \
        sym(self);                                                        \
    }

TTE_FACE(TrmpItem_GrowScale)
TTE_FACE(TrmpItem_Motion7)
TTE_FACE(TrmpItem_Motion8)
TTE_FACE(TrmpItem_Motion5)
TTE_FACE(TrmpItem_Motion6)
TTE_FACE(TrmpItem_Motion4)
TTE_FACE(TrmpItem_Motion3)
TTE_FACE(TrmpItem_Motion2)
TTE_FACE(TrmpItem_Motion1)
TTE_FACE(TrmpItem_Motion0)
TTE_FACE(TrmpItem_StateAppear)
TTE_FACE(TrmpLine_StateFade)
TTE_FACE(TrmpLine_StateSettle)
TTE_FACE(TrmpLine_StateFlex)

extern "C" {
/* the seventeen source pairs, mounted from ov006 .data */
extern MgPmf pmf_TrmpItem_Motion8;   /* written  src/TrmpItem_SetMotion8.c */
extern MgPmf pmf_TrmpItem_StateAppear;   /* SENTINEL src/TrmpItem_HitTest2D.c */
extern MgPmf pmf_TrmpItem_GrowScale;   /* written  src/TrmpItem_ProjectToScreen.c */
extern MgPmf pmf_TrmpItem_GrowScale_2;   /* SENTINEL src/TrmpItem_HitTest2D.c */
extern MgPmf pmf_TrmpItem_GrowScale_3;   /* SENTINEL src/TrmpItem_ReflectOffLine.cpp */
extern MgPmf pmf_TrmpItem_StateAppear_2;   /* written  src/TrmpItem_StartAppear.cpp */
extern MgPmf pmf_TrmpItem_Motion0;   /* written  src/TrmpItem_SetMotion0.c */
extern MgPmf pmf_TrmpItem_Motion1;   /* written  src/TrmpItem_SetMotion1.c */
extern MgPmf pmf_TrmpItem_Motion2;   /* written  src/TrmpItem_SetMotion2.c */
extern MgPmf pmf_TrmpLine_StateFlex;   /* written  src/TrmpLine_SetStateFlex.c */
extern MgPmf pmf_TrmpItem_Motion7;   /* written  src/TrmpItem_SetMotion7.c */
extern MgPmf pmf_TrmpLine_StateSettle;   /* written  src/TrmpLine_SetStateSettle.c */
extern MgPmf pmf_TrmpLine_StateFade;   /* written  src/TrmpLine_SetStateFade.c */
extern MgPmf pmf_TrmpItem_Motion4;   /* written  src/TrmpItem_SetMotion4.c */
extern MgPmf pmf_TrmpItem_Motion6;   /* written  src/TrmpItem_SetMotion6.c */
extern MgPmf pmf_TrmpItem_Motion5;   /* written  src/TrmpItem_SetMotion5.c */
extern MgPmf pmf_TrmpItem_Motion3;   /* written  src/TrmpItem_SetMotion3.c */
}

extern "C" void port_mg_tte_pairs_seat(void)
{
    static int done;
    if (done)
        return;
    done = 1;

    static const struct { MgPmf *p; unsigned rom; void *face;
                         const char *what; } seats[] = {
        {&pmf_TrmpItem_Motion8, 0x020cd9b0u, (void *)tte_face_TrmpItem_Motion8,
         "0213b31c written  src/TrmpItem_SetMotion8.c"},
        {&pmf_TrmpItem_StateAppear, 0x020cdf3cu, (void *)tte_face_TrmpItem_StateAppear,
         "0213b324 SENTINEL src/TrmpItem_HitTest2D.c"},
        {&pmf_TrmpItem_GrowScale, 0x020cd744u, (void *)tte_face_TrmpItem_GrowScale,
         "0213b32c written  src/TrmpItem_ProjectToScreen.c"},
        {&pmf_TrmpItem_GrowScale_2, 0x020cd744u, (void *)tte_face_TrmpItem_GrowScale,
         "0213b334 SENTINEL src/TrmpItem_HitTest2D.c"},
        {&pmf_TrmpItem_GrowScale_3, 0x020cd744u, (void *)tte_face_TrmpItem_GrowScale,
         "0213b33c SENTINEL src/TrmpItem_ReflectOffLine.cpp"},
        {&pmf_TrmpItem_StateAppear_2, 0x020cdf3cu, (void *)tte_face_TrmpItem_StateAppear,
         "0213b344 written  src/TrmpItem_StartAppear.cpp"},
        {&pmf_TrmpItem_Motion0, 0x020cdf1cu, (void *)tte_face_TrmpItem_Motion0,
         "0213b34c written  src/TrmpItem_SetMotion0.c"},
        {&pmf_TrmpItem_Motion1, 0x020cdea0u, (void *)tte_face_TrmpItem_Motion1,
         "0213b354 written  src/TrmpItem_SetMotion1.c"},
        {&pmf_TrmpItem_Motion2, 0x020cde4cu, (void *)tte_face_TrmpItem_Motion2,
         "0213b35c written  src/TrmpItem_SetMotion2.c"},
        {&pmf_TrmpLine_StateFlex, 0x020cfa44u, (void *)tte_face_TrmpLine_StateFlex,
         "0213b364 written  src/TrmpLine_SetStateFlex.c"},
        {&pmf_TrmpItem_Motion7, 0x020cd864u, (void *)tte_face_TrmpItem_Motion7,
         "0213b36c written  src/TrmpItem_SetMotion7.c"},
        {&pmf_TrmpLine_StateSettle, 0x020cf820u, (void *)tte_face_TrmpLine_StateSettle,
         "0213b374 written  src/TrmpLine_SetStateSettle.c"},
        {&pmf_TrmpLine_StateFade, 0x020cf790u, (void *)tte_face_TrmpLine_StateFade,
         "0213b37c written  src/TrmpLine_SetStateFade.c"},
        {&pmf_TrmpItem_Motion4, 0x020cdc8cu, (void *)tte_face_TrmpItem_Motion4,
         "0213b384 written  src/TrmpItem_SetMotion4.c"},
        {&pmf_TrmpItem_Motion6, 0x020cdc38u, (void *)tte_face_TrmpItem_Motion6,
         "0213b38c written  src/TrmpItem_SetMotion6.c"},
        {&pmf_TrmpItem_Motion5, 0x020cdaecu, (void *)tte_face_TrmpItem_Motion5,
         "0213b394 written  src/TrmpItem_SetMotion5.c"},
        {&pmf_TrmpItem_Motion3, 0x020cdd08u, (void *)tte_face_TrmpItem_Motion3,
         "0213b39c written  src/TrmpItem_SetMotion3.c"},
    };

    for (unsigned i = 0; i < sizeof seats / sizeof seats[0]; ++i) {
        if ((unsigned)(seats[i].p->code) < 0x02000000u || (unsigned)(seats[i].p->code) >= 0x02400000u) continue;  /* PORTSYNC3: a mount word that is not a DS code address: the class TU owns the live table */
        if (seats[i].p->code != seats[i].rom || seats[i].p->adj != 0) {
            std::fprintf(stderr, "FATAL: dScMgTrampoline2_c source pair "
                         "%s: the mount holds %08x/%d, the ROM's own "
                         "word says %08x/0 -- WRONG BYTES\n",
                         seats[i].what, seats[i].p->code, seats[i].p->adj,
                         seats[i].rom);
            std::abort();
        }
        seats[i].p->code = (unsigned)(size_t)seats[i].face;
    }
}
/* HOST COPY RETIRED, run link100 lane FWD gate 3.
   src/TrmpLine_Update.cpp is on port/slice_fwd.txt and dispatches the
   field itself; its one statement IS the dispatch, and with the pairs seated
   the word it jumps to is a host face. */

/* HOST COPY RETIRED, run link100 lane FWD gate 3.
   src/TrmpItem_Update.cpp is on port/slice_fwd.txt. The nine field copies,
   the AddVec3 on c+8 / c+0x2c and the Animation::Advance at c+0x194 that this
   copy carried verbatim are the matched TU's own again.

   THE SWITCH ABOVE STAYS AND IS NO LONGER REACHED FROM INSIDE THIS FILE.
   port_mg_tte_mario_call is this class's C-linkage entry point and nothing
   about it moves; the framework's UNHANDLED report still stands behind it. The
   two counters it kept are bumped by the faces now, so
   hal/scene_mg_trampoline2.cpp's census keeps counting the dispatches that
   actually happen. */
