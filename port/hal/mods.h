/* hal/mods.h: the mod list (run mods1, lane MODFILES1).

   settings.json's "EnabledMods" is the ordered list of installed mods that are
   on. mods.cpp reads it ONCE, at boot (the first call to any function below
   latches it; hdtex_configure makes that first call during start-up), checks
   each id and mod.json, and keeps the ones that passed. This header is the
   whole read API for the pieces that act on a mod: the file layer
   (hal/mods.cpp itself), the HD texture index (ntr/hdtex.cpp, through a hook
   because the ntr libraries link no hal code), the palette lookup
   (hal/fs_mods.cpp) and wave 2's plugin and sound lanes.

   ORDER: index 0 is the FIRST id in EnabledMods and index count-1 the LAST.
   A later mod wins over an earlier one, so walk from count-1 down to 0 to ask
   "who answers first". Code mods (kind "code") are in the list too: a code mod
   may also carry files/, textures_hd/ and palettes/ and gets them applied the
   same as a files mod. Whether its plugin is loaded (api and game checks) is
   the plugin lane's call.

   SM64DS_MODS=0 in the environment empties the list (a bug report switch).

   Only the two game targets link hal/mods.cpp, so these functions do not exist
   in the narrow smoke targets: every caller is either in a game-only file or
   goes through a hook that stays null there. */
#ifndef PORT_HAL_MODS_H
#define PORT_HAL_MODS_H

#ifdef __cplusplus
extern "C" {
#endif

/* How many mods are on and passed validation. */
int port_mods_count(void);
/* The i-th enabled mod (0 = lowest priority). Null when i is out of range. */
const char *port_mods_id(int i);
/* The mod's folder, no trailing slash: <bundle>/mods/<id>. */
const char *port_mods_folder(int i);
/* "files" or "code", straight from mod.json. */
const char *port_mods_kind(int i);
/* mod.json's "game" and "api" fields as the launcher wrote them ("" / 0 when
   absent); only code mods need them. */
const char *port_mods_game(int i);
int port_mods_api(int i);
/* 1 when this run is multiplayer: the launcher's lobby / VS match environment
   (any of SM64DS_COMMS_ROLE/RELAY/HOST/CODE/SLOT, SM64DS_VS_MAP/MODE/PLAYERS
   non-empty, SM64DS_PARTY nonzero) or the Adventure hub (settings.json
   AdventureGhosts, SM64DS_ADVENTURE). Mods are for single player only: in that
   case the mod list is empty for the whole run and the match runs the plain game. */
int port_mods_multiplayer(void);
/* The bundle folder mods/ sits in (the folder settings.json was found in). */
const char *port_mods_bundle(void);

#ifdef __cplusplus
}
#endif
#endif
