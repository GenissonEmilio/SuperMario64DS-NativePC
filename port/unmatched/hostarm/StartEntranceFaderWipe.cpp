/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/engine/fader/StartEntranceFaderWipe.cpp (port line). The arm
 * gave the file's local fader classes two destructor slots so their virtuals
 * sit at the ROM table's offsets; the 2026-10 main merge (749f5799a) took
 * main's src/, which keeps the file (classes renamed Fader -> dFader_c,
 * FaderWipe -> dFdWipe_c, SetFaders now the dScene_c static member) without the
 * arm. This file defines StartEntranceFaderWipe again: MAIN'S BODY and class
 * spellings with only the arm's slot change applied. The same @symbol body must
 * be parked in the TU's hostgen copy. mwccarm never sees this file, so nothing
 * here moves a ROM byte. */
#include "dScene_c.h"
#include "types.h"

struct dFader_c {
    Fix12i currInterp;
    Fix12i speed;
    /* THE DESTRUCTOR TAKES TWO SLOTS, AS IT DOES IN THE ROM TABLE. mwcc gives a
       virtual destructor two entries (D1, D0), MSVC folds them into one, so
       every virtual below sat one slot early on the PC: SetToStart reached
       0x20 (SetToEnd) and SetToEnd reached 0x1c. The wipe objects' tables
       are ROM-ordered (0x20 SetToEnd, 0x24 SetToStart), so two placeholder
       slots stand where D1 and D0 stand. */
    virtual void Destructor1();
    virtual void Destructor0();
    virtual int Advance();
    virtual int SetBackwardTime(unsigned frames);
    virtual int SetForwardTime(unsigned frames);
    virtual int IsAtStart();
    virtual int IsAtEnd();
    virtual void Virtual1C();
    virtual void SetToEnd();
    virtual void SetToStart();
};

/* dFdWipe_c: dFader_c(0x0c) + color(2)+unk0e(2) + model(0x50) = 0x60 */
struct dFdWipe_c : dFader_c {
    u16 color;
    u16 unk0e;
    u32 model[0x50/4];
    virtual void Destructor1();
    virtual void Destructor0();
    virtual int Advance();
    virtual int SetBackwardTime(unsigned frames);
    virtual int SetForwardTime(unsigned frames);
    virtual int IsAtStart();
    virtual int IsAtEnd();
    virtual void Virtual1C();
    virtual void SetToEnd();
    virtual void SetToStart();
};

/* The fader wipe array (seven objects of 0x60, filled by
   dScStage_c::InitResources). Declared extern "C" here, unlike main's TU:
   with C++ linkage MSVC decorates it with the class name, and the port's
   alias row (hal/cxx_aliases.cpp) names the old FaderWipe spelling, so the
   renamed class would leave the decorated name unresolved. */
extern "C" dFdWipe_c* data_0209f324;

// @symbol StartEntranceFaderWipe
extern "C" void StartEntranceFaderWipe(int index) {
    dFdWipe_c* f = &data_0209f324[index];
    dScene_c::SetFaders((dFdBrightness_c *)f);
    f->SetToEnd();
}
