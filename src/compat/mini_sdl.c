#include "SDL.h"
#include "regression.h"
#include "platform.h"

#include "bda_audio.h"

#include <stdlib.h>
#include <string.h>

static char g_error[96];
static SDL_VideoInfo g_video_info;
static SDL_Surface *g_video_surface;
static unsigned g_previous_keys;
static unsigned g_pending_changes;
static int g_quit_sent;
static SDL_AudioSpec g_audio_spec;
static Uint8 *g_audio_buffer;
static int g_audio_opened;
static int g_audio_paused;
static unsigned g_audio_lock_count;

struct SDL_mutex {
    int unused;
};

static void audio_pump(void)
{
    unsigned blocks = 0;
    if (!g_audio_opened || g_audio_paused || g_audio_lock_count != 0u ||
        !g_audio_buffer || !g_audio_spec.callback) {
        return;
    }
    /*
     * Keep the firmware queue full.  Rendering a rotated 320x200 frame can
     * take much longer than one 1024-byte (about 23 ms) PCM block on 9588,
     * so limiting a pump to one or two blocks causes audible gaps.
     */
    while (blocks++ < 32u && bda_audio_ready()) {
        int written;
        g_audio_spec.callback(
            g_audio_spec.userdata, g_audio_buffer, (int)g_audio_spec.size
        );
        written = bda_audio_write(g_audio_buffer, g_audio_spec.size);
        if (written != (int)g_audio_spec.size) break;
    }
}

void SDL_9588AudioPump(void)
{
    audio_pump();
}

static SDL_PixelFormat *create_format(
    int depth, Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask
)
{
    SDL_PixelFormat *format =
        (SDL_PixelFormat *)calloc(1u, sizeof(*format));
    if (!format) return 0;
    format->BitsPerPixel = (Uint8)depth;
    format->BytesPerPixel = (Uint8)((depth + 7) / 8);
    format->Rmask = rmask;
    format->Gmask = gmask;
    format->Bmask = bmask;
    format->Amask = amask;
    format->alpha = 255;
    if (depth == 8) {
        format->palette = (SDL_Palette *)calloc(1u, sizeof(SDL_Palette));
        if (!format->palette) {
            free(format);
            return 0;
        }
        format->palette->ncolors = 256;
        format->palette->colors =
            (SDL_Color *)calloc(256u, sizeof(SDL_Color));
        if (!format->palette->colors) {
            free(format->palette);
            free(format);
            return 0;
        }
    }
    return format;
}

static void free_format(SDL_PixelFormat *format)
{
    if (!format) return;
    if (format->palette) {
        free(format->palette->colors);
        free(format->palette);
    }
    free(format);
}

int SDL_Init(Uint32 flags)
{
    (void)flags;
    g_error[0] = 0;
    g_previous_keys = 0;
    g_pending_changes = 0;
    g_quit_sent = 0;
    if (!pal9588_platform_open()) {
        strcpy(g_error, "BBK 9588 window initialization failed");
        return -1;
    }
    return 0;
}

void SDL_Quit(void)
{
    pal9588_platform_close();
    g_video_surface = 0;
}

const char *SDL_GetError(void) { return g_error; }
Uint32 SDL_GetTicks(void) { return pal9588_platform_ticks(); }

void SDL_Delay(Uint32 milliseconds)
{
    Uint32 start = SDL_GetTicks();
    while ((Uint32)(SDL_GetTicks() - start) < milliseconds) {
        audio_pump();
        pal9588_platform_pump();
        pal9588_platform_delay(1u);
    }
}

SDL_Surface *SDL_CreateRGBSurface(
    Uint32 flags, int width, int height, int depth,
    Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask
)
{
    SDL_Surface *surface;
    size_t bytes;
    if (width <= 0 || height <= 0 ||
        (depth != 8 && depth != 16 && depth != 32)) {
        strcpy(g_error, "unsupported surface format");
        return 0;
    }
    surface = (SDL_Surface *)calloc(1u, sizeof(*surface));
    if (!surface) return 0;
    surface->format = create_format(depth, rmask, gmask, bmask, amask);
    if (!surface->format) {
        free(surface);
        return 0;
    }
    surface->flags = flags;
    surface->w = width;
    surface->h = height;
    surface->pitch = (Uint16)(width * surface->format->BytesPerPixel);
    surface->clip_rect.x = 0;
    surface->clip_rect.y = 0;
    surface->clip_rect.w = (Uint16)width;
    surface->clip_rect.h = (Uint16)height;
    bytes = (size_t)surface->pitch * (size_t)height;
    surface->pixels = calloc(1u, bytes);
    if (!surface->pixels) {
        free_format(surface->format);
        free(surface);
        return 0;
    }
    surface->owns_pixels = 1;
    surface->refcount = 1;
    return surface;
}

