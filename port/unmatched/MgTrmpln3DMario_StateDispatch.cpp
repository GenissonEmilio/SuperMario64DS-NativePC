// PORT_HOST_ABI.  The mwcc POINTER-TO-MEMBER WALL, SECOND LEVEL:
// dMgTrmpln3DMario_c's dispatcher and its twenty-one state bodies.
// Run mg11, lane TTI.
//
// Read port/unmatched/MgTrampolineTime_StateDispatch.cpp first -- it is the
// OUTER class's half of the same wall, and the framework entry point both files
// chain to is documented in unmatched/MgBase_StateDispatch.cpp.
//
// ---- 1. WHY A SECOND DISPATCHER EXISTS AT ALL ----------------------------
//
// dScMgTrampoline_c's factory builds FOUR records of 0xd0 bytes at this+0x500c
// whose element vtable is data_ov006_0213b2c4 (port/slice_tti.txt section 8),
// and the ROM's type_info at 0x0213b2c0 names that class dMgTrmpln3DMario_c --
// the four bouncing characters.  THOSE RECORDS HAVE A STATE MACHINE OF THEIR
// OWN, of exactly the same shape as their owner's: a FIELD-HELD member pointer,
// here at element+0x64 (code) and element+0x68 (adjustment), dispatched from one
// place.
//
// So this class is TWO LEVELS DEEP, which is the question
// port/mg_fanout_costs.txt section 14 tells a lane to ask and the answer here
// is YES.  The outer machine is five links; this one is twenty-one bodies over
// THIRTY-EIGHT pair slots.  (0x0213b224 - 0x0213b0f4) / 8 + 1 = 39 slots in
// the run, less the one non-pair inside it (0x0213b19c) = 38.  Three committed
// places said thirty-seven; section 4 records why the lane's own verifier did
// not catch it.
//
// ---- 2. THE ONE DECODE SITE, FOUND BY THE IMAGE --------------------------
//
// The five-instruction ARM Itanium encoding scan (the detector lane LKY
// invented, blind to spelling) over the element machinery 0x020caf00..
// 0x020cd800, body by body at each body's own start and size:
//
//     Trmp3DMario_Update   ONE site, at 0x020cb068
//
// THE ADDRESS A DECODE SITE IS REPORTED AT IS THE `add`, the FIRST instruction
// of the five-instruction sequence, in every place this lane names one.  The
// detector matches on the add/ands pair, so the add is what it has; the `blx`
// is three or five instructions later and is a different number for the same
// site.  Both sequences in full:
//
//     _ZN17dScMgTrampoline_c8BehaviorEv   add 0x0212151c   ands 0x02121520   blx 0x02121534
//     Trmp3DMario_Update   add 0x020cb068   ands 0x020cb06c   blx 0x020cb080
//
// An earlier version of the element file reported ITS site at the blx while
// this class's was reported at the add, so two numbers that look like the same
// kind of thing were pointing at different instructions.
//
// and nothing else in that range.  Every other `blx` in it is an ordinary
// vtable dispatch (`ldr r1,[r0] / ldr r1,[r1,#imm] / blx r1`), which is a
// different shape and is not this wall.
//
// ---- 3. THE src IS ALREADY OPEN-CODED, AND THAT IS THE DANGEROUS PART ----
//
// src/Trmp3DMario_Update.cpp does NOT use a member-pointer type.  It spells
// the pair by hand:
//
//     struct Closure { int off; int adj; };
//     Closure* cl = (Closure*)(o + 0x64);
//     void* tobj = o + (cl->adj >> 1);
//     if (cl->adj & 1) fn = *(void(**)())(*(char**)tobj + cl->off);
//     else             fn = (void(*)())cl->off;
//     ((void(*)(void*))fn)(tobj);
//
// which is a FAITHFUL reading of the ROM and the third of the three shapes
// port/mg_fanout_costs.txt section 4 names -- the one both prescribed detectors
// (a link, a `::*` sweep) are blind to, because there is no member pointer
// anywhere for either to find.  The stride is right, the virtual bit is right,
// the adjustment is right.  What is wrong on the host is the last line: `off`
// holds a DS CODE ADDRESS, so the call jumps into nothing.  It would link
// clean, compile clean, boot, and jump to 0x020cXXXX the first frame an element
// had a state.
//
// THE PORT CANNOT COMPILE THAT src ANYWAY, which is how this file came to be
// written before the run rather than after it:
//
//     src\Trmp3DMario_Update.cpp(14): error C2761: 'System *System::New(...)':
//     redeclaration of member is not allowed
//     src\Trmp3DMario_Update.cpp(15): error C2761: 'void Animation::Advance()'
//
// The TU declares two class members inside the class and then RE-DECLARES them
// at namespace scope.  mwcc accepted it; MSVC does not, and it is right.  So
// src/Trmp3DMario_Update.cpp is out of port/slice_tti.txt and the body below
// takes its symbol -- the unmatched/MgBSC_StateDispatch.cpp convention for "the
// port cannot compile the src".
//
// ---- 4. THE TABLE, READ OFF THE READING POOLS AND NOT OFF A SWEEP --------
//
// There is no overlay constructor: ov006's .init (0x0212f4c4..0x0213356c) has
// NO relocation into this class's data, the same as the outer class.  The pairs
// are statically relocated .data at an eight-byte stride from 0x0213b0f4 to
// 0x0213b224, and each one is identified by the literal pool that LOADS it --
// THIRTY-EIGHT slots, TWENTY-ONE distinct code words, every adjustment zero.
//
// THE COUNT WAS WRONG IN THREE COMMITTED PLACES AND THE LANE'S OWN VERIFIER
// COULD NOT HAVE CAUGHT IT: tmp/tti/verify.py checked that the distinct code
// words number 21 and that every adjustment is zero, and it never counted the
// slots.  A self-audit catches only what it measures.  The slot-count check
// is in verify.py now.
//
// A SWEEP OF THE SPAN WOULD HAVE COLLECTED THREE WORDS THAT ARE NOT PAIRS,
// which is the curling trap for the third time in this lane:
//
//     0x0213b0ec  {3, 3}          two ints. Read by Trmp3DMario_State6,
//                                 Trmp3DMario_ResetAll, and by the OUTER class's
//                                 slot 9 and chain link 1.
//     0x0213b19c  {0209a774, 0213b288}   the type_info of dMgTrmpObjAdapter_c,
//                                 this element class's own base.
//     0x0213b22c  {02140590, 02140560}   two .bss ADDRESSES, not code.
//
// ALL TWENTY-ONE CODE WORDS REACH A MATCHED src TU, so this machine has NO
// state floor, and the switch below is complete rather than partial.  The
// bodiless counter is kept for the reason every other one in the tree is kept.
//
// ---- 5. THE ARITY IS ZERO AND THE RECEIVER IS THE ADJUSTED POINTER -------
//
// The ROM's dispatch is `blx r1` with r0 = `o + (adj >> 1)` and nothing written
// to r1..r3 between the decode and the call, so every state is called with the
// element pointer alone.  Every stored adjustment is zero, so in practice the
// receiver IS the element, and the shift is implemented anyway rather than
// folded away: an adjustment this lane did not find would otherwise be silently
// ignored instead of reported.
//
// ---- 5b. THE src CALLS THE WRONG TWIN, AND THE HOST COPY DOES NOT -------
//
// src/Trmp3DMario_Update.cpp's second-to-last statement is
//
//     Trmp2Mario_ApplyGravity(o);
//
// and the ROM's is not.  config/arm9/overlays/ov006/relocs.txt has exactly one
// arm_call out of this body into that neighbourhood --
// `from:0x020cb088 kind:arm_call to:0x020cb134` -- and 0x020c9024 appears
// nowhere in the body's relocations at all.
//
// THE TWO ARE NOT THE SAME FUNCTION, which is what makes it worth writing down
// rather than filing as a typo.  Both are 0x38 bytes and both are
// ApproachLinear-then-AddVec3, and their bytes DIFFER:
//
//     Trmp2Mario_ApplyGravity   ApproachLinear(o+0x40, data_ov006_021405b4[0],
//                           *(o+0x48));  AddVec3(o+0x24, o+0x3c, o+0x24)
//     Trmp3DMario_ApplyGravity   ApproachLinear(o+0x38, data_ov006_02140548[0],
//                           *(o+0x40));  AddVec3(o+0x1c, o+0x34, o+0x1c)
//
// -- different offsets, different global.  The element object this dispatcher
// runs on is the one whose +0x1c/+0x20/+0x24 the first three statements of this
// very body copy to +0x28/+0x2c/+0x30, and whose +0x38 it compares three
// statements later.  Those are 0x020cb134's offsets, not 0x020c9024's.  A port
// that compiled the src would have integrated the wrong vector every frame, on
// an object whose fields the wrong body reads eight bytes further along.
//
// This is a DECOMP-SIDE correction to route, in the same family as the
// receiver-drop findings run mg9 lane BOX banked: the body is otherwise
// faithful, and the byte gate cannot see a wrong callee because the two
// candidates are the same size and the relocation is what distinguishes them.
//
// ---- 6. IT IS SHARED WITH THE SIBLING, AND THE FILE SAYS SO -------------
//
// data_ov006_0213b0ec and data_ov006_0213b0f0 are read by dScMgTrampoline2_c's
// bodies too (0x02123330 and 0x02124014), so the element machinery is common to
// the two trampoline classes even though the ELEMENT ARRAY is not -- only this
// class's factory calls Trmp3DMario_Construct, the element constructor that
// installs data_ov006_0213b2c4.  Lane TTE must CALL this file rather than write
// a second: a second definition of Trmp3DMario_Update is an LNK2005 and two
// switches over the same twenty-one addresses would split the witness.

