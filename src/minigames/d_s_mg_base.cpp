//cpp
/* ov004/dScMgBase_c -- the behavior every minigame scene shares, and the
 * sprite, OAM and graphics helpers that sit with it
 * (.text 0x020ad660..0x020b2c58, 125 functions).
 *
 * This is the whole ov004 linker unit, from dScMgBase_c::Virtual8C at the
 * first byte of ov004's .text up to the constructor, which ends where
 * func_ov004_020b2c58 begins. It holds the eight scene overrides at
 * 0x020b04e8..0x020b0a38 plus 117 former one-function sources folded in
 * around them, among them the score-number drawer func_ov004_020b2220, the
 * engine and background set-up slots 31 to 33, the destructor and the
 * constructor. Each former source keeps its own namespace block. Copies
 * that repeated decl_common.h or dScMgBase_c.h are gone; the local struct
 * views and extern declarations left disagree with a header or with each
 * other, and are separate work.
 *
 * Definitions are in ROM order, lowest address first, under
 * defer_codegen off. The one exception is the constructor and destructor
 * at the end, which sit in a defer_codegen on bracket with the constructor
 * written first: see the note there.
 *
 * GraphCallback0, the key function of dScMgBase_c::graphCallback_c, is here,
 * and so is ~dScMgBase_c, the key function of dScMgBase_c itself, so this
 * file emits both classes' vtables and RTTI.
 *
 * Still unnamed: the header's unk_ fields, and the bytes it leaves as
 * padding that this file reaches by offset (+0x68/+0x6c, +0xb0,
 * +0x2228/+0x4228, +0x4000, +0x4644, +0x4650), plus dScMgD3DBase_c's
 * +0x4664; the func_ov004 and data_ov004 helpers are unnamed in
 * symbols.txt.
 */

#include "dScMgBase_c.h"
#include "dWipe_c.h"
#include "decl_common.h"
#include "Sound.h"
#include "types.h"
#include "dEnemyBase_c.h"
#include "PlayerInput.h"

#pragma defer_codegen off

/* The 0x40 records at data_ov004_020bebe8 are 0x20 bytes each. func_ov004_020adf70
   fills one: a Fix12 position, two optional vectors, a frame timer and a state.
   State 0 (func_ov004_020adeb0 / func_ov004_020adcc8) adds mVecB into mVecA and
   mVecA into mPos each frame; state 1 (func_ov004_020addcc /
   func_ov004_020adc80) interpolates mPos between the two vectors as the timer
   runs down. The class is not recovered, so the type stays a local view.
   The three s32 pairs stay raw offsets: declaring them as s32 members makes
   the struct 4-aligned, and the pool's .bss section goes from 2-byte to
   4-byte alignment. */
struct Ent {
    char pad[0x1a];                     /* 0x00 mPos, 0x08 mVecA, 0x10 mVecB */
    s16 mTimer;                         /* 0x1a -- counts down to 0; 0 = free */
    s16 mDuration;                      /* 0x1c -- mTimer's starting value */
    s16 mState;                         /* 0x1e -- index into both PMF tables */
    Ent() {}
    ~Ent() {}
    void func_ov004_020adc80();
    void func_ov004_020adcc8();
    void func_ov004_020addcc();
    void func_ov004_020adeb0();
};

typedef void (Ent::*EntPMF)();

extern "C" {
/* Static minigame base state: the graphCallback_c registry instance, the
   Ent record pool, and the two PMF state tables the dispatchers call
   through. Declaration order is the ROM's __sinit order. */
dScMgBase_c::graphCallback_c data_ov004_020beb74;
Ent data_ov004_020bebe8[0x40];
EntPMF data_ov004_020beb88[2] = { &Ent::func_ov004_020adeb0, &Ent::func_ov004_020addcc };
EntPMF data_ov004_020beb98[2] = { &Ent::func_ov004_020adcc8, &Ent::func_ov004_020adc80 };
int data_ov004_020beba8[16];
}

/* Local view of the stylus owner at data_0209f5bc, also used by
 * dScMiniGm_c. Only slot 5 is known: it says whether input is live. The
 * owner's real class is not recovered. */
struct SceneVCall6 {
    virtual int v0(); virtual int v1(); virtual int v2();
    virtual int v3(); virtual int v4(); virtual int IsActive();
};

int ApproachLinear(int &value, int target, int step);
int ApproachLinear2(short &value, short target, short step);
namespace cstd { int fdiv(int num, int den); }

extern "C" {
extern void *data_ov004_020beb60;
extern void **data_0209d4a8;
int func_ov004_020b8ee0(char *p);
void func_ov004_020aeb24(char *c);
void func_ov004_020b321c(char *c);
void func_ov004_020adf2c(char *c);
extern SceneVCall6 *data_0209f5bc;
extern unsigned short data_020a0e5a[];
extern int data_0208ee44;
extern void func_ov004_020ad90c(void);
extern void FreeGfxSlotsById(int arg);
extern void func_0203cbc0(void *ptr);
extern void func_02012e1c(void);
void func_ov004_020ae330();
extern void Enable3dEngines(void);
extern char data_0209b308[];
extern dWipe_c data_0209f61c;
extern unsigned char data_0209d460[];
extern unsigned char data_0209d458[];

/* The scene's five saved records: func_02013580(table, i) reads record i of
   save table `table`, func_02013568(table, i, v) writes it, and
   func_ov004_020adc3c(scene) picks the table from byte 1 of param1. */
int func_02013580(int table, int index);
void func_02013568(int table, int index, int value);
int func_ov004_020adc3c(void *scene);

/* One table of sprite-list words per language, indexed by
   GetGameLanguage(); the helpers below hand its words to the OAM
   wrappers. */
extern int *data_ov004_020bbfa8[];
}

namespace s20ad660 {
extern "C" {
// @symbol _ZN11dScMgBase_c9Virtual8CEv
/* dScMgBase_c::Virtual8C - slot 35, the LAST slot of dScMgBase_c's own
   eighteen.  Twenty bytes, and the end of the 18-35 keystone range: with this
   declared the class emits its full 36-slot vtable from source and no table in
   the minigame family is a prefix any more.

   It is a predicate on the scene's own spawn parameter: `param1 & 0xff`, the
   fBase_c word at +0x08 that every fBase_c is constructed with, tested against
   zero.  dScMgAmida_c is the only class in the family that overrides it, and
   it asks the narrower question (`== 1`); the other 31 tables carry this body.

   Thirteen call sites, all in ov006, all the same shape --
   `mov r0,<this>; ldr r1,[r0]; ldr r1,[r1,#0x8c]; blx r1` -- spread across
   FOUR leaf classes' code regions: dScMgCoin_c (2), dScMgPanel_c (4),
   dScMgSound_c (3), dScMgSnowball_c (4).  Each is a class asking the question
   of itself and branching on the answer, which is how a single minigame runs
   in two variants: two asset tables at ov006:0x02105488, a different field
   path at 0x0211b9e0, a whole block skipped at 0x02126f58.

   All thirteen follow the call with `cmp r0, #0`.  That makes this the one
   slot in the campaign whose return value is not merely permitted by the ROM
   but seen to be consumed by it.

   `Virtual8C` is a placeholder after the +0x8c vtable offset, not a ROM name;
   as at slots 33 and 34 there was no `recovered name:` line here to correct.

   The rename had to be scoped: 0x020ad660 is an overlay load base, so ov000,
   ov002, ov003, ov004 and ov007 all have unrelated symbols at that address
   (ov003's is dScTitle_c's D1).  Only ov004's is this function. */
}
}

int dScMgBase_c::Virtual8C()
{
    return (param1 & 0xff) != 0;
}

// @symbol GetGameLanguage
namespace s20ad674 {
extern "C" {
s32 GetGameLanguage(void)
{
    switch (GetOwnerLanguage())
    {
    case 5:
        return 1;
    case 2:
        return 2;
    case 3:
        return 3;
    case 4:
        return 4;
    default:
        return 0;
    }
}
}
}

// @symbol func_ov004_020ad6f4
namespace s20ad6f4 {
extern "C" {
/* Returns unk_064: the index (0..4) at which func_ov004_020ad940 last
   inserted a score into the five saved records, or -1 if it placed nowhere
   (the constructor also seeds -1). */
int func_ov004_020ad6f4(dScMgBase_c *scene)
{
    return scene->unk_064;
}
}
}

// @symbol func_ov004_020ad6fc
namespace s20ad6fc {
extern "C" {
extern int data_0209b308[];
/* Returns the index (0..4) at which `value` would enter the scene's five
   saved records, or -1. Scoring mode 5 (word 2 of the menu row) is
   lower-is-better and never places a value <= 0; every other mode is
   higher-is-better. The `(unsigned)` casts make the compares unsigned. */
int func_ov004_020ad6fc(void *scene, int value)
{
    int i;
    if (data_0209b308[2] == 5) {
        if (value <= 0) return -1;
        for (i = 0; i < 5; i++) {
            if (func_02013580(func_ov004_020adc3c(scene), i) > (unsigned)value) return i;
        }
    } else {
        for (i = 0; i < 5; i++) {
            if (func_02013580(func_ov004_020adc3c(scene), i) < (unsigned)value) return i;
        }
    }
    return -1;
}
}
}

// @symbol func_ov004_020ad79c
namespace s20ad79c {
extern "C" {
extern int data_0209b308[];
extern int data_0209b300[];

/* Saves a two-value result into saved records 0 and 1 of the current scene.
   A first value <= 0 replaces both with the menu row's defaults (words 5
   and 6); a second value <= 0 alone falls back to word 6. A positive value
   is capped at 9999. Nothing is written when both already match;
   otherwise data_0209b300 is set to 1. */
void func_ov004_020ad79c(int first, int second) {
    int v0 = first, v1 = second;
    if (data_ov004_020beb68 == 0) return;
    if (v0 <= 0) {
        v0 = data_0209b308[5];
        v1 = data_0209b308[6];
    } else {
        if (v0 > 9999) v0 = 9999;
    }
    if (v1 <= 0) {
        v1 = data_0209b308[6];
    } else {
        if (v1 > 9999) v1 = 9999;
    }
    if (v0 == func_ov004_020ad8b8()) {
        if (v1 == func_ov004_020ad878()) return;
    }
    func_02013568(func_ov004_020adc3c(data_ov004_020beb68), 0, v0);
    func_02013568(func_ov004_020adc3c(data_ov004_020beb68), 1, v1);
    *(unsigned char*)data_0209b300 = 1;
}
}
}

// @symbol func_ov004_020ad878
namespace s20ad878 {
extern "C" {
/* Saved record 1 of the current scene, or 0 with no scene. */
int func_ov004_020ad878(void) {
    void *scene = data_ov004_020beb68;
    if (scene == 0) return 0;
    return func_02013580(func_ov004_020adc3c(scene), 1);
}
}
}

// @symbol func_ov004_020ad8b8
namespace s20ad8b8 {
extern "C" {
extern int data_0209b308[];
/* Saved record 0 of the current scene; an empty record reads as the menu
   row's word 5. 0 with no scene. */
int func_ov004_020ad8b8(void) {
    if (data_ov004_020beb68 != 0) {
        int r = func_02013580(func_ov004_020adc3c(data_ov004_020beb68), 0);
        if (r == 0) return data_0209b308[5];
        return r;
    }
    return 0;
}
}
}

// @symbol func_ov004_020ad90c
namespace s20ad90c {
extern "C" {
extern void func_ov004_020ad940(dScMgBase_c *scene);

void func_ov004_020ad90c(void) {
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene == 0) return;
    func_ov004_020ad940(scene);
}
}
}

// @symbol func_ov004_020ad940
namespace s20ad940 {
extern "C" {
int func_ov004_020adafc(void);
int func_ov004_020ad6fc(void *scene, int value);
void func_ov004_020ada20(void);
void func_ov004_020ada40(int score);
int func_ov004_020adbc0(void);
extern unsigned char data_0209b300[];

/* Ranks the session best (unk_4654, from func_ov004_020adafc) among the
   five saved records with func_ov004_020ad6fc and keeps that rank in
   unk_064 (-1 when it placed nowhere). If it placed, the lower rows shift
   down and the submitted score unk_464c (func_ov004_020adbc0) -- not the
   ranked value -- is written into that row, and data_0209b300 is set to 1.
   Either way the new-record flag is cleared, the session best moves to
   unk_4658 and is then reset to 0. */
void func_ov004_020ad940(dScMgBase_c *scene)
{
    int n = func_ov004_020ad6fc(scene, func_ov004_020adafc());
    scene->unk_064 = n;
    if (scene->unk_064 < 0) {
        func_ov004_020ada20();
        scene->unk_4658 = func_ov004_020adafc();
        func_ov004_020ada40(0);
        return;
    }
    int i;
    for (i = 4; i > scene->unk_064; i--) {
        int a = func_ov004_020adc3c(scene);
        int b = func_ov004_020adc3c(scene);
        int v = func_02013580(b, i-1);
        func_02013568(a, i, v);
    }
    int a = func_ov004_020adc3c(scene);
    int v = func_ov004_020adbc0();
    func_02013568(a, scene->unk_064, v);
    data_0209b300[0] = 1;
    func_ov004_020ada20();
    scene->unk_4658 = func_ov004_020adafc();
    func_ov004_020ada40(0);
}
}
}

// @symbol func_ov004_020ada20
namespace s20ada20 {
extern "C" {
/* Clears the new-record flag unk_465c. */
void func_ov004_020ada20(void) {
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene != 0) {
        scene->unk_465c = 0;
    }
}
}
}

// @symbol func_ov004_020ada40
namespace s20ada40 {
extern "C" {
extern int data_0209b308[];
extern int func_ov004_020adc1c(void);

/* Stores `score` as the session best (unk_4654). If it beats the record
   copy at +0x4650, it becomes the new copy and unk_465c is set. Beating
   it means, compared unsigned: in scoring mode 5, nonzero and either lower
   or the copy is 0; otherwise higher. */
void func_ov004_020ada40(int score)
{
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene == 0) return;
    scene->unk_4654 = score;
    if (data_0209b308[2] == 5) {
        if (score == 0) return;
        if (func_ov004_020adc1c() != 0) {
            if ((unsigned)score >= (unsigned)func_ov004_020adc1c()) return;
        }
        func_ov004_020adc00(score);
        ((dScMgBase_c *)data_ov004_020beb68)->unk_465c = 1;
        return;
    }
    if ((unsigned)score <= (unsigned)func_ov004_020adc1c()) return;
    func_ov004_020adc00(score);
    ((dScMgBase_c *)data_ov004_020beb68)->unk_465c = 1;
}
}
}

// @symbol func_ov004_020adafc
namespace s20adafc {
extern "C" {
/* The session best, unk_4654; 0 with no scene. */
int func_ov004_020adafc(void) {
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene != 0) {
        return scene->unk_4654;
    }
    return 0;
}
}
}

// @symbol func_ov004_020adb1c
namespace s20adb1c {
extern "C" {
extern int func_ov004_020adafc(void);
extern int func_ov004_020ada40(int);
extern int data_0209b308[];
/* Submits a score: stores it in unk_464c (outside scoring mode 5, capped
   at the menu row's word 3 when that is nonzero) and passes it to
   func_ov004_020ada40 when it beats the session best. */
void func_ov004_020adb1c(int score) {
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene == 0) return;
    if (data_0209b308[2] == 5) {
        scene->unk_464c = score;
        if (func_ov004_020adafc() != 0) {
            if ((unsigned)score >= (unsigned)func_ov004_020adafc()) return;
        }
        func_ov004_020ada40(score);
    } else {
        int cap = data_0209b308[3];
        if (cap != 0) {
            if ((unsigned)score > (unsigned)cap) score = cap;
        }
        scene->unk_464c = score;
        if ((unsigned)score <= (unsigned)func_ov004_020adafc()) return;
        func_ov004_020ada40(score);
    }
}
}
}

// @symbol func_ov004_020adbc0
namespace s20adbc0 {
extern "C" {
/* The submitted score, unk_464c; 0 with no scene. */
int func_ov004_020adbc0(void) {
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene != 0) {
        return scene->unk_464c;
    }
    return 0;
}
}
}

// @symbol func_ov004_020adbe0
namespace s20adbe0 {
extern "C" {
/* The new-record flag, unk_465c; 0 with no scene. */
int func_ov004_020adbe0(void) {
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene != 0) {
        return scene->unk_465c;
    }
    return 0;
}
}
}

// @symbol func_ov004_020adc00
namespace s20adc00 {
extern "C" {
/* Spelled char * here and in func_ov004_020adc1c, as unit020b4aa4.cpp
   spells it: with no char * copy in this file the declaration-agreement
   count ties 1-1 with the header's void *, the tie goes to the header and
   unit020b4aa4.cpp goes red. */
extern char *data_ov004_020beb68;
/* Sets the record copy at +0x4650 (unnamed padding in the header). Here
   and in func_ov004_020adc1c the base is a char *: with an int-typed base
   mwcc materializes the +0x46xx offset from the literal pool instead of
   splitting it as add r0,r0,#0x4000 / ldr r0,[r0,#0x6xx] the way the
   cartridge does. */
void func_ov004_020adc00(int v) {
    char *scene = data_ov004_020beb68;
    if (scene != 0) {
        *(int*)(scene + 0x4650) = v;
    }
}
}
}

// @symbol func_ov004_020adc1c
namespace s20adc1c {
extern "C" {
extern char *data_ov004_020beb68;
/* The record copy at +0x4650; 0 with no scene. */
int func_ov004_020adc1c(void) {
    char *scene = data_ov004_020beb68;
    if (scene != 0) {
        return *(int*)(scene + 0x4650);
    }
    return 0;
}
}
}

// @symbol func_ov004_020adc3c
namespace s20adc3c {
extern "C" {
/* The scene's save-table id: byte 1 of param1. */
int func_ov004_020adc3c(void *scene) {
    return (((dScMgBase_c *)scene)->param1 & 0xff00) >> 8;
}
}
}

// @symbol func_ov004_020adc4c
namespace s20adc4c {
extern "C" {
extern int data_ov004_020beb60[];
int func_ov004_020adc4c(void) { return data_ov004_020beb60[0]; }
}
}

// @symbol Ov004_Deallocate
namespace s20adc5c {
extern "C" {
void Ov004_Deallocate(void *ptr)
{
    Deallocate(ptr);
}
}
}

