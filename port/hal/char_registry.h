/* hal/char_registry.h: custom characters (run hunt8, lane CHARSLOT1).

   A custom character is a COSTUME on one of the four retail characters. It
   plays, hits and sounds like its BASE (mario, luigi, wario or yoshi); only the
   body model, the cap head and the no-cap head differ. The ROM only ever sees
   the base id, so the four retail slots and every table behind them are
   untouched and a save stays a valid retail save.

   A character mod declares one in its mod.json:

     "character": { "key": "purple", "base": "luigi", "name": "Purple Luigi",
                    "model": "character/body.bmd",
                    "headCap": "character/head_cap.bmd",
                    "headNoCap": "character/head_nocap.bmd",
                    "metal": "character/metal.bmd" }       (metal: optional)

   key, base, name and model are required. The files stay DORMANT in the mod's
   character/ folder: they are not part of the mod's files/ layer and replace
   nothing until that character is chosen. The full key is "<modid>:<key>".

   WHEN A SKIN APPLIES. The file layer is boot-latched (hal/fs_mods.cpp says why:
   the file cache and the card's patched range are built once), so the active
   skin is chosen at process start by SM64DS_CHAR_SKIN=<full key>. The debug
   menu's character row relaunches the current level with that variable set or
   cleared. Registered at boot, applied through hal/mods.cpp's files layer, so a
   skin gets its LZ77 handling, claims and card patches for free.

   Multiplayer: the mod list is empty in every multiplayer path, so no character
   registers and the base character is forced; the active skin is also refused
   here under port_mods_multiplayer. */
#ifndef PORT_HAL_CHAR_REGISTRY_H
#define PORT_HAL_CHAR_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

/* hal/mods.cpp offers every mod that passed validation, with its mod.json text. */
void port_char_offer(const char *mod_id, const char *folder, const char *mod_json);

int port_char_count(void);                 /* custom characters registered */
const char *port_char_key(int i);          /* "<modid>:<key>" */
const char *port_char_name(int i);
int port_char_base(int i);                 /* 0 mario, 1 luigi, 2 wario, 3 yoshi */
/* CHARMENU1, the picker's art: character.preview / character.icon (a png inside the mod; the preview defaults to the mod's preview.png), null
   when there is none; the credits line is mod.json's "credits" strings (else its "authors") joined with "; ", "" when there are none. */
const char *port_char_preview(int i);
const char *port_char_icon(int i);
const char *port_char_credits(int i);
/* CHARHUD1 (hal/char_hud.h): PNG files for the HUD life icon, the minimap marker and the star select portrait; null when the mod ships none. */
const char *port_char_hud_icon(int i);
const char *port_char_map_icon(int i);
const char *port_char_face(int i);

/* The active skin: index, or -1 for the plain four. Resolved once. */
int port_char_active(void);

/* The active skin's replacement files: k = 0.. until 0 is returned. rom is the
   ROM path being replaced (as under extracted/dsd/files/), disk the mod's file. */
int port_char_skin_file(int k, const char **rom, const char **disk);

#ifdef __cplusplus
}
#endif
#endif
