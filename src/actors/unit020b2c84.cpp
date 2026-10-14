//cpp
/* unit020b2c84 -- the 44 functions of the ov004 linker unit 0x020b2c84..0x020b4aa4.
 *
 * This is the unlabelled run between the end of dScMgBase_c's unit and the unit
 * that starts at 0x020b4aa4. No class label, RTTI record or vtable falls in it,
 * so the file keeps the unit<addr> name. What the functions do, from their
 * bodies: the first group manages a table of 29 loaded files (func_ov004_020b2c84
 * frees them, func_ov004_020b2cb8 loads them by language, func_ov004_020b3278
 * decompresses one into a graphics slot and sets up an Elem to show it), and
 * the rest are the behavior and render handlers of Elem, the 0x134-byte record
 * of which data_ov004_020bf648 holds three. An Elem draws its graphic as a
 * list of OAM records; its behavior moves it toward mTargetX/mTargetY or
 * animates it against mTimer.
 *
 * Edges. 0x020b2c84 is the first function after func_ov004_020b2c80, the last
 * of the three destructor helpers (func_ov004_020b2c58, func_ov004_020b2c7c,
 * func_ov004_020b2c80) that __sinit_ov004_020b948c registers for the preceding
 * unit; those stay with dScMgBase_c. 0x020b4aa4 is the next function after
 * _ZN4ElemD1Ev, the last of the three destructor helpers
 * (_ZN4ElemC1Ev, __arraydtor$954, _ZN4ElemD1Ev) that
 * __sinit_ov004_020b955c registers for this unit's static array at
 * data_ov004_020bf648; __arraydtor$954 runs __cxa_vec_cleanup over it.
 * The unit emits one data object of its own, the anonymous initializer template
 * of func_ov004_020b2cb8's local array at 0x020bc6e8 (0xe8 bytes), which this
 * file's .data claim covers. Everything else it touches is reached by extern.
 *
 * Emission order. `#pragma defer_codegen off` is load-bearing here: with codegen
 * deferred the opt_* pragmas around func_ov004_020b2cb8, func_ov004_020b3b38,
 * func_ov004_020b4360 and func_ov004_020b45c0 do not bind to the one function
 * they bracket. Source is therefore ROM-ascending.
 *
 * The bodies were matched as one-function C or C++ sources and keep that
 * spelling under extern "C" and their ROM names.
 *
 * Numbered names. mwccarm numbers its generated names from one counter that
 * every named parameter (prototypes included), local and temporary before a
 * definition advances. The array data_ov004_020bf648 at the end of the file
 * gets __arraydtor$954 (and @955), and symbols.txt and the manifest name
 * __arraydtor$954, so an edit here has to keep that count. The named
 * parameters on the ApproachLinear2 and __cxa_vec_cleanup prototypes stand
 * in for names removed elsewhere, and the
 * six record copies in func_ov004_020b37f0, func_ov004_020b39a4 and
 * func_ov004_020b422c stay W2 (int[2]) copies: assigning the records as
 * ElemFn emits the same code but moves the counter by 8 (measured).
 * func_ov004_020b3278's `b ? a : a` copies are a code shape, not a count:
 * a plain record copy there changes 12 words of that function (measured).
 *
 * Retired one-function sources (ROM address order):
 *   0x020b2c84  func_ov004_020b2c84
 *   0x020b2cb8  func_ov004_020b2cb8
 *   0x020b3194  func_ov004_020b3194
 *   0x020b31b4  func_ov004_020b31b4
 *   0x020b321c  func_ov004_020b321c
 *   0x020b3278  func_ov004_020b3278
 *   0x020b35d8  func_ov004_020b35d8
 *   0x020b3698  func_ov004_020b3698
 *   0x020b369c  func_ov004_020b369c
 *   0x020b37c4  func_ov004_020b37c4
 *   0x020b37f0  func_ov004_020b37f0
 *   0x020b3834  func_ov004_020b3834
 *   0x020b3888  func_ov004_020b3888
 *   0x020b38ac  func_ov004_020b38ac
 *   0x020b3978  func_ov004_020b3978
 *   0x020b39a4  func_ov004_020b39a4
 *   0x020b3b38  func_ov004_020b3b38
 *   0x020b3c58  func_ov004_020b3c58
 *   0x020b3c9c  func_ov004_020b3c9c
 *   0x020b3cb8  func_ov004_020b3cb8
 *   0x020b3e9c  func_ov004_020b3e9c
 *   0x020b4080  func_ov004_020b4080
 *   0x020b40ac  func_ov004_020b40ac
 *   0x020b40c0  func_ov004_020b40c0
 *   0x020b410c  func_ov004_020b410c
 *   0x020b4214  func_ov004_020b4214
 *   0x020b422c  func_ov004_020b422c
 *   0x020b42c0  func_ov004_020b42c0
 *   0x020b433c  func_ov004_020b433c
 *   0x020b4360  func_ov004_020b4360
 *   0x020b45c0  func_ov004_020b45c0
 *   0x020b4820  func_ov004_020b4820
 *   0x020b484c  func_ov004_020b484c
 *   0x020b49b8  func_ov004_020b49b8
 *   0x020b49e4  func_ov004_020b49e4
 *   0x020b49f0  func_ov004_020b49f0
 *   0x020b4a1c  func_ov004_020b4a1c
 *   0x020b4a28  func_ov004_020b4a28
 *   0x020b4a40  func_ov004_020b4a40
 *   0x020b4a4c  func_ov004_020b4a4c
 *   0x020b4a64  func_ov004_020b4a64
 *   0x020b4a70  _ZN4ElemC1Ev
 *   0x020b4a7c  __arraydtor$954
 *   0x020b4aa0  _ZN4ElemD1Ev
 */

#pragma defer_codegen off

#include "types.h"

struct M { int _00, _01, _10, _11; };
struct Pair { int a, b; };
struct W2 { int w[2]; };
struct PairW { int w0, w1; };
struct S3 { int v[3]; };
struct Entry { int id; int ptr; };
struct EntryTable { Entry e[29]; };

/* The minigame scene at data_ov004_020beb68, seen through the one virtual
   used here. Slot 26 is dScMgBase_c.h's OnHitByCannonBlastedChar; its scene
   bodies return 0 (dScMgBase_c), 1 (dScMgSingle3DBase_c) or 2
   (dScMgD3DBase_c), and this file only compares the result with 2. The
   view keeps the name Obj, the spelling config/decl-agreement-baseline.json
   records for this extern. */
