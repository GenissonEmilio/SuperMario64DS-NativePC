//cpp
/* Process-list node destructor. If this node is the one currently executing,
 * clear that pointer.
 *
 * leftovers:
 * - data_020a4b68 stays void *. fLiNdBa_c's destructor spells the same word
 *   fLiNdBa_c *, and the crash screen reads it as void *. Typing it here
 *   would be a second claim on one global.
 */
#include "fListNode.h"

extern "C" void *data_020a4b68;

fLiNdBaPr_c::~fLiNdBaPr_c()
{
    if (data_020a4b68 == this)
        data_020a4b68 = 0;
}
