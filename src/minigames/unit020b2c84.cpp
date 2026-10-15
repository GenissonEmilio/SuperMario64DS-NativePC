//cpp
/* unit020b2c84 -- the 44 functions of the ov004 linker unit 0x020b2c84..0x020b4aa4.
 *
 * This is the unlabelled run between the end of dScMgBase_c's unit and the unit
 * that starts at 0x020b4aa4. No class label, RTTI record or vtable falls in it,
 * so the file keeps the unit<addr> name. What the functions do, from their
 * bodies: the first group manages a table of 29 loaded files (Mg_FreeBannerFiles
 * frees them, Mg_LoadBannerFiles loads them by language, Banner_Init
 * decompresses one into a graphics slot and sets up an Elem to show it), and
 * the rest are the behavior and render handlers of Elem, the 0x134-byte record
 * of which gMgBanners holds three. An Elem draws its graphic as a
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
 * gMgBanners; __arraydtor$954 runs __cxa_vec_cleanup over it.
 * The unit emits one data object of its own, the anonymous initializer template
 * of Mg_LoadBannerFiles's local array at 0x020bc6e8 (0xe8 bytes), which this
 * file's .data claim covers. Everything else it touches is reached by extern.
 *
 * Emission order. `#pragma defer_codegen off` is load-bearing here: with codegen
 * deferred the opt_* pragmas around Mg_LoadBannerFiles, Banner_DrawSqueeze,
 * Banner_DrawZoomA and Banner_DrawZoomB do not bind to the one function
 * they bracket. Source is therefore ROM-ascending.
 *
 * The bodies were matched as one-function C or C++ sources and keep that
 * spelling under extern "C" and their ROM names.
 *
 * Numbered names. mwccarm numbers its generated names from one counter that
 * every named parameter (prototypes included), local and temporary before a
 * definition advances. The array gMgBanners at the end of the file
 * gets __arraydtor$954 (and @955), and symbols.txt and the manifest name
 * __arraydtor$954, so an edit here has to keep that count. The named
 * parameters on the ApproachLinear2 and __cxa_vec_cleanup prototypes stand
 * in for names removed elsewhere, and the
 * six record copies in Banner_StartSpin, Banner_StartWaveBurst and
 * Banner_Press stay W2 (int[2]) copies: assigning the records as
 * ElemFn emits the same code but moves the counter by 8 (measured).
 * Banner_Init's `b ? a : a` copies are a code shape, not a count:
 * a plain record copy there changes 12 words of that function (measured).
 *
 * Retired one-function sources (ROM address order):
 *   0x020b2c84  Mg_FreeBannerFiles
 *   0x020b2cb8  Mg_LoadBannerFiles
 *   0x020b3194  Banner_Free
 *   0x020b31b4  Banner_Draw
 *   0x020b321c  Banner_Update
 *   0x020b3278  Banner_Init
 *   0x020b35d8  Banner_DrawLevelNumber
 *   0x020b3698  Banner_SetupNone
 *   0x020b369c  Banner_DrawSpin
 *   0x020b37c4  Banner_UpdateSpin
 *   0x020b37f0  Banner_StartSpin
 *   0x020b3834  Banner_UpdateDrop
 *   0x020b3888  Banner_SetupDrop
 *   0x020b38ac  Banner_DrawWave
 *   0x020b3978  Banner_UpdateWave
 *   0x020b39a4  Banner_StartWaveBurst
 *   0x020b3b38  Banner_DrawSqueeze
 *   0x020b3c58  Banner_UpdateFall
 *   0x020b3c9c  Banner_SetupFall
 *   0x020b3cb8  Banner_DrawPopSpinA
 *   0x020b3e9c  Banner_DrawPopSpinB
 *   0x020b4080  Banner_UpdatePopSpin
 *   0x020b40ac  Banner_SetupPopSpin
 *   0x020b40c0  Banner_IsPressFinished
 *   0x020b410c  Banner_DrawPressed
 *   0x020b4214  Banner_UpdatePressed
 *   0x020b422c  Banner_Press
 *   0x020b42c0  Banner_DrawPlain
 *   0x020b433c  Banner_SetupZoom
 *   0x020b4360  Banner_DrawZoomA
 *   0x020b45c0  Banner_DrawZoomB
 *   0x020b4820  Banner_UpdateZoom
 *   0x020b484c  Banner_SetupZoomBurst
 *   0x020b49b8  Banner_UpdateSlideFromRight
 *   0x020b49e4  Banner_SetupSlideFromRight
 *   0x020b49f0  Banner_UpdateSlideFromLeft
 *   0x020b4a1c  Banner_SetupSlideFromLeft
 *   0x020b4a28  Banner_UpdateSlideFromBottom
 *   0x020b4a40  Banner_SetupSlideFromBottom
 *   0x020b4a4c  Banner_UpdateSlideFromTop
 *   0x020b4a64  Banner_SetupSlideFromTop
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

/* The minigame scene at gMgScene, seen through the one virtual
   used here. Slot 26 is dScMgBase_c.h's GetDisplayMode; its scene
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
    virtual int GetDisplayMode();  /* slot 26 */
};

