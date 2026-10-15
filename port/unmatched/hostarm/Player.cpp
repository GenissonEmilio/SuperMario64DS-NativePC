/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/game/player/Player.cpp (port line). Its _MSC_VER arms lived inside six
 * function bodies; the 2026-10 main merge (749f5799a) took main's src/, which
 * rewrote those functions as Player members and dropped the arms. Main's
 * spellings still mis-call under MSVC (one-argument EndMusic, cdecl raw vtable
 * calls, receiver-less GetHealth), so this file defines the six members again:
 * MAIN'S BODIES, statement for statement, with only the old arms' changes
 * applied. The same six @symbol bodies must be parked in the TU's hostgen copy.
 * mwccarm never sees this file, so nothing here moves a ROM byte.
 * The preamble below is main's TU preamble (includes and shadow types), kept
 * whole so the bodies compile exactly as they do there. */
#include "types.h"
#include "common.h"
#include "Player.h"
#include "PlayerInput.h"
#include "SharedFilePtr.h"
#include "PathPtr.h"
#include "Particle__System.h"
#include "SurfaceInfo.h"
#include "dBgCh_Gnd.h"
#include "dBgCh_Lin.h"
#include "dBgCh_Actr.h"
#include "decl_Animation.h"
#include "Sound.h"

bool ApproachLinear(short &value, short target, short step);

/* ---------------------------------------------------------------------------
 * Shadow types that cannot live inside a function body.
 * ------------------------------------------------------------------------- */
struct Ret;
struct V3Quad { Vector3 a; Vector3 d; Vector3 e; Vector3 b; };
struct Fix12v { int v; Fix12v(int a) : v(a) {} };
namespace Sound { void PlayBank0(unsigned int, const Vector3 &); }
struct V3a {
 int x, y, z;
 void set(int x_, int y_, int z_) { x = x_; y = y_; z = z_; }
};
namespace Particle { extern "C" void _ZN8Particle6System3NewEjj5Fix12IiES2_S2_PK11Vector3_16fPNS_8CallbackE(unsigned int, unsigned int, int, int, int, const void *, void *); }
struct Obj1eb4 {
 char pad0[0x5c];
 Vector3 p5c; /* 0x5c */
 char pad1[0x74 - 0x68];
 Vector3 p74; /* 0x74 */
 char pad2[0x8c - 0x80];
 short p8c; /* 0x8c */
 short p8e; /* 0x8e */
 short p90; /* 0x90 */
 char pad3[0x94 - 0x92];
 short p94; /* 0x94 */
 char pad4[0x98 - 0x96];
 int p98; /* 0x98 */
 char pad5[0x674 - 0x9c];
 int p674; /* 0x674 */
 char pad6[0x6de - 0x678];
 unsigned char b6de; /* 0x6de */
 char pad7[0x6e3 - 0x6df];
 unsigned char b6e3; /* 0x6e3 */
};
struct Obj2fec {
 char pad0[0x730];
 unsigned char idx;
 char pad1[3];
 int f734;
 int f738;
};
typedef struct {
 char _pad[0x734];
 int unk734;
 int unk738;
} Arg0;
typedef struct {
 int unk0, unk4, unk8, unkC;
} Arg1;
struct State3i { int a, b, c; };
struct V3 {
 int x, y, z;
 V3(int x, int y, int z) : x(x), y(y), z(z) {}
 V3() {}
};
struct PlayerRaw {
 char pad[0x6e3];
 unsigned char state;
};
struct Ret { int pad; int y; };
struct Arg {
 virtual void *m0();
 virtual void *m1();
 virtual struct Ret *m2();
};
struct CameraRaw {
 char pad60[0x60];
 int f60; // 0x60
 char pad_to6d8[0x6d8 - 0x64];
 unsigned char f6d8; // 0x6d8
 char pad_to6e5[0x6e5 - 0x6d9];
 unsigned char f6e5; // 0x6e5
};
/* The rest of this block is the same hoist for the same reason, and the tags
 * are unique in the same way.  A body may not declare `extern` with a
 * function-local class type: a local class has no linkage, so the declaration
 * names an entity the type cannot describe.  mwccarm accepts it; the C++ rule
 * ([basic.link]) does not, and neither does the host front end.  Each shape
 * below is the body's own, moved out unchanged, and the body keeps its own
 * declaration of the object. */
