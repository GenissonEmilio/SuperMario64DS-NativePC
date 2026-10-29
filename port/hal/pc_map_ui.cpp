#include "pc_map_ui.h"
#include <algorithm>

PcMapRect pc_map_button_rect(int w, int h)
{
    if (w < 128 || h < 96) return {0, 0, 0, 0};
    const int bh = std::max(18, h * 26 / 384);
    const int bw = std::max(64, h * 108 / 384);
    const int margin = std::max(5, h * 9 / 384);
    return {std::max(margin, w - bw - margin), h - bh - margin, std::min(bw, w - margin * 2), bh};
}

int pc_map_button_hit(int x, int y, int w, int h)
{
    const PcMapRect r = pc_map_button_rect(w, h);
    return r.w > 0 && x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

// Hand-drawn 5x7 lettering for MAPA / FECHAR. No system font or ROM font required.
static const unsigned char *glyph(char c)
{
    static const unsigned char a[7] = {14,17,17,31,17,17,17};
    static const unsigned char c_[7] = {14,17,16,16,16,17,14};
    static const unsigned char e[7] = {31,16,16,30,16,16,31};
    static const unsigned char f[7] = {31,16,16,30,16,16,16};
    static const unsigned char h[7] = {17,17,17,31,17,17,17};
    static const unsigned char m[7] = {17,27,21,21,17,17,17};
    static const unsigned char p[7] = {30,17,17,30,16,16,16};
    static const unsigned char r[7] = {30,17,17,30,20,18,17};
    switch (c) { case 'A': return a; case 'C': return c_; case 'E': return e;
    case 'F': return f; case 'H': return h; case 'M': return m; case 'P': return p; default: return r; }
}

void pc_map_button_draw(uint32_t *dst, int w, int h, int stride, int open)
{
    if (!dst || stride < w) return;
    const PcMapRect r = pc_map_button_rect(w, h);
    if (!r.w) return;
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x)
            dst[y * stride + x] = (x == r.x || y == r.y || x == r.x+r.w-1 || y == r.y+r.h-1)
                ? 0xffffd166u : 0xff172b44u;
    const char *s = open ? "FECHAR" : "MAPA";
    const int n = open ? 6 : 4;
    const int scale = std::max(1, std::min((r.h-6)/7, (r.w-8)/(n*6)));
    const int ox = r.x + (r.w-(n*6-1)*scale)/2;
    const int oy = r.y + (r.h-7*scale)/2;
    for (int i=0;i<n;++i) for (int gy=0;gy<7;++gy) for (int gx=0;gx<5;++gx)
        if (glyph(s[i])[gy] & (1 << (4-gx)))
            for (int sy=0;sy<scale;++sy) for (int sx=0;sx<scale;++sx)
                dst[(oy+gy*scale+sy)*stride+ox+(i*6+gx)*scale+sx] = 0xffffffffu;
}
