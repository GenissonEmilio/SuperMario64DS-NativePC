// Lane DTOR-FACES-CPP (LINKSCOPE lane 10): the real-C++ destructor TUs of
// seated classes, linked from src/ and seated in slot 16.
//
// THE SHAPE. src/_ZN4CoinD1Ev.cpp and its 60-odd siblings are the shadow-
// class C++ form: the TU declares its own `struct Actor { char pad[..];
// virtual ~Actor(); }`, its own `struct CommonModel { char pad[..];
// ~CommonModel(); }` and so on, derives the class, and writes `Coin::~Coin()
// {}`. Under mwcc that byte-matches the ROM's D1 (the member chain and the
// Actor D2 call are what the compiler synthesises). Under MSVC the same TU
// compiles to ??1Coin@@UAE@XZ whose body calls
//
//     ??1WithMeshClsn@@QAE@XZ  ??1MovingCylinderClsn@@QAE@XZ
//     ??1ShadowModel@@QAE@XZ   ??1CommonModel@@QAE@XZ   ??1Actor@@UAE@XZ
//
// decorated __thiscall names that nothing in the port defined, because the
// port carries those bodies under the ROM's C names (_ZN10dBgCh_ActrD1Ev,
// _ZN8dActor_cD2Ev, ...). Every fill so far answered that with a transcription:
// a static thunk spelling the ROM D1's own chain by hand (coin_d1 in
// hal/actor_classes_bob_world.cpp, btail_d1 in hal/actor_classes_ov060.cpp,
// hal/StarMarker_D1.cpp), and the matched TU stayed out of the link.
//
// THE MECHANISM. Two halves, both precedented:
//
//   1. FACES for the shadow-class destructors, one definition per decorated
//      name, forwarding `this` to the C body. This is exactly
//      unmatched/Mg3DEsp_Faces.cpp section 2 (??1Model@@QAE@XZ and friends)
//      and unmatched/RecRoomCupboard_ActorDtorSeam.cpp (??1Actor@@QAE@XZ):
//      an /alternatename would be a RECEIVER-SHAPE mismatch (ecx vs stack),
//      so each is a real __thiscall definition. The shadow class is declared
//      here exactly as the src TUs declare it (same name, same virtual-ness,
//      no members) so the mangle is byte for byte the one the TU asks for.
//      Which body each face forwards to is read off the ROM relocations of
//      the 67 class-form D1 TUs (config/arm9/overlays/*/relocs.txt), not
//      assumed:
//
//        every base call    -> 0x020112c8 _ZN8dActor_cD2Ev            (67 of 67)
//        every member call  -> that member class's D1, a plain bl:
//          _ZN7dCcAc_cD1Ev        0x020149a4   (32 TUs)
//          _ZN11ShadowModelD1Ev               0x02015ff8   (28)
//          _ZN10dBgCh_ActrD1Ev              0x02014a60   (24)
//          _ZN10dCcAcPos_cD1Ev              (11)
//          _ZN11CommonModelD1Ev                            (4)
//          _ZN14dBgW_KcMbgSclYD1Ev                  (1, PoleLift)
//          _ZN5ModelD1Ev / _ZN9ModelAnimD1Ev / _ZN15TextureSequenceD1Ev /
//          _ZN18TextureTransformerD1Ev: faces already defined in
//          unmatched/Mg3DEsp_Faces.cpp, linked in the same four targets.
//
//      All the member calls are plain `bl`s to the D1 (no vptr load), which is
//      what makes the non-virtual Q mangle the right one to define; the base
//      call is to Actor's D2, and the src TUs declare ~Actor VIRTUAL (the U
//      mangle), so ??1Actor@@UAE@XZ is a second, distinct face beside the
//      seam's ??1Actor@@QAE@XZ.
//
//   2. ADAPTERS, one per class, in the vtable-law thunk shape, whose body is
//      the qualified call `((Coin *)s)->Coin::~Coin()`. Qualified, so it is a
//      direct `call ??1Coin@@UAE@XZ` and not a dispatch through slot 16 back
//      into itself. The class is declared here with only its virtual
//      destructor; the mangle depends on nothing else.
//
// WHAT CHANGES AT RUN TIME, honestly. The transcribed thunk stored the port's
// host table (_ZTV4Coin) into the vptr before the chain; MSVC's body stores
// its own one-slot ??_7Coin@@6B@ instead. Nothing dispatches through the
// object between that store and Actor's D2, which stores _ZTV5Actor itself
// (src/_ZN8dActor_cD2Ev.cpp, first statement), so the window is unobservable.
// Otherwise the chain is the same calls at the same offsets, because MSVC
// lays the shadow structs out the way mwcc does (vptr at 0, char pads, 4-byte
// members): the offsets in the src TUs' comments are the ones both
// compilers produce.
//
// WHAT IS NOT DONE. The D0 half of each pair is a flat .c TU carrying the
// ROM's C name; lane DTOR-PAIRS-C (status/DTORPAIRS.md) already seats those
// and they are untouched. The unseated classes with a class-form D1 (Cloud:
// ov039 not mounted; the config-named _ZN10daChRoom_cD1Ev: the daChRoom_c
// table's word, no seat) are skipped, see status/DTORFACES.md. InvisiblePole's
// D1 (0x020b0748) is seated below since lane SEAT-CAMERATAG: the table it
// stores, _ZTV10daCamTag_c (0x0210853c), IS the camera tag's (RTTI
// daCamTag_c) and is host storage now (hal/actor_classes_bob_world.cpp).

