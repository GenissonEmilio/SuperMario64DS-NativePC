//cpp
// @symbol _ZN6Bowser6RenderEv
/* recovered: named members + shared header, real C++ method */
#include "Bowser.h"
struct Obj { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void m(void*); };
extern "C" {
extern void _ZN9Animation7AdvanceEv(void* a);
extern void _ZN15TextureSequence6UpdateER15ModelComponents(void* a, void* b);
#ifdef _MSC_VER
extern unsigned char data_0209f2c4;   /* pause-screen state; 0 = playing */
#endif
}

int Bowser::Render()
{
  if(mOpacity < 8) return 1;
#ifdef _MSC_VER
  /* Deliberate port choice (Tango, 2026-10-08), NOT cartridge behaviour: the DS
     runs this Advance on every Render, pause included, so his blink keeps
     going under the course pause menu (invisible at DS resolution, visible
     in the port's higher resolution). The port holds the blink frame while the
     pause screen is up; it resumes from the same frame on unpause. */
  if(data_0209f2c4 == 0)
#endif
  _ZN9Animation7AdvanceEv((char*)&mTextureSequence);
  _ZN15TextureSequence6UpdateER15ModelComponents(((char*)this)+0x138, ((char*)this)+0xdc);
  ((Obj*)((char*)&mModelAnim))->m((char*)&mScaleX);
  return 1;
}