// @symbol func_ov004_020adc68
namespace s20adc68 {
extern "C" {
extern void LoadFile(void);
void func_ov004_020adc68(void) { LoadFile(); }
}
}

// @symbol func_ov004_020adc74
namespace s20adc74 {
extern "C" {
extern void *func_020182bc(const char *path);

void *func_ov004_020adc74(const char *path)
{
    return func_020182bc(path);
}
}
}

// @symbol _ZN3Ent19func_ov004_020adc80Ev
namespace s20adc80 {
extern "C" {
extern int GetGameLanguage(void);
extern int func_ov004_020b1d60(int,int,int,long long);
}
}
/* State 1's draw: word 2 of the current language's entry in
   data_ov004_020bbfa8, at mPos >> 12. */
void Ent::func_ov004_020adc80(){
    int idx=s20adc80::GetGameLanguage();
    int *t=data_ov004_020bbfa8[idx];
    s20adc80::func_ov004_020b1d60(t[2], ((int*)this)[0]>>0xc, ((int*)this)[1]>>0xc, -1);
}

// @symbol _ZN3Ent19func_ov004_020adcc8Ev
namespace s20adcc8 {
extern "C" {
extern void func_ov004_020b1d60(int a0,int a1,int a2,int a3,int a4);
extern int data_ov004_020beb70;
extern int data_ov004_020bea14[];
extern int data_ov004_020beba8[];
}
}
/* State 0's draw. The first call builds the 16-entry frame table
   data_ov004_020beba8 from five words at data_ov004_020bea14 (bit 0 of
   data_ov004_020beb70 marks it built); the frame is picked by the low four
   bits of mTimer. */
void Ent::func_ov004_020adcc8(){
  if((s20adcc8::data_ov004_020beb70 & 1) == 0){
    int* s = s20adcc8::data_ov004_020bea14;
    int* d = s20adcc8::data_ov004_020beba8;
    d[0]=s[0];d[1]=s[0];d[2]=s[0];d[3]=s[0];d[4]=s[0];
    d[5]=s[4];d[6]=s[4];d[7]=s[3];d[8]=s[2];d[9]=s[1];
    d[10]=s[2];d[11]=s[2];d[12]=s[3];d[13]=s[4];d[14]=s[4];d[15]=s[0];
    s20adcc8::data_ov004_020beb70 |= 1;
  }
  {
    int idx=((int)mTimer)&0xf;
    int x=*(int*)this>>0xc; int y=*(int*)((char*)this+4)>>0xc;
    s20adcc8::func_ov004_020b1d60(s20adcc8::data_ov004_020beba8[idx],x,y,-1,-1);
  }
}

// @symbol func_ov004_020add88
namespace s20add88 {
extern "C" {
extern EntPMF data_ov004_020beb98[];
/* Draws one record through its state's handler. */
extern "C" void func_ov004_020add88(Ent *c) {
  (c->*data_ov004_020beb98[c->mState])();
}
}
}

// @symbol _ZN3Ent19func_ov004_020addccEv
namespace s20addcc {
extern "C" {
void func_ov004_020b1b40(void *c);
inline long long inline_fn(int arg0)
{
  return (long long) arg0;
}
}
}
/* State 1's update. Counts mTimer down, calling func_ov004_020b1b40(1) on
   the frame it reaches 0, then places mPos between the two vectors by the
   remaining fraction s = mTimer / mDuration: x = mVecA.x * s + mVecB.x * (1 - s),
   y = mVecB.y + s * s * (mVecA.y - mVecB.y), all Fix12. */
void Ent::func_ov004_020addcc()
{
  char *r5 = (char *)this;
  using namespace s20addcc;
  {
  if (ApproachLinear2(mTimer, 0, 1))
  {
    func_ov004_020b1b40((void *) 1);
  }
  int a = mTimer;
  int b = mDuration;
  int s = cstd::fdiv(a << 0xc, b << 0xc);
  int c3 = mDuration;
  int c2 = mTimer;
  int t = cstd::fdiv((c3 - c2) << 0xc, c3 << 0xc);
  int v8 = *((int *) (r5 + 8));
  int v10 = *((int *) (r5 + 0x10));
  long long m1 = inline_fn(v8) * inline_fn(s);
  long long m2 = inline_fn(v10) * inline_fn(t);
  long long m3 = inline_fn(s) * inline_fn(s);
  long long m4 = inline_fn((int) ((m3 + 0x800) >> 0xc)) * inline_fn((*((int *) (r5 + 0xc))) - (*((int *) (r5 + 0x14))));
  *((int *) r5) = ((int) ((m1 + 0x800) >> 0xc)) + ((int) ((m2 + 0x800) >> 0xc));
  *((int *) (r5 + 4)) = (*((int *) (r5 + 0x14))) + ((int) ((m4 + 0x800) >> 0xc));
  }
}

// @symbol _ZN3Ent19func_ov004_020adeb0Ev
/* State 0's update: counts mTimer down, passes mVecA to func_0203d630 with
   0xff8, then adds mVecB into mVecA and mVecA into mPos. */
void Ent::func_ov004_020adeb0()
{
    char* c = (char*)this;
    ApproachLinear2(mTimer, 0, 1);
    func_0203d630((int*)(c + 8), 0xff8);
    int *p = (int*)(((int)c + 8));
    int *p2 = (int*)(((int)c + 0xc));
    *p += *(int*)(c + 0x10);
    *p2 += *(int*)(c + 0x14);
    p = (int*)(((int)c + 4));
    *(int*)c += *(int*)(c + 8);
    *p += *(int*)(c + 0xc);
}

// @symbol func_ov004_020adf2c
namespace s20adf2c {
extern "C" {
extern EntPMF data_ov004_020beb88[];
/* Updates one record through its state's handler. */
extern "C" void func_ov004_020adf2c(Ent *c) {
  (c->*data_ov004_020beb88[c->mState])();
}
}
}

// @symbol func_ov004_020adf70
namespace s20adf70 {
extern "C" {
/* Fills record `c`. mDuration starts equal to mTimer; either vector may
   be omitted with a null pointer. */
void func_ov004_020adf70(char *c, short state, short timer, int *pos, int *vecA, int *vecB){
  Ent *e=(Ent*)c;
  e->mState=state;
  e->mTimer=timer;
  e->mDuration=e->mTimer;
  *(int*)(c)=pos[0];
  *(int*)(c+4)=pos[1];
  if(vecA){
    *(int*)(c+8)=vecA[0];
    *(int*)(c+0xc)=vecA[1];
  }
  if(vecB){
    *(int*)(c+0x10)=vecB[0];
    *(int*)(c+0x14)=vecB[1];
  }
}
}
}

// @symbol func_ov004_020adfc4
namespace s20adfc4 {
extern "C" {
extern void func_ov004_020adf70(void *rec, char *state, short timer, short pos, int *vecA, int *vecB);
/* Fills the first free record of the 0x40 at data_ov004_020bebe8 (see
   func_ov004_020adf70); does nothing without a position or a free record.
   The parameter types are not the callers': `state` arrives as an int and
   `pos` is the position pointer, tested whole (movs r4, r2). They are left
   as they are because retyping them adds new disagreements with the
   callers' differing declarations in the declaration-agreement gate. */
void func_ov004_020adfc4(char *state, short timer, short pos, int *vecA, int *vecB, int *unused){
  if(pos==0) return;
  int i;
  for(i=0;i<0x40;i++){
    if(data_ov004_020bebe8[i].mTimer == 0){
      func_ov004_020adf70(&data_ov004_020bebe8[i], state, timer, pos, vecA, vecB);
      return;
    }
  }
}
}
}

namespace s20ae03c {
extern "C" {
// @symbol _ZN11dScMgBase_c15graphCallback_c14GraphCallback3Ev
/* dScMgBase_c::graphCallback_c::GraphCallback3 -- slot 3: calls the
   registered scene's OnPushed (slot 25, +0x64) and reports handled. */
}
}

int dScMgBase_c::graphCallback_c::GraphCallback3()
{
    dScMgBase_c *p = mScene;

    if (p != 0) {
        p->OnPushed();
    }

    return 1;
}

namespace s20ae06c {
extern "C" {
// @symbol _ZN11dScMgBase_c15graphCallback_c14GraphCallback2Ev
/* dScMgBase_c::graphCallback_c::GraphCallback2 -- slot 2.
 *
 * Unlike slots 0, 1 and 3, this one RETURNS the scene's OnKicked (slot 24,
 * +0x60) result rather than discarding it and reporting handled; it only
 * reports handled when no scene is registered.
 */
}
}

int dScMgBase_c::graphCallback_c::GraphCallback2()
{
    dScMgBase_c *p = mScene;

    if (p == 0) {
        return 1;
    }

    return p->OnKicked();
}

namespace s20ae0a4 {
extern "C" {
// @symbol _ZN11dScMgBase_c15graphCallback_c14GraphCallback1Ev
/* dScMgBase_c::graphCallback_c::GraphCallback1 -- slot 1: calls the
   registered scene's OnAttacked1 (slot 22, +0x58) and reports handled. */
}
}

int dScMgBase_c::graphCallback_c::GraphCallback1()
{
    dScMgBase_c *p = mScene;

    if (p != 0) {
        p->OnAttacked1();
    }

    return 1;
}

namespace s20ae0d4 {
extern "C" {
// @symbol _ZN11dScMgBase_c15graphCallback_c14GraphCallback0Ev
/* dScMgBase_c::graphCallback_c::GraphCallback0 -- slot 0: calls the
   registered scene's OnAttacked2 (slot 23, +0x5c) and reports handled. */
}
}

int dScMgBase_c::graphCallback_c::GraphCallback0()
{
    dScMgBase_c *p = mScene;

    if (p != 0) {
        p->OnAttacked2();
    }

    return 1;
}

// @symbol _ZN11dScMgBase_c15graphCallback_cC1Ev
/* graphCallback_c ctor -- the base callback_c vptr store, the derived vptr
   store, and mScene = 0 are the three writes the ROM body makes. */
dScMgBase_c::graphCallback_c::graphCallback_c() : mScene(0) {}

namespace s20ae128 {
extern "C" {
// @symbol _ZN11dScMgBase_c8OnPushedEv
// recovered name: dScMgBase_c_OnPushed
/* dScMgBase_c::OnPushed - slot 25: nonzero while the menu is closed. */
}
}

int dScMgBase_c::OnPushed()
{
    return mMenuOpen == 0;
}

namespace s20ae140 {
extern "C" {
// @symbol _ZN11dScMgBase_c8OnKickedEv
// recovered name: dScMgBase_c_OnKicked
/* dScMgBase_c::OnKicked - slot 24. When mMenuOpen differs from its latch
   unk_462c, calls OnAimedAtWithEgg (slot 29) if the menu has opened or
   OnAimedAtWithEggReturnVec (slot 30) if it has closed, then latches it. */
}
}

int dScMgBase_c::OnKicked()
{
    int v2 = mMenuOpen;
    int v1 = unk_462c;
    if (v1 == v2) goto done;
    if (v2 == 0) {
        OnAimedAtWithEggReturnVec();
    } else {
        OnAimedAtWithEgg();
    }
    unk_462c = mMenuOpen;
done:
    return 1;
}

namespace s20ae198 {
extern "C" {
// @symbol _ZN11dScMgBase_c11OnAttacked1Ev
// recovered name: dScMgBase_c_OnAttacked1
/* dScMgBase_c::OnAttacked1 - slot 22 */
}
}

int dScMgBase_c::OnAttacked1()
{
    return 1;
}

namespace s20ae1a0 {
extern "C" {
// @symbol _ZN11dScMgBase_c11OnAttacked2Ev
// recovered name: dScMgBase_c_OnAttacked2
/* dScMgBase_c::OnAttacked2 - slot 23 */
}
}

int dScMgBase_c::OnAttacked2()
{
    return 1;
}

// @symbol func_ov004_020ae1a8
namespace s20ae1a8 {
extern "C" {
extern int data_0209b308[];
/* The menu row's scoring mode (word 2), or -1 with no scene. */
int func_ov004_020ae1a8(void) {
    if (data_ov004_020beb68 != 0) {
        return data_0209b308[2];
    }
    return -1;
}
}
}

// @symbol func_ov004_020ae1cc
namespace s20ae1cc {
extern "C" {
extern int data_0209b308[];

/* The menu row's music mode (word 11, +0x2c), or 0 with no scene. The
   music helpers below treat 0 as "stop the music" and 1 as "play
   func_ov004_020ae1f0's track". */
int func_ov004_020ae1cc(void) {
    if (data_ov004_020beb68 != 0) {
        return data_0209b308[11];
    }
    return 0;
}
}
}

// @symbol func_ov004_020ae1f0
namespace s20ae1f0 {
extern "C" {
/* The scene's music track, unk_0c8; 5 with no scene. */
int func_ov004_020ae1f0(void) {
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene != 0) return scene->unk_0c8;
    return 5;
}
}
}

// @symbol func_ov004_020ae20c
namespace s20ae20c {
extern "C" {
extern void _ZN5Sound7PlaySubEjjj5Fix12IiEb(unsigned int a, unsigned int b, unsigned int c, int d, int e);
extern int func_ov004_020ae1cc(void);
extern int func_ov004_020ae1f0(void);
/* Sound::PlaySub with third argument 0 and its flag set starts nothing:
   it only sets the music volume target (data_0208e42c, which
   ChangeMusicVolume and PauseMusic also write) back to 0x7f, with step
   0x7f000, and zeroes data_0209b470. Then, while a scene exists and in
   music mode 1, this loads the scene's track. PlaySub stays mangled:
   nothing in the tree mangles to its Fix12<int> parameter (see
   include/Sound.h). */
void func_ov004_020ae20c(void)
{
    _ZN5Sound7PlaySubEjjj5Fix12IiEb(0, 0x7f, 0, 0x7f000, 1);
    if (data_ov004_020beb68 == 0) return;
    if (func_ov004_020ae1cc() != 1) return;
    Sound::LoadAndSetMusic_Layer1(func_ov004_020ae1f0());
}
}
}

// @symbol func_ov004_020ae274
namespace s20ae274 {
extern "C" {
extern int func_ov004_020ae1cc(void);
extern void _ZN5Sound7PlaySubEjjj5Fix12IiEb(unsigned int, unsigned int, unsigned int, int, int);

/* While a scene exists, starts sequence `id` on Sound::PlaySub's own
   player (data_0209b4b0) unless it is already the current one, and sets
   the music volume target to 0x40 [GUESS: ducks the music; PauseMusic uses
   0x40 too]. The func_ov004_020ae1cc result is not used. */
void func_ov004_020ae274(void* id) {
    if (data_ov004_020beb68 == 0) return;
    func_ov004_020ae1cc();
    _ZN5Sound7PlaySubEjjj5Fix12IiEb((unsigned int)id, 0x40, 0x7f, 0x7f000, 1);
}
}
}

// @symbol func_ov004_020ae2c8
namespace s20ae2c8 {
extern "C" {
int func_ov004_020ae1cc(void);
int func_ov004_020ae1f0(void);
/* Music mode 0 stops the music; mode 1 plays the scene's track. */
void func_ov004_020ae2c8(void) {
    if (data_ov004_020beb68 == 0) return;
    int r = func_ov004_020ae1cc();
    if (r == 0) {
        Sound::StopLoadedMusic_Layer1(1);
        return;
    }
    if (r != 1) return;
    Sound::LoadAndSetMusic_Layer1(func_ov004_020ae1f0());
}
}
}

// @symbol func_ov004_020ae330
namespace s20ae330 {
extern "C" {
extern int func_ov004_020ae1cc(void);
extern int func_ov004_020ae1f0(void);
extern unsigned short data_0209b308;

/* Music mode 0 stops the music, and replays the scene's track only when
   the menu row's first halfword is 0x16f; any nonzero mode plays the
   track. */
void func_ov004_020ae330(void){
    if(data_ov004_020beb68==0) return;
    if(func_ov004_020ae1cc()==0){
        Sound::StopLoadedMusic_Layer1(1);
        if(data_0209b308 != 0x16f) return;
        Sound::LoadAndSetMusic_Layer1(func_ov004_020ae1f0());
        return;
    }
    Sound::LoadAndSetMusic_Layer1(func_ov004_020ae1f0());
}
}
}

namespace s20ae3b4 {
extern "C" {
// @symbol _ZN11dScMgBase_c9Virtual88Eiiii
/* dScMgBase_c::Virtual88 - slot 34.  THE BRUSH.

   Walks a `size` x `size` square centred on (cx, cy) and paints every cell
   inside it with palette index `colour`.  The address arithmetic is the DS
   4bpp background-character layout: `(x/8 + (y/8)*32)*32 + (y&7)*4` picks the
   word holding eight pixels of one row of one 8x8 tile, `(x&7)*4` is the
   nibble inside it, and MultiCopy_Int is used for the read and the write-back
   rather than a plain load/store.  Sixteen colours, one nibble per pixel.  All
   four sides clip.

   It reads exactly the two object fields slot 33 (Virtual84) initialises.
   `obj+0x6c` is the layer index: 0..3 select G2S::GetBG0CharPtr through
   GetBG3CharPtr, and anything else returns without drawing -- which is what
   slot 33's -1 buys.  `obj+0x68` gates the wrapped region above the touch
   screen, where the MAIN engine's G2::GetBG*CharPtr are used instead and y is
   folded by `+ data_ov004_020beb6c + 0xc0`.  So slot 33 leaves the brush
   disabled and a minigame arms it by choosing a layer.

   Its only in-family caller is ov004:0x020ae5c4, the line rasteriser sitting
   immediately after it in the image, which dispatches through +0x88 at seven
   separate sites as it walks a segment.  See the slot-34 block in
   include/dScMgBase_c.h for the arity and return-type measurements.

   `Virtual88` is a placeholder after the +0x88 vtable offset, not a ROM name --
   but unlike slots 29-32 there is no wrong name being retired here, because
   the recovery pass never guessed one for this address. */
extern "C" {
extern int* func_02054efc(void);
extern int* func_02054ea8(void);
extern int* _ZN2G213GetBG2CharPtrEv(void);
extern int* func_02054d88(void);
extern int* _ZN3G2S13GetBG0CharPtrEv(void);
extern int* _ZN3G2S13GetBG1CharPtrEv(void);
extern int* _ZN3G2S13GetBG2CharPtrEv(void);
extern int* _ZN3G2S13GetBG3CharPtrEv(void);
extern void MultiCopy_Int(void* a, void* b, int n);
}

extern "C" {
extern int data_ov004_020beb64;
extern int data_ov004_020beb6c;
}
}
}

