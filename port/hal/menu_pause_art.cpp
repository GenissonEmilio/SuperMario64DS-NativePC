/* menu_pause_art.cpp -- the pause menu's host pictures (run menu1, lane PAUSE3): the registry behind hostimg's extension ids 32..45.
   The bytes are res/menu/pause3/*.png (tools/prepare_pause3_art.py), embedded by tools/embed_pause3_art.py into menu_pause_art_data.cpp. */
#include "menu_hostimg.h"
#include "menu_pause_art_ids.h"

struct PauseArt { const char *file; const unsigned char *data; unsigned len; };
extern "C" const PauseArt g_pause_art[];
extern "C" const int g_pause_art_count;

extern "C" int menu_ext_art_src(int id, const char **file, const unsigned char **emb, unsigned *len)
{
    const int k = id - MENU_IMG_EXT_BASE;
    if (k < 0 || k >= g_pause_art_count) return 0;
    *file = g_pause_art[k].file; *emb = g_pause_art[k].data; *len = g_pause_art[k].len;
    return 1;
}

extern "C" int menu_ext_art_is_plank(int id)
{
    return id >= MENU_IMG_P_PK_RESUME && id <= MENU_IMG_P_PK_NOKEEP;
}
