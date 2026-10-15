/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/minigames/d_s_mg_roulette_methods.cpp (port line). Its _MSC_VER arm passed
 * the payout to MgCoins_StartPayout from OnTurnIntoEgg case 7; the 2026-10 main
 * merge (749f5799a) took main's src/, which keeps the file (now
 * dScMgRoulette_c::OnTurnIntoEgg with named members) but calls the callee with
 * no argument and dropped the arm. This file defines the member again: MAIN'S
 * BODY, statement for statement, with only the arm's change applied. The same
 * @symbol body must be parked in the TU's hostgen copy. mwccarm never sees this
 * file, so nothing here moves a ROM byte. */
#include "common.h"
#include "types.h"
#include "dScMgRoulette_c.h"
#include "Model.h"
#include "decl_common.h"
#include "PlayerInput.h"

extern "C" {
void FreeGfxSlotsById(int a);
int  Dealer_IsIdle(char *c);
int  RouletteResults_Tick(void);
extern u16 gMgCoinsStatus;
/* THE PAYOUT IS PASSED, AS THE ROM PASSES IT. MgCoins_StartPayout stores its
   argument as the coin shower's count. The ROM calls it from OnTurnIntoEgg
   case 7 (0x021097a8) straight after ldrsh r0,[r1,#0xf2], the payout sum at
   +0x53f2 (mScore), still in r0, so the matched source calls it with no
   argument. On x86 nothing was pushed and the callee stored the stack word
   above its return address instead of the payout. */
void MgCoins_StartPayout(int coins);
}

// @symbol _ZN15dScMgRoulette_c11CanEndStateEi
/* Runs the end of the round, phases 5 to 9. Returns 1 when it is done. */
int dScMgRoulette_c::CanEndState(int /* mode */)
{

    switch (mPhase) {
    case 5:
        FreeGfxSlotsById(0x1d);
        if (RouletteResults_Tick() != 0)
            (mPhase)++;
        break;
    case 6:
        if (Dealer_IsIdle((char *)mTable) != 0) {
            mPhaseTimer = 0x3c;
            mDealIndex = 0;
            (mPhase)++;
        }
        break;
    case 7:
        (mPhaseTimer)--;
        if (mPhaseTimer == 0) {
            if (mScore != 0)
                MgCoins_StartPayout(mScore);
            (mPhase)++;
        }
        break;
    case 8:
        (mPhase)++;
        /* fall through */
    case 9:
    default:
        if (gMgCoinsStatus <= 1)
            return 1;
        break;
    }
    return 0;
}
