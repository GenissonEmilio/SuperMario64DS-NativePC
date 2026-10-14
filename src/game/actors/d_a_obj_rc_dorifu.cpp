//cpp
/**
 * Rainbow Ride's Tricky Triangles (`dorifu`). Twin of the BitDW
 * leaf (`daObjKm1_Dorifu_c`) and the BitS leaf (`daObjKm3_Dorifu_c`).
 *
 * No fields of its own. InitResources / CleanupResources hand this
 * overlay's five-plank file table to daObjDorifu_c's shared
 * overloads. InitResources loads each slot 0 with Model::LoadFile,
 * slot 1 with dBgW_Kc::LoadFile, slot 2 as CLPS into SetFile.
 * ov036 sinit constructs those SharedFilePtrs as file IDs
 * 1690 / 1691, 1692 / 1693, 1694 / 1695, 1696 / 1697, 1698 / 1699.
 *
 * The five-entry table is defined in this TU. Retail .data
 * order is typeinfo, type-name, profile, table, vtable.
 *
 * daObjRc_Dorifu_c_classInit is reconstructed (RTTI
 * daObjRc_Dorifu_c, RC_DORIFU registry). Retail does not
 * store that spelling.
 *
 * deslop
 * Leftover: the BMD/KCL SharedFilePtrs and CLPS_Block are still
 *   data_ov036_*. The CLPS words sit before this TU's claimed
 *   .data (typeinfo at 0x02113e4c); the SharedFilePtrs are BSS.
 */

#include "daObjRc_Dorifu_c.h"

struct DorifuSpawnInfo {
    daObjRc_Dorifu_c *(*classInit)();
    s16 executePriority; /* +4: also RC_DORIFU registry id 0x007b = 123 */
    s16 renderPriority;  /* +6 */
    u32 actorFlags;
    Fix12i clipOffsetY;
    Fix12i clipRadius;
    Fix12i clipDistance;
    Fix12i farDistance;
};
typedef char DorifuSpawnInfo_size_must_be_0x1c[
    sizeof(DorifuSpawnInfo) == 0x1c ? 1 : -1];

extern "C" {
extern CLPS_Block data_ov036_02112be8;
extern CLPS_Block data_ov036_02112c08;
extern CLPS_Block data_ov036_02112a88;
extern CLPS_Block data_ov036_02112b88;
extern CLPS_Block data_ov036_02112ae8;
}

/* File-scope objects at the end of this file construct the ten resource
 * handles (model files 1690/1692/1694/1696/1698, collision files
 * 1691/1693/1695/1697/1699). mwcc emits __sinit_d_a_obj_rc_dorifu.cpp from
 * those definitions. The wrapper names are local; the handle constructors
 * and destructors are the ROM resource-family functions, aliased in the
 * manifest. */
struct RcDorifuModelFilePtr : SharedFilePtr {
    u32 words[2];

    RcDorifuModelFilePtr(u32 fileID);
    ~RcDorifuModelFilePtr();
};

struct RcDorifuCollisionFilePtr : SharedFilePtr {
    u32 words[2];

    RcDorifuCollisionFilePtr(u32 fileID);
    ~RcDorifuCollisionFilePtr();
};

extern "C" {
extern RcDorifuModelFilePtr data_ov036_0211411c;
extern RcDorifuCollisionFilePtr data_ov036_021140f4;
extern RcDorifuModelFilePtr data_ov036_021140fc;
extern RcDorifuCollisionFilePtr data_ov036_021140d4;
extern RcDorifuModelFilePtr data_ov036_021140e4;
extern RcDorifuCollisionFilePtr data_ov036_021140ec;
extern RcDorifuModelFilePtr data_ov036_02114104;
extern RcDorifuCollisionFilePtr data_ov036_02114114;
extern RcDorifuModelFilePtr data_ov036_0211410c;
extern RcDorifuCollisionFilePtr data_ov036_021140dc;
}

// @symbol daObjRc_Dorifu_c_classInit
extern "C" daObjRc_Dorifu_c *daObjRc_Dorifu_c_classInit()
{
    return new daObjRc_Dorifu_c();
}

// @symbol g_profile_RC_DORIFU
extern "C" DorifuSpawnInfo g_profile_RC_DORIFU = {
    daObjRc_Dorifu_c_classInit,
    0x007b,
    0x00fd,
    0,
    0x00500000,
    0x01000000,
    0x02000000,
    0
};

extern "C" daObjDorifuResources data_ov036_02113e88[5] = {
    {&data_ov036_0211411c, &data_ov036_021140f4, &data_ov036_02112be8},
    {&data_ov036_021140fc, &data_ov036_021140d4, &data_ov036_02112c08},
    {&data_ov036_021140e4, &data_ov036_021140ec, &data_ov036_02112a88},
    {&data_ov036_02114104, &data_ov036_02114114, &data_ov036_02112b88},
    {&data_ov036_0211410c, &data_ov036_021140dc, &data_ov036_02112ae8}
};

// @symbol _ZN16daObjRc_Dorifu_c13InitResourcesEv
s32 daObjRc_Dorifu_c::InitResources()
{
    return daObjDorifu_c::InitResources(data_ov036_02113e88);
}

// @symbol _ZN16daObjRc_Dorifu_c16CleanupResourcesEv
s32 daObjRc_Dorifu_c::CleanupResources()
{
    return daObjDorifu_c::CleanupResources(data_ov036_02113e88);
}

/* Static-init globals (was the handwritten __sinit_ov036_0211279c shard).
 * Definition order is the retail initializer's construction order: the five
 * model handles (files 1690/1692/1694/1696/1698), then the five collision
 * handles (files 1691/1693/1695/1697/1699). */
RcDorifuModelFilePtr data_ov036_0211411c(0x69a);
RcDorifuModelFilePtr data_ov036_021140fc(0x69c);
RcDorifuModelFilePtr data_ov036_021140e4(0x69e);
RcDorifuModelFilePtr data_ov036_02114104(0x6a0);
RcDorifuModelFilePtr data_ov036_0211410c(0x6a2);
RcDorifuCollisionFilePtr data_ov036_021140f4(0x69b);
RcDorifuCollisionFilePtr data_ov036_021140d4(0x69d);
RcDorifuCollisionFilePtr data_ov036_021140ec(0x69f);
RcDorifuCollisionFilePtr data_ov036_02114114(0x6a1);
RcDorifuCollisionFilePtr data_ov036_021140dc(0x6a3);