SDL_Surface *SDL_SetVideoMode(
    int width, int height, int bits, Uint32 flags
)
{
    SDL_Surface *surface;
    if (width != 320 || height != 240 || bits != 8) {
        width = 320;
        height = 240;
        bits = 8;
    }
    surface = SDL_CreateRGBSurface(flags, width, height, bits, 0, 0, 0, 0);
    if (surface) {
        g_video_surface = surface;
        memset(&g_video_info, 0, sizeof(g_video_info));
        g_video_info.vfmt = surface->format;
        g_video_info.current_w = width;
        g_video_info.current_h = height;
    }
    return surface;
}

void SDL_FreeSurface(SDL_Surface *surface)
{
    if (!surface) return;
    if (surface == g_video_surface) g_video_surface = 0;
    if (surface->owns_pixels) free(surface->pixels);
    free_format(surface->format);
    free(surface);
}

int SDL_LockSurface(SDL_Surface *surface)
{
    return surface ? 0 : -1;
}

void SDL_UnlockSurface(SDL_Surface *surface) { (void)surface; }

static void normalize_rect(
    const SDL_Surface *surface, const SDL_Rect *requested, SDL_Rect *result
)
{
    if (requested) {
        *result = *requested;
    } else {
        result->x = 0;
        result->y = 0;
        result->w = (Uint16)surface->w;
        result->h = (Uint16)surface->h;
    }
}

int SDL_FillRect(SDL_Surface *surface, SDL_Rect *rect, Uint32 color)
{
    SDL_Rect area;
    int x;
    int y;
    int bytes_per_pixel;
    if (!surface || !surface->pixels) return -1;
    normalize_rect(surface, rect, &area);
    bytes_per_pixel = surface->format->BytesPerPixel;
    for (y = area.y; y < area.y + area.h && y < surface->h; ++y) {
        Uint8 *row = (Uint8 *)surface->pixels + y * surface->pitch;
        for (x = area.x; x < area.x + area.w && x < surface->w; ++x) {
            if (bytes_per_pixel == 1) {
                row[x] = (Uint8)color;
            } else if (bytes_per_pixel == 2) {
                ((Uint16 *)row)[x] = (Uint16)color;
            } else {
                ((Uint32 *)row)[x] = color;
            }
        }
    }
    return 0;
}

int SDL_UpperBlit(
    SDL_Surface *source, const SDL_Rect *source_rect,
    SDL_Surface *destination, SDL_Rect *destination_rect
)
{
    SDL_Rect from;
    SDL_Rect to;
    int row;
    int bytes_per_pixel;
    if (!source || !destination ||
        source->format->BytesPerPixel !=
            destination->format->BytesPerPixel) {
        return -1;
    }
    normalize_rect(source, source_rect, &from);
    if (destination_rect) {
        to = *destination_rect;
    } else {
        to.x = 0;
        to.y = 0;
        to.w = from.w;
        to.h = from.h;
    }
    if (to.x < 0) {
        from.x -= to.x;
        from.w = (Uint16)((int)from.w + to.x);
        to.x = 0;
    }
    if (to.y < 0) {
        from.y -= to.y;
        from.h = (Uint16)((int)from.h + to.y);
        to.y = 0;
    }
    if ((int)from.w > destination->w - to.x) {
        from.w = (Uint16)(destination->w - to.x);
    }
    if ((int)from.h > destination->h - to.y) {
        from.h = (Uint16)(destination->h - to.y);
    }
    bytes_per_pixel = source->format->BytesPerPixel;
    for (row = 0; row < from.h; ++row) {
        const Uint8 *in = (const Uint8 *)source->pixels +
            (from.y + row) * source->pitch + from.x * bytes_per_pixel;
        Uint8 *out = (Uint8 *)destination->pixels +
            (to.y + row) * destination->pitch + to.x * bytes_per_pixel;
        memmove(out, in, (size_t)from.w * bytes_per_pixel);
    }
    if (destination_rect) {
        destination_rect->w = from.w;
        destination_rect->h = from.h;
    }
    return 0;
}