/* One of the three 0x134-byte records at gMgBanners: a graphic
   loaded from the file table and drawn as a list of OAM records. The class
   name Elem is the one its constructor and destructor symbols carry; the
   real class is not recovered. */
struct Elem;
typedef void (Elem::*ElemFn)();
struct Elem {
    ElemFn mBehavior;    /* 0x00 run by Banner_Update */
    ElemFn mRender;      /* 0x08 run by Banner_Draw; 0 draws plainly */
    s16 mX, mY;          /* 0x10 screen position, pixels */
    s16 mTargetX;        /* 0x14 where the slide-in handlers stop */
    s16 mTargetY;        /* 0x16 */
    int mPriority;       /* 0x18 passed right after mScreen to every draw call */
    int mScreen;         /* 0x1c -1 draws on both screens (Mg_RenderSpriteOn) */
    int mFileIndex;      /* 0x20 index into the file table; 29 when free */
    int mTimer;          /* 0x24 mostly stepped 0x1000 (1.0) per frame toward
                            mTimerEnd; Banner_UpdateDrop counts it by 1 */
    int mTimerEnd;       /* 0x28 */
    s16 mOamCount;       /* 0x2c records Mg_CopySpriteList copied */
    s16 mStyle;          /* 0x2e index into the three handler tables */
    s16 mGfxSlot;        /* 0x30 0..2, see sMgBannerGfxSlotUsed */
    s16 mArrived;        /* 0x32 set when mX reaches mTargetX
                            (Banner_UpdateSlideFromRight, Banner_UpdateSlideFromLeft) */
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
int Mg_CopySpriteList(int a, void *b, int c, int d);
int _ZN4cstd4fdivEii(int a, int b);
int func_02053200(int x);
unsigned int func_02012790(unsigned int x);
void __register_global_object(void *object, void *destructor, void **node);
void func_0203d704(int *o, int *a, int *b);
void func_0203d388(int *p, int angle);
int RandomIntInternal(int *seed);
void Mg_SpawnFx(int c, short a1, int *a2, int *r3, int *sp0);
void NullDestructor_0203d47c(void);
void __cxa_vec_cleanup(void *array, unsigned int count, unsigned int size, void (*dtor)(void *));
void Mg_RenderSpriteOnMtx(void *a0, int a1, int a2, int a3, int a4, M *a5);
void Mg_RenderSpriteOn(int a, int b, int c, int d, int e);
int Mg_GetLevel(void);
void Mg_DrawBigNumber(int a, int b, int c, int d, int e, int f, int g);
void Banner_DrawPlain(Elem *e);
void Banner_StartSpin(Elem *e);
void Banner_StartWaveBurst(Elem *e);
}