struct Obj {
    virtual int v00(); virtual int v01(); virtual int v02(); virtual int v03();
    virtual int v04(); virtual int v05(); virtual int v06(); virtual int v07();
    virtual int v08(); virtual int v09(); virtual int v10(); virtual int v11();
    virtual int v12(); virtual int v13(); virtual int v14(); virtual int v15();
    virtual int v16(); virtual int v17(); virtual int v18(); virtual int v19();
    virtual int v20(); virtual int v21(); virtual int v22(); virtual int v23();
    virtual int v24(); virtual int v25();
    virtual int OnHitByCannonBlastedChar();  /* slot 26 */
};

/* One of the three 0x134-byte records at data_ov004_020bf648: a graphic
   loaded from the file table and drawn as a list of OAM records. The class
   name Elem is the one its constructor and destructor symbols carry; the
   real class is not recovered. */
struct Elem;
typedef void (Elem::*ElemFn)();
struct Elem {
    ElemFn mBehavior;    /* 0x00 run by func_ov004_020b321c */
    ElemFn mRender;      /* 0x08 run by func_ov004_020b31b4; 0 draws plainly */
    s16 mX, mY;          /* 0x10 screen position, pixels */
    s16 mTargetX;        /* 0x14 where the slide-in handlers stop */
    s16 mTargetY;        /* 0x16 */
    int mPriority;       /* 0x18 passed right after mScreen to every draw call */
    int mScreen;         /* 0x1c -1 draws on both screens (func_ov004_020b1cf0) */
    int mFileIndex;      /* 0x20 index into the file table; 29 when free */
    int mTimer;          /* 0x24 mostly stepped 0x1000 (1.0) per frame toward
                            mTimerEnd; func_ov004_020b3834 counts it by 1 */
    int mTimerEnd;       /* 0x28 */
    s16 mOamCount;       /* 0x2c records func_ov004_020af5e0 copied */
    s16 mStyle;          /* 0x2e index into the three handler tables */
    s16 mGfxSlot;        /* 0x30 0..2, see data_ov004_020bf3e8 */
    s16 mArrived;        /* 0x32 set when mX reaches mTargetX
                            (func_ov004_020b49b8, func_ov004_020b49f0) */
    u8 mOam[0x100];      /* 0x34 8-byte OAM records; 0xffff at +6 ends the list */
    Elem();
    ~Elem() {}
};

extern int ApproachLinear(int &, int, int);
extern int ApproachLinear2(short &value, short target, short step);

extern "C" {
void Deallocate(void *ptr);
int GetGameLanguage(void);
int LoadFile(int handle);
void DecompressLZ16(void *src, void *dst);
int func_ov004_020af5e0(int a, void *b, int c, int d);
int _ZN4cstd4fdivEii(int a, int b);
int func_02053200(int x);
unsigned int func_02012790(unsigned int x);
void __register_global_object(void *object, void *destructor, void **node);
void func_0203d704(int *o, int *a, int *b);
void func_0203d388(int *p, int angle);
int RandomIntInternal(int *seed);
void func_ov004_020adfc4(int c, short a1, int *a2, int *r3, int *sp0);
void NullDestructor_0203d47c(void);
void __cxa_vec_cleanup(void *array, unsigned int count, unsigned int size, void (*dtor)(void *));
void func_ov004_020b1c68(void *a0, int a1, int a2, int a3, int a4, M *a5);
void func_ov004_020b1cf0(int a, int b, int c, int d, int e);
int func_ov004_020b1aec(void);
void func_ov004_020b2444(int a, int b, int c, int d, int e, int f, int g);
void func_ov004_020b42c0(Elem *e);
void func_ov004_020b37f0(Elem *e);
void func_ov004_020b39a4(Elem *e);
}

extern s16 data_02082214[];
extern int data_0209e650;
extern Obj *data_ov004_020beb68;
extern S3 data_ov004_020bc27c;
extern int data_ov004_020b9f54;
static EntryTable data_ov004_020bc6e8 = { { { 0, 0 }, { 0x140, (int)&data_ov004_020b9f54 } } };
extern int data_ov006_021346bc;
extern int data_ov004_020beb6c;
extern int data_ov004_020bc150;
extern Pair data_ov004_020bc17c;
extern Pair data_ov004_020bc1ec;
extern Pair data_ov004_020bc224;
extern Pair data_ov004_020bc274;
extern int data_ov004_020bc1b4[];
extern int data_ov004_020bc20c[];
extern int data_ov004_020bc254[];
extern unsigned char data_ov004_020bf3e8[];
extern int data_ov004_020bf3ec;
extern int data_ov004_020bf3f0;
extern int data_ov004_020bf3f4[2];
extern int data_ov004_020bf3fc[2];
extern void *data_ov004_020bf404[3];
extern void *data_ov004_020bf410[3];
extern PairW data_ov004_020bf428[];
extern PairW data_ov004_020bf490[];
extern PairW data_ov004_020bf4f8[];
extern int data_ov004_020bf560[];
extern int data_ov004_020bf5d4[];

extern int data_ov004_020bc42c[];
extern int data_ov004_020bc65c[];
extern int data_ov004_020bc418[];
extern int data_ov004_020bc648[];
extern int data_ov004_020bc3c8[];
extern int data_ov004_020bc5e4[];
extern int data_ov004_020bc4f4[];
extern int data_ov004_020bc300[];
extern int data_ov004_020bc3dc[];
extern int data_ov004_020bc5f8[];
extern int data_ov004_020bc3f0[];
extern int data_ov004_020bc60c[];
extern int data_ov004_020bc404[];
extern int data_ov004_020bc634[];
extern int data_ov004_020bc3b4[];
extern int data_ov004_020bc5bc[];
extern int data_ov004_020bc468[];
extern int data_ov004_020bc6ac[];
extern int data_ov004_020bc454[];
extern int data_ov004_020bc698[];
extern int data_ov004_020bc378[];
extern int data_ov004_020bc594[];
extern int data_ov004_020bc620[];
extern int data_ov004_020bc670[];
extern int data_ov004_020bc440[];
extern int data_ov004_020bc684[];
extern int data_ov004_020bc5d0[];
extern int data_ov004_020bc3a0[];
extern int data_ov004_020bc47c[];
extern int data_ov004_020bc288[];
extern int data_ov004_020bc490[];
extern int data_ov004_020bc29c[];
extern int data_ov004_020bc580[];
extern int data_ov004_020bc350[];
extern int data_ov004_020bc4a4[];
extern int data_ov004_020bc2b0[];
extern int data_ov004_020bc4b8[];
extern int data_ov004_020bc2c4[];
extern int data_ov004_020bc4cc[];
extern int data_ov004_020bc2d8[];
extern int data_ov004_020bc4e0[];
extern int data_ov004_020bc2ec[];
extern int data_ov004_020bc38c[];
extern int data_ov004_020bc508[];
extern int data_ov004_020bc314[];
extern int data_ov004_020bc51c[];
extern int data_ov004_020bc328[];
extern int data_ov004_020bc530[];
extern int data_ov004_020bc6d4[];
extern int data_ov004_020bc544[];
extern int data_ov004_020bc6c0[];
extern int data_ov004_020bc5a8[];
extern int data_ov004_020bc33c[];
extern int data_ov004_020bc558[];
extern int data_ov004_020bc364[];
extern int data_ov004_020bc56c[];


