// THE FOUR HEAD PICTURES. Run mp1 wave 56, lane PLIST1. See head_icons.h.
//
// Hand-made heads, built once from a few shapes (a face ellipse, a cap, a
// brim, eyes, a moustache, a letter on the cap), each with a one-pixel dark
// outline. No ROM art is copied or stored.

#include "hal/head_icons.h"

#include <cstring>

namespace port {
namespace {

uint32_t g_px[4][kHeadIconSize * kHeadIconSize];
bool g_built = false;

inline bool inside(int x, int y, double cx, double cy, double rx, double ry) {
    const double dx = (x + 0.5 - cx) / rx, dy = (y + 0.5 - cy) / ry;
    return dx * dx + dy * dy <= 1.0;
}

// 3 x 5 letters, one bit per pixel, MSB left: M, L, W.
const unsigned char kLetter[3][5] = {
    {0x5, 0x7, 0x7, 0x5, 0x5},   // M (blocky)
    {0x4, 0x4, 0x4, 0x4, 0x7},   // L
    {0x5, 0x5, 0x7, 0x7, 0x5},   // W (blocky)
};

void build_one(int c, uint32_t *p) {
    const uint32_t kClear = 0, kSkin = 0xFFF4C69Au, kOut = 0xFF202020u;
    const uint32_t kBrown = 0xFF5A3418u, kWhite = 0xFFFFFFFFu;
    uint32_t cap, letter;
    if (c == 0)      { cap = 0xFFD42020u; letter = 0xFFD42020u; }
    else if (c == 1) { cap = 0xFF28A838u; letter = 0xFF28A838u; }
    else             { cap = 0xFFF0C818u; letter = 0xFF7030A0u; }
    for (int i = 0; i < kHeadIconSize * kHeadIconSize; ++i) p[i] = kClear;
    auto set = [&](int x, int y, uint32_t v) {
        if (x >= 0 && x < kHeadIconSize && y >= 0 && y < kHeadIconSize)
            p[y * kHeadIconSize + x] = v;
    };
    if (c == 3) {
        // Yoshi: green head, big white snout, round eye.
        const uint32_t kGreen = 0xFF58C838u, kSnout = 0xFFF8F8F0u;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                if (inside(x, y, 7.5, 6.5, 5.5, 5.5)) set(x, y, kGreen);
                if (inside(x, y, 10.5, 10.0, 5.0, 3.6)) set(x, y, kSnout);
            }
        // red spikes on the back of the head
        set(2, 3, 0xFFE03030u); set(3, 2, 0xFFE03030u); set(2, 4, 0xFFE03030u);
        // eye: white oval, black pupil
        for (int y = 2; y <= 6; ++y)
            for (int x = 7; x <= 9; ++x) set(x, y, kWhite);
        set(8, 4, kOut); set(8, 5, kOut); set(9, 4, kOut); set(9, 5, kOut);
        set(12, 9, kOut);   // nostril
    } else {
        // face
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                if (inside(x, y, 8.0, 9.0, 6.0, 5.0)) set(x, y, kSkin);
        // cap dome and brim
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                if (y <= 7 && inside(x, y, 8.0, 7.0, 6.5, 6.0)) set(x, y, cap);
        for (int x = 3; x <= 14; ++x) { set(x, 7, cap); set(x, 8, cap); }
        // letter disc
        for (int y = 1; y <= 6; ++y)
            for (int x = 5; x <= 10; ++x)
                if (inside(x, y, 8.0, 3.8, 3.2, 3.0)) set(x, y, kWhite);
        const int li = c == 2 ? 2 : c;
        for (int r = 0; r < 5; ++r)
            for (int b = 0; b < 3; ++b)
                if (kLetter[li][r] & (4 >> b)) set(6 + b, 1 + r, letter);
        // eyes, nose, moustache
        set(6, 10, kOut); set(10, 10, kOut);
        set(8, 11, 0xFFE0A070u);
        for (int x = 5; x <= 11; ++x) if (x != 8) set(x, 12, kBrown);
        if (c == 2) { set(4, 11, kBrown); set(12, 11, kBrown); }   // Wario's big moustache
    }
    // one-pixel dark outline around the whole shape
    uint32_t src[kHeadIconSize * kHeadIconSize];
    std::memcpy(src, p, sizeof src);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            if (src[y * 16 + x] >> 24) continue;
            const int nx[4] = {x - 1, x + 1, x, x}, ny[4] = {y, y, y - 1, y + 1};
            for (int k = 0; k < 4; ++k)
                if (nx[k] >= 0 && nx[k] < 16 && ny[k] >= 0 && ny[k] < 16 &&
                    (src[ny[k] * 16 + nx[k]] >> 24)) {
                    p[y * 16 + x] = kOut;
                    break;
                }
        }
}

void build_all() {
    for (int c = 0; c < 4; ++c) build_one(c, g_px[c]);
    g_built = true;
}

}  // namespace

const uint32_t *head_icon(int character) {
    if (!g_built) build_all();
    return g_px[(character >= 0 && character <= 3) ? character : 0];
}

int head_icon_source(int) { return 0; }

}  // namespace port
