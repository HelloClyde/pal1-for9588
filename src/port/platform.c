#include "platform.h"
#include "pal_config.h"

#include "bda_graphics.h"
#include "bda_input.h"
#include "bda_memory.h"
#include "bda_time.h"
#include "bda_window.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGICAL_WIDTH 320
#define LOGICAL_HEIGHT 240
#define PHYSICAL_WIDTH 240
#define PHYSICAL_HEIGHT 320

static bda_handle_t g_frame;
static bda_handle_t g_draw;
static bda_handle_t g_draw_owner;
static void *g_draw_object;
static int g_detached;
static int g_timer_started;
static unsigned g_sampled_keys;
static unsigned g_latched_keys;
static uint16_t *g_pixels;
static bda_gui_framebuffer_t g_framebuffer;
static int g_direct_framebuffer;
static bda_gui_picture_t g_picture;

int pal9588_pak_last_error(void);

static unsigned read_input_keys(void)
{
    bda_gui_input_packet_t packet;
    unsigned keys = 0;
    memset(&packet, 0, sizeof(packet));
    (void)bda_gui_input_packet(&packet);

    /* The portrait LCD is viewed counter-clockwise in landscape. */
    if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_RIGHT))
        keys |= 1u << 0; /* logical up */
    if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_LEFT))
        keys |= 1u << 1; /* logical down */
    if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_UP))
        keys |= 1u << 2; /* logical left */
    if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_DOWN))
        keys |= 1u << 3; /* logical right */
    if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_ENTER))
        keys |= 1u << 4;
    if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_ESCAPE))
        keys |= 1u << 5;
    return keys;
}

static void sample_input_keys(void)
{
    unsigned keys = read_input_keys();
    g_latched_keys |= keys & ~g_sampled_keys;
    g_sampled_keys = keys;
}

static int pointer_valid(const void *pointer)
{
    return pointer && (uint32_t)pointer != 0xffffffffu;
}

static void release_draw_context(void)
{
    if (g_draw && (int32_t)g_draw != -1) bda_gui_end_draw(g_draw);
    g_draw = 0;
    g_draw_owner = 0;
}

static int acquire_draw_context(bda_handle_t owner)
{
    if (g_draw && g_draw_owner == owner) return 1;
    release_draw_context();
    g_draw = bda_gui_current_draw(owner);
    if (!g_draw || (int32_t)g_draw == -1) {
        g_draw = 0;
        return 0;
    }
    g_draw_owner = owner;
    return 1;
}

static int ensure_firmware_renderer(void)
{
    if (!g_frame || !acquire_draw_context(g_frame)) return 0;
    if (!g_draw_object) g_draw_object = bda_gui_draw_object_create(7u);
    return pointer_valid(g_draw_object);
}

static int window_proc(
    bda_handle_t handle, u32 message,
    u32 wparam, u32 lparam
)
{
    if (message == BDA_MSG_DRAW_CONTEXT_ATTACH) {
        g_frame = handle;
        if (!g_direct_framebuffer) (void)ensure_firmware_renderer();
    } else if (message == BDA_MSG_DRAW_CONTEXT_DETACH) {
        g_direct_framebuffer = 0;
        memset(&g_framebuffer, 0, sizeof(g_framebuffer));
        if (!g_draw_owner || g_draw_owner == handle) release_draw_context();
        g_detached = 1;
    }
    return bda_gui_default_proc(handle, message, wparam, lparam);
}

