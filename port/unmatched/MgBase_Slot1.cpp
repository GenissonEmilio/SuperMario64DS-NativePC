/* HOST COPY of dScMgBase_c slot 1, _ZN11dScMgBase_c19BeforeInitResourcesEv. Run link60, lane MG1.
 *
 * ONE OF FOUR. port/unmatched/MgBase_DeclConflict.cpp carries the full
 * derivation, the sweep that found exactly four, and the reason the fix is not
 * in include/decl_common.h. The short version: that header declares this
 * function void* while src/minigames/d_s_mg_base.cpp defines it char*, and the
 * TU includes the header, so MSVC refuses it with C2733. mwccarm accepts it.
 *
 * WHAT CHANGED: `char* c` became `void *cv`, plus a first line
 * `char *c = (char *)cv;`. Nothing else. The body below is the src file's,
 * statement for statement.
 *
 * WHY IT IS ITS OWN TU rather than living beside the other three: src spells
 * data_0209d460 and data_0209d458 as `unsigned char[]` here and slot 30 spells
 * the same two globals as scalar u8. Both are right for their own body and one
 * TU cannot hold both, so the four copies keep the one-body-per-TU shape src
 * has. Merging them was tried and produced C2040 on exactly those two names.
 *
 * Ruled REAL_DECOMP against the ROM before seating: 0x108, 60 instructions, a
 * six-word literal pool every word of which src names. See
 * port/tools/inferred_stub_adjudicated.txt.
 */
#include "types.h"
#include "decl_Scene.h"
#include "decl_common.h"
#include "dScMgBase_c.h"

/* ---- _ZN11dScMgBase_c19BeforeInitResourcesEv, slot 1, BeforeInitResources -------------------- */
namespace mg30 {
struct Obj {
    virtual void v0();  virtual void v1();  virtual void v2();  virtual void v3();
    virtual void v4();  virtual void v5();  virtual void v6();  virtual void v7();
    virtual void v8();  virtual void v9();  virtual void v10(); virtual void v11();
    virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15();
    virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19();
    virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23();
    virtual void v24(); virtual void v25(); virtual int v26();  virtual void v27();
    virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31();
    virtual void v32(); virtual void v33();
};
}  /* namespace mg30 */

extern "C" {
extern void Enable3dEngines(void);
extern char gMgMenuRow[];
extern void *sMgWorkBuffer;
/* PORTDROP2: main declares gMgScene as the scene class pointer now (include/dScMgBase_c.h) */
extern char data_0209f61c[];
extern unsigned char data_0209d460[];
extern unsigned char data_0209d458[];

// PORT_HOST_ABI: decl_common.h declares this void* while src defines it char*, which MSVC rejects as C2733 where mwccarm accepts; host copy respells the parameter
int _ZN11dScMgBase_c19BeforeInitResourcesEv(void *cv)
{
    char *c = (char *)cv;                       /* the one added line */
    struct dScMgBase_c *self = (struct dScMgBase_c *)(void *)c;
    /* PORTSYNC2: dScene_c::BeforeInitResources is a real virtual member in main
       (include/dScene_c.h, slot 1); the qualified call is the ROM's direct `bl`. */
    if (((dScene_c *)c)->dScene_c::BeforeInitResources() == 0) return 0;
    if (((mg30::Obj*)c)->v26() == 0)
        func_02019028();
    else
        Enable3dEngines();
    self->mMusicTrack = *(int*)(gMgMenuRow + 0x28);
    if (sMgWorkBuffer == 0)
        sMgWorkBuffer = _ZN6Memory13operator_new2Ej(0x4000);
    if (gMgScene != 0)
        *(int*)((char *)gMgScene + 0xb0) = 0;
    self->mHudScore = 0;
    self->mBestHudScore = 0;
    self->mNewRecord = 0;
    Mg_ResetBannerLayout(c + 0x4000);
    ((mg30::Obj*)c)->v33();
    Mg_LoadBannerFiles();
    _ZN8dScene_c9SetFadersEP15dFdBrightness_c(data_0209f61c);
    _ZN7dWipe_c13func_0202ec9cEi(data_0209f61c, 0);
    data_0209d460[0] = 0;
    data_0209d458[0] = 0;
    ((mg30::Obj*)c)->v31();
    return 1;
}
}  /* extern "C" */