void dScMgBase_c::Virtual88(int cx, int cy, int colour, int size)
{ using s20ae3b4::MultiCopy_Int; using s20ae3b4::_ZN2G213GetBG2CharPtrEv; using s20ae3b4::_ZN3G2S13GetBG0CharPtrEv; using s20ae3b4::_ZN3G2S13GetBG1CharPtrEv; using s20ae3b4::_ZN3G2S13GetBG2CharPtrEv; using s20ae3b4::_ZN3G2S13GetBG3CharPtrEv; using s20ae3b4::data_ov004_020beb64; using s20ae3b4::data_ov004_020beb6c; using s20ae3b4::func_02054d88; using s20ae3b4::func_02054ea8; using s20ae3b4::func_02054efc;
    char *obj = (char *)this;

    int oi;
    int x0;
    int ci0;
    int m;
    int xx;
    int yy;
    int ci;
    int yrow;
    int four;
    int half;

    half = size / 2;
    oi = 0;
    if (size <= 0)
        return;
    x0 = cx - half;
    ci0 = oi;
    m = 0xf;
    yrow = cy - half;
    four = 4;
    do {
        ci = ci0;
        if (size > 0) {
            do {
                yy = yrow;
                xx = ci + x0;
                int* base;
                int* cell;
                int sh;

                if (xx < 0)
                    goto inc;
                if (xx >= 0x100)
                    goto inc;
                if (yrow < -0xc0 - data_ov004_020beb6c)
                    goto inc;
                if (yrow < -data_ov004_020beb6c) {
                    if (*(u8*)(obj + 0x68) == 0)
                        goto inc;
                    switch (*(int*)(obj + 0x6c)) {
                    case 0: base = func_02054efc(); break;
                    case 1: base = func_02054ea8(); break;
                    case 2: base = _ZN2G213GetBG2CharPtrEv(); break;
                    case 3: base = func_02054d88(); break;
                    default: return;
                    }
                    yy += data_ov004_020beb6c + 0xc0;
                } else {
                    if (yrow < 0)
                        goto inc;
                    if (yrow >= 0xc0)
                        goto inc;
                    switch (*(int*)(obj + 0x6c)) {
                    case 0: base = _ZN3G2S13GetBG0CharPtrEv(); break;
                    case 1: base = _ZN3G2S13GetBG1CharPtrEv(); break;
                    case 2: base = _ZN3G2S13GetBG2CharPtrEv(); break;
                    case 3: base = _ZN3G2S13GetBG3CharPtrEv(); break;
                    default: return;
                    }
                }
                cell = (int*)((char*)base + ((xx / 8 + (yy / 8) * 32) * 32) + ((yy & 7) * 4));
                MultiCopy_Int(cell, &data_ov004_020beb64, four);
                sh = (xx & 7) << 2;
                data_ov004_020beb64 = (data_ov004_020beb64 & (-1 ^ (m << sh))) | (colour << sh);
                MultiCopy_Int(&data_ov004_020beb64, cell, four);
            inc:;
            } while (++ci < size);
        }
        yrow += 1;
        oi += 1;
    } while (oi < size);
}

// @symbol func_ov004_020ae5c4
namespace s20ae5c4 {
extern "C" {
/* Minigame canvas line rasteriser (ov004 0x020ae5c4, 0x294 bytes). Walks a
 * Bresenham line from (x0, y0) to (x1, y1) and plots every point through the
 * canvas's virtual dScMgBase_c::Virtual88(x, y, colour, size) (vtable slot 34),
 * stepping along the major axis with the error term seeded at half the major
 * span; a zero-length line plots once. Callers: the trampoline minigame's
 * trajectory trace (src/minigames/d_s_mg_trampoline.cpp).
 *
 * Written against include/dScMgBase_c.h so the plot call is the real virtual
 * call: the ROM's ldr rN,[r0]; ldr rN,[rN,#0x88]; blx chain with the fifth
 * argument stored to the stack first is what the header's declaration
 * produces; a function-pointer temp evaluated before the call put the vtable
 * load before the stack argument and loaded it into a separate register.
 * The end row is copied into a local (yy1) that the vertical loop compares
 * against, the ROM's [sp, #0xc] slot; the horizontal loop compares x1 itself.
 * The two absolute values must be spelled as the copies first and the two
 * sign tests after (adx = dx; ady = dy; if (adx < 0) ...; if (ady < 0) ...):
 * that order colours dx/dy into r6/fp and lets the x-span test reuse the
 * flags of the dx subtraction (subs + rsbmi) while the y-span gets its own
 * cmp; testing them in the other order, or each right after its copy, swaps
 * the pair or the flag reuse. */

extern "C" void func_ov004_020ae5c4(dScMgBase_c *thiz, int x0, int y0, int x1, int y1, int colour, int size)
{
    int dx, dy, adx, ady, err;
    int yy1;
    if (x0 == x1 && y0 == y1) {
        thiz->Virtual88(x0, y0, colour, size);
        return;
    }
    yy1 = y1;
    thiz->Virtual88(x0, y0, colour, size);
    dy = y1 - y0;
    dx = x1 - x0;
    adx = dx;
    ady = dy;
    if (adx < 0) adx = -adx;
    if (ady < 0) ady = -ady;
    if (adx >= ady) {
        err = adx / 2;
        for (;;) {
            if (dx == 0) {
                thiz->Virtual88(x0, y0, colour, size);
                return;
            }
            if (dx > 0) {
                err += ady;
                x0++;
                if (err > adx) {
                    if (dy >= 0) y0++; else y0--;
                    err -= adx;
                }
                thiz->Virtual88(x0, y0, colour, size);
                if (x0 == x1) return;
            } else {
                err += ady;
                x0--;
                if (err > adx) {
                    if (dy >= 0) y0++; else y0--;
                    err -= adx;
                }
                thiz->Virtual88(x0, y0, colour, size);
                if (x0 == x1) return;
            }
        }
    } else {
        err = ady / 2;
        for (;;) {
            if (dy == 0) {
                thiz->Virtual88(x0, y0, colour, size);
                return;
            }
            if (dy > 0) {
                err += adx;
                y0++;
                if (err > ady) {
                    if (dx >= 0) x0++; else x0--;
                    err -= ady;
                }
                thiz->Virtual88(x0, y0, colour, size);
                if (y0 == yy1) return;
            } else {
                err += adx;
                y0--;
                if (err > ady) {
                    if (dx >= 0) x0++; else x0--;
                    err -= ady;
                }
                thiz->Virtual88(x0, y0, colour, size);
                if (y0 == yy1) return;
            }
        }
    }
}
}
}

#pragma push
#pragma opt_common_subs off
// @symbol func_ov004_020ae858
namespace s20ae858 {
extern "C" {
extern "C" {
int func_ov004_020b8f18(void *c);
int GetGameLanguage(void);
void func_ov004_020aea30(void *self, int a1, int a2, int a3);
}

extern int *data_ov004_020bc020[];
extern int *data_ov004_020bbfbc[];
extern int *data_ov004_020bbfd0[];
extern s16 data_02082214[];
extern int data_ov006_021346bc;

/* Draws the three-item overlay menu: each item's per-language sprite list
   at its mMenuItemNX/Y, followed by data_ov006_021346bc at the same spot.
   The highlighted item (mMenuCursor) goes through a matrix whose diagonal
   is func_02053200 of a pulse that mMenuCursorPhase drives through the
   sin/cos table data_02082214. Nothing is drawn while
   func_ov004_020b8f18 returns nonzero for mTouchOptions. */
extern "C" void func_ov004_020ae858(dScMgBase_c *scene)
{
    int extra[3];
    s16 *tbl[3];
    int buf[4];
    int zero1;
    int zero2;
    int zero0;
    int i;

    if (func_ov004_020b8f18((void *)&scene->mTouchOptions) != 0)
        return;

    extra[0] = *data_ov004_020bc020[GetGameLanguage()];
    extra[1] = *data_ov004_020bbfbc[GetGameLanguage()];
    extra[2] = *data_ov004_020bbfd0[GetGameLanguage()];

    tbl[0] = &scene->mMenuItem0X;
    tbl[1] = &scene->mMenuItem1X;
    tbl[2] = &scene->mMenuItem2X;

    i = 0;
    zero1 = 0;
    zero2 = 0;
    zero0 = 0;

    for (; i < 3; i++) {
        if (i == scene->mMenuCursor) {
            unsigned int field = (unsigned int)scene->mMenuCursorPhase;
            unsigned int shifted = (field << 0xf) >> 0x10;
            int idx0 = (int)shifted >> 4;
            int s0 = data_02082214[idx0 * 2];
            char *cursor = (char *)extra[i];
            int arg0;
            int result;
            s16 *pos;
            u16 v;
            int *pbuf = buf;

            arg0 = (int)((((s64)s0 << 11) + 0x800) >> 12) + 0x1000;
            result = func_02053200(arg0);

            {
                volatile int *vp = pbuf;
                vp[0] = 0;
                vp[1] = 0;
                vp[2] = 0;
                vp[3] = 0;
            }
            buf[0] = result;
            buf[3] = result;

            pos = tbl[i];
            do {
                func_ov004_020aea78((void *)cursor, (void *)(int)pos[0], (void *)(int)pos[1], (void *)pbuf);
                v = *(u16 *)(cursor + 6);
                cursor += 8;
            } while (v != 0xffff);

            pos = tbl[i];
            func_ov004_020aea30((void *)data_ov006_021346bc, pos[0], pos[1], zero0);
        } else {
            s16 *pos = tbl[i];
            char *ep = (char *)extra[i];
            func_ov004_020aea30((void *)ep, pos[0], pos[1], zero1);
            func_ov004_020aea30((void *)data_ov006_021346bc, pos[0], pos[1], zero2);
        }
    }
}
}
}
#pragma pop

// @symbol func_ov004_020aea30
namespace s20aea30 {
extern "C" {
/* Draws an OAM sprite list: calls func_ov004_020aea78 on each 8-byte
   entry from `self`, up to and including the one whose attr3 is 0xffff. */
extern "C" void func_ov004_020aea30(void *self, void *actor, void *c2, void *c3)
{
    void *r8 = self;
    void *r7 = actor;
    void *r6 = c2;
    void *r5 = c3;
    u16 sentinel = 0xffff;
    do {
        func_ov004_020aea78(r8, r7, r6, r5);
        u16 v = *(u16*)((char*)r8 + 6);
        r8 = (char*)r8 + 8;
        if (v == sentinel) break;
    } while (1);
}
}
}

// @symbol func_ov004_020aea78
namespace s20aea78 {
extern "C" {
struct M { int _00, _01, _10, _11; };
extern "C" {
extern void func_ov004_020b1c68(void* a0, int a1, int a2, int a3, int a4, struct M* a5);
}

struct BF { unsigned int lo:10; unsigned int hi:22; };
struct S { int x; BF y; };

/* Draws one sprite-list entry at (a1, a2) through func_ov004_020b1c68.
   When the scene's OnHitByCannonBlastedChar (slot 26) returns 2, the copy
   drawn has 0x200 subtracted from its tile number (attr2 bits 0..9). */
extern "C" void func_ov004_020aea78(S* self, int a1, int a2, struct M* a3)
{
    S v = *self;
    if (((dScMgBase_c *)data_ov004_020beb68)->OnHitByCannonBlastedChar() == 2) {
        v.y.lo = (self->y.lo - 0x200) & 0x3ff;
    }
    func_ov004_020b1c68(&v, a1, a2, 0, -1, a3);
}
}
}

// @symbol func_ov004_020aeb24
namespace s20aeb24 {
extern "C" {
extern "C" {
extern void func_02012790(int a);
extern int func_ov004_020b8f78(char* p);
extern void func_0203b958(short* o, short* a, short* b);
extern void func_02012dd0(int a);
extern void func_ov004_020b9220(char* p);

extern unsigned short data_020a0e5a[];
extern dWipe_c data_0209f61c;

/* The overlay menu's frame while it is open: slides the items in and, on
   a new touch with nothing highlighted, highlights the touched item in
   mMenuCursor (within 0x60 x 0x18 of item 0, 0x40 x 0x13 of the others, by
   func_0203b958's offsets) and sets the s16 timer at +0x4644 (0x10 for
   item 0, else 0). Once mMenuCursorPhase reaches 0x10000 and that timer
   reaches 0, item 0 (and any other value) calls OnHitFromUnderneath
   (slot 28), item 1 starts a scene fade, and item 2 calls
   func_ov004_020b9220 on the touch options and clears the highlight. */
void func_ov004_020aeb24(char* c)
{
    dScMgBase_c *self = (dScMgBase_c *)c;
    unsigned char idx = gActivePlayerSlot;
    unsigned short flags = data_020a0e5a[idx * 2];
    int unlocked;
    short tmp[2];
    short d0[2];
    short d1[2];
    short d2[2];
    short a, b;

    if ((flags & 0x40) || (flags & 0x80) || (flags & 0x20) || (flags & 0x10)) {
        func_02012790(0xe);
    }
    if (func_ov004_020b8f78((char *)&self->mTouchOptions) != 0) return;

    ApproachLinear2(self->mMenuItem0X, 0x80, 0x10);
    ApproachLinear2(self->mMenuItem1X, 0x80, 0x10);
    ApproachLinear2(self->mMenuItem2Y, 0x90, 5);

    idx = gActivePlayerSlot;
    unlocked = 0;
    if (gTouchHeld[idx * 4] != 0) {
        unlocked = (gTouchEdge[idx * 4] != 0);
    }
    if (unlocked == 0) goto after;
    if (self->mMenuCursor >= 0) goto after;
    tmp[0] = gTouchX[idx * 4];
    tmp[1] = gTouchY[idx * 4];
    func_0203b958(d0, tmp, &self->mMenuItem0X);
    func_0203b958(d1, tmp, &self->mMenuItem1X);
    func_0203b958(d2, tmp, &self->mMenuItem2X);
    a = d0[0]; if (a < 0) a = -a;
    if (a < 0x60) {
        b = d0[1]; if (b < 0) b = -b;
        if (b < 0x18) {
            func_02012790(0x62);
            self->mMenuCursor = 0;
            *(short*)(c + 0x4644) = 0x10;
            goto after;
        }
    }
    a = d1[0]; if (a < 0) a = -a;
    if (a < 0x40) {
        b = d1[1]; if (b < 0) b = -b;
        if (b < 0x13) {
            func_02012790(0x63);
            self->mMenuCursor = 1;
            *(short*)(c + 0x4644) = 0;
            goto after;
        }
    }
    a = d2[0]; if (a < 0) a = -a;
    if (a < 0x40) {
        b = d2[1]; if (b < 0) b = -b;
        if (b < 0x13) {
            func_02012790(0x62);
            self->mMenuCursor = 2;
            *(short*)(c + 0x4644) = 0;
        }
    }

after:
    if (self->mMenuCursor < 0) return;
    if (ApproachLinear(self->mMenuCursorPhase, 0x10000, 0x1000) == 0) return;
    if (ApproachLinear2(*(short*)(c + 0x4644), 0, 1) == 0) return;

    switch (self->mMenuCursor) {
    case 0:
        self->OnHitFromUnderneath();
        return;
    case 1:
        dScene_c::SetFaders(&data_0209f61c);
        data_0209f61c.func_0202ec9c(1);
        dScene_c::StartSceneFade(5, 0, 0);
        if (self->unk_4648 != 0) return;
        func_02012dd0(0x3c);
        self->unk_4648 = 1;
        return;
    case 2:
        func_ov004_020b9220((char *)&self->mTouchOptions);
        self->mMenuCursor = -1;
        self->mMenuCursorPhase = 0;
        return;
    default:
        self->OnHitFromUnderneath();
        return;
    }
}
}
}
}

namespace s20aeed8 {
extern "C" {
// @symbol _ZN11dScMgBase_c25OnAimedAtWithEggReturnVecEv
// recovered name: dScMgBase_c_OnAimedAtWithEggReturnVec
/* dScMgBase_c::OnAimedAtWithEggReturnVec - slot 30.

   WHAT IT DOES, because the name says the opposite of both halves.  This is
   the exact mirror of slot 29: 29 saves the screen and draws the three-item
   overlay menu over it, and this restores everything 29 saved.  POWCNT1's
   screen-swap bit comes back out of mSavedScreenSwap; both DISPCNTs get their
   layer-enable fields back from data_0209d460 / data_0209d458 and then their
   BG-enable fields back from mSavedMainBgBits / mSavedSubBgBits; 0x400 bytes
   of BG palette come back out of the buffer at +0x4228, the second 0x200 of it
   to the sub screen; and 0x2000 bytes of sub-screen OBJ VRAM go back to
   0x06606000 from +0x2228.  dScMgBase_c::OnKicked calls this one when
   mMenuOpen falls to zero and slot 29 when it rises.  It is the menu coming
   down.  It is not a Yoshi egg, and it does not return a vector -- see the
   slot-30 block in include/dScMgBase_c.h for the AAPCS measurement that
   refutes the `ReturnVec` in the name. */
extern "C" u8 data_0209d45c;
extern "C" u8 data_0209d454;
extern "C" u8 data_0209d460;
extern "C" u8 data_0209d458;

extern "C" int _ZN2GX10LoadBGPlttEPKvjj(const void* p, u32 a, u32 b);
extern "C" int _ZN3GXS10LoadBGPlttEPKvjj(const void* p, u32 a, u32 b);

/* RETURN TYPE, MEASURED -- and it corrects the header's earlier `int`, which
   that slot's own evidence block called "a HINT".  Declaring this member `int`
   and letting control fall off the end does not cost an instruction, but it
   does reserve r0 as the result register, and the closing block wants four
   scratch registers.  mwcc then allocates r1-r4 where the ROM allocates
   r0-r3, and fourteen of the function's ninety-three words differ with no
   other change:

       ROM   ldr r0,[pc,#0x38] / mov r3,#0x4000000 / ldr r1,[r3] / ...
       int   ldr r1,[pc,#0x38] / mov r4,#0x4000000 / ldr r2,[r4] / ...

   Spelled `void` it is byte-exact.  Both overrides and decl_common.h:2321
   already said void; the retail source did too. */
}
}

