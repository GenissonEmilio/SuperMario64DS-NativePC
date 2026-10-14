//cpp
/* dMgState_c -- the minigame scene's little state machine, and the rest of its
 * translation unit: 52 functions, ov004 .text 0x020b67e8..0x020b8c18.
 *
 * dScMgBase_c keeps one of these by value and drives it once a frame: Behavior
 * counts the state timer down and calls the current state's per-frame handler,
 * Render calls the current state's draw handler, and SetState swaps both out.
 * Every handler is a pointer to a dMgState_c member, so the table below is a
 * table of member pointers rather than of plain function addresses.
 *
 * The four methods sit in the middle of the unit. Below them, 0x020b67e8 to
 * 0x020b8714, are the state handlers and the helpers they share. The .data
 * records at 0x020bc8bc..0x020bca3c hold 46 pointer-to-member constants over
 * 43 of them (the twenty-entry enter table at 0x020bc92c..0x020bca44 among
 * them), so all 43 are written as dMgState_c members under their address
 * names. The remaining five are genuine free helpers that write the shared
 * .data window (func_ov004_020b67e8, 020b67f8, 020b6808, 020b682c and
 * func_ov004_020b8a8c).
 *
 * Edges. 0x020b67e8 is the first function after func_ov004_020b67e4, the last
 * destructor helper __sinit_ov004_020b9ad0 registers for the preceding unit
 * (the 0x14-entry array at data_ov004_020bfa34). 0x020b8c18 is the start of
 * dMgPsOpt_c. The unit's .data window, 0x020bc850..0x020bca44, is read only
 * by this unit's functions and ends before dMgPsOpt_c's at 0x020bca58; the
 * words at data_ov004_020bc7d4 and data_ov004_020bfa24 belong to the preceding
 * unit, which this unit only writes. The unit emits no static initializer of
 * its own, so none of the four __sinit_ov004_* records belongs to it.
 *
 * mwccarm emits one .text section per function and lays them out in the reverse
 * of source order, so the functions are written here highest-ROM-address first.
 *
 * The cartridge proves the 0x28-byte layout, the twenty-entry enter table, the
 * construction order and the three callback roles. The class and member names
 * are inferred from the dMg* minigame family; see include/dMgState_c.h.
 *
 * Many states share one shape. The enter handler sets mTimer, shows a
 * graphic through func_ov004_020b0cac (file index, x, y; the x/y words at
 * data_ov004_020bc850..0x020bc8b8, except 0x020bc8a0, the file-index
 * override, are set by func_ov004_020b8a8c and patched by
 * func_ov004_020b6808 and func_ov004_020b682c) and installs a
 * behavior. data_ov004_020bc8a0 replaces the next such file index once;
 * 0x1d there means no override. The behavior waits for mTimer to run out
 * and for the scene's OnTurnIntoEgg(mState) to return nonzero, in several
 * states around a screen fade (SetForwardTime out, then SetBackwardTime
 * once the fader IsAtEnd). It then frees every graphics slot
 * (FreeGfxSlotsById(0x1d)), passes mState to func_ov004_020b29a0 and
 * returns the machine to idle (mState -1).
 *
 * Notes:
 *   - The .data member-pointer records keep their data_ov004_* names and
 *     are declared as dMgState_c::Callback; the comment on each extern names
 *     the member it points at.
 *   - Base (the scene) and Obj (the fader) are TU-local views; each says
 *     why it is not the shared header's class. Their names, and the W2 type
 *     of data_02086b58, are the spellings config/decl-agreement-baseline.json
 *     records for those externs.
 *   - mState doubles as the id passed to the scene's OnTurnIntoEgg and,
 *     through func_ov004_020b29a0, to its OnYoshiTryEat. The `(void *)mState`
 *     spelling follows that helper's declaration.
 *
 * Retired one-function sources (ROM address order):
 *   0x020b67e8  func_ov004_020b67e8
 *   0x020b67f8  func_ov004_020b67f8
 *   0x020b6808  func_ov004_020b6808
 *   0x020b682c  func_ov004_020b682c
 *   0x020b68e8  func_ov004_020b68e8
 *   0x020b6948  func_ov004_020b6948
 *   0x020b6ad8  func_ov004_020b6ad8
 *   0x020b6b40  func_ov004_020b6b40
 *   0x020b6c10  func_ov004_020b6c10
 *   0x020b6c9c  func_ov004_020b6c9c
 *   0x020b6d6c  func_ov004_020b6d6c
 *   0x020b6ddc  func_ov004_020b6ddc
 *   0x020b6f14  func_ov004_020b6f14
 *   0x020b6f88  func_ov004_020b6f88
 *   0x020b7020  func_ov004_020b7020
 *   0x020b70b4  func_ov004_020b70b4
 *   0x020b7124  func_ov004_020b7124
 *   0x020b724c  func_ov004_020b724c
 *   0x020b72d4  func_ov004_020b72d4
 *   0x020b743c  func_ov004_020b743c
 *   0x020b7460  func_ov004_020b7460
 *   0x020b746c  func_ov004_020b746c
 *   0x020b7594  func_ov004_020b7594
 *   0x020b75e4  func_ov004_020b75e4
 *   0x020b7744  func_ov004_020b7744
 *   0x020b77b4  func_ov004_020b77b4
 *   0x020b7854  func_ov004_020b7854
 *   0x020b78f4  func_ov004_020b78f4
 *   0x020b798c  func_ov004_020b798c
 *   0x020b79b0  func_ov004_020b79b0
 *   0x020b7a18  func_ov004_020b7a18
 *   0x020b7b20  func_ov004_020b7b20
 *   0x020b7b90  func_ov004_020b7b90
 *   0x020b7c04  func_ov004_020b7c04
 *   0x020b7cd0  func_ov004_020b7cd0
 *   0x020b7e38  func_ov004_020b7e38
 *   0x020b7eac  func_ov004_020b7eac
 *   0x020b7f5c  func_ov004_020b7f5c
 *   0x020b7fec  func_ov004_020b7fec
 *   0x020b8098  func_ov004_020b8098
 *   0x020b81f8  func_ov004_020b81f8
 *   0x020b8284  func_ov004_020b8284
 *   0x020b83ac  func_ov004_020b83ac
 *   0x020b841c  func_ov004_020b841c
 *   0x020b853c  func_ov004_020b853c
 *   0x020b8560  func_ov004_020b8560
 *   0x020b8688  func_ov004_020b8688
 *   0x020b8a8c  func_ov004_020b8a8c
 */

