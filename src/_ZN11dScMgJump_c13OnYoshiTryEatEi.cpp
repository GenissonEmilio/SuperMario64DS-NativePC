//cpp
// @symbol _ZN11dScMgJump_c10StartRoundEi
/* recovered: dScMgJump_c::OnYoshiTryEat, Bounce and Pounce's Yoshi-eat handler (vtable slot 18). */
// NONMATCHING: size differs: target 0x168 vs candidate 0x164 (the draft assembles to a different size, so no word count applies).
// @ 0x020ee994 size 0x168. Residue class: size mismatch: the draft is 4 bytes short (Andrew's #2702 records the one-word floor).
// Draft from nearmiss/db.jsonl (stored divergence 999), landed 2026-09-14 under Tango's ruling that the functionally-equivalent C
// drafts live on main with an honest banner so the port and readers have source. Logic is
// verified only as far as the residue class implies: register names and instruction order
// for the shape-exact rows, NOT an independent execution audit for the others. Counts as
// decompiled, not matched; tools/enroll.py leaves it out of the ROM build, which keeps the
// original bytes for this range. A byte-exact match replaces this file and drops the banner.
#include "decl_common.h"
#include "dScMgJump_c.h"

extern "C" int data_0209e650;
extern "C" int data_ov006_0213cb48;
extern "C" int RandomIntInternal(int *seed);
extern "C" void Jump3D_EnableInput(void);
extern "C" void BeginIntro(char *c);

void dScMgJump_c::OnYoshiTryEat(int arg)
{
    u32 v;

    if (arg == 0) {
        this->unk_0bc = this->unk_0bc + 1;
        if (this->unk_0bc > 0x270e) this->unk_0bc = 0x270e;
    } else if (arg == 0x12) {
        this->unk_0bc = 0;
        if (this->unk_0bc > 0x270e) this->unk_0bc = 0x270e;
        if (gMgScene != 0)
            *(int *)((char *)gMgScene + 0xb4) = 0;
        Mg_SubmitScore(this->mHudScore);
    } else {
        u32 t = this->unk_0bc;
        if (gMgScene != 0)
            *(int *)((char *)gMgScene + 0xb4) = t;
    }

    if (this->unk_0bc != 0)
        Jump3D_DisableInput();
    else
        Jump3D_EnableInput();

    v = this->unk_0bc;
    if (v >= 0xf) {
        do {
            v = (int)((u32)(RandomIntInternal(&data_0209e650) & 0x7fffffff) >> 0x13) * 0xa >> 0xc;
        } while (v == (u32)data_ov006_0213cb48);
        data_ov006_0213cb48 = v;
    }

    Jump3D_Arrange(this->unk_0bc, v);

    if (this->unk_0bc < 0xf)
        data_ov006_02140328 = 3;
    else if (this->unk_0bc < 0x14)
        data_ov006_02140328 = 4;
    else if (this->unk_0bc < 0x19)
        data_ov006_02140328 = 5;
    else
        data_ov006_02140328 = 6;

    Heyho_SpawnAll(this->unk_0bc, v);
    BeginIntro((char *)this);
}
