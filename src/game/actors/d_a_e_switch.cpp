//cpp
/* Enemy creation and event-switch actors. The cartridge RTTI names both
 * classes; their adjacent, interleaved methods support this combined TU.
 * The original source boundary remains an inference.
 *
 * leftovers:
 * - dCcAc_c::Init stays a mangled call: the real signature takes
 *   Fix12<int> by value and 2004/b56 homes class-typed by-value
 *   parameters on the stack (wall 6az), which changes InitResources'
 *   size (documented on the extern).
 * - func_ov102_0214ad14 and data_ov002_0210d9e0 keep linker names; the
 *   spawn target (actor 0xce) has no recovered header.
 */
#include "EnemySwitchTag.h"
#include "EnemySpawner.h"
#include "SharedFilePtr.h"
#include "Model.h"
#include "Event.h"

extern "C" {
    extern SharedFilePtr data_ov002_0210d9e0;
    void func_ov102_0214ad14(void *actor);
    /* The existing definition takes raw fixed-point words. Passing the
     * header's Fix12<int> values homes them on the stack under 2004/b56
     * and grows InitResources from 0xac to 0xc4 bytes (codegen wall 6az). */
    void _ZN7dCcAc_c4InitEP8dActor_c5Fix12IiES3_jj(
        dCcAc_c *self, dActor_c *actor, int radius, int height,
        u32 flags, u32 vulnFlags);
}

// @symbol daESwitch_c_classInit
extern "C" daESwitch_c *daESwitch_c_classInit()
{
    return new daESwitch_c;
}

// @symbol daECreate_c_classInit
extern "C" daECreate_c *daECreate_c_classInit()
{
    return new daECreate_c;
}

// @symbol _ZN11daESwitch_c13InitResourcesEv
int daESwitch_c::InitResources()
{
    _ZN7dCcAc_c4InitEP8dActor_c5Fix12IiES3_jj(&mdCcAc_c, this,
        ((mAngleX + 1) * 100) << 12, ((mAngleY + 1) * 200) << 12,
        2, 0x400000);
    mEventID = param1 & 0x1f;
    mIsReusable = (param1 >> 5) & 1;
    if (mAngleZ <= 0)
        mHoldDuration = 150;
    else
        mHoldDuration = mAngleZ;
    mHoldTimer = 0;
    Event::ClearBit(mEventID);
    return 1;
}

// @symbol _ZN11daECreate_c13InitResourcesEv
int daECreate_c::InitResources()
{
    mActorToSpawn = 0xce;
    mEventBit = mAngleZ & 0x1f;
    mPreviousEventBit = Event::GetBit(mEventBit);
    Model::LoadFile(data_ov002_0210d9e0);
    return 1;
}

// @symbol _ZN11daECreate_c16CleanupResourcesEv
int daECreate_c::CleanupResources()
{
    data_ov002_0210d9e0.Release();
    return 1;
}

// @symbol _ZN11daESwitch_c8BehaviorEv
int daESwitch_c::Behavior()
{
    if (mHoldTimer != 0) {
        --mHoldTimer;
        if (mHoldTimer == 0) {
            mdCcAc_c.flags &= ~1;
            Event::ClearBit(mEventID);
        }
    }
    if (mdCcAc_c.otherOwner != 0) {
        mdCcAc_c.flags |= 1;
        Event::SetBit(mEventID);
        if (mIsReusable != 0)
            mHoldTimer = mHoldDuration;
        else
            MarkForDestruction();
    }
    mdCcAc_c.Clear();
    mdCcAc_c.Update();
    return 1;
}

// @symbol _ZN11daECreate_c8BehaviorEv
int daECreate_c::Behavior()
{
    if (Event::GetBit(mEventBit) && mPreviousEventBit == 0) {
        /* dActor_c currently exposes these coordinate triples as scalars. */
        dActor_c *actor = dActor_c::Spawn(mActorToSpawn, 4,
            *reinterpret_cast<const Vector3 *>(&mPosX),
            reinterpret_cast<const Vector3_16 *>(&mPrevAngleX), mAreaId, -1);
        if (actor != 0)
            func_ov102_0214ad14(actor);
    }
    mPreviousEventBit = Event::GetBit(mEventBit);
    return 1;
}

// @symbol _ZN11daESwitch_c16CleanupResourcesEv
int daESwitch_c::CleanupResources()
{
    Event::ClearBit(mEventID);
    return 1;
}
