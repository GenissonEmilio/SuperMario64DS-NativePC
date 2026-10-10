// THE FOUR HEAD PICTURES. Run mp1 wave 56, lane PLIST1. See head_icons.h.
//
// TWO SOURCES, the better one winning per character:
//   (1) the ROM's own character-select portraits, read at run time from the
//       player's own game files through the caller's path lookup
//       (head_icons_load_rom): data/2D_cad/d_2d_player_select_<name>_8bit_ncg
//       and _ncl (file ids 148 luigi, 150 mario, 152 wario, 154 yoshi, the
//       palette is the next id). Each _ncg is one LZ10 stream of 16384 bytes
//       (the 0x4000 StarSel's InitResources decompresses the portrait to):
//       a 128 x 128 sheet of 8x8 8-bit tiles, 16 tiles a row, palette index 0
//       transparent, the bust in the top-left 80 x 80 and a lattice frame
//       beyond it (outside the crop). The bust is box-averaged down to 16 x 16
//       with its coverage kept as alpha. Nothing from the ROM is stored in the
//       repo or in the exe; a missing / short file leaves the hand-made head.
//   (2) hand-made heads, built once from a few shapes (a face ellipse, a cap,
//       a brim, eyes, a moustache, a letter on the cap), each with a one-pixel
//       dark outline.

#include "hal/head_icons.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace port {
namespace {

uint32_t g_px[4][kHeadIconSize * kHeadIconSize];
bool g_built = false;
int g_rom[4];

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

int head_icon_source(int character) {
    return (character >= 0 && character <= 3) ? g_rom[character] : 0;
}

namespace {

bool read_file(const char *path, unsigned char **out, long *len) {
    FILE *f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 8 || n > (1 << 20)) { std::fclose(f); return false; }
    unsigned char *b = (unsigned char *)std::malloc((size_t)n);
    if (!b) { std::fclose(f); return false; }
    const bool ok = std::fread(b, 1, (size_t)n, f) == (size_t)n;
    std::fclose(f);
    if (!ok) { std::free(b); return false; }
    *out = b;
    *len = n;
    return true;
}

// Nintendo LZ10 ("0x10", 24-bit size). Bounds-checked on both sides.
bool lz10(const unsigned char *s, long n, unsigned char *dst, long want) {
    if (n < 4 || s[0] != 0x10) return false;
    const long size = s[1] | (s[2] << 8) | (s[3] << 16);
    if (size != want) return false;
    long o = 4, d = 0;
    while (d < size) {
        if (o >= n) return false;
        const unsigned flags = s[o++];
        for (int k = 0; k < 8 && d < size; ++k) {
            if (flags & (0x80u >> k)) {
                if (o + 2 > n) return false;
                const unsigned x = ((unsigned)s[o] << 8) | s[o + 1];
                o += 2;
                const long len = (long)(x >> 12) + 3, disp = (long)(x & 0xfff) + 1;
                if (disp > d) return false;
                for (long i = 0; i < len && d < size; ++i, ++d) dst[d] = dst[d - disp];
            } else {
                if (o >= n) return false;
                dst[d++] = s[o++];
            }
        }
    }
    return true;
}

bool decode_portrait(const char *ncg, const char *ncl, uint32_t *out) {
    unsigned char *g = nullptr, *l = nullptr;
    long gn = 0, ln = 0;
    bool ok = false;
    static unsigned char tiles[16384];
    if (read_file(ncg, &g, &gn) && read_file(ncl, &l, &ln) && ln >= 512 &&
        lz10(g, gn, tiles, 16384)) {
        const int C = 80;                       // crop: the bust, not the frame
        const int B = C / kHeadIconSize;        // 5 source pixels per icon pixel
        for (int oy = 0; oy < kHeadIconSize; ++oy)
            for (int ox = 0; ox < kHeadIconSize; ++ox) {
                unsigned r = 0, gg = 0, b = 0, cov = 0;
                for (int j = 0; j < B; ++j)
                    for (int i = 0; i < B; ++i) {
                        const int x = ox * B + i, y = oy * B + j;
                        const unsigned idx =
                            tiles[((y >> 3) * 16 + (x >> 3)) * 64 + (y & 7) * 8 + (x & 7)];
                        if (!idx) continue;
                        const unsigned v = l[idx * 2] | ((unsigned)l[idx * 2 + 1] << 8);
                        r += (v & 31) * 255 / 31;
                        gg += ((v >> 5) & 31) * 255 / 31;
                        b += ((v >> 10) & 31) * 255 / 31;
                        ++cov;
                    }
                uint32_t px = 0;
                if (cov * 2 >= (unsigned)(B * B)) {      // more than half covered
                    const unsigned a = cov * 255 / (unsigned)(B * B);
                    px = (a << 24) | ((r / cov) << 16) | ((gg / cov) << 8) | (b / cov);
                }
                out[oy * kHeadIconSize + ox] = px;
            }
        ok = true;
    }
    std::free(g);
    std::free(l);
    return ok;
}

}  // namespace

int head_icons_load_rom(const char *(*path_for_id)(unsigned file_id)) {
    if (!path_for_id) return 0;
    if (!g_built) build_all();
    static const unsigned kNcg[4] = {150, 148, 152, 154};   // mario luigi wario yoshi
    int n = 0;
    for (int c = 0; c < 4; ++c) {
        if (g_rom[c]) { ++n; continue; }
        const char *a = path_for_id(kNcg[c]);
        const char *b = path_for_id(kNcg[c] + 1);
        if (!a || !b) continue;
        uint32_t tmp[kHeadIconSize * kHeadIconSize];
        if (decode_portrait(a, b, tmp)) {
            std::memcpy(g_px[c], tmp, sizeof tmp);
            g_rom[c] = 1;
            ++n;
        }
    }
    return n;
}


}  // namespace port