// @symbol func_ov004_020b2c84
extern "C" void func_ov004_020b2c84(void) {
    int i;
    for (i = 0; i < 0x1d; i++) {
        Deallocate((void *)data_ov004_020bf560[i]);
        data_ov004_020bf560[i] = 0;
    }
}

// @symbol func_ov004_020b2cb8
#pragma opt_strength_reduction off
extern "C" void func_ov004_020b2cb8(void)
{
    int i;
    /* The ROM's template for this array is at 0x020bc6e8 and is all zero except
       entry 1. What stood here was 29 invented pairs {1,100}, {2,101} ... {29,128};
       none of them were ever compared to anything, because mwccarm puts a local
       initializer in an anonymous .rodata object (`@7`) that objisolate drops and
       match.py wildcards the relocation to.

       It matters for exactly one entry. Every index except 1 is overwritten below
       before the loop reads it -- entry 1 is not, so its initial value is live, and
       it was wrong. */
    EntryTable entries = data_ov004_020bc6e8;

    entries.e[0].id = data_ov004_020bc42c[GetGameLanguage()];
    entries.e[0].ptr = data_ov004_020bc65c[GetGameLanguage()];
    entries.e[2].id = data_ov004_020bc418[GetGameLanguage()];
    entries.e[2].ptr = data_ov004_020bc648[GetGameLanguage()];
    entries.e[3].id = data_ov004_020bc3c8[GetGameLanguage()];
    entries.e[3].ptr = data_ov004_020bc5e4[GetGameLanguage()];
    entries.e[4].id = data_ov004_020bc4f4[GetGameLanguage()];
    entries.e[4].ptr = data_ov004_020bc300[GetGameLanguage()];
    entries.e[5].id = data_ov004_020bc3dc[GetGameLanguage()];
    entries.e[5].ptr = data_ov004_020bc5f8[GetGameLanguage()];
    entries.e[6].id = data_ov004_020bc3f0[GetGameLanguage()];
    entries.e[6].ptr = data_ov004_020bc60c[GetGameLanguage()];
    entries.e[7].id = data_ov004_020bc404[GetGameLanguage()];
    entries.e[7].ptr = data_ov004_020bc634[GetGameLanguage()];
    entries.e[8].id = data_ov004_020bc3b4[GetGameLanguage()];
    entries.e[8].ptr = data_ov004_020bc5bc[GetGameLanguage()];
    entries.e[9].id = data_ov004_020bc468[GetGameLanguage()];
    entries.e[9].ptr = data_ov004_020bc6ac[GetGameLanguage()];
    entries.e[10].id = data_ov004_020bc454[GetGameLanguage()];
    entries.e[10].ptr = data_ov004_020bc698[GetGameLanguage()];
    entries.e[11].id = data_ov004_020bc378[GetGameLanguage()];
    entries.e[11].ptr = data_ov004_020bc594[GetGameLanguage()];
    entries.e[12].id = data_ov004_020bc620[GetGameLanguage()];
    entries.e[12].ptr = data_ov004_020bc670[GetGameLanguage()];
    entries.e[13].id = data_ov004_020bc440[GetGameLanguage()];
    entries.e[13].ptr = data_ov004_020bc684[GetGameLanguage()];
    entries.e[14].id = data_ov004_020bc5d0[GetGameLanguage()];
    entries.e[14].ptr = data_ov004_020bc3a0[GetGameLanguage()];
    entries.e[15].id = data_ov004_020bc47c[GetGameLanguage()];
    entries.e[15].ptr = data_ov004_020bc288[GetGameLanguage()];
    entries.e[16].id = data_ov004_020bc490[GetGameLanguage()];
    entries.e[16].ptr = data_ov004_020bc29c[GetGameLanguage()];
    entries.e[17].id = data_ov004_020bc580[GetGameLanguage()];
    entries.e[17].ptr = data_ov004_020bc350[GetGameLanguage()];
    entries.e[18].id = data_ov004_020bc4a4[GetGameLanguage()];
    entries.e[18].ptr = data_ov004_020bc2b0[GetGameLanguage()];
    entries.e[19].id = data_ov004_020bc4b8[GetGameLanguage()];
    entries.e[19].ptr = data_ov004_020bc2c4[GetGameLanguage()];
    entries.e[20].id = data_ov004_020bc4cc[GetGameLanguage()];
    entries.e[20].ptr = data_ov004_020bc2d8[GetGameLanguage()];
    entries.e[21].id = data_ov004_020bc4e0[GetGameLanguage()];
    entries.e[21].ptr = data_ov004_020bc2ec[GetGameLanguage()];
    entries.e[22].id = data_ov004_020bc38c[GetGameLanguage()];
    entries.e[22].ptr = data_ov004_020bc508[GetGameLanguage()];
    entries.e[23].id = data_ov004_020bc314[GetGameLanguage()];
    entries.e[23].ptr = data_ov004_020bc51c[GetGameLanguage()];
    entries.e[24].id = data_ov004_020bc328[GetGameLanguage()];
    entries.e[24].ptr = data_ov004_020bc530[GetGameLanguage()];
    entries.e[25].id = data_ov004_020bc6d4[GetGameLanguage()];
    entries.e[25].ptr = data_ov004_020bc544[GetGameLanguage()];
    entries.e[26].id = data_ov004_020bc6c0[GetGameLanguage()];
    entries.e[26].ptr = data_ov004_020bc5a8[GetGameLanguage()];
    entries.e[27].id = data_ov004_020bc33c[GetGameLanguage()];
    entries.e[27].ptr = data_ov004_020bc558[GetGameLanguage()];
    entries.e[28].id = data_ov004_020bc364[GetGameLanguage()];
    entries.e[28].ptr = data_ov004_020bc56c[GetGameLanguage()];

    for (i = 0; i < 29; i++) {
        int t;
        t = LoadFile(entries.e[i].id);
        ((volatile int *)data_ov004_020bf560)[i] = t;
        t = (int)&entries.e[i];
        t = *(volatile int *)(t + 4);
        t = *(int *)t;
        data_ov004_020bf5d4[i] = t;
    }
}
#pragma opt_strength_reduction on