#include "dMgState_c.h"
#include "dScene_c.h"
#include "dWipe_c.h"
#include "PlayerInput.h"

extern int ApproachLinear(s32 &value, s32 target, s32 step);
/* local extern: including Sound.h renumbers SetState's local static. */
namespace Sound { u32 PlayBank2_2D(u32); }

/* The minigame scene at data_ov004_020beb68, seen through the two virtuals
   this unit calls and the one field it reads, under dScMgBase_c.h's names.
   Including dScMgBase_c.h instead renumbers SetState's local static
   (sEnterTable$184 becomes $208), and the manifest pins $184. Named
   parameters declared above SetState move that number too (measured: one
   name fewer gives $183), so these two views keep five between them. */
struct Base {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3();
    virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7();
    virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11();
    virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15();
    virtual void v16(); virtual void v17();
    virtual void OnYoshiTryEat(int);      /* slot 18, reached via func_ov004_020b29a0 */
    virtual int OnTurnIntoEgg(int mode);  /* slot 19 */
    virtual void Virtual50();             /* slot 20 */
    char pad[0xa4];
    s32 unk_0a8;                          /* 0x0a8 */
};
/* The installed screen fader at data_0209f5bc (a dFader_c), under
   dFader_c.h's slot names. The ROM sets r2 = 0 at every SetBackwardTime and
   SetForwardTime call here, which the header's one-argument spelling does
   not do, so this view declares the second argument. */
struct Obj {
    virtual void v0(); virtual void v1();
    virtual void AdvanceFade();                        /* slot 2 */
    virtual int SetBackwardTime(u32 frames, int arg);  /* slot 3 */
    virtual int SetForwardTime(u32 frames, int arg);   /* slot 4 */
    virtual int IsAtStart();                           /* slot 5 */
    virtual int IsAtEnd();                             /* slot 6 */
    virtual int IsBetweenStartAndEnd();                /* slot 7 */
};

extern "C" {
extern void FreeGfxSlotsById(int arg);
extern int GetGameLanguage(void);
extern unsigned int func_02012790(unsigned int x);
extern void func_02012dd0(void* c);
extern void func_ov004_020ad90c(void);
extern int func_ov004_020adbc0(void);
extern int func_ov004_020adbe0(void);
extern void func_ov004_020ae20c(void);
extern void func_ov004_020ae274(void* c);
extern void func_ov004_020ae2c8(void);
extern void func_ov004_020af948(void *a, int b, int c, void *d);
extern int func_ov004_020b04c0(void);
extern int func_ov004_020b0b1c(int arg);
extern void func_ov004_020b0cac(int c, int a1, int a2, int a3, int arg5, short arg6);
extern void func_ov004_020b0d8c(void *, int, int);
extern void func_ov004_020b14f0(void *);
extern void func_ov004_020b2444(int a, int b, int c, int d, int e, int f, int g);
extern void func_ov004_020b29a0(void* c, void* arg);
extern void func_ov004_020b67e8(int);
}

struct W2 { int w[2]; };
extern W2 data_02086b58;  /* __ptmf_null's address: the null member pointer */
extern Obj* data_0209f5bc;
extern dWipe_c data_0209f61c;
extern Base* data_ov004_020beb68;
extern int data_ov004_020beb6c;
extern int data_ov004_020bc150;
extern int data_ov004_020bc7d4;
extern int data_ov004_020bfa24;

extern int data_ov004_020bc850;
extern int data_ov004_020bc854;
extern int data_ov004_020bc858;
extern int data_ov004_020bc85c;
extern int data_ov004_020bc860;
extern int data_ov004_020bc864;
extern int data_ov004_020bc868;
extern int data_ov004_020bc86c;
extern int data_ov004_020bc870;
extern int data_ov004_020bc874;
extern int data_ov004_020bc878;
extern int data_ov004_020bc87c;
extern int data_ov004_020bc880;
extern int data_ov004_020bc884;
extern int data_ov004_020bc888;
extern int data_ov004_020bc88c;
extern int data_ov004_020bc890;
extern int data_ov004_020bc894;
extern int data_ov004_020bc898;
extern int data_ov004_020bc89c;
extern int data_ov004_020bc8a0;
extern int data_ov004_020bc8a4;
extern int data_ov004_020bc8a8;
extern int data_ov004_020bc8ac;
extern int data_ov004_020bc8b0;
extern int data_ov004_020bc8b4;
extern int data_ov004_020bc8b8;
extern unsigned char *data_ov004_020bca44[];

/* Behavior and render records. Each comment names the member the record
   points at (ov004 relocs.txt). */
