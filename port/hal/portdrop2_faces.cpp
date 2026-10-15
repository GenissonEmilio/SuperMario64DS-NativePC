/* hal/portdrop2_faces.cpp -- flat ROM spellings of members main now defines.
 *
 * PORTDROP2 (the merge of origin/main 03a6d6aea41). Main folded
 * dMgPsOpt_c::IsIdle into src/minigames/d_mg_ps_opt.cpp as a real member; the
 * port caller that spells the old flat C name (port/unmatched/MgBase_Slot7.cpp)
 * gets a one-statement forwarder. No behaviour of its own. */
#include "dMgPsOpt_c.h"

extern "C" {

int _ZN10dMgPsOpt_c6IsIdleEv(char *p)
{
    return ((dMgPsOpt_c *)p)->dMgPsOpt_c::IsIdle();
}

}
