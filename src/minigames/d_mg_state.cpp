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
 * .data window (Mg_SetNextBanner, 020b67f8, 020b6808, 020b682c and
 * Mg_ResetBannerLayout).
 *
 * Edges. 0x020b67e8 is the first function after func_ov004_020b67e4, the last
 * destructor helper __sinit_ov004_020b9ad0 registers for the preceding unit
 * (the 0x14-entry array at sMgCoins). 0x020b8c18 is the start of
 * dMgPsOpt_c. The unit's .data window, 0x020bc850..0x020bca44, is read only
 * by this unit's functions and ends before dMgPsOpt_c's at 0x020bca58; the
 * words at gMgCoinsShowDelta and sMgCoinsDeltaShown belong to the preceding
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
 * graphic through Mg_ShowBanner (file index, x, y; the x/y words at
 * data_ov004_020bc850..0x020bc8b8, except 0x020bc8a0, the file-index
 * override, are set by Mg_ResetBannerLayout and patched by
 * Mg_ResetRecordBannerPos and Mg_PlaceBannersOnTopScreen) and installs a
 * behavior. sMgNextBanner replaces the next such file index once;
 * 0x1d there means no override. The behavior waits for mTimer to run out
 * and for the scene's CanEndState(mState) to return nonzero, in several
 * states around a screen fade (SetForwardTime out, then SetBackwardTime
 * once the fader IsAtEnd). It then frees every graphics slot
 * (FreeGfxSlotsById(0x1d)), passes mState to Mg_CallStartRound and
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
 *   - mState doubles as the id passed to the scene's CanEndState and,
 *     through Mg_CallStartRound, to its StartRound. The `(void *)mState`
 *     spelling follows that helper's declaration.
 *
 * Retired one-function sources (ROM address order):
 *   0x020b67e8  Mg_SetNextBanner
 *   0x020b67f8  Mg_SetPerfectBanner
 *   0x020b6808  Mg_ResetRecordBannerPos
 *   0x020b682c  Mg_PlaceBannersOnTopScreen
 *   0x020b68e8  DrawRecordTable
 *   0x020b6948  DrawScoreDigits
 *   0x020b6ad8  DrawScore
 *   0x020b6b40  State19_Update
 *   0x020b6c10  State19_Enter
 *   0x020b6c9c  State18_Update
 *   0x020b6d6c  State18_Enter
 *   0x020b6ddc  State17_Update
 *   0x020b6f14  State17_Enter
 *   0x020b6f88  State16_Update
 *   0x020b7020  State16_Enter
 *   0x020b70b4  State15_Enter
 *   0x020b7124  UpdateFadeThenResume
 *   0x020b724c  State14_Enter
 *   0x020b72d4  UpdateRetryPrompt
 *   0x020b743c  State13_Enter
 *   0x020b7460  State12_Enter
 *   0x020b746c  State11_Update
 *   0x020b7594  State11_Enter
 *   0x020b75e4  State10_Update
 *   0x020b7744  State10_Enter
 *   0x020b77b4  State09_Update
 *   0x020b7854  State09_Enter
 *   0x020b78f4  UpdateWaitThenResume
 *   0x020b798c  State08_Enter
 *   0x020b79b0  State07_Enter
 *   0x020b7a18  UpdateAfterHidden
 *   0x020b7b20  State06_Update
 *   0x020b7b90  State06_Enter
 *   0x020b7c04  UpdateGameOverToMenu
 *   0x020b7cd0  State05_Update
 *   0x020b7e38  State05_Enter
 *   0x020b7eac  State04_Update
 *   0x020b7f5c  State04_Enter
 *   0x020b7fec  UpdateRetryFadeOut
 *   0x020b8098  State03_Update
 *   0x020b81f8  State03_Enter
 *   0x020b8284  State02_Update
 *   0x020b83ac  State02_Enter
 *   0x020b841c  State01_Update
 *   0x020b853c  State01_Enter
 *   0x020b8560  State00_Update
 *   0x020b8688  State00_Enter
 *   0x020b8a8c  Mg_ResetBannerLayout
 */

#include "dMgState_c.h"
#include "dScene_c.h"
#include "dWipe_c.h"
#include "PlayerInput.h"

extern int ApproachLinear(s32 &value, s32 target, s32 step);
/* local extern: including Sound.h renumbers SetState's local static. */
namespace Sound { u32 PlayBank2_2D(u32); }

