//cpp
/* unit020b4aa4 -- ov004 unit 0x020b4aa4..0x020b67e8: 43 functions, .text only.
 *
 * One translation unit of the shared minigame overlay, between unit020b2c84
 * and dMgState_c. No ROM RTTI or vtable falls in the range, so no class name
 * is claimed and the file keeps the unit<addr> name.
 *
 * What the code does. data_ov004_020bfa34 is an array of twenty 0x24-byte
 * Sprite records. Each one runs its own member-pointer state every frame
 * (func_ov004_020b65e4 through func_ov004_020b52fc) and, while visible, is
 * drawn at its position by func_ov004_020b6430. The Sprite states slide a
 * sprite from its position to a target by a fixed step, then hand it the
 * next state; the records at data_ov004_020bc7d8..0x020bc820 hold those
 * states, and the comment on each extern names its target. Over the array
 * runs a second machine, one plain function pointer at data_ov004_020bfa20
 * that func_ov004_020b65e4 calls once a frame and that each step replaces
 * (or clears) when it is done. The external entry points that start a
 * sequence are func_ov004_020b66d4 (reset), func_ov004_020b56c8,
 * func_ov004_020b5ed0, func_ov004_020b5e40, func_ov004_020b5dd4,
 * func_ov004_020b5d74, func_ov004_020b6324 and func_ov004_020b63a0.
 * func_ov004_020b6234 is internal: its one caller is the touch check in
 * func_ov004_020b65e4.
 *
 * Edges: 0x020b4aa4 is the first function after func_ov004_020b4aa0, the
 * last destructor helper __sinit_ov004_020b955c registers for the previous
 * unit. 0x020b67e8 is the end of func_ov004_020b67e4, the last helper
 * __sinit_ov004_020b9ad0 registers for this one (with func_ov004_020b67bc,
 * which runs __cxa_vec_cleanup over the array, and the empty per-element
 * destructor func_ov004_020b67e0); the next function reads the .data block
 * that starts at 0x020bc850, outside this unit's .data window
 * (0x020bc7d0..0x020bc83c).
 *
 * Emission order: mwccarm 2004/b56 emits one .text section per function in
 * the reverse of source order, so the highest-address function is written
 * first here and the functions come out in ROM order.
 *
 * The functions keep their ROM names under C linkage.
 *
 * Retired one-function sources (ROM address order):
 *   0x020b4aa4  func_ov004_020b4aa4
 *   0x020b4b84  func_ov004_020b4b84
 *   0x020b4c30  func_ov004_020b4c30
 *   0x020b4cc4  func_ov004_020b4cc4
 *   0x020b4d50  func_ov004_020b4d50
 *   0x020b4dfc  func_ov004_020b4dfc
 *   0x020b4e78  func_ov004_020b4e78
 *   0x020b4f44  func_ov004_020b4f44
 *   0x020b4ff0  func_ov004_020b4ff0
 *   0x020b506c  func_ov004_020b506c
 *   0x020b5108  func_ov004_020b5108
 *   0x020b51e4  func_ov004_020b51e4
 *   0x020b51f0  func_ov004_020b51f0
 *   0x020b5288  func_ov004_020b5288
 *   0x020b52b8  func_ov004_020b52b8
 *   0x020b52fc  func_ov004_020b52fc
 *   0x020b5334  func_ov004_020b5334
 *   0x020b5368  func_ov004_020b5368
 *   0x020b53f0  func_ov004_020b53f0
 *   0x020b556c  func_ov004_020b556c
 *   0x020b56c8  func_ov004_020b56c8
 *   0x020b5768  func_ov004_020b5768
 *   0x020b586c  func_ov004_020b586c
 *   0x020b58c4  func_ov004_020b58c4
 *   0x020b5a54  func_ov004_020b5a54
 *   0x020b5aac  func_ov004_020b5aac
 *   0x020b5c18  func_ov004_020b5c18
 *   0x020b5d74  func_ov004_020b5d74
 *   0x020b5dd4  func_ov004_020b5dd4
 *   0x020b5e40  func_ov004_020b5e40
 *   0x020b5ed0  func_ov004_020b5ed0
 *   0x020b5f6c  func_ov004_020b5f6c
 *   0x020b612c  func_ov004_020b612c
 *   0x020b6234  func_ov004_020b6234
 *   0x020b6324  func_ov004_020b6324
 *   0x020b63a0  func_ov004_020b63a0
 *   0x020b6430  func_ov004_020b6430
 *   0x020b653c  func_ov004_020b653c
 *   0x020b65e4  func_ov004_020b65e4
 *   0x020b66d4  func_ov004_020b66d4
 *   0x020b67bc  func_ov004_020b67bc
 *   0x020b67e0  func_ov004_020b67e0
 *   0x020b67e4  func_ov004_020b67e4
 */

#include "types.h"
#include "PlayerInput.h"

struct Vec2i { int x, y; };

/* One of the twenty records at data_ov004_020bfa34. Positions are Fix12
   screen pixels (func_ov004_020b52b8 draws at mPos >> 12). */
struct Sprite;
typedef void (Sprite::*SpriteFn)();
struct Sprite {
    SpriteFn mState;   /* 0x00 run every frame by func_ov004_020b52fc */
    Vec2i mPos;        /* 0x08 */
    Vec2i mTarget;     /* 0x10 */
    Vec2i mStep;       /* 0x18 per-frame step toward mTarget, both positive */
    s16 mDelay;        /* 0x20 frames to wait; also the rise counter */
    u8 mVisible;       /* 0x22 drawn while nonzero */
};

#ifndef SM64DS_PLATFORM_PC
typedef char Sprite_size_must_be_0x24[sizeof(Sprite) == 0x24 ? 1 : -1];
#endif