int pal9588_platform_open(void)
{
    bda_frame_desc_t descriptor;
    if (g_frame) return 1;

    memset(&descriptor, 0, sizeof(descriptor));
    memset(&g_framebuffer, 0, sizeof(g_framebuffer));
    memset(&g_picture, 0, sizeof(g_picture));
    g_detached = 0;
    g_direct_framebuffer = 0;
    g_draw = 0;
    g_draw_owner = 0;
    g_draw_object = 0;
    g_sampled_keys = 0;
    g_latched_keys = 0;
    g_pixels = (uint16_t *)malloc(
        PHYSICAL_WIDTH * PHYSICAL_HEIGHT * sizeof(*g_pixels)
    );
    if (!g_pixels) return 0;
    memset(g_pixels, 0,
           PHYSICAL_WIDTH * PHYSICAL_HEIGHT * sizeof(*g_pixels));

    g_picture.width = PHYSICAL_WIDTH;
    g_picture.height = PHYSICAL_HEIGHT;
    g_picture.source_pixels = g_pixels;
    g_picture.selected_index = -1;

    /* The BBK GUI expects legacy GBK text here: "仙剑1". */
    descriptor.title = "\xCF\xC9\xBD\xA3\x31";
    descriptor.wndproc = window_proc;
    descriptor.height = LOGICAL_HEIGHT;
    descriptor.width = LOGICAL_WIDTH;
    g_frame = bda_gui_register_frame_desc(&descriptor);
    if (!g_frame || (int32_t)g_frame == -1) {
        g_frame = 0;
        free(g_pixels);
        g_pixels = 0;
        return 0;
    }
    (void)bda_gui_frame_activate(g_frame, 0x100u);
    if (bda_gui_framebuffer_acquire(&g_framebuffer) == 0) {
        g_direct_framebuffer = 1;
        release_draw_context();
    } else if (!ensure_firmware_renderer()) {
        pal9588_platform_close();
        return 0;
    }
    bda_gui_millisecond_timer_start();
    g_timer_started = 1;
    return 1;
}

void pal9588_platform_close(void)
{
    bda_gui_message_t message;
    unsigned pumps = 0;
    if (g_timer_started) {
        bda_gui_millisecond_timer_stop();
        g_timer_started = 0;
    }
    g_direct_framebuffer = 0;
    memset(&g_framebuffer, 0, sizeof(g_framebuffer));
    if (g_frame) {
        memset(&message, 0, sizeof(message));
        (void)bda_gui_frame_stop(g_frame);
        (void)bda_gui_frame_release(g_frame);
        while (!g_detached && pumps++ < 32u &&
               bda_gui_event_pump_frame_once(&message, g_frame)) {
            bda_sys_delay(1u);
        }
        release_draw_context();
        bda_gui_close_frame(g_frame);
        g_frame = 0;
    }
    free(g_pixels);
    g_pixels = 0;
    g_draw_object = 0;
}

static uint16_t rgb565(SDL_Color color)
{
    return (uint16_t)(((uint16_t)(color.r & 0xf8u) << 8) |
                      ((uint16_t)(color.g & 0xfcu) << 3) |
                      ((uint16_t)color.b >> 3));
}