#include <cstdio>
#include <cstdlib>   /* std::abort, for the field seat (run linkfull lane PMFMG1) */

struct MgPmf { unsigned code; int adj; };

extern "C" {

/* the framework's entry point; see MgBase_StateDispatch.cpp */
void port_mg_call0(void *self, unsigned code, int adj);

/* ---- the twenty-one routed state bodies, in address order ---------------
   Each is declared with the parameter list ITS OWN src TU defines. */
void Trmp2Mario_State0(char *o);
void Trmp2Mario_State1(char *c);
void Trmp2Mario_State2(char *c);
void Trmp2Mario_State3(char *c);
void Trmp2Mario_State4(char *c);
void Trmp2Mario_State5(char *c);
void Trmp2Mario_State6(char *c);
void Trmp2Mario_State7(int *p);
void Trmp3DMario_State0(int *c);
void Trmp3DMario_State1(void *self);
void Trmp3DMario_State2(char *c);
void Trmp3DMario_State3(char *c);
void Trmp3DMario_State4(char *c);
void Trmp3DMario_State5(char *c);
void Trmp3DMario_State6(char *c);
void Trmp3DMario_State7(char *c);
void Trmp3DMario_State8(int *c);
void Trmp3DMario_State9(char *c);
void Trmp3DMario_State10(int *c);
void Trmp3DMario_State11(char *c);
void Trmp3DMario_State12(char *p);

/* the ordinary callees the host copy keeps, spelled as their own src TUs do */
void Trmp3DMario_ApplyGravity(char *o);
void Trmp3DMario_UpdateMatrix(char *o);
extern int data_ov006_0214059c;
int  _ZN8Particle6System3NewEjj5Fix12IiES2_S2_PK11Vector3_16fPNS_8CallbackE(
        int a, unsigned b, int x, int y, int z, const void *v, void *cb);
void _ZN15dExtFrameCtrl_c7AdvanceEv(void *self);

}  /* extern "C" */

