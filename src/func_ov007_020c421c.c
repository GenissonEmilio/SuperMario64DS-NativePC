
extern void func_ov007_020c42f8(char *self);
extern void func_ov007_020c4684(void *a, int b, int c, int d, int e, void *f);
#ifdef _MSC_VER
/* PORT ONLY (mwccarm never sees this arm): the one-screen title menu draws the title's sprite
   objects selectively. hal/menu_recipe_title.cpp sets the hook; null draws everything. */
int (*g_port_title_draw_skip)(char *obj);
/* and the matching end of a drawn object's submit (FILE2: the wide menu background tells sprite objects from the page model) */
void (*g_port_title_draw_end)(void);
#endif
struct Vec3s
{
  short x;
  short y;
  short z;
};
void func_ov007_020c421c(char *r4)
{
  struct Vec3s v;
  int diff;
  int t78;
  int t74;
#ifdef _MSC_VER
  if (g_port_title_draw_skip && g_port_title_draw_skip(r4))
  {
    return;
  }
#endif
  v.z = 0;
  v.y = v.z;
  v.x = v.y;
  *((volatile int *) 0x4000444) = 0;
  func_ov007_020c42f8(r4);
  func_ov007_020c4684(r4, *((unsigned short *) (r4 + 0x44)), ((*((int *) (r4 + 0x50))) >> 12) & 0xff, *((unsigned char *) (r4 + 0x4e)), *((int *) r4), &v);
  if ((*((int *) r4)) & 4)
  {
    t78 = *((int *) (r4 + 0x78));
    t74 = *((int *) (r4 + 0x74));
    diff = ((*((int *) (r4 + 0x50))) >> 12) - (*((unsigned char *) (r4 + 0x7c)));
    if (diff < 0)
    {
      diff = 0;
    }
    *((volatile int *) 0x400046c) = t74;
    *((volatile int *) 0x400046c) = t78;
    *((volatile int *) 0x400046c) = 0x1000;
    func_ov007_020c4684(r4, 0, diff & 0xff, *((unsigned char *) (r4 + 0x7d)), 0, r4 + 0x7e);
  }
  *((volatile int *) 0x4000448) = 1;
#ifdef _MSC_VER
  if (g_port_title_draw_end)
  {
    g_port_title_draw_end();
  }
#endif
}