namespace Sound { void PlayBank2_2D(unsigned int); }
int ApproachLinear(int&, int, int);
int ApproachLinear2(short&, short, short);

extern "C" {
extern void Vec2_Sub(void* o, void* a, void* b);
extern void func_0203d680(void* out, void* in, int scale);
extern int __aeabi_idiv(int a, int b);
extern void __cxa_vec_cleanup(void *, unsigned int, unsigned int, void (*)(void *));
extern unsigned int func_02012790(unsigned int a);
extern int GetGameLanguage(void);
extern void Hud_RenderSprite(void* a0, int a1, int a2, int a3, int a4);

extern void func_ov004_020b1b40(void* c);
extern void func_ov004_020b1b08(void* x);
extern void func_ov004_020b1de8(int a, int b, int c, int d);
extern int func_ov004_020b04c0(void);
extern int func_ov004_020ae1a8(void);
extern void func_ov004_020adb1c(int self);
extern void func_ov004_020af948(void *a, int b, int c, void *d);
extern void func_ov004_020b2444(int a, int b, int c, int d, int e, int f, int g);

extern void func_ov004_020b4b84(Sprite* s, int* in);
extern void func_ov004_020b4cc4(Sprite* s);
extern void func_ov004_020b4e78(Sprite* s);
extern void func_ov004_020b506c(Sprite* s);
extern void func_ov004_020b5108(Sprite* s, int* in);
extern void func_ov004_020b51e4(Sprite* s);
extern void func_ov004_020b52b8(Sprite* s);
extern void func_ov004_020b52fc(Sprite* s);
extern void func_ov004_020b5334(Sprite* s);
extern void func_ov004_020b5368(void);
extern void func_ov004_020b53f0(void);
extern void func_ov004_020b556c(void);
extern void func_ov004_020b586c(void);
extern void func_ov004_020b58c4(void);
extern void func_ov004_020b5a54(void);
extern void func_ov004_020b5aac(void);
extern void func_ov004_020b5c18(void);
extern void func_ov004_020b5768(void);
extern void func_ov004_020b5f6c(void);
extern void func_ov004_020b612c(void);
extern void func_ov004_020b6234(void);
extern void func_ov004_020b653c(int arg);
extern void func_ov004_020b67e0(void);

extern int data_ov004_020b9488[];
extern int data_ov004_020bc7d0;   /* cap func_ov004_020b6234 raises the count to */
extern int data_ov004_020bc7d4;   /* nonzero: sequences also show their count */
/* Sprite states: member-pointer records (ov004 relocs.txt names each
   target). */
extern SpriteFn data_ov004_020bc7d8; /* func_ov004_020b4ff0 */
extern SpriteFn data_ov004_020bc7e0; /* func_ov004_020b4d50 */
extern SpriteFn data_ov004_020bc7e8; /* func_ov004_020b5288 */
extern SpriteFn data_ov004_020bc7f0; /* func_ov004_020b5288 */
extern SpriteFn data_ov004_020bc7f8; /* func_ov004_020b4f44 */
extern SpriteFn data_ov004_020bc800; /* func_ov004_020b51f0 */
extern SpriteFn data_ov004_020bc808; /* func_ov004_020b4aa4 */
extern SpriteFn data_ov004_020bc810; /* func_ov004_020b4dfc */
extern SpriteFn data_ov004_020bc818; /* func_ov004_020b4c30 */
extern SpriteFn data_ov004_020bc820; /* func_ov004_020b5288 */
extern int *data_ov004_020bc828[];
extern unsigned char *data_ov004_020bc83c[];
extern short data_ov004_020bf9e4; /* 1 when a sequence finishes; 2, 3, 4 while one runs */
extern int data_ov004_020bf9e8;   /* the count func_ov004_020b653c shows */
extern int data_ov004_020bf9ec;   /* set once by func_ov004_020b63a0 */
extern int data_ov004_020bf9f0;   /* the sequence's sound is still to play */
extern int data_ov004_020bf9f4;   /* last sprite func_ov004_020b5aac stepped */
extern int data_ov004_020bf9f8;   /* count func_ov004_020b56c8 lays out */
extern int data_ov004_020bf9fc;   /* sprites still to step */
extern int data_ov004_020bfa00;   /* index of the sprite being stepped */
extern int data_ov004_020bfa04;   /* frames until the next step */
extern int data_ov004_020bfa08;   /* last sprite func_ov004_020b5768 stepped */
extern int data_ov004_020bfa0c;   /* last sprite func_ov004_020b58c4 stepped */
extern int data_ov004_020bfa10;   /* a func_ov004_020b5f6c run is pending */
extern int data_ov004_020bfa14;   /* last sprite func_ov004_020b5c18 stepped */
extern int data_ov004_020bfa18;   /* sprites in play */
extern int data_ov004_020bfa1c;   /* func_ov004_020b5f6c steps this many with a delay */
extern void (*data_ov004_020bfa20)(); /* the current step; 0 when idle */
extern int data_ov004_020bfa24;   /* nonzero: func_ov004_020b6430 shows the count */
extern struct Sprite data_ov004_020bfa34[];
/* The address of data_ov004_020bfa34[0].mVisible; the ROM indexes the
   array's visible bytes from it. */
extern unsigned char data_ov004_020bfa56[];
extern char* data_ov004_020beb68;  /* the minigame scene; +0xa8 is dScMgBase_c::unk_0a8 */
extern int data_020a0db0;
}

// @symbol func_ov004_020b67e4
extern "C" void func_ov004_020b67e4(void)
{
}

// @symbol func_ov004_020b67e0
extern "C" void func_ov004_020b67e0(void)
{
}

// @symbol func_ov004_020b67bc
extern "C" void func_ov004_020b67bc(void)
{
    __cxa_vec_cleanup(data_ov004_020bfa34, 0x14, 0x24, (void (*)(void *))func_ov004_020b67e0);
}

