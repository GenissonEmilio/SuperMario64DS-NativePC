/* menu_hostimg.cpp -- see menu_hostimg.h (run menu1, lane ASSET2). */
#include "menu_hostimg.h"
#include "../third_party/stb/stb_image.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

extern "C" const unsigned char g_menu_png_logo[], g_menu_png_btn_mp[], g_menu_png_btn_adventure[], g_menu_png_btn_rec[], g_menu_png_arrow_up[], g_menu_png_arrow_down[], g_menu_png_pattern[], g_menu_png_title_star[], g_menu_png_btn_save_cont[], g_menu_png_btn_save_quit[], g_menu_png_btn_cont[];
extern "C" const unsigned int g_menu_png_logo_len, g_menu_png_btn_mp_len, g_menu_png_btn_adventure_len, g_menu_png_btn_rec_len, g_menu_png_arrow_up_len, g_menu_png_arrow_down_len, g_menu_png_pattern_len, g_menu_png_title_star_len, g_menu_png_btn_save_cont_len, g_menu_png_btn_save_quit_len, g_menu_png_btn_cont_len;

namespace {

struct Img {
    int tried = 0, w = 0, h = 0;
    std::vector<uint32_t> px;           /* premultiplied, source size */
    /* ASSET5: TWO cached scaled sizes (the title's logo is drawn big and, after the choreography, small; a size change must not
       cost a rescale every frame) */
    int cw = 0, ch = 0;                 /* the cached scaled size, slot 0 */
    std::vector<uint32_t> scaled;
    int cw1 = 0, ch1 = 0;               /* slot 1 */
    std::vector<uint32_t> scaled1;
    int last_slot = 0;                  /* the slot filled last (the other one is replaced next) */
};
Img g_img[MENU_IMG_COUNT];
struct Src { const char *file; const unsigned char *emb; const unsigned *len; };
const Src kSrc[MENU_IMG_COUNT] = {
    { 0, 0, 0 },
    { "logo_sm64_pc.png", g_menu_png_logo, &g_menu_png_logo_len },
    { "btn_mp.png", g_menu_png_btn_mp, &g_menu_png_btn_mp_len },
    { "btn_adventure.png", g_menu_png_btn_adventure, &g_menu_png_btn_adventure_len },
    { "btn_rec.png", g_menu_png_btn_rec, &g_menu_png_btn_rec_len },
    { "arrow_up.png", g_menu_png_arrow_up, &g_menu_png_arrow_up_len },
    { "arrow_down.png", g_menu_png_arrow_down, &g_menu_png_arrow_down_len },
    { "pattern.png", g_menu_png_pattern, &g_menu_png_pattern_len },
    { "title_star.png", g_menu_png_title_star, &g_menu_png_title_star_len },
    { "btn_save_cont.png", g_menu_png_btn_save_cont, &g_menu_png_btn_save_cont_len },
    { "btn_save_quit.png", g_menu_png_btn_save_quit, &g_menu_png_btn_save_quit_len },
    { "btn_cont.png", g_menu_png_btn_cont, &g_menu_png_btn_cont_len },
};

bool take(const unsigned char *rgba, int w, int h, Img *I)
{
    if (!rgba || w < 8 || h < 8 || w > 8192 || h > 8192) return false;
    I->w = w; I->h = h;
    I->px.resize((size_t)w * h);
    for (int i = 0; i < w * h; ++i) {
        const unsigned r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2], a = rgba[i * 4 + 3];
        I->px[i] = (a << 24) | (((r * a + 127) / 255) << 16) | (((g * a + 127) / 255) << 8) | ((b * a + 127) / 255);
    }
    return true;
}

void load_img(int id)
{
    Img &I = g_img[id];
    if (I.tried) return;
    I.tried = 1;
    int w = 0, h = 0, comp = 0;
    unsigned char *p = 0;
    char path[1100];
    /* SM64DS_MENU_LOGO (id 1) or SM64DS_MENU_ART_DIR=<folder> (every id) point somewhere else; a name that cannot be read there
       means "no picture" (the recipe keeps the DS art) and the embedded copy is NOT used, so the fallback can be proven */
    const char *over = id == MENU_IMG_LOGO ? getenv("SM64DS_MENU_LOGO") : 0;
    const char *dir = getenv("SM64DS_MENU_ART_DIR");
    if (over && *over) {
        p = stbi_load(over, &w, &h, &comp, 4);
        fprintf(stderr, "[menu] art %d: SM64DS_MENU_LOGO=%s -> %s\n", id, over, p ? "loaded" : "unreadable, DS art kept");
    } else if (dir && *dir && strlen(dir) + 40 < sizeof path) {
        snprintf(path, sizeof path, "%s/%s", dir, kSrc[id].file);
        p = stbi_load(path, &w, &h, &comp, 4);
        fprintf(stderr, "[menu] art %d: %s -> %s\n", id, path, p ? "loaded" : "unreadable, DS art kept");
    } else {
        const char *root = getenv("SM64DS_ASSET_ROOT");
        if (root && *root && strlen(root) + 40 < sizeof path) {
            snprintf(path, sizeof path, "%s/menu/%s", root, kSrc[id].file);
            p = stbi_load(path, &w, &h, &comp, 4);
            if (p) fprintf(stderr, "[menu] art %d: kit file %s (%dx%d)\n", id, path, w, h);
        }
        if (!p) {
            p = stbi_load_from_memory(kSrc[id].emb, (int)*kSrc[id].len, &w, &h, &comp, 4);
            if (p) fprintf(stderr, "[menu] art %d: embedded copy %s (%dx%d)\n", id, kSrc[id].file, w, h);
        }
    }
    if (p) {
        if (!take(p, w, h, &I)) I.px.clear();
        stbi_image_free(p);
    }
}

