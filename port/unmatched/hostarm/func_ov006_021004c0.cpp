/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/func_ov006_021004c0.c (port line). Its _MSC_VER arm handed
 * func_ov004_020adc1c's result to func_ov004_020b19f0; the 2026-10 main merge
 * (749f5799a) took main's src/, which made the function the member
 * dScMgPachinko2_c::func_ov006_021004c0 in src/actors/dScMgPachinko2_c.cpp and
 * dropped the arm. This file defines the member again: MAIN'S BODY, statement
 * for statement, with only the arm's change applied. The same @symbol body
 * must be parked in the TU's hostgen copy. mwccarm never sees this file, so
 * nothing here moves a ROM byte. */
#include "types.h"
#include "dScMgPachinko2_c.h"

/* func_ov006_021004c0's view: the state word at 0x5660. */
struct Obj4c0 { char pad[0x5660]; int f; /* 0x5660 */ };

extern "C" {
extern int  func_ov004_020adc1c(void);
/* func_ov004_020b19f0 draws the number it is given. On the cartridge
   func_ov004_020adc1c's result is still in r0 at the bl that follows
   (ov006 0x021004e0 / 0x021004e4), so the PC build passes it. */
extern int  func_ov004_020b19f0(int value);
}

// @symbol _ZN16dScMgPachinko2_c19func_ov006_021004c0Ev
void dScMgPachinko2_c::func_ov006_021004c0()
{
    struct Obj4c0 *o = (struct Obj4c0 *)this;
    if (o->f < 2)
        return;
    func_ov004_020b19f0(func_ov004_020adc1c());
}