extern dMgState_c::Callback data_ov004_020bc8bc;  /* func_ov004_020b841c */
extern dMgState_c::Callback data_ov004_020bc8c4;  /* func_ov004_020b8284 */
extern dMgState_c::Callback data_ov004_020bc8cc;  /* func_ov004_020b6ad8 */
extern dMgState_c::Callback data_ov004_020bc8d4;  /* func_ov004_020b8098 */
extern dMgState_c::Callback data_ov004_020bc8dc;  /* func_ov004_020b7fec */
extern dMgState_c::Callback data_ov004_020bc8e4;  /* func_ov004_020b6f88 */
extern dMgState_c::Callback data_ov004_020bc8ec;  /* func_ov004_020b7eac */
extern dMgState_c::Callback data_ov004_020bc8f4;  /* func_ov004_020b7cd0 */
extern dMgState_c::Callback data_ov004_020bc8fc;  /* func_ov004_020b72d4 */
extern dMgState_c::Callback data_ov004_020bc904;  /* func_ov004_020b7c04 */
extern dMgState_c::Callback data_ov004_020bc90c;  /* func_ov004_020b7b20 */
extern dMgState_c::Callback data_ov004_020bc914;  /* func_ov004_020b7fec */
extern dMgState_c::Callback data_ov004_020bc91c;  /* func_ov004_020b68e8 */
extern dMgState_c::Callback data_ov004_020bc924;  /* func_ov004_020b6c9c */
extern dMgState_c::Callback data_ov004_020bc93c;  /* func_ov004_020b7a18 */
extern dMgState_c::Callback data_ov004_020bc98c;  /* func_ov004_020b7c04 */
extern dMgState_c::Callback data_ov004_020bc994;  /* func_ov004_020b7124 */
extern dMgState_c::Callback data_ov004_020bc9ac;  /* func_ov004_020b78f4 */
extern dMgState_c::Callback data_ov004_020bc9b4;  /* func_ov004_020b78f4 */
extern dMgState_c::Callback data_ov004_020bc9c4;  /* func_ov004_020b6948 */
extern dMgState_c::Callback data_ov004_020bc9cc;  /* func_ov004_020b6ddc */
extern dMgState_c::Callback data_ov004_020bc9e4;  /* func_ov004_020b77b4 */
extern dMgState_c::Callback data_ov004_020bc9fc;  /* func_ov004_020b75e4 */
extern dMgState_c::Callback data_ov004_020bca04;  /* func_ov004_020b7124 */
extern dMgState_c::Callback data_ov004_020bca14;  /* func_ov004_020b7fec */
extern dMgState_c::Callback data_ov004_020bca1c;  /* func_ov004_020b8560 */
extern dMgState_c::Callback data_ov004_020bca24;  /* func_ov004_020b746c */
extern dMgState_c::Callback data_ov004_020bca2c;  /* func_ov004_020b68e8 */
extern dMgState_c::Callback data_ov004_020bca34;  /* func_ov004_020b6b40 */

/* The per-state enter handlers, in state order. These are pointer-to-member
 * constants that mwccarm laid down in ov004's .data; the states they belong to
 * are not yet named, so they are still carried by address. */
extern dMgState_c::Callback data_ov004_020bc974;
extern dMgState_c::Callback data_ov004_020bc96c;
extern dMgState_c::Callback data_ov004_020bc964;
extern dMgState_c::Callback data_ov004_020bc95c;
extern dMgState_c::Callback data_ov004_020bc954;
extern dMgState_c::Callback data_ov004_020bc94c;
extern dMgState_c::Callback data_ov004_020bc944;
extern dMgState_c::Callback data_ov004_020bc92c;
extern dMgState_c::Callback data_ov004_020bc934;
extern dMgState_c::Callback data_ov004_020bc97c;
extern dMgState_c::Callback data_ov004_020bc984;
extern dMgState_c::Callback data_ov004_020bc99c;
extern dMgState_c::Callback data_ov004_020bc9a4;
extern dMgState_c::Callback data_ov004_020bca3c;
extern dMgState_c::Callback data_ov004_020bc9bc;
extern dMgState_c::Callback data_ov004_020bc9d4;
extern dMgState_c::Callback data_ov004_020bc9dc;
extern dMgState_c::Callback data_ov004_020bc9ec;
extern dMgState_c::Callback data_ov004_020bc9f4;
extern dMgState_c::Callback data_ov004_020bca0c;

// @symbol func_ov004_020b8a8c
extern "C" void func_ov004_020b8a8c(void) {
    data_ov004_020bc880 = 0x80;
    data_ov004_020bc854 = 0x80;
    data_ov004_020bc884 = 0x60;
    data_ov004_020bc888 = 0x80;
    data_ov004_020bc8b8 = 0x80;
    data_ov004_020bc87c = 0x60;
    data_ov004_020bc88c = 0x80;
    data_ov004_020bc860 = 0x60;
    data_ov004_020bc878 = 0x80;
    data_ov004_020bc890 = 0x60;
    data_ov004_020bc864 = -0x60;
    data_ov004_020bc8b4 = 0x40;
    data_ov004_020bc858 = 0x58;
    data_ov004_020bc874 = 0x28;
    data_ov004_020bc898 = 0x6c;
    data_ov004_020bc8a4 = 0x9c;
    data_ov004_020bc8a0 = 0x1d;
    data_ov004_020bc150 = 1;
    data_ov004_020bc868 = 0x80;
    data_ov004_020bc8b0 = 0x80;
    data_ov004_020bc8ac = 0x60;
    data_ov004_020bc894 = 0x80;
    data_ov004_020bc8a8 = 0x80;
    data_ov004_020bc86c = 0x80;
    data_ov004_020bc850 = 0x80;
    data_ov004_020bc89c = 0x60;
    data_ov004_020bc85c = 0x80;
    data_ov004_020bc870 = 0x80;
}

// @symbol _ZN10dMgState_cC1Ev
dMgState_c::dMgState_c()
    : mState(-1), mTimer(0), unk_020(0), unk_024(0)
{
}