extern s16 data_02082214[];
extern int data_0209e650;
extern Obj *gMgScene;  /* local extern: through the Obj view above, the spelling the decl-agreement baseline records */
extern S3 data_ov004_020bc27c;
extern int data_ov004_020b9f54;
static EntryTable data_ov004_020bc6e8 = { { { 0, 0 }, { 0x140, (int)&data_ov004_020b9f54 } } };
extern int data_ov006_021346bc;
extern int sMgTopScreenYOffset;
extern int sMgBannerButtonsArmed;
extern Pair pmf_Banner_UpdateWave;
extern Pair pmf_Banner_DrawWave;
extern Pair pmf_Banner_UpdatePressed;
extern Pair pmf_Banner_DrawPressed;
extern int pmf_Banner_DrawSpin[];
extern int pmf_Banner_UpdateSpin[];
extern int pmf_Banner_UpdatePressed_2[];
extern unsigned char sMgBannerGfxSlotUsed[];
extern int data_ov004_020bf3ec;
extern int data_ov004_020bf3f0;
extern int data_ov004_020bf3f4[2];
extern int data_ov004_020bf3fc[2];
extern void *data_ov004_020bf404[3];
extern void *data_ov004_020bf410[3];
extern PairW sBannerUpdateTable[];
extern PairW sBannerSetupTable[];
extern PairW sBannerDrawTable[];
extern int sMgBannerFiles[];
extern int sMgBannerSprites[];

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


// @symbol Mg_FreeBannerFiles
extern "C" void Mg_FreeBannerFiles(void) {
    int i;
    for (i = 0; i < 0x1d; i++) {
        Deallocate((void *)sMgBannerFiles[i]);
        sMgBannerFiles[i] = 0;
    }
}

// @symbol Mg_LoadBannerFiles
#pragma opt_strength_reduction off
extern "C" void Mg_LoadBannerFiles(void)
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
        ((volatile int *)sMgBannerFiles)[i] = t;
        t = (int)&entries.e[i];
        t = *(volatile int *)(t + 4);
        t = *(int *)t;
        sMgBannerSprites[i] = t;
    }
}
#pragma opt_strength_reduction on

// @symbol Banner_Free
extern "C" void Banner_Free(Elem *e)
{
    e->mFileIndex = 0x1d;
    s16 idx = e->mGfxSlot;
    sMgBannerGfxSlotUsed[idx] = 0;
}

// @symbol Banner_Draw
extern "C" void Banner_Draw(Elem *e) {
    if (e->mFileIndex == 0x1d) return;
    if (e->mRender != 0) {
        (e->*(e->mRender))();
        return;
    }
    Banner_DrawPlain(e);
}

// @symbol Banner_Update
extern "C" void Banner_Update(Elem *e) {
    if (e->mFileIndex == 0x1d) return;
    if (e->mBehavior == 0) return;
    (e->*(e->mBehavior))();
}

