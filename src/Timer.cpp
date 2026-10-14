//cpp
/* leftovers:
 * #pragma defer_codegen off stays. It emits .text in source order, which is
 * the ROM order 0x02019584..0x02019664. func_0201964c stays a free function;
 * Timer::Timer() would rename the symbol. func_02059650 is the tick read
 * (timer 0 at 0x4000100 plus the counter at data_020a6438). Its declaration
 * is only in decl_common.h.
 */
/* Timer: arm9 stopwatch. mTime is the start tick while running and the
 * frozen elapsed count while stopped.
 */
#include "Timer.h"
#include "decl_common.h"

#pragma defer_codegen off

// @symbol _ZN5Timer7GetTimeEv
s64 Timer::GetTime()
{
    if (!mIsRunning)
        return mTime;
    return func_02059650() - mTime;
}

// @symbol _ZN5Timer9StopTimerEv
void Timer::StopTimer()
{
    if (!mIsRunning)
        return;
    mIsRunning = 0;
    mTime = func_02059650() - mTime;
}

// @symbol _ZN5Timer10StartTimerEv
void Timer::StartTimer()
{
    mIsRunning = 1;
    mTime = func_02059650() - mTime;
}

// @symbol _ZN5Timer10ResetTimerEv
void Timer::ResetTimer()
{
    mIsRunning = 0;
    mTime = 0;
}

/* The C1 constructor (ResetTimer, then return this) stays an extern "C"
   helper under its address name; the Timer::Timer() spelling would rename
   the symbol. */
// @symbol func_0201964c
extern "C" Timer *func_0201964c(Timer *__this)
{
    __this->ResetTimer();
    return __this;
}