// @symbol _ZN10dMgState_c8SetStateEi
void dMgState_c::SetState(s32 state)
{
    static Callback sEnterTable[20] = {
        data_ov004_020bc974,  /*  0: func_ov004_020b8688 */
        data_ov004_020bc96c,  /*  1: func_ov004_020b853c */
        data_ov004_020bc964,  /*  2: func_ov004_020b83ac */
        data_ov004_020bc95c,  /*  3: func_ov004_020b81f8 */
        data_ov004_020bc954,  /*  4: func_ov004_020b7f5c */
        data_ov004_020bc94c,  /*  5: func_ov004_020b7e38 */
        data_ov004_020bc944,  /*  6: func_ov004_020b7b90 */
        data_ov004_020bc92c,  /*  7: func_ov004_020b79b0 */
        data_ov004_020bc934,  /*  8: func_ov004_020b798c */
        data_ov004_020bc97c,  /*  9: func_ov004_020b7854 */
        data_ov004_020bc984,  /* 10: func_ov004_020b7744 */
        data_ov004_020bc99c,  /* 11: func_ov004_020b7594 */
        data_ov004_020bc9a4,  /* 12: func_ov004_020b7460 */
        data_ov004_020bca3c,  /* 13: func_ov004_020b743c */
        data_ov004_020bc9bc,  /* 14: func_ov004_020b724c */
        data_ov004_020bc9d4,  /* 15: func_ov004_020b70b4 */
        data_ov004_020bc9dc,  /* 16: func_ov004_020b7020 */
        data_ov004_020bc9ec,  /* 17: func_ov004_020b6f14 */
        data_ov004_020bc9f4,  /* 18: func_ov004_020b6d6c */
        data_ov004_020bca0c,  /* 19: func_ov004_020b6c10 */
    };

    mState = state;
    mEnter = sEnterTable[mState];
    mRender = 0;
    if (mEnter)
        (this->*mEnter)();
}

// @symbol _ZN10dMgState_c8BehaviorEv
void dMgState_c::Behavior()
{
    if (mState == -1)
        return;
    ApproachLinear(mTimer, 0, 1);
    if (mBehavior == 0)
        return;
    (this->*mBehavior)();
}

// @symbol _ZN10dMgState_c6RenderEv
void dMgState_c::Render()
{
    if (mState == -1)
        return;
    if (mRender == 0)
        return;
    (this->*mRender)();
}

// @symbol _ZN10dMgState_c19func_ov004_020b8688Ev
void dMgState_c::func_ov004_020b8688() {
  int prevId, id;
  mTimer = 0xb4;
  prevId = data_ov004_020bc8a0;
  id = 3;
  if (prevId != 0x1d) { data_ov004_020bc8a0 = 0x1d; id = prevId; }
  func_ov004_020b0cac(id, data_ov004_020bc880, data_ov004_020bc884, -1, -1, 0xd);
  func_ov004_020ae274((void*)4);
  mBehavior = data_ov004_020bca1c;
}

// @symbol _ZN10dMgState_c19func_ov004_020b8560Ev
void dMgState_c::func_ov004_020b8560() {
  Base* scene = data_ov004_020beb68;
  if (data_0209f5bc->IsAtEnd()) {
    data_0209f5bc->SetBackwardTime(0x1e, 0);
    FreeGfxSlotsById(0x1d);
    func_ov004_020ae20c();
    func_ov004_020ae2c8();
    func_ov004_020b29a0(scene, (void *)mState);
    mTimer = 0;
    mState = -1;
    unk_020 = 0;
    unk_024 = 0;
  } else {
    if (mTimer != 0)
      return;
    if (data_0209f5bc->IsBetweenStartAndEnd())
      return;
    if (scene == 0)
      return;
    if (scene->OnTurnIntoEgg(mState) == 0)
      return;
    data_0209f5bc->SetForwardTime(0x1e, 0);
  }
}

// @symbol _ZN10dMgState_c19func_ov004_020b853cEv
void dMgState_c::func_ov004_020b853c() { mTimer = 15; mBehavior = data_ov004_020bc8bc; }

// @symbol _ZN10dMgState_c19func_ov004_020b841cEv
void dMgState_c::func_ov004_020b841c() {
  if (data_0209f5bc->IsAtEnd() != 0) {
    Base* scene = data_ov004_020beb68;
    if (scene == 0) return;
    if (scene->OnTurnIntoEgg(mState) == 0) return;
    data_0209f5bc->SetBackwardTime(0x1e, 0);
    FreeGfxSlotsById(0x1d);
    func_ov004_020b29a0(scene, (void *)mState);
    mTimer = 0;
    mState = -1;
    unk_020 = 0;
    unk_024 = 0;
  } else {
    if (mTimer != 0) return;
    if (data_0209f5bc->IsBetweenStartAndEnd() != 0) return;
    data_0209f5bc->SetForwardTime(0x1e, 0);
  }
}

// @symbol _ZN10dMgState_c19func_ov004_020b83acEv
void dMgState_c::func_ov004_020b83ac() {
  mTimer = 0xb4;
  func_ov004_020b0cac(5, data_ov004_020bc854, data_ov004_020bc87c, -1, -1, 0xd);
  func_ov004_020ae274((void*)1);
  mBehavior = data_ov004_020bc8c4;
}

// @symbol _ZN10dMgState_c19func_ov004_020b8284Ev
void dMgState_c::func_ov004_020b8284() {
  if (data_0209f5bc->IsAtEnd()) {
    Base* scene = data_ov004_020beb68;
    if (scene == 0)
      return;
    if (scene->OnTurnIntoEgg(mState) == 0)
      return;
    data_0209f5bc->SetBackwardTime(0x1e, 0);
    FreeGfxSlotsById(0x1d);
    func_ov004_020ae20c();
    func_ov004_020ae2c8();
    func_ov004_020b29a0(scene, (void *)mState);
    mTimer = 0;
    mState = -1;
    unk_020 = 0;
    unk_024 = 0;
  } else {
    if (mTimer != 0)
      return;
    if (data_0209f5bc->IsBetweenStartAndEnd() == 0)
      data_0209f5bc->SetForwardTime(0x1e, 0);
  }
}

// @symbol _ZN10dMgState_c19func_ov004_020b81f8Ev
void dMgState_c::func_ov004_020b81f8() {
  int prevId, id;
  mTimer = 0x3c;
  prevId = data_ov004_020bc8a0;
  id = 8;
  if (prevId != 0x1d) { data_ov004_020bc8a0 = 0x1d; id = prevId; }
  func_ov004_020b0cac(id, data_ov004_020bc8b8, data_ov004_020bc8b4, -1, -1, 0xd);
  func_ov004_020ae274((void*)3);
  mBehavior = data_ov004_020bc8d4;
}

