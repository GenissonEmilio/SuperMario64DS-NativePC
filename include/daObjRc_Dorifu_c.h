#ifndef DAOBJRC_DORIFU_C_H
#define DAOBJRC_DORIFU_C_H

#include "types.h"
#include "daObjDorifu_c.h"

extern "C" void *_ZN7fBase_cnwEj(unsigned size);

/**
 * Rainbow Ride's Tricky Triangles (`dorifu`). No fields of its
 * own: the factory allocates 0xdcc, which daObjDorifu_c fills
 * (five plank models at 0x320, five plank colliders at 0x4b0).
 * Overrides the two slots the base leaves null (InitResources,
 * CleanupResources) and hands this overlay's five-entry file table
 * to the shared overloads.
 *
 * `daObjRc_Dorifu_c` is the RTTI name.
 */
struct daObjRc_Dorifu_c : daObjDorifu_c {
    /* Inline empty dtor: mwccarm emits D1 then D0, no D2. */
    #ifdef _MSC_VER
    /* host two-arm destructor spelling; ruling in include/ModelBase.h */
    virtual void Destructor1();   /* D1 */
    virtual void Destructor0();   /* D0 */
    ~daObjRc_Dorifu_c() {}   /* no slot */
    #else
    virtual ~daObjRc_Dorifu_c() {}
    #endif
    int CleanupResources();                /* slot  3 */
    int InitResources();                   /* slot  0 */

    static void *operator new(size_t size) {
        return _ZN7fBase_cnwEj(size);
    }
};

#ifndef SM64DS_PLATFORM_PC
/* ROM layout under mwccarm; host ABI divergence is tracked separately. */
typedef char daObjRc_Dorifu_c_size_must_be_0xdcc[
    sizeof(daObjRc_Dorifu_c) == 0xdcc ? 1 : -1];
#endif

#endif /* DAOBJRC_DORIFU_C_H */
