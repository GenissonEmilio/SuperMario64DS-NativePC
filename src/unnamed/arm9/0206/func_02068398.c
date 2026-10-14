#include "types.h"

/* func_02068398 @ 0x02068398 size 0x78 (arm9). Picks the object the
 * 0x020a9d2c controller works on: the one its current object points to at
 * +0x4b4 when there is one, otherwise the default at +0, then latches its
 * +0x4ac byte and moves the controller to state 2. Returns 0 with no default.
 *
 * Matched 2004/b56. The fallback is written as the `||` arm of an if/else:
 * both failed tests then reach `res = sel`, a two-predecessor block that
 * mwccarm will not predicate, and once `res` and `sel` share r1 that copy
 * is gone, leaving the ROM's `beq` over a lone `mov r1, r0`. A plain
 * `if (q != 0) sel = q;` predicates into `movne` instead (notes 6u). */
struct G9d2c {
    u8 *fallback;   /* 0x0 */
    u8 *current;    /* 0x4 */
    char pad8[4];
    u8 state;       /* 0xc */
    char padd;
    u8 mode;        /* 0xe */
};
extern struct G9d2c data_020a9d2c;

extern void func_02068484(void);

int func_02068398(void)
{
    u8 *sel = data_020a9d2c.fallback;
    u8 *p;
    u8 *res;

    if (sel == 0)
        return 0;

    p = data_020a9d2c.current;
    if (p == 0 || (p = *(u8 **)(p + 0x4b4)) == 0)
        res = sel;
    else
        res = p;
    data_020a9d2c.current = res;

    func_02068484();
    data_020a9d2c.mode = data_020a9d2c.current[0x4ac];
    data_020a9d2c.state = 2;
    return 1;
}
