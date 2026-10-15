/* PORT_HOST_ABI.  The TRAMPOLINE-MARIO sub-object's member-pointer wall, and
 * it is port/mg_fanout_costs.txt section 4's THIRD SHAPE -- the one a link and
 * a `::*` sweep are both blind to.  Run mg11, lane TTE.
 *
 * ---- 1. WHY THIS IS NOT IN THE CLASS'S OWN WALL FILE ----------------------
 *
 * port/unmatched/MgTrampolineTerror_StateDispatch.cpp section 1 scans the ROM
 * for the five-instruction Itanium encoding over dScMgTrampoline2_c's OWN forty
 * bodies and finds exactly ONE site, vtable slot 6.  That measurement is right
 * and this file does not contradict it: THIS body is not one of the class's.
 * Trmp2Mario_Update belongs to the element class the factory constructs five
 * of at this+0x500c, and the ROM names that class itself -- the type_info at
 * 0x0213b238 points at 0x0213b25c, which reads "17dScMgTrmpln2Mario_c", and its
 * sibling at 0x0213b270 reads "18dScMgTrmpln3DMario_c".  The Marios on the
 * trampoline are their own class with their own state machine.
 *
 * AND IT IS INVISIBLE TO BOTH PRESCRIBED DETECTORS.  src/Trmp2Mario_Update.cpp
 * spells the decode in PLAIN INTS --
 *
 *     typedef void (*PMF)(void*);
 *     struct Closure { int off; int adj; };
 *     Closure* cl = (Closure*)(o + 0x70);
 *     void* tobj = o + (cl->adj >> 1);
 *     if (cl->adj & 1) fn = *(void(**)())(*(char**)tobj + cl->off);
 *     else             fn = (void(*)())cl->off;
 *     ((void(*)(void*))fn)(tobj);
 *
 * -- so there is no `::*` to sweep for and no unresolved symbol for a link to
 * name.  It COMPILES and it LINKS and then it jumps to a DS address.  This lane
 * convicted it the only way section 4 says it can be convicted: by a run.  The
 * third boot of scene 385 faulted with
 *
 *     FAULT code c0000005 at +0x01cca310 accessing 020ca310
 *       Trmp2Mario_Update
 *       Trmp2Mario_UpdateAll
 *       func_ov006_02123cb4      <- this class's PLAY state
 *       _ZN18dScMgTrampoline2_c8BehaviorEv      <- vtable slot 6
 *       tte_beh
 *
 * and 0x020ca310 is a real ov006 body: the faulting address IS the pair's code
 * word, called as if it were a host pointer.
 *
 * ---- 2. THE PAIR UNIVERSE, BOUNDED FROM THE ROM'S OWN RUN -----------------
 *
 * The field is o+0x70 and the writers name their sources, so the universe is
 * read off the .data rather than guessed.  Every writer this lane found copies
 * a named mwcc pair or an already-loaded pair:
 *
 *     src/Trmp2Mario_EnterState6.c   *(S2*)(o+0x70)   = pmf_Trmp2Mario_State6
 *     src/Trmp2Mario_EnterState5.c   *(S8*)(o+0x70)   = pmf_Trmp2Mario_State5
 *     src/Trmp2Mario_EnterState3.c   *(Vec2i*)(o+0x70)= pmf_Trmp2Mario_State3_2
 *     src/Trmp2Mario_EnterByKind.cpp   from pmf_Trmp2Mario_State0
 *     src/Trmp2Mario_EnterState1.cpp   from pmf_Trmp2Mario_State1
 *     src/Trmp2Mario_BounceOffLineA.cpp from pmf_Trmp2Mario_State1_2
 *
 * and every one of those symbols sits inside ONE CONTIGUOUS RUN of {code, 0}
 * pairs in ov006's .data, 0x0213b0f4 .. 0x0213b228, read out of
 * extracted/overlays/overlay_0006.bin at base 0x020bfec0.  The run's ends are
 * where it stops being pairs: 0x0213b19c is a type_info record ({0x0209a774,
 * 0x0213b288}) sitting inside it, and 0x0213b22c is a three-word pointer array
 * (0x02140590 / 0x02140560 / 0x02140580).  Every adjustment word in the run is
 * ZERO, so no site here is a virtual member pointer.
 *
 * TWENTY DISTINCT CODE WORDS, and all twenty have a matched src TU with a
 * delinks block.  They are switched below and their TUs are slice lines.  The
 * switch is bounded by the run rather than by the six writers because a field
 * pmf can be written by anything that can reach the object -- the same reason
 * port/unmatched/MgMemory2_FieldPmf.cpp gives -- and over-covering costs a
 * switch arm while under-covering costs a jump to a DS address.
 *
 * WHAT IS NOT COVERED FALLS THROUGH TO THE FRAMEWORK, which prints the address
 * on an UNHANDLED line and counts it (unmatched/MgBase_StateDispatch.cpp).  A
 * run with zero UNHANDLED is the evidence that the twenty are all of them; this
 * header is not.
 *
 * ---- 3. THE STORED PAIR IS NOT REWRITTEN ---------------------------------
 *
 * The seat routes at the DISPATCH SITE.  src/Trmp2Mario_BounceOffLineA.cpp compares
 * the stored words by VALUE (`if (s[0] != q[0] || (s[1] != q[1] && ...))`), so
 * writing host addresses over the .data pairs -- the ov085/ov100 treatment --
 * would leave that comparison permanently false while the dispatch itself
 * looked fine.  Same ruling as MgMemory2_FieldPmf.cpp section 3 and
 * port/mg_fanout_costs.txt section 4's flower case.
 *
 * ---- 4. THE HOST COPY IS ITS src TU WITH TWO LINES CHANGED ---------------
 *
 * The two redundant out-of-line member redeclarations MSVC rejects with C2761
 * (`System* System::New(...);` and `void Animation::Advance();` repeated after
 * the structs that already declare them) are dropped, and the five-line decode
 * is replaced by the routed call.  Every other statement, offset and constant
 * is src's, in src's order.  src/Trmp2Mario_Update.cpp is OUT of
 * port/slice_tte.txt: listing it would be an LNK2005 against this definition.
 * A hostgen MEMBER_REDECL entry
 * for that file would have been the alternative and was tried first; the host
 * copy supersedes it, so port/tools/hostgen.py carries only this lane's
 * HEADER_SHADOW row.
 */

