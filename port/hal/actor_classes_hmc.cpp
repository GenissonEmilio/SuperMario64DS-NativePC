// GATE 172: level 13's (Hazy Maze Cave) hosting wave, first class: CRATE
// (194, ov098), the breakable wooden box (RTTI daObjBlockS_c).
//
// Same law as hal/actor_classes_bob_world.cpp and hal/actor_classes_bbh.cpp --
// ROM slot order, __fastcall thunks that call QUALIFIED, unhosted slots trap by
// name -- and THIRTY-TWO SLOTS, the Platform shape SeesawBob has: an 18-slot
// Actor core plus the Platform tail, of which Crate owns 18 (OnYoshiTryEat), 19
// (OnTurnIntoEgg), 21 (OnGroundPounded) and 31 (Kill). Every slot below was
// read out of _ZTV5Crate (ov098 0x0213c534) with its relocations applied.
//
// ov098 is already mounted per symbol (ov098_syms.txt, the WaterBomb /
// FallBlockWf / ExclamationSwitch gates); g_profile_BLOCK_S (0x0213c510, +4 reads
// 194) and the fourteen state-table source statics were mounted with it. The
// class's seven-state machine dispatches through data_ov098_0213c878, seated by
// port_crate_states_seat (port/unmatched/Crate_StateDispatch.cpp) before
// __sinit_ov098_0213c058 copies it -- the two tiny dispatcher TUs are host
// copies there (the OneUpMushroom PMF case); everything else in the closure is
// matched src, slice_gate172.txt.
//
// Slot 21 (OnGroundPounded, _ZN5Crate15OnGroundPoundedER8dActor_c) takes only `this` and
// dispatches slot 31 (Kill) virtually, which is why 31 is bound rather than
// trapped: the first ground-pound of a crate reaches it.
#include "port_d16.h"

#include <cstdio>

/* hal/actor_slot30_seat.cpp -- the shared seat for vtable slot 30,
   Actor::OnAimedAtWithEggReturnVec. The ROM word in slot 30 of every vtable
   this file fills IS the arm9 base body 0x020100dc (checked against
   config/<module>/relocs.txt at vtable+30*4), and that body is now in the
   link from src/_ZN8dActor_c25OnAimedAtWithEggReturnVecEv.cpp on slice_gate50.
   The three-parameter __fastcall is the sret contract MSVC uses for a
   thiscall member returning a 12-byte struct: this in ecx, the hidden result
   pointer the one (callee-popped) stack argument. Same shape as whomp_s30. */
extern "C" void *__fastcall port_actor_s30_base(void *self, void *, void *out);
#include <cstdlib>

#include "dActor_c.h"
#include "fBase_c.h"
#include "daObjBlockS_c.h"

extern "C" {
/* the shared lifecycle halves, the same functions every fill writes */
int _ZN8dActor_c19BeforeInitResourcesEv(void *self);            /* slot 1  */
void _ZN8dActor_c18AfterInitResourcesEj(void *self, unsigned a); /* slot 2  */
int _ZN8dActor_c14BeforeBehaviorEv(void *self);                 /* slot 7  */
int _ZN8dActor_c12BeforeRenderEv(void *self);                   /* slot 10 */

extern int data_02099f24[];          /* the frame phase the lists are in */
extern unsigned char data_020a4b4c;  /* the spawn spine's own step */
const char *port_actor_class_name(unsigned id);   /* hal/actor_registry */
  void port_actor_slot_decline(const char *what);  /* func_02043fdc_hostcopy.cpp */
void port_actor_render_probe(const char *cls, void *model); /* actor_classes */

/* Crate's own C-linkage bodies (matched src, slice_gate172.txt) */
void *_ZN13daObjBlockS_cD1Ev(void *self);              /* slot 16, .cpp extern "C" */
void *_ZN13daObjBlockS_cD0Ev(void *self);              /* slot 17, .c */
int _ZN13daObjBlockS_c13OnYoshiTryEatEv(unsigned char *c);    /* slot 18, OnYoshiTryEat */
void _ZN13daObjBlockS_c13OnTurnIntoEggER6Player(char *self, char *other); /* slot 19, OnTurnIntoEgg */
void _ZN13daObjBlockS_c15OnGroundPoundedER8dActor_c(void *self);         /* slot 21, OnGroundPounded */
void _ZN13daObjBlockS_c4KillEv(char *self);         /* slot 31, Kill */
void *daObjBlockS_c_classInit(void);

/* The one array the ROM factory installs (daObjBlockS_c_classInit does
   `*(void **)p = &_ZTV5Crate`); thirty-two slots. Defined here, not just
   declared: the `int` type and C linkage match the `extern int _ZTV5Crate[]`
   in the destructor TUs. */
int _ZTV13daObjBlockS_c[32];
}