// @symbol func_ov004_020b66d4
/* Reset every sprite and the sequence state. */
extern "C" void func_ov004_020b66d4(void) {
    int i;
    for (i = 0; i < 0x14; i++)
        func_ov004_020b5334(&data_ov004_020bfa34[i]);
    data_ov004_020bfa00 = 0;
    data_ov004_020bfa18 = 0;
    data_ov004_020bc7d0 = 5;
    data_ov004_020bf9fc = 0;
    data_ov004_020bf9f8 = 0;
    data_ov004_020bfa04 = 0;
    data_ov004_020bf9e4 = 0;
    data_ov004_020bf9ec = 0;
    data_ov004_020bfa10 = 0;
    data_ov004_020bf9f0 = 0;
    data_ov004_020bf9e8 = 0;
    data_ov004_020bfa24 = 0;
    data_ov004_020bc7d4 = 0;
    data_ov004_020bfa20 = 0;
}

// @symbol func_ov004_020b65e4
/* One frame. A fresh touch inside the top-left 0x40 x 0x50 box adds a
   sprite (func_ov004_020b6234) while that is allowed; then every sprite
   runs its state and the current step runs. */
extern "C" void func_ov004_020b65e4(void) {
    int i;
    Sprite *p;

    if (data_ov004_020bf9ec == 0 && data_ov004_020bfa18 != 0) {
        int idx = gActivePlayerSlot;
        int flag = ((unsigned char (*)[4])gTouchHeld)[idx][0] != 0 && ((unsigned char (*)[4])gTouchEdge)[idx][0] != 0;
        if (flag) {
            int a = ((unsigned char (*)[4])gTouchX)[idx][0];
            int b = ((unsigned char (*)[4])gTouchY)[idx][0];
            if (a < 0x40 && b < 0x50) {
                func_ov004_020b6234();
            }
        }
    }

    p = data_ov004_020bfa34;
    for (i = 0; i < 0x14; i++) {
        func_ov004_020b52fc(p);
        p++;
    }

    if (data_ov004_020bfa20 != 0) {
        data_ov004_020bfa20();
    }
}

// @symbol func_ov004_020b653c
/* Draw the signed count on row 0x60: the sprite icon (func_ov004_020b1de8)
   at x 0x6e, the per-language +0x34 graphic for arg >= 0 or +0x38 for
   arg < 0 at x 0x80, then |arg| at x 0x92. */
extern "C" void func_ov004_020b653c(int arg)
{
    int absv;
    int r4;
    int gfx;

    if (arg < 0) absv = -arg; else absv = arg;

    if (arg >= 0) {
        int idx = GetGameLanguage();
        r4 = 0x14;
        gfx = *(int *)(data_ov004_020bc83c[idx] + 0x34);
    } else {
        int idx = GetGameLanguage();
        r4 = 0x28;
        gfx = *(int *)(data_ov004_020bc83c[idx] + 0x38);
    }

    func_ov004_020b1de8(0x6e, 0x60, 0, -1);
    func_ov004_020af948((void *)gfx, 0x80, 0x60, 0);
    func_ov004_020b2444(0x92, 0x60, absv, 0, -1, 2, r4);
}

// @symbol func_ov004_020b6430
/* Draw: the touch button at (0x14, 0x30) while func_ov004_020b65e4 would
   take a touch, the count while data_ov004_020bfa24 is set, then every
   sprite. */
extern "C" void func_ov004_020b6430(void) {
    int i;
    Sprite *p;
    if (data_ov004_020bf9ec == 0 && data_ov004_020bfa18 != 0) {
        if (data_020a0db0 & 0x10) {
            Hud_RenderSprite((void*)data_ov004_020bc828[GetGameLanguage()][0], 0x14, 0x30, -1, -1);
        } else {
            Hud_RenderSprite((void*)data_ov004_020bc828[GetGameLanguage()][1], 0x14, 0x30, -1, -1);
        }
        Hud_RenderSprite((void*)data_ov004_020bc828[GetGameLanguage()][2], 0x14, 0x30, -1, -1);
    }
    if (data_ov004_020bfa24 != 0) {
        func_ov004_020b653c(data_ov004_020bf9e8);
    }
    for (p = data_ov004_020bfa34, i = 0; i < 0x14; i++) {
        func_ov004_020b52b8(p);
        p++;
    }
}

// @symbol func_ov004_020b63a0
/* Once: add up to n sprites, capped by the scene's unk_0a8, and start
   launching them (func_ov004_020b612c). */
extern "C" void func_ov004_020b63a0(int n){
  char* p;
  int v;
  if (data_ov004_020bf9ec != 0) return;
  p = data_ov004_020beb68;
  v = p ? *(int*)(p + 0xa8) : 0;
  if (n <= v) v = n;
  data_ov004_020bfa18 += v;
  data_ov004_020bfa20 = func_ov004_020b612c;
  Sound::PlayBank2_2D(0x14d);
  data_ov004_020bf9ec = 1;
}

// @symbol func_ov004_020b6324
/* Raise the count toward 5 by c and launch, the first c with a delay. */
extern "C" void func_ov004_020b6324(int c) {
    data_ov004_020bc7d0 = 5;
    ApproachLinear(data_ov004_020bfa18, 5, c);
    data_ov004_020bfa00 = 0;
    data_ov004_020bfa1c = c;
    data_ov004_020bfa10 = 1;
    data_ov004_020bfa04 = 0;
    data_ov004_020bfa20 = func_ov004_020b5f6c;
}

// @symbol func_ov004_020b6234
/* Add one sprite and launch it, or play sound 0xe when the scene's unk_0a8
   is 0, the count is at its cap, or func_ov004_020b63a0 has run. Does
   nothing while a launch is pending (data_ov004_020bfa10). */