#include "dtor_faces_cpp.h"

extern "C" {
void *_ZN8dActor_cD2Ev(void *self);
void _ZN7dCcAc_cD1Ev(void *self);
void _ZN10dCcAcPos_cD1Ev(void *self);
void _ZN17dExtShadowModel_cD1Ev(void *self);
void _ZN10dBgCh_ActrD1Ev(void *self);
void _ZN17dExtCommonModel_cD1Ev(void *self);
void _ZN14dBgW_KcMbgSclYD1Ev(void *self);
}

/* ---- 1. the shadow-class faces ------------------------------------------ */

struct Actor { virtual ~Actor(); };                          /* ??1Actor@@UAE@XZ */
struct MovingCylinderClsn { ~MovingCylinderClsn(); };
struct MovingCylinderClsnWithPos { ~MovingCylinderClsnWithPos(); };
/* RETIRED at ALIAS2 (wave 8, the main -> port sync): ShadowModel and
   CommonModel, the two lines below and their two definitions further down.
   Since main's langmode migration every destructor TU that destroys a
   ShadowModel or a CommonModel member emits its own COMDAT copy of
   ??1ShadowModel@@QAE@XZ / ??1CommonModel@@QAE@XZ, and the out-of-line
   definitions here are not COMDAT, so they collided: 67 of walk_window's 196
   remaining LNK2005 rows were these two names, a third of the whole duplicate
   wall out of one file. The other five faces in this group are untouched;
   nothing else in the build defines those names twice.

   WHAT THIS COSTS, recorded rather than hidden (out/ALIAS2/bugs.md): the
   include/ShadowModel.h this tree compiles declares the pair as
   `virtual void Destructor1(); virtual void Destructor0();` under _MSC_VER, so
   MSVC's ??1ShadowModel@@QAE@XZ is the IMPLICIT destructor and does nothing.
   The face below routed it into the ROM body hosted in
   unmatched/ModelFamily_Dtors_HostCopy.c, which unlinks the shadow from its
   prev/next list. Retiring the face drops that unlink on the member-destroy
   path of the 67 classes. The ROM's own vtable slot 0 is not affected: that
   slot is filled from the table, not through this name.
struct ShadowModel { ~ShadowModel(); };
struct CommonModel { ~CommonModel(); };                                      */
struct WithMeshClsn { ~WithMeshClsn(); };
struct ExtendingMeshCollider { ~ExtendingMeshCollider(); };

Actor::~Actor()                                 { _ZN8dActor_cD2Ev(this); }
MovingCylinderClsn::~MovingCylinderClsn()       { _ZN7dCcAc_cD1Ev(this); }
MovingCylinderClsnWithPos::~MovingCylinderClsnWithPos()
                                                { _ZN10dCcAcPos_cD1Ev(this); }
/* RETIRED at ALIAS2, see the block above:
ShadowModel::~ShadowModel()                     { _ZN11ShadowModelD1Ev(this); }
CommonModel::~CommonModel()                     { _ZN11CommonModelD1Ev(this); } */
WithMeshClsn::~WithMeshClsn()                   { _ZN10dBgCh_ActrD1Ev(this); }
ExtendingMeshCollider::~ExtendingMeshCollider() { _ZN14dBgW_KcMbgSclYD1Ev(this); }

/* ---- 2. the per-class adapters ------------------------------------------ */

#define CPPD1(Class) \
    struct Class { virtual ~Class(); }; \
    int __fastcall hal_cppd1_##Class(void *s, void *) \
    { ((Class *)s)->Class::~Class(); return (int)(size_t)s; }

/* PORTSYNC2 (the 2026-10 main merge): the face keeps its port-side name
   (hal_cppd1_<Old>, which the seats call) but names the class main defines
   now, and spells the destructor the way main's header declares it on the
   host: CPPD1V where the header lost its `#ifdef _MSC_VER` Destructor1/
   Destructor0 pair and ~Cls() is virtual (??1Cls@@UAE@XZ), CPPD1Q where the
   pair is still there and ~Cls() holds no slot (??1Cls@@QAE@XZ). */