// @symbol func_ov004_020b3194
extern "C" void func_ov004_020b3194(Elem *e)
{
    e->mFileIndex = 0x1d;
    s16 idx = e->mGfxSlot;
    data_ov004_020bf3e8[idx] = 0;
}

// @symbol func_ov004_020b31b4
extern "C" void func_ov004_020b31b4(Elem *e) {
    if (e->mFileIndex == 0x1d) return;
    if (e->mRender != 0) {
        (e->*(e->mRender))();
        return;
    }
    func_ov004_020b42c0(e);
}

// @symbol func_ov004_020b321c
extern "C" void func_ov004_020b321c(Elem *e) {
    if (e->mFileIndex == 0x1d) return;
    if (e->mBehavior == 0) return;
    (e->*(e->mBehavior))();
}

// @symbol func_ov004_020b3278
extern "C" void func_ov004_020b3278(Elem *e, int file, short x, short y, int screen, int priority, short style)
{
    int a, b;

    if (data_ov004_020beb68->OnHitByCannonBlastedChar() == 2) {
        a = 0x6400000;
        b = 0;
    } else {
        a = 0x6600000;
        b = 0x6400000;
    }

    switch (file) {
    case 3: case 4: case 5: case 6:
    case 8: case 9: case 10: case 11: case 12:
    case 14: case 16: case 17: case 18: case 19: case 20: case 21:
        if (data_ov004_020bf3e8[0] != 0)
            return;
        DecompressLZ16((void *)data_ov004_020bf560[file], (void *)(a + 0x7000));
        if (b != 0)
            DecompressLZ16((void *)data_ov004_020bf560[file], (void *)(b + 0x7000));
        e->mGfxSlot = 0;
        data_ov004_020bf3e8[0] = 1;
        break;
    default:
        if (data_ov004_020bf3e8[1] == 0) {
            DecompressLZ16((void *)data_ov004_020bf560[file], (void *)(a + 0x6000));
            if (b != 0)
                DecompressLZ16((void *)data_ov004_020bf560[file], (void *)(b + 0x6000));
            e->mGfxSlot = 1;
            data_ov004_020bf3e8[1] = 1;
            break;
        }
        if (data_ov004_020bf3e8[2] == 0) {
            DecompressLZ16((void *)data_ov004_020bf560[file], (void *)(a + 0x6800));
            if (b != 0)
                DecompressLZ16((void *)data_ov004_020bf560[file], (void *)(b + 0x6800));
            e->mGfxSlot = 2;
            data_ov004_020bf3e8[2] = 1;
            break;
        }
        return;
    }

    e->mFileIndex = file;
    e->mX = x;
    e->mY = y;
    e->mTargetX = e->mX;
    e->mTargetY = e->mY;
    e->mScreen = screen;
    e->mPriority = priority;
    e->mArrived = 0;

    if (style != 0xd) {
        e->mStyle = style;
    } else {
        switch (file) {
        case 11:
            e->mStyle = 7;
            break;
        case 3: case 4: case 5: case 6: case 20: case 21:
            e->mStyle = 8;
            break;
        case 8: case 14:
            e->mStyle = 9;
            break;
        case 0:
            e->mStyle = 3;
            break;
        case 1: case 2:
            e->mStyle = 4;
            break;
        case 13:
            e->mStyle = 0xc;
            break;
        default:
            e->mStyle = 0;
            break;
        }
    }

    {
        S3 tmp = data_ov004_020bc27c;
        e->mOamCount = (short)func_ov004_020af5e0(
            data_ov004_020bf5d4[e->mFileIndex],
            e->mOam,
            tmp.v[e->mGfxSlot],
            e->mFileIndex);
    }

    {
        int st = e->mStyle;
        if (((ElemFn *)data_ov004_020bf490)[st])
            (e->*((ElemFn *)data_ov004_020bf490)[st])();
    }

    {
        short st;
        PairW *r;
        st = e->mStyle;
        r = &data_ov004_020bf428[st];
        a = data_ov004_020bf428[st].w0;
        b = r->w1;
        ((int *)&e->mBehavior)[0] = b ? a : a;
        ((int *)&e->mBehavior)[1] = b;
        st = e->mStyle;
        r = &data_ov004_020bf4f8[st];
        a = data_ov004_020bf4f8[st].w0;
        b = r->w1;
        ((int *)&e->mRender)[0] = b ? a : a;
        ((int *)&e->mRender)[1] = b;
    }
}

// @symbol func_ov004_020b35d8
extern "C" void func_ov004_020b35d8(Elem *e)
{
  int x;
  int originX = e->mX;
  int n;
  s16 drawY;
  /* the first OAM record's 9-bit signed x, made positive */
  int oamX = (int) (((unsigned int) (*(int *)e->mOam << 7)) >> 0x17);
  int w;
  unsigned long left;
  n = func_ov004_020b1aec() + 1;
  if (oamX > 0x100)
  {
    oamX -= 0x200;
  }
  if (oamX < 0)
  {
    oamX = -oamX;
  }
  x = (x = (e->mX + oamX) + 0x10);
  w = 0x10;
  {
    int p = 1;
    while (p < n)
    {
      p *= 10;
      w += 0x10;
    }

  }
  /* both draws shift left by w/2; w grows 0x10 per power of ten below n */
  x = x - (w >> 1);
  left = w >> 1;
  left = originX - left;
  func_ov004_020b1cf0((int) e->mOam, left, e->mY, e->mScreen, e->mPriority);
  drawY = e->mY;
  func_ov004_020b2444(x, drawY, n, e->mScreen, e->mPriority, 2, 0);
}

// @symbol func_ov004_020b3698
extern "C" void func_ov004_020b3698(void)
{
}

