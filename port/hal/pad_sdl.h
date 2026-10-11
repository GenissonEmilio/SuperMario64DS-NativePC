/* THE SDL3 GAMEPAD LAYER: PlayStation, Switch Pro, 8BitDo, generic and Xbox pads
 * through SDL's gamepad API (its HIDAPI drivers and its built-in mapping database),
 * answered in XInput's own shape so nothing downstream of hal/pad_backend.cpp moves.
 *
 * This file is only the SDL half. hal/pad_backend.cpp owns the worker thread, the
 * order of the backends (SDL first, then XInput, then DirectInput as the fallback) and
 * the snapshot the game thread copies; it calls these four functions on that worker and
 * nowhere else. SDL is initialised, pumped and shut down on ONE thread, which is what
 * SDL's Windows joystick code needs (its device-change window belongs to the thread that
 * made it), and it keeps every slow SDL call (HIDAPI enumeration, WGI start-up) off the
 * game thread.
 *
 * Compiled only into the two shipped game targets (PORT_PAD_SDL). SDL3 is linked as a
 * static library and reaches Windows through hal-free lazy thunks, so the exe's import
 * table stays KERNEL32 only; see third_party/SDL3_glue/gen_sdl_shim.py.
 *
 * MAPPING (the same as an Xbox pad through XInput today):
 *   south = A, east = B, west = X, north = Y        (POSITIONAL: SDL3 names buttons by
 *       position, so a Nintendo pad's bottom button, Nintendo-labelled B, is south = A,
 *       exactly the current Switch Pro table in pad_backend.cpp);
 *   shoulders, stick clicks, back, start, the d-pad;
 *   triggers 0..32767 -> 0..255; sticks -32768..32767 -> +-32767 with Y UP (SDL's Y is
 *   down-positive, flipped here). The guide button has no bit in the XInput mask and is
 *   not reported, as XInputGetState never reported it either.
 */
#ifndef PORT_PAD_SDL_H
#define PORT_PAD_SDL_H

#include "hal/pad_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start SDL on the calling thread (the pad worker). Returns 1 when the gamepad
   subsystem is up. learned_fn(vid, pid) answers nonzero for a pad the player taught a
   layout (settings.json PadLayouts): SDL leaves that pad alone so the DirectInput
   backend, which that layout is written for, keeps it. Never blocks on a pad. */
int pad_sdl_init(int (*learned_fn)(unsigned vid, unsigned pid));

/* Pump SDL's events (hot-plug, activity) and read the pad the player is using: the
   most recently pressed or pushed of the connected ones, the first until then. Returns
   1 with *out filled and desc set to the "[pad]" text when one is bound, else 0.
   Worker only; cheap enough for a 4 ms cadence. */
int pad_sdl_pump(PortPadState *out, char *desc, int desc_n);

/* How many gamepads SDL has open, and the nth one's state: the seam a later
   multi-player change reads. Slot order is the order they were opened. */
int pad_sdl_count(void);
int pad_sdl_state(int slot, PortPadState *out);

void pad_sdl_quit(void);

/* ---- test hooks, off unless the environment asks (see pad_sdl.cpp) ---------------
   SM64DS_PAD_SDL_VIRTUAL=<xbox|ds4|ds5|pro|8bitdo|generic> attaches a virtual SDL
   gamepad with that pad's vid:pid and makes SDL ignore every real pad;
   SM64DS_PAD_SDL_SCRIPT=<hexmask>@<f0>[-<f1>],... drives it with the XInput mask on
   frame numbers counted by port_pad_poll, in lock-step with the game thread (the pad
   worker applies each step and publishes before the poll returns). */
int pad_sdl_virtual_wanted(void);
int pad_sdl_script_wanted(void);
void pad_sdl_script_apply(int frame);        /* worker: set the virtual pad for frame */

/* The sweep the proof rows print: for the given virtual pad type, presses every button
   and moves every axis through SDL and reports the XInput-shaped answer. Returns the
   number of mismatches (0 = pass). FOR A TEST PROCESS: initialises SDL itself. */
int pad_sdl_selftest_sweep(const char *type, int verbose);

#ifdef __cplusplus
}
#endif
#endif
