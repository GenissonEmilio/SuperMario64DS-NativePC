#ifndef DSCMGPACHINKO2_C_H
#define DSCMGPACHINKO2_C_H
#include "dScMgBase_c.h"

/* The Tamaire minigame scene: scene 0x171, profile MG_TAMAIRE. RTTI derives
 * dScMgPachinko2_c directly from dScMgBase_c, and no RTTI record names it as
 * a base. It overrides vtable slots 0 (InitResources), 6 (Behavior),
 * 9 (Render), 16 and 17 (the destructors) and 18 (StartRound). Offsets
 * below 0x4660 belong to dScMgBase_c. Own offsets that only the unnamed
 * helper functions touch are still padding.
 *
 * The factory dScMgPachinko2_c_classInit (historical alias
 * MgLakituLaunch_Spawn) installs this class's vtable. The ROM proves the
 * class; the classInit spelling follows later EAD lineage.
 */
/* One launched ball. Ball_State1 drives it from the touch record while
   the pen is down (x/y follow the pen, px/py keep the pen offset) and, on
   release, aims it at (0x80, 0x20): angle = atan2, speed from the distance,
   vx/vy from the sine table. */
struct dScMgPachinko2_Ball {
    s32 x;            /* +0x00 -- Fix12 */
    s32 y;            /* +0x04 */
    s32 vx;           /* +0x08 */
    s32 vy;           /* +0x0c */
    s32 px;           /* +0x10 -- pen offset while held */
    s32 py;           /* +0x14 */
    u8  unk_18[0x8];
    s32 speed;        /* +0x20 */
    u8  unk_24[0x4];
    s32 sound;        /* +0x28 -- Sound_PlayIfNotActive handle */
    s32 prevDist;     /* +0x2c */
    u8  unk_30[0x2];
    s16 unk_32;       /* +0x32 */
    s16 angle;        /* +0x34 */
    s16 unk_36;       /* +0x36 */
    u8  unk_38;
    u8  state;        /* +0x39 -- 2 while released, 0 when reset */
    u8  unk_3a;
    u8  unk_3b;
    u8  unk_3c;
    u8  unk_3d;
    u8  unk_3e;
    u8  pad_3f;
};
#ifndef SM64DS_PLATFORM_PC
/* ROM layout under mwccarm; host ABI divergence is tracked separately. */
typedef char dScMgPachinko2_Ball_size_must_be_0x40[sizeof(struct dScMgPachinko2_Ball) == 0x40 ? 1 : -1];
#endif

/* One of the three paddles. State 0..14 dispatches through
   data_ov006_02142734; Paddle_Hit parks the current motion in
   home/saved* and later states restore it. */
struct dScMgPachinko2_Paddle {
    s32 x;            /* +0x00 -- Fix12 */
    s32 y;            /* +0x04 */
    s32 vx;           /* +0x08 */
    s32 vy;           /* +0x0c */
    s32 speed;        /* +0x10 */
    u8  unk_14[0x8];
    s32 homeX;        /* +0x1c */
    s32 homeY;        /* +0x20 */
    s32 savedVx;      /* +0x24 */
    s32 savedVy;      /* +0x28 */
    s32 sound;        /* +0x2c -- Sound_PlayIfNotActive handle */
    s16 angle;        /* +0x30 */
    u16 timer;        /* +0x32 */
    u8  active;       /* +0x34 */
    u8  visible;      /* +0x35 */
    u8  state;        /* +0x36 */
    u8  savedState;   /* +0x37 */
    u8  dir;          /* +0x38 */
    u8  ballsHeld;    /* +0x39 -- balls caught, dropped back one at a time */
    u8  swing;        /* +0x3a */
    u8  unk_3b;
    u8  hits;         /* +0x3c -- four parks the paddle */
    u8  flip;         /* +0x3d */
    u8  pad_3e[0x2];
};

/* The flash at a paddle catch: four frames from data_ov006_02136bd4, eight
   ticks each (Spark_UpdateAll). */
struct dScMgPachinko2_HitFx {
    s32 x;            /* +0x00 */
    s32 y;            /* +0x04 */
    s32 unk_08;
    s32 unk_0c;
    u16 timer;        /* +0x10 */
    u8  pad_12[0x2];
    u8  visible;      /* +0x14 */
    u8  frame;        /* +0x15 */
    u8  active;       /* +0x16 */
    u8  pad_17;
};

/* The popup a scoring ball throws up: rises, falls, and expires after
   0x40 ticks (Popup_UpdateAll). */
