/* settings_page_hooks.cpp -- how the settings page opens (run menu1, lane SETMENU1). The ROM-aware half of hal/settings_page.cpp.

   With OneScreenMenus on, the title's WRENCH and the pause menu's OPTIONS plate open the port's settings page instead of the ROM's
   own options screen. Both are caught BEFORE the ROM sees the press, so the ROM never leaves the page it is on and Back returns to it
   exactly as it was:
     - a pad / key confirm: hal_menu_pad_seam (hal/sub_screen.cpp) hands the pressed word here first (settings_page_hook_pad);
     - a mouse click or a synthesised stylus press: poll_touch hands the frame's stylus here (settings_page_hook_touch).
   The ROM facts read here are the ones the two ROM screens themselves test:
     PAUSE  src/_ZN5Stage9PS_UpdateEv.cpp: the main page is state 1 (castle) or 3 (course); the Options plate is selection 2 (data_0209f2e0),
            its touch box x 8..0xf7, y 0x70..0x8f in the castle (:277-299) and y 0x68..0x87 in a course (:485-505); it acts only when the
            input lock after the open (data_0209f300) and the press timers (data_0209f22c, data_0209f244) are 0 (hal/menu_recipe_pause.cpp
            pause_cursor holds the same gate); a VS pause (states 4, 5) has no Options plate.
     TITLE  the main page is top-state 0, element machine 0, no choice taken (req -1) (hal/menu_recipe_title.cpp title_cursor_assist); the
            wrench is element 7 (its box from the ROM's own element list, port_title_elem_boxes, else the dumped 216,0..256,40), and the
            ROM's cursor (data_ov007_02103360) names 7 while it is selected.
   Nothing here draws or reads a device. */
#include "settings_page.h"
#include "menu_compose.h"

#include <stdint.h>
#include <stdio.h>

extern "C" {
extern unsigned char data_0209f248;    /* the pause state PS_Update is in */
extern unsigned char data_0209f2c4;    /* 0 = playing, else the pause is up */
extern unsigned char data_0209f20c;    /* level-clear owns the game */
extern unsigned char data_0209f2e0;    /* the selected plate */
extern unsigned char data_0209f300;    /* input lock after open */
extern unsigned char data_0209f22c;    /* press hold ticks */
extern unsigned char data_0209f244;    /* press flash ticks */
extern char *data_ov007_02103360;      /* the title's element-cursor struct */
extern unsigned char data_020a0e40;    /* the local player's slot */
extern int data_020a0e58[];            /* PadData[i]: {u16 held, u16 pressed} */
int port_scene_live_id(void);
int port_title_elem_boxes(int *slot, int *id, int *x0, int *y0, int *x1, int *y1, int max);
int port_title_page_state(int *top, int *elem, int *req);
void port_host_keys_clear(unsigned short mask);   /* hal/scene_boot.cpp */
}

namespace {

enum { TGT_NONE = 0, TGT_PAUSE = 1, TGT_TITLE = 2 };

int armed_target()
{
    if (settings_page_open()) return TGT_NONE;
    if (!hal_menu_enabled(MENU_KEY_ONE_SCREEN_MENUS)) return TGT_NONE;
    const int scene = port_scene_live_id();
    if ((scene == 3 || scene == -1) && data_0209f2c4 && !data_0209f20c) {
        const unsigned st = data_0209f248;
        if ((st == 1 || st == 3) && data_0209f300 == 0 && data_0209f22c == 0 && data_0209f244 == 0) return TGT_PAUSE;
        return TGT_NONE;
    }
    if (scene == 1) {
        int top, elem, req;
        if (port_title_page_state(&top, &elem, &req) && top == 0 && elem == 0 && req == -1) return TGT_TITLE;
    }
    return TGT_NONE;
}

int wrench_box(int *x0, int *y0, int *x1, int *y1)
{
    int slot[24], ids[24], bx0[24], by0[24], bx1[24], by1[24];
    const int n = port_title_elem_boxes(slot, ids, bx0, by0, bx1, by1, 24);
    for (int k = 0; k < n; ++k)
        if (ids[k] == 7) { *x0 = bx0[k]; *y0 = by0[k]; *x1 = bx1[k]; *y1 = by1[k]; return 1; }
    *x0 = 216; *y0 = 0; *x1 = 256; *y1 = 40;
    return 1;
}

/* MENUS093 (D5): the title's MP / VS button (element 1) takes no press: multiplayer is not in this build and the ROM's answer (a
   page change to a heap the host does not mount) restarted the title in the middle of the main menu. The press is taken away before the
   ROM sees it, in either menu mode: a stylus on the button's box, or a confirm while the ROM's cursor is on it. */
int title_main_page()
{
    if (port_scene_live_id() != 1) return 0;
    int top, elem, req;
    return port_title_page_state(&top, &elem, &req) && top == 0 && elem == 0 && req == -1;
}

int vs_box(int *x0, int *y0, int *x1, int *y1)
{
    int slot[24], ids[24], bx0[24], by0[24], bx1[24], by1[24];
    const int n = port_title_elem_boxes(slot, ids, bx0, by0, bx1, by1, 24);
    for (int k = 0; k < n; ++k)
        if (ids[k] == 1) { *x0 = bx0[k]; *y0 = by0[k]; *x1 = bx1[k]; *y1 = by1[k]; return 1; }
    *x0 = 2; *y0 = 148; *x1 = 66; *y1 = 188;
    return 1;
}

int title_cursor_on_vs()
{
    if (!data_ov007_02103360) return 0;
    const short *p0 = *(const short **)data_ov007_02103360;
    return p0 && p0[0] == 1;
}

int title_cursor_on_wrench()
{
    if (!data_ov007_02103360) return 0;
    const short *p0 = *(const short **)data_ov007_02103360;
    return p0 && p0[0] == 7;
}

}  // namespace

