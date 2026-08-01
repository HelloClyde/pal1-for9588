#ifndef PAL9588_WCHAR_H
#define PAL9588_WCHAR_H

#include <stddef.h>
#include <stdarg.h>

#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

size_t wcslen(const wchar_t *text);
wchar_t *wcscpy(wchar_t *destination, const wchar_t *source);
wchar_t *wcsncpy(
    wchar_t *destination, const wchar_t *source, size_t count
);
int wcscmp(const wchar_t *left, const wchar_t *right);
wchar_t *wcschr(const wchar_t *text, wchar_t character);
long wcstol(
    const wchar_t *text, wchar_t **end, int base
);
int vswprintf(
    wchar_t *output, size_t size, const wchar_t *format, va_list args
);

#ifdef __cplusplus
}
#endif

#endif
