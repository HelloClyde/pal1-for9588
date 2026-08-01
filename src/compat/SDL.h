#ifndef PAL9588_MINI_SDL_H
#define PAL9588_MINI_SDL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t Uint8;
typedef int8_t Sint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint16_t SDL_AudioFormat;

#define SDL_MAJOR_VERSION 1
#define SDL_MINOR_VERSION 2
#define SDL_PATCHLEVEL 15
#define SDL_VERSION_ATLEAST(x, y, z) 0
#define SDLCALL
#define SDL_INLINE inline

#define SDL_INIT_TIMER       0x00000001u
#define SDL_INIT_AUDIO       0x00000010u
#define SDL_INIT_VIDEO       0x00000020u
#define SDL_INIT_JOYSTICK    0x00000200u
#define SDL_INIT_NOPARACHUTE 0x00100000u

#define SDL_SWSURFACE  0x00000000u
#define SDL_HWSURFACE  0x00000001u
#define SDL_FULLSCREEN 0x80000000u
#define SDL_RESIZABLE  0x00000010u
#define SDL_RLEACCEL   0x00004000u
#define SDL_LOGPAL     0x01
#define SDL_PHYSPAL    0x02
#define SDL_ENABLE     1
#define SDL_DISABLE    0
#define SDL_MIX_MAXVOLUME 128

#define AUDIO_U8      0x0008
#define AUDIO_S16LSB  0x8010
#define AUDIO_S16SYS  AUDIO_S16LSB
#define AUDIO_S16     AUDIO_S16LSB

typedef struct SDL_Color {
    Uint8 r, g, b, unused;
} SDL_Color;

typedef struct SDL_Palette {
    int ncolors;
    SDL_Color *colors;
} SDL_Palette;

typedef struct SDL_PixelFormat {
    SDL_Palette *palette;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint8 Rloss, Gloss, Bloss, Aloss;
    Uint8 Rshift, Gshift, Bshift, Ashift;
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint32 colorkey;
    Uint8 alpha;
} SDL_PixelFormat;

typedef struct SDL_Rect {
    Sint16 x, y;
    Uint16 w, h;
} SDL_Rect;

typedef struct SDL_Surface {
    Uint32 flags;
    SDL_PixelFormat *format;
    int w, h;
    Uint16 pitch;
    void *pixels;
    SDL_Rect clip_rect;
    int refcount;
    int owns_pixels;
} SDL_Surface;

typedef struct SDL_VideoInfo {
    Uint32 hw_available : 1;
    Uint32 wm_available : 1;
    Uint32 unused_bits1 : 6;
    Uint32 unused_bits2 : 1;
    Uint32 blit_hw : 1;
    Uint32 blit_hw_CC : 1;
    Uint32 blit_hw_A : 1;
    Uint32 blit_sw : 1;
    Uint32 blit_sw_CC : 1;
    Uint32 blit_sw_A : 1;
    Uint32 blit_fill : 1;
    Uint32 unused_bits3 : 16;
    Uint32 video_mem;
    SDL_PixelFormat *vfmt;
    int current_w;
    int current_h;
} SDL_VideoInfo;

typedef int SDLKey;
typedef Uint16 SDLMod;

typedef struct SDL_keysym {
    Uint8 scancode;
    SDLKey sym;
    SDLMod mod;
    Uint16 unicode;
} SDL_keysym;