extern "C" void func_ov004_020b6234(void) {
    int x;
    if (data_ov004_020beb68 != 0) {
        x = *(int *)(data_ov004_020beb68 + 0xa8);
    } else {
        x = 0;
    }
    if (x == 0) {
        func_02012790(0xe);
        return;
    }
    if (data_ov004_020bf9ec != 0 || data_ov004_020bfa18 >= data_ov004_020bc7d0) {
        func_02012790(0xe);
        return;
    }
    if (data_ov004_020bfa10 != 0) {
        return;
    }
    data_ov004_020bfa10 = 1;
    ApproachLinear(data_ov004_020bfa18, data_ov004_020bc7d0, 1);
    data_ov004_020bfa00 = data_ov004_020bfa18 - 1;
    Sound::PlayBank2_2D(0x14e);
    data_ov004_020bfa20 = func_ov004_020b5f6c;
}

// @symbol func_ov004_020b612c
/* Step: every 0x18 frames send the next sprite to
   (index * 16 + 8, -func_ov004_020b04c0() - 16) until all sprites in play
   are out. */
extern "C" void func_ov004_020b612c(void)
{
    int in[2];
    int n;
    int idx;
    if (data_ov004_020bfa18 <= data_ov004_020bfa00) return;
    if (ApproachLinear(data_ov004_020bfa04, 0, 1) == 0) return;
    n = -func_ov004_020b04c0();
    idx = data_ov004_020bfa00;
    n = (n - 0x10) << 12;
    in[0] = (idx * 16 + 8) << 12;
    in[1] = n;
    func_ov004_020b5108(&data_ov004_020bfa34[idx], in);
    Sound::PlayBank2_2D(0x14d);
    {
        int b = data_ov004_020bfa00;
        if (b == data_ov004_020bfa18 - 1) {
            data_ov004_020bf9e4 = 1;
            data_ov004_020bfa20 = 0;
            return;
        }
        data_ov004_020bfa00 = b + 1;
        data_ov004_020bfa04 = 0x18;
    }
}

// @symbol func_ov004_020b5f6c
/* Step: as func_ov004_020b612c, but only the first data_ov004_020bfa1c
   sprites wait 0x18 frames and play the sound; the rest go one per frame,
   with no wait or sound. */
extern "C" void func_ov004_020b5f6c(void)
{
    int inA[2];
    int inB[2];
    if (data_ov004_020bfa18 <= data_ov004_020bfa00)
        return;
    if (data_ov004_020bfa00 < data_ov004_020bfa1c) {
        if (ApproachLinear(data_ov004_020bfa04, 0, 1) == 0)
            return;
        {
            int v1 = (-func_ov004_020b04c0() - 0x10) << 12;
            int v0 = (data_ov004_020bfa00 * 16 + 8) << 12;
            inA[0] = v0;
            inA[1] = v1;
        }
        func_ov004_020b5108(&data_ov004_020bfa34[data_ov004_020bfa00], inA);
        Sound::PlayBank2_2D(0x14d);
        data_ov004_020bfa04 = 0x18;
        if (data_ov004_020bfa00 == data_ov004_020bfa18 - 1) {
            data_ov004_020bfa10 = 0;
            data_ov004_020bf9e4 = 1;
            data_ov004_020bfa20 = 0;
            return;
        }
        data_ov004_020bfa00 = data_ov004_020bfa00 + 1;
        return;
    }
    {
        int v1 = (-func_ov004_020b04c0() - 0x10) << 12;
        int v0 = (data_ov004_020bfa00 * 16 + 8) << 12;
        inB[0] = v0;
        inB[1] = v1;
        func_ov004_020b5108(&data_ov004_020bfa34[data_ov004_020bfa00], inB);
    }
    data_ov004_020bfa10 = 0;
    if (data_ov004_020bfa00 == data_ov004_020bfa18 - 1) {
        data_ov004_020bf9e4 = 1;
        data_ov004_020bfa20 = 0;
        return;
    }
    data_ov004_020bfa00 = data_ov004_020bfa00 + 1;
}

// @symbol func_ov004_020b5ed0
/* Start func_ov004_020b5c18 over every sprite in play. */
extern "C" void func_ov004_020b5ed0(void) {
    data_ov004_020bf9e4 = 2;
    data_ov004_020bfa04 = 0;
    data_ov004_020bf9fc = data_ov004_020bfa18;
    if (data_ov004_020bc7d4 != 0) {
        data_ov004_020bf9e8 = -data_ov004_020bfa18;
        data_ov004_020bfa24 = 1;
    }
    data_ov004_020bf9f0 = 1;
    data_ov004_020bfa20 = func_ov004_020b5c18;
}

// @symbol func_ov004_020b5e40
/* Start func_ov004_020b58c4 over x sprites. */
extern "C" void func_ov004_020b5e40(int x){
  int cond = data_ov004_020bc7d4;
  data_ov004_020bf9e4 = 2;
  data_ov004_020bfa04 = 0;
  data_ov004_020bf9fc = x;
  if (cond != 0) {
    data_ov004_020bf9e8 = -x;
    data_ov004_020bfa24 = 1;
  }
  data_ov004_020bf9f0 = 1;
  data_ov004_020bfa20 = func_ov004_020b58c4;
}

// @symbol func_ov004_020b5dd4
/* Start func_ov004_020b5aac with one more sprite still to step
   (data_ov004_020bf9fc + 1; 1 after a finished run). */
extern "C" void func_ov004_020b5dd4(void){
  data_ov004_020bf9e4 = 2;
  data_ov004_020bfa04 = 0;
  data_ov004_020bf9fc = data_ov004_020bf9fc + 1;
  data_ov004_020bf9f0 = 1;
  data_ov004_020bfa20 = func_ov004_020b5aac;
}

