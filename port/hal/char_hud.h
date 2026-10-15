/* hal/char_hud.h: the places the game shows "who you are", for a costume (run hunt8, lane CHARHUD1).

   While a custom character is the active skin (hal/char_registry.h), a mod may ship pictures for the game's own sprites:

     "character": { ...,
                    "hud_icon": "character/hud_icon.png",   the life counter icon (the head next to "x 5")
                    "map_icon": "character/map_icon.png",   the minimap player marker
                    "face":     "character/face.png" }      the star select portrait of the current character

   All three are optional and independent; a character that ships none shows the base character's art (the behaviour before this
   lane). They are ordinary PNG files (alpha is the cut-out). The port repaints the game's own OAM sprites: it reads the sprite
   list the ROM would draw for the base character (OAM::LIFE_ICONS, OAM::MM_PLAYER_ICONS, the star select portrait lists), maps
   the PNG onto the bounding box of those sprites and writes it into the sprites' own tiles and palette bank in OBJ VRAM, so
   the picture is drawn by the game's sprite path at the game's own position, scale and blink. Nothing is a screen rectangle. */
#ifndef PORT_HAL_CHAR_HUD_H
#define PORT_HAL_CHAR_HUD_H

#ifdef __cplusplus
extern "C" {
#endif

enum { PCH_LIFE = 0, PCH_MINIMAP = 1, PCH_STARSEL = 2, PCH_STARSEL_LIFE = 3, PCH_LIFE_ROM = 4 };   /* PCH_LIFE: the port's top-screen life copy (engine A); PCH_LIFE_ROM: the ROM's own leaf (engine by its state); PCH_STARSEL_LIFE: the star select's own counter */

/* Called by the host seat of the thing that draws the sprites, every frame, before it draws: repaints that element's tiles when the
   active skin ships a picture for it. A no-op (a few instructions) when there is no skin or no picture. */
void port_char_hud_apply(int what);

#ifdef __cplusplus
}
#endif
#endif