// @symbol func_ov004_020b369c
extern "C" void func_ov004_020b369c(Elem *e) {
    u8* el = e->mOam;
    int angle = (u16)e->mTimer;
    M m;
    u16 flag;
    do {
        int s0 = data_02082214[(angle >> 4) * 2];
        int mm = (int)(((s64)s0 * 0x800 + 0x800) >> 12);
        int idx = (u16)mm >> 4;
        int cos1 = data_02082214[idx * 2 + 1];
        int sin1 = data_02082214[idx * 2];
        int fc = (int)(((s64)cos1 * 0x1000 + 0x800) >> 12);
        int fs = (int)(((s64)sin1 * 0x1000 + 0x800) >> 12);
        m._00 = fc;
        m._11 = fc;
        m._01 = fs;
        m._10 = -fs;
        func_ov004_020b1c68(el, e->mX, e->mY,
                            e->mScreen, e->mPriority, &m);
        angle = (u16)(angle + 0x4000);
        flag = *(u16*)(el + 6);
        el += 8;
    } while (flag != 0xffff);
}

// @symbol func_ov004_020b37c4
extern "C" void func_ov004_020b37c4(Elem *e) {
    if (ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000)) {
        e->mTimer = 0;
    }
}

// @symbol func_ov004_020b37f0
extern "C" void func_ov004_020b37f0(Elem *e){
  e->mTimer=0;
  e->mTimerEnd=0x27000;
  *(W2*)&e->mBehavior=*(W2*)data_ov004_020bc20c;
  *(W2*)&e->mRender=*(W2*)data_ov004_020bc1b4;
}

// @symbol func_ov004_020b3834
extern "C" void func_ov004_020b3834(Elem *e){
    if (e->mTimer < 0x30){
        *(unsigned *)&e->mTimer += 1;
        return;
    }
    if (ApproachLinear2(e->mY, e->mTargetY, 0x10)){
        func_ov004_020b37f0(e);
    }
}

// @symbol func_ov004_020b3888
extern "C" void func_ov004_020b3888(Elem *e) {
    e->mTimer = 0;
    e->mY = -(data_ov004_020beb6c + 0x60);
}

// @symbol func_ov004_020b38ac
extern "C" void func_ov004_020b38ac(Elem *e) {
    u8* el = e->mOam;
    int x = e->mTimer >> 2;
    int t = 0x1000;
    int base = t;

    u16 flag;
    do {
        int v = base;
        int d = x - t;
        if (d < 0) d = -d;
        if (d < 0x1000) v += (int)(((s64)(0x1000 - d) * 0x400 + 0x800) >> 12);
        v = func_02053200(v);
        {
            M m = {0};
            m._00 = v;
            m._11 = v;
            func_ov004_020b1c68(el, e->mX, e->mY,
                                e->mScreen, e->mPriority, &m);
        }
        t += 0x1000;
        flag = *(u16*)(el + 6);
        el += 8;
    } while (flag != 0xffff);
}

// @symbol func_ov004_020b3978
extern "C" void func_ov004_020b3978(Elem *e) {
    if (ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000)) {
        e->mTimer = 0;
    }
}

// @symbol func_ov004_020b39a4
extern "C" void func_ov004_020b39a4(Elem* e) {
    int i;
    int angle;
    int boxA[2], boxB[2], boxC[2];
    int posA[2];
    int outA[2], outB[2];
    int flags;

    int shifted;

    e->mTimer = 0;
    angle = e->mOamCount;
    angle = angle + 1;
    shifted = angle << 14;
    e->mTimerEnd = shifted;

    *(W2*)&e->mBehavior = *(W2*)&data_ov004_020bc17c;
    *(W2*)&e->mRender   = *(W2*)&data_ov004_020bc1ec;

    boxA[0] = 0; boxA[1] = -0x2000;
    boxB[0] = 0; boxB[1] = -0x1800;
    boxC[0] = 0; boxC[1] = -0x4000;

    flags = data_ov004_020bf3ec;

    {
        int y12 = e->mY;
        int y10 = e->mX;
        int sY12, sY10;
        flags = flags & 1;
        sY12 = y12 << 12;
        sY10 = y10 << 12;
        posA[0] = sY10;
        posA[1] = sY12;
    }

    if (flags == 0) {
        data_ov004_020bf3f4[0] = 0;
        data_ov004_020bf3f4[1] = 0xc0;
        __register_global_object(data_ov004_020bf3f4, (void *)NullDestructor_0203d47c, data_ov004_020bf410);
        data_ov004_020bf3ec = data_ov004_020bf3ec | 1;
    }

    for (i = 0; i < 0x20; i++) {
        int r;
        int ang;
        s16 spawnAngle;

        func_0203d704(outA, posA, boxC);
        func_0203d388(boxA, 0x800);
        func_0203d388(boxC, 0x800);
        func_0203d704(outB, boxB, boxA);

        r = RandomIntInternal(&data_0209e650);
        r &= 0x7fffffff;
        ang = (int)((u32)r >> 0x13) * 0x3c >> 0xc;
        ang = ang + 0x5a;
        spawnAngle = (s16)ang;

        func_ov004_020adfc4(0, spawnAngle, outA, outB, data_ov004_020bf3f4);
    }
}

// @symbol func_ov004_020b3b38
#pragma opt_propagation off
extern "C" void func_ov004_020b3b38(Elem* e) {
    int m, fd, t, v;
    unsigned short term;
    if (e->mTimer == 0) return;
    m = (int)(((s64)e->mTimer * 0xcc + 0x800) >> 12);
    {
        u8* el = e->mOam;
        fd = _ZN4cstd4fdivEii(m, 0x1000);
        t = (int)(((s64)fd * 0x1000 + 0x800) >> 12);
        v = func_02053200(m);
        {
            M buf = {0};
            buf._00 = v;
            buf._11 = v;
            t = 0x1000 - t;
            do {
                s16 f = (s16)(((unsigned int)(*(int*)el << 7)) >> 23);
                int off;
                if (f > 0x100) f -= 0x200;
                off = f * t;
                func_ov004_020b1c68(el,
                    e->mX - ((off << 4) >> 16),
                    e->mY, e->mScreen,
                    e->mPriority, &buf);
                term = *(unsigned short*)(el + 6);
                el += 8;
            } while (term != 0xffff);
        }
    }
}
#pragma opt_propagation on

// @symbol func_ov004_020b3c58
extern "C" void func_ov004_020b3c58(Elem *e)
{
    ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000);
    if (ApproachLinear2(e->mY, e->mTargetY, 0x10) == 0)
        return;
    func_ov004_020b39a4(e);
}

// @symbol func_ov004_020b3c9c
extern "C" void func_ov004_020b3c9c(Elem *e)
{
    e->mTimer = 0;
    e->mTimerEnd = 81920;
    e->mY = 96;
}