struct Entry18 { s16 val; /* 0x0 */ char _pad[0x16]; };
struct Anim2fec { short a; short x; short y; short z; };
struct ObjTeleport { s16 x, y, z; u16 param; };
struct ObjLE { char pad[0x118]; void *f118; };
struct PMF { int adj; int ptr; };
struct State2i { int a; int b; };
struct S18a { short f0; char pad[0x16]; };
struct S18b { u16 f0; char pad[0x16]; };
struct S18b2 { short a; short b; int c[5]; };
/* Two shadow scaffolds, hoisted for the same reason as the block above and one
 * more: C++ requires every virtual of a LOCAL class to be defined in the same
 * body, because a local class's vtable can have nowhere else to come from.
 * Neither scaffold is ever constructed -- each exists only to give a call
 * through a pointer the ROM's vtable-slot shape (see the note on
 * Player::CanEnterDoor) -- so at file scope the declarations are enough and
 * no vtable is emitted on either compiler. */
struct StarDoor {
 virtual int v00(); virtual int v01(); virtual int v02(); virtual int v03();
 virtual int v04(); virtual int v05(); virtual int v06(); virtual int v07();
 virtual int v08(); virtual int v09(); virtual int v10(); virtual int v11();
 virtual int v12(); virtual int v13(); virtual int v14(); virtual int v15();
 virtual int v16(); virtual int v17();
 virtual int GetType();
};
struct ObjHS {
 virtual void v0();
 virtual void v1();
 virtual ObjHS* v2();
};
/* The Kuppa-script command table at the head of the file holds pointers to
 * members of one signature. */
typedef int (Player::*ScriptCommand)(unsigned char *, int, int);

/* -------------------------------------------------------------------------- */
/* ROM ordinal 0 -- _ZN6Player19func_ov002_020bccfcEPhii, 0x020bccfc, size 0x1c */

// @symbol _ZN6Player19func_ov002_020bd8c0Ej
extern "C" {
void Player::func_ov002_020bd8c0(unsigned int r1){
    char* c = (char*)this;
    /* SOUND::ENDMUSIC TAKES TWO ARGUMENTS, and on the cartridge the second one
       rides in r1. Sound::EndMusic(unsigned, unsigned) is the definition, and
       none of these four music-layer bodies writes r1 before it calls EndMusic
       (0x020bd8d8 and 0x020bd900 here, 0x020bd960, 0x020bd99c and 0x020bd9c4,
       0x020bda24 below), so each passes its own second argument. The
       one-argument spelling main's TU keeps is what mwccarm compiles to those
       bytes. Under MSVC's cdecl it pushes one word, and EndMusic spills into
       its second-argument slot, which is then the caller's saved esi: the
       caller comes back with esi clobbered. These bodies pass what the
       cartridge passes. */
    extern void _ZN5Sound8EndMusicEjj(unsigned int, unsigned int);
    extern int _ZN5Sound8SetMusicEjj(unsigned char, unsigned int);
  unsigned int r2=*(unsigned int*)(c+0x678);
  if(r2==0){
    _ZN5Sound8EndMusicEjj(*(unsigned char*)(c+0x6d8), r1);
    *(unsigned int*)(c+0x680)=0;
  } else if(*(unsigned int*)(c+0x680)==r1 && r2!=r1){
    _ZN5Sound8EndMusicEjj(*(unsigned char*)(c+0x6d8), r1);
    _ZN5Sound8SetMusicEjj(*(unsigned char*)(c+0x6d8), *(unsigned int*)(c+0x678));
    *(unsigned int*)(c+0x680)=*(unsigned int*)(c+0x678);
  }
  *(unsigned int*)(c+0x67c)=0;
}
}

