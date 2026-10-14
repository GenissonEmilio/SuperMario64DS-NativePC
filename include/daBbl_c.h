#ifndef DABBL_C_H
#define DABBL_C_H

#include "types.h"

/* The Lava Bubble (Podoboo) of the lava levels, registry profile BUBBLE.
 * RTTI names the class: _ZTS7daBbl_c at ov064 0x0211beb0, _ZTI7daBbl_c at
 * 0x0211bebc with dEnemyBase_c the single base. The BUBBLE profile row at
 * 0x0211bec8 takes daBbl_c_classInit as its factory, which allocates 0x31c.
 *
 * Members:
 *     0x110 dCcAc_c      0x34   -> 0x144
 *     0x144 dBgCh_Actr   0x1bc  -> 0x300
 */

#include "dEnemyBase_c.h"
#include "dCcAc_c.h"
#include "dBgCh_Actr.h"

/* The actor heap's own allocator. daBbl_c_classInit's first call is
 * fBase_c::operator new with the literal 0x31c, not the global operator new;
 * declaring it here, and the leaf `operator new` below, is what lets the
 * factory be written as a plain `new daBbl_c()` and still emit that call.
 * Spelt exactly as include/decl_common.h spells it. */
extern "C" void *_ZN7fBase_cnwEj(unsigned size);

struct daBbl_c : dEnemyBase_c {
    /* The bubble runs a two-entry state table. Each entry is an enter hook
     * the transition calls once and an execute hook Behavior calls every
     * frame, both pointer-to-member -- the cartridge stores them as the
     * eight-byte {ptr, adjustment} pairs at ov064 0x0211be90..0x0211beb0.
     * The tables themselves live in this overlay's .bss at 0x0211c7b8 and
     * 0x0211c7c8, filled by the module's static initializer. */
    typedef int (daBbl_c::*StateFn)();

    struct State {
        StateFn mEnter;
        StateFn mExecute;
    };

    dCcAc_c    mdCcAc_c;       /* 0x110 */
    dBgCh_Actr mWithMeshClsn;  /* 0x144 */
    State     *mState;         /* 0x300 -- the table this bubble is running */
    s32        mSpawnPosX;     /* 0x304 -- where InitResources found the actor */
    s32        mSpawnPosY;     /* 0x308 */
    s32        mSpawnPosZ;     /* 0x30c */
    u8         mJumps;         /* 0x310 -- clear: the fixed flame, which only
                                *          hurts; set: the jumping bubble, which
                                *          falls under gravity and is hittable */
    u8         pad_311[0x3];
    s32        mFireParticle;  /* 0x314 -- both handles are rolled forward every
                                *          frame the bubble is airborne */
    s32        mSmokeParticle; /* 0x318 */

    /* Inline empty body on purpose. From an inline destructor mwccarm emits D1
     * and then D0 -- the cartridge's own order at 0x021185c0 and 0x021185f8 --
     * and no leaf D2. Written out of line in the translation unit instead, the
     * same two bodies come out D0-before-D1 and the isolation step rejects the
     * object. Every instruction in both is compiler-generated: this class's
     * vptr store, then dBgCh_Actr at 0x144 and dCcAc_c at 0x110 in reverse
     * construction order, then the dEnemyBase_c base; D0 additionally returns
     * the object to the actor heap through dEnemyBase_c's inline
     * operator delete. */
    virtual ~daBbl_c() {}            /* slots 16, 17 */

    virtual int  InitResources();    /* slot  0 */
    virtual int  CleanupResources(); /* slot  3 */
    virtual int  Behavior();         /* slot  6 */
    virtual int  Render();           /* slot  9 */
    virtual void OnPendingDestroy(); /* slot 12 */
    virtual s32  OnYoshiTryEat();    /* slot 18 */

    /* The five state hooks. The ROM records no English names; the
     * pointer-to-member records at 0x0211be90..0x0211beb0 prove they are
     * members, so they keep their address labels as method names. */
    int func_ov064_02118644();
    int func_ov064_0211873c();
    int func_ov064_02118760();
    int func_ov064_021187d0();
    int func_ov064_021187ec(State *state);

    static void *operator new(unsigned long size) {
        return _ZN7fBase_cnwEj((unsigned)size);
    }
};

#ifndef SM64DS_PLATFORM_PC
/* ROM layout under mwccarm; host ABI divergence is tracked separately. */
typedef char daBbl_c_size_must_be_0x31c[sizeof(daBbl_c) == 0x31c ? 1 : -1];
#endif

#endif /* DABBL_C_H */
