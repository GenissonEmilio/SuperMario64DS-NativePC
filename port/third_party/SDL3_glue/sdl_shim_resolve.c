/* The lazy half of the SDL3 import shim (see gen_sdl_shim.py). The generated thunk
   pushes its table index and calls this; the answer is cached, so a function costs one
   LoadLibrary/GetProcAddress the first time SDL calls it and one table read after.
   Every DLL is loaded from the system directory by full path. A function that cannot be
   found answers 0 here; the generated thunk then returns the FAILURE VALUE OF THE
   FUNCTION'S OWN RETURN TYPE (E_FAIL for HRESULT, CR_FAILURE for CONFIGRET, ... see
   sdl_shim_failures.tsv) with the right stack cleanup, so a missing optional API reads as
   "this call failed" to SDL even where 0 would mean success. */
#include <windows.h>

extern const char *const sdlshim_names[];
extern const char *const sdlshim_dlls[];
extern const unsigned sdlshim_count;

static FARPROC g_proc[512];

FARPROC __cdecl sdlshim_resolve(unsigned i)
{
    if (i >= sdlshim_count || i >= 512) return 0;
    FARPROC p = g_proc[i];
    if (p) return p;
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    UINT (WINAPI *sysdir)(LPSTR, UINT) =
        (UINT (WINAPI *)(LPSTR, UINT))GetProcAddress(k32, "GetSystemDirectoryA");
    char path[MAX_PATH + 32];
    path[0] = 0;
    if (sysdir) {
        UINT n = sysdir(path, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) path[0] = 0;
        else { path[n] = (char)92;path[n + 1] = 0; }
    }
    {
        char *d = path;
        const char *s;
        while (*d) ++d;
        for (s = sdlshim_dlls[i]; *s; ) *d++ = *s++;
        *d = 0;
    }
    HMODULE m = GetModuleHandleA(sdlshim_dlls[i]);
    if (!m) m = LoadLibraryA(path);
    if (m) p = GetProcAddress(m, sdlshim_names[i]);
    g_proc[i] = p;
    return p;
}