void dScMgBase_c::OnAimedAtWithEggReturnVec()
{ using s20aeed8::_ZN2GX10LoadBGPlttEPKvjj; using s20aeed8::_ZN3GXS10LoadBGPlttEPKvjj; using s20aeed8::data_0209d454; using s20aeed8::data_0209d458; using s20aeed8::data_0209d45c; using s20aeed8::data_0209d460;
    char *c = (char *)this;

    data_0209d45c = 0;
    data_0209d454 = 0;

    *(u32*)0x4000000 &= ~0x1f00;
    *(u32*)0x4001000 &= ~0x1f00;
    *(u16*)0x4000304 = (mSavedScreenSwap << 15) | (*(u16*)0x4000304 & ~0x8000);
    *(u32*)0x4000000 = (*(u32*)0x4000000 & ~0xe000) | ((u32)data_0209d460 << 13);
    *(u32*)0x4001000 = (*(u32*)0x4001000 & ~0xe000) | ((u32)data_0209d458 << 13);

    _ZN2GX10LoadBGPlttEPKvjj(c + 0x4228, 0x20, 0x1e0);
    {
        char *p = c + 0x4228; p += 0x200;
        _ZN3GXS10LoadBGPlttEPKvjj(p, 0, 0x80);
    }

    if (OnHitByCannonBlastedChar() != 2)
    {
        char *src = (char*)0x6600000; src += 0x6000;
        MultiCopy_Int((int*)(c + 0x2228), (int*)src, 0x2000);
    }

    data_0209d45c = (u8)mSavedMainBgBits;
    data_0209d454 = (u8)mSavedSubBgBits;

    if (OnHitByCannonBlastedChar() == 2)
        return;

    *(u32*)0x4000000 = (*(u32*)0x4000000 & ~0x1f00) | ((u32)data_0209d45c << 8);
    *(u32*)0x4001000 = (*(u32*)0x4001000 & ~0x1f00) | ((u32)data_0209d454 << 8);
}

namespace s20af04c {
extern "C" {
// @symbol _ZN11dScMgBase_c19OnHitFromUnderneathEv
// recovered name: dScMgBase_c_OnHitFromUnderneath
/* dScMgBase_c::OnHitFromUnderneath - slot 28.

   A hit from underneath closes the three-item overlay menu
   that OnHitByMegaChar opens, resets the polymorphic touch-icon set, and
   re-enables the 3D engines only if the class's own OnHitByCannonBlastedChar
   says so. */

extern "C" void func_02012e1c(char *c);
extern "C" void Enable3dEngines();

/* THE EARLY EXITS ARE SPELT AS NESTED IFS, NOT `return;`. mwccarm accepts a
   valueless `return` in a non-void function; C++ does not, and no host option
   reaches it (MSVC C2561). The ROM sets no return value on these paths -- it
   leaves r0 holding whatever the last call left there and branches straight to
   the epilogue -- so the faithful shape is a body that reaches its closing
   brace with nothing to return, which is what the host already accepts for the
   rest of this family. Byte-identical under 2004/b56: the compiled object is
   unchanged. */
}
}

int dScMgBase_c::OnHitFromUnderneath()
{ using s20af04c::Enable3dEngines; using s20af04c::func_02012e1c;
    func_02012e1c((char *)this);
    mMenuOpen = 0;
    func_ov004_020b91fc((char *)&mTouchOptions);
    int r = OnHitByCannonBlastedChar();
    if (r != 0) Enable3dEngines();
}

namespace s20af094 {
extern "C" {
// @symbol _ZN11dScMgBase_c16OnAimedAtWithEggEv
// recovered name: dScMgBase_c_OnAimedAtWithEgg
/* dScMgBase_c::OnAimedAtWithEgg - slot 29.

   The two buffers at +0x2228 and +0x4228 stay as raw offsets: they are
   inside pad_228, and this body is the only evidence for their extent.

   WHAT IT DOES, worth writing down because the NAME DOES NOT SAY IT.
   `OnAimedAtWithEgg` is transplanted from include/dActor_c.h at the same slot
   index; nothing in the cartridge carries a method name for this class.  The
   body saves the screen and then draws over it.  It stashes POWCNT1's
   screen-swap bit and the two BG-enable bytes into the object, clears every
   layer- and window-enable bit in both DISPCNTs, saves 0x400 bytes of BG
   palette out of palette RAM into +0x4228 and loads the menu's palettes over
   the top, then saves 0x2000 bytes of sub-screen OBJ VRAM at 0x06606000 into
   +0x2228 and decompresses a per-language image over that.  Its one caller in
   this module is dScMgBase_c::OnKicked, which reaches it only on a rising edge
   of mMenuOpen.  That is the three-item overlay menu going up, not a Yoshi egg.
   See the slot 29 block in include/dScMgBase_c.h for why the name is kept. */
extern "C" void _ZN4CP1527FlushAndInvalidateDataCacheEjj(void *addr, unsigned int size);
extern "C" void _ZN2GX10LoadBGPlttEPKvjj(const void *src, unsigned int offset, unsigned int size);
extern "C" void _ZN3GXS10LoadBGPlttEPKvjj(const void *src, unsigned int offset, unsigned int size);
extern "C" void MultiStore16(unsigned short val, char *dst, int nbytes);
extern "C" void func_0201f32c(int arg0);
extern "C" int GetGameLanguage(void);
extern "C" void DecompressLZ16(void *src, void *dst);

extern "C" unsigned char data_0209d45c;
extern "C" unsigned char data_0209d454;

/* THE EARLY EXITS ARE SPELT AS NESTED IFS, NOT `return;`. mwccarm accepts a
   valueless `return` in a non-void function; C++ does not, and no host option
   reaches it (MSVC C2561). The ROM sets no return value on these paths -- it
   leaves r0 holding whatever the last call left there and branches straight to
   the epilogue -- so the faithful shape is a body that reaches its closing
   brace with nothing to return, which is what the host already accepts for the
   rest of this family. Byte-identical under 2004/b56: the compiled object is
   unchanged. */
}
}

int dScMgBase_c::OnAimedAtWithEgg()
{ using s20af094::DecompressLZ16; using s20af094::GetGameLanguage; using s20af094::MultiStore16; using s20af094::_ZN2GX10LoadBGPlttEPKvjj; using s20af094::_ZN3GXS10LoadBGPlttEPKvjj; using s20af094::_ZN4CP1527FlushAndInvalidateDataCacheEjj; using s20af094::data_0209d454; using s20af094::data_0209d45c; using s20af094::func_0201f32c;
    char *c = (char *)this;

    if (OnHitByCannonBlastedChar() != 0)
        func_02019028();

    mSavedScreenSwap = (*(volatile u16 *)0x4000304 & 0x8000) >> 15;

    mSavedMainBgBits = data_0209d45c;
    mSavedSubBgBits = data_0209d454;

    *(volatile u16 *)0x4000304 |= 0x8000;
    data_0209d45c = 0;
    data_0209d454 = 0;

    *(volatile int *)0x4000000 &= ~0x1f00;
    *(volatile int *)0x4001000 &= ~0x1f00;
    *(volatile int *)0x4000000 &= ~0xe000;
    *(volatile int *)0x4001000 &= ~0xe000;

    MultiCopy_Int((int *)0x5000020, (int *)(c + 0x4228), 0x1e0);
    {
        int *ip4228 = (int *)(c + 0x4228);
        MultiCopy_Int((int *)0x5000400, ip4228 + 0x80, 0x80);
    }
    _ZN4CP1527FlushAndInvalidateDataCacheEjj((void *)(c + 0x4228), 0x400);

    _ZN2GX10LoadBGPlttEPKvjj((const void *)data_ov004_020bea28, 0x20, 0xa0);
    _ZN3GXS10LoadBGPlttEPKvjj((const void *)data_ov004_020beac8, 0, 0x80);

    {
        volatile unsigned short tmp = 0;
        MultiStore16(tmp, (char *)0x5000000, 2);
    }
    {
        volatile unsigned short tmp = 0;
        MultiStore16(tmp, (char *)0x5000400, 2);
    }

    func_0201f32c(mSceneKind);

    data_0209d45c = 0x12;
    data_0209d454 = 0x10;

    if (OnHitByCannonBlastedChar() != 2) {
        int *vbase = (int *)0x6600000;
        int *vram = vbase + 0x1800;
        MultiCopy_Int(vram, (int *)(c + 0x2228), 0x2000);
        int idx = GetGameLanguage();
        DecompressLZ16(data_ov004_020bbf94[idx], vram);
        _ZN4CP1527FlushAndInvalidateDataCacheEjj((void *)(c + 0x2228), 0x2000);
    }
}

namespace s20af27c {
extern "C" {
// @symbol _ZN11dScMgBase_c15OnHitByMegaCharEv
// recovered name: dScMgBase_c_OnHitByMegaChar
/* dScMgBase_c::OnHitByMegaChar - slot 27. Unless unk_4630 is set, opens
   the three-item overlay menu: items start at their slide-in positions,
   the prompt is disabled and nothing is highlighted. */
}
}

void dScMgBase_c::OnHitByMegaChar()
{
    if (unk_4630 != 0) return;
    func_02012e78();
    mMenuItem0X = -0x80;
    mMenuItem0Y = 0x30;
    mMenuItem1X = 0x180;
    mMenuItem1Y = 0x60;
    mMenuItem2X = 0x80;
    mMenuItem2Y = 0xe0;
    mPromptEnabled = 0;
    mMenuOpen = 1;
    mMenuCursor = -1;
    mMenuCursorPhase = 0;
}

// @symbol func_ov004_020af2f8
namespace s20af2f8 {
extern "C" {
extern "C" {
extern int _ZN2G212GetBG0ScrPtrEv(void);
extern void MultiStore16(int val, int ptr, int n);
extern int _ZN3G2S12GetBG0ScrPtrEv(void);
extern int _ZN2G212GetBG1ScrPtrEv(void);
extern int _ZN3G2S12GetBG1ScrPtrEv(void);
extern int _ZN2G212GetBG2ScrPtrEv(void);
extern int _ZN3G2S12GetBG2ScrPtrEv(void);
extern int _ZN2G212GetBG3ScrPtrEv(void);
extern int _ZN3G2S12GetBG3ScrPtrEv(void);
extern int func_02054efc(void);
extern int _ZN3G2S13GetBG0CharPtrEv(void);
extern int func_02054ea8(void);
extern int _ZN3G2S13GetBG1CharPtrEv(void);
extern int _ZN2G213GetBG2CharPtrEv(void);
extern int _ZN3G2S13GetBG2CharPtrEv(void);
extern int _ZN3G2S13GetBG3CharPtrEv(void);
extern void _ZN2GX10LoadBGPlttEPKvjj(const void* p, unsigned int a, unsigned int b);
extern void _ZN3GXS10LoadBGPlttEPKvjj(const void* p, unsigned int a, unsigned int b);

extern char data_ov004_020b9468;

/* Sets up the brush that Virtual88 paints with. Stores `en` at +0x68 and
   the layer `mode` (0..3) at +0x6c, fills the first 0x300 screen entries of
   that sub-screen BG layer (and of the main-screen one when `en` is set)
   with tile numbers 0..0x2ff under palette `base`, zeroes 0x6000 bytes of
   the layer's character data, and loads the 0x20-byte palette
   data_ov004_020b9468 into palette slot `base` on both screens. Any other
   mode returns at once. */
void func_ov004_020af2f8(char* sb, char en, int mode, int base)
{
    volatile unsigned short v0, v1, v2, v3, v4, v5, v6, v7;
    volatile unsigned short w0, w1, w2, w3, w4, w5, w6, w7;
    int val;
    int i;
    int off;
    int t;

    *(unsigned char*)(sb + 0x68) = en;
    i = 0;
    off = i;
    *(int*)(sb + 0x6c) = mode;
    val = i;
    val += base << 12;

    for (; i < 0x300; i++, val++, off += 2) {
        switch (*(int*)(sb + 0x6c)) {
        case 0:
            if (*(unsigned char*)(sb + 0x68)) {
                t = _ZN2G212GetBG0ScrPtrEv();
                t = t + off;
                MultiStore16(v0 = val, t, 2);
            }
            t = _ZN3G2S12GetBG0ScrPtrEv();
            t = t + off;
            MultiStore16(v1 = val, t, 2);
            break;
        case 1:
            if (*(unsigned char*)(sb + 0x68)) {
                t = _ZN2G212GetBG1ScrPtrEv();
                t = t + off;
                MultiStore16(v2 = val, t, 2);
            }
            t = _ZN3G2S12GetBG1ScrPtrEv();
            t = t + off;
            MultiStore16(v3 = val, t, 2);
            break;
        case 2:
            if (*(unsigned char*)(sb + 0x68)) {
                t = _ZN2G212GetBG2ScrPtrEv();
                t = t + off;
                MultiStore16(v4 = val, t, 2);
            }
            t = _ZN3G2S12GetBG2ScrPtrEv();
            t = t + off;
            MultiStore16(v5 = val, t, 2);
            break;
        case 3:
            if (*(unsigned char*)(sb + 0x68)) {
                t = _ZN2G212GetBG3ScrPtrEv();
                t = t + off;
                MultiStore16(v6 = val, t, 2);
            }
            t = _ZN3G2S12GetBG3ScrPtrEv();
            t = t + off;
            MultiStore16(v7 = val, t, 2);
            break;
        default:
            return;
        }
    }

    switch (*(int*)(sb + 0x6c)) {
    case 0:
        if (*(unsigned char*)(sb + 0x68)) {
            t = func_02054efc();
            w0 = 0;
            MultiStore16(w0, t, 0x6000);
        }
        t = _ZN3G2S13GetBG0CharPtrEv();
        w1 = 0;
        MultiStore16(w1, t, 0x6000);
        break;
    case 1:
        if (*(unsigned char*)(sb + 0x68)) {
            t = func_02054ea8();
            w2 = 0;
            MultiStore16(w2, t, 0x6000);
        }
        t = _ZN3G2S13GetBG1CharPtrEv();
        w3 = 0;
        MultiStore16(w3, t, 0x6000);
        break;
    case 2:
        if (*(unsigned char*)(sb + 0x68)) {
            t = _ZN2G213GetBG2CharPtrEv();
            w4 = 0;
            MultiStore16(w4, t, 0x6000);
        }
        t = _ZN3G2S13GetBG2CharPtrEv();
        w5 = 0;
        MultiStore16(w5, t, 0x6000);
        break;
    case 3:
        if (*(unsigned char*)(sb + 0x68)) {
            t = func_02054d88();
            w6 = 0;
            MultiStore16(w6, t, 0x6000);
        }
        t = _ZN3G2S13GetBG3CharPtrEv();
        w7 = 0;
        MultiStore16(w7, t, 0x6000);
        break;
    default:
        return;
    }

    _ZN2GX10LoadBGPlttEPKvjj(&data_ov004_020b9468, base << 5, 0x20);
    _ZN3GXS10LoadBGPlttEPKvjj(&data_ov004_020b9468, base << 5, 0x20);
}
}
}
}

#pragma push
#pragma opt_strength_reduction off
// @symbol func_ov004_020af5e0
namespace s20af5e0 {
extern "C" {
typedef struct Item {
    u32 w0;
    u32 val : 10;
    u32 rest : 22;
} Item;

/* Copies an OAM sprite list (8-byte entries, ending at the one whose
   attr3 is 0xffff) from `src` to `dst`, adding `add` to each tile number
   (the low ten bits of attr2). At most 0x20 entries: the 32nd is given the
   0xffff terminator. Returns the number of entries written. */
int func_ov004_020af5e0(Item* src, Item* dst, int add)
{
    int n = 1;
    int i = 0;
    Item* q = dst;
    do {
        u32 w;
        u32* p;
        dst[i].w0 = src->w0;
        *(u32*)((char*)&dst[i] + 4) = *(u32*)((char*)src + 4);
        w = (add + src->val) & 0x3ff;
        p = (u32*)((char*)q + 4);
        *p = (*p & ~0x3ff) | ((u32)(w) & 0x3ff);
        if (*(u16*)((char*)src + 6) == 0xffff)
            break;
        if (i == 0x1f) {
            *(u16*)((char*)&dst[i] + 6) = 0xffff;
            break;
        }
        i++;
        src++;
        n++;
        q++;
    } while (i < 0x20);
    return n;
}
}
}
#pragma pop

/* OAM::Render is a C++-linkage member of the global OAM namespace, so its
 * declaration sits outside the address namespace and its extern "C" block. */
struct OamAttr; struct Matrix2x2;
namespace OAM {
void Render(bool draw, OamAttr *obj, int px, int py, int pal, int prio, Matrix2x2 *mtx);
void RenderSub(OamAttr *data, s32 x, s32 y, s32 palette, s32 priority);
}

/* The sprite wrappers from here to func_ov004_020b0380 share one gate.
 * With no scene they draw nothing. With the menu closed and the scene's
 * OnHitByCannonBlastedChar (slot 26) returning 2 -- the value
 * dScMgD3DBase_c's body returns -- they read the u16 at +0x4664
 * (dScMgD3DBase_c::unk_4664) and draw once, or not at all. Otherwise they
 * take their normal path. +0x4664 is past dScMgBase_c, so it is read
 * raw: including dScMgD3DBase_c.h renumbers this file's
 * compiler-generated @ labels (@329 becomes @529). The OAM::Render
 * overloads taking a Fix12<int> by value stay mangled (see
 * include/OAM.h). */

// @symbol Hud_RenderSprite
namespace s20af68c {
extern "C" {
extern "C" {
void _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(int, OamAttr*, s32, s32, int, int, int, int, int, int);
void Hud_RenderSprite(void* attr, int x, int y, int palette, int priority){
  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
  if(scene == 0) return;
  if(scene->mMenuOpen == 0){
    if(scene->OnHitByCannonBlastedChar() == 2){
      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) != 0) return;
      _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(0, (OamAttr*)attr, x, y, palette, priority, 0x1000, 0x1000, 0, -1);
      return;
    }
  }
  OAM::RenderSub((OamAttr*)attr, x, y, palette, priority);
}
}
}
}

