/* settings_page_shot: the in-game settings page (hal/settings_page.cpp) drawn on a plain picture with no ROM and no window,
   for the LOOK rows and the page's own logic rows. Run menu1, lane SETMENU1.

     settings_page_shot <w> <h> <out base> <frames> [<shot frame>,<shot frame>...]

   SM64DS_ASSET_ROOT names a folder whose settings.json is the file the page reads and writes (the engine's own rule).
   SM64DS_SETPAGE_SCRIPT drives it ("f:cmd[:a[:b]];..." see settings_page.cpp); the page opens before frame 1. A picture <out base>_<frame>.bmp is
   written after each listed frame (the last frame when none is listed). Prints the draw cost of each shot. */
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"

#include "../hal/settings_page.h"
#include "../hal/settings_live.h"
#include "../hal/host_settings.h"
#include "../hal/ui_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

static void write_bmp(const char *path, const uint32_t *px, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    const int rowb = (w * 3 + 3) & ~3;
    unsigned char hd[54] = { 'B', 'M' };
    const unsigned size = 54 + rowb * h;
    memcpy(hd + 2, &size, 4);
    const unsigned off = 54, dib = 40;
    memcpy(hd + 10, &off, 4); memcpy(hd + 14, &dib, 4);
    memcpy(hd + 18, &w, 4); memcpy(hd + 22, &h, 4);
    hd[26] = 1; hd[28] = 24;
    fwrite(hd, 1, 54, f);
    std::vector<unsigned char> row(rowb);
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            const uint32_t c = px[(size_t)y * w + x];
            row[x * 3] = (unsigned char)(c & 255); row[x * 3 + 1] = (unsigned char)((c >> 8) & 255); row[x * 3 + 2] = (unsigned char)((c >> 16) & 255);
        }
        fwrite(row.data(), 1, rowb, f);
    }
    fclose(f);
}

/* a stand-in for the game: sky, hills, a few bright shapes */
static void fake_game(uint32_t *px, int w, int h)
{
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int r = 90 + y * 100 / h, g = 150 + y * 80 / h, b = 235 - y * 40 / h;
            if (y > h * 62 / 100) { r = 60 + ((x / 64 + y / 64) & 1) * 30; g = 150; b = 60; }
            if ((x - w / 2) * (x - w / 2) + (y - h / 2) * (y - h / 2) < (h / 5) * (h / 5)) { r = 230; g = 40; b = 30; }
            px[(size_t)y * w + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
}

int main(int argc, char **argv)
{
    if (argc < 5) { fprintf(stderr, "usage: settings_page_shot w h outbase frames [shot,shot..]\n"); return 2; }
    const int w = atoi(argv[1]), h = atoi(argv[2]), frames = atoi(argv[4]);
    std::vector<int> shots;
    if (argc > 5) for (char *p = argv[5]; *p;) { shots.push_back(atoi(p)); while (*p && *p != ',') ++p; if (*p == ',') ++p; }
    if (shots.empty()) shots.push_back(frames);
    std::vector<uint32_t> pic((size_t)w * h), game((size_t)w * h);
    fake_game(game.data(), w, h);
    settings_page_open_now(0);
    settings_page_draw(pic.data(), w, w, h);       /* the first draw tells the page the picture size */
    SpInput in;
    memset(&in, 0, sizeof in);
    for (int f = 1; f <= frames; ++f) {
        SpInput cur;
        memset(&cur, 0, sizeof cur);
        settings_page_scripted(&cur, f);
        if (!settings_page_frame(&cur)) { fprintf(stderr, "[shot] page closed at frame %d\n", f); }
        for (size_t i = 0; i < shots.size(); ++i)
            if (shots[i] == f) {
                memcpy(pic.data(), game.data(), (size_t)w * h * 4);
                settings_page_draw(pic.data(), w, w, h);
                char path[512];
                snprintf(path, sizeof path, "%s_%d.bmp", argv[3], f);
                write_bmp(path, pic.data(), w, h);
                fprintf(stderr, "[shot] frame %d tab %d row %d note '%s' draw %.3f ms glyphs cached %d -> %s\n", f, settings_page_tab(), settings_page_row(), settings_page_note(),
                        settings_page_draw_ms(), ui_text_cache_count(), path);
            }
    }
    /* a steady cost: the same frame 200 more times */
    {
        double tot = 0, mx = 0, mn = 1e9;
        for (int i = 0; i < 200; ++i) {
            memcpy(pic.data(), game.data(), (size_t)w * h * 4);
            settings_page_draw(pic.data(), w, w, h);
            const double ms = settings_page_draw_ms();
            tot += ms; if (ms > mx) mx = ms; if (ms < mn) mn = ms;
        }
        fprintf(stderr, "[shot] steady draw %.3f ms min, %.3f ms mean, %.3f ms max over 200 (incl. dim; the picture copy is not counted) at %dx%d\n", mn, tot / 200, mx, w, h);
    }
    settings_page_close();
    return 0;
}