// @symbol Banner_Init
extern "C" void Banner_Init(Elem *e, int file, short x, short y, int screen, int priority, short style)
{
    int a, b;

    if (gMgScene->GetDisplayMode() == 2) {
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
        if (sMgBannerGfxSlotUsed[0] != 0)
            return;
        DecompressLZ16((void *)sMgBannerFiles[file], (void *)(a + 0x7000));
        if (b != 0)
            DecompressLZ16((void *)sMgBannerFiles[file], (void *)(b + 0x7000));
        e->mGfxSlot = 0;
        sMgBannerGfxSlotUsed[0] = 1;
        break;
    default:
        if (sMgBannerGfxSlotUsed[1] == 0) {
            DecompressLZ16((void *)sMgBannerFiles[file], (void *)(a + 0x6000));
            if (b != 0)
                DecompressLZ16((void *)sMgBannerFiles[file], (void *)(b + 0x6000));
            e->mGfxSlot = 1;
            sMgBannerGfxSlotUsed[1] = 1;
            break;
        }
        if (sMgBannerGfxSlotUsed[2] == 0) {
            DecompressLZ16((void *)sMgBannerFiles[file], (void *)(a + 0x6800));
            if (b != 0)
                DecompressLZ16((void *)sMgBannerFiles[file], (void *)(b + 0x6800));
            e->mGfxSlot = 2;
            sMgBannerGfxSlotUsed[2] = 1;
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
        e->mOamCount = (short)Mg_CopySpriteList(
            sMgBannerSprites[e->mFileIndex],
            e->mOam,
            tmp.v[e->mGfxSlot],
            e->mFileIndex);
    }

    {
        int st = e->mStyle;
        if (((ElemFn *)sBannerSetupTable)[st])
            (e->*((ElemFn *)sBannerSetupTable)[st])();
    }

    {
        short st;
        PairW *r;
        st = e->mStyle;
        r = &sBannerUpdateTable[st];
        a = sBannerUpdateTable[st].w0;
        b = r->w1;
        ((int *)&e->mBehavior)[0] = b ? a : a;
        ((int *)&e->mBehavior)[1] = b;
        st = e->mStyle;
        r = &sBannerDrawTable[st];
        a = sBannerDrawTable[st].w0;
        b = r->w1;
        ((int *)&e->mRender)[0] = b ? a : a;
        ((int *)&e->mRender)[1] = b;
    }
}

// @symbol Banner_DrawLevelNumber
extern "C" void Banner_DrawLevelNumber(Elem *e)
{
  int x;
  int originX = e->mX;
  int n;
  s16 drawY;
  /* the first OAM record's 9-bit signed x, made positive */
  int oamX = (int) (((unsigned int) (*(int *)e->mOam << 7)) >> 0x17);
  int w;
  unsigned long left;
  n = Mg_GetLevel() + 1;
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
  Mg_RenderSpriteOn((int) e->mOam, left, e->mY, e->mScreen, e->mPriority);
  drawY = e->mY;
  Mg_DrawBigNumber(x, drawY, n, e->mScreen, e->mPriority, 2, 0);
}

// @symbol Banner_SetupNone
extern "C" void Banner_SetupNone(void)
{
}

// @symbol Banner_DrawSpin
extern "C" void Banner_DrawSpin(Elem *e) {
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
        Mg_RenderSpriteOnMtx(el, e->mX, e->mY,
                            e->mScreen, e->mPriority, &m);
        angle = (u16)(angle + 0x4000);
        flag = *(u16*)(el + 6);
        el += 8;
    } while (flag != 0xffff);
}

// @symbol Banner_UpdateSpin
extern "C" void Banner_UpdateSpin(Elem *e) {
    if (ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000)) {
        e->mTimer = 0;
    }
}

// @symbol Banner_StartSpin
extern "C" void Banner_StartSpin(Elem *e){
  e->mTimer=0;
  e->mTimerEnd=0x27000;
  *(W2*)&e->mBehavior=*(W2*)pmf_Banner_UpdateSpin;
  *(W2*)&e->mRender=*(W2*)pmf_Banner_DrawSpin;
}

// @symbol Banner_UpdateDrop
extern "C" void Banner_UpdateDrop(Elem *e){
    if (e->mTimer < 0x30){
        *(unsigned *)&e->mTimer += 1;
        return;
    }
    if (ApproachLinear2(e->mY, e->mTargetY, 0x10)){
        Banner_StartSpin(e);
    }
}

// @symbol Banner_SetupDrop
extern "C" void Banner_SetupDrop(Elem *e) {
    e->mTimer = 0;
    e->mY = -(sMgTopScreenYOffset + 0x60);
}

// @symbol Banner_DrawWave
extern "C" void Banner_DrawWave(Elem *e) {
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
            Mg_RenderSpriteOnMtx(el, e->mX, e->mY,
                                e->mScreen, e->mPriority, &m);
        }
        t += 0x1000;
        flag = *(u16*)(el + 6);
        el += 8;
    } while (flag != 0xffff);
}

// @symbol Banner_UpdateWave
extern "C" void Banner_UpdateWave(Elem *e) {
    if (ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000)) {
        e->mTimer = 0;
    }
}

