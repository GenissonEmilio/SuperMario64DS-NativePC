/* hal/mod_hooks.h -- the three seams the plugin loader (hal/mod_plugins.cpp,
   run mods1, lane MODPLUGIN1) hangs on. Each is a plain function pointer that
   stays null unless the loader installs it, so the code that calls them
   (hal/actor_registry.cpp, hal/level_boot.cpp) costs one load and one test
   when no plugin is loaded, and the smoke targets, which link no loader, need
   nothing. They are selectany so every translation unit that mentions one
   shares a single zero-initialised copy and no source file has to own them. */
#ifndef PORT_HAL_MOD_HOOKS_H
#define PORT_HAL_MOD_HOOKS_H

#ifdef __cplusplus
extern "C" {
#endif

/* once per loop frame, at the end of the frame's actor work (the level loop
   and the scene loop both) */
__declspec(selectany) void (*port_mod_hook_frame)(void) = 0;
/* a level has finished booting (end of port_stage_a_boot) */
__declspec(selectany) void (*port_mod_hook_level)(int level_id) = 0;
/* the actor registry has just (re)written the spawn table: every level boot */
__declspec(selectany) void (*port_mod_hook_registry)(void) = 0;

/* hal/actor_registry.cpp: the registry's own factory for an actor id, or 0
   when the id is not a registered class. */
void *port_actor_registry_stock_factory(unsigned actor_id);

#ifdef __cplusplus
}
#endif
#endif