// @symbol func_ov004_020b5d74
/* Start func_ov004_020b5768 over every sprite in play, after 0x1e frames. */
extern "C" void func_ov004_020b5d74(void) {
    data_ov004_020bf9e4 = 4;
    data_ov004_020bfa04 = 0x1e;
    data_ov004_020bf9fc = data_ov004_020bfa18;
    data_ov004_020bfa20 = func_ov004_020b5768;
}

// @symbol func_ov004_020b5c18
/* Step: every 0x10 frames send the current sprite off with
   func_ov004_020b506c, walking the index down; then wait until the last
   one is hidden (mVisible 0) and hold 0x3c frames (func_ov004_020b5a54). */
extern "C" void func_ov004_020b5c18(void)
{
    if (data_ov004_020bf9fc > 0) {
        if (ApproachLinear(data_ov004_020bfa04, 0, 1) == 0)
            return;
        func_ov004_020b506c(&data_ov004_020bfa34[data_ov004_020bfa00]);
        if (data_ov004_020bf9f0 != 0) {
            int t = data_ov004_020beb68 ? *(int*)(data_ov004_020beb68 + 0xa8) : 0;
            if (t != 0)
                Sound::PlayBank2_2D(0x14b);
            else
                Sound::PlayBank2_2D(0x14c);
            data_ov004_020bf9f0 = 0;
        }
        data_ov004_020bfa14 = data_ov004_020bfa00;
        ApproachLinear(data_ov004_020bfa00, 0, 1);
        if (ApproachLinear(data_ov004_020bf9fc, 0, 1) == 0)
            data_ov004_020bfa04 = 0x10;
        return;
    }
    if (data_ov004_020bfa56[data_ov004_020bfa14 * 0x24] != 0)
        return;
    data_ov004_020bfa04 = 0x3c;
    data_ov004_020bfa20 = func_ov004_020b5a54;
}

// @symbol func_ov004_020b5aac
/* Step: func_ov004_020b5c18 with a different sound rule. */
extern "C" void func_ov004_020b5aac(void)
{
    if (data_ov004_020bf9fc > 0) {
        if (ApproachLinear(data_ov004_020bfa04, 0, 1) == 0)
            return;
        func_ov004_020b506c(&data_ov004_020bfa34[data_ov004_020bfa00]);
        if (data_ov004_020bf9f0 != 0) {
            int t = data_ov004_020beb68 ? *(int*)(data_ov004_020beb68 + 0xa8) : 0;
            if (t == 0 && data_ov004_020bfa00 == 0)
                Sound::PlayBank2_2D(0x14c);
            else
                Sound::PlayBank2_2D(0x14b);
            data_ov004_020bf9f0 = 0;
        }
        data_ov004_020bf9f4 = data_ov004_020bfa00;
        ApproachLinear(data_ov004_020bfa00, 0, 1);
        if (ApproachLinear(data_ov004_020bf9fc, 0, 1) == 0)
            data_ov004_020bfa04 = 0x10;
        return;
    }
    if (data_ov004_020bfa56[data_ov004_020bf9f4 * 0x24] != 0)
        return;
    data_ov004_020bfa04 = 0x3c;
    data_ov004_020bfa20 = func_ov004_020b5a54;
}

// @symbol func_ov004_020b5a54
/* Step: wait out data_ov004_020bfa04, then finish. */
extern "C" void func_ov004_020b5a54(void) {
    if (ApproachLinear(data_ov004_020bfa04, 0, 1) == 0)
        return;
    data_ov004_020bf9e4 = 1;
    data_ov004_020bfa20 = 0;
}

// @symbol func_ov004_020b58c4
/* Step: every 0x10 frames bring a sprite in with func_ov004_020b4e78,
   walking the index up and wrapping at 0x14, while the scene's unk_0a8 is
   nonzero; then wait until the last one is hidden (mVisible 0) and hold
   0x3c frames (func_ov004_020b586c). */
extern "C" void func_ov004_020b58c4(void)
{
    if (data_ov004_020bf9fc > 0 && (data_ov004_020beb68 ? *(int*)(data_ov004_020beb68 + 0xa8) : 0) != 0) {
        if (ApproachLinear(data_ov004_020bfa04, 0, 1) == 0)
            return;
        if (data_ov004_020bf9f0 != 0) {
            int t = data_ov004_020beb68 ? *(int*)(data_ov004_020beb68 + 0xa8) : 0;
            if (t <= data_ov004_020bf9fc)
                Sound::PlayBank2_2D(0x14c);
            else
                Sound::PlayBank2_2D(0x14b);
            data_ov004_020bf9f0 = 0;
        }
        func_ov004_020b4e78(&data_ov004_020bfa34[data_ov004_020bfa00]);
        data_ov004_020bfa0c = data_ov004_020bfa00;
        if (ApproachLinear(data_ov004_020bfa00, 0x14, 1) != 0)
            data_ov004_020bfa00 = 0;
        if (ApproachLinear(data_ov004_020bf9fc, 0, 1) == 0)
            data_ov004_020bfa04 = 0x10;
        return;
    }
    if (data_ov004_020bfa56[data_ov004_020bfa0c * 0x24] != 0)
        return;
    data_ov004_020bfa04 = 0x3c;
    data_ov004_020bfa20 = func_ov004_020b586c;
}

// @symbol func_ov004_020b586c
/* Step: wait out data_ov004_020bfa04, then finish. */
extern "C" void func_ov004_020b586c(void) {
    if (ApproachLinear(data_ov004_020bfa04, 0, 1) == 0)
        return;
    data_ov004_020bf9e4 = 1;
    data_ov004_020bfa20 = 0;
}

// @symbol func_ov004_020b5768
/* Step: every 0xc frames send the current sprite back to the corner with
   func_ov004_020b4cc4, walking the index down; finish when the last one
   reaches the corner. */
