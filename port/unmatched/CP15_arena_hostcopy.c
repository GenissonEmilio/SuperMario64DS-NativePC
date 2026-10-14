/* HOST COPY of OS_SetArenaLo (func_02058d58, arm9 0x02058d58, size 0x14) and
 * OS_AllocFromArenaLo (func_02058cd0, arm9 0x02058cd0, size 0x88), PORTSYNC2.
 *
 * port/slice_w23_arena.txt ran both as the ROM's own bodies from their
 * one-function files (src/func_02058d58.c, src/func_02058cd0.c; see the
 * slice's header for the ROM-address verification). The 2026-10 main merge
 * folded both into src/engine/cp15/CP15.cpp, a TU whose other members are
 * the seven whole-function `asm` CP15 cache primitives (0x02058bb0..
 * 0x02058c84), which MSVC cannot read and hostgen.py refuses. The TU also
 * defines func_02058c84 (transcribed by port/hal/boot_os.cpp),
 * OS_GetInitArenaLo and func_02058d6c, none of which the port line compiled.
 * The cache primitives are answered by port/ntr/runtime.cpp (_ZN4CP15...).
 * So the slice row is this file: the two bodies the slice ran, text
 * identical to main's CP15.cpp. Retire it if CP15.cpp gains a host route. */

extern unsigned int func_02058ea0(int idx);
extern unsigned int func_02058eb4(int idx);
extern void func_02058d58(int idx, unsigned int val);

void func_02058d58(int idx, unsigned int val) {
    ((unsigned int *)0x27ffda0)[idx] = val;
}

unsigned int func_02058cd0(int idx, unsigned int size, unsigned int align)
{
    unsigned int base = func_02058ea0(idx);
    unsigned int start, t, end;
    if (base == 0) return 0;
    start = (base + align - 1) & ~(align - 1);
    t = start + size;
    end = (t + align - 1) & ~(align - 1);
    if (end > func_02058eb4(idx)) return 0;
    func_02058d58(idx, end);
    return start;
}
