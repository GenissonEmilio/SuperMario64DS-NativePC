/* HOST COPIES of src/func_ov084_0212c960.cpp and src/func_ov084_0212c9a8.cpp
 * -- the BOB_OMB_BUDDY's state machine, and the mwcc pointer-to-member
 * dispatch for the ninth time in this port.
 *
 * The matched pair is
 *
 *     struct C; typedef void (C::*PMF)();
 *     struct Entry { PMF pmf[2]; };
 *     extern Entry data_ov084_02130dc4[];
 *     ...
 *     c->idx = i;  (c->*data_ov084_02130dc4[i].pmf[0])();     // change state
 *     ...          (c->*data_ov084_02130dc4[c->idx].pmf[1])(); // run it
 *
 * and `C` is INCOMPLETE where the typedef is formed. MSVC answers that with
 * the four-word general representation, so an Entry becomes 32 bytes where
 * mwcc's is 16 and state 2 reads out of the middle of state 0.
 *
 * The ROM's table is three states of two {function, delta} halves --
 * __sinit_ov084_02130558 copies six 8-byte statics into it -- so that is what
 * is read here, with the same virtual-bit test the Butterfly's matched source
 * spells out for itself.
 *
 * THE SIX STATICS ARE SEATED on both sides of the sinit, the treatment
 * port/unmatched/Enemy_UpdateDeath.cpp settled on: the sinit runs from the
 * window's boot and the registry runs later, so seating only the source side
 * would leave the copy full of DS addresses.
 *
 * STATES, in the order the sinit stores them:
 *   0  {0212c92c, 0212c8b0}   idle by the cannon, facing the player
 *   1  {0212c89c, 0212c508}   the talk, and the cannon-open path
 *   2  {0212c4a0, 0212c1a0}   walking back to the post
 *
 * All six halves are matched src, so this file carries no trap.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "daRedBombhei_c.h"
#include "daKrb_c.h"

extern "C" {

struct PortBuddyPmf { unsigned fn; int delta; };
struct PortBuddyState { PortBuddyPmf init, main; };

/* the six ROM statics the sinit copies, and the bss table it copies into */
extern PortBuddyPmf data_ov084_021309dc[], data_ov084_021309ec[],
    data_ov084_021309cc[], data_ov084_021309e4[], data_ov084_021309c4[],
    data_ov084_021309d4[];
extern PortBuddyState data_ov084_02130dc4[];

}  /* extern "C" */

typedef void (*PortBuddyFn)(void *);

/* PORTSYNC3: the dispatcher in the class TU calls the cell's function word with
   `this` in ecx (a thiscall member), so the host word is the MEMBER's address. The
   flat symbol of the same name is the facegen face, which reads `this` off the
   stack, and seating it gave the state body a garbage receiver. */
template <class C> static PortBuddyFn port_member_addr(void (C::*m)())
{
    PortBuddyFn p;
    std::memcpy(&p, &m, sizeof p);
    return p;
}
static struct { PortBuddyPmf *slot; unsigned rom; PortBuddyFn host; }
g_buddy_states[] = {
    {data_ov084_021309dc, 0x0212c92c, 0},
    {data_ov084_021309ec, 0x0212c8b0, 0},
    {data_ov084_021309cc, 0x0212c89c, 0},
    {data_ov084_021309e4, 0x0212c508, 0},
    {data_ov084_021309c4, 0x0212c4a0, 0},
    {data_ov084_021309d4, 0x0212c1a0, 0},
};

static void port_buddy_seat_one(PortBuddyPmf *p, unsigned rom, PortBuddyFn host,
                                const char *where, unsigned i)
{
    if (p->fn == (unsigned)(size_t)host)
        return;
    if ((unsigned)(p->fn) < 0x02000000u || (unsigned)(p->fn) >= 0x02400000u) return;  /* PORTSYNC3: a mount word that is not a DS code address: the class TU owns the live table */
    if (p->fn != rom || p->delta != 0) {
        std::fprintf(stderr, "FATAL: ov084 state half %u (%s): the mount "
                     "holds %08x/%d, the ROM's own table says %08x/0 -- WRONG "
                     "BYTES\n", i, where, p->fn, p->delta, rom);
        std::abort();
    }
    p->fn = (unsigned)(size_t)host;
}

extern "C" void port_bob_omb_buddy_states_seat(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    g_buddy_states[0].host = port_member_addr(&daRedBombhei_c::func_ov084_0212c92c);
    g_buddy_states[1].host = port_member_addr(&daRedBombhei_c::func_ov084_0212c8b0);
    g_buddy_states[2].host = port_member_addr(&daRedBombhei_c::func_ov084_0212c89c);
    g_buddy_states[3].host = port_member_addr(&daRedBombhei_c::func_ov084_0212c508);
    g_buddy_states[4].host = port_member_addr(&daRedBombhei_c::func_ov084_0212c4a0);
    g_buddy_states[5].host = port_member_addr(&daRedBombhei_c::func_ov084_0212c1a0);
    for (unsigned i = 0; i < 6; ++i) {
        PortBuddyPmf *dst =
            (i & 1) ? &data_ov084_02130dc4[i / 2].main
                    : &data_ov084_02130dc4[i / 2].init;
        port_buddy_seat_one(g_buddy_states[i].slot, g_buddy_states[i].rom,
                            g_buddy_states[i].host, "static", i);
        port_buddy_seat_one(dst, g_buddy_states[i].rom, g_buddy_states[i].host,
                            "table", i);
    }
}