// @symbol _ZN6Player19func_ov002_020bd928Ej
extern "C" {
void Player::func_ov002_020bd928(unsigned int r4){
    char* c = (char*)this;
    extern void _ZN5Sound8EndMusicEjj(unsigned int, unsigned int);
    extern int _ZN5Sound8SetMusicEjj(unsigned char, unsigned int);
  unsigned int r2=*(unsigned int*)(c+0x678);
  if(r2==r4){ *(unsigned int*)(c+0x67c)=r4; return; }
  if((r2|*(unsigned int*)(c+0x67c))!=0){
    _ZN5Sound8EndMusicEjj(*(unsigned char*)(c+0x6d8), r4);
  }
  *(unsigned int*)(c+0x67c)=r4;
  *(unsigned int*)(c+0x680)=r4;
  _ZN5Sound8SetMusicEjj(*(unsigned char*)(c+0x6d8),r4);
}
}

// @symbol _ZN6Player19func_ov002_020bd984Ej
extern "C" {
void Player::func_ov002_020bd984(unsigned int r1){
    char* c = (char*)this;
    extern void _ZN5Sound8EndMusicEjj(unsigned int, unsigned int);
    extern int _ZN5Sound8SetMusicEjj(unsigned char, unsigned int);
  unsigned int r2=*(unsigned int*)(c+0x67c);
  if(r2==0){
    _ZN5Sound8EndMusicEjj(*(unsigned char*)(c+0x6d8), r1);
    *(unsigned int*)(c+0x680)=0;
  } else if(*(unsigned int*)(c+0x680)==r1 && r2!=r1){
    _ZN5Sound8EndMusicEjj(*(unsigned char*)(c+0x6d8), r1);
    _ZN5Sound8SetMusicEjj(*(unsigned char*)(c+0x6d8), *(unsigned int*)(c+0x67c));
    *(unsigned int*)(c+0x680)=*(unsigned int*)(c+0x67c);
  }
  *(unsigned int*)(c+0x678)=0;
}
}

// @symbol _ZN6Player19func_ov002_020bd9ecEj
extern "C" {
void Player::func_ov002_020bd9ec(unsigned int r4){
    char* c = (char*)this;
    extern void _ZN5Sound8EndMusicEjj(unsigned int, unsigned int);
    extern int _ZN5Sound8SetMusicEjj(unsigned char, unsigned int);
  unsigned int r2=*(unsigned int*)(c+0x67c);
  if(r2==r4){ *(unsigned int*)(c+0x678)=r4; return; }
  if((*(unsigned int*)(c+0x678)|r2)!=0){
    _ZN5Sound8EndMusicEjj(*(unsigned char*)(c+0x6d8), r4);
  }
  *(unsigned int*)(c+0x678)=r4;
  *(unsigned int*)(c+0x680)=r4;
  _ZN5Sound8SetMusicEjj(*(unsigned char*)(c+0x6d8),r4);
}
}

// @symbol _ZN6Player19func_ov002_020bdb50Ei
extern "C" {
void Player::func_ov002_020bdb50(int arg) {
    char* c = (char*)this;
    extern void Player_ReleaseHeldActor(void*);
    extern void func_ov002_020d5cec(void);
    /* THE HELD OBJECT'S SLOTS 18 AND 19 ARE VIRTUAL CALLS: Actor::OnYoshiTryEat()
       and Actor::OnTurnIntoEgg(Player &), the same two slots St_Swallow_Main
       spells as virtuals. On ARM main's spelling, a raw function-pointer call
       through the vtable, is the same instructions as a virtual call (r0 = the
       held object, r1 = this Player), and it is what mwccarm compiles to the
       ROM's bytes. Under MSVC it is not: a raw function-pointer call is cdecl
       (every argument pushed, the caller cleans), while every seated slot-18/19
       face is the thiscall-shaped __fastcall (the receiver in ecx, the argument
       pushed, the callee pops it). At slot 19 the face read the held object as
       its Player argument and popped four bytes the caller popped again, so this
       function returned through its own stacked Player pointer. This body makes
       the calls the cartridge makes. Pure virtuals: MSVC refuses an undefined
       virtual in a local class (C3640); the call shape is the same. */
    struct HeldActor {
        virtual int v00() = 0; virtual int v01() = 0; virtual int v02() = 0;
        virtual int v03() = 0; virtual int v04() = 0; virtual int v05() = 0;
        virtual int v06() = 0; virtual int v07() = 0; virtual int v08() = 0;
        virtual int v09() = 0; virtual int v10() = 0; virtual int v11() = 0;
        virtual int v12() = 0; virtual int v13() = 0; virtual int v14() = 0;
        virtual int v15() = 0; virtual int v16() = 0; virtual int v17() = 0;
        virtual int OnYoshiTryEat() = 0;               /* slot 18, +0x48 */
        virtual void OnTurnIntoEgg(char* player) = 0;  /* slot 19, +0x4c */
    };
    ((Player *)(c))->func_ov002_020bdc18();
    Player_ReleaseHeldActor(c);
    if (arg != 0) return;
    if (*(int*)(c+0x360) == 0) return;
    ((Player*)c)->func_ov002_020d71a0();
    {
        char* obj = *(char**)(c+0x360);
        int r = ((HeldActor*)obj)->OnYoshiTryEat();
        if (r == 0) return;
    }
    {
        char* obj = *(char**)(c+0x360);
        int b1 = (*(unsigned short*)(obj+0xc) == 0xbf);
        if (b1) {
            func_ov002_020d5cec();
            (*(unsigned short *)(((int)c + 0x6ce))) &= ~2;
        } else {
            ((HeldActor*)obj)->OnTurnIntoEgg(c);
        }
    }
    *(int*)(c+0x360) = 0;
}
}