// ---- the element class's address switch ------------------------------------

static unsigned g_mario_state_hits;
static unsigned g_mario_floor_hits;   /* structurally zero -- section 4 */
static unsigned g_mario_unrouted;
static unsigned g_mario_virtual_arm;

static int mario_try_0(void *self, unsigned code)
{
    char *c = (char *)self;
    switch (code) {
    case 0x020c905cu: Trmp2Mario_State0(c);            return 1;
    case 0x020c91acu: Trmp2Mario_State1(c);            return 1;
    case 0x020c97bcu: Trmp2Mario_State2(c);            return 1;
    case 0x020c9d7cu: Trmp2Mario_State3(c);            return 1;
    case 0x020c9efcu: Trmp2Mario_State4(c);            return 1;
    case 0x020ca070u: Trmp2Mario_State5(c);            return 1;
    case 0x020ca310u: Trmp2Mario_State6(c);            return 1;
    case 0x020ca39cu: Trmp2Mario_State7((int *)c);     return 1;
    case 0x020cb16cu: Trmp3DMario_State0((int *)c);     return 1;
    case 0x020cb2b4u: Trmp3DMario_State1(c);            return 1;
    case 0x020cb5c4u: Trmp3DMario_State2(c);            return 1;
    case 0x020cb72cu: Trmp3DMario_State3(c);            return 1;
    case 0x020cb838u: Trmp3DMario_State4(c);            return 1;
    case 0x020cbd7cu: Trmp3DMario_State5(c);            return 1;
    case 0x020cc2acu: Trmp3DMario_State6(c);            return 1;
    case 0x020cc408u: Trmp3DMario_State7(c);            return 1;
    case 0x020cc63cu: Trmp3DMario_State8((int *)c);     return 1;
    case 0x020cc724u: Trmp3DMario_State9(c);            return 1;
    case 0x020cc9b8u: Trmp3DMario_State10((int *)c);     return 1;
    case 0x020ccae0u: Trmp3DMario_State11(c);            return 1;
    case 0x020ccd64u: Trmp3DMario_State12(c);            return 1;
    default:                                             return 0;
    }
}

