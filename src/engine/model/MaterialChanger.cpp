//cpp
/* MaterialChanger -- Animation child driving BMA-file playback. Cartridge
   class: _ZTV15MaterialChanger at 0x0208e7f4 (two slots, the destructor
   pair); the cartridge RTTI names it dExtAnmMaterial_c. TU claims
   0x0201577c..0x0201587c, the whole class run in delinks order. Written
   back-to-front: mwccarm emits .text in reverse source order under default
   deferred codegen.

   leftovers:
   - SetFile and its SetAnimation call keep mangled spellings (Fix12<int>
     by value, wall 6az); the member declarations in the headers are real.
   - func_02046e28 and func_020470e8 stay extern "C" helpers with no
     declaring header; Prepare's ROM body is a 0xc long-call veneer, so it
     calls func_020470e8 with that function's own two-argument signature.
   - RTTI stays off: the cartridge names the class dExtAnmMaterial_c, so
     the emitted _ZTS/_ZTI records would have no configured home. */
#include "MaterialChanger.h"

/* The cartridge spells this class dExtAnmMaterial_c and its base
   dExtFrameCtrl_c, so the compiler-emitted _ZTS15MaterialChanger /
   _ZTI15MaterialChanger / _ZTS9Animation / _ZTI9Animation records would be
   homeless under either spelling. Compiling with RTTI off emits no
   records at all; the vtable preamble's typeinfo word deadstrips with the
   rest of the data sections. */
#pragma RTTI off

extern "C" void func_020470e8(BMD_File *model, BMA_File *file);
extern "C" void func_02046e28(ModelComponents *model, BMA_File *file, int frame);
/* SetFile below and its SetAnimation call keep their mangled spellings:
   wall 6az (notes/mwccarm-codegen.md) homes class-typed by-value
   parameters that a body reads, and the real signature carries
   Fix12<int> -- passing one to the member declaration homes it to the
   caller's stack. The declarations in MaterialChanger.h and dExtFrameCtrl_c.h
   are the real ones. */
extern "C" void _ZN15dExtFrameCtrl_c12SetAnimationEti5Fix12IiEt(dExtFrameCtrl_c *self, u16 numFrames, s32 flags, s32 speed, u16 startFrame);

// @symbol _ZN15MaterialChangerC1Ev
MaterialChanger::MaterialChanger()
{
    file = 0;
}

// @symbol _ZN15MaterialChangerD1Ev
MaterialChanger::~MaterialChanger()
{
}

// @symbol _ZN15MaterialChanger7SetFileER8BMA_Filei5Fix12IiEj
extern "C" void _ZN15MaterialChanger7SetFileER8BMA_Filei5Fix12IiEj(MaterialChanger *self, BMA_File *file, s32 flags, s32 speed, u16 startFrame)
{
    if (file == self->file) {
        self->SetFlags(flags);
        self->speed = speed;
    } else {
        self->file = file;
        _ZN15dExtFrameCtrl_c12SetAnimationEti5Fix12IiEt(self, file->numFrames, flags, speed, startFrame);
    }
}

// @symbol _ZN15MaterialChanger6UpdateER15ModelComponents
void MaterialChanger::Update(ModelComponents &model)
{
    func_02046e28(&model, file, (u16)((u32)currFrame >> 12));
}

/* The ROM body is a 0xc long-call veneer (ldr ip, [pc]; bx ip); the
   registers pass through untouched, so the matched func_020470e8.c's
   own two-argument signature is the call surface. Prepare is static:
   no this. */
// @symbol _ZN15MaterialChanger7PrepareER8BMD_FileR8BMA_File
void MaterialChanger::Prepare(BMD_File &model, BMA_File &animFile)
{
    func_020470e8(&model, &animFile);
}