/* the pad seam's pressed word, before hal_menu_pad: 1 when this press opened the page (the bits are taken out of the pressed AND the held
   word, so the ROM, which builds its key record from the held word on the title, sees no confirm) */
extern "C" int settings_page_hook_pad(uint16_t *pressed)
{
    {
        /* MENUS093: a confirm on the title's MP button does nothing. MENUS093B: it is the HELD word the ROM's key record is built from
           (current & ~previous), so a press that lasts more than one frame must be taken out of the held word on every frame it is down, not
           only on the frame of its edge: the guard used to read the pressed word alone, the held word came back on the second frame and the
           ROM saw a fresh edge (a keyboard or pad press is never one frame long). */
        const uint16_t bits = 0x0001 | 0x0002 | 0x0008;
        unsigned short *held = (unsigned short *)((char *)data_020a0e58 + data_020a0e40 * 4);
        if ((*pressed | *held) & bits) {
            if (hal_menu_trace_on() && port_scene_live_id() == 1) {
                int t, e, r;
                const int ok = port_title_page_state(&t, &e, &r);
                const short *c = data_ov007_02103360 ? *(const short **)data_ov007_02103360 : 0;
                fprintf(stderr, "[mpguard] pressed %04x held %04x page %d top %d elem %d req %d cursor %d,%d\n", *pressed, *held, ok, t, e, r,
                        c ? c[0] : -99, c ? c[1] : -99);
            }
            if (title_main_page() && title_cursor_on_vs()) {
                *pressed = (uint16_t)(*pressed & ~bits);
                *held = (unsigned short)(*held & ~bits);
                port_host_keys_clear(bits);
                return 1;
            }
        }
    }
    const int tgt = armed_target();
    if (!tgt) return 0;
    const uint16_t confirm = 0x0002 | 0x0008;           /* the port's jump (pad A, Space) and Start (Enter) */
    if (!(*pressed & confirm)) return 0;
    if (tgt == TGT_PAUSE) {
        if (data_0209f2e0 != 2) return 0;
    } else {
        if (!title_cursor_on_wrench()) return 0;
    }
    *pressed = (uint16_t)(*pressed & ~confirm);
    unsigned short *held = (unsigned short *)((char *)data_020a0e58 + data_020a0e40 * 4);
    *held = (unsigned short)(*held & ~confirm);
    if (tgt == TGT_TITLE) port_host_keys_clear(confirm);   /* MENUS093: the title reads the host key word too */
    settings_page_request();
    return 1;
}

/* poll_touch's stylus: 1 when the page takes it (the press that opens it, then everything while it is up and until the finger is lifted) */
extern "C" int settings_page_hook_touch(unsigned char down, unsigned char sx, unsigned char sy)
{
    if (settings_page_swallow_touch(down)) return 1;
    if (!down) return 0;
    if (title_main_page()) {                           /* MENUS093: a stylus / click on the title's MP button does nothing */
        int vx0, vy0, vx1, vy1;
        vs_box(&vx0, &vy0, &vx1, &vy1);
        if (sx >= vx0 && sx < vx1 && sy >= vy0 && sy < vy1) return 1;
    }
    const int tgt = armed_target();
    if (!tgt) return 0;
    if (tgt == TGT_PAUSE) {
        const unsigned st = data_0209f248;
        const unsigned y0 = st == 1 ? 0x70u : 0x68u;
        if ((unsigned char)(sx - 8) < 0xf0 && (unsigned char)(sy - y0) < 0x20) { settings_page_request(); return 1; }
    } else {
        int x0, y0, x1, y1;
        wrench_box(&x0, &y0, &x1, &y1);
        if (sx >= x0 && sx < x1 && sy >= y0 && sy < y1) { settings_page_request(); return 1; }
    }
    return 0;
}

/* SETPAGEFIX1 / PAUSEFIX1: called once a frame from the idle side of the page's frame; builds the page's caches in small slices a while
   after start, so the first open does not stall and no frame pays more than a slice. The slice runs only when the shared warm-up slot
   (menu_warm_claim, menu_recipe_pause.cpp: one slot per few ms for the pause and the page together, none while the game is hitching)
   is free. Nothing at all when OneScreenMenus is off. */
extern "C" int menu_warm_claim(void);
extern "C" void settings_page_idle_tick(int idle_frames, int w, int h)
{
    static int done_w, done_h;
    if (idle_frames < 120) return;
    if (!hal_menu_enabled(MENU_KEY_ONE_SCREEN_MENUS)) return;
    if (done_w == w && done_h == h) return;
    if (settings_page_warm_waiting()) return;
    if (!menu_warm_claim()) return;
    if (settings_page_warm_step(w, h)) { done_w = w; done_h = h; }
}
