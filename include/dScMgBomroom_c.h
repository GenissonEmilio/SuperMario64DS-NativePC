#ifndef DSCMGBOMROOM_C_H
#define DSCMGBOMROOM_C_H
#include "dScMgBase_c.h"

/* Bob-omb sorting minigame (dScMgBomroom_c : dScMgBase_c). Fields below
 * 0x4660 belong to the base and stay raw; the bombs themselves start there.
 *
 * The name is the ROM's RTTI name, and rtti_extract.py confirms it is a
 * leaf (no RTTI record names it as a base). dScMgBomroom_c_classInit (alias
 * MgSortOrSplode_Spawn) installs its vtable for the MG_BOMROOM profile.
 */
/* One bomb: 0x70 records at +0x4660, stride 0x40. grabX/grabY are the
   stylus offset at the grab; speed is the Fix12 step along angle. */
struct dScMgBomroom_Bomb {
    s32 x;            /* +0x00 -- Fix12 */
    s32 y;            /* +0x04 -- Fix12 */
    s32 grabX;        /* +0x08 -- stylus x minus bomb x, Fix12 */
    s32 grabY;        /* +0x0c -- stylus y minus bomb y, Fix12 */
    s32 speed;        /* +0x10 -- Fix12 per-frame step; scaled by sin/cos of
                                   angle while roaming, added directly when
                                   walking to its slot */
    s32 unk_14;       /* +0x14 */
    s32 unk_18;       /* +0x18 */
    s32 prevX;        /* +0x1c -- position before this frame's move */
    s32 prevY;        /* +0x20 */
    s32 sound;        /* +0x24 -- sound handle; func_02012468 takes it back
                                   and returns it each frame */
    s32 unk_28;       /* +0x28 -- set to the spawn step base, see func_ov006_020d8408 */
    u16 angle;        /* +0x2c -- 0..0xffff, sine-table index >> 4 */
    u16 counter;      /* +0x2e -- frame counter, see func_ov006_020d69b8 */
    u16 unk_30;       /* +0x30 */
    u16 unk_32;       /* +0x32 -- second per-bomb timer; counts down on a held bomb */
    u8  type;         /* +0x34 -- anim row; picks frame time/count and sfx */
    u8  frame;        /* +0x35 */
    u8  color;        /* +0x36 -- 0/1; also picks which pen box it bounces in */
    u8  state;        /* +0x37 -- 5 = settled; indexes data_ov006_02141730 */
    u8  active;       /* +0x38 */
    u8  unk_39;       /* +0x39 -- sprite-drawn flag, see func_ov006_020d7524 */
    u8  pen;          /* +0x3a -- which blast marked it; 0 = was loose */
    u8  substate;     /* +0x3b -- indexes data_ov006_02141708 */
    u8  unk_3c;       /* +0x3c */
    u8  unk_3d;       /* +0x3d */
    u8  unk_3e;       /* +0x3e */
    u8  pad_3f;
};

/* A 0x10-byte per-slot record: the scene keeps three runs of them between
   +0x6260 and +0x62cf, and +0x0c/+0x0d are always the PMF-table index and
   the dispatch gate. */
struct dScMgBomroom_Slot {
    s32 x;            /* +0x00 -- Fix12 */
    s32 y;            /* +0x04 -- Fix12 */
    u16 timer;        /* +0x08 -- frame counter, see func_ov006_020d5d08/020d61dc */
    u8  unk_0a[0x2];
    u8  state;        /* +0x0c -- PMF-table index (markers: animating flag) */
    u8  gate;         /* +0x0d -- dispatch gate (markers: drawn/active) */
    u8  idx;          /* +0x0e -- sprite/sub index */
    u8  level;        /* +0x0f -- see func_ov006_020d61dc */
};
#ifndef SM64DS_PLATFORM_PC
/* ROM layout under mwccarm; host ABI divergence is tracked separately. */
typedef char dScMgBomroom_Bomb_size_must_be_0x40[sizeof(struct dScMgBomroom_Bomb) == 0x40 ? 1 : -1];
typedef char dScMgBomroom_Slot_size_must_be_0x10[sizeof(struct dScMgBomroom_Slot) == 0x10 ? 1 : -1];
#endif

struct dScMgBomroom_c : dScMgBase_c {
    /* The destructor pair spelled as two plain virtuals on the host, plus
       the non-virtual destructor declaration the src/ definitions need; the
       whole ruling is in include/ModelBase.h. An override takes its base's
       slots, so these carry the SAME TWO NAMES the base declares -- a fresh
       name would append a slot instead of claiming one. */
#ifdef _MSC_VER
    virtual void Destructor1();   /* D1 */
    virtual void Destructor0();   /* D0 */
    ~dScMgBomroom_c();   /* no slot */
#else
    virtual ~dScMgBomroom_c();
#endif
    virtual s32 InitResources();  /* slot 0 */
    virtual s32 Behavior();       /* slot 6 */
    virtual s32 Render();         /* slot 9 */
    virtual void OnYoshiTryEat(int arg);               /* slot 18 */