#include <cstdio>

struct MgPmf { unsigned code; int adj; };

extern "C" {

/* the framework's one entry point for a zero-argument state call */
void port_mg_call0(void *self, unsigned code, int adj);

/* the twenty distinct code words in 0x0213b0f4..0x0213b228 */
void Trmp2Mario_State0(void *o);
void Trmp2Mario_State1(void *o);
void Trmp2Mario_State2(void *o);
void Trmp2Mario_State3(void *o);
void Trmp2Mario_State4(void *o);
void Trmp2Mario_State5(void *o);
void Trmp2Mario_State6(void *o);
void Trmp2Mario_State7(void *o);
void Trmp3DMario_State0(void *o);
void Trmp3DMario_State1(void *o);
void Trmp3DMario_State2(void *o);
void Trmp3DMario_State3(void *o);
void Trmp3DMario_State5(void *o);
void Trmp3DMario_State6(void *o);
void Trmp3DMario_State7(void *o);
void Trmp3DMario_State8(void *o);
void Trmp3DMario_State9(void *o);
void Trmp3DMario_State10(void *o);
void Trmp3DMario_State11(void *o);
void Trmp3DMario_State12(void *o);

/* src/Trmp2Mario_Update.cpp's own callees, spelled as that file spells them */
void Trmp2Mario_ApplyGravity(char *o);
void Trmp2Mario_UpdateMatrix(char *o);
extern int data_ov006_0214059c;

void Trmp2Mario_Update(char *o);

}  /* extern "C" */

/* The two names src/Trmp2Mario_Update.cpp reaches through local structs.  The
   Faces file carries the /alternatename rows that make both reachable; here
   they are declared with the same local shapes that file uses so the emitted
   call surface is identical. */
