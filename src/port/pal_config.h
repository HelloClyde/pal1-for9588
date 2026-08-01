#ifndef PAL_9588_CONFIG_H
#define PAL_9588_CONFIG_H

#include <sys/time.h>

/* The 9588 filesystem ABI consumes raw GBK/ASCII byte strings. */
#define PAL_9588_DATA_PATH            "A:\\\xd3\xa6\xd3\xc3\\\xca\xfd\xbe\xdd\\PAL\\"
#define PAL_PREFIX                    PAL_9588_DATA_PATH
#define PAL_SAVE_PREFIX               PAL_9588_DATA_PATH
#define PAL_CONFIG_PREFIX             PAL_9588_DATA_PATH
#define PAL_PATH_SEPARATORS           "\\/"
#define PAL_IS_PATH_SEPARATOR(c)       ((c) == '\\' || (c) == '/')
#define PAL_FILESYSTEM_IGNORE_CASE     1

#define PAL_DEFAULT_WINDOW_WIDTH       320
#define PAL_DEFAULT_WINDOW_HEIGHT      240
#define PAL_DEFAULT_FULLSCREEN_HEIGHT  240
#define PAL_VIDEO_INIT_FLAGS           (SDL_SWSURFACE | SDL_FULLSCREEN)
#define PAL_SDL_INIT_FLAGS             (SDL_INIT_VIDEO | SDL_INIT_AUDIO)

#define PAL_PLATFORM                   "BBK 9588"
#define PAL_CREDIT                     "SDLPAL for 9588 contributors"
#define PAL_PORTYEAR                   "2026"

#define PAL_HAS_JOYSTICKS              0
#define PAL_HAS_MOUSE                  0
#define PAL_HAS_TOUCH                  0
#define PAL_HAS_MP3                    0
#define PAL_HAS_OGG                    0
#define PAL_HAS_SDLCD                  0
#define PAL_HAS_NATIVEMIDI             0
#define PAL_HAS_CONFIG_PAGE            0
#define PAL_HAS_PLATFORM_SPECIFIC_UTILS 1
#define PAL_SCALE_SCREEN               FALSE
#define PAL_FORCE_UPDATE_ON_PALETTE_SET 1

void PAL9588_FatalOutput(const char *message);
#define PAL_FATAL_OUTPUT(message) PAL9588_FatalOutput(message)

#endif
