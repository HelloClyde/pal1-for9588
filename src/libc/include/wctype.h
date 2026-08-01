#ifndef PAL9588_WCTYPE_H
#define PAL9588_WCTYPE_H

#include <wchar.h>

static inline int iswspace(wchar_t value)
{
    return value == L' ' || value == L'\t' || value == L'\n' ||
           value == L'\r' || value == L'\f' || value == L'\v';
}

#endif
