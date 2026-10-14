/* PORT-UNMATCHED walk-physics core (func_ov002_020d3b9c, 0x5a0 bytes).
 * Adopted from the banked near-miss draft (nearmiss/db.jsonl,
 * source lunavyqo-b56-perm6975, 52 codegen divergences vs ROM). The
 * divergences are register/schedule-level; the walk semantics are what
 * the port needs. Retires when the function matches for real. */

typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
extern int Player_ScaleByCharFactor(void *c, int a);
extern int func_ov002_020bf224(void *c, int a, int b);
extern int func_ov002_020bf56c(void *c, int b);
extern void _ZN6Player19func_ov002_020bf88cEv(void *c);
extern int _ZN6Player19func_ov002_020c031cEv(void *c);
extern int func_ov002_020f035c(unsigned int sel, int r1);
extern int AngleDiff(int a, int b);
extern int func_01ffabe4(int a, int b);
extern void ApproachAngle(short *cur, short target, int divisor, int band, int maxStep);
extern void _ZN6Player11ChangeStateERNS_5StateE(void *c, void *s);
extern void Player_AdvanceAnims(void *c);
extern void func_ov002_020d4540(void *c);
extern void func_ov002_020d454c(void *c);
extern int func_ov002_020d4d88(void *c, int a, int b);
extern void func_ov002_020c18b0(void *c, int a);
extern unsigned char data_020a0e40;
extern short data_0209f4a0[];
extern char data_0209f49c[];
extern char data_0209f4ac[];
extern char data_0209f4ae[];
extern int data_ov002_02110184[];
int func_ov002_020d3b9c(char *c)
{
  char *self;
  int acc;
  int spd;
  int idx;
  s16 stick;
  s16 targ;
  int diff;
  acc = 0;
  idx = data_020a0e40 * 0x18;
  stick = *((s16 *) (((char *) data_0209f4a0) + idx));
  self = c;
  spd = acc;
  if ((stick != 0) && (((u16) ((*((u16 *) (self + 0x6ce))) & 0xc)) == 0))
  {
    *((u16 *) (self + 0x6ac)) = 4;
    if ((*((u8 *) (data_0209f4ac + idx))) == 0)
    {
      spd = Player_ScaleByCharFactor(self, 0x14000);
      acc = 0x1000;
      if (((*((u16 *) (data_0209f49c + (data_020a0e40 * 0x18)))) & 0x800) == 0)
      {
        if ((*((u16 *) (self + 0x6a4))) != 0)
        {
          spd = Player_ScaleByCharFactor(self, 0xa000);
        }
      }
      else
      {
        spd = Player_ScaleByCharFactor(self, 0x28000);
      }
    }
    else
    {
      /* the ROM is `ldrb r3, [r0, r1]` at 020D3C70 with r0 = &data_0209f4ae
         and r1 = idx: the BYTE at that offset. The draft took the byte and
         then dereferenced it as a pointer, which reads address 0..255. It
         has never fired because the port leaves data_0209f4ac at 0 and only
         the stylus control mode reaches here, but it is a fault waiting for
         the first touch-mode frame. */
      u8 mode = (u8) data_0209f4ae[idx];
      s16 stick2 = *((s16 *) (((char *) data_0209f4a0) + idx));
      if (stick2 <= ((mode != 2) ? (0x471) : (0x555)))
      {
        spd = 0;
      }
      else
        if (stick2 <= ((mode != 2) ? (0x955) : (0xAAA)))
      {
        int t = stick2 * ((mode != 2) ? (0x24) : (0x30));
        int s = (mode != 2) ? (0xa) : (0x10);
        t = (t >> 12) - s;
        s = (mode != 2) ? (0xa) : (0x10);
        {
          int lim = (mode != 2) ? (0x15) : (0x20);
          spd = t << 15;
          spd = func_01ffabe4(spd, lim - s);
        }
      }
      else
      {
        int a = Player_ScaleByCharFactor(self, 0x28000);
        int b = Player_ScaleByCharFactor(self, 0xa000);
        spd = func_ov002_020bf224(self, a, b);
      }
      acc = 0x1000;
      if ((*((s16 *) (((char *) data_0209f4a0) + (data_020a0e40 * 0x18)))) >= 0xf00)
      {
        acc = 0x1000;
      }
    }
    if ((*((u8 *) (self + 0x6ed))) != 0)
    {
      if (((*((u16 *) (data_0209f49c + (data_020a0e40 * 0x18)))) & 0x800) == 0)
      {
        *((u8 *) (self + 0x6ed)) = 0;
      }
      else
      {
        u8 *p = (u8 *) ((void *) ((int) (((long long) ((int) (((char *) self) + 0x6ED))) & 0xFFFFFFFFFFFFFFFFLL)));
        /* 0x1333 is the ROM's literal at 020D412C, loaded by the ldr at
           020D3D7C: the dash boost is target * 1.2. The draft had
           0x88888889 here, which is the DIVIDE-BY-30 magic from the next
           pool slot at 020D4134 (the h/30 cap further down this function),
           picked up out of the wrong literal. */
        spd = (int) (((((long long) spd) * 0x1333LL) + 0x800) >> 12);
        *p = (u8) ((*p) - 1);
        acc = 0x1e000;
        _ZN6Player19func_ov002_020bf88cEv(self);
      }
    }
    {
      int v = *((int *) (self + 0x68c));
      if (v > 0xa000)
      {
        int q = func_01ffabe4(0x6400, v >> 12);
        spd = (int) (((((long long) spd) * q) + 0x800) >> 12);
      }
    }
    *((int *) (self + 0x690)) = 0;
    if ((*((int *) (self + 8))) == 1)
    {
      u16 h = *((u16 *) (self + 0x6ba));
      if (((h != 0) && ((*((u8 *) (self + 0x6ea))) != 0)) && (h < 0x1e))
      {
        int a = Player_ScaleByCharFactor(self, 0x28000);
        int prod = h * a;
        {
          int cap = prod / 30;
          if (spd > cap)
          {
            spd = cap;
          }
        }
        {
          int tmp = -((0x1e - ((int) h)) << 12);
          tmp = tmp * 2;
          *((int *) (self + 0x690)) = tmp;
        }
      }
    }
    if (func_ov002_020f035c(_ZN6Player19func_ov002_020c031cEv(self), *((int *) (self + 0x558))) != 0)
    {
      acc = 0x1000;
    }
    targ = *((s16 *) (self + 0x6d2));
    diff = AngleDiff(*((s16 *) (self + 0x94)), targ);
    if (diff < 0)
    {
      diff = -diff;
    }
    if (diff > 0x6000)
    {
      int cur = *((int *) (self + 0x98));
      if ((cur >= Player_ScaleByCharFactor(self, 0x1c000)) && ((*((u8 *) (self + 0x703))) == 0))
      {
        _ZN6Player11ChangeStateERNS_5StateE(self, data_ov002_02110184);
        Player_AdvanceAnims(self);
        return 1;
      }
      if (cur == 0)
      {
        *((s16 *) (self + 0x94)) = targ;
      }
      else
      {
        ApproachAngle((s16 *) (self + 0x94), targ, 4, 0x2000, 0x800);
      }
    }
    else
    {
      int dv;
      int step;
      int idx2;
      step = 0x800;
      idx2 = data_020a0e40 * 0x18;
      dv = 4;
      if ((*((u8 *) (data_0209f4ac + idx2))) == 0)
      {
        if ((*((s16 *) (((char *) data_0209f4a0) + idx2))) >= 0xa00)
        {
          if (AngleDiff(*((s16 *) (self + 0x94)), targ) >= 0x1000)
          {
            if ((*((u16 *) (self + 0x6f0))) < 0x18)
            {
              u16 *p = (u16 *) ((void *) ((int) (((long long) ((int) (((char *) self) + 0x6F0))) & 0xFFFFFFFFFFFFFFFFLL)));
              *p = (u16) ((*p) + 1);
              dv = (s16) (0x10 - ((*((u16 *) (self + 0x6f0))) >> 1));
            }
          }
          else
          {
            dv = 0x10;
            if ((*((u16 *) (self + 0x6f0))) >= 2)
            {
              u16 *p = (u16 *) ((void *) ((int) (((long long) ((int) (((char *) self) + 0x6F0))) & 0xFFFFFFFFFFFFFFFFLL)));
              *p = (u16) ((*p) - 2);
            }
          }
          step = (s16) ((0x10 - (dv - 4)) << 3);
        }
        else
          if ((*((u16 *) (self + 0x6f0))) >= 2)
        {
          u16 *p = (u16 *) ((void *) ((int) (((long long) ((int) (((char *) self) + 0x6F0))) & 0xFFFFFFFFFFFFFFFFLL)));
          *p = (u16) ((*p) - 2);
        }
      }
      ApproachAngle((s16 *) (self + 0x94), targ, dv, 0x2000, step);
    }
    if ((*((u8 *) (self + 0x6e0))) != 0)
    {
      func_ov002_020d4540(self);
    }
  }
  else
  {
    *((u8 *) (self + 0x6ed)) = 0;
    if ((*((u8 *) (self + 0x6e0))) != 0)
    {
      acc = func_ov002_020bf56c(self, 0x2000);
      _ZN6Player19func_ov002_020bf88cEv(self);
    }
    else
      if ((*((u16 *) (self + 0x6ac))) == 0)
    {
      acc = ((*((u8 *) (self + 0x703))) == 0) ? (0x1000) : (0x4000);
      if ((*((int *) (self + 0x98))) < (-0xa000))
      {
        *((int *) (self + 0x98)) = -0xa000;
      }
      if ((*((u8 *) (self + 0x703))) == 0)
      {
        if ((*((int *) (self + 0x98))) > Player_ScaleByCharFactor(self, 0x10000))
        {
          func_ov002_020d454c(self);
        }
      }
    }
    if ((*((int *) (self + 0x98))) < Player_ScaleByCharFactor(self, 0xa000))
    {
      *((u16 *) (self + 0x6a4)) = 0x3c;
    }
  }
  func_ov002_020d4d88(self, spd, acc);
  func_ov002_020c18b0(self, 1);
  *((u16 *) (self + 0x90)) = 0;
  return 0;
}