// @symbol func_ov004_020af770
namespace s20af770 {
extern "C" {
extern "C" {
struct OamAttr;
}
typedef int Fix12i;
extern "C" int _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(
    int show, struct OamAttr* attr, int a, int b, int c, int d, Fix12i e, int f);

extern "C" int func_ov004_020af770(int a0, int a1, int a2, int a3, int a4, int a5, unsigned short a6)
{
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene == 0) return 0;
    if (scene->mMenuOpen == 0) {
        if (scene->OnHitByCannonBlastedChar() == 2) {
            if (*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) != 0) return 0;
            return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(0, (struct OamAttr*)a0, a1, a2, a3, a4, a5, a6);
        }
    }
    return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(1, (struct OamAttr*)a0, a1, a2, a3, a4, a5, a6);
}
}
}

// @symbol func_ov004_020af868
namespace s20af868 {
extern "C" {
void func_ov004_020af868(void* a0, int a1, int a2, int a3, int a4, void* a5){
  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
  if(scene == 0) return;
  if(scene->mMenuOpen == 0){
    if(scene->OnHitByCannonBlastedChar() == 2){
      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) != 0) return;
      OAM::Render(0, (OamAttr*)a0, a1, a2, a3, a4, (Matrix2x2*)a5);
      return;
    }
  }
  OAM::Render(1, (OamAttr*)a0, a1, a2, a3, a4, (Matrix2x2*)a5);
}
}
}

// @symbol func_ov004_020af948
namespace s20af948 {
extern "C" {
void func_ov004_020af948(void* a, int b, int c, void* m)
{
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene == 0) return;
    if (scene->mMenuOpen == 0) {
        if (scene->OnHitByCannonBlastedChar() == 2) {
            if (*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) != 0) return;
            OAM::Render(false, (OamAttr*)a, b, c, -1, -1, (Matrix2x2*)m);
            return;
        }
    }
    OAM::Render(true, (OamAttr*)a, b, c, -1, -1, (Matrix2x2*)m);
}
}
}

// @symbol RenderOamMainScreen
namespace s20afa20 {
extern "C" {
extern "C" {
void _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(int, OamAttr*, s32, s32, int, int, int, int, int, int);

extern "C" void RenderOamMainScreen(int oam, int x, int y, int palette, int priority)
{
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene == 0) return;
    if (scene->mMenuOpen == 0) {
        if (scene->OnHitByCannonBlastedChar() == 2) {
            if (*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) != 1) return;
            _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(0, (OamAttr*)oam, x, y, palette, priority, 0x1000, 0x1000, 0, -1);
            return;
        }
    }
    _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(0, (OamAttr*)oam, x, y, palette, priority, 0x1000, 0x1000, 0, -1);
}
}
}
}

// @symbol func_ov004_020afb20
namespace s20afb20 {
extern "C" {
extern "C" {
struct OamAttr;
}
typedef int Fix12i;
extern "C" int _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(
    int show, struct OamAttr* attr, int a, int b, int c, int d, Fix12i e, int f);

extern "C" int func_ov004_020afb20(int a0, int a1, int a2, int a3, int a4, int a5, unsigned short a6)
{
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene == 0) return 0;
    if (scene->mMenuOpen == 0) {
        if (scene->OnHitByCannonBlastedChar() == 2) {
            if (*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) != 1) return 0;
            return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(0, (struct OamAttr*)a0, a1, a2, a3, a4, a5, a6);
        }
    }
    return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(0, (struct OamAttr*)a0, a1, a2, a3, a4, a5, a6);
}
}
}

// @symbol func_ov004_020afc18
namespace s20afc18 {
extern "C" {
void func_ov004_020afc18(void* a0, int a1, int a2, int a3, int a4, void* a5){
  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
  if(scene == 0) return;
  if(scene->mMenuOpen == 0){
    if(scene->OnHitByCannonBlastedChar() == 2){
      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) != 1) return;
      OAM::Render(0, (OamAttr*)a0, a1, a2, a3, a4, (Matrix2x2*)a5);
      return;
    }
  }
  OAM::Render(0, (OamAttr*)a0, a1, a2, a3, a4, (Matrix2x2*)a5);
}
}
}

// @symbol DrawOamSprite
namespace s20afcf8 {
extern "C" {
extern "C" void DrawOamSprite(void *arg0, void *arg1, int arg2, void *arg3)
{
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene == 0) return;
    if (scene->mMenuOpen == 0 && scene->OnHitByCannonBlastedChar() == 2) {
        if (*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) != 1) return;
        OAM::Render(false, (OamAttr*)arg0, (int)arg1, arg2, -1, -1, (Matrix2x2*)arg3);
        return;
    }
    OAM::Render(false, (OamAttr*)arg0, (int)arg1, arg2, -1, -1, (Matrix2x2*)arg3);
}
}
}

// @symbol func_ov004_020afdd0
namespace s20afdd0 {
extern "C" {
extern "C" {
extern int data_ov004_020beb6c;
int _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(int, void*, int, int, int, int, int, int, int, int);
void func_ov004_020afdd0(void* a0, int a1, int a2, int a3, int a4){
  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
  if(scene == 0) return;
  if(scene->mMenuOpen == 0){
    if(scene->OnHitByCannonBlastedChar() == 2){
      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){
        _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(0, a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, a4, 0x1000, 0x1000, 0, -1);
        return;
      }
      _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(0, a0, a1, a2, a3, a4, 0x1000, 0x1000, 0, -1);
      return;
    }
  }
  _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiES3_ii(0, a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, a4, 0x1000, 0x1000, 0, -1);
  OAM::RenderSub((OamAttr*)a0, a1, a2, a3, a4);
}
}
}
}

// @symbol func_ov004_020aff38
namespace s20aff38 {
extern "C" {
extern "C" {
struct OamAttr;
extern int data_ov004_020beb6c;
}
typedef int Fix12i;
extern "C" int _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(
    int show, struct OamAttr* attr, int a, int b, int c, int d, Fix12i e, int f);

extern "C" int func_ov004_020aff38(struct OamAttr* a0, int a1, int a2, int a3, int a4, Fix12i a5, int a6)
{
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene == 0) return 0;
    if (scene->mMenuOpen == 0 && scene->OnHitByCannonBlastedChar() == 2) {
        if (*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1) {
            int hp = data_ov004_020beb6c;
            if (a2 >= -0x100 - hp && a2 < -hp) {
                return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(0, a0, a1, a2 + 0xc0 + hp, a3, a4, a5, a6);
            }
            if (a2 >= -0x40 && a2 < 0xc0) {
                return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(0, a0, a1, a2, a3, a4, a5, a6);
            }
        }
    } else {
        int hp = data_ov004_020beb6c;
        if (a2 >= -0x100 - hp && a2 < -hp) {
            return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(0, a0, a1, a2 + 0xc0 + hp, a3, a4, a5, a6);
        }
        if (a2 >= -0x40 && a2 < 0xc0) {
            return _ZN3OAM6RenderEbP7OamAttriiii5Fix12IiEi(1, a0, a1, a2, a3, a4, a5, a6);
        }
    }
    return 0;
}
}
}

// @symbol RenderOamBothScreens
namespace s20b0104 {
extern "C" {
extern "C" {
extern int data_ov004_020beb6c;
void RenderOamBothScreens(void* attr, int x, int y, int palette, int priority, void* mtx){
  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
  if(scene == 0) return;
  if(scene->mMenuOpen == 0){
    if(scene->OnHitByCannonBlastedChar() == 2){
      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){
        OAM::Render(false, (OamAttr*)attr, x, y + 0xc0 + data_ov004_020beb6c, palette, priority, (Matrix2x2*)mtx);
        return;
      }
      OAM::Render(false, (OamAttr*)attr, x, y, palette, priority, (Matrix2x2*)mtx);
      return;
    }
  }
  OAM::Render(false, (OamAttr*)attr, x, y + 0xc0 + data_ov004_020beb6c, palette, priority, (Matrix2x2*)mtx);
  OAM::Render(true, (OamAttr*)attr, x, y, palette, priority, (Matrix2x2*)mtx);
}
}
}
}

// @symbol func_ov004_020b023c
namespace s20b023c {
extern "C" {
extern "C" {
extern int data_ov004_020beb6c;
void func_ov004_020b023c(void* a0, int a1, int a2, int a3, void* a4){
  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
  if(scene == 0) return;
  if(scene->mMenuOpen == 0){
    if(scene->OnHitByCannonBlastedChar() == 2){
      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){
        OAM::Render(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, -1, (Matrix2x2*)a4);
        return;
      }
      OAM::Render(0, (OamAttr*)a0, a1, a2, a3, -1, (Matrix2x2*)a4);
      return;
    }
  }
  OAM::Render(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, a3, -1, (Matrix2x2*)a4);
  OAM::Render(1, (OamAttr*)a0, a1, a2, a3, -1, (Matrix2x2*)a4);
}
}
}
}

// @symbol func_ov004_020b0380
namespace s20b0380 {
extern "C" {
extern "C" {
extern int data_ov004_020beb6c;
void func_ov004_020b0380(void* a0, int a1, int a2, void* a3){
  dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
  if(scene == 0) return;
  if(scene->mMenuOpen == 0){
    if(scene->OnHitByCannonBlastedChar() == 2){
      if(*(unsigned short*)((char*)data_ov004_020beb68 + 0x4664) == 1){
        OAM::Render(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, -1, -1, (Matrix2x2*)a3);
        return;
      }
      OAM::Render(0, (OamAttr*)a0, a1, a2, -1, -1, (Matrix2x2*)a3);
      return;
    }
  }
  OAM::Render(0, (OamAttr*)a0, a1, a2 + 0xc0 + data_ov004_020beb6c, -1, -1, (Matrix2x2*)a3);
  OAM::Render(1, (OamAttr*)a0, a1, a2, -1, -1, (Matrix2x2*)a3);
}
}
}
}

// @symbol func_ov004_020b04c0
namespace s20b04c0 {
extern "C" {
extern int data_ov004_020beb6c[];
/* Get and set data_ov004_020beb6c: the offset Virtual88 and the sprite
   wrappers above add, with 0xc0, to y on their upper-screen path. */
int func_ov004_020b04c0(void) { return data_ov004_020beb6c[0]; }
}
}

// @symbol func_ov004_020b04d0
namespace s20b04d0 {
extern "C" {
extern int data_ov004_020beb6c[];
void func_ov004_020b04d0(int v) { data_ov004_020beb6c[0] = v; }
}
}

namespace s20b04e0 {
extern "C" {
// @symbol _ZN11dScMgBase_c24OnHitByCannonBlastedCharEv
// recovered name: dScMgBase_c_OnHitByCannonBlastedChar
/* dScMgBase_c::OnHitByCannonBlastedChar - slot 26.  The recovered name above is
 * this one's own and is correct; the two overrides' were not, and both are
 * corrected where they live.
 *
 * `return 0;` here, `return 1;` in dScMgSingle3DBase_c's override and
 * `return 2;` in dScMgD3DBase_c's.  Three distinct constants out of three
 * bodies is what makes the return type int rather than void, and it is why the
 * caller has to be able to tell the three apart. */
}
}

int dScMgBase_c::OnHitByCannonBlastedChar()
{
    return 0;
}

// @symbol _ZN11dScMgBase_c16OnPendingDestroyEv
void dScMgBase_c::OnPendingDestroy()
{
}

// @symbol _ZN11dScMgBase_c6RenderEv
s32 dScMgBase_c::Render()
{
    return 1;
}

// @symbol _ZN11dScMgBase_c12BeforeRenderEv
/* Draws the menu instead of the scene while it is open. The two loops draw
 * the three UI records in two passes, those with 0x30 clear first. */
int dScMgBase_c::BeforeRender()
{
    int i; char *p; int i2; char *p2; int j;

    if (dScene_c::BeforeRender() == 0)
        return 0;

    if (mMenuOpen != 0) {
        func_ov004_020ae858(this);
        return 0;
    }

    if (mStateController.unk_024 == 0) {
        p = data_ov004_020bf648;
        for (i = 0; i < 3; i++, p += 0x134) {
            if (*(int *)(p + 0x20) == 0x1d) continue;
            if (*(s16 *)(p + 0x30) != 0) continue;
            func_ov004_020b31b4(p);
        }
        p2 = data_ov004_020bf648;
        for (i2 = 0; i2 < 3; i2++, p2 += 0x134) {
            if (*(int *)(p2 + 0x20) == 0x1d) continue;
            if (*(s16 *)(p2 + 0x30) == 0) continue;
            func_ov004_020b31b4(p2);
        }
        mStateController.Render();
    }

    p = (char *)data_ov004_020bebe8;
    for (j = 0; j < 0x40; j++, p += 0x20) {
        if (data_ov004_020bebe8[j].mTimer != 0)
            func_ov004_020add88(p);
    }

    func_ov004_020b0de0(this);
    return 1;
}

// @symbol _ZN11dScMgBase_c8BehaviorEv
s32 dScMgBase_c::Behavior()
{
    return 1;
}

// @symbol _ZN11dScMgBase_c14BeforeBehaviorEv
/* Handles input and the menu, runs the UI state controller and the shared
 * records, approaches unk_0ac toward unk_0a8, and advances the 40-frame
 * counter. Returns 0 to stop the derived scene's behavior this frame. */
int dScMgBase_c::BeforeBehavior()
{
    int mode;
    unsigned short flags;

    if (dScene_c::BeforeBehavior() == 0)
        return 0;

    if (data_0209f5bc->IsActive()) {
        mode = gActivePlayerSlot;
        flags = data_020a0e5a[mode * 2];
        if ((flags & 8) || (flags & 4) || (flags & 1) || (flags & 2)) {
            if (mMenuOpen != 0) {
                if (mMenuCursor < 0) {
                    if (func_ov004_020b8ee0((char *)&mTouchOptions))
                        OnHitFromUnderneath();
                }
            } else {
                if (gTouchHeld[mode * 4] == 0)
                    OnHitByMegaChar();
            }
        }
    }

    if (mMenuOpen != 0) {
        func_ov004_020aeb24((char *)this);
        return 0;
    }
    if (unk_462c != 0)
        return 0;

    if (mStateController.unk_024 == 0) {
        int i;
        char *g;
        mStateController.Behavior();
        g = data_ov004_020bf648;
        for (i = 0; i < 3; i++) {
            if (*(int *)(g + 0x20) != 0x1d)
                func_ov004_020b321c(g);
            g += 0x134;
        }
    }

    {
        int j;
        char *p = (char *)data_ov004_020bebe8;
        Ent *base = data_ov004_020bebe8;
        for (j = 0; j < 0x40; j++) {
            if (base[j].mTimer != 0)
                func_ov004_020adf2c(p);
            p += 0x20;
        }
    }

    if (unk_0a4 == 0) {
        if (data_0209f5bc->IsActive() == 0)
            return 0;
    }

    ApproachLinear(unk_0ac, unk_0a8, data_0208ee44);

    mFrameCounter++;
    if (mFrameCounter >= 0x28)
        mFrameCounter = 0;

    return 1;
}

// @symbol _ZN11dScMgBase_c21AfterCleanupResourcesEj
void dScMgBase_c::AfterCleanupResources(u32 vfSuccess)
{
    if (vfSuccess == 2) {
        if (((int *)data_0209b308)[4] == 0)
            func_ov004_020ad90c();
        data_0209d4a8 = 0;
        data_ov004_020beb74.mScene = 0;
        FreeGfxSlotsById(0x1d);
        if (data_ov004_020beb60 != 0) {
            func_0203cbc0(data_ov004_020beb60);
            data_ov004_020beb60 = 0;
        }
        if (mMenuOpen != 0) {
            func_02012e1c();
            Sound::StopLoadedMusic_Layer1(1);
        }
        func_ov004_020b2c84();
    }
    dScene_c::AfterCleanupResources(vfSuccess);
}

// @symbol _ZN11dScMgBase_c18AfterInitResourcesEj
/* Finishes the display and font setup, then reports to dScene_c. */
void dScMgBase_c::AfterInitResources(u32 vfSuccess)
{
    Virtual80();
    LoadFont(2);
    func_ov004_020ae330();
    dScene_c::AfterInitResources(vfSuccess);
}

// @symbol _ZN11dScMgBase_c19BeforeInitResourcesEv
/* Sets up the scene and both displays. Slots 26, 31 and 33 must stay
 * virtual calls so derived scenes can override them. The block at 0x4000 is
 * padding in the header, so it is still passed by offset. */
bool dScMgBase_c::BeforeInitResources()
{
    if (dScene_c::BeforeInitResources() == 0) return 0;
    if (OnHitByCannonBlastedChar() == 0)
        func_02019028();
    else
        Enable3dEngines();
    unk_0c8 = *(int*)(data_0209b308 + 0x28);
    if (data_ov004_020beb60 == 0)
        data_ov004_020beb60 = _ZN6Memory13operator_new2Ej(0x4000);
    if (data_ov004_020beb68 != 0)
        *(int*)((char*)data_ov004_020beb68 + 0xb0) = 0;
    mHudScore = 0;
    unk_0b8 = 0;
    unk_465c = 0;
    func_ov004_020b8a8c((char *)this + 0x4000);
    Virtual84();
    func_ov004_020b2cb8();
    dScene_c::SetFaders(&data_0209f61c);
    data_0209f61c.func_0202ec9c(0);
    data_0209d460[0] = 0;
    data_0209d458[0] = 0;
    Virtual7C();
    return 1;
}

// @symbol func_ov004_020b0a38
namespace s20b0a38 {
extern "C" {
/* Clears mStateController.unk_024, so BeforeBehavior and BeforeRender
   step and draw the UI state controller again. */
void func_ov004_020b0a38(void)
{
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene == 0) return;
    scene->mStateController.unk_024 = 0;
}
}
}

// @symbol func_ov004_020b0a54
namespace s20b0a54 {
extern "C" {
/* Sets the UI state controller's state and clears unk_060. The singleton
   is read again after SetState, as the ROM does. */
extern "C" void func_ov004_020b0a54(s32 state)
{
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (!scene)
        return;
    scene->mStateController.SetState(state);
    ((dScMgBase_c *)data_ov004_020beb68)->unk_060 = 0;
}
}
}