    /* scene helpers -- the PMF targets of the seven sinit state tables
       (data_ov006_02141660/680/6c0/6e0/708/730 and the raw {fptr,adj} pairs at
       data_ov006_021416a0) plus the per-frame and round bookkeeping they call. */
    void func_ov006_020d5ab0();
    void func_ov006_020d5b00();
    void func_ov006_020d5b10();
    void func_ov006_020d5c60();
    void func_ov006_020d5c88();
    void func_ov006_020d5d08();
    void func_ov006_020d5d90(int idx);
    void func_ov006_020d5dd4();
    void func_ov006_020d5e1c();
    void func_ov006_020d5e3c();
    void func_ov006_020d5e5c();
    void func_ov006_020d5eb8(int index);
    void func_ov006_020d5f28();
    void func_ov006_020d5f2c(int index);
    void func_ov006_020d5fd8(int index);
    void func_ov006_020d5fec();
    void func_ov006_020d604c();
    void func_ov006_020d6084();
    void func_ov006_020d6098();
    void func_ov006_020d6100(int index);
    void func_ov006_020d6170(int index);
    void func_ov006_020d61dc(int index);
    void func_ov006_020d6264(int index);
    void func_ov006_020d6278();
    void func_ov006_020d62e0();
    void func_ov006_020d634c(int index);
    void func_ov006_020d63ac();
    void func_ov006_020d63d4();
    void func_ov006_020d6454(int index);
    void func_ov006_020d64c4();
    void func_ov006_020d64c8(int index);
    void func_ov006_020d65b4(int index);
    void func_ov006_020d65c8();
    void func_ov006_020d6630();
    void func_ov006_020d669c();
    void func_ov006_020d66c4(int index);
    void func_ov006_020d672c();
    void func_ov006_020d6784();
    void func_ov006_020d68a8(int picked);
    void func_ov006_020d69b8(int index);
    void func_ov006_020d6b88(int index);
    void func_ov006_020d6c90(int index);
    void func_ov006_020d6d7c(int index);
    void func_ov006_020d6e8c(int index);
    void func_ov006_020d7524();
    void func_ov006_020d7604();
    void func_ov006_020d7778();
    void func_ov006_020d777c(int index);
    void func_ov006_020d7958();
    void func_ov006_020d795c(int index);
    void func_ov006_020d7a84(int index);
    void func_ov006_020d7c00(int index);
    void func_ov006_020d7c4c(int idx);
    void func_ov006_020d7e7c(int i);
    void func_ov006_020d7edc(int idx);
    void func_ov006_020d7f5c(int idx);
    void func_ov006_020d816c(int idx);
    void func_ov006_020d8324(int i);
    void func_ov006_020d836c();
    void func_ov006_020d8408();
    void func_ov006_020d8904();
    void func_ov006_020d893c();
    void func_ov006_020d89c4();
    void func_ov006_020d8af8();
    int func_ov006_020d8c88();
    void func_ov006_020d8cc4();
    void func_ov006_020d8d84();
    void func_ov006_020d8f34();
    void func_ov006_020d8f98();
    void func_ov006_020d8ff4();
    void func_ov006_020d9020();
    void func_ov006_020d904c();
    void func_ov006_020d907c();

    dScMgBomroom_Bomb mBombs[0x70]; /* 0x4660 -- 0x70 x 0x40 */
    /* three runs of 0x10-byte slot records; +0x0c/+0x0d are each run's
       PMF-table index and dispatch gate:
         mSlotsA    @0x6260 -- data_ov006_02141680 (func_ov006_020d65c8)
         mSlotsB    @0x6280 -- data_ov006_021416c0 (func_ov006_020d6278);
                             [2] is the scene-level slot, dispatched through
                             data_ov006_02141660 by func_ov006_020d5fec
         mMarkers   @0x62b0 -- the two HUD sprite markers */
    dScMgBomroom_Slot mSlotsA[2];  /* 0x6260 */
    dScMgBomroom_Slot mSlotsB[3];  /* 0x6280 */
    dScMgBomroom_Slot mMarkers[2]; /* 0x62b0 */
    s32 mSceneState;         /* 0x62d0 -- round state; 3 once a picked bomb has gone off */
    s32 mSubState;           /* 0x62d4 -- index into the data_ov006_021416a0 {fptr,adj} pairs */
    s32 mSpawnCount;         /* 0x62d8 -- bombs dropped so far; picks the spawn step */
    s32 mBg2Y;               /* 0x62dc -- BG2 scroll offset, Fix12 */
    u16 unk_62e0;            /* 0x62e0 -- second countdown run after mStateTimer */
    u16 mSpawnDelay;         /* 0x62e2 -- frames until the next spawn step */
    u16 unk_62e4;            /* 0x62e4 */
    u16 unk_62e6;            /* 0x62e6 */
    u16 mStateTimer;         /* 0x62e8 -- per-state countdown; 0x60 after the first blast */
    u16 unk_62ea;            /* 0x62ea -- mod-4 tick counter */
    u8  pad_62ec[0x2];       /* 0x62ec */
    u16 unk_62ee;            /* 0x62ee */
    u16 unk_62f0;            /* 0x62f0 -- delay waited out before state dispatch */
    u16 mBg2Hold;            /* 0x62f2 -- frames BG2 stays raised (state 2) */
    u8  mBg2State;           /* 0x62f4 -- BG2: 0 idle, 1 raising, 2 holding/lowering */
    u8  mWinColor;           /* 0x62f5 -- color that filled its pen */
    u8  mHeldColor;          /* 0x62f6 -- held bomb's color, 0xff = nothing held */
    u8  unk_62f7;            /* 0x62f7 */
    u8  mBlastPen;           /* 0x62f8 -- first blast's pen + 1; 0 = none yet */
    u8  unk_62f9;            /* 0x62f9 */
    u8  mRoundOver;          /* 0x62fa -- gates the round-over sprite */
    u8  unk_62fb;            /* 0x62fb -- freezes the bombs' +0x30 timers while set */
    u8  unk_62fc[0x4];       /* 0x62fc -- trailing bytes, handed out as a small buffer */
};

#ifndef SM64DS_PLATFORM_PC
/* ROM layout under mwccarm; host ABI divergence is tracked separately. */
typedef char dScMgBomroom_c_size_must_be_0x6300[sizeof(struct dScMgBomroom_c) == 0x6300 ? 1 : -1];
#endif

#endif
