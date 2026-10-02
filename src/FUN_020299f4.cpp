//cpp
struct A {
  virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3();
  virtual void v4(); virtual void v5(); virtual void v6(); virtual int v7();
};
/* The wipe at data_0209f324 + 0x240 (wipe 6). w4 is its slot 0x10,
   SetForwardTime, which this call hands two words (0x1e, 0) as
   dScene_c::BeforeBehavior does.

   THE _MSC_VER ARM IS THE HOST'S CALLING CONVENTION FOR THAT ONE SLOT. On the
   PC the wipes are port/hal/fader_wipes.cpp's HalFaderWipe, whose slots 0x0c
   and 0x10 are __cdecl with the receiver as the first stack word, the frame
   BeforeBehavior's function-pointer call builds. A plain virtual compiles
   __thiscall here: the receiver in ecx, two words pushed and the CALLEE
   expected to take them back. The __cdecl slot then reads 0x1e as its
   receiver, refuses it ("WRONG RECEIVER at SetForwardTime"), returns without
   taking the eight bytes, and this function returns into the pushed 0. That
   is the crash on your own pit death in a VS match (Player::St_DeadPit_Init
   calls this only for the local player). Declared __cdecl, the call pushes
   the receiver itself and cleans up after, which is the slot's own frame.
   The #else arm is the matched source, unchanged. */
struct B {
  virtual void w0(); virtual void w1(); virtual void w2();
#ifdef _MSC_VER
  virtual void w3(); virtual void __cdecl w4(int,int); virtual void w5();
#else
  virtual void w3(); virtual void w4(int,int); virtual void w5();
#endif
  virtual void w6(); virtual void w7(); virtual void w8(); virtual void w9();
};
extern "C" { extern A* data_0209f5bc; extern char* data_0209f324; extern B* data_0209d4b0; }
extern "C" void FUN_020299f4(void){
  if(data_0209f5bc->v7() != 0) return;
  B* o2 = (B*)(data_0209f324 + 0x240);
  data_0209d4b0 = o2;
  o2->w9();
  o2->w4(0x1e, 0);
}
