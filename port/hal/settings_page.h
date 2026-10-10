/* settings_page.h -- the in-game settings page (run menu1, lane SETMENU1).

   A host-resolution page drawn by the port over the paused game, in the launcher's look (parchment sheet, wooden title and back
   planks, purple drop-downs, wooden keycaps, cream check boxes with the DS lettering's X, brown sliders), with three tabs,
   CONTROLS, CAMERA and SOUND. Every row is one settings_live id (hal/settings_live.h): a change applies at once and is saved by the
   engine (sl_set; sl_flush when a slider is let go and when the page closes), so the launcher's page (same keys, same ranges) and
   this page are two views of one file.

   Two files: this one (settings_page.cpp) is pure host code, the page's state, input rules and drawing, and links without the ROM
   (the picture harness tests/settings_page_shot.cpp runs it); hal/settings_page_hooks.cpp is the part that reads ROM state, to
   open the page from the title's wrench and the pause menu's Options plate.

   THE FRAME CONTRACT. The window loops (tests/walk_window.cpp, menu_input) hold the game still (the debug menu's own pause,
   menu_on) while the page is open and call, once a frame on the logic side, settings_page_frame with the raw device state; the
   picture overlay hook calls settings_page_draw on the finished host picture. Nothing in the draw reads the ROM or a device. */
#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SpInput.keys: what is HELD this frame (the page makes the edges and the key repeat itself) */
enum {
    SPK_UP = 1, SPK_DOWN = 2, SPK_LEFT = 4, SPK_RIGHT = 8,
    SPK_ACCEPT = 16,     /* enter, space, pad A */
    SPK_BACK = 32,       /* escape, backspace, pad B */
    SPK_TABL = 64, SPK_TABR = 128,   /* pad LB / RB, page up / down, tab */
    SPK_CLEAR = 256,     /* delete, pad Y: unbind the binding under the cursor */
    SPK_CAP_CANCEL = 512 /* pad BACK: gives up a rebind capture (pad B and A are bindable) */
};

struct SpInput {
    unsigned keys;
    int mouse_valid, mouse_x, mouse_y;   /* the pointer over the picture, in picture pixels */
    int mouse_l, mouse_r;                /* buttons held */
    int wheel;                           /* notches since the last frame, forward positive */
    int inject_key;                      /* proof runs only (SM64DS_SETPAGE_SCRIPT key:<vk>): a key the window procedure would have seen, for the rebind capture */
};

int  settings_page_open(void);                       /* 1 while the page is up */
void settings_page_request(void);                    /* ask for it to open on the next frame (a hook saw the press) */
int  settings_page_pending(void);                    /* a request is waiting (cleared by settings_page_open_now) */
void settings_page_clear_request(void);               /* drop a request that cannot be honoured (the debug menu holds the pause) */
void settings_page_open_now(int from);               /* from: 0 pause, 1 title, 2 other; starts on CONTROLS, cursor on the first row */
void settings_page_close(void);                      /* flushes the settings file */
/* One logic frame with this frame's device state. Returns 1 while the page is still up, 0 on the frame it closed. */
int  settings_page_frame(const SpInput *in);
/* draw onto an opaque 32 bit picture (0x00RRGGBB or 0xFFRRGGBB pixels) of w x h with `stride` pixels per row */
void settings_page_draw(uint32_t *px, int stride, int w, int h);
/* the stylus filter of the touch path: 1 when the page swallows this frame's touch (it is open, or a press that opened it is
   still held), and the touch must read as not pressed */
int  settings_page_swallow_touch(int touch_down);
/* test seams */
int  settings_page_tab(void);
int  settings_page_row(void);
const char *settings_page_note(void);
int  settings_page_scripted(SpInput *in, int page_frame);   /* SM64DS_SETPAGE_SCRIPT: fills the device state for this frame */
double settings_page_draw_ms(void);                  /* the last draw's cost, for the cost line */
int  settings_page_env_open_at(void);                /* SM64DS_SETPAGE_AT=<frame>: the page opens at that frame (proof rows); -1 none */
const char *settings_page_key_name(int vk, char *buf, int cap);
const char *settings_page_pad_name(int mask, char *buf, int cap);

#ifdef __cplusplus
}
#endif
#endif
