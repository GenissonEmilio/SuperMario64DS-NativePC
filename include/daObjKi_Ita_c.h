#ifndef DAOBJKI_ITA_C_H
#define DAOBJKI_ITA_C_H

#include "types.h"

extern "C" void *_ZN7fBase_cnwEj(unsigned size);

/**
 * Jolly Roger Bay floating plank (`ita` = board).
 *
 * A daObjFloatBoard_c leaf: no fields of its own. InitResources hands
 * this overlay's files to the shared ov002 helper; CleanupResources
 * is the base's.
 */

#ifdef __cplusplus

#include "daObjFloatBoard_c.h"

struct daObjKi_Ita_c : daObjFloatBoard_c {
    int InitResources();               /* slot  0 */

    static void *operator new(size_t size);
    #ifdef _MSC_VER
    /* host two-arm destructor spelling; ruling in include/ModelBase.h */
    virtual void Destructor1();   /* D1 */
    virtual void Destructor0();   /* D0 */
    ~daObjKi_Ita_c() {}   /* no slot */
    #else
    virtual ~daObjKi_Ita_c() {}
    #endif
};

inline void *daObjKi_Ita_c::operator new(size_t size)
{
    return _ZN7fBase_cnwEj(size);
}

#ifndef SM64DS_PLATFORM_PC
/* ROM layout under mwccarm; host ABI divergence is tracked separately. */
typedef char daObjKi_Ita_c_size_must_be_0x348[sizeof(daObjKi_Ita_c) == 0x348 ? 1 : -1];
#endif

#endif /* __cplusplus */

#endif /* DAOBJKI_ITA_C_H */