#define CPPD1V(Old, Cls) \
    struct Cls { virtual ~Cls(); }; \
    int __fastcall hal_cppd1_##Old(void *s, void *) \
    { ((Cls *)s)->Cls::~Cls(); return (int)(size_t)s; }
#define CPPD1Q(Old, Cls) \
    struct Cls { ~Cls(); }; \
    int __fastcall hal_cppd1_##Old(void *s, void *) \
    { ((Cls *)s)->Cls::~Cls(); return (int)(size_t)s; }

/* group 1 (the proof): Coin, hal/actor_classes_bob_world.cpp.
   ROM 0x020b0f54 (ov002): WithMeshClsn, MovingCylinderClsn, ShadowModel,
   CommonModel x2, Actor D2. */
CPPD1V(Coin, daCoin_c)

/* group 2: hal/actor_classes.cpp. StarMarker (ov002 0x020e6cf4; the host
   copy unmatched/StarMarker_D1.cpp is deleted), Bird (ov009), Butterfly,
   Fish, Door (ov100; the ROM word 16 of _ZTV4Door), PeachPainting (ov010),
   Flag (ov009), Cannon (ov098), and the three Actor-only bodies VirtualDoor,
   PoppingLavaBubbles, AmbientSoundEffects (ov002), whose slot was the shared
   ac_d1_actor_only thunk. Trap is NOT here: hal/actor_classes.cpp's tr_d1
   ruling (TRAP 0x24 and LIGHT_BEAM 0x25 share the table with different
   layouts, and the ROM chain fits only LIGHT_BEAM) stands. */
CPPD1V(StarMarker, daStarBase_c)
CPPD1V(Bird, daSBird_c)
CPPD1V(Butterfly, daBtfly_c)
CPPD1V(Fish, daFish_c)
CPPD1V(PeachPainting, daObjC1Peach_c)
CPPD1Q(daMcFlag_c, daMcFlag_c)
CPPD1V(Cannon, daCnn_c)
CPPD1Q(daChScene_c, daChScene_c)
CPPD1V(daObjWaterfall_c, daObjWaterfall_c)
CPPD1V(daSetSE_c, daSetSE_c)
CPPD1V(Door, daDoor_c)
/* daStarGate_c: ov100 0x021458d4, the ROM word 16 of _ZTV12daStarGate_c
   (0x021483cc). Added by lane CRASH12 once the two door tables were separated.
   Gate 40's slot 16 used to hold hal_cppd1_Door, which destroys a ModelAnim at
   +0xd4; the star door's member there is a CommonModel, and the ROM gives that
   table its own D1. */
CPPD1Q(daStarGate_c, daStarGate_c)

/* group 3: ov060 / ov070 / ov071 / ov072, the enemy overlays */
CPPD1(BowserTail)
CPPD1(Bowser)
CPPD1V(BowserSkyPlatform, daKpa3Bg_c)
CPPD1V(daBrq_c, daBrq_c)
CPPD1V(daKrpa_c, daKrpa_c)
CPPD1V(daKpFr_c, daKpFr_c)
CPPD1V(MrI, daEykn_c)
CPPD1Q(daEyBm_c, daEyBm_c)
CPPD1V(Scuttlebug, Scuttlebug)
CPPD1V(BabyPenguin, daPgBby_c)
CPPD1Q(daBgSnmBdy_c, daBgSnmBdy_c)
CPPD1V(SnowmanHead, daBgSnmHed_c)

/* group 4: the small-cast level overlays and ov002 tags */
CPPD1V(PyramidTag, daObjDlPyramidDummy_c)
CPPD1V(CrazedCrate, daBttBk_c)
CPPD1V(BookShotSpawner, daBookGen_c)
CPPD1V(WingFeather, daFeather_c)
CPPD1V(Number, daObjNumber_c)
CPPD1V(EnemySpawner, daECreate_c)
CPPD1V(EnemySwitchTag, daESwitch_c)
CPPD1V(HauntedChair, daChair_c)
CPPD1V(PrincessPeach, daPeach_c)
CPPD1V(Toad, daKinopio_c)
CPPD1V(RacingPenguin, daPgRcer_c)
CPPD1Q(daSldMng_c, daSldMng_c)
CPPD1V(RollingLogLll, daObjFlMaruta_c)
CPPD1V(RollingLogTtm, daObjHmMaruta_c)
CPPD1V(TreasureChest, daObjTbox_c)
CPPD1V(Clam, daObjShell_c)
CPPD1V(CutsceneObject, daDemo_c)

