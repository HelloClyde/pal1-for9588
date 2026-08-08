#include "main.h"
#include "resampler.h"

BOOL UTIL_GetScreenSize(DWORD *width, DWORD *height)
{
    if (width) *width = 320;
    if (height) *height = 240;
    return TRUE;
}

BOOL UTIL_IsAbsolutePath(LPCSTR filename)
{
    if (!filename || !filename[0]) return FALSE;
    if (PAL_IS_PATH_SEPARATOR(filename[0])) return TRUE;
    return filename[0] && filename[1] == ':';
}

INT UTIL_Platform_Init(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    gConfig.fLaunchSetting = FALSE;
    gConfig.iAudioChannels = 1;
    gConfig.iSampleRate = 22050;
    gConfig.iOPLSampleRate = 22050;
    gConfig.wAudioBufferSize = 2048;
    /*
     * The DOS VOC archive contains many non-integer source rates (the night
     * watch drum, sound 93, is 6000 Hz). The desktop default is the 32-tap
     * SINC resampler, whose floating-point work stalls the single game/input
     * thread on the soft-float JZ4730 while an effect is active. Linear
     * interpolation keeps the original effect timing and is light enough to
     * mix in real time without making keys or touch unresponsive.
     */
    gConfig.iResampleQuality = RESAMPLER_QUALITY_LINEAR;
    gConfig.eMusicType = MUSIC_RIX;
    gConfig.eOPLType = OPL_DOSBOX_NEW;
    gConfig.fUseSurroundOPL = FALSE;
    return 0;
}

VOID UTIL_Platform_Quit(VOID) {}