// @symbol _ZN10dMgState_c19func_ov004_020b8098Ev
void dMgState_c::func_ov004_020b8098() {
  Obj* obj;
  if (func_ov004_020b0b1c(0)) {
    obj = data_0209f5bc;
    obj->SetForwardTime(0x1e, 0);
    mBehavior = data_ov004_020bc8dc;
    return;
  }
  if (func_ov004_020b0b1c(2) || func_ov004_020b0b1c(1)) {
    obj = data_0209f5bc;
    if (obj->IsBetweenStartAndEnd())
      return;
    dScene_c::SetFaders(&data_0209f61c);
    data_0209f61c.func_0202ec9c(2);
    dScene_c::StartSceneFade(5, 0, 0);
    func_02012dd0((void*)0x3c);
    return;
  }
  if (mTimer != 0)
    return;
  func_ov004_020b0cac(0, data_ov004_020bc8a8, data_ov004_020bc898, -1, -1, 0xd);
  func_ov004_020b0cac(2, data_ov004_020bc86c, data_ov004_020bc8a4, -1, -1, 0xd);
}

// @symbol _ZN10dMgState_c19func_ov004_020b7fecEv
void dMgState_c::func_ov004_020b7fec() {
  if (data_0209f5bc->IsAtEnd() == 0)
    return;
  void* scene = data_ov004_020beb68;
  if (scene == 0)
    return;
  data_0209f5bc->SetBackwardTime(0x1e, 0);
  FreeGfxSlotsById(0x1d);
  func_ov004_020ae20c();
  func_ov004_020ae2c8();
  func_ov004_020b29a0(scene, (void *)mState);
  mTimer = 0;
  mState = -1;
  unk_020 = 0;
  unk_024 = 0;
}

// @symbol _ZN10dMgState_c19func_ov004_020b7f5cEv
void dMgState_c::func_ov004_020b7f5c() {
  int prevId, id;
  mTimer = 0x78;
  prevId = data_ov004_020bc8a0;
  id = 9;
  if (prevId != 0x1d) { data_ov004_020bc8a0 = 0x1d; id = prevId; }
  func_ov004_020b0cac(id, data_ov004_020bc88c, data_ov004_020bc860, -1, -1, 0xd);
  Sound::PlayBank2_2D(0x136);
  mBehavior = data_ov004_020bc8ec;
}

// @symbol _ZN10dMgState_c19func_ov004_020b7eacEv
void dMgState_c::func_ov004_020b7eac() {
  if (mTimer > 0x64)
    return;
  if (mTimer != 0)
    return;
  Base* scene = data_ov004_020beb68;
  if (scene == 0)
    return;
  if (scene->OnTurnIntoEgg(mState) == 0)
    return;
  FreeGfxSlotsById(0x1d);
  func_ov004_020ae20c();
  func_ov004_020ae2c8();
  func_ov004_020b29a0(scene, (void *)mState);
  mTimer = 0;
  mState = -1;
  unk_020 = 0;
  unk_024 = 0;
}

// @symbol _ZN10dMgState_c19func_ov004_020b7e38Ev
void dMgState_c::func_ov004_020b7e38() {
    mTimer = 0x78;
    func_ov004_020b0cac(0xa, data_ov004_020bc878, data_ov004_020bc890, -1, -1, 0xd);
    Sound::PlayBank2_2D(0x137);
    mBehavior = data_ov004_020bc8f4;
}

// @symbol _ZN10dMgState_c19func_ov004_020b7cd0Ev
void dMgState_c::func_ov004_020b7cd0() {
  int state = mTimer;
  if (state > 0x64)
    return;
  Base* p = data_ov004_020beb68;
  int a8 = (p != 0) ? p->unk_0a8 : 0;
  if (a8 == 0) {
    if (state != 0)
      return;
    Base* q = data_ov004_020beb68;
    if (q == 0)
      return;
    if (q->OnTurnIntoEgg(mState) == 0)
      return;
    data_ov004_020bc7d4 = 0;
    data_ov004_020bfa24 = 0;
    func_ov004_020ae20c();
    FreeGfxSlotsById(0x1d);
    mTimer = 0xb4;
    mBehavior = data_ov004_020bc904;
  } else {
    if (state != 0)
      return;
    Base* scene = data_ov004_020beb68;
    if (scene == 0)
      return;
    if (scene->OnTurnIntoEgg(mState) == 0)
      return;
    FreeGfxSlotsById(0x1d);
    func_ov004_020ae20c();
    func_ov004_020ae2c8();
    func_ov004_020b29a0(scene, (void *)mState);
    mTimer = 0;
    mState = -1;
    unk_020 = 0;
    unk_024 = 0;
  }
}

// @symbol _ZN10dMgState_c19func_ov004_020b7c04Ev
void dMgState_c::func_ov004_020b7c04() {
    Obj* obj;
    int v = mTimer;
    if (v == 0xb3) {
        func_ov004_020b0cac(8, data_ov004_020bc8b8, data_ov004_020bc8b4, -1, -1, 0xd);
        func_ov004_020ae274((void*)3);
        return;
    }
    if (v != 0) return;
    obj = data_0209f5bc;
    if (obj->IsBetweenStartAndEnd() != 0) return;
    dScene_c::SetFaders(&data_0209f61c);
    data_0209f61c.func_0202ec9c(2);
    dScene_c::StartSceneFade(5, 0, 0);
    func_02012dd0((void*)0x3c);
}

// @symbol _ZN10dMgState_c19func_ov004_020b7b90Ev
void dMgState_c::func_ov004_020b7b90() {
    mTimer = 0x78;
    func_ov004_020b0cac(0xa, data_ov004_020bc878, data_ov004_020bc890, -1, -1, 0xd);
    Sound::PlayBank2_2D(0x137);
    mBehavior = data_ov004_020bc90c;
}

