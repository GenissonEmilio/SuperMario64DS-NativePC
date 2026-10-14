//cpp
/* ov006 sprite pool, 0x020c8a30..0x020c8dd4.
 *
 * Three 0x20-byte records at data_ov006_021404d8. Jump, Jump2,
 * Trampoline and Trampoline2 reset the pool through 0x020c8a9c.
 * No RTTI or vtable, so the functions keep their address names.
 * The empty constructor and destructor are what the sinit at
 * 0x0212f660 passes to __cxa_vec_ctor.
 *
 * leftovers:
 * - 020c8ba8 and 020c8af4 still address the record as bytes. The
 *   ((int)c + offset) form is the one those bodies matched.
 * - 020c8c6c clears the active flag through a short* at index 12.
 * - ApproachLinear and ApproachLinear2 stay local externs. They
 *   take the value by reference.
 * - The pool, the counter and the sprite table are overlay data,
 *   not this TU's.
 */

typedef long long s64;

struct Ent_ov006 {
    int x;
    int y;
    int mVelX;
    int mVelY;
    int mSpread;
    int mScale;
    short mActive;
    short mIndex;
    short mAngle;
    short mTimer;
};

extern int ApproachLinear(int&, int, int);
extern int ApproachLinear2(short&, short, short);

extern "C" {
int GetGameLanguage(void);
void Hud_RenderSprite(void* a0, int a1, int a2, int a3, int a4);
typedef void (*dtor_t)(void *);
void __cxa_vec_cleanup(void *block, unsigned int n, unsigned int size, dtor_t dtor);
extern short data_02082214[];
extern void **data_ov006_0213b0d8[];
extern int data_ov006_021404c4;
extern void **data_ov006_021404c8;
extern struct Ent_ov006 data_ov006_021404d8[3];
extern int data_ov006_02140518[];

void func_ov006_020c8af4(char *c);
void func_ov006_020c8ba8(char *c);
void func_ov006_020c8c6c(short *p);
void func_ov006_020c8dcc(void);

// @symbol func_ov006_020c8dd0
void func_ov006_020c8dd0(void)
{
}

// @symbol func_ov006_020c8dcc
void func_ov006_020c8dcc(void)
{
}

// @symbol func_ov006_020c8da8
void func_ov006_020c8da8(void)
{
    __cxa_vec_cleanup(data_ov006_021404d8, 3, 0x20, (dtor_t)func_ov006_020c8dcc);
}

// @symbol func_ov006_020c8c78
void func_ov006_020c8c78(int x, int y)
{
    int i;
    int v;

    if (data_ov006_021404c4 >= 3) {
        return;
    }
    for (i = 0; i < 3; i++) {
        if (data_ov006_021404d8[i].mActive != 0) {
            continue;
        }
        data_ov006_021404d8[i].mIndex = data_ov006_021404c4;
        data_ov006_021404d8[i].x = x << 12;
        v = data_ov006_021404d8[i].x;
        if (v < 0x20000) {
            v = 0x20000;
        } else if (v > 0xE0000) {
            v = 0xE0000;
        }
        data_ov006_021404d8[i].x = v;
        data_ov006_021404d8[i].y = y << 12;
        data_ov006_021404d8[i].mVelX = 0;
        data_ov006_021404d8[i].mVelY = -0x400;
        data_ov006_021404d8[i].mActive = 1;
        data_ov006_021404d8[i].mAngle = 0;
        data_ov006_021404d8[i].mScale = 0x1000;
        data_ov006_021404d8[i].mSpread = 0x30000;
        data_ov006_021404d8[i].mTimer = 90;
        data_ov006_021404c4++;
        return;
    }
}

// @symbol func_ov006_020c8c6c
void func_ov006_020c8c6c(short *p)
{
    p[12] = 0;
}

// @symbol func_ov006_020c8ba8
void func_ov006_020c8ba8(char* c)
{
    if (*(short*)(c+0x18) == 0) return;
    *(short*)(((int)c + 0x1c)) += 0x200;
    *(int*)(c+0x10) = (int)(((long long)*(int*)(c+0x10) * *(int*)(c+0x14) + 0x800) >> 12);
    *(int*)(((int)c + 0xc)) -= 0x40;
    ApproachLinear(*(int*)(c+0x14), 0x800, 0x10);
    if (ApproachLinear2(*(short*)(c+0x1e), 0, 1) != 0) {
        if (*(short*)(c+0x1a) != 2)
            *(short*)(c+0x18) = 0;
    } else {
        *(int*)(c) += *(int*)(c+0x8);
        *(int*)(((int)c + 0x4)) += *(int*)(c+0xc);
    }
}

// @symbol func_ov006_020c8af4
void func_ov006_020c8af4(char *c)
{
    void *x;
    int t;
    int prod;
    int pos;
    if (*(short*)(c + 0x18) == 0) return;
    if (data_ov006_021404c8 != 0) {
        x = data_ov006_021404c8[*(short*)(c + 0x1a)];
    } else {
        int idx2 = GetGameLanguage();
        x = data_ov006_0213b0d8[idx2][*(short*)(c + 0x1a)];
    }
    t = data_02082214[(*(unsigned short*)(c + 0x1c) >> 4) * 2];
    prod = (int)(((s64)*(int*)(c + 0x10) * t + 0x800) >> 12);
    pos = *(int*)c + prod;
    Hud_RenderSprite(x, pos >> 12, *(int*)(c + 4) >> 12, -1, -1);
}

// @symbol func_ov006_020c8a9c
void func_ov006_020c8a9c(int a0,int a1){
  int i;
  short* p=(short*)data_ov006_021404d8;
  for(i=0;i<3;i++){
    func_ov006_020c8c6c(p);
    p=(short*)((char*)p+0x20);
  }
  data_ov006_021404c4=a0;
  data_ov006_021404c8=(void**)a1;
}

// @symbol func_ov006_020c8a64
void func_ov006_020c8a64(void){
  int i = 0;
  char *p = (char*)data_ov006_021404d8;
  for(;;){
    func_ov006_020c8ba8(p);
    i++;
    p += 0x20;
    if(i >= 3) break;
  }
}

// @symbol func_ov006_020c8a30
void func_ov006_020c8a30(void){
  int i;
  char *p=(char*)data_ov006_02140518;
  for(i=2;i>=0;i--){
    func_ov006_020c8af4(p);
    p-=0x20;
  }
}
}