// @symbol Banner_StartWaveBurst
extern "C" void Banner_StartWaveBurst(Elem* e) {
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

    *(W2*)&e->mBehavior = *(W2*)&pmf_Banner_UpdateWave;
    *(W2*)&e->mRender   = *(W2*)&pmf_Banner_DrawWave;

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

        Mg_SpawnFx(0, spawnAngle, outA, outB, data_ov004_020bf3f4);
    }
}

// @symbol Banner_DrawSqueeze
#pragma opt_propagation off
extern "C" void Banner_DrawSqueeze(Elem* e) {
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
                Mg_RenderSpriteOnMtx(el,
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

// @symbol Banner_UpdateFall
extern "C" void Banner_UpdateFall(Elem *e)
{
    ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000);
    if (ApproachLinear2(e->mY, e->mTargetY, 0x10) == 0)
        return;
    Banner_StartWaveBurst(e);
}

// @symbol Banner_SetupFall
extern "C" void Banner_SetupFall(Elem *e)
{
    e->mTimer = 0;
    e->mTimerEnd = 81920;
    e->mY = 96;
}

// @symbol Banner_DrawPopSpinA
extern "C" void Banner_DrawPopSpinA(Elem *e) {
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
                    Mg_RenderSpriteOnMtx(el, e->mX, e->mY,
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
                Mg_RenderSpriteOnMtx(el, e->mX, e->mY,
                                    e->mScreen, e->mPriority, &m2);
                angle = (u16)(angle + 0x4000);
                flag = *(u16*)(el + 6);
                el += 8;
            } while (flag != 0xffff);
        }
    }
}

// @symbol Banner_DrawPopSpinB
extern "C" void Banner_DrawPopSpinB(Elem *e) {
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
                    Mg_RenderSpriteOnMtx(el, e->mX, e->mY,
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
                Mg_RenderSpriteOnMtx(el, e->mX, e->mY,
                                    e->mScreen, e->mPriority, &m2);
                angle = (u16)(angle + 0x4000);
                flag = *(u16*)(el + 6);
                el += 8;
            } while (flag != 0xffff);
        }
    }
}

// @symbol Banner_UpdatePopSpin
extern "C" void Banner_UpdatePopSpin(Elem *e) {
    if (ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000)) {
        e->mTimer = 98304;
    }
}

// @symbol Banner_SetupPopSpin
extern "C" void Banner_SetupPopSpin(Elem *e)
{
    e->mTimer = 0;
    e->mTimerEnd = 159744;
}

// @symbol Banner_IsPressFinished
extern "C" int Banner_IsPressFinished(Elem* e){
  int* d = pmf_Banner_UpdatePressed_2;
  int* o = (int*)&e->mBehavior;
  if(o[0] == d[0] && (o[1] == d[1] || o[0] == 0) && e->mTimer == e->mTimerEnd)
    return 1;
  return 0;
}

// @symbol Banner_DrawPressed
extern "C" void Banner_DrawPressed(Elem *e)
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
        Mg_RenderSpriteOnMtx(el,
            e->mX, e->mY,
            e->mScreen, e->mPriority, &m);
        flag = *(u16*)(el + 6);
        el += 8;
    } while (flag != lim);

    x = e->mFileIndex;
    if (x != 0 && x != 1 && x != 2)
        return;
    Mg_RenderSpriteOn(data_ov006_021346bc,
        e->mX, e->mY,
        e->mScreen, e->mPriority);
}

// @symbol Banner_UpdatePressed
extern "C" void Banner_UpdatePressed(Elem *e) {
    ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000);
}

// @symbol Banner_Press
extern "C" void Banner_Press(Elem* e){
  sMgBannerButtonsArmed = 0;
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
  *(W2*)&e->mBehavior = *(W2*)&pmf_Banner_UpdatePressed;
  *(W2*)&e->mRender = *(W2*)&pmf_Banner_DrawPressed;
}

