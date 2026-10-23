#include "types.h"
// SetPlayerGlobals - initializes player globals: lives, health, controller mode
extern u8 data_0209f2f4;
extern u16 data_02092144[];
extern u8 data_0209caa0[];
extern signed char data_02092114;
extern u8 data_0209f254;

extern void InitControllerMode(s32 playerIdx, s32 healthVal);

#ifdef _MSC_VER
/* host only: the port's KeepLives setting (hal/char_save.cpp) seats the saved life count after the 4 below. Null on the DS side and in
   targets without the hook. */
void (*port_lives_restore_hook)(void) = 0;
#endif

void SetPlayerGlobals(void) {
    data_0209f2f4 = 4;
    s32 i = 0;
    do {
        u8 healthVal = data_0209caa0[0x42];
        data_02092144[i] = 0x880;
        InitControllerMode(i, healthVal);
        i++;
    } while (i < 4);
    data_02092114 = -1;
    data_0209f254 = 0;
#ifdef _MSC_VER
    if (port_lives_restore_hook)
        port_lives_restore_hook();
#endif
}