/* daObjBlockS_c_classInit.cpp declares the table `extern void *_ZTV5Crate;` OUTSIDE any
   extern "C" block, so MSVC mangles that one reference as a C++ data name.
   Point the mangled spelling at the C symbol -- the IDENTITY_MATRIX4X3 precedent. */
#pragma comment(linker, "/alternatename:?_ZTV13daObjBlockS_c@@3PAXA=__ZTV13daObjBlockS_c")

/* The C++/C linkage bridge, the gate-42 pragma block's reading. Crate's
   InitResources and CleanupResources declare data_ov098_0213c4c8 (the model
   file-pointer table, mounted in ov098_syms.txt) outside their extern "C"
   blocks and with two different element types, so MSVC mangles two C++ data
   spellings of the one C symbol. func_ov098_021390ec spells its call to
   func_ov002_020ef228 the same C++ way, and names WithMeshClsn::GetWallResult
   with the pointer return the real record has, while the compiled matched
   body (slice_gate10.txt) spells the int-return form -- same __thiscall body,
   the return rides eax either way. */
#pragma comment(linker, "/alternatename:?data_ov098_0213c4c8@@3PADA=_data_ov098_0213c4c8")
#pragma comment(linker, "/alternatename:?data_ov098_0213c4c8@@3PAHA=_data_ov098_0213c4c8")
#pragma comment(linker, "/alternatename:?func_ov002_020ef228@@YAHPAXH@Z=_func_ov002_020ef228")
#pragma comment(linker, "/alternatename:?GetWallResult@dBgCh_Actr@@QBEPAUClsnResult@@XZ=?GetWallResult@dBgCh_Actr@@QBEHXZ")

// ---- the trap --------------------------------------------------------------
static void hmc_trap_report(void *self, int slot)
{
    unsigned id = self ? *(unsigned short *)((char *)self + 0xc) : 0u;
    std::fprintf(stderr,
                 "UNHOSTED: vtable slot %d is not hosted (actor id %u %s, "
                 "phase %d, spawn step %d)\n",
                 slot, id, port_actor_class_name(id), data_02099f24[0],
                 (int)data_020a4b4c);
    { static char _m[128];
      std::snprintf(_m, sizeof _m, "unhosted vtable slot %d on id %u %s",
                    slot, id, port_actor_class_name(id));
      port_actor_slot_decline(_m); }
}
#define HMC_TRAP(n) \
    static int __fastcall hmc_trap##n(void *s, void *) \
    { hmc_trap_report(s, n); return 0; }
HMC_TRAP(13) HMC_TRAP(14) HMC_TRAP(29)
#undef HMC_TRAP

// ---- the shared half -------------------------------------------------------
static int __fastcall hmc_binit(void *s, void *)
{ return _ZN8dActor_c19BeforeInitResourcesEv(s); }
static void __fastcall hmc_ainit(void *s, void *, unsigned a)
{ _ZN8dActor_c18AfterInitResourcesEj(s, a); }
static int __fastcall hmc_bclean(void *s, void *)
{ return ((dActor_c *)s)->dActor_c::BeforeCleanupResources(); }
static void __fastcall hmc_aclean(void *s, void *, unsigned a)
{ ((fBase_c *)s)->fBase_c::AfterCleanupResources(a); }
static int __fastcall hmc_bbeh(void *s, void *)
{ return _ZN8dActor_c14BeforeBehaviorEv(s); }
static void __fastcall hmc_abeh(void *s, void *, unsigned a)
{ ((fBase_c *)s)->fBase_c::AfterBehavior(a); }
static int __fastcall hmc_bren(void *s, void *)
{ return _ZN8dActor_c12BeforeRenderEv(s); }
static void __fastcall hmc_aren(void *s, void *, unsigned a)
{ ((fBase_c *)s)->fBase_c::AfterRender(a); }
static int __fastcall hmc_pdes(void *s, void *)
{ ((fBase_c *)s)->fBase_c::OnPendingDestroy(); return 0; }
static int __fastcall hmc_heap(void *s, void *)
{ return ((fBase_c *)s)->fBase_c::OnHeapCreated(); }

