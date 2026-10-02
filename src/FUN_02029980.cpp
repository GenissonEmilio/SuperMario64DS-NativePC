//cpp
struct A {
  virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3();
  virtual void v4(); virtual void v5(); virtual void v6(); virtual int v7();
};
/* The wipe at data_0209f324 + 0x240 (wipe 6). w3 is its slot 0x0c,
   SetBackwardTime, which this call hands two words (0x1e, 0).

   THE _MSC_VER ARM IS THE HOST'S CALLING CONVENTION FOR THAT ONE SLOT, for the
   reason src/FUN_020299f4.cpp gives: HalFaderWipe's slots 0x0c and 0x10 are
   __cdecl with the receiver on the stack, and a plain virtual call here would
   leave eight bytes on this frame and return into them. Player::St_Respawn_Main
   calls this for the local player, so it is the respawn after your own pit
   death in a VS match. The #else arm is the matched source, unchanged. */
struct B {
  virtual void w0(); virtual void w1(); virtual void w2();
#ifdef _MSC_VER
  virtual void __cdecl w3(int,int); virtual void w4(); virtual void w5();
#else
  virtual void w3(int,int); virtual void w4(); virtual void w5();
#endif
  virtual void w6(); virtual void w7(); virtual void w8();
};
extern "C" { extern A* data_0209f5bc; extern char* data_0209f324; extern B* data_0209d4b0; }
extern "C" void FUN_02029980(void){
  if(data_0209f5bc->v7() != 0) return;
  B* o2 = (B*)(data_0209f324 + 0x240);
  data_0209d4b0 = o2;
  o2->w8();
  o2->w3(0x1e, 0);
}
