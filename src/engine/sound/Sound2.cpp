//cpp
/* arm9/Sound2 -- eight tail-call thunks, .text 0x02048eb4..0x02048f34.
 * Each one loads func_02048fd4 and branches with r0 = 0, 7, 6, 5, 4, 3, 2, 1
 * in ROM order. func_02048fd4 is the next body and is not in this file.
 * func_02048e94 is the previous body and stays its own shard.
 * The three Func_ wrappers are Sound:: entry points the compiler mangles
 * itself. mwccarm emits .text in reverse source order, so the
 * definitions below run ROM-descending. The Sound thunks are one file
 * through 0x02048F34.
 */

#include "types.h"

extern "C" {
extern void func_02048fd4(int i);
}

// @symbol func_02048f24
extern "C" void func_02048f24(void)
{
    func_02048fd4(1);
}

// @symbol func_02048f14
extern "C" void func_02048f14(void)
{
    func_02048fd4(2);
}

// @symbol func_02048f04
extern "C" void func_02048f04(void)
{
    func_02048fd4(3);
}

// @symbol func_02048ef4
extern "C" void func_02048ef4(void)
{
    func_02048fd4(4);
}

namespace Sound {

// @symbol _ZN5Sound13Func_02048ee4Ev
void Func_02048ee4()
{
    func_02048fd4(5);
}

}

// @symbol func_02048ed4
extern "C" void func_02048ed4(void)
{
    func_02048fd4(6);
}

namespace Sound {

// @symbol _ZN5Sound13Func_02048ec4Ev
void Func_02048ec4()
{
    func_02048fd4(7);
}

// @symbol _ZN5Sound13Func_02048eb4Ev
void Func_02048eb4()
{
    func_02048fd4(0);
}

}