/* BOTH DISPATCHERS ARE BACK ON THE SLICE. src/func_ov084_0212c960.cpp and
   src/func_ov084_0212c9a8.cpp are on port/slice_pmf3.txt (run link100 lane
   PMF3). Under /vmg /vmm an Entry is the ROM's sixteen bytes rather than
   MSVC's thirty-two, the emitted bodies stride `shl edx, 4` and TAIL JUMP, and
   port/hal/pmf3_aliases.cpp bridges the mangled table name onto the mount's C
   symbol. The seat above is unchanged and is the gate: it aborts on a nonzero
   adjustment word and rewrites the function word with a host body on BOTH
   sides of the sinit, the six statics and the six destination halves, so what
   the matched dispatchers read is a host address with a zero delta. The six
   source pairs at ov084 0x021309c4..0x021309ec were re-read out of
   overlay_0084.bin with their relocations, every adjustment word ROM zero.

   port_buddy_call went with them -- it was the hand-rolled {function, delta}
   call and nothing else used it. It carried a virtual-member-pointer refusal
   that tested bit 0 of the FUNCTION word, which is not where the ROM keeps
   that flag: the ROM tests bit 0 of the ADJUSTMENT word (`ands r1,r1,#1` on
   the word it has just loaded from record+4). The seat's own abort covers
   that case properly and earlier, because it refuses any nonzero adjustment
   at all. port_buddy_seat_one, which the goomba half below still calls,
   stays. */

/* ---- and the GOOMBA's five, which need SEATING and nothing else ------------
   Goomba::Behavior dispatches data_ov084_02130d74[state * 2] the way the
   Butterfly's matched source does -- it reads the pair as two plain ints and
   does the virtual-bit and this-adjustment arithmetic itself -- so MSVC's
   pointer-to-member representation never comes into it and there is no host
   copy. What it does need is the same seat: __sinit_ov084_0213035c copies five
   8-byte statics into that table and every one of them is a DS code address.
   Measured: with the table unseated, spawning a goomba jumped to 0x0212b2dc,
   which is state 0's own body in the overlay image.

   STATES: 0 walk, 1 turn, 2 squashed, 3 knocked back, 4 the spawner's wait. */
extern "C" {
extern PortBuddyPmf krbStateDesc_Walk[], krbStateDesc_HopStart[],
    data_ov084_021308c4[], krbStateDesc_Tumble[], krbStateDesc_Pause[];
extern PortBuddyPmf data_ov084_02130d74[];

void func_ov084_0212b2dc(void *self);
void func_ov084_0212ab48(void *self);
void _ZN7daKrb_c19func_ov084_0212aab0Ev(void *self);
void func_ov084_0212a774(void *self);
void func_ov084_0212a6f8(void *self);
}

/* PORTSYNC3: main's daKrb_c::Behavior calls these through a real pointer to member
   (`(this->*table[state])()`), so the host word is entered with `this` in ecx and
   nothing on the stack. Four of the five targets are free functions that read
   `this` off the stack (the ROM's cdecl-shaped stubs), so each cell holds a
   fastcall adapter that re-pushes it; the fifth is a member whose own address is
   used. */
template <void (*F)(void *)> static void __fastcall port_krb_adapt(void *self, void *)
{ F(self); }
static struct { PortBuddyPmf *slot; unsigned rom; PortBuddyFn host; PortBuddyFn raw; }
g_goomba_states[] = {
    {krbStateDesc_Walk, 0x0212b2dc, (PortBuddyFn)port_krb_adapt<func_ov084_0212b2dc>, func_ov084_0212b2dc},
    {krbStateDesc_HopStart, 0x0212ab48, (PortBuddyFn)port_krb_adapt<func_ov084_0212ab48>, func_ov084_0212ab48},
    {data_ov084_021308c4, 0x0212aab0, 0, 0},
    {krbStateDesc_Tumble, 0x0212a774, (PortBuddyFn)port_krb_adapt<func_ov084_0212a774>, func_ov084_0212a774},
    {krbStateDesc_Pause, 0x0212a6f8, (PortBuddyFn)port_krb_adapt<func_ov084_0212a6f8>, func_ov084_0212a6f8},
};

extern "C" void port_goomba_states_seat(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    g_goomba_states[2].host = port_member_addr(&daKrb_c::func_ov084_0212aab0);
    for (unsigned i = 0; i < 5; ++i) {
        port_buddy_seat_one(g_goomba_states[i].slot, g_goomba_states[i].rom,
                            g_goomba_states[i].host, "goomba static", i);
        port_buddy_seat_one(&data_ov084_02130d74[i], g_goomba_states[i].rom,
                            g_goomba_states[i].host, "goomba table", i);
        /* PORTSYNC3: the class TU's own initialiser has already filled the table with
           the free functions' HOST addresses (a union pun of `void (*)(daKrb_c *)` as a
           pointer to member), which the seat above leaves alone because they are not DS
           addresses. Put the thiscall adapter over each one. */
        if (g_goomba_states[i].raw && data_ov084_02130d74[i].fn == (unsigned)(size_t)g_goomba_states[i].raw)
            data_ov084_02130d74[i].fn = (unsigned)(size_t)g_goomba_states[i].host;
        if (g_goomba_states[i].raw && g_goomba_states[i].slot->fn == (unsigned)(size_t)g_goomba_states[i].raw)
            g_goomba_states[i].slot->fn = (unsigned)(size_t)g_goomba_states[i].host;
    }
}