/* one axis of a separable resample: for each destination index, the source span and weights (triangle filter whose
   support grows with the reduction, so a shrink is an area average and an enlarge is bilinear) */
struct Tap { int first, n; std::vector<float> w; };
void taps(int sn, int dn, std::vector<Tap> *out)
{
    out->resize((size_t)dn);
    const double sc = (double)sn / dn, sup = sc > 1.0 ? sc : 1.0;
    for (int d = 0; d < dn; ++d) {
        const double c = (d + 0.5) * sc;
        int a = (int)floor(c - sup), b = (int)ceil(c + sup);
        if (a < 0) a = 0;
        if (b > sn) b = sn;
        Tap &t = (*out)[d];
        t.first = a; t.n = b - a; t.w.assign((size_t)t.n, 0.f);
        double sum = 0;
        for (int s = a; s < b; ++s) {
            double x = fabs((s + 0.5) - c) / sup;
            double wv = x < 1.0 ? 1.0 - x : 0.0;
            t.w[s - a] = (float)wv; sum += wv;
        }
        if (sum <= 0) { t.first = a < sn ? a : sn - 1; t.n = 1; t.w.assign(1, 1.f); sum = 1; }
        for (int s = 0; s < t.n; ++s) t.w[s] = (float)(t.w[s] / sum);
    }
}

void scale_to(Img *I, int dw, int dh, std::vector<uint32_t> *outv)
{
    std::vector<Tap> tx, ty;
    taps(I->w, dw, &tx);
    taps(I->h, dh, &ty);
    std::vector<float> mid((size_t)dw * I->h * 4);
    for (int y = 0; y < I->h; ++y) {
        const uint32_t *row = &I->px[(size_t)y * I->w];
        for (int x = 0; x < dw; ++x) {
            float a = 0, r = 0, g = 0, b = 0;
            const Tap &t = tx[x];
            for (int k = 0; k < t.n; ++k) {
                const uint32_t c = row[t.first + k];
                const float wv = t.w[k];
                a += wv * (c >> 24); r += wv * ((c >> 16) & 255); g += wv * ((c >> 8) & 255); b += wv * (c & 255);
            }
            float *m = &mid[((size_t)y * dw + x) * 4];
            m[0] = a; m[1] = r; m[2] = g; m[3] = b;
        }
    }
    outv->assign((size_t)dw * dh, 0);
    for (int y = 0; y < dh; ++y) {
        const Tap &t = ty[y];
        for (int x = 0; x < dw; ++x) {
            float a = 0, r = 0, g = 0, b = 0;
            for (int k = 0; k < t.n; ++k) {
                const float *m = &mid[((size_t)(t.first + k) * dw + x) * 4];
                const float wv = t.w[k];
                a += wv * m[0]; r += wv * m[1]; g += wv * m[2]; b += wv * m[3];
            }
            int ia = (int)(a + 0.5f), ir = (int)(r + 0.5f), ig = (int)(g + 0.5f), ib = (int)(b + 0.5f);
            if (ia > 255) ia = 255;
            if (ir > ia) ir = ia;
            if (ig > ia) ig = ia;
            if (ib > ia) ib = ia;
            (*outv)[(size_t)y * dw + x] = ((uint32_t)ia << 24) | ((uint32_t)ir << 16) | ((uint32_t)ig << 8) | (uint32_t)ib;
        }
    }
}

Img *img_of(int id) { return id >= 1 && id < MENU_IMG_COUNT ? &g_img[id] : 0; }

}  // namespace

extern "C" int menu_hostimg_ready(int id)
{
    Img *I = img_of(id);
    if (!I) return 0;
    load_img(id);
    return I->w > 0 && !I->px.empty();
}

extern "C" int menu_hostimg_size(int id, int *w, int *h)
{
    if (!menu_hostimg_ready(id)) return 0;
    Img *I = img_of(id);
    *w = I->w; *h = I->h;
    return 1;
}

extern "C" const uint32_t *menu_hostimg_scaled(int id, int dw, int dh)
{
    if (!menu_hostimg_ready(id) || dw < 1 || dh < 1 || dw > 8192 || dh > 8192) return 0;
    Img *I = img_of(id);
    if (I->cw == dw && I->ch == dh && !I->scaled.empty()) return &I->scaled[0];
    if (I->cw1 == dw && I->ch1 == dh && !I->scaled1.empty()) return &I->scaled1[0];
    /* a miss replaces the slot that was not filled last */
    if (I->last_slot == 0) { scale_to(I, dw, dh, &I->scaled1); I->cw1 = dw; I->ch1 = dh; I->last_slot = 1; return &I->scaled1[0]; }
    scale_to(I, dw, dh, &I->scaled); I->cw = dw; I->ch = dh; I->last_slot = 0;
    return &I->scaled[0];
}
