/* ui_art.h -- where the port's reworked UI pictures come from (run hunt8, lane UIMOD1).

   Every reworked button, menu, HUD and minimap picture is in ONE mod, "HD UI Textures", under hd_ui/<rel> (found by hal/mods.cpp:
   port_mods_ui_file). The port itself carries no such picture: no file in the repo, nothing embedded in the exe, nothing in the kit.
   ui_art_path asks the enabled mods and answers 1 with the full path of the file (a later enabled mod wins), or 0 when no mod
   provides it, which is every run with no such mod and every run in a lobby or match (mods are off there). Callers then draw the
   ROM's own art or a plain code-drawn skin. The smoke targets do not link hal/mods.cpp: the hook stays null there and the answer is 0. */
#ifndef UI_ART_H
#define UI_ART_H
#ifdef __cplusplus
extern "C" {
#endif
extern int (*port_ui_file_hook)(const char *rel, char *out, unsigned cap);   /* set by hal/mods.cpp from an initializer */
int ui_art_path(const char *rel, char *out, unsigned cap);
#ifdef __cplusplus
}
#endif
#endif
