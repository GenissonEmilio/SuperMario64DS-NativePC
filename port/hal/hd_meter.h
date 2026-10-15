/* hd_meter.h -- the HD power meter, as a mod's art (run hunt8, lane HDMETER1).

   The HUD's power meter (HUD::RenderHealthMeter: a pie sprite picked by the ROM's displayed wedge count plus the hand frame with
   the POWER letters, both at (0x80, mHealthMeterY)) is drawn from a mod's redrawn art when an enabled mod carries it, and from
   the DS sprites otherwise (no mod = today's meter exactly). Position, size, fill, label and motion all stay the ROM's: this only
   changes the pixels the sprite's footprint shows; message_compositor.cpp does the drawing (see "THE HD POWER METER" there).

   WHERE THE ART LIVES. In the "HD UI Textures" mod (any enabled mod, any kind, or a part folder a setting switches on; looked up with port_mods_ui_file), under
       hd_ui/hud/power_meter/meter_health<N>_with_label.png       N = 0..8, the DS pixel grid: 43 x 42, content 40 x 40 at the top left
       hd_ui/hud/power_meter/meter_health<N>_with_label_4x.png    the same at 4x: 172 x 168, content 160 x 160
   each the hand frame, the POWER label and the pie for that wedge count in one picture. A later mod in EnabledMods wins over an
   earlier one, per file. The 4x files are used when a DS pixel is two or more host pixels, the 1x files otherwise; a mod that
   carries only one of the two sizes is used for both. Mods are single player only (port_mods_multiplayer): in a lobby or a match
   no mod is on, so the DS meter shows.

   The compositor reaches this through a hook (port_hd_meter_art_hook) that hd_meter.cpp sets from an initializer, because only
   the two game targets link hal/mods.cpp; the smoke targets leave it null and draw the DS meter. */
#ifndef HD_METER_H
#define HD_METER_H

struct HdMeterArt {
    int k;                         /* art pixels per DS pixel: 1 or 4 */
    int w, h;                      /* the image's own size */
    const unsigned char *rgba;     /* w * h * 4, straight alpha */
};

/* the art for wedge count `health` (0..8) at k = 1 or 4, from the enabled mods; null when no mod provides it (the DS sprites stay) */
extern "C++" const HdMeterArt *hd_meter_art(int health, int k);

#endif
