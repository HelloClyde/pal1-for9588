#include "main.h"

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
    gConfig.eMusicType = MUSIC_RIX;
    gConfig.eOPLType = OPL_DOSBOX_NEW;
    gConfig.fUseSurroundOPL = FALSE;
    return 0;
}

VOID UTIL_Platform_Quit(VOID) {}