extern "C" void func_ov004_020b5768() {
    if (data_ov004_020bf9fc > 0) {
        if (ApproachLinear(data_ov004_020bfa04, 0, 1) == 0)
            return;
        func_ov004_020b4cc4(&data_ov004_020bfa34[data_ov004_020bfa00]);
        data_ov004_020bfa08 = data_ov004_020bfa00;
        ApproachLinear(data_ov004_020bfa00, 0, 1);
        if (ApproachLinear(data_ov004_020bf9fc, 0, 1) == 0) {
            data_ov004_020bfa04 = 0xc;
        }
        return;
    }
    if (data_ov004_020bfa56[data_ov004_020bfa08 * 0x24] != 0)
        return;
    data_ov004_020bf9e4 = 1;
    data_ov004_020bfa20 = 0;
}

// @symbol func_ov004_020b56c8
/* Hide every sprite and lay out p of them (func_ov004_020b556c). p is a
   count; the callers in d_s_mg_bsc.cpp and d_s_mg_card.cpp pass an int. */
extern "C" void func_ov004_020b56c8(char* p)
{
    int i;
    int cond;
    Sprite* it = data_ov004_020bfa34;
    for (i = 0; i < 0x14; i++) {
        func_ov004_020b51e4(it);
        it++;
    }
    cond = data_ov004_020bc7d4;
    data_ov004_020bf9f8 = (int)p;
    data_ov004_020bfa04 = 0;
    data_ov004_020bf9e4 = 3;
    if (cond != 0) {
        data_ov004_020bf9e8 = (int)p;
        data_ov004_020bfa24 = 1;
    }
    data_ov004_020bfa20 = func_ov004_020b556c;
}


// @symbol func_ov004_020b556c
/* Step: place up to 20 sprites on a grid five wide (rows 16 pixels apart,
   centred when fewer than five), each with a growing start delay, then
   hand over to func_ov004_020b53f0. */
extern "C" void func_ov004_020b556c(void)
{
    int step, idx, n, limit;
    int cx, rx;
    int sp[2];
    Sprite* e;
    int delay;
    if (ApproachLinear(data_ov004_020bfa04, 0, 1) == 0)
        return;
    step = 8;
    n = data_ov004_020bf9f8;
    if (n > 0x14) limit = 0x14; else limit = n;
    if (n > 0x14) step >>= 1;
    idx = 0;
    if (limit > 0) {
        e = data_ov004_020bfa34;
        delay = idx;
        do {
            int row = idx / 5;
            int col = idx % 5;
            cx = col << 0x10;
            rx = -(row << 0x10);
            {
                int ret = -func_ov004_020b04c0();
                sp[1] = (ret - 0x10) << 0xc;
            }
            sp[0] = 0x60000;
            {
                int c2 = data_ov004_020bf9f8;
                if (c2 < 5)
                    sp[0] = (0x80 - ((c2 - 1) << 3)) << 0xc;
            }
            sp[0] = sp[0] + cx;
            sp[1] = sp[1] + rx;
            func_ov004_020b4b84(e, sp);
            e->mDelay += delay;
            idx++;
            e++;
            delay += step;
        } while (idx < limit);
    }
    data_ov004_020bfa04 = 0;
    data_ov004_020bfa20 = func_ov004_020b53f0;
}

// @symbol func_ov004_020b53f0
/* Step: once no sprite is visible, lay out the next batch the same way
   (the count is decremented as sprites reach the corner); when none are
   left, hold 0x3c frames and go to func_ov004_020b5368. */
extern "C" void func_ov004_020b53f0(void)
{
    int step, i, idx, n, limit;
    Sprite* e;
    int sp[2];

    e = data_ov004_020bfa34;
    for (i = 0; i < 0x14; i++) {
        if (e->mVisible != 0)
            return;
        e++;
    }

    n = data_ov004_020bf9f8;
    if (n > 0) {
        if (n > 0x14)
            limit = 0x14;
        else
            limit = n;
        step = 8;
        if (n > 0x14)
            step >>= 1;
        /* idx counts the sprites placed; i is the next start delay */
        idx = 0;
        if (limit > 0) {
            e = data_ov004_020bfa34;
            i = idx;
            do {
                int row = idx / 5;
                int col = idx % 5;
                int cx = col << 0x10;
                int rx = -(row << 0x10);
                int ret = -func_ov004_020b04c0();
                sp[1] = (ret - 0x10) << 0xc;
                sp[0] = 0x60000;
                {
                    int c2 = data_ov004_020bf9f8;
                    if (c2 < 5) {
                        sp[0] = (0x80 - ((c2 - 1) << 3)) << 0xc;
                    }
                }
                sp[0] = sp[0] + cx;
                sp[1] = sp[1] + rx;
                func_ov004_020b4b84(e, sp);
                e->mDelay = i;
                idx++;
                e++;
                i += step;
            } while (idx < limit);
        }
        data_ov004_020bfa04 = 0;
        return;
    }
    data_ov004_020bfa04 = 0x3c;
    data_ov004_020bfa20 = func_ov004_020b5368;
}

// @symbol func_ov004_020b5368
/* Step: after data_ov004_020bfa04 frames, report the scene's unk_0a8
   through func_ov004_020adb1c when func_ov004_020ae1a8 returns 2, then
   finish. */
extern "C" void func_ov004_020b5368(void){
  if(ApproachLinear(data_ov004_020bfa04, 0, 1)==0) return;
  if(func_ov004_020ae1a8()==2){
    char* p = data_ov004_020beb68;
    func_ov004_020adb1c(p ? *(int*)(p+0xa8) : 0);
  }
  data_ov004_020bfa04 = 0;
  data_ov004_020bf9e4 = 1;
  data_ov004_020bfa20 = 0;
}