typedef int Fix12;
struct Vector3_16f;
struct Callback;
struct Animation { void Advance(); };
struct System {
    static System *New(unsigned, unsigned, Fix12, Fix12, Fix12,
                       const Vector3_16f *, Callback *);
};

// ---- the field switch -------------------------------------------------------

static unsigned g_tte_sub_calls, g_tte_sub_routed;

static int tte_sub_try(void *self, unsigned code)
{
    switch (code) {
    case 0x020c905cu: Trmp2Mario_State0(self); return 1;
    case 0x020c91acu: Trmp2Mario_State1(self); return 1;
    case 0x020c97bcu: Trmp2Mario_State2(self); return 1;
    case 0x020c9d7cu: Trmp2Mario_State3(self); return 1;
    case 0x020c9efcu: Trmp2Mario_State4(self); return 1;
    case 0x020ca070u: Trmp2Mario_State5(self); return 1;
    case 0x020ca310u: Trmp2Mario_State6(self); return 1;
    case 0x020ca39cu: Trmp2Mario_State7(self); return 1;
    case 0x020cb16cu: Trmp3DMario_State0(self); return 1;
    case 0x020cb2b4u: Trmp3DMario_State1(self); return 1;
    case 0x020cb5c4u: Trmp3DMario_State2(self); return 1;
    case 0x020cb72cu: Trmp3DMario_State3(self); return 1;
    case 0x020cbd7cu: Trmp3DMario_State5(self); return 1;
    case 0x020cc2acu: Trmp3DMario_State6(self); return 1;
    case 0x020cc408u: Trmp3DMario_State7(self); return 1;
    case 0x020cc63cu: Trmp3DMario_State8(self); return 1;
    case 0x020cc724u: Trmp3DMario_State9(self); return 1;
    case 0x020cc9b8u: Trmp3DMario_State10(self); return 1;
    case 0x020ccae0u: Trmp3DMario_State11(self); return 1;
    case 0x020ccd64u: Trmp3DMario_State12(self); return 1;
    default:                                     return 0;
    }
}

extern "C" void port_mg_tte_sub_call(void *self, unsigned code, int adj)
{
    ++g_tte_sub_calls;
    if (code != 0 && adj == 0 && tte_sub_try(self, code)) {
        ++g_tte_sub_routed;
        return;
    }
    port_mg_call0(self, code, adj);
}

/* THE CENSUS ENTRY POINT, run linkfull lane PMFMG1. hal/scene_mg_trampoline2.cpp
   prints these two as "attempted" and "routed". The host copy that counted
   them is retired (below), and every dispatch through the field now lands in
   one of the wrappers unmatched/MgTrmpln3DMario_StateDispatch.cpp seats for
   the shared pair run, which count it; so both numbers are that count (every
   attempt is routed once the words are host words). The switch above stays,
   unreached, for the reason every retired switch in this family stays. */
extern "C" unsigned port_mg_trmpln_field_hits(void);
extern "C" void port_mg_tte_sub_counts(unsigned *calls, unsigned *routed)
{
    const unsigned n = g_tte_sub_calls + port_mg_trmpln_field_hits();
    if (calls)  *calls  = n;
    if (routed) *routed = g_tte_sub_routed + port_mg_trmpln_field_hits();
}

// ---- the host copy -- RETIRED, run linkfull lane PMFMG1 ----------------------
//
// src/Trmp2Mario_Update.cpp runs from src (through hostgen's MEMBER_REDECL row
// for its two C2761 redeclarations). Its pair universe, 0x0213b0f4..0x0213b224,
// is the one dMgTrmpln3DMario_c's dispatcher reads too, and it is seated with
// __cdecl wrappers, one per code word, in
// unmatched/MgTrmpln3DMario_StateDispatch.cpp, whose header says why and has
// the ROM evidence. Section 3's "the stored pair is not rewritten" is answered
// there the same way lane FWD gate 3 answered it one record class over: every
// pair holding a code word gets that code word's one host word, so the
// by-value comparison in src/Trmp2Mario_BounceOffLineA.cpp still answers what the
// cartridge answers.
