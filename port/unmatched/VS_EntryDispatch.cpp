/* dScEntry_c's THREE POINTER-TO-MEMBER DISPATCH SITES, host copies -- the
 * MgTrampolineTerror_StateDispatch treatment, applied to the VS menu.
 *
 * THE DEFECT CLASS (measured, not predicted): the scene's states live as
 * {fn, delta} pairs at self+0x54/+0x5c/+0x64, seeded by func_ov075_0211a194
 * from six-word records the sinits build in bss, and dispatched by Behavior
 * (_ZN10dScEntry_c8BehaviorEv), Render (_ZN10dScEntry_c6RenderEv) and the setter itself
 * through `(self->**pmf)()`. The matched TUs model that with a real MSVC
 * pointer-to-member, which compiles to a __thiscall: `this` rides in ecx and
 * nothing goes to the stack. The fn words hold plain cdecl host bodies (the
 * seat's generated patch put them there), so the receiver never arrives --
 * the first VS lobby boot faulted in state func_ov075_02119b34 at +0x37
 * reading [esi+0x50] with esi = stack garbage (1). Same story for Behavior's
 * element walk, which the TU models as a virtual call `o->v0()` over the
 * 0x24-stride records at self+0x70 whose vptr is the mounted
 * _ZTVN10dScEntry_c6icon_cE table.
 *
 * THE COPIES CHANGE THE CALL SHAPE AND NOTHING ELSE: every dispatch becomes
 * an explicit cdecl call through the record word with self as the first
 * argument -- which is exactly what the mwcc pair-dispatch does on ARM
 * (r0 = self + delta, delta 0 throughout this single-inheritance scene).
 * Run hunt8, lane WIGGLER1: the three host copies of the bodies are RETIRED.
 * The matched TUs (src/unnamed/ov075/func_ov075_0211a194.cpp,
 * src/_ZN10dScEntry_c8BehaviorEv.cpp, src/_ZN10dScEntry_c6RenderEv.cpp) are on
 * the slice through PORT_HOSTGEN_TU, and tools/hostgen_portsync2.py rewrites
 * each one's pointer-to-member dispatch into a call to port_vs_state_call
 * below, which is the dispatch the copies spelled by hand. The fader dispatch
 * data_0209f5bc->v5() and the icon walk keep the TUs' own virtual calls:
 * those objects' host vtables already serve every other scene's matched TUs.
 */

#include <cstdio>

typedef void (*VsStateFn)(void *self);

// PORT_HOST_ABI: mwcc pointer-to-member dispatch spelled as an explicit cdecl call through the record word with self as the first argument
extern "C" void port_vs_state_call(void *self, unsigned off)
{
    char *c = (char *)self;
    VsStateFn fn = *(VsStateFn *)(c + off);
    /* the mwcc pair: fn word at +off, delta word at +off+4; the ROM calls
       fn(self + delta) and every record this scene builds carries delta 0 */
    char *recv = c + *(int *)(c + off + 4);
    if (fn)
        fn(recv);
}