extern "C" void port_mg_mario_call0(void *self, unsigned code, int adj)
{
    if (adj != 0) ++g_mario_virtual_arm;
    if (code != 0 && adj == 0 && mario_try_0(self, code)) {
        ++g_mario_state_hits;
        return;
    }
    if (code != 0) ++g_mario_unrouted;
    port_mg_call0(self, code, adj);
}

extern "C" void port_mg_mario_counts(unsigned *hits, unsigned *floor,
                                     unsigned *unrouted, unsigned *virt)
{
    *hits     = g_mario_state_hits;
    *floor    = g_mario_floor_hits;
    *unrouted = g_mario_unrouted;
    *virt     = g_mario_virtual_arm;
}

// ---- THE FIELD IS SEATED AND BOTH HOST COPIES ARE GONE, run linkfull lane PMFMG1
//
// src/Trmp3DMario_Update.cpp (this class's element dispatcher) and
// src/Trmp2Mario_Update.cpp (dScMgTrmpln2Mario_c's, Trampoline Terror's
// element: unmatched/MgTrampolineTerror_SubDispatch.cpp) both run from src now.
// They read ONE pair universe, which is why the seat is here and serves both,
// as section 6 above asks.
//
// ---- 1. THE UNIVERSE, CLOSED BY THE ROM -----------------------------------
//
// runs/linkfull/out/PMFMG1/rom_records.txt reads all THIRTY-EIGHT slots of
// 0x0213b0f4..0x0213b224 (the run section 4 bounds, 0x0213b19c excluded) out
// of overlay_0006.bin with their relocations: every one {load-relocated code
// word, 0}, nothing relocated from +4, TWENTY-ONE distinct code words. The
// address-targeted sweep over the whole overlay for those twenty-one
// (rom_records_sweep.txt) finds exactly these thirty-eight pairs and no other;
// rom_trmpln_code_refs.txt lists every other relocation that reaches one of the
// twenty-one: eight arm_call rows (plain calls of a body) and three literal
// pool words, each a tail call at the end of a two-line writer
// (020ca2ec, 020cb814, 020cc618: `*field = pair; body(this);`). No code
// compares the field against a DS constant.
//
// ---- 2. ONE HOST WORD PER CODE WORD --------------------------------------
//
// The field is copied BY VALUE (thirty-odd writers) and compared by value
// (src/Trmp2Mario_BounceOffLineA.cpp compares a stored pair with a pair of the run),
// so each code word maps to ONE wrapper, written into every pair that holds
// it. Both sides of every comparison move together and a comparison answers
// what the cartridge answers: the MgMemory2_FieldPmf.cpp / FWD gate 3 rule.
//
// ---- 3. __cdecl, OFF BOTH LISTINGS ----------------------------------------
//
// Both TUs compile (under hostgen's MEMBER_REDECL rows for their two C2761s)
// to `push ecx / call edx / add esp, 4` (runs/linkfull/out/PMFMG1/
// Trmp3DMario_Update.asm, Trmp2Mario_Update.asm): the receiver pushed and
// the caller cleaning it. So the cells hold __cdecl wrappers, one per code
// word, each counting what the two switches counted and calling the body with
// its own src parameter list. Both src TUs keep their own null guard on the
// code word, as the ROM does.
//
// ---- 4. THE WRONG TWIN IS FIXED IN src -----------------------------------
//
// Section 5b's finding (020c9024 called where the ROM calls 020cb134) is fixed
// in src/Trmp3DMario_Update.cpp itself; its comment says so and its
// relocation row 0x020cb088 -> 0x020cb134 agrees.

