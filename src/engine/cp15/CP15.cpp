//cpp
/* arm9/CP15 -- the coprocessor-15 cache primitives at .text
 * 0x02058bb0..0x02058c84, folded from seven legacy shards, the four
 * compiled free functions that continue the run (func_02058c84,
 * func_02058cd0, func_02058d58, func_02058d6c), and OS_GetInitArenaLo, which
 * closes it at 0x02058df4.
 * HAND-ASM PRIMITIVE. Each cache primitive is a whole-function `asm`
 * block: `mcr p15` coprocessor writes are not reachable from C, so
 * the asm is the faithful source rather than a transcription of
 * something lost. func_02058b9c bounds the run
 * below. Above, func_02058df4 stops the run: it reads the boot vectors
 * func_00000000/func_00000600, which no linked object defines (its delinks
 * entry was never `complete`), so folding it in breaks the ROM link -- the
 * rest of the note run (the 0x02058df4..0x0205938c helpers and the
 * 0x020593a8 DTCM/MPU band) stays out. The other CP15 members (system
 * control, the 0x020593a8 band) live in their own
 * runs and are deliberately out of scope -- the linker places one object's
 * .text contiguously.
 *
 *   FlushDataCache() / FlushAndInvalidateDataCache() -- whole-cache clean and
 *   clean+invalidate by set and way (c7,c10,2 / c7,c14,2).
 *   InvalidateDataCache, FlushDataCache, FlushAndInvalidateDataCache,
 *   InvalidateInstructionCache -- the same ops ranged over an MVA window
 *   (c7,c6,1 / c7,c10,1 / c7,c14,1 / c7,c5,1).
 *   DrainWriteBuffer -- wait for the write buffer to empty (c7,c10,4).
 */
// leftovers: the func_02058df4 band stays out (it reads the boot vectors
// func_00000000/func_00000600, which no linked object defines, so folding it
// in breaks the ROM link). The func_02058c84/58cd0/58d58/58d6c helpers keep
// address names. func_02058d58/02058d6c write literal arena words
// (0x27ffda0/0x27ffdc4); no symbol names those slots.

namespace CP15 {

// @symbol _ZN4CP1514FlushDataCacheEv
asm void FlushDataCache(void)
{
    mov r1, #0
outer:
    mov r0, #0
seg:
    orr r2, r1, r0
    mcr p15, 0, r2, c7, c10, 2
    add r0, r0, #0x20
    cmp r0, #0x400
    blt seg
    add r1, r1, #0x40000000
    cmp r1, #0
    bne outer
    bx lr
}

/* Clean and invalidate the whole data cache by set and way (c7,c14,2). Same
 * traversal as FlushDataCache: r1 walks the four ways in the top two bits, r0
 * walks the 32 lines of each set; the outer loop ends when r1 wraps to zero. */
// @symbol _ZN4CP1527FlushAndInvalidateDataCacheEv
asm void FlushAndInvalidateDataCache(void)
{
    mov r1, #0
outer:
    mov r0, #0
seg:
    orr r2, r1, r0
    mcr p15, 0, r2, c7, c14, 2
    add r0, r0, #0x20
    cmp r0, #0x400
    blt seg
    add r1, r1, #0x40000000
    cmp r1, #0
    bne outer
    bx lr
}

// @symbol _ZN4CP1519InvalidateDataCacheEjj
asm void InvalidateDataCache(unsigned int addr, unsigned int size)
{
    add r1, r1, r0
    bic r0, r0, #0x1f
loop:
    mcr p15, 0, r0, c7, c6, 1
    add r0, r0, #0x20
    cmp r0, r1
    blt loop
    bx lr
}

// @symbol _ZN4CP1514FlushDataCacheEjj
asm void FlushDataCache(unsigned int addr, unsigned int size)
{
    add r1, r1, r0
    bic r0, r0, #0x1f
loop:
    mcr p15, 0, r0, c7, c10, 1
    add r0, r0, #0x20
    cmp r0, r1
    blt loop
    bx lr
}

// @symbol _ZN4CP1527FlushAndInvalidateDataCacheEjj
asm void FlushAndInvalidateDataCache(unsigned int addr, unsigned int size)
{
    add r1, r1, r0
    bic r0, r0, #0x1f
loop:
    mcr p15, 0, r0, c7, c14, 1
    add r0, r0, #0x20
    cmp r0, r1
    blt loop
    bx lr
}

// @symbol _ZN4CP1516DrainWriteBufferEv
asm void DrainWriteBuffer(void)
{
    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4
    bx lr
}

// @symbol _ZN4CP1526InvalidateInstructionCacheEjj
asm void InvalidateInstructionCache(unsigned int addr, unsigned int size)
{
    add r1, r1, r0
    bic r0, r0, #0x1f
loop:
    mcr p15, 0, r0, c7, c5, 1
    add r0, r0, #0x20
    cmp r0, r1
    blt loop
    bx lr
}

} // namespace CP15

extern "C" {

extern void func_02058f28(void);
extern void func_0205b858(void);
extern void func_02057320(void);
extern void func_02058ec8(void);
extern void func_02057000(void);
extern void func_02059594(void);
extern void func_02059f48(int v);
extern void func_02059cb4(void);
extern void func_02058308(void);
extern void func_02059e48(void);
extern void func_0206a88c(void);
extern void func_02060890(void);
extern void func_0205fde8(void);
extern unsigned int func_02058ea0(int idx);
extern unsigned int func_02058eb4(int idx);
extern void func_02058d58(int idx, unsigned int val);

// @symbol OS_GetInitArenaLo
unsigned int OS_GetInitArenaLo(int id)
{
    switch (id) {
    case 0: return 0x214eaa0;
    case 2: return 0x2400000;
    case 3: return 0x1ffdf40;
    case 4: return 0x23c0020;
    case 5: return 0x27ff000;
    case 6: return 0x37f8000;
    }
    return 0;
}

// @symbol func_02058d6c
void func_02058d6c(int idx, unsigned int val) {
    ((unsigned int *)0x27ffdc4)[idx] = val;
}

// @symbol func_02058d58
void func_02058d58(int idx, unsigned int val) {
    ((unsigned int *)0x27ffda0)[idx] = val;
}

// @symbol func_02058cd0
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

// @symbol func_02058c84
void func_02058c84(void)
{
    func_02058f28();
    func_0205b858();
    func_02057320();
    func_02058ec8();
    func_02057000();
    func_02059594();
    func_02059f48(3);
    func_02059cb4();
    func_02058308();
    func_02059e48();
    func_0206a88c();
    func_02060890();
    func_0205fde8();
}

} // extern "C"
