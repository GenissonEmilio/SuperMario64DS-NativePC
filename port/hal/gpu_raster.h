/* THE OPTIONAL GRAPHICS-CARD RENDERER, stage 2 of the road that started with
 * hal/gpu_present.cpp.
 *
 * WHAT IT IS. "PresentBackend" moved the finished picture to the card.
 * "Renderer" moves the DRAWING of the 3D picture to the card: the OPAQUE pass
 * of ntr/gx.cpp -- which is nearly all of the fill -- is drawn into offscreen
 * colour, depth and polygon-ID targets, read straight back into the software
 * rasteriser's own buffers, and everything after that (the translucent and
 * shadow pass, the edge smoothing, the 2D compositor, the display capture, the
 * screenshots, the present) runs on exactly the frame it always saw.
 *
 *     key "Renderer"   0 the software rasteriser, which is the default and the
 *                      only byte-exact reference; 1 Direct3D 11.
 *     env SM64DS_RENDERER overrides the key.
 *
 * WHY. The game takes one fixed step per tick and never catches up, so a frame
 * over its budget is not a dropped picture, it is slow motion. The software
 * raster costs about 20 ms of a 33.3 ms budget at RenderScale 4 in a course and
 * about 73 ms on the title screen BEFORE filtering, anti-aliasing and a
 * smoothed model's extra triangles are added. The fill is the cost, and the
 * card does fill.
 *
 * THE WHOLE FRAME (run cardfull). The translucent polygons, the shadow volumes,
 * the toon and highlight polygons and the edge smoothing can be drawn on the
 * card too, into the same targets, so that one finished 3D picture is read back
 * instead of an opaque one that the processor then finishes. It is what the
 * setting does now; env SM64DS_RENDERER_FULL=0 is the escape hatch back to the
 * opaque-only card above; a device below Direct3D feature level 11_0, or any failure of the
 * whole-frame pass, drops back to the opaque-only card for the rest of the run
 * with one line, and a failure of the card itself drops back to the software
 * rasteriser as before. The in-process A/B (SM64DS_RENDERER_AB=1) compares the
 * whole frame in this mode.
 *
 * WHAT IT IS NOT. It is not byte-identical to the software path and cannot be:
 * two rasterisers with different fill rules and different float paths tie
 * differently on edges. The software path stays the default, stays the byte
 * gate, and does not move. With the key absent nothing here loads, allocates
 * or runs.
 *
 * ON ANY FAILURE -- no device, a refused call, a device removed by the driver,
 * a readback that will not map -- one plain line is printed and the software
 * opaque pass draws that same frame and every frame after it. The game never
 * exits, never stalls and never shows a half-drawn picture because of this
 * setting.
 */
#ifndef PORT_HAL_GPU_RASTER_H
#define PORT_HAL_GPU_RASTER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Read the setting and, when it is on, hand ntr/gx.cpp the backend. Call once
 * at boot, before any geometry, beside the other configure calls: which
 * rasteriser draws the frame decides which library is loaded and which device
 * exists, and a mid-run change would be a second code path nobody tests. With
 * the key off this returns having done nothing at all. */
void port_gpu_raster_configure(void);

/* 1 when the setting is on and the card has not fallen back. Diagnostics and
 * the perf line; nothing in the render path asks. */
int port_gpu_raster_active(void);

/* THE PICTURE HELD ON THE CARD (run cardfull, lane CARD2; the long form is the
   banner above cp_keep_ok in gpu_raster.cpp). 1 when the frame being finished
   is still on the card unread, which is when the present may composite it
   there instead of uploading the processor's copy. */
int port_gpu_card_held(void);

/* The Direct3D present (hal/gpu_present.cpp) has stood down: stop holding
   frames on the card for it. Called from its own fall-back, once. */
void port_gpu_raster_present_down(void);

/* Engine A's brightness fade for the held frame (evy 0..16, white 1 = to
   white), recorded instead of applied, because the composite applies it to the
   whole picture. Ignored when no frame is held. */
void port_gpu_card_set_fade(int evy, int white);

/* The brightness mask that fade is limited by, when the frame has one
   (hal/message_compositor.cpp, in the encoding its comment gives for a frame
   held on the card): `mask` stays valid until the next frame's compositor.
   Null means the fade takes every pixel. */
void port_gpu_card_set_mask(const unsigned char *mask, int stride, int shown3d,
                            int clear_opaque);

/* Composite the held frame with the overlay (the framebuffer, `stride_px`
   pixels to a row, alpha byte = ntr/gx.h's GX_OV_* verdict) into a card target
   and return a view of it (an ID3D11ShaderResourceView as void *), or null
   after the one plain line that says why the card cannot. */
void *port_gpu_card_compose(const unsigned int *overlay, int stride_px);

/* run hunt8, lane CARDSM1: a tick composited ahead of the pictures between two
   ticks keeps its finished picture in the composite target; this is its view
   (null when it is gone). port_gpu_card_fade_recorded: the held frame has an
   engine A fade waiting for its composite. */
void *port_gpu_card_retained_view(int slot);   /* slot 1 the tick, 2 an in-between */
/* run hunt8, lane GPUINTERP1: composite the frame just drawn and kept (a picture
   between two ticks) with the retained tick's overlay into the in-between
   target and return its view, or null (the caller then blends on the
   processor). */
void *port_gpu_card_compose_inter(const unsigned int *overlay, int stride_px);
int port_gpu_card_fade_recorded(void);

/* THE READBACK WAIT, summed in milliseconds over every frame the card has
 * drawn. This is the CPU side of the opaque pass: the copy out of the render
 * targets, the Map -- which is synchronous and therefore waits for the card
 * to finish -- and the copy of the covered pixels into the software
 * rasteriser's buffers. It is the same quantity the [renderer] cost line
 * calls "readback", and it is the cost the next stage of this road removes,
 * which is why the overlay shows it beside the card's own time rather than
 * folded into it. 0.0 with the setting off. */
double port_gpu_raster_readback_ms(void);

#ifdef __cplusplus
}
#endif

#endif  /* PORT_HAL_GPU_RASTER_H */