// @symbol _ZN10dMgState_c19func_ov004_020b7b20Ev
void dMgState_c::func_ov004_020b7b20() {
  int v = mTimer;
  if(v > 0x64) return;
  if(v != 0) return;
  FreeGfxSlotsById(0x1d);
  mBehavior = data_ov004_020bc93c;
  unk_024 = 1;
  data_ov004_020bc7d4 = 0;
  data_ov004_020bfa24 = 0;
}

// @symbol _ZN10dMgState_c19func_ov004_020b7a18Ev
void dMgState_c::func_ov004_020b7a18() {
    Base *g = data_ov004_020beb68;
    int cond = g ? g->unk_0a8 : 0;
    if (cond == 0) {
        g = data_ov004_020beb68;
        if (!g) return;
        if (!g->OnTurnIntoEgg(mState)) return;
        func_ov004_020ae20c();
        mTimer = 0xb4;
        mBehavior = data_ov004_020bc98c;
        return;
    }
    g = data_ov004_020beb68;
    if (!g) return;
    if (!g->OnTurnIntoEgg(mState)) return;
    func_ov004_020ae20c();
    func_ov004_020ae2c8();
    func_ov004_020b29a0(g, (void *)mState);
    mTimer = 0;
    mState = -1;
    unk_020 = 0;
    unk_024 = 0;
}

// @symbol _ZN10dMgState_c19func_ov004_020b79b0Ev
void dMgState_c::func_ov004_020b79b0() {
    mTimer = 0x78;
    func_ov004_020b0cac(0xb, data_ov004_020bc868, data_ov004_020bc858, -1, -1, 0xd);
    mBehavior = data_ov004_020bc9ac;
}

// @symbol _ZN10dMgState_c19func_ov004_020b798cEv
void dMgState_c::func_ov004_020b798c() { mTimer = 0; mBehavior = data_ov004_020bc9b4; }

// @symbol _ZN10dMgState_c19func_ov004_020b78f4Ev
void dMgState_c::func_ov004_020b78f4() {
  Base* scene;
  if (mTimer != 0) return;
  scene = data_ov004_020beb68;
  if (scene == 0) return;
  if (scene->OnTurnIntoEgg(mState) == 0) return;
  FreeGfxSlotsById(0x1d);
  func_ov004_020b29a0(scene, (void *)mState);
  mTimer = 0;
  mState = -1;
  unk_020 = 0;
  unk_024 = 0;
}

// @symbol _ZN10dMgState_c19func_ov004_020b7854Ev
void dMgState_c::func_ov004_020b7854() {
    int id;
    mTimer = 0xb4;
    func_ov004_020ae274((void*)4);
    id = 3;
    if (data_ov004_020bc8a0 != 0x1d) { id = data_ov004_020bc8a0; data_ov004_020bc8a0 = 0x1d; }
    func_ov004_020b0cac(id, data_ov004_020bc8b0, data_ov004_020bc8ac - (data_ov004_020beb6c + 0xc0), -1, -1, 0xa);
    mBehavior = data_ov004_020bc9e4;
}

// @symbol _ZN10dMgState_c19func_ov004_020b77b4Ev
void dMgState_c::func_ov004_020b77b4() {
  if (mTimer != 0)
    return;
  Base* scene = data_ov004_020beb68;
  if (scene == 0)
    return;
  if (scene->OnTurnIntoEgg(mState) == 0)
    return;
  FreeGfxSlotsById(0x1d);
  func_ov004_020ae20c();
  func_ov004_020ae2c8();
  func_ov004_020b29a0(scene, (void *)mState);
  mTimer = 0;
  mState = -1;
  unk_020 = 0;
  unk_024 = 0;
}

// @symbol _ZN10dMgState_c19func_ov004_020b7744Ev
void dMgState_c::func_ov004_020b7744() {
  mTimer = 0xb4;
  func_ov004_020ae274((void*)3);
  func_ov004_020b0cac(8, data_ov004_020bc894, data_ov004_020bc874, -1, -1, 0xb);
  mBehavior = data_ov004_020bc9fc;
}

// @symbol _ZN10dMgState_c19func_ov004_020b75e4Ev
void dMgState_c::func_ov004_020b75e4() {
  Obj* obj;
  if (func_ov004_020b0b1c(0)) {
    obj = data_0209f5bc;
    obj->SetForwardTime(0x1e, 0);
    mBehavior = data_ov004_020bca14;
    return;
  }
  if (func_ov004_020b0b1c(2) || func_ov004_020b0b1c(1)) {
    obj = data_0209f5bc;
    if (obj->IsBetweenStartAndEnd())
      return;
    dScene_c::SetFaders(&data_0209f61c);
    data_0209f61c.func_0202ec9c(2);
    dScene_c::StartSceneFade(5, 0, 0);
    func_02012dd0((void*)0x3c);
    return;
  }
  if (mTimer != 0)
    return;
  func_ov004_020b0cac(0, data_ov004_020bc8a8, data_ov004_020bc898, -1, -1, 0xd);
  func_ov004_020b0cac(1, data_ov004_020bc86c, data_ov004_020bc8a4, -1, -1, 0xd);
}

// @symbol _ZN10dMgState_c19func_ov004_020b7594Ev
void dMgState_c::func_ov004_020b7594() {
  mTimer=0xb4;
  func_ov004_020ae274((void*)1);
  mBehavior = data_ov004_020bca24;
  mRender = data_ov004_020bc8cc;
}

// @symbol _ZN10dMgState_c19func_ov004_020b746cEv
void dMgState_c::func_ov004_020b746c() {
  if (data_0209f5bc->IsAtEnd()) {
    Base* scene = data_ov004_020beb68;
    if (scene == 0)
      return;
    if (scene->OnTurnIntoEgg(mState) == 0)
      return;
    data_0209f5bc->SetBackwardTime(0x1e, 0);
    FreeGfxSlotsById(0x1d);
    func_ov004_020ae20c();
    func_ov004_020ae2c8();
    func_ov004_020b29a0(scene, (void *)mState);
    mTimer = 0;
    mState = -1;
    unk_020 = 0;
    unk_024 = 0;
  } else {
    if (mTimer != 0)
      return;
    if (data_0209f5bc->IsBetweenStartAndEnd() == 0)
      data_0209f5bc->SetForwardTime(0x1e, 0);
  }
}