// @symbol FreeGfxSlotsById
namespace s20b0aa0 {
extern "C" {
extern void func_ov004_020b3194(void *c);
struct E
{
  char pad[0x20];
  int f20;
  char pad2[0x134 - 0x24];
};
extern struct E data_ov004_020bf648[];
/* Calls func_ov004_020b3194 on each of the three 0x134-byte records at
   data_ov004_020bf648 whose word at +0x20 equals `arg` -- or, for 0x1d,
   on each whose word is not 0x1d. */
void FreeGfxSlotsById(int arg)
{
  int i;
  struct E *p;
  i = arg;
  if (i == 0x1d)
  {
    p = data_ov004_020bf648;
    for (i = 0; i < 3; i++)
    {
      if (p->f20 != 0x1d)
      {
        func_ov004_020b3194(p);
      }
      p++;
    }

  }
  else
  {
    p = data_ov004_020bf648;
    for (i = 0; i < 3; i++)
    {
      if (arg == p->f20)
      {
        func_ov004_020b3194(p);
      }
      p++;
    }

  }
}
}
}

// @symbol func_ov004_020b0b1c
namespace s20b0b1c {
extern "C" {

struct Vec2 {
    short x;
    short y;
};

struct Obj004 {
    char pad0[0x10];
    struct Vec2 pos;  /* 0x10 */
    char pad14[0xc];
    int f20;          /* 0x20 */
    char pad24[0xe];
    unsigned short f32;/* 0x32 */
    char pad34[0x100];
};

extern struct Obj004 data_ov004_020bf648[3];
extern int data_ov004_020bc150;

extern void func_ov004_020b422c(struct Obj004 *o);
extern int func_ov004_020b40c0(struct Obj004 *o);

// The ROM scales idx into the addressing mode here, which only the 4-byte-record view of the lane reproduces.
#define TOUCH_REC(lane) ((unsigned char (*)[4])(lane))
int func_ov004_020b0b1c(int arg) {
    int idx = gActivePlayerSlot;
    int flag = 0;
    int i;

    if (TOUCH_REC(gTouchHeld)[idx][0] != 0) {
        if (TOUCH_REC(gTouchEdge)[idx][0] != 0) flag = 1;
    }

    if (flag) {
        struct Obj004 *o = data_ov004_020bf648;
        int cx = TOUCH_REC(gTouchX)[idx][0];
        int cy = TOUCH_REC(gTouchY)[idx][0];
        for (i = 0; i < 3; i++, o++) {
            if (arg == o->f20 && o->f32 != 0) {
                volatile struct Vec2 pos;
                int dx, dy;
                short ty;
                pos.x = o->pos.x;
                ty = o->pos.y;
                dx = pos.x - cx;
                pos.y = ty;
                dy = pos.y - cy;
                if (dx <= 0x40 && dx >= -0x40 && dy <= 0x13 && dy >= -0x13) {
                    if (data_ov004_020bc150 != 0) {
                        func_ov004_020b422c(o);
                        if ((unsigned int)(arg - 1) <= 1) {
                            ((dScMgBase_c *)data_ov004_020beb68)->unk_4630 = 1;
                        }
                    }
                }
            }
        }
    } else {
        int j;
        struct Obj004 *o = data_ov004_020bf648;
        for (j = 0; j < 3; j++, o++) {
            if (arg == o->f20) {
                if (func_ov004_020b40c0(o) != 0) {
                    if (arg == 1) {
                    } else if (arg == 2) {
                    } else {
                        data_ov004_020bc150 = 1;
                    }
                    return 1;
                }
            }
        }
    }
    return 0;
}
}
}

// @symbol func_ov004_020b0cac
namespace s20b0cac {
extern "C" {
struct Elem { char pad[0x134]; };
extern struct Elem data_ov004_020bf648[];
extern void func_ov004_020b3278(struct Elem *e, int c, int a1, int a2, int a3, int s1, short s2);
/* Hands its arguments to func_ov004_020b3278 for the first of the three
   records at data_ov004_020bf648 whose word at +0x20 is 0x1d; does nothing
   when none is. */
void func_ov004_020b0cac(int c, int a1, int a2, int a3, int arg5, short arg6) {
    int i;
    for (i = 0; i < 3; i++) {
        if (*(int*)((char*)&data_ov004_020bf648[i] + 0x20) == 0x1d) {
            func_ov004_020b3278(&data_ov004_020bf648[i], c, a1, a2, a3, arg5, arg6);
            return;
        }
    }
}
}
}

// @symbol func_ov004_020b0d30
namespace s20b0d30 {
extern "C" {
extern char data_ov004_020bf3e8[];

void func_ov004_020b0d30(void) {
    FreeGfxSlotsById(0x1d);
    int i = 0;
    do {
        data_ov004_020bebe8[i].mTimer = 0;
        i++;
    } while (i < 0x40);
    int j = 0;
    char* p = data_ov004_020bf3e8;
    do {
        j++;
        *p++ = 0;
    } while (j < 3);
}
}
}

// @symbol func_ov004_020b0d8c
namespace s20b0d8c {
extern "C" {
extern s32 GetGameLanguage(void);
extern void Hud_RenderSprite(void *fn, s32 a, s32 b, s32 r3arg, s32 stack_arg);
/* Draws per-language sprite word 0x11 or 0x12 at (arg1, arg2): word 0x11
   for the first 20 frames of the scene's 40-frame mFrameCounter cycle,
   word 0x12 for the rest. */
void func_ov004_020b0d8c(void *c, s32 arg1, s32 arg2)
{
    s32 idx1 = ((dScMgBase_c *)c)->mFrameCounter;
    s32 r4 = (idx1 < 0x14) ? 0x11 : 0x12;
    s32 idx2 = GetGameLanguage();
    int *table = data_ov004_020bbfa8[idx2];
    Hud_RenderSprite((void *)table[r4], arg1, arg2, -1, -1);
}
}
}

// @symbol func_ov004_020b0de0
namespace s20b0de0 {
extern "C" {
s32 GetGameLanguage(void);
void RenderOamMainScreen(int a0, int a1, int a2, int a3, int a4);

/* Draws the per-language prompt sprite at (0xc0, 0xb0) while mPromptEnabled
   is set: blinking (shown for the first 0x18 frames of each 0x30-frame
   mPromptBlinkTimer cycle) for four cycles, then every frame. */
void func_ov004_020b0de0(char* c)
{
    dScMgBase_c *self = (dScMgBase_c *)c;
    if (self->mPromptEnabled == 0)
        return;
    if ((unsigned int)self->mPromptBlinkCount < 4U) {
        self->mPromptBlinkTimer++;
        if ((unsigned int)self->mPromptBlinkTimer >= 0x30U) {
            self->mPromptBlinkTimer = 0U;
            self->mPromptBlinkCount++;
        }
        if ((unsigned int)self->mPromptBlinkTimer >= 0x18U)
            return;
    }
    {
        int i = GetGameLanguage();
        RenderOamMainScreen(data_ov004_020bbfa8[i][10], 0xc0, 0xb0, -1, -1);
    }
}
}
}

// @symbol func_ov004_020b0e84
namespace s20b0e84 {
extern "C" {
extern "C" void func_02012790(int);
extern "C" int GetGameLanguage(void);
extern "C" void RenderOamMainScreen(int a0, int a1, int a2, int a3, int a4);

static inline int sdiv4(int d) {
    int t = d + (int)((unsigned)(d >> 1) >> 0x1e);
    return t >> 2;
}

static inline int sdiv2(int e) {
    return (e + (int)((unsigned)e >> 31)) >> 1;
}

/* Draws a tally of `sb` in a column at x 0xf0, upward from y 0xa8: one
   icon (per-language sprite word 9) per unit, and from five upward the
   whole fives as a number (func_ov004_020b1ea4) beside sprite word 11 with
   only the remainder as icons. unk_050 holds the last tally. A rise plays
   sound 0x1bb (func_02012790) and starts the unk_054 counter -- one step a
   frame, two when slot 26 returns 2 -- while the newest icon blinks with a
   period of 60 unk_054 steps (30 frames when slot 26 returns 2); a fall
   stops it. When the tally reaches a multiple of
   five, the icons close up as unk_054 runs from mTimeLimit to
   mTimeLimit + 0x20, with sound 0x1ba as they start. */
extern "C" void func_ov004_020b0e84(dScMgBase_c *sl, int sb) {
    int *r8;
    int r7;
    int i;
    int pin;

    {
        int t = sl->unk_050;
        if (t < sb) {
            sl->unk_054 = 1;
            sl->unk_050 = sb;
            func_02012790(0x1bb);
        } else if (t > sb) {
            sl->unk_054 = 0;
            sl->unk_050 = sb;
        } else if (sl->unk_054 > 0) {
            if (sl->OnHitByCannonBlastedChar() == 2) {
                sl->unk_054 += 2;
            } else {
                sl->unk_054 += 1;
            }
        }
    }

    r8 = data_ov004_020bbfa8[GetGameLanguage()];
    r7 = 0xa8;

    if (sb < 5) {
        r7 -= 8;
        i = 0;
        if (sb > 0) {
            do {
                if (i == sb - 1 && sl->unk_054 > 0 && sl->unk_054 % 0x3c >= 0x1e) {
                    break;
                }
                RenderOamMainScreen(r8[9], 0xf0, r7, -1, -1);
                i += 1;
                r7 -= 0x10;
            } while (i < sb);
        }
        if (sl->unk_054 >= sl->mTimeLimit) {
            sl->unk_054 = 0;
        }
        return;
    }

    if (sb == 5) {
        int u = sl->unk_054;
        if (u == 0) {
            func_ov004_020b1ea4(0xf0, r7, 5, 1, 0, 0, 0x14);
            RenderOamMainScreen(r8[11], 0xf0, r7, -1, -1);
        } else if (u < sl->mTimeLimit) {
            r7 -= 8;
            i = 0;
            do {
                RenderOamMainScreen(r8[9], 0xf0, r7, -1, -1);
                i += 1;
                r7 -= 0x10;
            } while (i < 4);
            if (sl->unk_054 % 0x3c < 0x1e) {
                RenderOamMainScreen(r8[9], 0xf0, r7, -1, -1);
            }
        } else {
            int d = (sl->mTimeLimit + 0x20) - sl->unk_054;
            r7 -= sdiv4(d);
            i = 0;
            do {
                RenderOamMainScreen(r8[9], 0xf0, r7, -1, -1);
                {
                    unsigned e = (sl->mTimeLimit + 0x20) - sl->unk_054;
                    r7 -= sdiv2((int)e);
                }
                i += 1;
            } while (i < 5);
        }
        if (sl->OnHitByCannonBlastedChar() == 2) {
            int b = sl->mTimeLimit;
            int c = sl->unk_054;
            if (c == b || c == b + 1) {
                func_02012790(0x1ba);
            }
        } else if (sl->unk_054 == sl->mTimeLimit) {
            func_02012790(0x1ba);
        }
        if (sl->unk_054 >= sl->mTimeLimit + 0x20) {
            sl->unk_054 = 0;
        }
        return;
    }

    {
        int rem = sb % 5;
        if (rem == 0) {
            int u = sl->unk_054;
            if (u == 0) {
                func_ov004_020b1ea4(0xf0, r7, sb, 1, 0, 0, 0x14);
                RenderOamMainScreen(r8[11], 0xf0, r7, -1, -1);
            } else if (u < sl->mTimeLimit) {
                func_ov004_020b1ea4(0xf0, r7, sb - 5, 1, 0, 0, 0x14);
                RenderOamMainScreen(r8[11], 0xf0, r7, -1, -1);
                r7 -= 0x18;
                i = 0;
                do {
                    if (i == 4 && sl->unk_054 > 0 && sl->unk_054 % 0x3c >= 0x1e) {
                        break;
                    }
                    RenderOamMainScreen(r8[9], 0xf0, r7, -1, -1);
                    i += 1;
                    r7 -= 0x10;
                } while (i < 5);
            } else {
                int d;
                func_ov004_020b1ea4(0xf0, r7, sb - 5, 1, 0, 0, 0x14);
                RenderOamMainScreen(r8[11], 0xf0, r7, -1, -1);
                d = (sl->mTimeLimit + 0x20) - sl->unk_054;
                r7 -= sdiv4(d);
                i = 0;
                do {
                    unsigned e = (sl->mTimeLimit + 0x20) - sl->unk_054;
                    r7 -= sdiv2((int)e);
                    RenderOamMainScreen(r8[9], 0xf0, r7, -1, -1);
                    i += 1;
                } while (i < 5);
            }
            if (sl->OnHitByCannonBlastedChar() == 2) {
                int b = sl->mTimeLimit;
                int c = sl->unk_054;
                if (c == b || c == b + 1) {
                    func_02012790(0x1ba);
                }
            } else if (sl->unk_054 == sl->mTimeLimit) {
                func_02012790(0x1ba);
            }
            if (sl->unk_054 >= sl->mTimeLimit + 0x20) {
                sl->unk_054 = 0;
            }
            return;
        }

        {
            int base5 = (sb / 5) * 5;
            func_ov004_020b1ea4(0xf0, r7, base5, 1, 0, 0, 0x14);
        }
        RenderOamMainScreen(r8[11], 0xf0, r7, -1, -1);
        r7 -= 0x18;
        sb = 0;
        if (rem > 0) {
            pin = rem - 1;
            do {
                if (sb == pin && sl->unk_054 > 0 && sl->unk_054 % 0x3c >= 0x1e) {
                    break;
                }
                RenderOamMainScreen(r8[9], 0xf0, r7, -1, -1);
                sb += 1;
                r7 -= 0x10;
            } while (sb < rem);
        }
        if (sl->unk_054 >= sl->mTimeLimit) {
            sl->unk_054 = 0;
        }
    }
}
}
}

// @symbol func_ov004_020b14f0
namespace s20b14f0 {
extern "C" {
extern int func_ov004_020ad6f4(void* self, int a);
extern int func_ov004_020adc3c(void* self);
extern int func_02013580(int a, int b);
extern void func_ov004_020b1710(void* self, int a1, int a2, int a3);

/* Draws the five saved records in rows 0x18 apart from (0xa8, 0x28),
   revealing one more row every 0x14 frames as unk_060 counts up. unk_060
   then loops over 0x64..0x8b, and the session best (unk_4658) blinks on
   that 40-frame loop: in the row of the rank it took (unk_064, read
   through func_ov004_020ad6f4), or in a row below the five when it placed
   nowhere. func_ov004_020ad6f4 takes only the scene; the second argument
   passed here is unused. */
void func_ov004_020b14f0(char* self)
{
    dScMgBase_c *scene = (dScMgBase_c *)self;
    int a2 = 0x28;
    int n;
    int i;
    int j;
    int k;

    scene->unk_060 += 1;
    if (scene->unk_060 >= 0x8c)
        scene->unk_060 = 0x64;

    n = func_ov004_020ad6f4(self, scene->unk_4658);
    if (n < 0)
        goto neg;

    i = 0;
    if (n > 0)
    {
        do
        {
            if (scene->unk_060 >= i * 0x14)
                func_ov004_020b1710(self, 0xa8, a2, func_02013580(func_ov004_020adc3c(self), i));
            a2 += 0x18;
            i++;
        } while (i < n);
    }

    if (scene->unk_060 >= n * 0x14)
    {
        if (scene->unk_060 < 0x64 || scene->unk_060 - 0x64 < 0x14)
            func_ov004_020b1710(self, 0xa8, a2, scene->unk_4658);
        else
            func_ov004_020b1710(self, 0xa8, a2, -1);
    }

    a2 += 0x18;
    j = n + 1;
    if (j >= 5)
        return;
    do
    {
        if (scene->unk_060 >= j * 0x14)
            func_ov004_020b1710(self, 0xa8, a2, func_02013580(func_ov004_020adc3c(self), j));
        a2 += 0x18;
        j++;
    } while (j < 5);
    return;

neg:
    k = 0;
    do
    {
        if (scene->unk_060 >= k * 0x14)
            func_ov004_020b1710(self, 0xa8, a2, func_02013580(func_ov004_020adc3c(self), k));
        a2 += 0x18;
        k++;
    } while (k < 5);

    if (scene->unk_060 < 0x64)
        return;

    if (scene->unk_060 - 0x64 < 0x14)
        func_ov004_020b1710(self, 0xa8, a2 + 0x10, scene->unk_4658);
    else
        func_ov004_020b1710(self, 0xa8, a2 + 0x10, -1);
}
}
}

// @symbol func_ov004_020b1710
namespace s20b1710 {
extern "C" {
extern int GetGameLanguage(void);
extern void Hud_RenderSprite(void *a0, int a1, int a2, int a3, int a4);

extern int data_0209b308[];

/* Draws one record row at y. In scoring mode 5 (word 2 of the menu row)
   `val` is a time in frames, drawn as seconds, sprite word 7, tenths and
   hundredths ending at x, and a negative val draws nothing. Otherwise
   modes 1, 2 and 4 draw two label sprites at x 0x58 and 0x68 first; mode
   3 then draws val 4 px left through func_ov004_020b1ea4, every other mode
   through func_ov004_020b2444. A negative val draws no number. */
void func_ov004_020b1710(int a0, int x, int y, int val)
{
    int idx = GetGameLanguage();
    int mode = data_0209b308[2];
    int *tbl = data_ov004_020bbfa8[idx];
    int flag = 0;

    if (mode == 5) {
        int major;
        int rem;
        int pct;
        if (val < 0)
            return;
        major = val / 60;
        func_ov004_020b1ea4(x - 0x18, y, major, flag, -1, 1, 0xa);
        Hud_RenderSprite((void *)tbl[7], x - 0x10, y, -1, -1);

        rem = val % 60;
        pct = rem * 100;
        func_ov004_020b1ea4(x - 8, y, pct / 600, 0, -1, 1, 0xa);

        rem = val % 60;
        pct = rem * 100 / 60;
        func_ov004_020b1ea4(x, y, pct % 10, 0, -1, 1, 0xa);
        return;
    }

    if (mode == 3)
        flag = 1;
    switch (mode) {
    case 1:
        Hud_RenderSprite((void *)tbl[9], 0x58, y, -1, -1);
        Hud_RenderSprite((void *)tbl[1], 0x68, y, -1, -1);
        break;
    case 2:
        Hud_RenderSprite((void *)tbl[2], 0x58, y, -1, -1);
        Hud_RenderSprite((void *)tbl[1], 0x68, y, -1, -1);
        break;
    case 3:
        flag = 1;
        break;
    case 4:
        Hud_RenderSprite((void *)tbl[0x13], 0x58, y, -1, -1);
        Hud_RenderSprite((void *)tbl[1], 0x68, y, -1, -1);
        break;
    default:
        break;
    }

    if (flag == 1)
        x -= 4;
    if (val < 0)
        return;
    if (flag == 1)
        func_ov004_020b1ea4(x, y, val, 0, -1, 1, 0xa);
    else
        func_ov004_020b2444(x, y, val, 0, -1, 1, 0);
}
}
}

