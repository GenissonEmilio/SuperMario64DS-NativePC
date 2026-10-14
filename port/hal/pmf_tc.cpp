/* hal/pmf_tc.cpp -- see hal/pmf_tc.h. */
#include "pmf_tc.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
struct Entry { const void *flat; void *stub; };
Entry g_ent[4096];
int g_n;
unsigned char *g_page;
size_t g_used;
const size_t kPage = 65536;
}

extern "C" void *port_pmf_tc(const void *flat)
{
    if (!flat)
        return 0;
    for (int i = 0; i < g_n; ++i)
        if (g_ent[i].flat == flat)
            return g_ent[i].stub;
    if (g_n >= 4096) {
        std::fprintf(stderr, "FATAL: port_pmf_tc table full\n");
        std::abort();
    }
    if (!g_page || g_used + 16 > kPage) {
        g_page = (unsigned char *)VirtualAlloc(0, kPage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!g_page) {
            std::fprintf(stderr, "FATAL: port_pmf_tc: no executable page\n");
            std::abort();
        }
        g_used = 0;
    }
    unsigned char *p = g_page + g_used;
    g_used += 16;
    /* push ecx ; call rel32 flat ; add esp,4 ; ret */
    p[0] = 0x51;
    p[1] = 0xE8;
    const int rel = (int)((const unsigned char *)flat - (p + 6));
    std::memcpy(p + 2, &rel, 4);
    p[6] = 0x83; p[7] = 0xC4; p[8] = 0x04;
    p[9] = 0xC3;
    g_ent[g_n].flat = flat;
    g_ent[g_n].stub = p;
    ++g_n;
    return p;
}
