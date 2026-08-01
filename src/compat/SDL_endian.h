#ifndef PAL9588_SDL_ENDIAN_H
#define PAL9588_SDL_ENDIAN_H

#include <stdint.h>

#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER SDL_LIL_ENDIAN

static inline uint16_t SDL_Swap16(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static inline uint32_t SDL_Swap32(uint32_t value)
{
    return ((value & 0x000000ffu) << 24) |
           ((value & 0x0000ff00u) << 8) |
           ((value & 0x00ff0000u) >> 8) |
           ((value & 0xff000000u) >> 24);
}

#define SDL_SwapLE16(value) (value)
#define SDL_SwapLE32(value) (value)

#endif
