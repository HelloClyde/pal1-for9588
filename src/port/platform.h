#ifndef PAL9588_PLATFORM_H
#define PAL9588_PLATFORM_H

#include "SDL.h"

int pal9588_platform_open(void);
void pal9588_platform_close(void);
void pal9588_platform_present(const SDL_Surface *surface);
void pal9588_platform_present_rgb555(
    const Uint16 *pixels, int width, int height, int pitch_pixels
);
void pal9588_platform_pump(void);
unsigned pal9588_platform_keys(void);
void pal9588_platform_wait_action_release(void);
int pal9588_platform_detached(void);
Uint32 pal9588_platform_ticks(void);
void pal9588_platform_delay(Uint32 milliseconds);

#endif