// ---- CRATE's own slots -----------------------------------------------------
static int __fastcall crate_init(void *s, void *)
{ return ((daObjBlockS_c *)s)->daObjBlockS_c::InitResources(); }
static int __fastcall crate_clean(void *s, void *)
{ return ((daObjBlockS_c *)s)->daObjBlockS_c::CleanupResources(); }
static int __fastcall crate_behavior(void *s, void *)
{ return ((daObjBlockS_c *)s)->daObjBlockS_c::Behavior(); }
static int __fastcall crate_render(void *s, void *)
{ port_actor_render_probe("CRATE", (char *)s + 0xd4);
  return ((daObjBlockS_c *)s)->daObjBlockS_c::Render(); }
static int __fastcall crate_d1(void *s, void *)
{ return (int)(size_t)_ZN13daObjBlockS_cD1Ev(s); }
static int __fastcall crate_d0(void *s, void *)
{ return (int)(size_t)_ZN13daObjBlockS_cD0Ev(s); }
static int __fastcall crate_yoshi(void *s, void *)
{ return _ZN13daObjBlockS_c13OnYoshiTryEatEv((unsigned char *)s); }
/* slot 19's second argument arrives on the stack, the cap_egg reading */
static int __fastcall crate_egg(void *s, void *, void *o)
{ _ZN13daObjBlockS_c13OnTurnIntoEggER6Player((char *)s, (char *)o); return 0; }
/* Slot 29, OnAimedAtWithEgg: the Crate does NOT override it. _ZTV5Crate
   (ov098 0x0213c534) holds the arm9 base 0x02010124 there, so this is the
   ROM's word. It was trapped, and it has to answer now because slot 30 below
   is the Actor base body, whose one job is to add slot 29's result to y. */
extern "C" int _ZN8dActor_c16OnAimedAtWithEggEv(void *self);
static int __fastcall crate_aimed(void *s, void *)
{ return _ZN8dActor_c16OnAimedAtWithEggEv(s); }
/* Slots 20 and 22..28: _ZTV5Crate holds the arm9 Actor bodies at every one of
   them (ov098 0x0213c534 + 20*4 .. 28*4 = 0x0201014c, 0x02010144, 0x02010140,
   0x0201013c, 0x02010138, 0x02010134, 0x02010130, 0x0201012c; slot 21 is the
   Crate's own). They were trapped, so the first thing that reached one froze
   the crate. Mega Mario (giant Mario) reaches slot 27, OnHitByMegaChar, on
   everything he touches, and a crate on the castle grounds took the port down
   to a quarantine of the PLAYER: the trap is dispatched from inside the
   player's phase callback, so the freeze landed on Mario (crash C5, 4 reports,
   exe +00513c9d). The bodies are the Actor ones Star Camera's table already
   uses, and the shapes follow the dispatch sites: slot 20 takes only `this`,
   22..28 take one argument the callee pops, hence the third parameter. */