// @symbol _ZN6Player12St_Swim_MainEv
/* recovered: named members + shared header, real C++ method
 *
 * Swimming, per frame. Metal cap diverts to the metal-underwater state and
 * surfacing to the walking one. mStateStep selects the stroke phase; each
 * phase drives the stroke animation, the body model's animation block at
 * +0x50, and the forward impulse, with a kick sound on the frame the
 * animation crosses its trigger.
 */
extern "C" {
int Player::St_Swim_Main()
{
    extern void _ZN6Player11ChangeStateERNS_5StateE(void*, void*);
    /* THE RECEIVER IS PASSED, AS THE ROM PASSES IT. St_Swim_Main reaches
       GetHealth (0x020bf548) at 0x020cd97c with this still in r0 from the
       entry, so main's no-argument call byte-matches; the port's face
       (hal/player_bridges.cpp) reads its receiver off the stack, and with
       nothing pushed it read the dispatcher's state-function word
       (0x020cd94c) as the Player. Declared and called with this, the host
       passes what r0 held. */
    extern int _ZN6Player9GetHealthEv(void*);
    extern int func_ov002_020cede0(void*);
    extern int func_ov002_020cec2c(void*);
    extern void func_ov002_020cd71c(void*);
    extern void func_ov002_020cd550(void*);
    extern unsigned int func_02022d44(unsigned int, unsigned int, int, int, int, void*);
    extern int _ZNK10dBgCh_Actr10IsOnGroundEv(void*);
    extern void* _ZNK10dBgCh_Actr14GetFloorResultEv(void*);
    extern void* func_02037dc4(void*);
    extern int _ZN4cstd5atan2E5Fix12IiES1_(int, int);
    extern void Vec3_RotateYAndTranslate(void*, void*, short, void*);
    extern void _Z14ApproachLinearRiii(void*, int, int);
    extern int _ZN6Player6IsAnimEj(void*, unsigned int);
    extern int _ZN6Player12FinishedAnimEv(void*);
    extern void _ZN6Player7SetAnimEji5Fix12IiEj(void*, unsigned int, int, int, unsigned int);
    extern int _ZNK6Player14GetBodyModelIDEjb(void*, unsigned char, int);
    extern int func_ov002_020ce324(void*);
    extern void func_ov002_020cd364(void*);
    extern void func_ov002_020cd3e0(void*);
    extern int func_ov002_020cedb0(void*, int);
    extern void func_ov002_020cd39c(void*);
    extern int _ZNK15dExtFrameCtrl_c12WillHitFrameEi(void*, int);
    extern void _ZN5Sound9PlayBank0EjRK7Vector3(unsigned int, void*);
    extern void func_ov002_020cd2c4(void*);
    extern void func_ov002_020cd308(void*);
    extern void func_ov002_020dbf4c(void*);
    extern void _ZN5dCc_c5ClearEv(void*);
    extern void _ZN5dCc_c6UpdateEv(void*);
    extern void Player_ReleaseHeldActor(void*);
    extern void func_ov002_020cd218(void*, int, void*, void*);
    extern int func_ov002_020ceaf4(void*);
    extern int func_ov002_020ce5f8(void*);
    extern void Player_AdvanceAnims(void*);
    extern char data_ov002_021106ac[];
    extern char data_ov002_021101b4[];
    extern unsigned short data_0209f49c;
    extern unsigned short data_0209f49e;
    extern char data_0209ee90[];
    int t4;
    volatile int pos[3];
    int* fr;
    int ang;
    int out[3];
    int vec[3];
    short yaw;
    int var_r2;
    unsigned short a4;
    int var_r6;
    int var_r1;
    int has;
    int* p;
    int* sub;
    unsigned int t5;


    if (mIsMetal != 0) {
        _ZN6Player11ChangeStateERNS_5StateE(this, data_ov002_021106ac);
        return 1;
    }
    if (_ZN6Player9GetHealthEv(this) == 0) {
        func_ov002_020c5dec(8);
        return 1;
    }
    if (func_ov002_020cede0(this) == 0) {
        mIsUnderwater = 0;
        _ZN6Player11ChangeStateERNS_5StateE(this, data_ov002_021101b4);
        return 1;
    }
    mPeakY = mPosY;
    if (func_ov002_020cec2c(this) != 0) return 1;

    t4 = (*(unsigned short*)((char*)&data_0209f49c + gActivePlayerSlot * 0x18)) & 0x802;
    unk_69e = mAngleZ;
    func_ov002_020cd71c(this);
    func_ov002_020cd550(this);

    if (mJumpComboTimer != 0) {
        int eid = 0xcd;
        int y, z;
        pos[0] = mPosX;
        y = mPosY;
        pos[1] = y;
        z = mPosZ;
        pos[2] = z;
        y = y + 0x32000;
        pos[1] = y;
        *(unsigned int*)&mParticle3 = func_02022d44(*(volatile unsigned int*)(&mParticle3), eid, pos[0], pos[1], z, 0);
    }

    if (mPrevVertSpeed == 0 && _ZNK10dBgCh_Actr10IsOnGroundEv(&mMeshClsn) != 0) {
        fr = (int*)func_02037dc4((char*)_ZNK10dBgCh_Actr14GetFloorResultEv(&mMeshClsn) + 4);
        ang = fr[1];
        if (ang < 0x720 && ang > 0x400 && mPosY >= unk_64c - 0xc8000) {
            int at;
            mPosY = mPreClsnPosY - 0x8000;
            at = _ZN4cstd5atan2E5Fix12IiES1_(fr[0], fr[2]);
            vec[0] = 0;
            vec[1] = 0;
            at = at + 0x8000;
            yaw = (short)at;
            vec[2] = (*(int*)((char*)data_0209ee90 + 0x244)) << 0xc;
            Vec3_RotateYAndTranslate(out, &mPosX, yaw, vec);
            mPosX = out[0];
            mPosY = out[1];
            mPosZ = out[2];
        }
    }

    switch (mStateStep) {
    case 0:
        mPrevAngleX = -0x4000;
        mPrevAngleZ = 0;
        mAngleX = 0;
        mAngleZ = 0;
        mAngleYSpeed = 0;
        _Z14ApproachLinearRiii((int*)(&mPrevVertSpeed), 0, 0x5000);
        if (_ZN6Player6IsAnimEj(this, 0xa8) == 0 && _ZN6Player12FinishedAnimEv(this) != 0) {
            _ZN6Player7SetAnimEji5Fix12IiEj(this, 0xa8, 0, 0x1000, 0);
        }
        if (mPrevVertSpeed == 0) {
            mStateStep = 1;
            mPrevAngleX = 0;
            mStateTimer = 0x1e;
        }
        break;
    case 1:
        var_r2 = 0x800;
        if (mStateTimer != 0) var_r2 = 0x200;
        _Z14ApproachLinearRiii((int*)(&mPrevVertSpeed), 0, var_r2);
        if (_ZN6Player6IsAnimEj(this, 0xa8) == 0) {
            if (_ZN6Player12FinishedAnimEv(this) != 0) {
                _ZN6Player7SetAnimEji5Fix12IiEj(this, 0xa8, 0, 0x1000, 0);
            }
        } else if (mAngleX >= 0x1000) {
            sub = (int*)((char*)(*(int*)((char*)this + _ZNK6Player14GetBodyModelIDEjb(this, (unsigned char)*(int*)&param1, 0)*4 + 0xdc)) + 0x50);
            if (*(int*)((char*)sub + 0xc) != 0x3000) {
                sub = (int*)((char*)(*(int*)((char*)this + _ZNK6Player14GetBodyModelIDEjb(this, (unsigned char)*(int*)&param1, 0)*4 + 0xdc)) + 0x50);
                *(int*)((char*)sub + 0xc) = 0x3000;
            }
        } else if (mAngleYSpeed != 0) {
            sub = (int*)((char*)(*(int*)((char*)this + _ZNK6Player14GetBodyModelIDEjb(this, (unsigned char)*(int*)&param1, 0)*4 + 0xdc)) + 0x50);
            if (*(int*)((char*)sub + 0xc) != 0x2000) {
                sub = (int*)((char*)(*(int*)((char*)this + _ZNK6Player14GetBodyModelIDEjb(this, (unsigned char)*(int*)&param1, 0)*4 + 0xdc)) + 0x50);
                *(int*)((char*)sub + 0xc) = 0x2000;
            }
        } else {
            sub = (int*)((char*)(*(int*)((char*)this + _ZNK6Player14GetBodyModelIDEjb(this, (unsigned char)*(int*)&param1, 0)*4 + 0xdc)) + 0x50);
            if (*(int*)((char*)sub + 0xc) != 0x1000) {
                sub = (int*)((char*)(*(int*)((char*)this + _ZNK6Player14GetBodyModelIDEjb(this, (unsigned char)*(int*)&param1, 0)*4 + 0xdc)) + 0x50);
                *(int*)((char*)sub + 0xc) = 0x1000;
            }
        }
        if (func_ov002_020ce324(this) != 0) return 1;
        if ((*(unsigned short*)((char*)&data_0209f49e + gActivePlayerSlot * 0x18)) & 1) {
            func_ov002_020cd364(this);
        } else if (t4 != 0) {
            func_ov002_020cd3e0(this);
        }
        break;
    case 2:
        a4 = mStateTimer;
        if (a4 >= 0xa) var_r6 = 0x800;
        else if (a4 <= 4) var_r6 = 0x1800;
        else var_r6 = 0;
        _Z14ApproachLinearRiii((int*)(&mPrevVertSpeed), func_ov002_020cedb0(this, 0x10000), var_r6);
        if (mStateTimer < 0xa && t4 == 0) {
            mStateWork |= 1;
        }
        if (mStateTimer == 0 && _ZN6Player6IsAnimEj(this, 0xa9) != 0 && _ZN6Player12FinishedAnimEv(this) != 0) {
            if (!(mStateWork & 1)) {
                mStateStep = 3;
            } else {
                func_ov002_020cd39c(this);
            }
        }
        break;
    case 3:
        _Z14ApproachLinearRiii((int*)(&mPrevVertSpeed), func_ov002_020cedb0(this, 0xc000), 0x200);
        if (func_ov002_020ce324(this) != 0) return 1;
        if (t4 != 0) {
            _ZN6Player7SetAnimEji5Fix12IiEj(this, 0xa6, 0, 0x1000, 0);
            if (_ZNK15dExtFrameCtrl_c12WillHitFrameEi((void*)(*(int*)((char*)this + _ZNK6Player14GetBodyModelIDEjb(this, (unsigned char)*(int*)&param1, 0)*4 + 0xdc) + 0x50), 0) != 0) {
                _ZN5Sound9PlayBank0EjRK7Vector3(0x15, &mCamSpacePosX);
            }
        } else {
            func_ov002_020cd39c(this);
        }
        break;
    case 4:
        _Z14ApproachLinearRiii((int*)(&mPrevVertSpeed), 0, 0x400);
        if (func_ov002_020ce324(this) != 0) return 1;
        if (mStateTimer == 0 && t4 != 0) {
            func_ov002_020cd3e0(this);
        }
        if (_ZN6Player12FinishedAnimEv(this) != 0 && _ZN6Player6IsAnimEj(this, 0xaa) != 0) {
            func_ov002_020cd2c4(this);
        }
        break;
    case 5:
        if (mStateTimer == 0) {
            func_ov002_020cd308(this);
        }
        break;
    case 6:
        func_ov002_020dbf4c(this);
        var_r1 = func_ov002_020cedb0(this, 0x10000);
        a4 = mStateTimer;
        if (a4 >= 5) var_r2 = 0x1000;
        else if (a4 == 0) { var_r2 = 0x800; var_r1 = 0; }
        else var_r2 = 0;
        _Z14ApproachLinearRiii((int*)(&mPrevVertSpeed), var_r1, var_r2);
        if (_ZN6Player12FinishedAnimEv(this) != 0) {
            has = (mHeldObj != 0) ? 1 : 0;
            if (has != 0) {
                mStateStep = 8;
                mStateTimer = 0xf0;
                p = (int*)(mHeldObj + 0xb0);
                *p |= 0x4000;
                _ZN6Player7SetAnimEji5Fix12IiEj(this, 0xb0, 0, 0x1000, 0);
            } else {
                func_ov002_020cd2c4(this);
            }
        }
        _ZN5dCc_c5ClearEv(&mAttackClsn);
        sub = (int*)((char*)(*(int*)((char*)this + _ZNK6Player14GetBodyModelIDEjb(this, (unsigned char)*(int*)&param1, 0)*4 + 0xdc)) + 0x50);
        t5 = ((unsigned int)(*(int*)((char*)sub + 8)) << 4) >> 0x10;
        if ((int)t5 >= 5 && (int)t5 <= 7) {
            _ZN5dCc_c6UpdateEv(&mAttackClsn);
        }
        break;
    case 7:
        if (_ZN6Player12FinishedAnimEv(this) != 0) {
            mStateStep = 8;
            mStateTimer = 0xf0;
            _ZN6Player7SetAnimEji5Fix12IiEj(this, 0xb0, 0, 0x1000, 0);
            p = (int*)(mHeldObj + 0xb0);
            *p |= 0x4000;
            func_ov002_020bd928(0x33);
            mSwimMusicPushed = 1;
        }
        break;
    case 8:
        _Z14ApproachLinearRiii((int*)(&mPrevVertSpeed), func_ov002_020cedb0(this, 0x1e000), 0x400);
        if ((*(unsigned short*)((char*)&data_0209f49e + gActivePlayerSlot * 0x18)) & 1) {
            _ZN6Player7SetAnimEji5Fix12IiEj(this, 0xb1, 0x40000000, 0x1000, 0);
            mStateStep = 9;
            func_ov002_020bd8c0(0x33);
        }
        if (mStateTimer != 0) {
            has = (mHeldObj != 0) ? 1 : 0;
            if (has == 0) goto case8_block107;
        } else {
        case8_block107:
            Player_ReleaseHeldActor(this);
            func_ov002_020cd2c4(this);
            func_ov002_020bd8c0(0x33);
            mSwimMusicPushed = 0;
        }
        if (_ZNK15dExtFrameCtrl_c12WillHitFrameEi((void*)(*(int*)((char*)this + _ZNK6Player14GetBodyModelIDEjb(this, (unsigned char)*(int*)&param1, 0)*4 + 0xdc) + 0x50), 0) != 0) {
            _ZN5Sound9PlayBank0EjRK7Vector3(0x15, &mCamSpacePosX);
        }
        break;
    case 9:
        if (_ZNK15dExtFrameCtrl_c12WillHitFrameEi((void*)(*(int*)((char*)this + _ZNK6Player14GetBodyModelIDEjb(this, (unsigned char)*(int*)&param1, 0)*4 + 0xdc) + 0x50), 0xd) != 0) {
            func_ov002_020daa74();
        }
        if (_ZN6Player12FinishedAnimEv(this) != 0) {
            func_ov002_020cd2c4(this);
        }
        break;
    }

    func_ov002_020cd218(this, mPrevVertSpeed, &mHorzSpeed, &mVertSpeed);
    {
        int* pa8 = (int*)((unsigned int)(&mVertSpeed));
        *pa8 = *pa8 + func_ov002_020ceaf4(this);
    }
    if (func_ov002_020ce5f8(this) != 0) return 1;
    Player_AdvanceAnims(this);
    return 1;
}
}