static unsigned g_trmpln_field_hits;

/* PORTDROP2: main renamed these (symbols.txt, by address); the macro below pastes the
   old address-spelled name, so each is aliased to its new name here. */
#define func_ov006_020c905c Trmp2Mario_State0
#define func_ov006_020c91ac Trmp2Mario_State1
#define func_ov006_020c97bc Trmp2Mario_State2
#define func_ov006_020c9d7c Trmp2Mario_State3
#define func_ov006_020c9efc Trmp2Mario_State4
#define func_ov006_020ca070 Trmp2Mario_State5
#define func_ov006_020ca310 Trmp2Mario_State6
#define func_ov006_020ca39c Trmp2Mario_State7
#define func_ov006_020cb16c Trmp3DMario_State0
#define func_ov006_020cb2b4 Trmp3DMario_State1
#define func_ov006_020cb5c4 Trmp3DMario_State2
#define func_ov006_020cb72c Trmp3DMario_State3
#define func_ov006_020cb838 Trmp3DMario_State4
#define func_ov006_020cbd7c Trmp3DMario_State5
#define func_ov006_020cc2ac Trmp3DMario_State6
#define func_ov006_020cc408 Trmp3DMario_State7
#define func_ov006_020cc63c Trmp3DMario_State8
#define func_ov006_020cc724 Trmp3DMario_State9
#define func_ov006_020cc9b8 Trmp3DMario_State10
#define func_ov006_020ccae0 Trmp3DMario_State11
#define func_ov006_020ccd64 Trmp3DMario_State12

#define TRM_W(code, cast)                                                  \
    static void tw_##code(void *o)                                        \
    {                                                                     \
        ++g_mario_state_hits;                                             \
        ++g_trmpln_field_hits;                                            \
        func_ov006_##code(cast o);                                        \
    }
TRM_W(020c905c, (char *))
TRM_W(020c91ac, (char *))
TRM_W(020c97bc, (char *))
TRM_W(020c9d7c, (char *))
TRM_W(020c9efc, (char *))
TRM_W(020ca070, (char *))
TRM_W(020ca310, (char *))
TRM_W(020ca39c, (int *))
TRM_W(020cb16c, (int *))
TRM_W(020cb2b4, )
TRM_W(020cb5c4, (char *))
TRM_W(020cb72c, (char *))
TRM_W(020cb838, (char *))
TRM_W(020cbd7c, (char *))
TRM_W(020cc2ac, (char *))
TRM_W(020cc408, (char *))
TRM_W(020cc63c, (int *))
TRM_W(020cc724, (char *))
TRM_W(020cc9b8, (int *))
TRM_W(020ccae0, (char *))
TRM_W(020ccd64, (char *))