// @symbol func_ov004_020b19f0
namespace s20b19f0 {
extern "C" {
s32 GetGameLanguage(void);
int RenderOamMainScreen(int a, int b, int c, int d, int e);
int func_ov004_020b1ea4(int a, int b, void* c, int d, int e, int f, int g);
/* Draws sprite word 8 at (0x20, 0xc) and `value`, starting at x
   0x48, through func_ov004_020b1ea4. */
int func_ov004_020b19f0(void* value){
  int i = GetGameLanguage();
  RenderOamMainScreen(data_ov004_020bbfa8[i][8], 0x20, 0xc, -1, -1);
  return func_ov004_020b1ea4(0x48, 0xc, value, 1, -1, 2, 0xa);
}
}
}

// @symbol func_ov004_020b1a5c
namespace s20b1a5c {
extern "C" {
s32 GetGameLanguage(void);
int RenderOamMainScreen(int a, int b, int c, int d, int e);
int func_ov004_020b1ea4(int a, int b, int c, int d, int e, int f, int g);
extern int data_ov004_020b944c[];
/* Draws a0, capped at data_ov004_020b944c[a1], right-aligned at (0xf4,
   0xc), and sprite word 4 to its left at an x that a1 sets. */
void func_ov004_020b1a5c(int a0, int a1){
  int lim = data_ov004_020b944c[a1];
  int c = a0;
  int r4 = 0xf4;
  int i;
  if (c >= lim) c = lim;
  func_ov004_020b1ea4(0xf4, 0xc, c, 1, -1, 1, 0xa);
  r4 = r4 - (0x44 - (6 - a1) * 8);
  i = GetGameLanguage();
  RenderOamMainScreen(data_ov004_020bbfa8[i][4], r4, 0xc, -1, -1);
}
}
}

// @symbol func_ov004_020b1aec
namespace s20b1aec {
extern "C" {
/* The scene's unk_0bc; 0 with no scene. */
int func_ov004_020b1aec(void) {
    dScMgBase_c *scene = (dScMgBase_c *)data_ov004_020beb68;
    if (scene != 0) return scene->unk_0bc;
    return 0;
}
}
}

// @symbol func_ov004_020b1b08
namespace s20b1b08 {
extern "C" {
extern void func_ov004_020b1b78(void *c, int val);
/* Subtracts `amount` from the scene's unk_0a8 (func_ov004_020b1b78). Most
   callers declare the amount as a void *, so it stays one here. */
void func_ov004_020b1b08(void *amount) {
    void *v = data_ov004_020beb68;
    if (v == 0) return;
    func_ov004_020b1b78(v, (int)amount);
}
}
}

// @symbol func_ov004_020b1b40
namespace s20b1b40 {
extern "C" {
extern void func_ov004_020b1ba0(void *c, s32 delta);
/* Adds `amount` to the scene's unk_0a8 (func_ov004_020b1ba0). */
void func_ov004_020b1b40(void *amount) {
    void *v = data_ov004_020beb68;
    if (v == 0) return;
    func_ov004_020b1ba0(v, (s32)amount);
}
}
}

// @symbol func_ov004_020b1b78
namespace s20b1b78 {
extern "C" {
/* unk_0a8 -= val, clamped to 0..9999. */
void func_ov004_020b1b78(void* c, int val) {
    dScMgBase_c *scene = (dScMgBase_c *)c;
    int r2 = scene->unk_0a8;
    r2 -= val;
    if (r2 < 0) r2 = 0;
    else if (r2 > 0x270f) r2 = 0x270f;
    scene->unk_0a8 = r2;
}
}
}

// @symbol func_ov004_020b1ba0
namespace s20b1ba0 {
extern "C" {
/* unk_0a8 += delta, clamped to 0..9999. */
void func_ov004_020b1ba0(void *c, s32 delta)
{
    dScMgBase_c *scene = (dScMgBase_c *)c;
    s32 v = scene->unk_0a8 + delta;
    if (v < 0) v = 0;
    else if (v > 0x270f) v = 0x270f;
    scene->unk_0a8 = v;
}
}
}

// @symbol func_ov004_020b1bc8
namespace s20b1bc8 {
extern "C" {
extern void func_ov004_020b1de8(int r0, int r1, int r2, int r3);
/* Draws sprite word 2 at (a1, a2) on wrapper a3 (func_ov004_020b1de8),
   then the scene's unk_0ac beside it: below 100 through
   func_ov004_020b2444, otherwise through func_ov004_020b1ea4. */
void func_ov004_020b1bc8(char *a0, int a1, int a2, int a3)
{
    func_ov004_020b1de8(a1, a2, a3, -1);
    int v = ((dScMgBase_c *)a0)->unk_0ac;
    if (v < 0x64) {
        func_ov004_020b2444(a1+0x12, a2, v, a3, -1, 2, 0);
    } else {
        func_ov004_020b1ea4(a1+0xc, a2, v, a3, -1, 2, 0x32);
    }
}
}
}

// @symbol func_ov004_020b1c68
namespace s20b1c68 {
extern "C" {
struct M { int _00, _01, _10, _11; };
extern void RenderOamBothScreens(void* a0, int a1, int a2, int a3, int a4, void* a5);
extern void func_ov004_020afc18(void* a0, int a1, int a2, int a3, int a4, void* a5);
/* a3 picks the wrapper: -1 RenderOamBothScreens, 0 func_ov004_020af868,
   anything else func_ov004_020afc18; each is passed -1 in its place. */
void func_ov004_020b1c68(void* a0, int a1, int a2, int a3, int a4, struct M* a5){
  if(a3==-1){
    RenderOamBothScreens(a0, a1, a2, -1, a4, a5);
    return;
  }
  if(a3==0){
    func_ov004_020af868(a0, a1, a2, -1, a4, a5);
    return;
  }
  func_ov004_020afc18(a0, a1, a2, -1, a4, a5);
}
}
}

// @symbol func_ov004_020b1cf0
namespace s20b1cf0 {
extern "C" {
void func_ov004_020afdd0(int a, int b, int c, int d, int e);
void Hud_RenderSprite(int a, int b, int c, int d, int e);
void RenderOamMainScreen(int a, int b, int c, int d, int e);
/* `sel` picks the wrapper: -1 func_ov004_020afdd0, 0 Hud_RenderSprite,
   anything else RenderOamMainScreen; each is passed -1 in its place. */
void func_ov004_020b1cf0(int a, int b, int c, int sel, int e){
  if(sel == -1){ func_ov004_020afdd0(a,b,c,-1,e); return; }
  if(sel == 0){ Hud_RenderSprite(a,b,c,-1,e); return; }
  RenderOamMainScreen(a,b,c,-1,e);
}
}
}

// @symbol func_ov004_020b1d60
namespace s20b1d60 {
extern "C" {
extern void RenderOamBothScreens(void* a0, int a1, int a2, int a3, int a4, void* a5);
extern void func_ov004_020afc18(void* a0, int a1, int a2, int a3, int a4, void* a5);
/* func_ov004_020b1c68 without a matrix: the wrappers get 0. */
void func_ov004_020b1d60(void* a0, int a1, int a2, int a3, int a4){
  if(a3==-1){
    RenderOamBothScreens(a0, a1, a2, -1, a4, 0);
    return;
  }
  if(a3==0){
    func_ov004_020af868(a0, a1, a2, -1, a4, 0);
    return;
  }
  func_ov004_020afc18(a0, a1, a2, -1, a4, 0);
}
}
}

// @symbol func_ov004_020b1de8
namespace s20b1de8 {
extern "C" {
extern int GetGameLanguage(void);
extern int func_ov004_020b1d60(int, int, int, int, int);

/* Draws sprite word 2 at (r0, r1) through func_ov004_020b1d60. */
void func_ov004_020b1de8(int r0, int r1, int r2, int r3) {
    int idx = GetGameLanguage();
    int *t = data_ov004_020bbfa8[idx];
    func_ov004_020b1d60(t[2], r0, r1, r2, r3);
}
}
}

// @symbol func_ov004_020b1e34
namespace s20b1e34 {
extern "C" {
extern void func_ov004_020b0e84(dScMgBase_c *sl, int sb);

/* Draws the mHudScore tally through func_ov004_020b0e84 (a tail call). */
void func_ov004_020b1e34(void* a, int unused1, int unused2, int unused3) {
    func_ov004_020b0e84((dScMgBase_c *)a, ((dScMgBase_c *)a)->mHudScore);
}
}
}

// @symbol func_ov004_020b1e44
namespace s20b1e44 {
extern "C" {
extern int GetGameLanguage(void);
extern void DrawOamSprite(int a, int b, int c, int d);
/* Draws `arg` centred at (0x80, 0xc) and sprite word 3 at (0x60, 0xc). */
void func_ov004_020b1e44(int arg) {
    int r;
    func_ov004_020b2444(0x80, 0xc, arg, 1, -1, 0, 0);
    r = GetGameLanguage();
    DrawOamSprite(data_ov004_020bbfa8[r][3], 0x60, 0xc, 0);
}
}
}

// @symbol func_ov004_020b1ea4
namespace s20b1ea4 {
extern "C" {
extern int data_ov006_02137cd8[];
extern void func_ov004_020b1d60(void *glyph, int x, int a2, int a3, int a4);

/* Draws `val`, capped at 999999, in digits 8 px apart: glyph
   data_ov006_02137cd8[fb + digit], where fb 0 means 0xa and 0x14 means
   0x1e. Mode 0 centres the number on x, mode 1 ends it at x, any other
   mode starts it at x. */
void func_ov004_020b1ea4(int x, int a1, int val, int a3, int a4, int mode, int fb) {
    int d[6];
    int i;
    if (val >= 0xf423f) val = 0xf423f;
    for (i = 0; i < 6; i++) d[i] = 0;
    d[0] = val;
    while (d[0] >= 0x186a0) { d[5]++; d[0] -= 0x186a0; }
    while (d[0] >= 0x2710) { d[4]++; d[0] -= 0x2710; }
    while (d[0] >= 0x3e8) { d[3]++; d[0] -= 0x3e8; }
    while (d[0] >= 0x64) { d[2]++; d[0] -= 0x64; }
    while (d[0] >= 0xa) { d[1]++; d[0] -= 0xa; }
    if (fb == 0) fb = 0xa;
    else if (fb == 0) fb = 0xa;
    else if (fb == 0x14) fb = 0x1e;
    else if (fb == 0xa) fb = 0xa;

    if (d[5] != 0) {
        int r5;
        int k;
        if (mode == 0) x -= 0x14;
        else if (mode == 1) x -= 0x28;
        r5 = 5;
        for (k = 0; k < 5; k++) {
            func_ov004_020b1d60((void*)data_ov006_02137cd8[fb + d[r5]], x, a1, a3, a4);
            x += 8;
            r5--;
        }
    } else if (d[4] != 0) {
        int r5;
        int k;
        if (mode == 0) x -= 0x10;
        else if (mode == 1) x -= 0x20;
        r5 = 4;
        for (k = 0; k < 4; k++) {
            func_ov004_020b1d60((void*)data_ov006_02137cd8[fb + d[r5]], x, a1, a3, a4);
            x += 8;
            r5--;
        }
    } else if (d[3] != 0) {
        int r5;
        int k;
        if (mode == 0) x -= 0xc;
        else if (mode == 1) x -= 0x18;
        r5 = 3;
        for (k = 0; k < 3; k++) {
            func_ov004_020b1d60((void*)data_ov006_02137cd8[fb + d[r5]], x, a1, a3, a4);
            x += 8;
            r5--;
        }
    } else if (d[2] != 0) {
        int r5;
        int k;
        if (mode == 0) x -= 8;
        else if (mode == 1) x -= 0x10;
        r5 = 2;
        for (k = 0; k < 2; k++) {
            func_ov004_020b1d60((void*)data_ov006_02137cd8[fb + d[r5]], x, a1, a3, a4);
            x += 8;
            r5--;
        }
    } else if (d[1] != 0) {
        if (mode == 0) x -= 4;
        else if (mode == 1) x -= 8;
        func_ov004_020b1d60((void*)data_ov006_02137cd8[fb + d[1]], x, a1, a3, a4);
        x += 8;
    }
    func_ov004_020b1d60((void*)data_ov006_02137cd8[d[0] + fb], x, a1, a3, a4);
}
}
}

// @symbol func_ov004_020b2220
#pragma push
#pragma opt_propagation off
namespace s20b2220 {
extern "C" {
/* Draws a score number rotated by an angle: splits the value (clamped to
   9999) into thousands, hundreds, tens and units, builds the rotation-and-
   scale matrix from the sine table, and draws the digits centred on x: four
   digits from x-0x30, three from x-0x20, two from x-0x10, a lone unit at x.
   The fixed-point multiply and the 2x2 rotation are the NitroSDK FX_Mul and
   MTX_Rot22 shapes, written as local inlines. Spelled that way the
   allocator keeps the thousands counter in r0 as the cartridge does; with
   the products written out in the body it lands in ip or r3. */
struct M { int _00, _01, _10, _11; };
extern void func_ov004_020b1c68(void* a0, int a1, int a2, int a3, int a4, struct M* a5);
extern s16 data_02082214[];
extern int data_ov006_02137cd8[];

static inline int FX_Mul(int v1, int v2)
{
    s64 t = (s64)v1 * v2;
    return (int)((t + 0x800) >> 12);
}

static inline void MTX_Rot22(struct M *p, int sinVal, int cosVal)
{
    p->_00 = cosVal;
    p->_01 = sinVal;
    p->_10 = -sinVal;
    p->_11 = cosVal;
}

static inline s16 FX_SinIdx(int idx) { return data_02082214[idx << 1]; }
static inline s16 FX_CosIdx(int idx) { return data_02082214[(idx << 1) + 1]; }

void func_ov004_020b2220(int x, int y, int value, int a3, int a4, int scale, u16 angle)
{
    struct M m;
    int idx;
    int te, hu, th;

    if (value >= 9999) value = 9999;
    te = hu = th = 0;
    while (value >= 1000) { value -= 1000; th++; }
    while (value >= 100) { value -= 100; hu++; }
    while (value >= 10) { value -= 10; te++; }

    idx = angle >> 4;
    MTX_Rot22(&m, FX_Mul(FX_SinIdx(idx), scale), FX_Mul(FX_CosIdx(idx), scale));

    if (th != 0) {
        func_ov004_020b1c68((void *)data_ov006_02137cd8[th], x - 0x30, y, a3, a4, &m);
        func_ov004_020b1c68((void *)data_ov006_02137cd8[hu], x - 0x10, y, a3, a4, &m);
        func_ov004_020b1c68((void *)data_ov006_02137cd8[te], x + 0x10, y, a3, a4, &m);
        x += 0x30;
    } else if (hu != 0) {
        func_ov004_020b1c68((void *)data_ov006_02137cd8[hu], x - 0x20, y, a3, a4, &m);
        func_ov004_020b1c68((void *)data_ov006_02137cd8[te], x, y, a3, a4, &m);
        x += 0x20;
    } else if (te != 0) {
        func_ov004_020b1c68((void *)data_ov006_02137cd8[te], x - 0x10, y, a3, a4, &m);
        x += 0x10;
    }
    func_ov004_020b1c68((void *)data_ov006_02137cd8[value], x, y, a3, a4, &m);
}
}
}
#pragma pop

// @symbol func_ov004_020b2444
namespace s20b2444 {
extern "C" {
extern int data_ov006_02137cd8[];
extern void func_ov004_020b1d60(int a0, int a1, int a2, int a3, int a4);

/* Draws `num` in digits 16 px apart, glyph data_ov006_02137cd8[idx +
   digit]. The units digit sits at a1 for sel 1 (or any value but 0 and
   2), at a1 + 8 per extra digit for sel 0, and at a1 + 16 per extra digit
   for sel 2, so the number ends at, centres on, or starts at a1. 0 draws
   one glyph; a negative num draws nothing. */
void func_ov004_020b2444(int a1, int a2, int num, int a4, int a5, int sel, int idx)
{
    unsigned t = num;
    int digits = 0;
    int off;
    if (num != 0) {
        do {
            t /= 10;
            digits++;
        } while (t != 0);
    }
    off = 0;
    switch (sel) {
    case 0:
        off = (digits << 3) - 8;
        break;
    case 1:
        break;
    case 2:
        off = (digits << 4) - 0x10;
        break;
    }
    if (num == 0) {
        func_ov004_020b1d60(data_ov006_02137cd8[idx], a1, a2, a4, a5);
        return;
    }
    if (num <= 0)
        return;
    do {
        func_ov004_020b1d60(data_ov006_02137cd8[num % 10 + idx], a1 + off, a2, a4, a5);
        num /= 10;
        off -= 0x10;
    } while (num > 0);
}
}
}

// @symbol func_ov004_020b2574
namespace s20b2574 {
extern "C" {
extern int GetGameLanguage(void);
extern void DrawOamSprite(void *arg0, void *arg1, int arg2, void *arg3);

/* Unless arg1 is 1, draws sprite words 0 and 1 at (12, 12) and (30, 12)
   and arg0 from x 0x30. With arg1 1, draws sprite word 0 arg0 times,
   16 px apart from (12, 12). */
void func_ov004_020b2574(int arg0, int arg1)
{
    int *table = data_ov004_020bbfa8[GetGameLanguage()];
    void *sb = (void *)table[0];
    int b = 12;
    if (arg1 != 1) {
        DrawOamSprite(sb, (void *)b, b, 0);
        DrawOamSprite((void *)data_ov004_020bbfa8[GetGameLanguage()][1], (void *)30, b, 0);
        func_ov004_020b2444(0x30, b, arg0, 1, -1, 2, 0);
    } else {
        int i;
        for (i = 0; i < arg0; i++) {
            func_ov004_020af948(sb, b, 12, 0);
            b += 16;
        }
    }
}
}
}

