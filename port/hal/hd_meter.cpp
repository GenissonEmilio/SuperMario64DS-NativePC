/* hd_meter.cpp -- see hd_meter.h (run hunt8, lane HDMETER1): the HD power meter art out of the enabled mods. */
#include "hd_meter.h"
#include "mods.h"
#include "../third_party/stb/stb_image.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

HdMeterArt g_art[18];
bool g_tried[18];

/* the file for this wedge count and size, from the last enabled mod that has it (mods.h: port_mods_ui_file) */
unsigned char *load_from_mods(const char *name, int *w, int *h, const char **from)
{
    char rel[160], path[1400];
    std::snprintf(rel, sizeof rel, "hud/power_meter/%s", name);
    if (!port_mods_ui_file(rel, path, sizeof path)) return 0;
    int comp = 0;
    unsigned char *p = stbi_load(path, w, h, &comp, 4);
    if (p) *from = path;
    return p;
}

const HdMeterArt *get(int health, int k)
{
    const int idx = health * 2 + (k == 4 ? 1 : 0);
    if (g_tried[idx]) return g_art[idx].rgba ? &g_art[idx] : 0;
    g_tried[idx] = true;
    char name[64];
    std::snprintf(name, sizeof name, "meter_health%d_with_label%s.png", health, k == 4 ? "_4x" : "");
    int w = 0, h = 0;
    const char *from = "";
    unsigned char *p = load_from_mods(name, &w, &h, &from);
    if (!p) return 0;
    /* the content is 40 x 40 DS pixels; a file that cannot hold it is refused (the DS sprites stay) */
    if (w < 40 * k || h < 40 * k) {
        stbi_image_free(p);
        std::fprintf(stderr, "[hdmeter] %s from %s: %dx%d is too small for 40x40 DS pixels at %dx, ignored
", name, from, w, h, k);
        return 0;
    }
    g_art[idx].k = k; g_art[idx].w = w; g_art[idx].h = h; g_art[idx].rgba = p;
    std::fprintf(stderr, "[hdmeter] %s %dx%d from %s
", name, w, h, from);
    return &g_art[idx];
}

}  // namespace

const HdMeterArt *hd_meter_art(int health, int k)
{
    if (health < 0 || health > 8 || (k != 1 && k != 4)) return 0;
    if (const HdMeterArt *a = get(health, k)) return a;
    return get(health, k == 4 ? 1 : 4);   /* a mod with one size only serves both */
}

/* the compositor's hook (hal/message_compositor.cpp), set before main like hal/mods.cpp's own link */
extern "C" const HdMeterArt *(*port_hd_meter_art_hook)(int health, int k);
namespace {
struct HookSet { HookSet() { port_hd_meter_art_hook = hd_meter_art; } } g_hook_set;
}