// @symbol Banner_DrawPlain
extern "C" void Banner_DrawPlain(Elem *e) {
    Mg_RenderSpriteOn((int)e->mOam, e->mX, e->mY, e->mScreen, e->mPriority);
    if (e->mFileIndex != 0 && e->mFileIndex != 1 && e->mFileIndex != 2)
        return;
    Mg_RenderSpriteOn(data_ov006_021346bc, e->mX, e->mY, e->mScreen, e->mPriority);
}

// @symbol Banner_SetupZoom
extern "C" void Banner_SetupZoom(Elem *e) {
    e->mTimer = 0;
    e->mTimerEnd = ((e->mOamCount + 1) << 2) + 0x1e << 0xc;
}

// @symbol Banner_DrawZoomA
#pragma opt_propagation off
extern "C" void Banner_DrawZoomA(Elem *e) {
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
                    Mg_RenderSpriteOnMtx(el,
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
                    Mg_RenderSpriteOnMtx(el, e->mX, e->mY,
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

// @symbol Banner_DrawZoomB
#pragma opt_propagation off
extern "C" void Banner_DrawZoomB(Elem *e) {
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
                    Mg_RenderSpriteOnMtx(el,
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
                    Mg_RenderSpriteOnMtx(el, e->mX, e->mY,
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

// @symbol Banner_UpdateZoom
extern "C" void Banner_UpdateZoom(Elem *e) {
    if (ApproachLinear(e->mTimer, e->mTimerEnd, 0x1000)) {
        e->mTimer = 122880;
    }
}

// @symbol Banner_SetupZoomBurst
extern "C" void Banner_SetupZoomBurst(Elem* e)
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

        Mg_SpawnFx(0, spawnAngle, outA, outB, data_ov004_020bf3fc);
    }
}

// @symbol Banner_UpdateSlideFromRight
extern "C" void Banner_UpdateSlideFromRight(Elem *e) {
    if (ApproachLinear2(e->mX, e->mTargetX, 0x10))
        e->mArrived = 1;
}

// @symbol Banner_SetupSlideFromRight
extern "C" void Banner_SetupSlideFromRight(Elem *e)
{
    e->mX = 320;
}

// @symbol Banner_UpdateSlideFromLeft
extern "C" void Banner_UpdateSlideFromLeft(Elem *e) {
    if (ApproachLinear2(e->mX, e->mTargetX, 0x10))
        e->mArrived = 1;
}

// @symbol Banner_SetupSlideFromLeft
extern "C" void Banner_SetupSlideFromLeft(Elem *e)
{
    e->mX = -0x40;
}

// @symbol Banner_UpdateSlideFromBottom
extern "C" void Banner_UpdateSlideFromBottom(Elem *e) {
    ApproachLinear2(e->mY, e->mTargetY, 4);
}

// @symbol Banner_SetupSlideFromBottom
extern "C" void Banner_SetupSlideFromBottom(Elem *e)
{
    e->mY = 200;
}

// @symbol Banner_UpdateSlideFromTop
extern "C" void Banner_UpdateSlideFromTop(Elem *e) {
    ApproachLinear2(e->mY, e->mTargetY, 4);
}

// @symbol Banner_SetupSlideFromTop
extern "C" void Banner_SetupSlideFromTop(Elem *e)
{
    e->mY = -8;
}

Elem::Elem()
{
    mFileIndex = 29;
}

/* __sinit_ov004_020b955c sources: the member-pointer records the three
   per-style tables copy (setup sBannerSetupTable, behavior
   sBannerUpdateTable, render sBannerDrawTable), and the words the
   29-entry file-id table copies. */
extern "C" {
extern PairW data_02086b58;  /* __ptmf_null's address: the null member pointer */
extern PairW pmf_Banner_SetupSlideFromTop, pmf_Banner_SetupSlideFromBottom, pmf_Banner_SetupSlideFromLeft, pmf_Banner_SetupSlideFromRight;
extern PairW pmf_Banner_SetupZoomBurst, pmf_Banner_SetupPopSpin, pmf_Banner_SetupZoom, pmf_Banner_SetupZoomBurst_2;
extern PairW pmf_Banner_SetupPopSpin_2, pmf_Banner_SetupFall, pmf_Banner_SetupDrop, pmf_Banner_SetupNone;
extern PairW pmf_Banner_UpdateSlideFromTop, pmf_Banner_UpdateSlideFromBottom, pmf_Banner_UpdateSlideFromLeft, pmf_Banner_UpdateSlideFromRight;
extern PairW pmf_Banner_UpdateZoom_2, pmf_Banner_UpdatePopSpin_2, pmf_Banner_UpdateZoom_3, pmf_Banner_UpdateZoom;
extern PairW pmf_Banner_UpdatePopSpin, pmf_Banner_UpdateFall, pmf_Banner_UpdateDrop;
extern PairW pmf_Banner_DrawZoomB, pmf_Banner_DrawPopSpinB, pmf_Banner_DrawZoomB_2, pmf_Banner_DrawZoomA;
extern PairW pmf_Banner_DrawPopSpinA, pmf_Banner_DrawSqueeze, pmf_Banner_DrawLevelNumber;
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
Elem gMgBanners[3];
}
}

extern "C" {
int sMgBannerSprites[29] = {
    data_ov004_020b9e98, data_ov004_020b9f54, data_ov004_020b9e8c, data_ov004_020b9f2c,
    data_ov004_020b9ea4, data_ov004_020b9fd8, data_ov004_020b9eb8, data_ov004_020b9f5c,
    data_ov004_020ba010, data_ov004_020b9e90, data_ov004_020b9ed4, data_ov004_020b9ffc,
    data_ov004_020b9f78, data_ov004_020b9fa0, data_ov004_020b9e7c, data_ov004_020b9ebc,
    data_ov004_020b9ee4, data_ov004_020b9ea8, data_ov004_020b9ef0, data_ov004_020b9f7c,
    data_ov004_020b9f0c, data_ov004_020b9e50, data_ov004_020b9e70, data_ov004_020b9f60,
    data_ov004_020b9efc, data_ov004_020b9f88, data_ov004_020b9e78, data_ov004_020b9fc8,
    data_ov004_020b9ee8
};
PairW sBannerSetupTable[13] = {
    data_02086b58, pmf_Banner_SetupSlideFromTop, pmf_Banner_SetupSlideFromBottom, pmf_Banner_SetupSlideFromLeft,
    pmf_Banner_SetupSlideFromRight, pmf_Banner_SetupZoomBurst, pmf_Banner_SetupPopSpin, pmf_Banner_SetupZoom,
    pmf_Banner_SetupZoomBurst_2, pmf_Banner_SetupPopSpin_2, pmf_Banner_SetupFall, pmf_Banner_SetupDrop,
    pmf_Banner_SetupNone
};
PairW sBannerUpdateTable[13] = {
    data_02086b58, pmf_Banner_UpdateSlideFromTop, pmf_Banner_UpdateSlideFromBottom, pmf_Banner_UpdateSlideFromLeft,
    pmf_Banner_UpdateSlideFromRight, pmf_Banner_UpdateZoom_2, pmf_Banner_UpdatePopSpin_2, pmf_Banner_UpdateZoom_3,
    pmf_Banner_UpdateZoom, pmf_Banner_UpdatePopSpin, pmf_Banner_UpdateFall, pmf_Banner_UpdateDrop,
    data_02086b58
};
PairW sBannerDrawTable[13] = {
    data_02086b58, data_02086b58, data_02086b58, data_02086b58, data_02086b58,
    pmf_Banner_DrawZoomB, pmf_Banner_DrawPopSpinB, pmf_Banner_DrawZoomB_2, pmf_Banner_DrawZoomA,
    pmf_Banner_DrawPopSpinA, pmf_Banner_DrawSqueeze, data_02086b58, pmf_Banner_DrawLevelNumber
};
int sMgBannerFiles[29];
}