/* The minigame scene at gMgScene, seen through the two virtuals
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
    virtual void StartRound(int);      /* slot 18, reached via Mg_CallStartRound */
    virtual int CanEndState(int mode);  /* slot 19 */
    virtual void OnNewRecord();             /* slot 20 */
    char pad[0xa4];
    s32 mCredits;                          /* 0x0a8 */
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
extern void Mg_CommitRecords(void);
extern int Mg_GetSubmittedScore(void);
extern int Mg_IsNewRecord(void);
extern void Mg_RestoreMusic(void);
extern void Mg_PlayJingle(void* c);
extern void Mg_StartMusic(void);
extern void Mg_RenderSubSpriteMtx(void *a, int b, int c, void *d);
extern int Mg_GetTopScreenYOffset(void);
extern int Mg_CheckBannerButton(int arg);
extern void Mg_ShowBanner(int c, int a1, int a2, int a3, int arg5, short arg6);
extern void Mg_DrawBlinkSprite(void *, int, int);
extern void Mg_DrawRecordTable(void *);
extern void Mg_DrawBigNumber(int a, int b, int c, int d, int e, int f, int g);
extern void Mg_CallStartRound(void* c, void* arg);
extern void Mg_SetNextBanner(int);
}

struct W2 { int w[2]; };
extern W2 data_02086b58;  /* __ptmf_null's address: the null member pointer */
extern Obj* data_0209f5bc;
extern dWipe_c data_0209f61c;
extern Base* gMgScene;  /* local extern: seen through the TU-local Base view (see Base above) */
extern int sMgTopScreenYOffset;
extern int sMgBannerButtonsArmed;
extern int gMgCoinsShowDelta;
extern int sMgCoinsDeltaShown;

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
extern int sMgNextBanner;
extern int data_ov004_020bc8a4;
extern int data_ov004_020bc8a8;
extern int data_ov004_020bc8ac;
extern int data_ov004_020bc8b0;
extern int data_ov004_020bc8b4;
extern int data_ov004_020bc8b8;
extern unsigned char *data_ov004_020bca44[];

/* Behavior and render records. Each comment names the member the record
   points at (ov004 relocs.txt). */
extern dMgState_c::Callback pmf_dMgState_c_State01_Update;  /* State01_Update */
extern dMgState_c::Callback pmf_dMgState_c_State02_Update;  /* State02_Update */
extern dMgState_c::Callback pmf_dMgState_c_DrawScore;  /* DrawScore */
extern dMgState_c::Callback pmf_dMgState_c_State03_Update;  /* State03_Update */
extern dMgState_c::Callback pmf_dMgState_c_UpdateRetryFadeOut;  /* UpdateRetryFadeOut */
extern dMgState_c::Callback pmf_dMgState_c_State16_Update;  /* State16_Update */
extern dMgState_c::Callback pmf_dMgState_c_State04_Update;  /* State04_Update */
extern dMgState_c::Callback pmf_dMgState_c_State05_Update;  /* State05_Update */
extern dMgState_c::Callback pmf_dMgState_c_UpdateRetryPrompt;  /* UpdateRetryPrompt */
extern dMgState_c::Callback pmf_dMgState_c_UpdateGameOverToMenu;  /* UpdateGameOverToMenu */
extern dMgState_c::Callback pmf_dMgState_c_State06_Update;  /* State06_Update */
extern dMgState_c::Callback pmf_dMgState_c_UpdateRetryFadeOut_2;  /* UpdateRetryFadeOut */
extern dMgState_c::Callback pmf_dMgState_c_DrawRecordTable;  /* DrawRecordTable */
extern dMgState_c::Callback pmf_dMgState_c_State18_Update;  /* State18_Update */
extern dMgState_c::Callback pmf_dMgState_c_UpdateAfterHidden;  /* UpdateAfterHidden */
extern dMgState_c::Callback pmf_dMgState_c_UpdateGameOverToMenu_2;  /* UpdateGameOverToMenu */
extern dMgState_c::Callback pmf_dMgState_c_UpdateFadeThenResume;  /* UpdateFadeThenResume */
extern dMgState_c::Callback pmf_dMgState_c_UpdateWaitThenResume;  /* UpdateWaitThenResume */
extern dMgState_c::Callback pmf_dMgState_c_UpdateWaitThenResume_2;  /* UpdateWaitThenResume */
extern dMgState_c::Callback pmf_dMgState_c_DrawScoreDigits;  /* DrawScoreDigits */
extern dMgState_c::Callback pmf_dMgState_c_State17_Update;  /* State17_Update */
extern dMgState_c::Callback pmf_dMgState_c_State09_Update;  /* State09_Update */
extern dMgState_c::Callback pmf_dMgState_c_State10_Update;  /* State10_Update */
extern dMgState_c::Callback pmf_dMgState_c_UpdateFadeThenResume_2;  /* UpdateFadeThenResume */
extern dMgState_c::Callback pmf_dMgState_c_UpdateRetryFadeOut_3;  /* UpdateRetryFadeOut */
extern dMgState_c::Callback pmf_dMgState_c_State00_Update;  /* State00_Update */
extern dMgState_c::Callback pmf_dMgState_c_State11_Update;  /* State11_Update */
extern dMgState_c::Callback pmf_dMgState_c_DrawRecordTable_2;  /* DrawRecordTable */
extern dMgState_c::Callback pmf_dMgState_c_State19_Update;  /* State19_Update */

