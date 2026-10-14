//cpp
// @symbol _ZN3IRQ20UserInterruptHandlerEv
/* IRQ::UserInterruptHandler -- the ARM9 IRQ entry, itcm 0x01ff8020..0x01ff8078.
 * NitroSDK's OS_IrqHandler verbatim: read IE&IF, clear to the lowest set bit,
 * dispatch through the data_02099fe4 handler table (the same array
 * IRQ::GetIRQHandler/IRQ::SetIRQHandler index), and tail-jump through
 * func_01ffd9d4, the thread-switch return path -- lr is preloaded with its
 * address so the dispatched handler's `ldr pc, [sp], #4` exits both frames.
 *
 * leftovers: hand-asm stays -- the body needs `clz`, which mwccarm 2004/b56
 * has no intrinsic for (a portable count-leading-zeros loop compiles to 0xa8
 * bytes against the ROM's 0x58), and the cross-function stack handoff to
 * func_01ffd9d4 has no C expression either. The SDK source itself ships this
 * routine as `asm void OS_IrqHandler(void)`.
 */
#include "IRQ.h"

extern "C" {
extern IRQ::Handler data_02099fe4[];
extern void func_01ffd9d4(void);
}

namespace IRQ {

// HAND-ASM PRIMITIVE: NitroSDK's OS_IrqHandler verbatim; `clz` and the
// cross-function stack handoff to func_01ffd9d4 have no C expression under
// mwccarm 2004/b56, so the asm block is the faithful source.
asm void UserInterruptHandler(void)
{
    stmdb sp!, {lr}
    mov ip, #0x04000000
    add ip, ip, #0x210
    ldr r1, [ip, #-8]
    cmp r1, #0
    ldmeqia sp!, {pc}
    ldmia ip, {r1, r2}
    ands r1, r1, r2
    ldmeqia sp!, {pc}
    mov r3, #0x80000000
_loop:
    clz r0, r1
    bics r1, r1, r3, lsr r0
    bne _loop
    mov r1, r3, lsr r0
    str r1, [ip, #0x4]
    rsbs r0, r0, #0x1f
    ldr r1, =data_02099fe4
    ldr r0, [r1, r0, lsl #2]
    ldr lr, =func_01ffd9d4
    bx r0
}

}
