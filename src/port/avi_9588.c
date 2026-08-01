#include "aviplay.h"
#include "input.h"
#include "palcfg.h"
#include "platform.h"
#include "regression.h"

/*
 * The decoder and RIFF parser come from SDLPAL's GPL aviplay.c.  That source
 * is compiled with its public entry point renamed so this small bridge can
 * also offer AVI playback when the DOS data set is in use.  PAL98 movies are
 * optional files and do not change the game-data edition detected by SDLPAL.
 */
extern BOOL PAL9588_InternalPlayAVI(const char *path);

static unsigned g_presented_frames;
static unsigned g_exit_keys;

unsigned pal9588_avi_last_frame_count(void)
{
    return g_presented_frames;
}

unsigned pal9588_avi_last_exit_keys(void)
{
    return g_exit_keys;
}

BOOL PAL_PlayAVI(const char *path)
{
    BOOL enabled = gConfig.fEnableAviPlay;
    BOOL result;

    if (pal9588_regression_active() && path &&
        ((path[0] == '1' && path[1] == '.') ||
         (path[0] == '2' && path[1] == '.'))) {
        return TRUE;
    }

    pal9588_platform_wait_action_release();
    SDL_9588ResetInputState();
    PAL_ClearKeyState();
    gConfig.fEnableAviPlay = TRUE;
    g_presented_frames = 0;
    g_exit_keys = 0;
    result = PAL9588_InternalPlayAVI(path);
    g_exit_keys = g_InputState.dwKeyPress & (kKeyMenu | kKeySearch);
    gConfig.fEnableAviPlay = enabled;
    result = result && g_presented_frames != 0u;
    if (result) {
        pal9588_platform_wait_action_release();
        SDL_9588ResetInputState();
        PAL_ClearKeyState();
    }
    return result;
}

/* aviplay.c is compiled with VIDEO_DrawSurfaceToScreen redirected here. */
VOID PAL9588_DrawAVISurface(SDL_Surface *surface)
{
    if (!surface || !surface->pixels || !surface->format ||
        surface->format->BitsPerPixel != 16) {
        return;
    }
    ++g_presented_frames;
    pal9588_platform_present_rgb555(
        (const Uint16 *)surface->pixels,
        surface->w,
        surface->h,
        (int)surface->pitch / (int)sizeof(Uint16)
    );
}