int SDL_SoftStretch(
    SDL_Surface *source, const SDL_Rect *source_rect,
    SDL_Surface *destination, SDL_Rect *destination_rect
)
{
    SDL_Rect from;
    SDL_Rect to;
    int x;
    int y;
    int bytes_per_pixel;
    if (!source || !destination ||
        source->format->BytesPerPixel !=
            destination->format->BytesPerPixel) {
        return -1;
    }
    normalize_rect(source, source_rect, &from);
    normalize_rect(destination, destination_rect, &to);
    bytes_per_pixel = source->format->BytesPerPixel;
    for (y = 0; y < to.h; ++y) {
        int source_y = from.y + y * from.h / to.h;
        Uint8 *out = (Uint8 *)destination->pixels +
            (to.y + y) * destination->pitch + to.x * bytes_per_pixel;
        const Uint8 *in = (const Uint8 *)source->pixels +
            source_y * source->pitch;
        for (x = 0; x < to.w; ++x) {
            int source_x = from.x + x * from.w / to.w;
            memcpy(out + x * bytes_per_pixel,
                   in + source_x * bytes_per_pixel,
                   (size_t)bytes_per_pixel);
        }
    }
    return 0;
}

int SDL_SetPalette(
    SDL_Surface *surface, int flags, SDL_Color *colors,
    int first_color, int color_count
)
{
    (void)flags;
    if (!surface || !surface->format || !surface->format->palette ||
        first_color < 0 || color_count < 0 ||
        first_color + color_count > 256) {
        return 0;
    }
    memcpy(surface->format->palette->colors + first_color, colors,
           (size_t)color_count * sizeof(*colors));
    return 1;
}

int SDL_SetColorKey(SDL_Surface *surface, Uint32 flags, Uint32 key)
{
    if (!surface || !surface->format) return -1;
    surface->format->colorkey = key;
    surface->flags = (surface->flags & ~SDL_RLEACCEL) |
        (flags & SDL_RLEACCEL);
    return 0;
}

Uint32 SDL_MapRGB(SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b)
{
    if (!format) return 0;
    if (format->BitsPerPixel == 16) {
        return ((Uint32)(r & 0xf8u) << 8) |
               ((Uint32)(g & 0xfcu) << 3) | ((Uint32)b >> 3);
    }
    if (format->BitsPerPixel == 32) {
        return ((Uint32)r << 16) | ((Uint32)g << 8) | b;
    }
    return 0;
}

void SDL_UpdateRect(
    SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h
)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    pal9588_platform_present(screen);
    audio_pump();
}

const SDL_VideoInfo *SDL_GetVideoInfo(void) { return &g_video_info; }
void SDL_WM_SetCaption(const char *title, const char *icon)
{
    (void)title;
    (void)icon;
}
int SDL_ShowCursor(int toggle) { return toggle; }
int SDL_SaveBMP(SDL_Surface *surface, const char *path)
{
    (void)surface;
    (void)path;
    return -1;
}

int SDL_PollEvent(SDL_Event *event)
{
    static const SDLKey key_symbols[6] = {
        SDLK_UP, SDLK_DOWN, SDLK_LEFT,
        SDLK_RIGHT, SDLK_RETURN, SDLK_ESCAPE
    };
    unsigned keys;
    unsigned change;
    int index;
    static int regression_auto_sent;

    audio_pump();
    pal9588_platform_pump();
    if (pal9588_platform_detached()) {
        if (g_quit_sent) return 0;
        g_quit_sent = 1;
        if (event) {
            memset(event, 0, sizeof(*event));
            event->type = SDL_QUIT;
        }
        return 1;
    }

    if (pal9588_regression_auto_battle()) {
        if (!regression_auto_sent) {
            regression_auto_sent = 1;
            if (event) {
                memset(event, 0, sizeof(*event));
                event->type = SDL_KEYDOWN;
                event->key.type = SDL_KEYDOWN;
                event->key.state = 1;
                event->key.keysym.sym = SDLK_a;
                event->key.keysym.mod = KMOD_NONE;
            }
            return 1;
        }
    } else {
        regression_auto_sent = 0;
    }

    keys = pal9588_platform_keys();
    g_pending_changes |= keys ^ g_previous_keys;
    if (g_pending_changes == 0u) return 0;
    for (index = 0; index < 6; ++index) {
        change = 1u << index;
        if (g_pending_changes & change) {
            g_pending_changes &= ~change;
            if (event) {
                memset(event, 0, sizeof(*event));
                event->type = (keys & change) ? SDL_KEYDOWN : SDL_KEYUP;
                event->key.type = event->type;
                event->key.state = (keys & change) ? 1 : 0;
                event->key.keysym.sym = key_symbols[index];
                event->key.keysym.mod = KMOD_NONE;
            }
            if (keys & change) g_previous_keys |= change;
            else g_previous_keys &= ~change;
            return 1;
        }
    }
    return 0;
}

