/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN5Stump8BehaviorEv.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/game/actors/d_a_obj_pile.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * The arm changed both the SpawnCoins declaration and its call inside the
 * method, so the host definition is the whole member: main's body text
 * (Stump -> daObjPile_c, mState -> mStepsLeft) with the arm's pointer-passing
 * declaration and call. Main's TU also defines this member, so the TU's
 * hostgen copy must park _ZN11daObjPile_c8BehaviorEv. */
// @symbol _ZN11daObjPile_c8BehaviorEv
#include "decl_common.h"
#include "daObjPile_c.h"
#include "Player.h"

/* Main's TU-local by-value coin position (src/game/actors/d_a_obj_pile.cpp). */
struct V3 {
    int x, y, z;
    V3() {}
    V3(const V3 &o) { x = o.x; y = o.y; z = o.z; }
    ~V3() {}
};

extern "C" {
extern unsigned char DecIfAbove0_Byte(unsigned char *p);
extern int _ZN10dBgActor_c21IsClsnInRangeOnScreenE5Fix12IiES1_(void *c, int a, int b);
/* mwccarm passes the by-value V3 as a pointer to a caller-built copy, which is
 * the const Vector3 * the definition takes. MSVC x86 copies the 12 bytes onto
 * the stack instead, so SpawnCoins read the stake's X coordinate as the
 * pointer and the circle payout faulted in the first coin's constructor. The
 * host arm passes the pointer the cartridge passes. */
extern void _ZN8dActor_c10SpawnCoinsERK7Vector3j5Fix12IiEs(void *c, const V3 *v, unsigned int n, int f, short s);
}

s32 daObjPile_c::Behavior()
{
    Player *player = ClosestPlayer();
    if (mStepsLeft == 3
        && GetBitInDeathTable() == 0
        && player->mPosY < mPosY + 0x64000) {
        int angle = HorzAngleToCPlayer();
        if (DistToCPlayer() > 0x190000) {
            mAngleDelta = 0;
        } else {
            s16 diff = (s16)(angle - mPrevAngle);
            int val = mAngleDelta;
            int *p;
            if ((val > 0 && diff < -100) || (val < 0 && diff > 100))
                mAngleDelta = 0;
            p = &mAngleDelta;
            *p = *p + diff;
            {
                int a = mAngleDelta;
                if (a < 0)
                    a = -a;
                if (a > 0x30000 && mBusy == 0
                    && GetBitInDeathTable() == 0) {
                    V3 v;
                    v.x = mPosX;
                    v.y = mPosY;
                    v.z = mPosZ;
                    v.y += 0xc8000;
                    _ZN8dActor_c10SpawnCoinsERK7Vector3j5Fix12IiEs(this, &v, 5, 0x5000, 0);
                    TrackInDeathTable();
                }
            }
        }
        mPrevAngle = angle;
    }
    DecIfAbove0_Byte((unsigned char *)&mAttackCooldown);
    _ZN10dBgActor_c21IsClsnInRangeOnScreenE5Fix12IiES1_(this, 0, 0);
    return 1;
}
