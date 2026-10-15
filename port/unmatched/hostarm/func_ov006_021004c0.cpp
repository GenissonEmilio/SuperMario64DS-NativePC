/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/DrawTopRecord.c (port line). Its _MSC_VER arm handed
 * Mg_GetTopRecord's result to Mg_DrawTopLeftCounter; the 2026-10 main merge
 * (749f5799a) took main's src/, which made the function the member
 * dScMgPachinko2_c::DrawTopRecord in src/minigames/d_s_mg_pachinko2.cpp and
 * dropped the arm. This file defines the member again: MAIN'S BODY, statement
 * for statement, with only the arm's change applied. The same @symbol body
 * must be parked in the TU's hostgen copy. mwccarm never sees this file, so
 * nothing here moves a ROM byte. */
#include "types.h"
#include "dScMgPachinko2_c.h"

/* DrawTopRecord's view: the state word at 0x5660. */
struct Obj4c0 { char pad[0x5660]; int f; /* 0x5660 */ };

extern "C" {
extern int  Mg_GetTopRecord(void);
/* Mg_DrawTopLeftCounter draws the number it is given. On the cartridge
   Mg_GetTopRecord's result is still in r0 at the bl that follows
   (ov006 0x021004e0 / 0x021004e4), so the PC build passes it. */
extern int  Mg_DrawTopLeftCounter(int value);
}

// @symbol _ZN16dScMgPachinko2_c13DrawTopRecordEv
void dScMgPachinko2_c::DrawTopRecord()
{
    struct Obj4c0 *o = (struct Obj4c0 *)this;
    if (o->f < 2)
        return;
    Mg_DrawTopLeftCounter(Mg_GetTopRecord());
}
