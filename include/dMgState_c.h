#ifndef DMGSTATE_C_H
#define DMGSTATE_C_H

#include "types.h"

/* The 20 shared states Mg_SetSharedState enters (dMgState_c::SetState).
 * Names describe each enter handler's mechanics, which are certain: which
 * banner and jingle it shows and what runs after it. The on-screen meaning
 * of most is unobserved (notes/minigames/names.md, "the shared banner
 * states"; notes/minigames/runtime-checks.md). Confidence in the meaning
 * word: [high] where the mechanics leave no doubt, [medium] from who
 * enters it, [low] where no caller was found. */
enum MgSharedState {
    MG_STATE_ROUND_CLEAR = 0,          /* [medium] banner 3, jingle 4; wins in Cup, Amida, ... */
    MG_STATE_NEXT_ROUND = 1,           /* [high, seen] silent fade, then StartRound(1); Wanted! after every 5th level */
    MG_STATE_ROUND_BANNER5 = 2,        /* [low] banner 5, jingle 1; no caller found */
    MG_STATE_RETRY_PROMPT = 3,         /* [high, seen] PLAY AGAIN / QUIT under the banner ("TIME IS UP" in Snowball) */
    MG_STATE_WIN = 4,                  /* [medium] banner 9; wins in the card games */
    MG_STATE_LOSE = 5,                 /* [high] game over at 0 credits, else StartRound(5) */
    MG_STATE_LOSE_RESUME = 6,          /* [low] banner 0xa, then hands back to the game */
    MG_STATE_DRAW = 7,                 /* [low] banner 0xb; Card only */
    MG_STATE_TIE = 8,                  /* [medium] silent, StartRound(8) at once; Roulette's tie */
    MG_STATE_ROUND_CLEAR_TOP = 9,      /* [low] state 0 on the top screen; no caller found */
    MG_STATE_RETRY_PROMPT_B = 10,      /* [low] retry / quit with banner style 0xb */
    MG_STATE_SCORE_THEN_ROUND = 11,    /* [low] draws the score, then StartRound(11) */
    MG_STATE_RETRY_PROMPT_WIDE = 12,   /* [high, seen] PLAY AGAIN / QUIT side by side; Flower */
    MG_STATE_RETRY_PROMPT_WIDE_B = 13, /* [low] as 12 */
    MG_STATE_DIGITS_THEN_ROUND = 14,   /* [low] draws score digits, then StartRound(14) */
    MG_STATE_BANNER_E_THEN_ROUND = 15, /* [low] banner 0xe, jingle 3 */
    MG_STATE_RESULTS = 16,             /* [high, seen] banner 7 "HIGH SCORE" over the record table */
    MG_STATE_RESULTS_NEW_RECORD = 17,  /* [high, seen] 16 plus OnNewRecord, banner 5, jingle 1 */
    MG_STATE_GAME_OVER = 18,           /* [high, seen] banner 8 "GAME OVER", jingle 3, then 16 or 17 */
    MG_STATE_FINISHED = 19             /* [medium] banner 3, then 16 or 17; Snowball */
};

/* The shared minigame state machine, embedded in dScMgBase_c at 0xcc. It
 * has no vtable and the ROM has no RTTI for it, so the class name and its
 * member names are coined. The matched methods fix the layout: SetState
 * takes mEnter from a 20-entry table and calls it, Behavior counts mTimer
 * toward 0 and calls mBehavior, Render calls mRender, and mState -1 (the
 * constructor's value) makes Behavior and Render do nothing. */
#ifdef _MSC_VER
#pragma pack(push, 4) /* host only: MSVC aligns the pointer-to-member fields to 8, mwccarm to 4 (ROM size 0x28 at 4-byte alignment) */
#endif
struct dMgState_c {
    typedef void (dMgState_c::*Callback)();

    Callback mEnter;
    Callback mBehavior;
    Callback mRender;
    s32 mState;
    s32 mTimer;
    s32 unk_020;
    s32 unk_024; /* dScMgBase_c steps and draws the machine only while this is 0 */

    dMgState_c();
    void Render();
    void Behavior();
    void SetState(s32 state);

    /* State handlers: pointer-to-member constants at 0x020bc8bc..0x020bca3c
       (the enter/behavior/render records) name these as targets. */
    void DrawRecordTable();
    void DrawScoreDigits();
    void DrawScore();
    void State19_Update();
    void State19_Enter();
    void State18_Update();
    void State18_Enter();
    void State17_Update();
    void State17_Enter();
    void State16_Update();
    void State16_Enter();
    void State15_Enter();
    void UpdateFadeThenResume();
    void State14_Enter();
    void UpdateRetryPrompt();
    void State13_Enter();
    void State12_Enter();
    void State11_Update();
    void State11_Enter();
    void State10_Update();
    void State10_Enter();
    void State09_Update();
    void State09_Enter();
    void UpdateWaitThenResume();
    void State08_Enter();
    void State07_Enter();
    void UpdateAfterHidden();
    void State06_Update();
    void State06_Enter();
    void UpdateGameOverToMenu();
    void State05_Update();
    void State05_Enter();
    void State04_Update();
    void State04_Enter();
    void UpdateRetryFadeOut();
    void State03_Update();
    void State03_Enter();
    void State02_Update();
    void State02_Enter();
    void State01_Update();
    void State01_Enter();
    void State00_Update();
    void State00_Enter();
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifndef SM64DS_PLATFORM_PC
/* ROM layout under mwccarm; host ABI divergence is tracked separately. */
typedef char dMgState_c_size_must_be_0x28[
    sizeof(dMgState_c) == 0x28 ? 1 : -1];
#endif

#endif