/* group 5: the rest of the seated cast */
CPPD1Q(daObjKinokoTag_c, daObjKinokoTag_c)
CPPD1Q(daObjClock_c, daObjClock_c)
CPPD1Q(daObjHsBillboard_c, daObjHsBillboard_c)
CPPD1V(daObjRc_Hane_c, daObjRc_Hane_c)
CPPD1Q(daObjKb1Billboard_c, daObjKb1Billboard_c)
CPPD1Q(daSanbo_c, daSanbo_c)
CPPD1V(Tornado, daTor_c)
/* TORNADO IS THE ONE CLASS IN THIS FILE THAT ALSO NEEDS THE BOTTOM HALF.
   Run link100 wave 9c, lane LINK21, and it is the shape section 1 above uses
   for "a face the ledger cannot spell".

   Every other CPPD1 row only DECLARES ??1<Cls>@@UAE@XZ and calls it, because
   something else in the link defines it: the class's own seated structor TU, or
   a COMDAT copy MSVC emits from a header-inline destructor. Tornado has
   neither. src/_ZN7TornadoD1Ev.cpp is on port/slice_dtorfaces.txt and compiles,
   and dumpbin over its object shows exactly one external,
   ?Tornado_EmitDestructor@@YAXPAUTornado@@@Z: MSVC inlined the whole class-body
   destructor and emitted no out-of-line copy to name.

   THE LEDGER CANNOT ANSWER IT EITHER, and that is measured rather than assumed.
   port/faces_sync.txt:2586 carries the reverse F row
   `__ZN7TornadoD1Ev -> ??1Tornado@@UAE@XZ` COMMENTED OUT, and it has to stay
   that way: a reverse face defines the FLAT name, and both of Tornado's flat
   destructor names are already defined in this link (D0 by a live ledger D row
   at faces_sync.txt:2416, D1 by port/unmatched/Tornado_HostSites.cpp:72), so
   either forwarder is an LNK2005. out/DTORS2/build_dtors2_2.log lines 2207 and
   2208 are that measurement.

   So the definition is written by hand here, in the one direction that is
   still free: the DECORATED name calling the flat body, which is exactly what
   Actor, MovingCylinderClsn and WithMeshClsn do in section 1. The flat body it
   calls is the PORT_HOST_ABI host copy in Tornado_HostSites.cpp, which stores
   _ZTV7Tornado into word 0 and then destroys the four member subobjects at
   +0x328, +0x2c4, +0x108 and +0xd4 before chaining to _ZN8dActor_cD2Ev, in the
   cartridge's own order. Nothing about dispatch changes: hal_cppd1_Tornado's
   call is QUALIFIED, so it was always a direct call to this name, and until now
   that name simply had no body. */
/* PORTSYNC2: RETIRED. Main's src/actors/daTor_c.cpp now defines
   daTor_c::~daTor_c() itself (??1daTor_c@@UAE@XZ), so the name has a body and
   this hand definition would be a second one; hal_cppd1_Tornado above calls it.
extern "C" int *_ZN7daTor_cD1Ev(int *self);
Tornado::~Tornado()                             { _ZN7daTor_cD1Ev((int *)this); }
*/
CPPD1V(Spiny, daTgz_c)
CPPD1V(Lakitu, daJgm_c)
CPPD1(daGmch_c)
CPPD1V(MugenBgm, daMugenBGM_c)
CPPD1V(PowerFlower, daObjPowerUpItem_c)
CPPD1V(PoleLift, daObjKm2_Ami_Bou_c)

/* ExtendingPlatform (ov045 0x02112e80 word 16 -> 0x02111840, its own D1).
   This slot used to be filled with hal_cppd1_PoleLift, one class off. */
CPPD1V(ExtendingPlatform, daObjKm2_Nobiru_c)
CPPD1Q(daObjFire_c, daObjFire_c)
CPPD1V(KoopaFlag, daRFlag_c)
CPPD1V(BobOmbBuddy, daRedBombhei_c)
CPPD1V(MontyMole, daChoropu_c)
CPPD1V(OneUpLogo, daObj1UpLogo_c)
CPPD1V(SkiLift, daObjSm_Lift_c)
CPPD1V(HealingHeart, daObjHeart_c)
CPPD1V(BrickBlock, daObjBlockItemTag_c)
/* ArrowLift: the ROM word 16 of _ZTV9ArrowLift (ov029 0x02113db4), which
   hal/actor_classes_ov029.cpp fills as WaterDiamond's live table; the slot
   was a trap. SkiLift above is the ROM word 16 of _ZTV7SkiLift, which the
   port fills as MotherPenguin's table (the ccm name shift), and Seaweed is
   the word of _ZTV7Seaweed, HealingHeart's table. */
CPPD1Q(daObjWc_Obj03_c, daObjWc_Obj03_c)
/* lane SEAT-CAMERATAG: the ROM word 16 of the table at ov002 0x0210853c, the
   config's _ZTV10daCamTag_c, which daCamTag_c_classInit installs (RTTI
   daCamTag_c). Actor-only chain: Actor::D2 and nothing else. */
CPPD1V(daCamTag_c, daCamTag_c)