// @symbol _ZN10dMgState_c19func_ov004_020b7460Ev
void dMgState_c::func_ov004_020b7460() {
    func_ov004_020b743c();
}

// @symbol _ZN10dMgState_c19func_ov004_020b743cEv
void dMgState_c::func_ov004_020b743c() { mTimer = 10; mBehavior = data_ov004_020bc8fc; }

// @symbol _ZN10dMgState_c19func_ov004_020b72d4Ev
void dMgState_c::func_ov004_020b72d4() {
  if (mTimer == 0) {
    if (func_ov004_020b0b1c(0) != 0) {
      data_0209f5bc->SetForwardTime(0x1e, 0);
      mBehavior = data_ov004_020bc914;
      return;
    }
    if (func_ov004_020b0b1c(2) == 0) {
      if (func_ov004_020b0b1c(1) == 0)
        return;
    }
    if (data_0209f5bc->IsBetweenStartAndEnd() != 0)
      return;
    dScene_c::SetFaders(&data_0209f61c);
    data_0209f61c.func_0202ec9c(2);
    dScene_c::StartSceneFade(5, 0, 0);
    func_02012dd0((void*)0x3c);
    return;
  }
  func_ov004_020b0cac(0, data_ov004_020bc8a8, data_ov004_020bc898, -1, -1, 0xd);
  func_ov004_020b0cac(2, data_ov004_020bc86c, data_ov004_020bc8a4, -1, -1, 0xd);
  mTimer = 0;
}

// @symbol _ZN10dMgState_c19func_ov004_020b724cEv
void dMgState_c::func_ov004_020b724c() {
  mTimer = 0xb4;
  func_ov004_020b0cac(5, data_ov004_020bc854, data_ov004_020bc87c, -1, -1, 0xd);
  func_ov004_020ae274((void*)1);
  mBehavior = data_ov004_020bc994;
  mRender = data_ov004_020bc9c4;
}

// @symbol _ZN10dMgState_c19func_ov004_020b7124Ev
void dMgState_c::func_ov004_020b7124() {
  if (data_0209f5bc->IsAtEnd()) {
    Base* scene = data_ov004_020beb68;
    if (scene == 0)
      return;
    if (scene->OnTurnIntoEgg(mState) == 0)
      return;
    data_0209f5bc->SetBackwardTime(0x1e, 0);
    func_ov004_020ae20c();
    func_ov004_020ae2c8();
    func_ov004_020b29a0(scene, (void *)mState);
    FreeGfxSlotsById(0x1d);
    mTimer = 0;
    mState = -1;
    unk_020 = 0;
    unk_024 = 0;
  } else {
    if (mTimer != 0)
      return;
    if (data_0209f5bc->IsBetweenStartAndEnd() == 0)
      data_0209f5bc->SetForwardTime(0x1e, 0);
  }
}

// @symbol _ZN10dMgState_c19func_ov004_020b70b4Ev
void dMgState_c::func_ov004_020b70b4() {
  mTimer = 0x3c;
  func_ov004_020b0cac(0xe, data_ov004_020bc8b8, data_ov004_020bc8b4, -1, -1, 0xd);
  func_ov004_020ae274((void*)3);
  mBehavior = data_ov004_020bca04;
}

// @symbol _ZN10dMgState_c19func_ov004_020b7020Ev
void dMgState_c::func_ov004_020b7020() {
  if (func_ov004_020adbe0()) {
    func_ov004_020b6f14();
    return;
  }
  func_ov004_020b0cac(7, 0x80, 0x14, -1, -1, 0xd);
  mTimer = 0x78;
  mBehavior = data_ov004_020bc8e4;
  mRender = data_ov004_020bc91c;
  func_ov004_020ad90c();
}

// @symbol _ZN10dMgState_c19func_ov004_020b6f88Ev
void dMgState_c::func_ov004_020b6f88() {
  int ok;
  unsigned char idx;
  int i;
  if (mTimer != 0) return;
  idx = gActivePlayerSlot;
  ok = 0;
  i = idx * 4;
  if (gTouchHeld[i] != 0) {
    if (gTouchEdge[i] != 0) ok = 1;
  }
  if (ok == 0) return;
  func_02012790(0x62);
  FreeGfxSlotsById(7);
  *(W2 *)&mRender = data_02086b58;
  func_ov004_020b7460();
}

// @symbol _ZN10dMgState_c19func_ov004_020b6f14Ev
void dMgState_c::func_ov004_020b6f14() {
  func_ov004_020b0cac(7, 0x80, 0x14, -1, -1, 0xd);
  mTimer = 0x12c;
  mBehavior = data_ov004_020bc9cc;
  mRender = data_ov004_020bca2c;
  func_ov004_020ad90c();
}

// @symbol _ZN10dMgState_c19func_ov004_020b6ddcEv
void dMgState_c::func_ov004_020b6ddc() {
    int idx;
    int flag;
    Base *scene;
    int v;
    int s;
    int id;

    if (mTimer == 0) {
        idx = gActivePlayerSlot;
        flag = 0;
        if (gTouchHeld[idx * 4] != 0) {
            if (gTouchEdge[idx * 4] != 0)
                flag = 1;
        }
        if (flag != 0) {
            FreeGfxSlotsById(7);
            func_02012790(0x62);
            *(W2 *)&mRender = data_02086b58;
            func_ov004_020b7460();
        }
    }

    if (mTimer != 0xb4)
        return;

    scene = data_ov004_020beb68;
    if (scene) {
        scene->Virtual50();
    }

    id = 5;
    v = data_ov004_020bc8a0;
    if (v != 0x1d) {
        data_ov004_020bc8a0 = 0x1d;
        id = v;
    }

    s = data_ov004_020bc864;
    if (s < 0)
        s -= func_ov004_020b04c0();

    func_ov004_020b0cac(id, data_ov004_020bc888, s, -1, -1, 0xd);
    func_ov004_020ae274((void*)1);
}