// @symbol func_ov004_020b3cb8
extern "C" void func_ov004_020b3cb8(Elem *e) {
    u16 flag;
    if (e->mTimer == 0) return;
    {
        u8* el = e->mOam;
        if (e->mTimer < 0x18000) {
            int v = 0x2000;
            if (e->mTimer >= 0x14000) {
                v = 0x1000;
            } else {
                int t = _ZN4cstd4fdivEii(0x1000, 0x18000);
                v -= (int)(((s64)e->mTimer * t + 0x800) >> 12);
                v = func_02053200(v);
            }
            {
                M m = {0};
                m._00 = v;
                m._11 = v;
                do {
                    func_ov004_020b1c68(el, e->mX, e->mY,
                                        e->mScreen, e->mPriority, &m);
                    flag = *(u16*)(el + 6);
                    el += 8;
                } while (flag != 0xffff);
            }
        } else {
            int angle = (u16)(e->mTimer - 0x18000);
            M m2;
            do {
                int s0 = data_02082214[(angle >> 4) * 2];
                int mm = (int)(((s64)s0 * 0x800 + 0x800) >> 12);
                int idx = (u16)mm >> 4;
                int cos1 = data_02082214[idx * 2 + 1];
                int sin1 = data_02082214[idx * 2];
                int fc = (int)(((s64)cos1 * 0x1000 + 0x800) >> 12);
                int fs = (int)(((s64)sin1 * 0x1000 + 0x800) >> 12);
                m2._00 = fc;
                m2._11 = fc;
                m2._01 = fs;
                m2._10 = -fs;
                func_ov004_020b1c68(el, e->mX, e->mY,
                                    e->mScreen, e->mPriority, &m2);
                angle = (u16)(angle + 0x4000);
                flag = *(u16*)(el + 6);
                el += 8;
            } while (flag != 0xffff);
        }
    }
}

// @symbol func_ov004_020b3e9c
extern "C" void func_ov004_020b3e9c(Elem *e) {
    u16 flag;
    if (e->mTimer == 0) return;
    {
        u8* el = e->mOam;
        if (e->mTimer < 0x18000) {
            int v = 0x2000;
            if (e->mTimer >= 0x14000) {
                v = 0x1000;
            } else {
                int t = _ZN4cstd4fdivEii(0x1000, 0x18000);
                v -= (int)(((s64)e->mTimer * t + 0x800) >> 12);
                v = func_02053200(v);
            }
            {
                M m = {0};
                m._00 = v;
                m._11 = v;
                do {
                    func_ov004_020b1c68(el, e->mX, e->mY,
                                        e->mScreen, e->mPriority, &m);
                    flag = *(u16*)(el + 6);
                    el += 8;
                } while (flag != 0xffff);
            }
        } else {
            int angle = (u16)(e->mTimer - 0x18000);
            M m2;
            do {
                int s0 = data_02082214[(angle >> 4) * 2];
                int mm = (int)(((s64)s0 * 0x800 + 0x800) >> 12);
                int idx = (u16)mm >> 4;
                int cos1 = data_02082214[idx * 2 + 1];
                int sin1 = data_02082214[idx * 2];
                int fc = (int)(((s64)cos1 * 0x1000 + 0x800) >> 12);
                int fs = (int)(((s64)sin1 * 0x1000 + 0x800) >> 12);
                m2._00 = fc;
                m2._11 = fc;
                m2._01 = fs;
                m2._10 = -fs;
                func_ov004_020b1c68(el, e->mX, e->mY,
                                    e->mScreen, e->mPriority, &m2);
                angle = (u16)(angle + 0x4000);
                flag = *(u16*)(el + 6);
                el += 8;
            } while (flag != 0xffff);
        }
    }
}

// @symbol func_ov004_020b4080
extern "C" void func_ov004_020b4080(Elem *e) {
    if (ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000)) {
        e->mTimer = 98304;
    }
}

// @symbol func_ov004_020b40ac
extern "C" void func_ov004_020b40ac(Elem *e)
{
    e->mTimer = 0;
    e->mTimerEnd = 159744;
}

// @symbol func_ov004_020b40c0
extern "C" int func_ov004_020b40c0(Elem* e){
  int* d = data_ov004_020bc254;
  int* o = (int*)&e->mBehavior;
  if(o[0] == d[0] && (o[1] == d[1] || o[0] == 0) && e->mTimer == e->mTimerEnd)
    return 1;
  return 0;
}

// @symbol func_ov004_020b410c
extern "C" void func_ov004_020b410c(Elem *e)
{
    u8* el = e->mOam;
    int x = e->mTimer;
    int idx = (int)(((unsigned int)x << 15) >> 16) >> 4;
    s16 s = *(s16*)((int)data_02082214 + (idx << 2));
    int v = func_02053200((int)((((s64)s << 11) + 0x800) >> 12) + 0x1000);
    M m = {0};
    u16 lim = 0xffff;
    u16 flag;
    m._00 = v;
    m._11 = v;
    do {
        func_ov004_020b1c68(el,
            e->mX, e->mY,
            e->mScreen, e->mPriority, &m);
        flag = *(u16*)(el + 6);
        el += 8;
    } while (flag != lim);

    x = e->mFileIndex;
    if (x != 0 && x != 1 && x != 2)
        return;
    func_ov004_020b1cf0(data_ov006_021346bc,
        e->mX, e->mY,
        e->mScreen, e->mPriority);
}

// @symbol func_ov004_020b4214
extern "C" void func_ov004_020b4214(Elem *e) {
    ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000);
}

// @symbol func_ov004_020b422c
extern "C" void func_ov004_020b422c(Elem* e){
  data_ov004_020bc150 = 0;
  switch (e->mFileIndex) {
  case 0:
    func_02012790(0x62);
    break;
  case 1:
  case 2:
    func_02012790(0x63);
    break;
  }
  e->mTimer = 0;
  e->mTimerEnd = 0x10000;
  *(W2*)&e->mBehavior = *(W2*)&data_ov004_020bc224;
  *(W2*)&e->mRender = *(W2*)&data_ov004_020bc274;
}

// @symbol func_ov004_020b42c0
extern "C" void func_ov004_020b42c0(Elem *e) {
    func_ov004_020b1cf0((int)e->mOam, e->mX, e->mY, e->mScreen, e->mPriority);
    if (e->mFileIndex != 0 && e->mFileIndex != 1 && e->mFileIndex != 2)
        return;
    func_ov004_020b1cf0(data_ov006_021346bc, e->mX, e->mY, e->mScreen, e->mPriority);
}