int SDL_EnableKeyRepeat(int delay, int interval)
{
    (void)delay;
    (void)interval;
    return 0;
}

void SDL_9588ResetInputState(void)
{
    g_previous_keys = 0;
    g_pending_changes = 0;
}

SDL_RWops *SDL_RWFromFile(const char *path, const char *mode)
{
    SDL_RWops *rw = (SDL_RWops *)calloc(1u, sizeof(*rw));
    if (!rw) return 0;
    rw->file = fopen(path, mode);
    if (!rw->file) {
        free(rw);
        return 0;
    }
    rw->kind = 1;
    return rw;
}

SDL_RWops *SDL_RWFromConstMem(const void *memory, int size)
{
    SDL_RWops *rw;
    if (!memory || size < 0) return 0;
    rw = (SDL_RWops *)calloc(1u, sizeof(*rw));
    if (!rw) return 0;
    rw->kind = 2;
    rw->memory = (const Uint8 *)memory;
    rw->size = (size_t)size;
    return rw;
}

int SDL_RWclose(SDL_RWops *rw)
{
    int result = 0;
    if (!rw) return -1;
    if (rw->kind == 1 && rw->file) result = fclose(rw->file);
    free(rw);
    return result;
}

size_t SDL_RWread(
    SDL_RWops *rw, void *buffer, size_t size, size_t count
)
{
    size_t bytes;
    if (!rw) return 0;
    if (rw->kind == 1) return fread(buffer, size, count, rw->file);
    bytes = size * count;
    if (bytes > rw->size - rw->position) bytes = rw->size - rw->position;
    memcpy(buffer, rw->memory + rw->position, bytes);
    rw->position += bytes;
    return size == 0u ? 0u : bytes / size;
}

int SDL_RWseek(SDL_RWops *rw, int offset, int whence)
{
    size_t position;
    if (!rw) return -1;
    if (rw->kind == 1) {
        if (fseek(rw->file, offset, whence) != 0) return -1;
        return (int)ftell(rw->file);
    }
    if (whence == SEEK_SET) position = (size_t)offset;
    else if (whence == SEEK_CUR) position = rw->position + offset;
    else position = rw->size + offset;
    if (position > rw->size) return -1;
    rw->position = position;
    return (int)position;
}

int SDL_RWtell(SDL_RWops *rw)
{
    if (!rw) return -1;
    return rw->kind == 1 ? (int)ftell(rw->file) : (int)rw->position;
}

void SDL_FreeRW(SDL_RWops *rw) { (void)SDL_RWclose(rw); }

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
    if (!desired || !desired->callback || g_audio_opened) return -1;
    if (desired->format != AUDIO_S16SYS || desired->channels != 1 ||
        desired->freq != (int)BDA_AUDIO_SAMPLE_RATE_22050) {
        strcpy(g_error, "audio requires 22050 Hz signed 16-bit mono");
        return -1;
    }
    g_audio_spec = *desired;
    g_audio_spec.format = AUDIO_S16SYS;
    g_audio_spec.channels = BDA_AUDIO_CHANNELS_MONO;
    g_audio_spec.silence = 0;
    /* One 92.9 ms DMA block tolerates the 9588's long software-render bursts. */
    g_audio_spec.samples = 2048;
    g_audio_spec.padding = 0;
    g_audio_spec.size = (Uint32)g_audio_spec.samples * sizeof(Sint16);
    g_audio_buffer = (Uint8 *)malloc(g_audio_spec.size);
    if (!g_audio_buffer) {
        strcpy(g_error, "audio buffer allocation failed");
        return -1;
    }
    memset(g_audio_buffer, 0, g_audio_spec.size);
    bda_audio_open_pcm(
        BDA_AUDIO_SAMPLE_RATE_22050,
        BDA_AUDIO_BITS_16,
        BDA_AUDIO_CHANNELS_MONO
    );
    g_audio_opened = 1;
    g_audio_paused = 1;
    g_audio_lock_count = 0;
    if (obtained) *obtained = g_audio_spec;
    return 0;
}
void SDL_CloseAudio(void)
{
    if (!g_audio_opened) return;
    g_audio_paused = 1;
    bda_audio_stop();
    free(g_audio_buffer);
    g_audio_buffer = 0;
    memset(&g_audio_spec, 0, sizeof(g_audio_spec));
    g_audio_lock_count = 0;
    g_audio_opened = 0;
}
void SDL_PauseAudio(int pause_on)
{
    g_audio_paused = pause_on != 0;
    if (!g_audio_paused) audio_pump();
}
void SDL_LockAudio(void) { ++g_audio_lock_count; }
void SDL_UnlockAudio(void)
{
    if (g_audio_lock_count != 0u) --g_audio_lock_count;
}
int SDL_BuildAudioCVT(
    SDL_AudioCVT *cvt, SDL_AudioFormat src_format, Uint8 src_channels,
    int src_rate, SDL_AudioFormat dst_format, Uint8 dst_channels, int dst_rate
)
{
    int src_bytes;
    int dst_bytes;
    int numerator;
    int denominator;
    if (!cvt || src_rate <= 0 || dst_rate <= 0 ||
        (src_channels != 1 && src_channels != 2) ||
        dst_channels != 1 ||
        (src_format != AUDIO_U8 && src_format != AUDIO_S16LSB) ||
        dst_format != AUDIO_S16SYS) {
        return -1;
    }
    memset(cvt, 0, sizeof(*cvt));
    cvt->src_format = src_format;
    cvt->dst_format = dst_format;
    cvt->src_channels = src_channels;
    cvt->dst_channels = dst_channels;
    cvt->src_rate = src_rate;
    cvt->dst_rate = dst_rate;
    cvt->needed = src_format != dst_format ||
        src_channels != dst_channels || src_rate != dst_rate;
    src_bytes = src_format == AUDIO_U8 ? 1 : 2;
    dst_bytes = 2;
    numerator = dst_rate * (int)dst_channels * dst_bytes;
    denominator = src_rate * (int)src_channels * src_bytes;
    cvt->len_mult = (numerator + denominator - 1) / denominator;
    if (cvt->len_mult < 1) cvt->len_mult = 1;
    cvt->rate_incr = 1.0;
    cvt->len_ratio = 1.0;
    return cvt->needed;
}