extern "C" {
extern MgPmf pmf_Trmp3DMario_State6;
extern MgPmf pmf_Trmp2Mario_State3;
extern MgPmf pmf_Trmp2Mario_State7;
extern MgPmf pmf_Trmp2Mario_State7_2;
extern MgPmf pmf_Trmp2Mario_State0;
extern MgPmf pmf_Trmp2Mario_State7_3;
extern MgPmf pmf_Trmp2Mario_State6;
extern MgPmf pmf_Trmp3DMario_State10;
extern MgPmf pmf_Trmp2Mario_State5;
extern MgPmf pmf_Trmp2Mario_State4;
extern MgPmf pmf_Trmp2Mario_State7_4;
extern MgPmf pmf_Trmp2Mario_State2;
extern MgPmf pmf_Trmp2Mario_State4_2;
extern MgPmf pmf_Trmp3DMario_State1;
extern MgPmf pmf_Trmp3DMario_State12;
extern MgPmf pmf_Trmp2Mario_State3_2;
extern MgPmf pmf_Trmp3DMario_State12_2;
extern MgPmf pmf_Trmp3DMario_State12_3;
extern MgPmf pmf_Trmp3DMario_State11;
extern MgPmf pmf_Trmp3DMario_State11_2;
extern MgPmf pmf_Trmp2Mario_State1;
extern MgPmf pmf_Trmp3DMario_State12_4;
extern MgPmf pmf_Trmp3DMario_State4;
extern MgPmf pmf_Trmp2Mario_State2_2;
extern MgPmf pmf_Trmp3DMario_State5;
extern MgPmf pmf_Trmp3DMario_State3;
extern MgPmf pmf_Trmp3DMario_State6_2;
extern MgPmf pmf_Trmp2Mario_State6_2;
extern MgPmf pmf_Trmp3DMario_State5_2;
extern MgPmf pmf_Trmp3DMario_State8;
extern MgPmf pmf_Trmp2Mario_State1_2;
extern MgPmf pmf_Trmp3DMario_State7;
extern MgPmf pmf_Trmp3DMario_State1_2;
extern MgPmf pmf_Trmp3DMario_State3_2;
extern MgPmf pmf_Trmp3DMario_State0;
extern MgPmf pmf_Trmp2Mario_State7_5;
extern MgPmf pmf_Trmp3DMario_State9;
extern MgPmf pmf_Trmp3DMario_State2;
}