// @symbol func_ov004_020b5334
/* Reset one sprite: hidden, no delay, plain movement state. */
extern "C" void func_ov004_020b5334(Sprite *s) {
    func_ov004_020b51e4(s);
    s->mDelay = 0;
    s->mState = data_ov004_020bc7e8;
}

// @symbol func_ov004_020b52fc
extern "C" void func_ov004_020b52fc(Sprite *s) {
  (s->*(s->mState))();
}

// @symbol func_ov004_020b52b8
extern "C" void func_ov004_020b52b8(Sprite *s)
{
    if (s->mVisible == 0)
        return;
    func_ov004_020b1de8(s->mPos.x >> 12, s->mPos.y >> 12, -1, -1);
}

// @symbol func_ov004_020b5288
/* State: move toward the target and stay. */
extern "C" void func_ov004_020b5288(Sprite *s)
{
    ApproachLinear(s->mPos.x, s->mTarget.x, s->mStep.x);
    ApproachLinear(s->mPos.y, s->mTarget.y, s->mStep.y);
}

// @symbol func_ov004_020b51f0
/* State: move; on arrival call func_ov004_020b1b40(1), hide, and stop. */
extern "C" void func_ov004_020b51f0(Sprite* s) {
  int out[2];
  ApproachLinear(s->mPos.x, s->mTarget.x, s->mStep.x);
  ApproachLinear(s->mPos.y, s->mTarget.y, s->mStep.y);
  Vec2_Sub(out, &s->mTarget, &s->mPos);
  if (out[0] != 0) return;
  if (out[1] != 0) return;
  func_ov004_020b1b40((void*)1);
  func_ov004_020b51e4(s);
  s->mState = data_ov004_020bc7f0;
}

// @symbol func_ov004_020b51e4
extern "C" void func_ov004_020b51e4(Sprite *s)
{
    s->mVisible = 0;
}

// @symbol func_ov004_020b5108
/* Launch from the corner (12, 12) to `in`, at 1/16 of the distance per
   frame. Unless the sprite is already in func_ov004_020b51f0, call
   func_ov004_020b1b08(1). */
extern "C" void func_ov004_020b5108(Sprite* s, int* in)
{
    int loc0[2];
    int loc2[2];
    s->mVisible = 1;
    s->mPos.x = 0xc000;
    s->mPos.y = 0xc000;
    s->mTarget.x = in[0];
    s->mTarget.y = in[1];
    Vec2_Sub(loc0, &s->mTarget, &s->mPos);
    func_0203d680(loc2, loc0, 0x100);
    s->mStep.x = loc2[0];
    s->mStep.y = loc2[1];
    {
        int v = s->mStep.x;
        if (v < 0) v = -v;
        s->mStep.x = v;
    }
    {
        int v = s->mStep.y;
        if (v < 0) v = -v;
        s->mStep.y = v;
    }
    if (s->mState != data_ov004_020bc800)
        func_ov004_020b1b08((void*)1);
    s->mState = data_ov004_020bc820;
}

// @symbol func_ov004_020b506c
/* Send the sprite to x = 128 at 0xc0/0x1000 of the distance per frame
   (move state func_ov004_020b4ff0, then the rise in func_ov004_020b4f44). */
extern "C" void func_ov004_020b506c(Sprite *s)
{
    Vec2i a;
    Vec2i b;
    s->mTarget.x = 0x80000;
    Vec2_Sub(&a, &s->mTarget, &s->mPos);
    func_0203d680(&b, &a, 0xc0);
    s->mStep.x = b.x;
    s->mStep.y = b.y;
    {
        int x = s->mStep.x;
        if (x < 0) x = -x;
        s->mStep.x = x;
    }
    {
        int y = s->mStep.y;
        if (y < 0) y = -y;
        s->mStep.y = y;
    }
    s->mDelay = 0x14;
    Sound::PlayBank2_2D(0x14a);
    s->mState = data_ov004_020bc7d8;
}

// @symbol func_ov004_020b4ff0
/* State: move; on arrival rise (func_ov004_020b4f44). */
extern "C" void func_ov004_020b4ff0(Sprite *s) {
    int v[2];
    ApproachLinear(s->mPos.x, s->mTarget.x, s->mStep.x);
    ApproachLinear(s->mPos.y, s->mTarget.y, s->mStep.y);
    Vec2_Sub(v, &s->mTarget, &s->mPos);
    if (v[0] != 0) return;
    if (v[1] == 0) s->mState = data_ov004_020bc7f8;
}

// @symbol func_ov004_020b4f44
/* State: rise. g = data_ov004_020b9488[0] is 0x14 in the ROM; for mDelay
   0x14 down to 1, y changes by -3.5 px a frame, 0.2 px less each frame,
   ending near +0.3 px (about 32 px up in all). When mDelay reaches 0 the
   sprite leaves play and resets. */
extern "C" void func_ov004_020b4f44(Sprite* s){
  int* p = &s->mPos.y;
  int g = data_ov004_020b9488[0];
  int n = -(((short)s->mDelay - (g >> 1)) << 12);
  int q;
  int v;
  n = p ? n : n;  /* code shape: dropping it makes the function 12 bytes shorter */
  q = __aeabi_idiv(n, g);  /* `n / g` here compiles differently (measured) */
  v = (int)(((long long)(q - 0x600) * 0x4000 + 0x800) >> 12);
  *p += v;
  if (ApproachLinear2(s->mDelay, 0, 1) == 0)
    return;
  ApproachLinear(data_ov004_020bfa18, 0, 1);
  func_ov004_020b5334(s);
}

// @symbol func_ov004_020b4e78
/* Bring a sprite from the corner (12, 12) to
   (128, -func_ov004_020b04c0() - 16) (move state func_ov004_020b4dfc,
   then the rise in func_ov004_020b4d50). */
