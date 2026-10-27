/* Lane EGG1: the title's small-logo clip (see hal/egg_audio.cpp). */
#ifndef SM64DS_PORT_EGG_AUDIO_H
#define SM64DS_PORT_EGG_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the clip once: 1 started, 0 refused (it is already sounding or asked for), -1 no clip (the file is
   missing or unreadable; the reason was logged once). */
int egg_audio_play(void);
/* 1 from a start until the clip's last frame went out. */
int egg_audio_busy(void);
/* The mouse's left button: 0 up, 1 down with the cursor in the foreground window's client pixels, -1 down without a point. */
int egg_mouse_read(int *cx, int *cy);

#ifdef __cplusplus
}
#endif

#endif
