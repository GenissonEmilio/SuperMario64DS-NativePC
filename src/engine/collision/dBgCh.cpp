//cpp
/* dBgCh -- the collision-query root (ROM RTTI _ZTS5dBgCh). Owns the
 * contiguous .text run 0x02035448..0x0203553c, which ends where
 * dBgCh_Actr.cpp begins. The lifecycle group is defined first: D2, D0, D1,
 * then C2 at the tail (C1 has no ROM counterpart and is deadstripped).
 */
// leftovers: the four func_020354xx flag helpers stay extern "C" free
// functions taking the object by pointer (renaming them is a symbols.txt
// change, per include/dBgCh.h). unk_008/unk_00c keep header names (no
// caller evidence renames them).

#include "dBgCh.h"

// @symbol _ZN5dBgChC2Ev
dBgCh::dBgCh()
{
    mDetectFlags = 1;
    unk_00c = 0;
    unk_008 = -1;
}

// @symbol _ZN5dBgChD2Ev
// @symbol _ZN5dBgChD0Ev
// @symbol _ZN5dBgChD1Ev
dBgCh::~dBgCh()
{
}

// @symbol _ZN5dBgCh21StopDetectingOrdinaryEv
void dBgCh::StopDetectingOrdinary()
{
    mDetectFlags &= ~1;
}

// @symbol func_020354b0
extern "C" int func_020354b0(unsigned char *p)
{
    return p[4] & 1;
}

// @symbol _ZN5dBgCh19StartDetectingWaterEv
void dBgCh::StartDetectingWater()
{
    mDetectFlags |= 2;
}

// @symbol _ZN5dBgCh18StopDetectingWaterEv
void dBgCh::StopDetectingWater()
{
    mDetectFlags &= ~2;
}

// @symbol func_0203547c
extern "C" int func_0203547c(unsigned char *p)
{
    return p[4] & 2;
}

// @symbol func_02035468
extern "C" void func_02035468(char *self)
{
    *(unsigned char *)(self + 4) |= 4;
}

// @symbol func_0203545c
extern "C" int func_0203545c(unsigned char *p)
{
    return p[4] & 4;
}

// @symbol _ZN5dBgCh19StartDetectingToxicEv
void dBgCh::StartDetectingToxic()
{
    mDetectFlags |= 8;
}
