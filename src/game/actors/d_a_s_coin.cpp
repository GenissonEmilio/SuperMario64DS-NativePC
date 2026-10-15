//cpp
/* Secret Coin (SECRET_COIN 329) -- ov002/daSCoin_c. RTTI ov002:0x0210b000
 * names daSCoin_c. Collecting all five coins of a group spawns POWER_STAR
 * at the STARBASE marker with the same mStarID.
 *
 * Function order is the reverse of the ROM's: mwccarm 2004/b56 emits one
 * .text section per function in reverse source order.
 *
 * deslop leftovers:
 * - dCcAc_c::Init stays a scalar extern: it takes Fix12<int> by value and
 *   the member form size-DIFFs (mwccarm-codegen wall 6az).
 * - SpawnStar reads the marker's position through int* at +0x5c: named
 *   mPosX/Y/Z on the daStarBase_c* size-DIFFs. dActor_c has no Pos().
 * - LinkToBlock keeps the goto + sequential isMatch stores: the plain
 *   for/else-if loop size-DIFFs.
 * - data_ov002_0210d9a8 (the ov002 SharedFilePtr handle) and func_02012790
 *   (the no-position jingle player, id 0x25 here) have no recovered names.
 */

#include "daSCoin_c.h"
#include "SharedFilePtr.h"
#include "Model.h"
#include "daStar_c.h"
#include "daStarBase_c.h"
#include "daObjHatenaBlock_c.h"
#include "daObjPushblock_c.h"

extern "C" {
int Vec3_Dist(const Vector3 *a, const Vector3 *b);
unsigned char DecIfAbove0_Byte(unsigned char *p);
unsigned int func_02012790(unsigned int);
void _ZN7dCcAc_c4InitEP8dActor_c5Fix12IiES3_jj(void *self, void *actor,
                                              int radius, int height,
                                              unsigned int flags,
                                              unsigned int vulnFlags);
extern SharedFilePtr data_ov002_0210d9a8;
}

/* The typed 0x1c actor profile: fBase_c reads the halfwords at +4/+6 as
 * behavior/render priorities. dActor_c reads actor flags at +8 and passes
 * the words at +0xc/+0x10/+0x14/+0x18 to SetRanges. */
struct SCoinSpawnInfo {
    daSCoin_c *(*classInit)();
    s16 behaviorPriority;   /* 0x0149 */
    s16 renderPriority;     /* 0x014a */
    u32 actorFlags;
    s32 clipOffsetY;
    s32 clipRadius;
    s32 clipDistance;
    s32 farDistance;
};
typedef char SCoinSpawnInfo_size_must_be_0x1c[
    sizeof(SCoinSpawnInfo) == 0x1c ? 1 : -1];

// @symbol daSCoin_c_classInit
extern "C" daSCoin_c *daSCoin_c_classInit()
{
    return new daSCoin_c();
}

// @symbol g_profile_SECRET_COIN
extern "C" SCoinSpawnInfo g_profile_SECRET_COIN = {
    daSCoin_c_classInit, 0x0149, 0x014a, 0x00000000,
    0x00000000, 0x00320000, 0x01f40000, 0x00050000
};

// @symbol _ZN9daSCoin_c13InitResourcesEv
s32 daSCoin_c::InitResources()
{
    _ZN7dCcAc_c4InitEP8dActor_c5Fix12IiES3_jj(&mdCcAc_c, this, 0x64000,
                                              0x40000, 0x800002, 0);
    mStarID = param1 & 0xf;
    mGroupId = (param1 >> 8) & 0xf;
    mGroupRole = 0;
    mLeaderUniqueID = 0;
    mCollectedCount = 0;
    mDeathTimer = 0;
    Model::LoadFile(data_ov002_0210d9a8);
    return 1;
}

// @symbol _ZN9daSCoin_c8BehaviorEv
s32 daSCoin_c::Behavior()
{
    if (mDeathTimer) {
        if (DecIfAbove0_Byte(&mDeathTimer) == 0) {
            SpawnStar();
            MarkForDestruction();
        }
        return 1;
    }
    if (mGroupRole == 0) {
        u8 group = mGroupId;
        if (group == 0 || group == 0xf) {
            mGroupRole = 1;
            mLeaderUniqueID = uniqueID;
            daSCoin_c *other = 0;
            for (;;) {
                other = (daSCoin_c *)dActor_c::FindWithActorID(0x149, other);
                if (other == 0) break;
                if (other != this) {
                    other->mGroupRole = 2;
                    other->mLeaderUniqueID = uniqueID;
                }
            }
        }
    }
    if (mGroupRole == 1 && mCollectedCount == 5) {
        MarkForDestruction();
        return 1;
    }
    LinkToBlock();
    if (mdCcAc_c.otherOwner) {
        Collect();
    }
    mdCcAc_c.Clear();
    if (mClsnDisabled == 0) {
        mdCcAc_c.Update();
    }
    return 1;
}

