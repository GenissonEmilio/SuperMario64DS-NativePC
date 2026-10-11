/* SDL3 pieces the gamepad backend never reaches, replaced by do-nothing versions so
   the exe carries no process-spawning, open-URL or Steam-storage code (and none of the
   Win32 names those pull in). The real files are filtered out of SDL3-static in
   sdl_build.cmake; this file is compiled INTO SDL3-static so it sees SDL's own headers.
   SDL_CreateProcess / SDL_OpenURL / SDL_OpenUserStorage still exist and fail with an
   error, exactly as SDL does on a platform without the feature. */
#include "SDL_internal.h"
#include "process/SDL_sysprocess.h"
#include "misc/SDL_sysurl.h"
#include "storage/SDL_sysstorage.h"

bool SDL_SYS_CreateProcessWithProperties(SDL_Process *process, SDL_PropertiesID props)
{
    (void)process; (void)props;
    return SDL_Unsupported();
}

bool SDL_SYS_KillProcess(SDL_Process *process, bool force)
{
    (void)process; (void)force;
    return SDL_Unsupported();
}

bool SDL_SYS_WaitProcess(SDL_Process *process, bool block, int *exitcode)
{
    (void)process; (void)block; (void)exitcode;
    return SDL_Unsupported();
}

void SDL_SYS_DestroyProcess(SDL_Process *process)
{
    (void)process;
}

bool SDL_SYS_OpenURL(const char *url)
{
    (void)url;
    return SDL_Unsupported();
}

static SDL_Storage *NoSteam_create(const char *org, const char *app, SDL_PropertiesID props)
{
    (void)org; (void)app; (void)props;
    return NULL; /* SDL_OpenUserStorage then tries its generic driver */
}

UserStorageBootStrap STEAM_userbootstrap = {
    "steam",
    "Steam user storage (not built)",
    NoSteam_create
};
