// THE FOUR HEAD PICTURES for the player list (hal/player_list.h). Run mp1
// wave 56, lane PLIST1.
//
// A head is a 16 x 16 block of 0xAARRGGBB pixels (alpha 0 = transparent), one
// per character: 0 Mario, 1 Luigi, 2 Wario, 3 Yoshi. The painter scales it by
// the overlay scale and blends it. Source: see head_icons.cpp (the ROM's own
// HUD art when it decoded cleanly, otherwise four small hand-made heads).
#ifndef PORT_HAL_HEAD_ICONS_H
#define PORT_HAL_HEAD_ICONS_H

#include <cstdint>

namespace port {

enum : int { kHeadIconSize = 16 };

// Pixels of character `c` (0..3, anything else = Mario), row-major
// kHeadIconSize x kHeadIconSize. Never null. Stable for the process.
const uint32_t *head_icon(int character);

// 0 = hand-made fallback, 1 = decoded from the ROM's character-select art.
int head_icon_source(int character);

// Replace the hand-made heads with the ROM's own portraits where the files
// can be read. `path_for_id` maps a FAT file id to a readable path (hal/fs.cpp
// port_fs_catalog_path) or null. Returns how many of the four are now ROM art.
// Safe to call again (it only retries the ones still hand-made).
int head_icons_load_rom(const char *(*path_for_id)(unsigned file_id));

}  // namespace port

#endif  // PORT_HAL_HEAD_ICONS_H