// @symbol func_ov004_020b433c
extern "C" void func_ov004_020b433c(Elem *e) {
    e->mTimer = 0;
    e->mTimerEnd = ((e->mOamCount + 1) << 2) + 0x1e << 0xc;
}

// @symbol func_ov004_020b4360
#pragma opt_propagation off
extern "C" void func_ov004_020b4360(Elem *e) {
    u16 flag;
    int val = e->mTimer;
    if (val == 0) return;
    {
        u8* el = e->mOam;
        if (val < 0x1e000) {
            int d;
            int scale;
            if (val < 0x14000) {
                int t;
                d = (int)(((s64)val * 0x100 + 0x800) >> 12);
                t = _ZN4cstd4fdivEii(d, 0x1400);
                scale = (int)(((s64)t * 0x1400 + 0x800) >> 12);
            } else {
                int a = (int)(((s64)(val - 0x14000) * 0x66 + 0x800) >> 12);
                int b = 0x1400 - a;
                int t = _ZN4cstd4fdivEii(b - 0x1000, 0x400);
                scale = (0x1000 - t) + (int)(((s64)t * 0x1400 + 0x800) >> 12);
                d = b;
            }
            d = func_02053200(d);
            {
                M m = {0};
                m._00 = d;
                m._11 = d;
                scale = 0x1000 - scale;
                do {
                    s16 f = (s16)(((unsigned int)(*(int*)el << 7)) >> 23);
                    int off;
                    if (f > 0x100) f -= 0x200;
                    off = f * scale;
                    func_ov004_020b1c68(el,
                                        e->mX - ((off << 4) >> 16),
                                        e->mY, e->mScreen,
                                        e->mPriority, &m);
                    flag = *(u16*)(el + 6);
                    el += 8;
                } while (flag != 0xffff);
            }
        } else {
            int idx = (val - 0x1e000) >> 2;
            int i = 0x1000;
            do {
                int diff = idx - i;
                int r = 0x1000;
                if (diff < 0) diff = -diff;
                if (diff < 0x1000) {
                    r += (int)(((s64)(0x1000 - diff) * 0x400 + 0x800) >> 12);
                }
                r = func_02053200(r);
                {
                    M m2 = {0};
                    m2._00 = r;
                    m2._11 = r;
                    func_ov004_020b1c68(el, e->mX, e->mY,
                                        e->mScreen, e->mPriority, &m2);
                }
                i += 0x1000;
                flag = *(u16*)(el + 6);
                el += 8;
            } while (flag != 0xffff);
        }
    }
}
#pragma opt_propagation on

// @symbol func_ov004_020b45c0
#pragma opt_propagation off
extern "C" void func_ov004_020b45c0(Elem *e) {
    u16 flag;
    int val = e->mTimer;
    if (val == 0) return;
    {
        u8* el = e->mOam;
        if (val < 0x1e000) {
            int d;
            int scale;
            if (val < 0x14000) {
                int t;
                d = (int)(((s64)val * 0x100 + 0x800) >> 12);
                t = _ZN4cstd4fdivEii(d, 0x1400);
                scale = (int)(((s64)t * 0x1400 + 0x800) >> 12);
            } else {
                int a = (int)(((s64)(val - 0x14000) * 0x66 + 0x800) >> 12);
                int b = 0x1400 - a;
                int t = _ZN4cstd4fdivEii(b - 0x1000, 0x400);
                scale = (0x1000 - t) + (int)(((s64)t * 0x1400 + 0x800) >> 12);
                d = b;
            }
            d = func_02053200(d);
            {
                M m = {0};
                m._00 = d;
                m._11 = d;
                scale = 0x1000 - scale;
                do {
                    s16 f = (s16)(((unsigned int)(*(int*)el << 7)) >> 23);
                    int off;
                    if (f > 0x100) f -= 0x200;
                    off = f * scale;
                    func_ov004_020b1c68(el,
                                        e->mX - ((off << 4) >> 16),
                                        e->mY, e->mScreen,
                                        e->mPriority, &m);
                    flag = *(u16*)(el + 6);
                    el += 8;
                } while (flag != 0xffff);
            }
        } else {
            int idx = (val - 0x1e000) >> 2;
            int i = 0x1000;
            do {
                int diff = idx - i;
                int r = 0x1000;
                if (diff < 0) diff = -diff;
                if (diff < 0x1000) {
                    r += (int)(((s64)(0x1000 - diff) * 0x400 + 0x800) >> 12);
                }
                r = func_02053200(r);
                {
                    M m2 = {0};
                    m2._00 = r;
                    m2._11 = r;
                    func_ov004_020b1c68(el, e->mX, e->mY,
                                        e->mScreen, e->mPriority, &m2);
                }
                i += 0x1000;
                flag = *(u16*)(el + 6);
                el += 8;
            } while (flag != 0xffff);
        }
    }
}
#pragma opt_propagation on

// @symbol func_ov004_020b4820
extern "C" void func_ov004_020b4820(Elem *e) {
    if (ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000)) {
        e->mTimer = 122880;
    }
}

// @symbol func_ov004_020b484c
extern "C" void func_ov004_020b484c(Elem* e)
{
    int i;
    int boxA[2], boxB[2], boxC[2];
    int posA[2];
    int outA[2], outB[2];
    int flags;
    int r;
    int ang;
    s16 spawnAngle;

    e->mTimer = 0;
    e->mTimerEnd = (((e->mOamCount + 1) << 2) + 0x1e) << 0xc;

    boxA[0] = 0;
    boxA[1] = -0x2000;
    boxB[0] = 0;
    boxB[1] = -0x1800;
    boxC[0] = 0;
    boxC[1] = -0x4000;

    flags = data_ov004_020bf3f0;

    {
        int y12 = e->mY;
        int y10 = e->mX;
        int sY12, sY10;
        flags = flags & 1;
        sY12 = y12 << 12;
        sY10 = y10 << 12;
        posA[0] = sY10;
        posA[1] = sY12;
    }

    if (flags == 0) {
        data_ov004_020bf3fc[0] = 0;
        data_ov004_020bf3fc[1] = 0xc0;
        __register_global_object(data_ov004_020bf3fc, (void*)NullDestructor_0203d47c, data_ov004_020bf404);
        data_ov004_020bf3f0 = data_ov004_020bf3f0 | 1;
    }

    for (i = 0; i < 0x20; i++) {
        func_0203d704(outA, posA, boxC);
        func_0203d388(boxA, 0x800);
        func_0203d388(boxC, 0x800);
        func_0203d704(outB, boxB, boxA);

        r = RandomIntInternal(&data_0209e650);
        r &= 0x7fffffff;
        ang = (int)((u32)r >> 0x13) * 0x3c >> 0xc;
        ang = ang + 0x5a;
        spawnAngle = (s16)ang;

        func_ov004_020adfc4(0, spawnAngle, outA, outB, data_ov004_020bf3fc);
    }
}