extern "C" void func_ov004_020b4e78(Sprite *s)
{
    Vec2i a;
    Vec2i b;
    func_ov004_020b1b08((void*)1);
    s->mVisible = 1;
    s->mPos.x = 0xc000;
    s->mPos.y = 0xc000;
    s->mTarget.x = 0x80000;
    s->mTarget.y = ((-func_ov004_020b04c0()) - 0x10) << 12;
    Vec2_Sub(&a, &s->mTarget, &s->mPos);
    func_0203d680(&b, &a, 0xc0);
    s->mStep.x = b.x;
    s->mStep.y = b.y;
    {
        int x = s->mStep.x;
        if (x < 0) x = -x;
        s->mStep.x = x;
    }
    {
        int y = s->mStep.y;
        if (y < 0) y = -y;
        s->mStep.y = y;
    }
    s->mDelay = 0x14;
    Sound::PlayBank2_2D(0x14a);
    s->mState = data_ov004_020bc810;
}

// @symbol func_ov004_020b4dfc
/* State: move; on arrival rise (func_ov004_020b4d50). */
extern "C" void func_ov004_020b4dfc(Sprite *s) {
    int v[2];
    ApproachLinear(s->mPos.x, s->mTarget.x, s->mStep.x);
    ApproachLinear(s->mPos.y, s->mTarget.y, s->mStep.y);
    Vec2_Sub(v, &s->mTarget, &s->mPos);
    if (v[0] != 0) return;
    if (v[1] == 0) s->mState = data_ov004_020bc7e0;
}

// @symbol func_ov004_020b4d50
/* State: func_ov004_020b4f44's rise, dividing with `/`. */
extern "C" void func_ov004_020b4d50(Sprite* s){
  int* p = &s->mPos.y;
  int g = data_ov004_020b9488[0];
  int n = -(((short)s->mDelay - (g >> 1)) << 12);
  int q = n / g;
  int v = (int)(((long long)(q - 0x600) * 0x4000 + 0x800) >> 12);
  *p = *p + v;
  if (ApproachLinear2(s->mDelay, 0, 1) == 0)
    return;
  ApproachLinear(data_ov004_020bfa18, 0, 1);
  func_ov004_020b5334(s);
}

// @symbol func_ov004_020b4cc4
/* Send the sprite back to the corner (12, 12) at 0xc0/0x1000 of the
   distance per frame (func_ov004_020b4c30). */
extern "C" void func_ov004_020b4cc4(Sprite* s){
  Vec2i v1;
  Vec2i v2;
  int a, b;
  s->mTarget.x = 0xc000;
  s->mTarget.y = 0xc000;
  Vec2_Sub(&v1, &s->mTarget, &s->mPos);
  func_0203d680(&v2, &v1, 0xc0);
  s->mStep.x = v2.x;
  s->mStep.y = v2.y;
  a = s->mStep.x;
  if (a < 0) a = -a;
  s->mStep.x = a;
  b = s->mStep.y;
  if (b < 0) b = -b;
  s->mStep.y = b;
  s->mState = data_ov004_020bc818;
}

// @symbol func_ov004_020b4c30
/* State: move; on arrival leave play, call func_ov004_020b1b40(1), reset. */
extern "C" void func_ov004_020b4c30(Sprite* s){
  int v[2];
  ApproachLinear(s->mPos.x, s->mTarget.x, s->mStep.x);
  ApproachLinear(s->mPos.y, s->mTarget.y, s->mStep.y);
  Vec2_Sub(v, &s->mTarget, &s->mPos);
  if (v[0] != 0) return;
  if (v[1] != 0) return;
  ApproachLinear(data_ov004_020bfa18, 0, 1);
  func_ov004_020b1b40((void*)1);
  func_ov004_020b5334(s);
}

// @symbol func_ov004_020b4b84
/* Place a sprite at `in`, visible, aimed at the corner (12, 12) at 1/16 of
   the distance per frame (func_ov004_020b4aa4). */
extern "C" void func_ov004_020b4b84(Sprite* s, int* in){
  Vec2i a;
  Vec2i b;
  s->mPos.x = in[0];
  s->mPos.y = in[1];
  s->mDelay = 0;
  s->mVisible = 1;
  s->mTarget.x = 0xc000;
  s->mTarget.y = 0xc000;
  Vec2_Sub(&a, &s->mTarget, &s->mPos);
  func_0203d680(&b, &a, 0x100);
  s->mStep.x = b.x;
  s->mStep.y = b.y;
  {
    int x = s->mStep.x;
    if (x < 0) x = -x;
    s->mStep.x = x;
  }
  {
    int y = s->mStep.y;
    if (y < 0) y = -y;
    s->mStep.y = y;
  }
  s->mState = data_ov004_020bc808;
}

// @symbol func_ov004_020b4aa4
/* State: wait out mDelay, then move; on arrival play sound 0x149, call
   func_ov004_020b1b40(1), take one from the layout count, and reset. */
extern "C" void func_ov004_020b4aa4(Sprite* s) {
    Vec2i t;
    if (ApproachLinear2(s->mDelay, 0, 1) == 0) return;
    ApproachLinear(s->mPos.x, s->mTarget.x, s->mStep.x);
    ApproachLinear(s->mPos.y, s->mTarget.y, s->mStep.y);
    Vec2_Sub(&t, &s->mTarget, &s->mPos);
    if (t.x != 0) return;
    if (t.y != 0) return;
    if (ApproachLinear2(s->mDelay, 0, 1) == 0) return;
    Sound::PlayBank2_2D(0x149);
    func_ov004_020b1b40((void*)1);
    ApproachLinear(data_ov004_020bf9f8, 0, 1);
    func_ov004_020b5334(s);
}