static const struct { MgPmf *p; unsigned rom; void *host; } g_trmpln_seats[] = {
    {&pmf_Trmp3DMario_State6, 0x020cc2acu, (void *)tw_020cc2ac},
    {&pmf_Trmp2Mario_State3, 0x020c9d7cu, (void *)tw_020c9d7c},
    {&pmf_Trmp2Mario_State7, 0x020ca39cu, (void *)tw_020ca39c},
    {&pmf_Trmp2Mario_State7_2, 0x020ca39cu, (void *)tw_020ca39c},
    {&pmf_Trmp2Mario_State0, 0x020c905cu, (void *)tw_020c905c},
    {&pmf_Trmp2Mario_State7_3, 0x020ca39cu, (void *)tw_020ca39c},
    {&pmf_Trmp2Mario_State6, 0x020ca310u, (void *)tw_020ca310},
    {&pmf_Trmp3DMario_State10, 0x020cc9b8u, (void *)tw_020cc9b8},
    {&pmf_Trmp2Mario_State5, 0x020ca070u, (void *)tw_020ca070},
    {&pmf_Trmp2Mario_State4, 0x020c9efcu, (void *)tw_020c9efc},
    {&pmf_Trmp2Mario_State7_4, 0x020ca39cu, (void *)tw_020ca39c},
    {&pmf_Trmp2Mario_State2, 0x020c97bcu, (void *)tw_020c97bc},
    {&pmf_Trmp2Mario_State4_2, 0x020c9efcu, (void *)tw_020c9efc},
    {&pmf_Trmp3DMario_State1, 0x020cb2b4u, (void *)tw_020cb2b4},
    {&pmf_Trmp3DMario_State12, 0x020ccd64u, (void *)tw_020ccd64},
    {&pmf_Trmp2Mario_State3_2, 0x020c9d7cu, (void *)tw_020c9d7c},
    {&pmf_Trmp3DMario_State12_2, 0x020ccd64u, (void *)tw_020ccd64},
    {&pmf_Trmp3DMario_State12_3, 0x020ccd64u, (void *)tw_020ccd64},
    {&pmf_Trmp3DMario_State11, 0x020ccae0u, (void *)tw_020ccae0},
    {&pmf_Trmp3DMario_State11_2, 0x020ccae0u, (void *)tw_020ccae0},
    {&pmf_Trmp2Mario_State1, 0x020c91acu, (void *)tw_020c91ac},
    {&pmf_Trmp3DMario_State12_4, 0x020ccd64u, (void *)tw_020ccd64},
    {&pmf_Trmp3DMario_State4, 0x020cb838u, (void *)tw_020cb838},
    {&pmf_Trmp2Mario_State2_2, 0x020c97bcu, (void *)tw_020c97bc},
    {&pmf_Trmp3DMario_State5, 0x020cbd7cu, (void *)tw_020cbd7c},
    {&pmf_Trmp3DMario_State3, 0x020cb72cu, (void *)tw_020cb72c},
    {&pmf_Trmp3DMario_State6_2, 0x020cc2acu, (void *)tw_020cc2ac},
    {&pmf_Trmp2Mario_State6_2, 0x020ca310u, (void *)tw_020ca310},
    {&pmf_Trmp3DMario_State5_2, 0x020cbd7cu, (void *)tw_020cbd7c},
    {&pmf_Trmp3DMario_State8, 0x020cc63cu, (void *)tw_020cc63c},
    {&pmf_Trmp2Mario_State1_2, 0x020c91acu, (void *)tw_020c91ac},
    {&pmf_Trmp3DMario_State7, 0x020cc408u, (void *)tw_020cc408},
    {&pmf_Trmp3DMario_State1_2, 0x020cb2b4u, (void *)tw_020cb2b4},
    {&pmf_Trmp3DMario_State3_2, 0x020cb72cu, (void *)tw_020cb72c},
    {&pmf_Trmp3DMario_State0, 0x020cb16cu, (void *)tw_020cb16c},
    {&pmf_Trmp2Mario_State7_5, 0x020ca39cu, (void *)tw_020ca39c},
    {&pmf_Trmp3DMario_State9, 0x020cc724u, (void *)tw_020cc724},
    {&pmf_Trmp3DMario_State2, 0x020cb5c4u, (void *)tw_020cb5c4},
};

/* Called from port_mg_framework_states_seat (unmatched/MgBase_StateDispatch
   .cpp), which hal/scene_mg.cpp runs once per process after the overlay
   constructors and before any trampoline element exists. */
extern "C" void port_mg_trmpln_field_seat(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    for (unsigned i = 0; i < sizeof g_trmpln_seats / sizeof g_trmpln_seats[0]; ++i) {
        if ((unsigned)(g_trmpln_seats[i].p->code) < 0x02000000u || (unsigned)(g_trmpln_seats[i].p->code) >= 0x02400000u) continue;  /* PORTSYNC3: a mount word that is not a DS code address: the class TU owns the live table */
        if (g_trmpln_seats[i].p->code != g_trmpln_seats[i].rom ||
            g_trmpln_seats[i].p->adj != 0) {
            std::fprintf(stderr, "FATAL: trampoline Mario field pair %u: the "
                         "mount holds %08x/%d, the ROM's own word says %08x/0 "
                         "-- WRONG BYTES\n", i, g_trmpln_seats[i].p->code,
                         g_trmpln_seats[i].p->adj, g_trmpln_seats[i].rom);
            std::abort();
        }
        g_trmpln_seats[i].p->code = (unsigned)(size_t)g_trmpln_seats[i].host;
    }
}

/* The shared count, for unmatched/MgTrampolineTerror_SubDispatch.cpp's census
   entry point: one process runs one of the two scenes, so on scene 385 this is
   Trampoline Terror's element dispatch count and on scene 384 Trampoline
   Time's. */
extern "C" unsigned port_mg_trmpln_field_hits(void) { return g_trmpln_field_hits; }
