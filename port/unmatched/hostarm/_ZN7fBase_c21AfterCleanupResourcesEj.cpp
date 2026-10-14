/* HOST COPY of src/_ZN7fBase_c21AfterCleanupResourcesEj.cpp as the port line
 * compiled it before the 2026-10 main merge (749f5799a) folded the body into
 * src/actors/ActorBase.cpp. It has to stay a translation unit of its own:
 * port/CMakeLists.txt force-includes unmatched/FBase_DtorDispatch_HostAbi.h,
 * which SHADOWS fBase_c, into it (the slot-16 destructor dispatch ruling),
 * and that shadow cannot be put over the whole class TU. GATE9 hostgens it
 * with --extern-data as before; tools/hostgen.py HOST_COPY_PARK parks the
 * folded body in the ActorBase.cpp copy so the member is defined once. */
#include "fBase_c.h"
#include "Heap.h"

/* fBase_c::AfterCleanupResources(u32 vfSuccess) at 0x02043b2c.
 *
 * Successful cleanup unlinks this actor's manager nodes, destroys its private
 * heap and auxiliary state, dispatches the complete-object destructor through
 * vtable slot 16, then releases the actor through the game heap. The typed
 * manager layout and virtual destructor come from fBase_c itself; no local
 * shadow class is needed to spell either operation. */
extern "C" {
#ifdef SM64DS_PLATFORM_PC
extern char data_020a4b6c;
#else
char data_020a4b6c;
#endif
char data_020a4ba8;
void func_0203b3c0(void *, void *);
void func_0203b27c(void *, void *);
void func_02044334(void *);
}

void fBase_c::AfterCleanupResources(u32 vfSuccess)
{
    if (vfSuccess != 2)
        return;

    func_0203b3c0(&data_020a4b6c, &manager.sceneNode);
    func_0203b27c(&data_020a4ba8, &manager.behaviorNode);

    if (heap)
        static_cast<Heap *>(heap)->_Destroy();
    if (lifecycleState)
        func_02044334(lifecycleState);

    this->~fBase_c();
    fBase_c::operator delete(this);
}