/* The per-state enter handlers, in state order. These are pointer-to-member
 * constants that mwccarm laid down in ov004's .data; the states they belong to
 * are not yet named, so they are still carried by address. */
extern dMgState_c::Callback pmf_dMgState_c_State00_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State01_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State02_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State03_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State04_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State05_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State06_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State07_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State08_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State09_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State10_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State11_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State12_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State13_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State14_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State15_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State16_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State17_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State18_Enter;
extern dMgState_c::Callback pmf_dMgState_c_State19_Enter;

// @symbol Mg_ResetBannerLayout
extern "C" void Mg_ResetBannerLayout(void) {
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
    sMgNextBanner = 0x1d;
    sMgBannerButtonsArmed = 1;
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
        pmf_dMgState_c_State00_Enter,  /*  0: State00_Enter */
        pmf_dMgState_c_State01_Enter,  /*  1: State01_Enter */
        pmf_dMgState_c_State02_Enter,  /*  2: State02_Enter */
        pmf_dMgState_c_State03_Enter,  /*  3: State03_Enter */
        pmf_dMgState_c_State04_Enter,  /*  4: State04_Enter */
        pmf_dMgState_c_State05_Enter,  /*  5: State05_Enter */
        pmf_dMgState_c_State06_Enter,  /*  6: State06_Enter */
        pmf_dMgState_c_State07_Enter,  /*  7: State07_Enter */
        pmf_dMgState_c_State08_Enter,  /*  8: State08_Enter */
        pmf_dMgState_c_State09_Enter,  /*  9: State09_Enter */
        pmf_dMgState_c_State10_Enter,  /* 10: State10_Enter */
        pmf_dMgState_c_State11_Enter,  /* 11: State11_Enter */
        pmf_dMgState_c_State12_Enter,  /* 12: State12_Enter */
        pmf_dMgState_c_State13_Enter,  /* 13: State13_Enter */
        pmf_dMgState_c_State14_Enter,  /* 14: State14_Enter */
        pmf_dMgState_c_State15_Enter,  /* 15: State15_Enter */
        pmf_dMgState_c_State16_Enter,  /* 16: State16_Enter */
        pmf_dMgState_c_State17_Enter,  /* 17: State17_Enter */
        pmf_dMgState_c_State18_Enter,  /* 18: State18_Enter */
        pmf_dMgState_c_State19_Enter,  /* 19: State19_Enter */
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

// @symbol _ZN10dMgState_c13State00_EnterEv
void dMgState_c::State00_Enter() {
  int prevId, id;
  mTimer = 0xb4;
  prevId = sMgNextBanner;
  id = 3;
  if (prevId != 0x1d) { sMgNextBanner = 0x1d; id = prevId; }
  Mg_ShowBanner(id, data_ov004_020bc880, data_ov004_020bc884, -1, -1, 0xd);
  Mg_PlayJingle((void*)4);
  mBehavior = pmf_dMgState_c_State00_Update;
}

// @symbol _ZN10dMgState_c14State00_UpdateEv
void dMgState_c::State00_Update() {
  Base* scene = gMgScene;
  if (data_0209f5bc->IsAtEnd()) {
    data_0209f5bc->SetBackwardTime(0x1e, 0);
    FreeGfxSlotsById(0x1d);
    Mg_RestoreMusic();
    Mg_StartMusic();
    Mg_CallStartRound(scene, (void *)mState);
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
    if (scene->CanEndState(mState) == 0)
      return;
    data_0209f5bc->SetForwardTime(0x1e, 0);
  }
}

// @symbol _ZN10dMgState_c13State01_EnterEv
void dMgState_c::State01_Enter() { mTimer = 15; mBehavior = pmf_dMgState_c_State01_Update; }

// @symbol _ZN10dMgState_c14State01_UpdateEv
void dMgState_c::State01_Update() {
  if (data_0209f5bc->IsAtEnd() != 0) {
    Base* scene = gMgScene;
    if (scene == 0) return;
    if (scene->CanEndState(mState) == 0) return;
    data_0209f5bc->SetBackwardTime(0x1e, 0);
    FreeGfxSlotsById(0x1d);
    Mg_CallStartRound(scene, (void *)mState);
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

// @symbol _ZN10dMgState_c13State02_EnterEv
void dMgState_c::State02_Enter() {
  mTimer = 0xb4;
  Mg_ShowBanner(5, data_ov004_020bc854, data_ov004_020bc87c, -1, -1, 0xd);
  Mg_PlayJingle((void*)1);
  mBehavior = pmf_dMgState_c_State02_Update;
}

// @symbol _ZN10dMgState_c14State02_UpdateEv
void dMgState_c::State02_Update() {
  if (data_0209f5bc->IsAtEnd()) {
    Base* scene = gMgScene;
    if (scene == 0)
      return;
    if (scene->CanEndState(mState) == 0)
      return;
    data_0209f5bc->SetBackwardTime(0x1e, 0);
    FreeGfxSlotsById(0x1d);
    Mg_RestoreMusic();
    Mg_StartMusic();
    Mg_CallStartRound(scene, (void *)mState);
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

// @symbol _ZN10dMgState_c13State03_EnterEv
void dMgState_c::State03_Enter() {
  int prevId, id;
  mTimer = 0x3c;
  prevId = sMgNextBanner;
  id = 8;
  if (prevId != 0x1d) { sMgNextBanner = 0x1d; id = prevId; }
  Mg_ShowBanner(id, data_ov004_020bc8b8, data_ov004_020bc8b4, -1, -1, 0xd);
  Mg_PlayJingle((void*)3);
  mBehavior = pmf_dMgState_c_State03_Update;
}

// @symbol _ZN10dMgState_c14State03_UpdateEv
void dMgState_c::State03_Update() {
  Obj* obj;
  if (Mg_CheckBannerButton(0)) {
    obj = data_0209f5bc;
    obj->SetForwardTime(0x1e, 0);
    mBehavior = pmf_dMgState_c_UpdateRetryFadeOut;
    return;
  }
  if (Mg_CheckBannerButton(2) || Mg_CheckBannerButton(1)) {
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
  Mg_ShowBanner(0, data_ov004_020bc8a8, data_ov004_020bc898, -1, -1, 0xd);
  Mg_ShowBanner(2, data_ov004_020bc86c, data_ov004_020bc8a4, -1, -1, 0xd);
}

// @symbol _ZN10dMgState_c18UpdateRetryFadeOutEv
void dMgState_c::UpdateRetryFadeOut() {
  if (data_0209f5bc->IsAtEnd() == 0)
    return;
  void* scene = gMgScene;
  if (scene == 0)
    return;
  data_0209f5bc->SetBackwardTime(0x1e, 0);
  FreeGfxSlotsById(0x1d);
  Mg_RestoreMusic();
  Mg_StartMusic();
  Mg_CallStartRound(scene, (void *)mState);
  mTimer = 0;
  mState = -1;
  unk_020 = 0;
  unk_024 = 0;
}

// @symbol _ZN10dMgState_c13State04_EnterEv
void dMgState_c::State04_Enter() {
  int prevId, id;
  mTimer = 0x78;
  prevId = sMgNextBanner;
  id = 9;
  if (prevId != 0x1d) { sMgNextBanner = 0x1d; id = prevId; }
  Mg_ShowBanner(id, data_ov004_020bc88c, data_ov004_020bc860, -1, -1, 0xd);
  Sound::PlayBank2_2D(0x136);
  mBehavior = pmf_dMgState_c_State04_Update;
}

// @symbol _ZN10dMgState_c14State04_UpdateEv
void dMgState_c::State04_Update() {
  if (mTimer > 0x64)
    return;
  if (mTimer != 0)
    return;
  Base* scene = gMgScene;
  if (scene == 0)
    return;
  if (scene->CanEndState(mState) == 0)
    return;
  FreeGfxSlotsById(0x1d);
  Mg_RestoreMusic();
  Mg_StartMusic();
  Mg_CallStartRound(scene, (void *)mState);
  mTimer = 0;
  mState = -1;
  unk_020 = 0;
  unk_024 = 0;
}

// @symbol _ZN10dMgState_c13State05_EnterEv
void dMgState_c::State05_Enter() {
    mTimer = 0x78;
    Mg_ShowBanner(0xa, data_ov004_020bc878, data_ov004_020bc890, -1, -1, 0xd);
    Sound::PlayBank2_2D(0x137);
    mBehavior = pmf_dMgState_c_State05_Update;
}

// @symbol _ZN10dMgState_c14State05_UpdateEv
void dMgState_c::State05_Update() {
  int state = mTimer;
  if (state > 0x64)
    return;
  Base* p = gMgScene;
  int a8 = (p != 0) ? p->mCredits : 0;
  if (a8 == 0) {
    if (state != 0)
      return;
    Base* q = gMgScene;
    if (q == 0)
      return;
    if (q->CanEndState(mState) == 0)
      return;
    gMgCoinsShowDelta = 0;
    sMgCoinsDeltaShown = 0;
    Mg_RestoreMusic();
    FreeGfxSlotsById(0x1d);
    mTimer = 0xb4;
    mBehavior = pmf_dMgState_c_UpdateGameOverToMenu;
  } else {
    if (state != 0)
      return;
    Base* scene = gMgScene;
    if (scene == 0)
      return;
    if (scene->CanEndState(mState) == 0)
      return;
    FreeGfxSlotsById(0x1d);
    Mg_RestoreMusic();
    Mg_StartMusic();
    Mg_CallStartRound(scene, (void *)mState);
    mTimer = 0;
    mState = -1;
    unk_020 = 0;
    unk_024 = 0;
  }
}

// @symbol _ZN10dMgState_c20UpdateGameOverToMenuEv
void dMgState_c::UpdateGameOverToMenu() {
    Obj* obj;
    int v = mTimer;
    if (v == 0xb3) {
        Mg_ShowBanner(8, data_ov004_020bc8b8, data_ov004_020bc8b4, -1, -1, 0xd);
        Mg_PlayJingle((void*)3);
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

// @symbol _ZN10dMgState_c13State06_EnterEv
void dMgState_c::State06_Enter() {
    mTimer = 0x78;
    Mg_ShowBanner(0xa, data_ov004_020bc878, data_ov004_020bc890, -1, -1, 0xd);
    Sound::PlayBank2_2D(0x137);
    mBehavior = pmf_dMgState_c_State06_Update;
}

// @symbol _ZN10dMgState_c14State06_UpdateEv
void dMgState_c::State06_Update() {
  int v = mTimer;
  if(v > 0x64) return;
  if(v != 0) return;
  FreeGfxSlotsById(0x1d);
  mBehavior = pmf_dMgState_c_UpdateAfterHidden;
  unk_024 = 1;
  gMgCoinsShowDelta = 0;
  sMgCoinsDeltaShown = 0;
}

// @symbol _ZN10dMgState_c17UpdateAfterHiddenEv
void dMgState_c::UpdateAfterHidden() {
    Base *g = gMgScene;
    int cond = g ? g->mCredits : 0;
    if (cond == 0) {
        g = gMgScene;
        if (!g) return;
        if (!g->CanEndState(mState)) return;
        Mg_RestoreMusic();
        mTimer = 0xb4;
        mBehavior = pmf_dMgState_c_UpdateGameOverToMenu_2;
        return;
    }
    g = gMgScene;
    if (!g) return;
    if (!g->CanEndState(mState)) return;
    Mg_RestoreMusic();
    Mg_StartMusic();
    Mg_CallStartRound(g, (void *)mState);
    mTimer = 0;
    mState = -1;
    unk_020 = 0;
    unk_024 = 0;
}

// @symbol _ZN10dMgState_c13State07_EnterEv
void dMgState_c::State07_Enter() {
    mTimer = 0x78;
    Mg_ShowBanner(0xb, data_ov004_020bc868, data_ov004_020bc858, -1, -1, 0xd);
    mBehavior = pmf_dMgState_c_UpdateWaitThenResume;
}

// @symbol _ZN10dMgState_c13State08_EnterEv
void dMgState_c::State08_Enter() { mTimer = 0; mBehavior = pmf_dMgState_c_UpdateWaitThenResume_2; }

// @symbol _ZN10dMgState_c20UpdateWaitThenResumeEv
void dMgState_c::UpdateWaitThenResume() {
  Base* scene;
  if (mTimer != 0) return;
  scene = gMgScene;
  if (scene == 0) return;
  if (scene->CanEndState(mState) == 0) return;
  FreeGfxSlotsById(0x1d);
  Mg_CallStartRound(scene, (void *)mState);
  mTimer = 0;
  mState = -1;
  unk_020 = 0;
  unk_024 = 0;
}

// @symbol _ZN10dMgState_c13State09_EnterEv
void dMgState_c::State09_Enter() {
    int id;
    mTimer = 0xb4;
    Mg_PlayJingle((void*)4);
    id = 3;
    if (sMgNextBanner != 0x1d) { id = sMgNextBanner; sMgNextBanner = 0x1d; }
    Mg_ShowBanner(id, data_ov004_020bc8b0, data_ov004_020bc8ac - (sMgTopScreenYOffset + 0xc0), -1, -1, 0xa);
    mBehavior = pmf_dMgState_c_State09_Update;
}

// @symbol _ZN10dMgState_c14State09_UpdateEv
void dMgState_c::State09_Update() {
  if (mTimer != 0)
    return;
  Base* scene = gMgScene;
  if (scene == 0)
    return;
  if (scene->CanEndState(mState) == 0)
    return;
  FreeGfxSlotsById(0x1d);
  Mg_RestoreMusic();
  Mg_StartMusic();
  Mg_CallStartRound(scene, (void *)mState);
  mTimer = 0;
  mState = -1;
  unk_020 = 0;
  unk_024 = 0;
}

// @symbol _ZN10dMgState_c13State10_EnterEv
void dMgState_c::State10_Enter() {
  mTimer = 0xb4;
  Mg_PlayJingle((void*)3);
  Mg_ShowBanner(8, data_ov004_020bc894, data_ov004_020bc874, -1, -1, 0xb);
  mBehavior = pmf_dMgState_c_State10_Update;
}

// @symbol _ZN10dMgState_c14State10_UpdateEv
void dMgState_c::State10_Update() {
  Obj* obj;
  if (Mg_CheckBannerButton(0)) {
    obj = data_0209f5bc;
    obj->SetForwardTime(0x1e, 0);
    mBehavior = pmf_dMgState_c_UpdateRetryFadeOut_3;
    return;
  }
  if (Mg_CheckBannerButton(2) || Mg_CheckBannerButton(1)) {
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
  Mg_ShowBanner(0, data_ov004_020bc8a8, data_ov004_020bc898, -1, -1, 0xd);
  Mg_ShowBanner(1, data_ov004_020bc86c, data_ov004_020bc8a4, -1, -1, 0xd);
}

// @symbol _ZN10dMgState_c13State11_EnterEv
void dMgState_c::State11_Enter() {
  mTimer=0xb4;
  Mg_PlayJingle((void*)1);
  mBehavior = pmf_dMgState_c_State11_Update;
  mRender = pmf_dMgState_c_DrawScore;
}

// @symbol _ZN10dMgState_c14State11_UpdateEv
void dMgState_c::State11_Update() {
  if (data_0209f5bc->IsAtEnd()) {
    Base* scene = gMgScene;
    if (scene == 0)
      return;
    if (scene->CanEndState(mState) == 0)
      return;
    data_0209f5bc->SetBackwardTime(0x1e, 0);
    FreeGfxSlotsById(0x1d);
    Mg_RestoreMusic();
    Mg_StartMusic();
    Mg_CallStartRound(scene, (void *)mState);
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

// @symbol _ZN10dMgState_c13State12_EnterEv
void dMgState_c::State12_Enter() {
    State13_Enter();
}

// @symbol _ZN10dMgState_c13State13_EnterEv
void dMgState_c::State13_Enter() { mTimer = 10; mBehavior = pmf_dMgState_c_UpdateRetryPrompt; }

// @symbol _ZN10dMgState_c17UpdateRetryPromptEv
void dMgState_c::UpdateRetryPrompt() {
  if (mTimer == 0) {
    if (Mg_CheckBannerButton(0) != 0) {
      data_0209f5bc->SetForwardTime(0x1e, 0);
      mBehavior = pmf_dMgState_c_UpdateRetryFadeOut_2;
      return;
    }
    if (Mg_CheckBannerButton(2) == 0) {
      if (Mg_CheckBannerButton(1) == 0)
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
  Mg_ShowBanner(0, data_ov004_020bc8a8, data_ov004_020bc898, -1, -1, 0xd);
  Mg_ShowBanner(2, data_ov004_020bc86c, data_ov004_020bc8a4, -1, -1, 0xd);
  mTimer = 0;
}

// @symbol _ZN10dMgState_c13State14_EnterEv
void dMgState_c::State14_Enter() {
  mTimer = 0xb4;
  Mg_ShowBanner(5, data_ov004_020bc854, data_ov004_020bc87c, -1, -1, 0xd);
  Mg_PlayJingle((void*)1);
  mBehavior = pmf_dMgState_c_UpdateFadeThenResume;
  mRender = pmf_dMgState_c_DrawScoreDigits;
}

// @symbol _ZN10dMgState_c20UpdateFadeThenResumeEv
void dMgState_c::UpdateFadeThenResume() {
  if (data_0209f5bc->IsAtEnd()) {
    Base* scene = gMgScene;
    if (scene == 0)
      return;
    if (scene->CanEndState(mState) == 0)
      return;
    data_0209f5bc->SetBackwardTime(0x1e, 0);
    Mg_RestoreMusic();
    Mg_StartMusic();
    Mg_CallStartRound(scene, (void *)mState);
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

// @symbol _ZN10dMgState_c13State15_EnterEv
void dMgState_c::State15_Enter() {
  mTimer = 0x3c;
  Mg_ShowBanner(0xe, data_ov004_020bc8b8, data_ov004_020bc8b4, -1, -1, 0xd);
  Mg_PlayJingle((void*)3);
  mBehavior = pmf_dMgState_c_UpdateFadeThenResume_2;
}

// @symbol _ZN10dMgState_c13State16_EnterEv
void dMgState_c::State16_Enter() {
  if (Mg_IsNewRecord()) {
    State17_Enter();
    return;
  }
  Mg_ShowBanner(7, 0x80, 0x14, -1, -1, 0xd);
  mTimer = 0x78;
  mBehavior = pmf_dMgState_c_State16_Update;
  mRender = pmf_dMgState_c_DrawRecordTable;
  Mg_CommitRecords();
}

// @symbol _ZN10dMgState_c14State16_UpdateEv
void dMgState_c::State16_Update() {
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
  State12_Enter();
}

// @symbol _ZN10dMgState_c13State17_EnterEv
void dMgState_c::State17_Enter() {
  Mg_ShowBanner(7, 0x80, 0x14, -1, -1, 0xd);
  mTimer = 0x12c;
  mBehavior = pmf_dMgState_c_State17_Update;
  mRender = pmf_dMgState_c_DrawRecordTable_2;
  Mg_CommitRecords();
}

// @symbol _ZN10dMgState_c14State17_UpdateEv
void dMgState_c::State17_Update() {
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
            State12_Enter();
        }
    }

    if (mTimer != 0xb4)
        return;

    scene = gMgScene;
    if (scene) {
        scene->OnNewRecord();
    }

    id = 5;
    v = sMgNextBanner;
    if (v != 0x1d) {
        sMgNextBanner = 0x1d;
        id = v;
    }

    s = data_ov004_020bc864;
    if (s < 0)
        s -= Mg_GetTopScreenYOffset();

    Mg_ShowBanner(id, data_ov004_020bc888, s, -1, -1, 0xd);
    Mg_PlayJingle((void*)1);
}

// @symbol _ZN10dMgState_c13State18_EnterEv
void dMgState_c::State18_Enter() {
  mTimer = 0xb4;
  Mg_ShowBanner(8, data_ov004_020bc8b8, data_ov004_020bc8b4, -1, -1, 0xd);
  Mg_PlayJingle((void*)3);
  mBehavior = pmf_dMgState_c_State18_Update;
}

// @symbol _ZN10dMgState_c14State18_UpdateEv
void dMgState_c::State18_Update() {
    int x = mTimer;
    Base *g = gMgScene;
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
    if (g->CanEndState(mState) == 0)
        return;
    *(W2 *)&mRender = data_02086b58;
    FreeGfxSlotsById(0x1d);
    if (Mg_IsNewRecord() != 0) {
        State17_Enter();
        return;
    }
    State16_Enter();
}

// @symbol _ZN10dMgState_c13State19_EnterEv
void dMgState_c::State19_Enter() {
  int prevId, id;
  mTimer = 0xb4;
  prevId = sMgNextBanner;
  id = 3;
  if (prevId != 0x1d) { sMgNextBanner = 0x1d; id = prevId; }
  Mg_ShowBanner(id, data_ov004_020bc880, data_ov004_020bc884, -1, -1, 0xd);
  Mg_PlayJingle((void*)4);
  mBehavior = pmf_dMgState_c_State19_Update;
}

// @symbol _ZN10dMgState_c14State19_UpdateEv
void dMgState_c::State19_Update() {
    int x = mTimer;
    Base *g = gMgScene;
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
    if (g->CanEndState(mState) == 0)
        return;
    *(W2 *)&mRender = data_02086b58;
    FreeGfxSlotsById(0x1d);
    if (Mg_IsNewRecord() != 0) {
        State17_Enter();
        return;
    }
    State16_Enter();
}

// @symbol _ZN10dMgState_c9DrawScoreEv
void dMgState_c::DrawScore() {
    int r;
    if (gMgScene == 0) return;
    r = Mg_GetSubmittedScore();
    Mg_DrawBigNumber(data_ov004_020bc850, data_ov004_020bc89c, r, -1, -1, 0, 0);
}

// @symbol _ZN10dMgState_c15DrawScoreDigitsEv
void dMgState_c::DrawScoreDigits() {
    unsigned int r;
    int idx;

    if (gMgScene == 0) return;

    r = Mg_GetSubmittedScore();
    Mg_DrawBigNumber(data_ov004_020bc85c + 4, data_ov004_020bc870, r / 100, -1, -1, 1, 0);

    r = Mg_GetSubmittedScore();
    Mg_DrawBigNumber(data_ov004_020bc85c + 0x1c, data_ov004_020bc870, r % 100 / 10, -1, -1, 1, 0);

    r = Mg_GetSubmittedScore();
    Mg_DrawBigNumber(data_ov004_020bc85c + 0x2c, data_ov004_020bc870, r % 10, -1, -1, 1, 0);

    idx = GetGameLanguage();
    Mg_RenderSubSpriteMtx((void *)*(int *)(data_ov004_020bca44[idx] + 0xc), data_ov004_020bc85c - 0x24, data_ov004_020bc870, 0);

    idx = GetGameLanguage();
    Mg_RenderSubSpriteMtx((void *)*(int *)(data_ov004_020bca44[idx] + 0x1c), data_ov004_020bc85c + 0x10, data_ov004_020bc870, 0);
}

// @symbol _ZN10dMgState_c15DrawRecordTableEv
void dMgState_c::DrawRecordTable() {
    void *g = gMgScene;
    if (g == 0) return;
    Mg_DrawRecordTable(g);
    if (mTimer > 0) return;
    Mg_DrawBlinkSprite(g, 0xe0, 0xa0);
}

// @symbol Mg_PlaceBannersOnTopScreen
extern "C" void Mg_PlaceBannersOnTopScreen(void)
{
    int v1 = -0x1c - Mg_GetTopScreenYOffset();
    data_ov004_020bc88c = 0x80;
    data_ov004_020bc860 = v1;
    {
        int v2 = -0x1c - Mg_GetTopScreenYOffset();
        data_ov004_020bc878 = 0x80;
        data_ov004_020bc890 = v2;
    }
    {
        int v3 = -0x1c - Mg_GetTopScreenYOffset();
        data_ov004_020bc868 = 0x80;
        data_ov004_020bc858 = v3;
        data_ov004_020bc888 = 0x80;
        data_ov004_020bc864 = -0x1c;
        data_ov004_020bc8b8 = 0x80;
        data_ov004_020bc8b4 = 0x60;
    }
}

// @symbol Mg_ResetRecordBannerPos
extern "C" void Mg_ResetRecordBannerPos(void)
{
    data_ov004_020bc888 = 0x80;
    data_ov004_020bc864 = -0x1c;
}

// @symbol Mg_SetPerfectBanner
extern "C" void Mg_SetPerfectBanner(void) {
    Mg_SetNextBanner(6);
}

// @symbol Mg_SetNextBanner
extern "C" void Mg_SetNextBanner(int v) { sMgNextBanner = v; }