typedef struct SDL_KeyboardEvent {
    Uint8 type;
    Uint8 which;
    Uint8 state;
    SDL_keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_MouseButtonEvent {
    Uint8 type, which, button, state;
    Uint16 x, y;
} SDL_MouseButtonEvent;

typedef struct SDL_JoyAxisEvent {
    Uint8 type, which, axis, padding1;
    Sint16 value;
} SDL_JoyAxisEvent;

typedef struct SDL_JoyHatEvent {
    Uint8 type, which, hat, value;
} SDL_JoyHatEvent;

typedef struct SDL_JoyButtonEvent {
    Uint8 type, which, button, state;
} SDL_JoyButtonEvent;

typedef struct SDL_ResizeEvent {
    Uint8 type;
    int w, h;
} SDL_ResizeEvent;

typedef union SDL_Event {
    Uint8 type;
    SDL_KeyboardEvent key;
    SDL_MouseButtonEvent button;
    SDL_JoyAxisEvent jaxis;
    SDL_JoyHatEvent jhat;
    SDL_JoyButtonEvent jbutton;
    SDL_ResizeEvent resize;
} SDL_Event;

#define SDL_NOEVENT         0
#define SDL_KEYDOWN         2
#define SDL_KEYUP           3
#define SDL_MOUSEBUTTONDOWN 5
#define SDL_MOUSEBUTTONUP   6
#define SDL_JOYAXISMOTION   7
#define SDL_JOYHATMOTION    9
#define SDL_JOYBUTTONDOWN   10
#define SDL_QUIT            12
#define SDL_VIDEORESIZE     16

#define KMOD_NONE 0x0000
#define KMOD_ALT  0x0300

#define SDLK_UNKNOWN   0
#define SDLK_RETURN    13
#define SDLK_ESCAPE    27
#define SDLK_SPACE     32
#define SDLK_HASH      35
#define SDLK_2         50
#define SDLK_5         53
#define SDLK_7         55
#define SDLK_a         97
#define SDLK_d         100
#define SDLK_e         101
#define SDLK_f         102
#define SDLK_p         112
#define SDLK_q         113
#define SDLK_r         114
#define SDLK_s         115
#define SDLK_w         119
#define SDLK_KP0       256
#define SDLK_KP1       257
#define SDLK_KP2       258
#define SDLK_KP3       259
#define SDLK_KP4       260
#define SDLK_KP5       261
#define SDLK_KP6       262
#define SDLK_KP7       263
#define SDLK_KP8       264
#define SDLK_KP9       265
#define SDLK_KP_ENTER  271
#define SDLK_UP        273
#define SDLK_DOWN      274
#define SDLK_RIGHT     275
#define SDLK_LEFT      276
#define SDLK_INSERT    277
#define SDLK_HOME      278
#define SDLK_END       279
#define SDLK_PAGEUP    280
#define SDLK_PAGEDOWN  281
#define SDLK_F4        285
#define SDLK_LCTRL     306
#define SDLK_LALT      308
#define SDLK_RALT      307

#define SDL_HAT_UP    0x01
#define SDL_HAT_RIGHT 0x02
#define SDL_HAT_DOWN  0x04
#define SDL_HAT_LEFT  0x08

typedef struct SDL_AudioSpec {
    int freq;
    SDL_AudioFormat format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint16 padding;
    Uint32 size;
    void (SDLCALL *callback)(void *userdata, Uint8 *stream, int len);
    void *userdata;
} SDL_AudioSpec;

typedef struct SDL_AudioCVT {
    int needed;
    SDL_AudioFormat src_format, dst_format;
    Uint8 src_channels, dst_channels;
    int src_rate, dst_rate;
    double rate_incr;
    Uint8 *buf;
    int len, len_cvt, len_mult;
    double len_ratio;
} SDL_AudioCVT;

typedef struct SDL_RWops {
    int kind;
    FILE *file;
    const Uint8 *memory;
    size_t size;
    size_t position;
} SDL_RWops;

typedef struct SDL_Joystick SDL_Joystick;
typedef struct SDL_mutex SDL_mutex;
typedef struct SDL_sem SDL_sem;
typedef struct SDL_Thread SDL_Thread;

int SDL_Init(Uint32 flags);
void SDL_Quit(void);
const char *SDL_GetError(void);
Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 milliseconds);

SDL_Surface *SDL_SetVideoMode(int width, int height, int bits, Uint32 flags);
SDL_Surface *SDL_CreateRGBSurface(
    Uint32 flags, int width, int height, int depth,
    Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask
);
void SDL_FreeSurface(SDL_Surface *surface);
int SDL_LockSurface(SDL_Surface *surface);
void SDL_UnlockSurface(SDL_Surface *surface);
#define SDL_MUSTLOCK(surface) 0
int SDL_FillRect(SDL_Surface *surface, SDL_Rect *rect, Uint32 color);
int SDL_UpperBlit(
    SDL_Surface *source, const SDL_Rect *source_rect,
    SDL_Surface *destination, SDL_Rect *destination_rect
);
#define SDL_BlitSurface SDL_UpperBlit
int SDL_SoftStretch(
    SDL_Surface *source, const SDL_Rect *source_rect,
    SDL_Surface *destination, SDL_Rect *destination_rect
);
int SDL_SetPalette(
    SDL_Surface *surface, int flags, SDL_Color *colors,
    int first_color, int color_count
);
int SDL_SetColorKey(SDL_Surface *surface, Uint32 flags, Uint32 key);
Uint32 SDL_MapRGB(SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b);
void SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h);
const SDL_VideoInfo *SDL_GetVideoInfo(void);
void SDL_WM_SetCaption(const char *title, const char *icon);
int SDL_ShowCursor(int toggle);
int SDL_SaveBMP(SDL_Surface *surface, const char *path);

int SDL_PollEvent(SDL_Event *event);
int SDL_EnableKeyRepeat(int delay, int interval);

SDL_RWops *SDL_RWFromFile(const char *path, const char *mode);
SDL_RWops *SDL_RWFromConstMem(const void *memory, int size);
int SDL_RWclose(SDL_RWops *rw);
size_t SDL_RWread(SDL_RWops *rw, void *buffer, size_t size, size_t count);
int SDL_RWseek(SDL_RWops *rw, int offset, int whence);
int SDL_RWtell(SDL_RWops *rw);
void SDL_FreeRW(SDL_RWops *rw);

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
void SDL_CloseAudio(void);
void SDL_PauseAudio(int pause_on);
void SDL_LockAudio(void);
void SDL_UnlockAudio(void);
void SDL_9588AudioPump(void);
void SDL_9588ResetInputState(void);
SDL_mutex *SDL_CreateMutex(void);
void SDL_DestroyMutex(SDL_mutex *mutex);
int SDL_mutexP(SDL_mutex *mutex);
int SDL_mutexV(SDL_mutex *mutex);
int SDL_BuildAudioCVT(
    SDL_AudioCVT *cvt, SDL_AudioFormat src_format, Uint8 src_channels,
    int src_rate, SDL_AudioFormat dst_format, Uint8 dst_channels, int dst_rate
);
int SDL_ConvertAudio(SDL_AudioCVT *cvt);

#define SDL_strcasecmp strcasecmp
#define SDL_strncasecmp strncasecmp

#ifdef __cplusplus
}
#endif

#endif
