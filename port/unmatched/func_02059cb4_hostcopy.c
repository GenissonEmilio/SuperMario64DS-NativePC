/* HOST COPY of func_02059cb4 (arm9 0x02059cb4, size 0x54), PORTSYNC2.
 *
 * The port line compiled src/func_02059cb4.c on port/slice_gate214.txt (the
 * boot spine calls it from hal/boot_os.cpp's func_02058c84 transcription).
 * The 2026-10 main merge folded it into src/IRQ3.cpp, beside the five
 * whole-function `asm` CPSR primitives IRQ::Enable / Disable / Restore /
 * DisableAll / RestoreAll (0x02059d08..0x02059d74). MSVC cannot read those,
 * and hostgen.py refuses a file with an asm block, so the TU cannot be
 * compiled for the host. The host already answers the five primitives:
 * port/ntr/runtime.cpp (_ZN3IRQ6EnableEv, _ZN3IRQ7DisableEv,
 * _ZN3IRQ7RestoreEj) and port/hal/boot_hw.cpp (_ZN3IRQ10DisableAllEv,
 * _ZN3IRQ10RestoreAllEj). So the slice row is this file: the one compiled
 * body, text identical to main's src/IRQ3.cpp (and to the old one-function
 * file). Retire it if IRQ3.cpp ever gains a host route. */
#include "types.h"

extern unsigned int _ZN3IRQ11DisableIRQsEj(u32 mask);

extern volatile u16 data_020a644c;    /* 0x020a644c */
extern u32 data_020a6450[2];          /* 0x020a6450 */

void func_02059cb4(void) {
    if (data_020a644c != 0)
        return;
    u32 *data = data_020a6450;
    data_020a644c = 1;
    data[0] = 0;
    data[1] = 0;
    _ZN3IRQ11DisableIRQsEj(4);
}