static uint16_t rgb555_to_rgb565(uint16_t color)
{
    uint16_t red = (color >> 10) & 0x1fu;
    uint16_t green = (color >> 5) & 0x1fu;
    uint16_t blue = color & 0x1fu;
    green = (uint16_t)((green << 1) | (green >> 4));
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

static void submit_physical_pixels(int strip_size)
{
    int strip_y;
    void *old_object;
    if (!g_pixels) return;

    if (g_direct_framebuffer) {
        SDL_9588AudioPump();
        if (bda_gui_framebuffer_present_rgb565(
                &g_framebuffer, g_pixels
            ) == 0) {
            SDL_9588AudioPump();
            return;
        }
        g_direct_framebuffer = 0;
        memset(&g_framebuffer, 0, sizeof(g_framebuffer));
    }
    if (!ensure_firmware_renderer()) return;

    (void)bda_gui_draw_guard_begin();
    old_object = bda_gui_select_draw_object(g_draw, g_draw_object);
    /* Split the slow firmware blit so PCM can be rearmed between strips. */
    if (strip_size <= 0 || strip_size > PHYSICAL_HEIGHT) {
        strip_size = PHYSICAL_HEIGHT;
    }
    SDL_9588AudioPump();
    for (strip_y = 0; strip_y < PHYSICAL_HEIGHT; strip_y += strip_size) {
        int strip_height = PHYSICAL_HEIGHT - strip_y;
        if (strip_height > strip_size) strip_height = strip_size;
        g_picture.height = (u32)strip_height;
        g_picture.source_pixels = g_pixels + strip_y * PHYSICAL_WIDTH;
        (void)bda_gui_render_picture(
            g_draw, 0, strip_y, PHYSICAL_WIDTH, strip_height, &g_picture
        );
        SDL_9588AudioPump();
    }
    g_picture.height = PHYSICAL_HEIGHT;
    g_picture.source_pixels = g_pixels;
    (void)bda_gui_select_draw_object(g_draw, old_object);
    (void)bda_gui_draw_guard_end();
}

void pal9588_platform_present(const SDL_Surface *surface)
{
    int x;
    int y;
    if (!surface || !surface->pixels || !surface->format ||
        !surface->format->palette || !g_pixels ||
        (!g_direct_framebuffer && (!g_draw || !g_draw_object))) {
        return;
    }
    for (y = 0; y < LOGICAL_HEIGHT; ++y) {
        const uint8_t *source =
            (const uint8_t *)surface->pixels + y * surface->pitch;
        for (x = 0; x < LOGICAL_WIDTH; ++x) {
            int physical_x = LOGICAL_HEIGHT - 1 - y;
            int physical_y = x;
            uint8_t index = source[x];
            g_pixels[physical_y * PHYSICAL_WIDTH + physical_x] =
                rgb565(surface->format->palette->colors[index]);
        }
    }

    submit_physical_pixels(32);
}

void pal9588_platform_present_rgb555(
    const Uint16 *pixels, int width, int height, int pitch_pixels
)
{
    int x;
    int y;
    const int target_width = 320;
    const int target_height = 200;
    const int target_y = (LOGICAL_HEIGHT - target_height) / 2;

    if (!pixels || width <= 0 || height <= 0 || pitch_pixels < width ||
        !g_pixels ||
        (!g_direct_framebuffer && (!g_draw || !g_draw_object))) {
        return;
    }

    memset(g_pixels, 0,
           PHYSICAL_WIDTH * PHYSICAL_HEIGHT * sizeof(*g_pixels));
    for (y = 0; y < target_height; ++y) {
        int source_y = y * height / target_height;
        const Uint16 *source = pixels + source_y * pitch_pixels;
        int logical_y = target_y + y;
        int physical_x = LOGICAL_HEIGHT - 1 - logical_y;
        for (x = 0; x < target_width; ++x) {
            int source_x = x * width / target_width;
            int physical_y = x;
            g_pixels[physical_y * PHYSICAL_WIDTH + physical_x] =
                rgb555_to_rgb565(source[source_x]);
        }
    }
    /* One full submit avoids ten full-screen captures in the emulator. */
    submit_physical_pixels(PHYSICAL_HEIGHT);
}

void pal9588_platform_pump(void)
{
    bda_gui_message_t message;
    unsigned count = 0;
    if (!g_frame) return;
    memset(&message, 0, sizeof(message));
    while (count++ < 8u &&
           bda_gui_event_pump_frame_once(&message, g_frame)) {}
}

unsigned pal9588_platform_keys(void)
{
    unsigned keys;
    sample_input_keys();
    keys = g_sampled_keys | g_latched_keys;
    g_latched_keys = 0;
    return keys;
}

void pal9588_platform_wait_action_release(void)
{
    const unsigned action_keys = (1u << 4) | (1u << 5);
    while (!g_detached && (read_input_keys() & action_keys) != 0u) {
        SDL_9588AudioPump();
        pal9588_platform_pump();
        bda_sys_delay(1u);
    }
    g_sampled_keys = read_input_keys();
    g_latched_keys = 0;
}

int pal9588_platform_detached(void) { return g_detached; }

Uint32 pal9588_platform_ticks(void)
{
    return g_timer_started ? bda_gui_millisecond_count() :
        bda_gui_tick_count_25ms() * 25u;
}

void pal9588_platform_delay(Uint32 milliseconds)
{
    Uint32 start = pal9588_platform_ticks();
    do {
        bda_sys_delay(1u);
    } while ((Uint32)(pal9588_platform_ticks() - start) < milliseconds);
}

static const uint8_t *glyph_rows(char character)
{
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t colon[7] = {0, 4, 4, 0, 4, 4, 0};
    static const uint8_t backslash[7] = {16, 8, 4, 2, 1, 0, 0};
    static const uint8_t period[7] = {0, 0, 0, 0, 0, 6, 6};
    static const uint8_t letters[36][7] = {
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
        {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
        {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
        {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2}, {31,16,30,1,1,17,14},
        {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12}
    };
    if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
    if (character >= 'A' && character <= 'Z') {
        return letters[character - 'A'];
    }
    if (character >= '0' && character <= '9') {
        return letters[26 + character - '0'];
    }
    if (character == ':') {
        return colon;
    }
    if (character == '\\') {
        return backslash;
    }
    if (character == '.') {
        return period;
    }
    return blank;
}

static void draw_fatal_text(
    int x, int y, const char *text, uint16_t color, int scale
)
{
    while (*text) {
        const uint8_t *rows = glyph_rows(*text++);
        int row;
        int column;
        for (row = 0; row < 7; ++row) {
            for (column = 0; column < 5; ++column) {
                if (rows[row] & (1u << (4 - column))) {
                    int sx;
                    int sy;
                    for (sy = 0; sy < scale; ++sy) {
                        for (sx = 0; sx < scale; ++sx) {
                            int logical_x = x + column * scale + sx;
                            int logical_y = y + row * scale + sy;
                            int physical_x =
                                LOGICAL_HEIGHT - 1 - logical_y;
                            int physical_y = logical_x;
                            if ((unsigned)physical_x < PHYSICAL_WIDTH &&
                                (unsigned)physical_y < PHYSICAL_HEIGHT) {
                                g_pixels[
                                    physical_y * PHYSICAL_WIDTH + physical_x
                                ] = color;
                            }
                        }
                    }
                }
            }
        }
        x += 6 * scale;
    }
}

void PAL9588_FatalOutput(const char *message)
{
    unsigned previous;
    unsigned keys;
    FILE *log = fopen(PAL_9588_DATA_PATH "PALFATAL.LOG", "wb");
    if (log) {
        fputs(message ? message : "unknown fatal error", log);
        fprintf(log, "palpak_error=%d\n", pal9588_pak_last_error());
        fclose(log);
    }
    if (!g_pixels) return;
    {
        unsigned index;
        for (index = 0; index < PHYSICAL_WIDTH * PHYSICAL_HEIGHT; ++index) {
            g_pixels[index] = 0x0843u;
        }
    }
    draw_fatal_text(22, 32, "SDLPAL FOR BBK 9588", 0xffffu, 2);
    draw_fatal_text(55, 78, "GAME DATA NOT FOUND", 0xfbe0u, 1);
    draw_fatal_text(46, 103, "COPY LEGAL PAL FILES TO", 0xc69au, 1);
    draw_fatal_text(121, 123, "A:\\...\\PAL\\", 0x07ffu, 1);
    draw_fatal_text(88, 169, "PRESS ESC TO EXIT", 0xffffu, 1);

    submit_physical_pixels(PHYSICAL_HEIGHT);

    previous = pal9588_platform_keys();
    for (;;) {
        pal9588_platform_pump();
        if (g_detached) break;
        keys = pal9588_platform_keys();
        if ((keys & (1u << 5)) && !(previous & (1u << 5))) break;
        previous = keys;
        bda_sys_delay(1u);
    }
}