// @symbol _ZN10dMgState_c19func_ov004_020b6d6cEv
void dMgState_c::func_ov004_020b6d6c() {
  mTimer = 0xb4;
  func_ov004_020b0cac(8, data_ov004_020bc8b8, data_ov004_020bc8b4, -1, -1, 0xd);
  func_ov004_020ae274((void*)3);
  mBehavior = data_ov004_020bc924;
}

// @symbol _ZN10dMgState_c19func_ov004_020b6c9cEv
void dMgState_c::func_ov004_020b6c9c() {
    int x = mTimer;
    Base *g = data_ov004_020beb68;
    if (x != 0) {
        unsigned char idx = gActivePlayerSlot;
        int off = idx * 4;
        int ok = 0;
        if (gTouchHeld[off]) {
            if (gTouchEdge[off]) ok = 1;
        }
        if (ok == 0)
            return;
    }
    if (g->OnTurnIntoEgg(mState) == 0)
        return;
    *(W2 *)&mRender = data_02086b58;
    FreeGfxSlotsById(0x1d);
    if (func_ov004_020adbe0() != 0) {
        func_ov004_020b6f14();
        return;
    }
    func_ov004_020b7020();
}

// @symbol _ZN10dMgState_c19func_ov004_020b6c10Ev
void dMgState_c::func_ov004_020b6c10() {
  int prevId, id;
  mTimer = 0xb4;
  prevId = data_ov004_020bc8a0;
  id = 3;
  if (prevId != 0x1d) { data_ov004_020bc8a0 = 0x1d; id = prevId; }
  func_ov004_020b0cac(id, data_ov004_020bc880, data_ov004_020bc884, -1, -1, 0xd);
  func_ov004_020ae274((void*)4);
  mBehavior = data_ov004_020bca34;
}

// @symbol _ZN10dMgState_c19func_ov004_020b6b40Ev
void dMgState_c::func_ov004_020b6b40() {
    int x = mTimer;
    Base *g = data_ov004_020beb68;
    if (x != 0) {
        unsigned char idx = gActivePlayerSlot;
        int off = idx * 4;
        int ok = 0;
        if (gTouchHeld[off]) {
            if (gTouchEdge[off]) ok = 1;
        }
        if (ok == 0)
            return;
    }
    if (g->OnTurnIntoEgg(mState) == 0)
        return;
    *(W2 *)&mRender = data_02086b58;
    FreeGfxSlotsById(0x1d);
    if (func_ov004_020adbe0() != 0) {
        func_ov004_020b6f14();
        return;
    }
    func_ov004_020b7020();
}

// @symbol _ZN10dMgState_c19func_ov004_020b6ad8Ev
void dMgState_c::func_ov004_020b6ad8() {
    int r;
    if (data_ov004_020beb68 == 0) return;
    r = func_ov004_020adbc0();
    func_ov004_020b2444(data_ov004_020bc850, data_ov004_020bc89c, r, -1, -1, 0, 0);
}

// @symbol _ZN10dMgState_c19func_ov004_020b6948Ev
void dMgState_c::func_ov004_020b6948() {
    unsigned int r;
    int idx;

    if (data_ov004_020beb68 == 0) return;

    r = func_ov004_020adbc0();
    func_ov004_020b2444(data_ov004_020bc85c + 4, data_ov004_020bc870, r / 100, -1, -1, 1, 0);

    r = func_ov004_020adbc0();
    func_ov004_020b2444(data_ov004_020bc85c + 0x1c, data_ov004_020bc870, r % 100 / 10, -1, -1, 1, 0);

    r = func_ov004_020adbc0();
    func_ov004_020b2444(data_ov004_020bc85c + 0x2c, data_ov004_020bc870, r % 10, -1, -1, 1, 0);

    idx = GetGameLanguage();
    func_ov004_020af948((void *)*(int *)(data_ov004_020bca44[idx] + 0xc), data_ov004_020bc85c - 0x24, data_ov004_020bc870, 0);

    idx = GetGameLanguage();
    func_ov004_020af948((void *)*(int *)(data_ov004_020bca44[idx] + 0x1c), data_ov004_020bc85c + 0x10, data_ov004_020bc870, 0);
}

// @symbol _ZN10dMgState_c19func_ov004_020b68e8Ev
void dMgState_c::func_ov004_020b68e8() {
    void *g = data_ov004_020beb68;
    if (g == 0) return;
    func_ov004_020b14f0(g);
    if (mTimer > 0) return;
    func_ov004_020b0d8c(g, 0xe0, 0xa0);
}

// @symbol func_ov004_020b682c
extern "C" void func_ov004_020b682c(void)
{
    int v1 = -0x1c - func_ov004_020b04c0();
    data_ov004_020bc88c = 0x80;
    data_ov004_020bc860 = v1;
    {
        int v2 = -0x1c - func_ov004_020b04c0();
        data_ov004_020bc878 = 0x80;
        data_ov004_020bc890 = v2;
    }
    {
        int v3 = -0x1c - func_ov004_020b04c0();
        data_ov004_020bc868 = 0x80;
        data_ov004_020bc858 = v3;
        data_ov004_020bc888 = 0x80;
        data_ov004_020bc864 = -0x1c;
        data_ov004_020bc8b8 = 0x80;
        data_ov004_020bc8b4 = 0x60;
    }
}

// @symbol func_ov004_020b6808
extern "C" void func_ov004_020b6808(void)
{
    data_ov004_020bc888 = 0x80;
    data_ov004_020bc864 = -0x1c;
}

// @symbol func_ov004_020b67f8
extern "C" void func_ov004_020b67f8(void) {
    func_ov004_020b67e8(6);
}

// @symbol func_ov004_020b67e8
extern "C" void func_ov004_020b67e8(int v) { data_ov004_020bc8a0 = v; }
