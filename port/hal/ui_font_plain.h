/* ui_font_plain.h -- see ui_font_plain.cpp (run hunt8, lane UIMOD1). */
#ifndef UI_FONT_PLAIN_H
#define UI_FONT_PLAIN_H
#include <stdint.h>
#include <vector>

struct UiGlyphSrc { int ch, x, y, w, h, bx, by; float adv; };   /* atlas box, bearing from the pen / baseline, advance (atlas pixels) */

void ui_font_plain_build(UiGlyphSrc *gl95, std::vector<uint8_t> *atlas, int *aw, int *ah);
#endif
