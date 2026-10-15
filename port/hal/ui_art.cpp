/* ui_art.cpp -- see ui_art.h (run hunt8, lane UIMOD1). */
#include "ui_art.h"

extern "C" {
int (*port_ui_file_hook)(const char *rel, char *out, unsigned cap) = 0;
int ui_art_path(const char *rel, char *out, unsigned cap)
{
    if (out && cap) out[0] = 0;
    return port_ui_file_hook ? port_ui_file_hook(rel, out, cap) : 0;
}
}
