/* HOST ARM recovered by PORTSYNC2 (port/tools/hostarm_recover.py draft, reviewed).
 * The port line carried this in src/_ZN6Camera14GoBehindPlayerEj.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/view/d_camera.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte.
 *
 * The arm sat in the middle of the method, so the host definition is the
 * whole member: main's body text (Camera -> dCamera_c, its ChangeState member
 * call in place of the old flat-name call) with the arm's one statement put
 * back before the func_0200c66c call. Main's TU also defines this member, so
 * the TU's hostgen copy must park _ZN9dCamera_c14GoBehindPlayerEj. */
// @symbol _ZN9dCamera_c14GoBehindPlayerEj
#include "dCamera_c.h"
#include "decl_common.h"

extern "C" {
extern unsigned char data_0209f250;
extern signed char data_02092110;
extern void *data_0209f394[];
}

void dCamera_c::GoBehindPlayer(unsigned int j)
{
    int slot4, slot8, slotc;

    if (j != data_0209f250)
        return;
    if (data_02092110 >= 0)
        return;

    *(unsigned int *)(((int)&mFlags)) &= 0xfffffaf7u;
    func_0200cb58((void *)((int)this), 0xa);
    *(unsigned int *)(((long long)(int)((int)&mFlags))) |= 4u;

    slot4 = mState_13c;
    slotc = 0;
    /* THE VIEW SLOT HOLDS WHAT THE CARTRIDGE'S STACK LEAVES IN IT. The ROM
       never writes slot8 either (0x0200d304: only sp+4, sp+0xc and sp are
       stored before the call at 0x0200d398), and func_0200c66c reads it back
       as its view object whenever the floor names no view (col2 == 0x3f).
       On the cartridge that word is the caller's player pointer, pushed by
       the call made just before this one: daStarGate_c::St_OpenClose_Main
       reaches it through func_ov100_02144fcc -> func_02012be0's push of r6
       (the player, 0x0214598c / 0x02012be0), and the Door's open callback
       func_ov100_02144730 through Animation::WillHitFrame's push of r4 (the
       player, 0x021448c0 / 0x02015a98). j is that player's index (checked
       above). On the host the slot held whatever the last call left there,
       and a star door's close faulted reading it (eax 0x47). */
    slot8 = (int)data_0209f394[j];
    func_0200c66c((void *)((int)this), (void *)(mTargetPlayer + 0x5c), &slot8, &slot4, &slotc);
    if (slot4 == (int)&data_020873dc)
        return;
    if (slot4 == (int)&data_0208742c)
        return;
    ChangeState((State *)&data_0209b0e8);
}
