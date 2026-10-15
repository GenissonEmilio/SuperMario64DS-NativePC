//cpp
/* IRQ CPSR primitives, .text 0x02059d08..0x02059d74.
 * HAND-ASM PRIMITIVE: mrs/msr read and write CPSR state. Bit 0x80 is IRQ,
 * 0x40 is FIQ. Each operation returns the previous state for restoration.
 * The preceding IRQ-state initializer has its own compiler input.
 */

namespace IRQ {

// @symbol _ZN3IRQ6EnableEv
asm unsigned int Enable(void)
{
    mrs r0, cpsr
    bic r1, r0, #0x80
    msr cpsr_c, r1
    and r0, r0, #0x80
    bx lr
}

// @symbol _ZN3IRQ7DisableEv
asm unsigned int Disable(void)
{
    mrs r0, cpsr
    orr r1, r0, #0x80
    msr cpsr_c, r1
    and r0, r0, #0x80
    bx lr
}

// @symbol _ZN3IRQ7RestoreEj
asm unsigned int Restore(unsigned int state)
{
    mrs r1, cpsr
    bic r2, r1, #0x80
    orr r2, r2, r0
    msr cpsr_c, r2
    and r0, r1, #0x80
    bx lr
}

// @symbol _ZN3IRQ10DisableAllEv
asm unsigned int DisableAll(void)
{
    mrs r0, cpsr
    orr r1, r0, #0xc0
    msr cpsr_c, r1
    and r0, r0, #0xc0
    bx lr
}

// @symbol _ZN3IRQ10RestoreAllEj
asm unsigned int RestoreAll(unsigned int state)
{
    mrs r1, cpsr
    bic r2, r1, #0xc0
    orr r2, r2, r0
    msr cpsr_c, r2
    and r0, r1, #0xc0
    bx lr
}

}
