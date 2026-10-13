/* hal/sdat_mods.cpp: run mods1, lane MODSOUND1.

   hal/sdat/sdat.cpp applies each enabled mod's sound/ folder to the SDAT
   archive, but it is also built into targets that do not link hal/mods.cpp.
   So it reads the mod list through three function pointers, and this file
   (compiled only into the two game targets, beside hal/mods.cpp) fills them
   before main. The pointers are plain null-initialised globals, so the order
   of static construction does not matter: sdat_init runs long after. */
#include "mods.h"
#include "host_settings.h"

extern "C" {
extern int (*g_sdat_mods_count)(void);
extern const char *(*g_sdat_mods_id)(int);
extern const char *(*g_sdat_mods_folder)(int);
extern int (*g_sdat_audio_quality)(void);
}

namespace {
struct SdatModsInstall {
    SdatModsInstall()
    {
        g_sdat_mods_count = port_mods_count;
        g_sdat_mods_id = port_mods_id;
        g_sdat_mods_folder = port_mods_folder;
        g_sdat_audio_quality = host_setting_audio_quality;   // lane AUDIOHQ1: AudioQuality
    }
} g_install;
}