namespace s20b265c {
extern "C" {
// @symbol _ZN11dScMgBase_c9Virtual84Ev
/* dScMgBase_c::Virtual84 - slot 33.  The ROM names nothing here: Virtual84
   is the repo's no-name spelling after the +0x84 vtable offset, as fBase_c
   uses for Virtual34 and Virtual38.  See the slot-33 block in
   include/dScMgBase_c.h.

   ENGINE BRING-UP.  Graphics modes for both engines (GX mode 1/0/0, GXS mode
   0), VRAM banks assigned to BG and OBJ on both screens, and BOTH BG-enable
   shadows -- data_0209d45c for the main engine, data_0209d454 for the sub --
   initialised to 0x10, which is the value slots 30 and 31 later save, clear
   bits out of and restore.  A language-indexed character file (indexed by
   GetGameLanguage into data_ov004_020bbfe4) is decompressed into BOTH engines'
   BG char VRAM at 0x06404000 and 0x06604000, OBJ palette file 0xc3 is loaded
   into both, and the scene object is published into the global registry --
   data_ov004_020beb74[1] = this, then data_0209d4a8 points at that registry.
   The object fields it touches are its own: +0x68 cleared and +0x6c set to -1.

   WHEN IT RUNS.  First out of dScMgBase_c::BeforeInitResources
   (ov004:0x020b0930): the +0x84 dispatch at 0x020b09d0 is near the top and
   slot 31's +0x7c dispatch at 0x020b0a0c is the last thing before the
   function returns 1.  Slot 32 runs out of AfterInitResources.  So the
   sequence is: bring the engines up (33), dress the sub screen (31), then
   dress the main screen (32).

   arity: no explicit parameters, MEASURED.  Scanning arm9 and all 103 overlays
   for the dispatch pair -- `ldr rD,[rN,#0x84]` with Rn != pc, followed within
   three instructions by `blx rD`/`bx rD` -- finds exactly two sites image-wide,
   of which one is in ov004 or ov006: the 0x020b09d0 call above.  It reads
   `mov r0,r4; ldr r1,[r0]; ldr r1,[r1,#0x84]; blx r1`, so r1 is the loaded
   pointer and cannot also be a second argument.  The scanner was validated by
   re-running it at +0x80 and reproducing slot 32's known call site.
   return type: void, which is what all three bodies do -- none assigns a
   result and the one caller ignores whatever falls out. */
extern void func_ov004_020b2980(void);
extern void func_ov004_020b290c(void);
extern void _ZN2GX12SetBankForBGEt(u16 a);
extern void _ZN2GX13SetBankForOBJEt(u16 a);
extern void _ZN2GX15SetBankForSubBGEt(u16 a);
extern void _ZN2GX16SetBankForSubOBJEt(u16 a);
extern s32 GetGameLanguage(void);
extern void *func_ov004_020adc68(int id);
extern void DecompressLZ16(void *src, void *dst);
extern void Ov004_Deallocate(void *p);
extern void _ZN4CP1527FlushAndInvalidateDataCacheEjj(void *p, u32 len);
extern void _ZN2GX11LoadOBJPlttEPKvjj(const void *p, u32 a, u32 b);
extern void _ZN3GXS11LoadOBJPlttEPKvjj(const void *p, u32 a, u32 b);
extern void func_ov004_020b0d30(void);
extern void FreeGfxSlotsById(int arg);
extern int data_ov004_020beb6c;
extern u8 data_0209d45c;
extern int data_ov004_020bbfe4[];
extern u8 data_0209d454;
extern void **data_0209d4a8;
extern int data_0208ee44;
}
}

void dScMgBase_c::Virtual84()
{ using s20b265c::DecompressLZ16; using s20b265c::FreeGfxSlotsById; using s20b265c::GetGameLanguage; using s20b265c::Ov004_Deallocate; using s20b265c::_ZN2GX11LoadOBJPlttEPKvjj; using s20b265c::_ZN2GX12SetBankForBGEt; using s20b265c::_ZN2GX13SetBankForOBJEt; using s20b265c::_ZN2GX15SetBankForSubBGEt; using s20b265c::_ZN2GX16SetBankForSubOBJEt; using s20b265c::_ZN3GXS11LoadOBJPlttEPKvjj; using s20b265c::_ZN4CP1527FlushAndInvalidateDataCacheEjj; using s20b265c::data_0208ee44; using s20b265c::data_0209d454; using s20b265c::data_0209d45c; using s20b265c::data_0209d4a8; using s20b265c::data_ov004_020bbfe4; using s20b265c::data_ov004_020beb6c; using s20b265c::func_ov004_020adc68; using s20b265c::func_ov004_020b0d30; using s20b265c::func_ov004_020b290c; using s20b265c::func_ov004_020b2980;
    char *obj = (char *)this;

    void *p;

    data_ov004_020beb6c = 0;
    obj[0x68] = 0;
    *(int *)(obj + 0x6c) = -1;
    *(vu32 *)0x4001000u |= 0x10000u;
    _ZN2GX15SetGraphicsModeEiii(1, 0, 0);
    _ZN3GXS15SetGraphicsModeEi(0);
    *(vu32 *)0x4001000u &= 0xffcfffefu;
    *(vu16 *)0x4000304u |= 0x8000u;
    func_ov004_020b2980();
    func_ov004_020b290c();
    *(vu32 *)0x4000000u &= ~0x7000000u;
    *(vu32 *)0x4000000u &= ~0x38000000u;
    _ZN2GX12SetBankForBGEt(3);
    _ZN2GX13SetBankForOBJEt(0x10);
    data_0209d45c = 0x10;
    _ZN2GX15SetBankForSubBGEt(4);
    _ZN2GX16SetBankForSubOBJEt(8);
    p = func_ov004_020adc68(data_ov004_020bbfe4[GetGameLanguage()]);
    {
        char *dst = (char *)0x6400000; dst += 0x4000;
        DecompressLZ16(p, dst);
    }
    {
        char *dst = (char *)0x6600000; dst += 0x4000;
        DecompressLZ16(p, dst);
    }
    Ov004_Deallocate(p);
    p = func_ov004_020adc68(0xc3);
    _ZN4CP1527FlushAndInvalidateDataCacheEjj(p, 0x100u);
    _ZN2GX11LoadOBJPlttEPKvjj(p, 0x100u, 0x100u);
    _ZN3GXS11LoadOBJPlttEPKvjj(p, 0x100u, 0x100u);
    Ov004_Deallocate(p);
    data_0209d454 = 0x10;
    data_ov004_020beb74.mScene = this;
    data_0209d4a8 = (void **)&data_ov004_020beb74;
    func_ov004_020b0d30();
    FreeGfxSlotsById(0x1d);
    data_0208ee44 = 1;
}

namespace s20b27f4 {
extern "C" {
// @symbol _ZN11dScMgBase_c9Virtual80Ev
// recovered name: dScMgBase_c_AfterClsn  -- WRONG, see below
/* dScMgBase_c::Virtual80 - slot 32.

   The `recovered name:` line above is kept visible because it is wrong.  This
   time the borrowed name is a REAL ROM name -- _ZN16dPathLiftActor_c9AfterClsnEi,
   declared at include/PathLift.h:58 -- which makes it the more misleading of the
   two.  dPathLiftActor_c derives from dBgActor_c, which derives from dActor_c;
   dScMgBase_c is a dScene_c.  The chains share only dBase_c, which adds no
   virtual, so the two slot 32s have fBase_c's first eighteen entries in common
   and nothing else.  That AfterClsn also takes an int.  See the slot-32 block in
   include/dScMgBase_c.h.

   It touches no collision.  It is slot 31 with the other display engine: three
   read-modify-writes leave the MAIN BG1CNT at 0x0400000a holding exactly 0x1000,
   the layer's scroll is reset, BG1's bit is cleared from data_0209d45c -- the
   MAIN BG-enable shadow slot 30 restores the main DISPCNT from, where 31 cleared
   the sub's data_0209d454 -- and a language-indexed character file plus the
   shared screen map (file 0x67 here, 0x5b there) are installed.  It builds this
   minigame's TOP-screen background, from AfterInitResources. */
extern int GetGameLanguage(void);
extern unsigned int LoadCompressedFileAt(int fileID, void *target);
extern unsigned char data_0209d45c[];
}
}

void dScMgBase_c::Virtual80()
{ using s20b27f4::GetGameLanguage; using s20b27f4::LoadCompressedFileAt; using s20b27f4::data_0209d45c;
    int f;
    *(volatile unsigned short *)0x400000a = *(volatile unsigned short *)0x400000a & ~3;
    *(volatile unsigned short *)0x400000a = (*(volatile unsigned short *)0x400000a & 0x43) | 0x1000;
    *(volatile unsigned short *)0x400000a = *(volatile unsigned short *)0x400000a & ~0x40;
    SetBg1Offset(0, 0);
    data_0209d45c[0] &= ~2;
    f = GetGameLanguage();
    LoadCompressedFileAt(data_ov004_020bbff8[f], (void *)func_02054ea8());
    LoadCompressedFileAt(0x67, _ZN2G212GetBG1ScrPtrEv());
}

namespace s20b2880 {
extern "C" {
// @symbol _ZN11dScMgBase_c9Virtual7CEv
// recovered name: dScMgBase_c_Kill  -- WRONG, see below
/* dScMgBase_c::Virtual7C - slot 31.

   The `recovered name:` line above is kept visible because it is wrong.  The
   ROM names nothing here: dScMgBase_c is a SCENE (fBase_c -> dBase_c ->
   dScene_c -> dScMgBase_c), not an actor, and dActor_c -- whose names slots
   18-30 borrowed by index -- has no slot 31 at all.  `Kill` was carried across
   from dBgActor_c, dActor_c's own child, where _ZN10dBgActor_c4KillEv
   (ov002:0x020ee55c) is that class's new slot 31.  Different branch, same
   index, no relationship.  See the slot-31 block in include/dScMgBase_c.h.

   It also does not kill anything.  Three read-modify-writes leave the sub
   engine's BG1CNT holding exactly 0x10 -- priority 0, no mosaic -- then the
   layer's scroll is reset, BG1's bit is cleared from the sub BG-enable shadow
   that slot 30 restores the sub DISPCNT from, and a language-indexed character
   file plus the shared screen map (file 0x5b) are installed.  It builds this
   minigame's touch-screen background, from BeforeInitResources. */
extern int GetGameLanguage(void);
extern unsigned int _ZN3G2S13GetBG1CharPtrEv(void);
extern unsigned int LoadCompressedFileAt(int fileID, void *target);
extern void *_ZN3G2S12GetBG1ScrPtrEv(void);
extern unsigned char data_0209d454[];
}
}

int dScMgBase_c::Virtual7C()
{ using s20b2880::GetGameLanguage; using s20b2880::LoadCompressedFileAt; using s20b2880::_ZN3G2S12GetBG1ScrPtrEv; using s20b2880::_ZN3G2S13GetBG1CharPtrEv; using s20b2880::data_0209d454;
    int f;
    *(volatile unsigned short *)0x400100a = (*(volatile unsigned short *)0x400100a & 0x43) | 0x10;
    *(volatile unsigned short *)0x400100a = *(volatile unsigned short *)0x400100a & ~0x40;
    *(volatile unsigned short *)0x400100a = *(volatile unsigned short *)0x400100a & ~3;
    SetSubBg1Offset(0, 0);
    data_0209d454[0] &= ~2;
    f = GetGameLanguage();
    LoadCompressedFileAt(data_ov004_020bc00c[f], (void *)_ZN3G2S13GetBG1CharPtrEv());
    LoadCompressedFileAt(0x5b, _ZN3G2S12GetBG1ScrPtrEv());
}

// @symbol func_ov004_020b290c
namespace s20b290c {
extern "C" {
/* Resets the scroll of all eight BG layers, main and sub, to (0, 0). */
void func_ov004_020b290c(void) {
    SetBg0Offset(0, 0);
    SetBg1Offset(0, 0);
    SetBg2Offset(0, 0);
    SetBg3Offset(0, 0);
    SetSubBg0Offset(0, 0);
    SetSubBg1Offset(0, 0);
    SetSubBg2Offset(0, 0);
    SetSubBg3Offset(0, 0);
}
}
}

// @symbol func_ov004_020b2980
namespace s20b2980 {
extern "C" {
void func_ov004_020b2980(void) { _ZN2GX15DisableAllBanksEv(); }
}
}

// @symbol _ZN11dScMgBase_c15OnGroundPoundedEv
// recovered name: dScMgBase_c_OnGroundPounded
/* dScMgBase_c::OnGroundPounded - slot 21. */
void dScMgBase_c::OnGroundPounded()
{
}

// @symbol _ZN11dScMgBase_c9Virtual50Ev
/* Minigame slot 20; Virtual50 is a placeholder, not an original name.
   The reconstructed void contract is documented in dScMgBase_c.h. */
/* The base does nothing: the ROM body is a single `bx lr`. */
void dScMgBase_c::Virtual50()
{
}

// @symbol _ZN11dScMgBase_c13OnTurnIntoEggEi
// recovered name: dScMgBase_c_OnTurnIntoEgg
/* dScMgBase_c::OnTurnIntoEgg - vtable slot 19. Both the name and the signature are evidenced here: this
   body carries its own `recovered name:` above, and dScMgJump_c and
   dScMgBSC_c carry theirs. The parameter is an int, not dActor_c.h:132's
   `Player &` -- see include/dScMgBase_c.h. The base ignores the mode. */
int dScMgBase_c::OnTurnIntoEgg(int /* mode */)
{
    return 1;
}

// @symbol _ZN11dScMgBase_c13OnYoshiTryEatEi
/* vtable slot 18. The name is NOT a `recovered name:` -- this body carried
   none. It comes from include/dActor_c.h:131 and from ov006/symbols.txt,
   which already named dScMgCoin_c's override of this slot. The signature,
   unlike the name, is measured; see include/dScMgBase_c.h.
   The base ignores both arguments -- every descendant that cares reads
   them. */
void dScMgBase_c::OnYoshiTryEat(int arg)
{
}

// @symbol func_ov004_020b29a0
namespace s20b29a0 {
/* Calls the scene's OnYoshiTryEat (slot 18). Its callers, all in dMgState_c,
   pass the scene singleton and its mState, declaring the state as a
   void *, so the parameter stays one here. */
extern "C" void func_ov004_020b29a0(dScMgBase_c *scene, void *state)
{
    scene->OnYoshiTryEat((int)state);
}
}

/* The constructor and destructor are the end of the unit, and the one place
   the file is not in ROM order. Under defer_codegen off the out-of-line
   destructor comes out D1, D0, D2; the cartridge has D2, D0, D1 and then C2.
   With deferred codegen mwccarm emits a run of definitions in reverse, so
   writing the constructor first and the destructor after it, inside this
   bracket, produces D2, D0, D1, C2 as on the cartridge. */
#pragma push
#pragma defer_codegen on

namespace s20b2adc {
extern "C" {
// @symbol _ZN11dScMgBase_cC2Ev
extern short data_ov004_020bc070[];
}
}

dScMgBase_c::dScMgBase_c()
    : unk_0a4(0), unk_0c2(1)
{ using s20b2adc::data_ov004_020bc070;
    mMenuOpen = 0;
    unk_462c = 0;
    unk_4630 = 0;
    unk_4648 = 0;

    data_ov004_020beb68 = this;
    unk_0bc = *(u32 *)(data_0209b308 + 0x30);

    mSceneKind = data_ov004_020bc070[(param1 >> 16) & 0xff];
    /* param1 &= 0xffff, through a char *: spelled with the member, either
       as &= or as a plain assignment, mwccarm forms the address this + 8
       in ip first and the constructor grows by 4 bytes. */
    *(s32 *)((char *)this + 8) =
        *(u32 *)((char *)this + 8) & 0xffff;
    unk_050 = 0;
    unk_054 = 0;

    if (actorID == 0x16e || actorID == 0x185 ||
        actorID == 0x16d || actorID == 0x182)
        mTimeLimit = 0x3c;
    else
        mTimeLimit = 0x78;

    unk_060 = 0;
    mFrameCounter = 0;
    unk_464c = 0;
    unk_4654 = 0;
    unk_4658 = 0;
    unk_064 = -1;

    func_ov004_020adc00(func_02013580(func_ov004_020adc3c(this), 0));
}

// @symbol _ZN11dScMgBase_cD2Ev
// @symbol _ZN11dScMgBase_cD0Ev
// @symbol _ZN11dScMgBase_cD1Ev
/* The destructor clears the shared minigame pointer; CodeWarrior supplies the
   vptr restores and the dScene_c teardown. This one out-of-line definition
   emits all three variants, D2 at 0x020b29c0, D0 at 0x020b2a18 and D1 at
   0x020b2a84, in that order inside the deferred bracket. As the first
   declared virtual it is the key function, so this file also emits the
   dScMgBase_c vtable and RTTI. */
dScMgBase_c::~dScMgBase_c()
{
    data_ov004_020beb68 = 0;
}

#ifdef _MSC_VER
/* THE HOST NEEDS THE ROM'S FLAT D2 AND D0 NAMES, AND MSVC NEVER EMITS THEM.
 * MSVC folds the Itanium destructor variants into the one ~dScMgBase_c()
 * above. Descendants tearing down a dScMgBase_c base subobject call the
 * base-object variant by its flat name; the deleting variant is the D1 body
 * followed by the class-specific operator delete. Each is spelled here in
 * terms of the one host destructor, called qualified so it is a direct call.
 * mwccarm never defines _MSC_VER, so nothing here reaches the cartridge
 * object. */
extern "C" dScMgBase_c *_ZN11dScMgBase_cD2Ev(dScMgBase_c *thiz)
{
    thiz->dScMgBase_c::~dScMgBase_c();
    return thiz;
}

extern "C" dScMgBase_c *_ZN11dScMgBase_cD0Ev(dScMgBase_c *thiz)
{
    thiz->dScMgBase_c::~dScMgBase_c();
    dScMgBase_c::operator delete(thiz);
    return thiz;
}
#endif

#pragma pop