struct dScMgPachinko2_ScorePop {
    s32 x;            /* +0x00 */
    s32 y;            /* +0x04 */
    s32 vy;           /* +0x08 */
    s32 unk_0c;
    u16 life;         /* +0x10 */
    u8  pad_12[0x2];
    u8  active;       /* +0x14 */
    u8  visible;      /* +0x15 */
    u8  sprite;       /* +0x16 */
    u8  falling;      /* +0x17 */
};

/* One of the two cups. State 0..4 dispatches through data_ov006_021426cc. */
struct dScMgPachinko2_Cup {
    s32 x;            /* +0x00 -- Fix12 */
    s32 y;            /* +0x04 */
    s32 vx;           /* +0x08 */
    s32 unk_0c;
    u16 timer;        /* +0x10 */
    u8  pad_12[0x2];
    u8  active;       /* +0x14 */
    u8  state;        /* +0x15 */
    u8  phase;        /* +0x16 */
    u8  frame;        /* +0x17 -- sprite index into data_ov006_02136b80 */
    u8  animStep;     /* +0x18 */
    u8  visible;      /* +0x19 */
    u8  dir;          /* +0x1a */
    u8  armed;        /* +0x1b */
    u8  hops;         /* +0x1c */
    u8  lastMove;     /* +0x1d -- 0xff before the first */
    u8  repeats;      /* +0x1e */
    u8  pad_1f;
};

#ifndef SM64DS_PLATFORM_PC
typedef char dScMgPachinko2_Paddle_size_must_be_0x40[sizeof(struct dScMgPachinko2_Paddle) == 0x40 ? 1 : -1];
typedef char dScMgPachinko2_HitFx_size_must_be_0x18[sizeof(struct dScMgPachinko2_HitFx) == 0x18 ? 1 : -1];
typedef char dScMgPachinko2_ScorePop_size_must_be_0x18[sizeof(struct dScMgPachinko2_ScorePop) == 0x18 ? 1 : -1];
typedef char dScMgPachinko2_Cup_size_must_be_0x20[sizeof(struct dScMgPachinko2_Cup) == 0x20 ? 1 : -1];
#endif

struct dScMgPachinko2_c : dScMgBase_c {
    /* The destructor pair spelled as two plain virtuals on the host, plus
       the non-virtual destructor declaration the src/ definitions need; the
       whole ruling is in include/ModelBase.h. An override takes its base's
       slots, so these carry the SAME TWO NAMES the base declares -- a fresh
       name would append a slot instead of claiming one. */
#ifdef _MSC_VER
    virtual void Destructor1();   /* D1 */
    virtual void Destructor0();   /* D0 */
    ~dScMgPachinko2_c();   /* no slot */
#else
    virtual ~dScMgPachinko2_c();  /* slots 16 and 17 */
#endif
    virtual s32 InitResources();  /* slot 0 */
    virtual s32 Behavior();       /* slot 6 */
    virtual s32 Render();         /* slot 9 */
    virtual void StartRound(int arg); /* slot 18 */

    /* cups: the two 0x20-byte drop targets at 0x5620 (the mBalls[63] slot) */
    void CupSlot_DrawAll();            /* render both cups */
    void CupSlot_ResetAll();            /* reset cup slots */
    void Cup_PickTarget(int k);       /* launched-ball vs cup k check */
    void Cup_State4(int k);       /* cup state 4: randomize */
    void Cup_State3(int k);       /* cup state 3 */
    void Cup_State2(int k);       /* cup state 2 */
    void Cup_State1(int k);       /* cup state 1 */
    void Cup_State0(int k);       /* cup state 0: idle reset */
    void Cup_DrawAll();            /* cup PMF dispatch, data_ov006_021426cc */
    void Cup_ShowAll();            /* init both cups */
    void Cup_HideAll();            /* clear cup flags */

    /* scene state */
    void ChooseLaneConfig();            /* pick lane config -> unk_5668 */
    void DrawScore();            /* round-end jingle gate */
    void DrawTopRecord();            /* round-end sfx gate */
    void StartRoundEnd(int a);       /* enter state 3, fade */
    void CheckRoundOver();            /* all-paddles-done check -> state 3 */
    void Paddle_SpawnForLanes();            /* arm paddles for the round */
    void CheckCountdownEnd();            /* enter state 2 */
    void DrawCountdown();            /* countdown tick gate */
    void UpdateCountdown();            /* countdown ticker */
    void ClearAimLine();            /* cancel launch if ball left pen */
    void PlotSubBg2Pixels(int a1, int a2, int a3); /* BG2 tile stamp (a0 unused) */
    void DrawAimLine();            /* draw the guide lines */
    void StartCountdown();            /* arm 30-tick countdown */
    void UpdateRoundEnd();            /* launch queued ball */
    void ResetRound();            /* reset the whole round */