// @symbol func_ov004_020b49b8
extern "C" void func_ov004_020b49b8(Elem *e) {
    if (ApproachLinear2(e->mX, e->mTargetX, 0x10))
        e->mArrived = 1;
}

// @symbol func_ov004_020b49e4
extern "C" void func_ov004_020b49e4(Elem *e)
{
    e->mX = 320;
}

// @symbol func_ov004_020b49f0
extern "C" void func_ov004_020b49f0(Elem *e) {
    if (ApproachLinear2(e->mX, e->mTargetX, 0x10))
        e->mArrived = 1;
}

// @symbol func_ov004_020b4a1c
extern "C" void func_ov004_020b4a1c(Elem *e)
{
    e->mX = -0x40;
}

// @symbol func_ov004_020b4a28
extern "C" void func_ov004_020b4a28(Elem *e) {
    ApproachLinear2(e->mY, e->mTargetY, 4);
}

// @symbol func_ov004_020b4a40
extern "C" void func_ov004_020b4a40(Elem *e)
{
    e->mY = 200;
}

// @symbol func_ov004_020b4a4c
extern "C" void func_ov004_020b4a4c(Elem *e) {
    ApproachLinear2(e->mY, e->mTargetY, 4);
}

// @symbol func_ov004_020b4a64
extern "C" void func_ov004_020b4a64(Elem *e)
{
    e->mY = -8;
}

Elem::Elem()
{
    mFileIndex = 29;
}

/* __sinit_ov004_020b955c sources: the member-pointer records the three
   per-style tables copy (setup data_ov004_020bf490, behavior
   data_ov004_020bf428, render data_ov004_020bf4f8), and the words the
   29-entry file-id table copies. */
extern "C" {
extern PairW data_02086b58;  /* __ptmf_null's address: the null member pointer */
extern PairW data_ov004_020bc234, data_ov004_020bc23c, data_ov004_020bc244, data_ov004_020bc16c;
extern PairW data_ov004_020bc1bc, data_ov004_020bc1ac, data_ov004_020bc25c, data_ov004_020bc264;
extern PairW data_ov004_020bc1c4, data_ov004_020bc164, data_ov004_020bc15c, data_ov004_020bc1d4;
extern PairW data_ov004_020bc22c, data_ov004_020bc19c, data_ov004_020bc184, data_ov004_020bc204;
extern PairW data_ov004_020bc18c, data_ov004_020bc1fc, data_ov004_020bc1f4, data_ov004_020bc154;
extern PairW data_ov004_020bc1dc, data_ov004_020bc1e4, data_ov004_020bc214;
extern PairW data_ov004_020bc1a4, data_ov004_020bc24c, data_ov004_020bc21c, data_ov004_020bc26c;
extern PairW data_ov004_020bc1cc, data_ov004_020bc174, data_ov004_020bc194;
extern int data_ov004_020b9e98, data_ov004_020b9e8c, data_ov004_020b9f2c;
extern int data_ov004_020b9ea4, data_ov004_020b9fd8, data_ov004_020b9eb8, data_ov004_020b9f5c;
extern int data_ov004_020ba010, data_ov004_020b9e90, data_ov004_020b9ed4, data_ov004_020b9ffc;
extern int data_ov004_020b9f78, data_ov004_020b9fa0, data_ov004_020b9e7c, data_ov004_020b9ebc;
extern int data_ov004_020b9ee4, data_ov004_020b9ea8, data_ov004_020b9ef0, data_ov004_020b9f7c;
extern int data_ov004_020b9f0c, data_ov004_020b9e50, data_ov004_020b9e70, data_ov004_020b9f60;
extern int data_ov004_020b9efc, data_ov004_020b9f88, data_ov004_020b9e78, data_ov004_020b9fc8;
extern int data_ov004_020b9ee8;
}

namespace s20b955c {
extern "C" {
Elem data_ov004_020bf648[3];
}
}

extern "C" {
int data_ov004_020bf5d4[29] = {
    data_ov004_020b9e98, data_ov004_020b9f54, data_ov004_020b9e8c, data_ov004_020b9f2c,
    data_ov004_020b9ea4, data_ov004_020b9fd8, data_ov004_020b9eb8, data_ov004_020b9f5c,
    data_ov004_020ba010, data_ov004_020b9e90, data_ov004_020b9ed4, data_ov004_020b9ffc,
    data_ov004_020b9f78, data_ov004_020b9fa0, data_ov004_020b9e7c, data_ov004_020b9ebc,
    data_ov004_020b9ee4, data_ov004_020b9ea8, data_ov004_020b9ef0, data_ov004_020b9f7c,
    data_ov004_020b9f0c, data_ov004_020b9e50, data_ov004_020b9e70, data_ov004_020b9f60,
    data_ov004_020b9efc, data_ov004_020b9f88, data_ov004_020b9e78, data_ov004_020b9fc8,
    data_ov004_020b9ee8
};
PairW data_ov004_020bf490[13] = {
    data_02086b58, data_ov004_020bc234, data_ov004_020bc23c, data_ov004_020bc244,
    data_ov004_020bc16c, data_ov004_020bc1bc, data_ov004_020bc1ac, data_ov004_020bc25c,
    data_ov004_020bc264, data_ov004_020bc1c4, data_ov004_020bc164, data_ov004_020bc15c,
    data_ov004_020bc1d4
};
PairW data_ov004_020bf428[13] = {
    data_02086b58, data_ov004_020bc22c, data_ov004_020bc19c, data_ov004_020bc184,
    data_ov004_020bc204, data_ov004_020bc18c, data_ov004_020bc1fc, data_ov004_020bc1f4,
    data_ov004_020bc154, data_ov004_020bc1dc, data_ov004_020bc1e4, data_ov004_020bc214,
    data_02086b58
};
PairW data_ov004_020bf4f8[13] = {
    data_02086b58, data_02086b58, data_02086b58, data_02086b58, data_02086b58,
    data_ov004_020bc1a4, data_ov004_020bc24c, data_ov004_020bc21c, data_ov004_020bc26c,
    data_ov004_020bc1cc, data_ov004_020bc174, data_02086b58, data_ov004_020bc194
};
int data_ov004_020bf560[29];
}
