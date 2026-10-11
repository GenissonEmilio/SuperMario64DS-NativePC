/* THE PRESENTER THREAD (run hunt8, lane PRESENTER1).
 *
 * WHAT IT FIXES. Before this, the thread that runs the game tick also handed
 * every picture to the swap chain. A tick takes 7 to 14 ms of work, so each
 * tick left at least one refresh of a 144 Hz display with nothing new, and no
 * pacing tweak could close that: the same thread cannot both be inside the
 * tick and be at the display's next refresh.
 *
 * WHAT IT IS NOW. The game thread still makes every picture (the tick's own
 * and the pictures drawn between two ticks) and draws each finished one,
 * through the same quad, fit and filter as always, into a texture of a small
 * ring the presenter owns. It then publishes that texture, with the display
 * moment the picture is for, through a short queue and goes back to the game.
 * The presenter thread owns the swap chain: on its own clock (Present(1) when
 * VSync is on, a high-resolution timer grid at the refresh rate when it is
 * off) it takes the queued picture made for that refresh (see the top of
 * presenter.cpp), or keeps the last one if there is none, copies it into the
 * swap chain's back buffer and presents. It never waits for the game thread
 * and the game thread never waits for it.
 *
 * THE PICTURE IS NEVER HALF MADE. A texture is published only after its draw
 * has been issued, and the immediate context executes in submission order, so
 * the presenter's copy always sees a whole picture. A keyed mutex on each
 * texture keeps the game from drawing into one the presenter is copying.
 *
 * THREADING OF THE CARD. No context is shared. The game thread uses only its
 * own device and context (the ring draw, the keyed-mutex hand-over); the
 * presenter thread uses only its own device and context (the copy out of the
 * ring and the Present). The textures are shared memory, and a keyed mutex on
 * each one decides who may touch it. The game thread flushes its context just
 * before it releases the key, so the picture's writes are on the card by then.
 *
 * OFF: SM64DS_PRESENTER=0 puts the old path back (the game thread draws into
 * the back buffer and presents). Any failure of the thread (a lost device, a
 * bad Present) is reported to the game thread, which falls back to GDI the way
 * it always has.
 *
 * Every Windows entry point it needs that the exe did not already import is
 * reached through GetProcAddress, so the import table does not move.
 */
#ifndef PORT_HAL_PRESENTER_H
#define PORT_HAL_PRESENTER_H

#ifdef __cplusplus
extern "C" {
#endif

/* 1 unless SM64DS_PRESENTER=0 (cached). */
int presenter_enabled(void);

/* The presenter's own Direct3D device (same adapter as dev1), made on first
   call: the swap chain is created on it, so the Present, which holds its
   device's lock while it queues the flip and waits for the display, never
   holds the game thread's. Null when it cannot be made (the old path stays). */
void *presenter_swap_device(void *dev1);
/* Release it, after the swap chain is released (process exit). */
void presenter_shutdown(void);

/* Start the thread on this device context and swap chain (the sizes are the
   swap chain's). interval_mode 1 = Present(1) paced by the display, 0 = timer
   grid at grid_hz with Present(0). Returns 1 when it is running. */
int presenter_begin(void *dev, void *ctx, void *swapchain, int w, int h,
                    int interval_mode, double grid_hz);
/* Move the timer grid to grid_hz while running (a no-op in Present(1) mode
   apart from remembering the period). Game thread only. */
void presenter_set_grid_hz(double grid_hz);
/* Stop the thread and release its textures. Safe to call any time, any
   number of times, from the game thread only. */
void presenter_end(void);
int presenter_running(void);
/* QPC of the presenter's latest turn (about one per refresh), 0 when not running. */
long long presenter_phase_qpc(void);

/* The render target view (ID3D11RenderTargetView *) to draw the next picture
   into; null when not running. Then presenter_publish(repeat) hands it over.
   `repeat` says the picture is the same as the one published before (only for
   the statistics: it is still shown). */
void *presenter_target(void);
void *presenter_target_texture(void);
void presenter_publish(int repeat);

/* TIMED PICTURES (run hunt8, lane PRESENTER1B). The game thread says which
   display moment (QPC) the next published picture is made for; the presenter
   shows, at each refresh, the picture made for that refresh. 0 = no moment,
   show it at once; -1 = right after the last picture still waiting (the
   tick's own picture following the ones drawn ahead of it). */
void presenter_set_target(long long qpc_target);
/* The display delay in milliseconds (0 unless asked for), and whether the
   presenter takes timed pictures. */
double presenter_delay_ms(void);
int presenter_timed(void);
/* One tick's own work in milliseconds, once per tick: the display delay follows it. */
void presenter_note_work(double ms);
/* MENUPACE1: the scene path's time from a tick's start to the end of its first in-between; the display delay follows it too. */
void presenter_note_ready(double ms);
/* Free textures in the ring, for drawing ahead. */
int presenter_room(void);

/* 1 when a picture of this swap-chain size is already published, so a repeat
   of it can be skipped altogether. */
int presenter_have_picture(int w, int h);

/* Nonzero (an HRESULT) once the thread met a failure. */
long presenter_error(void);

/* Statistics shared with the old path: one call per swap-chain present,
   after Present returned. new_pic is 0 for a repeat of the previous picture.
   t_pub is the QPC at which the picture was complete. */
void presenter_stat_note(long long t_pub, int new_pic);
int presenter_stats_on(void);
/* Test only (stats on): the camera eye the next published picture was made
   from, so a run can tell how far the view moved between two shown pictures. */
void presenter_note_cam(const float eye[3]);

/* Test: FNV hash of a texture's pixels (staging readback), 0 when off. */
unsigned presenter_hash_texture(void *dev, void *ctx, void *tex, int who);
int presenter_hash_on(void);
void presenter_hash_log(const char *tag, long long serial, unsigned hash);

#ifdef __cplusplus
}
#endif

#endif