    /* sprite bursts: the two 16 x 0x18 records at 0x5320 and 0x54a0 */
    void Popup_DrawAll();            /* render records at 0x54a0 */
    void Popup_UpdateAll();            /* step records at 0x54a0 */
    void Popup_Spawn(int r1, int r2, int r3); /* spawn at 0x54a0 */
    void Spark_DrawAll();            /* render records at 0x5320 */
    void Spark_UpdateAll();            /* step records at 0x5320 */
    void Spark_Spawn(int a2, int a3);         /* spawn at 0x5320 */

    /* paddles: the three 0x40-byte records at 0x5260 */
    void Paddle_DrawAll();            /* render paddles */
    void Paddle_HideAll();            /* reset paddle timers */
    void Paddle_Stop(int idx);     /* park paddle idx */
    void Paddle_CheckBallHits(int idx);     /* paddle idx vs balls */
    void Paddle_UpdateAll();            /* paddle PMF dispatch, data_ov006_02142734 */
    void Paddle_ClearAll();            /* clear paddle flags */
    void Paddle_State14(int i);       /* paddle state 14 */
    void Paddle_State13(int i);       /* paddle state 13 */
    void Paddle_State12(int i);       /* paddle state 12: seek lane target */
    void Paddle_State11(int i);       /* paddle state 11 */
    void Paddle_State10(int i);       /* paddle state 10 */
    void Paddle_State9(int i);       /* paddle state 9 */
    void Paddle_State8(int i);       /* paddle state 8 */
    void Paddle_Hit(int i);       /* collect -> next state */
    void Paddle_State7(int i);       /* paddle state 7 */
    void Paddle_State6(int i);       /* paddle state 6: horizontal patrol */
    void Paddle_State5(int i);       /* paddle state 5 */
    void Paddle_State4(int i);       /* paddle state 4: circle lane */
    void Paddle_State3(int i);       /* paddle state 3 */
    void Paddle_State2(int i);       /* paddle state 2 */
    void Paddle_State1(int i);       /* paddle state 1: rise then split */
    void Paddle_State0(int i);       /* paddle state 0: enter */

    /* balls: mBalls[0x30] active */
    void Ball_HitAt(int x, int z, int d); /* spawn ball */
    void Ball_Bounce(int i);       /* ball i hits the floor */
    void Ball_ClearAll();            /* clear ball flags */
    void Ball_DrawAll();            /* render balls */
    void Ball_UpdateAll();            /* ball PMF dispatch, data_ov006_021426f4 */
    void Ball_State0(int arg1);    /* ball state 0: idle in tray */
    void Ball_State1(int i);       /* ball state 1: pen drag */
    void Ball_State2(int i);       /* ball state 2: launched flight */
    void Ball_State3(int i);       /* ball state 3 */
    void Ball_State4(int i);       /* ball state 4 */
    void Ball_State5(int i);       /* ball state 5 */
    void Ball_State6(int i);       /* ball state 6: despawn */
    void Ball_State7(int i);       /* ball state 7 */

    dScMgPachinko2_Ball     mBalls[0x30];      /* 0x4660 */
    dScMgPachinko2_Paddle   mPaddles[3];       /* 0x5260 */
    dScMgPachinko2_HitFx    mHitFx[0x10];      /* 0x5320 */
    dScMgPachinko2_ScorePop mScorePops[0x10];  /* 0x54a0 */
    dScMgPachinko2_Cup      mCups[2];          /* 0x5620 */
    s32 unk_5660;            /* 0x5660 -- Behavior switches on it, 0..3 */
    s32 mGuideStep;          /* 0x5664 -- DrawAimLine */
    s32 mLaneConfig;         /* 0x5668 -- 0..5, ChooseLaneConfig */
    u16 unk_566c;            /* 0x566c */
    u16 unk_566e;            /* 0x566e -- set to 0x40; Behavior's case 3 counts it down */
    u16 unk_5670;            /* 0x5670 */
    u16 unk_5672;            /* 0x5672 */
    u16 mCountdown;          /* 0x5674 -- seconds; UpdateCountdown */
    u8  mHeldBall;           /* 0x5676 -- 1 + the ball in the pen, 0 for none */
    u8  mResult;             /* 0x5677 */
    u8  mCountdownArmed;     /* 0x5678 */
    u8  mCountdownTick;      /* 0x5679 -- frames into the current second */
    u8  mCountdownShown;     /* 0x567a */
    /* the factory allocates 0x567c bytes; see tools/opnew_sizes.py */
    u8  pad_567b;
};

#ifndef SM64DS_PLATFORM_PC
/* ROM layout under mwccarm; host ABI divergence is tracked separately. */
typedef char dScMgPachinko2_c_size_must_be_0x567c[sizeof(struct dScMgPachinko2_c) == 0x567c ? 1 : -1];
#endif

#endif