// @symbol _ZN9daSCoin_c16CleanupResourcesEv
s32 daSCoin_c::CleanupResources()
{
    data_ov002_0210d9a8.Release();
    return 1;
}

// @symbol _ZN9daSCoin_c9SpawnStarEv
/* Finds the STARBASE whose mStarID equals this group's and spawns POWER_STAR
 * (0xb2) 0x12c000 above its position, then calls AddStarMarker on it.
 * Returns without spawning when there is no such marker. */
void daSCoin_c::SpawnStar()
{
    daStarBase_c *marker = 0;
    for (;;) {
        marker = (daStarBase_c *)dActor_c::FindWithActorID(0xb4, marker);
        if (marker == 0) return;
        if (mStarID == marker->mStarID) {
            int *base = (int *)((char *)marker + 0x5c);
            Vector3 pos;
            pos.x = base[0];
            pos.y = base[1];
            pos.z = base[2];
            pos.y += 0x12c000;
            daStar_c *star = (daStar_c *)dActor_c::Spawn(
                0xb2, mStarID | 0x40, pos, 0, mAreaId, -1);
            if (star != 0) {
                star->AddStarMarker();
            }
            return;
        }
    }
}

// @symbol _ZN9daSCoin_c11LinkToBlockEv
/* Runs once (mBlockScanDone guards it). Looks through the actor list for a
 * HATENA_BLOCK (0x14), ITEM_BLOCK (0x15) or PUSHBLOCK (0xc0) closer than
 * 0xc8000 to this coin; on a hit it disables the touch collider and stores
 * itself in the block's held/linked slot. The block collects the coin when
 * it pops. */
void daSCoin_c::LinkToBlock()
{
    dActor_c *other;
    u32 otherID;
    int isMatch;
    if (mBlockScanDone != 0) return;
    other = dActor_c::Next(0);
    if (other == 0) goto done;
    do {
        otherID = other->actorID;
        isMatch = (otherID == 0x14);
        if (isMatch == 0) {
            isMatch = (otherID == 0x15);
            if (isMatch == 0) goto chk2;
        }
        if (Vec3_Dist((Vector3 *)&mPosX, (Vector3 *)&other->mPosX) < 0xc8000) {
            mClsnDisabled = 1;
            ((daObjHatenaBlock_c *)other)->mHeldActor = this;
            goto done;
        }
        goto next;
      chk2:
        isMatch = (otherID == 0xc0);
        if (isMatch == 0) goto next;
        if (Vec3_Dist((Vector3 *)&mPosX, (Vector3 *)&other->mPosX) < 0xc8000) {
            mClsnDisabled = 1;
            ((daObjPushblock_c *)other)->mLinkedActor = this;
            goto done;
        }
      next:
        other = dActor_c::Next(other);
    } while (other != 0);
  done:
    mBlockScanDone = 1;
}

// @symbol _ZN9daSCoin_c7CollectEv
/* Collection handler. Finds the group leader by mLeaderUniqueID; unless this
 * coin is mGroupId 0xf, its mGroupId must equal the leader's mCollectedCount.
 * Plays func_02012790(0x25), bumps the leader's count, spawns the count as a
 * number, and marks the collider flags. The fifth collection starts this
 * coin's mDeathTimer at 0x1e frames; otherwise a mGroupRole 2 coin destroys
 * itself. */
void daSCoin_c::Collect()
{
    daSCoin_c *leader = (daSCoin_c *)dActor_c::FindWithID(mLeaderUniqueID);
    if (!leader) return;
    if (mGroupId != 0xf && mGroupId != leader->mCollectedCount) return;
    func_02012790(0x25);
    leader->mCollectedCount++;
    {
        Vector3 v;
        v.x = mPosX;
        v.y = mPosY;
        v.z = mPosZ;
        SpawnNumber(v, leader->mCollectedCount, false, 0, 0);
    }
    mdCcAc_c.flags |= 1;
    if (leader->mCollectedCount == 5) { mDeathTimer = 0x1e; return; }
    if (mGroupRole != 2) return;
    MarkForDestruction();
}

/*   _ZN9daSCoin_cD1Ev  0x020f03c4  size 0x30  (complete-object destructor)   */
/*   _ZN9daSCoin_cD0Ev  0x020f03f4  size 0x44  (deleting destructor)          */

// @symbol _ZN9daSCoin_cD0Ev
// @symbol _ZN9daSCoin_cD1Ev
