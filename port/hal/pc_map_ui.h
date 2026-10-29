// Original PC interface, MIT; no cartridge data or platform input dependencies.
#ifndef PC_MAP_UI_H
#define PC_MAP_UI_H
#include <stdint.h>
struct PcMapRect { int x, y, w, h; };
PcMapRect pc_map_button_rect(int w, int h);
int pc_map_button_hit(int x, int y, int w, int h);
void pc_map_button_draw(uint32_t *dst, int w, int h, int stride, int open);
#endif