extern "C" {
int  _ZN8dActor_c9Virtual50Ev(void *self);
void _ZN8dActor_c11OnAttacked1ERS_(void *self, void *o);
void _ZN8dActor_c11OnAttacked2ERS_(void *self, void *o);
void _ZN8dActor_c8OnKickedERS_(void *self, void *o);
void _ZN8dActor_c8OnPushedERS_(void *self, void *o);
void _ZN8dActor_c24OnHitByCannonBlastedCharERS_(void *self, void *o);
void _ZN8dActor_c15OnHitByMegaCharER6Player(void *self, void *p);
void _ZN8dActor_c19OnHitFromUnderneathERS_(void *self, void *o);
}
static int __fastcall crate_v50(void *s, void *)
{ return _ZN8dActor_c9Virtual50Ev(s); }
static int __fastcall crate_atk1(void *s, void *, void *o)
{ _ZN8dActor_c11OnAttacked1ERS_(s, o); return 0; }
static int __fastcall crate_atk2(void *s, void *, void *o)
{ _ZN8dActor_c11OnAttacked2ERS_(s, o); return 0; }
static int __fastcall crate_kicked(void *s, void *, void *o)
{ _ZN8dActor_c8OnKickedERS_(s, o); return 0; }
static int __fastcall crate_pushed(void *s, void *, void *o)
{ _ZN8dActor_c8OnPushedERS_(s, o); return 0; }
static int __fastcall crate_cannon(void *s, void *, void *o)
{ _ZN8dActor_c24OnHitByCannonBlastedCharERS_(s, o); return 0; }
static int __fastcall crate_mega(void *s, void *, void *p)
{ _ZN8dActor_c15OnHitByMegaCharER6Player(s, p); return 0; }
static int __fastcall crate_under(void *s, void *, void *o)
{ _ZN8dActor_c19OnHitFromUnderneathERS_(s, o); return 0; }
/* Slot 21 is OnGroundPounded(Actor &other), and its argument arrives the way
   slot 19's does: on the stack. The ov098 body reads only r0 and ignores the
   pounder, but the HOST caller still pushes it, because the slot's
   declaration in include/Actor.h has the parameter. A __fastcall thunk
   carries its first two parameters in ecx/edx and pops nothing; only a
   declared THIRD parameter lands on the stack and makes the thunk `ret 4`.
   Written with two, the pushed word outlives the call, the caller's epilogue
   pops one slot short, and its `ret` takes the saved EBP -- a stack address
   -- as the return address. Declare the argument, ignore it like the ROM. */
static int __fastcall crate_pounded(void *s, void *, void *)
{ _ZN13daObjBlockS_c15OnGroundPoundedER8dActor_c(s); return 0; }
static int __fastcall crate_kill(void *s, void *)
{ _ZN13daObjBlockS_c4KillEv((char *)s); return 0; }

extern "C" void hal_fill_crate_vtable(void)
{
    void **vt = (void **)_ZTV13daObjBlockS_c;
    vt[0]  = (void *)crate_init;
    vt[1]  = (void *)hmc_binit;
    vt[2]  = (void *)hmc_ainit;
    vt[3]  = (void *)crate_clean;
    vt[4]  = (void *)hmc_bclean;
    vt[5]  = (void *)hmc_aclean;
    vt[6]  = (void *)crate_behavior;
    vt[7]  = (void *)hmc_bbeh;
    vt[8]  = (void *)hmc_abeh;
    vt[9]  = (void *)crate_render;
    vt[10] = (void *)hmc_bren;
    vt[11] = (void *)hmc_aren;
    vt[12] = (void *)hmc_pdes;
    vt[13] = (void *)hmc_trap13;
    vt[14] = (void *)hmc_trap14;
    vt[15] = (void *)hmc_heap;
    vt[16] = (void *)PORT_D16(crate_d1);
    vt[17] = (void *)crate_d0;
    vt[18] = (void *)crate_yoshi;
    vt[19] = (void *)crate_egg;
    /* the Platform tail (20..31): Actor's bodies, except the two Crate owns */
    vt[20] = (void *)crate_v50;
    vt[21] = (void *)crate_pounded;
    vt[22] = (void *)crate_atk1;
    vt[23] = (void *)crate_atk2;
    vt[24] = (void *)crate_kicked;
    vt[25] = (void *)crate_pushed;
    vt[26] = (void *)crate_cannon;
    vt[27] = (void *)crate_mega;
    vt[28] = (void *)crate_under;
    vt[29] = (void *)crate_aimed;   /* Actor::OnAimedAtWithEgg, the ROM's word */
    vt[30] = (void *)port_actor_s30_base;
    vt[31] = (void *)crate_kill;
}
