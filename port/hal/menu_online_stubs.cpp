/* menu_online_stubs.cpp -- the online menu's draw side for the smoke player (run after093, lane NAME1): the smoke target does not link ui_text, so the
   build side answers "nothing to add" and every plank is its solo self. See hal/menu_online_draw.cpp for the real one. */
#include "menu_online.h"

void menu_online_plank_state(const OnlFacts *, int k, int *kind, int *w1000)
{
    *w1000 = 1000;
    *kind = k == 0 ? ONL_K_ONLINE : k == 1 ? ONL_K_ADV : ONL_K_REC;
}
int menu_online_add_plank(MenuLayout *, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { return -1; }
int menu_online_plank_w(int, int fh) { return fh; }
int menu_online_plank_art_ready(int) { return 0; }
void menu_online_add_modal(MenuLayout *, const OnlFacts *) {}
void menu_online_add_help(MenuLayout *, int, int, int) {}