static Sint16 read_converted_sample(
    const SDL_AudioCVT *cvt, const Uint8 *input, int frame
)
{
    int channel;
    int total = 0;
    int bytes_per_sample = cvt->src_format == AUDIO_U8 ? 1 : 2;
    const Uint8 *at = input +
        frame * (int)cvt->src_channels * bytes_per_sample;
    for (channel = 0; channel < cvt->src_channels; ++channel) {
        int value;
        if (cvt->src_format == AUDIO_U8) {
            value = ((int)at[channel] - 128) << 8;
        } else {
            const Uint8 *sample = at + channel * 2;
            value = (Sint16)((Uint16)sample[0] |
                             ((Uint16)sample[1] << 8));
        }
        total += value;
    }
    return (Sint16)(total / (int)cvt->src_channels);
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
    int src_bytes;
    int src_frame_bytes;
    int src_frames;
    int dst_frames;
    int reverse;
    int index;
    if (!cvt || !cvt->buf || cvt->len < 0) return -1;
    if (!cvt->needed) {
        cvt->len_cvt = cvt->len;
        return 0;
    }
    src_bytes = cvt->src_format == AUDIO_U8 ? 1 : 2;
    src_frame_bytes = src_bytes * (int)cvt->src_channels;
    if (src_frame_bytes <= 0 || cvt->src_rate <= 0 || cvt->dst_rate <= 0) {
        cvt->len_cvt = 0;
        return -1;
    }
    src_frames = cvt->len / src_frame_bytes;
    dst_frames = (src_frames * cvt->dst_rate) / cvt->src_rate;
    cvt->len_cvt = dst_frames * (int)sizeof(Sint16);
    reverse = cvt->len_cvt > cvt->len;
    for (index = reverse ? dst_frames - 1 : 0;
         reverse ? index >= 0 : index < dst_frames;
         index += reverse ? -1 : 1) {
        int source_frame = (index * cvt->src_rate) / cvt->dst_rate;
        Sint16 value;
        Uint8 *output = cvt->buf + index * 2;
        if (source_frame >= src_frames) source_frame = src_frames - 1;
        value = src_frames > 0 ?
            read_converted_sample(cvt, cvt->buf, source_frame) : 0;
        output[0] = (Uint8)((Uint16)value & 0xffu);
        output[1] = (Uint8)(((Uint16)value >> 8) & 0xffu);
    }
    return 0;
}

SDL_mutex *SDL_CreateMutex(void)
{
    return (SDL_mutex *)calloc(1u, sizeof(SDL_mutex));
}

void SDL_DestroyMutex(SDL_mutex *mutex) { free(mutex); }
int SDL_mutexP(SDL_mutex *mutex) { return mutex ? 0 : -1; }
int SDL_mutexV(SDL_mutex *mutex) { return mutex ? 0 : -1; }
