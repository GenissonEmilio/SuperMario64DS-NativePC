// Keep the game's hot threads on performance cores (run hunt8, lane PCORE1).
//
// On a hybrid CPU (Intel 12th gen and later, some AMD) Windows can park a
// process on its efficiency cores at low clocks: EcoQoS / execution-speed
// throttling (Efficiency mode, a power plan, an unfocused or minimized
// window). The logic thread then misses ticks and the in-between pictures
// starve. This opts the process and the hot threads out of that throttling,
// asks MMCSS for the "Games" class on them, and on a CPU that reports more
// than one efficiency class pins the logic and presenter threads to the
// fastest class. Every entry point is reached through GetProcAddress, so the
// exe's import list does not change. SM64DS_PCORE=0 restores the old
// behaviour (timer-resolution opt-out only).
#pragma once

enum PcoreRole { PCORE_LOGIC = 0, PCORE_PRESENTER = 1, PCORE_WORKER = 2 };

// Process level opt-out plus the logic (calling) thread. Prints the [cpu] line.
// Idempotent; call once from the main thread at boot.
void pcore_boot(void);

// Call on the thread itself, as the first thing it does. Safe before
// pcore_boot (it then does nothing) and with the kill switch set.
void pcore_thread_enter(PcoreRole role);

// Call on the same thread just before it ends. Reverts MMCSS.
void pcore_thread_leave(void);

// 1 once pcore_boot ran and the kill switch is not set.
int pcore_active(void);
