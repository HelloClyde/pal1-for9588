#include "bda_filesystem.h"
#include "bda_memory.h"
#include "bda_time.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <sys/time.h>

#define ALLOCATION_MAGIC 0x9588a110u

typedef union allocation_header {
    struct {
        uint32_t magic;
        uint32_t size;
    } fields;
    uint64_t alignment;
} allocation_header_t;

struct pal9588_file {
    int handle;
    int error;
    int eof;
    int standard;
};

int errno;

static FILE g_stdin = {0, 0, 1, 1};
static FILE g_stdout = {0, 0, 0, 1};
static FILE g_stderr = {0, 0, 0, 1};
FILE *stdin = &g_stdin;
FILE *stdout = &g_stdout;
FILE *stderr = &g_stderr;

static uint32_t g_random_state = 1u;

void *malloc(size_t size)
{
    allocation_header_t *header;
    if (size == 0u) size = 1u;
    if (size > 0xffffffffu - sizeof(*header)) return 0;
    header = (allocation_header_t *)bda_alloc(
        (bda_size_t)(size + sizeof(*header))
    );
    if (!header || (uint32_t)header == 0xffffffffu) return 0;
    header->fields.magic = ALLOCATION_MAGIC;
    header->fields.size = (uint32_t)size;
    return header + 1;
}

void free(void *pointer)
{
    allocation_header_t *header;
    if (!pointer) return;
    header = (allocation_header_t *)pointer - 1;
    if (header->fields.magic != ALLOCATION_MAGIC) return;
    header->fields.magic = 0;
    bda_free(header);
}

void *calloc(size_t count, size_t size)
{
    size_t total;
    void *pointer;
    if (size != 0u && count > (size_t)-1 / size) return 0;
    total = count * size;
    pointer = malloc(total);
    if (pointer) memset(pointer, 0, total);
    return pointer;
}

void *realloc(void *pointer, size_t size)
{
    allocation_header_t *header;
    void *replacement;
    size_t copy_size;
    if (!pointer) return malloc(size);
    if (size == 0u) {
        free(pointer);
        return 0;
    }
    header = (allocation_header_t *)pointer - 1;
    if (header->fields.magic != ALLOCATION_MAGIC) return 0;
    replacement = malloc(size);
    if (!replacement) return 0;
    copy_size = header->fields.size < size ? header->fields.size : size;
    memcpy(replacement, pointer, copy_size);
    free(pointer);
    return replacement;
}

void *memcpy(void *destination, const void *source, size_t count)
{
    uint8_t *out = (uint8_t *)destination;
    const uint8_t *in = (const uint8_t *)source;
    while (count-- != 0u) *out++ = *in++;
    return destination;
}

void *memmove(void *destination, const void *source, size_t count)
{
    uint8_t *out = (uint8_t *)destination;
    const uint8_t *in = (const uint8_t *)source;
    if (out <= in || out >= in + count) return memcpy(out, in, count);
    out += count;
    in += count;
    while (count-- != 0u) *--out = *--in;
    return destination;
}

void *memset(void *destination, int value, size_t count)
{
    uint8_t *out = (uint8_t *)destination;
    while (count-- != 0u) *out++ = (uint8_t)value;
    return destination;
}

int memcmp(const void *left, const void *right, size_t count)
{
    const uint8_t *a = (const uint8_t *)left;
    const uint8_t *b = (const uint8_t *)right;
    while (count-- != 0u) {
        if (*a != *b) return (int)*a - (int)*b;
        ++a;
        ++b;
    }
    return 0;
}

void *memchr(const void *memory, int character, size_t count)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    while (count-- != 0u) {
        if (*bytes == (uint8_t)character) return (void *)bytes;
        ++bytes;
    }
    return 0;
}

size_t strlen(const char *text)
{
    const char *end = text;
    while (*end) ++end;
    return (size_t)(end - text);
}

int strcmp(const char *left, const char *right)
{
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return (int)(uint8_t)*left - (int)(uint8_t)*right;
}

int strncmp(const char *left, const char *right, size_t count)
{
    while (count != 0u && *left && *left == *right) {
        ++left;
        ++right;
        --count;
    }
    return count == 0u ? 0 : (int)(uint8_t)*left - (int)(uint8_t)*right;
}

char *strcpy(char *destination, const char *source)
{
    char *result = destination;
    while ((*destination++ = *source++) != 0) {}
    return result;
}

char *strncpy(char *destination, const char *source, size_t count)
{
    char *result = destination;
    while (count != 0u && *source) {
        *destination++ = *source++;
        --count;
    }
    while (count-- != 0u) *destination++ = 0;
    return result;
}

char *strcat(char *destination, const char *source)
{
    strcpy(destination + strlen(destination), source);
    return destination;
}

char *strncat(char *destination, const char *source, size_t count)
{
    char *end = destination + strlen(destination);
    while (count-- != 0u && *source) *end++ = *source++;
    *end = 0;
    return destination;
}

char *strchr(const char *text, int character)
{
    char target = (char)character;
    do {
        if (*text == target) return (char *)text;
    } while (*text++);
    return 0;
}

char *strrchr(const char *text, int character)
{
    const char *match = 0;
    char target = (char)character;
    do {
        if (*text == target) match = text;
    } while (*text++);
    return (char *)match;
}

char *strstr(const char *text, const char *needle)
{
    size_t length = strlen(needle);
    if (length == 0u) return (char *)text;
    while (*text) {
        if (*text == *needle && memcmp(text, needle, length) == 0) {
            return (char *)text;
        }
        ++text;
    }
    return 0;
}

char *strpbrk(const char *text, const char *characters)
{
    while (*text) {
        if (strchr(characters, *text)) return (char *)text;
        ++text;
    }
    return 0;
}

size_t strspn(const char *text, const char *characters)
{
    const char *start = text;
    while (*text && strchr(characters, *text)) ++text;
    return (size_t)(text - start);
}

size_t strcspn(const char *text, const char *characters)
{
    const char *start = text;
    while (*text && !strchr(characters, *text)) ++text;
    return (size_t)(text - start);
}

char *strtok(char *text, const char *delimiters)
{
    static char *next;
    char *token;
    if (text) next = text;
    if (!next) return 0;
    next += strspn(next, delimiters);
    if (!*next) {
        next = 0;
        return 0;
    }
    token = next;
    next += strcspn(next, delimiters);
    if (*next) *next++ = 0;
    else next = 0;
    return token;
}

char *strdup(const char *text)
{
    size_t length = strlen(text) + 1u;
    char *copy = (char *)malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}

int strcasecmp(const char *left, const char *right)
{
    while (*left && tolower((uint8_t)*left) == tolower((uint8_t)*right)) {
        ++left;
        ++right;
    }
    return tolower((uint8_t)*left) - tolower((uint8_t)*right);
}

int strncasecmp(const char *left, const char *right, size_t count)
{
    while (count != 0u && *left &&
           tolower((uint8_t)*left) == tolower((uint8_t)*right)) {
        ++left;
        ++right;
        --count;
    }
    return count == 0u ? 0 :
        tolower((uint8_t)*left) - tolower((uint8_t)*right);
}

static int digit_value(int character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'z') return character - 'a' + 10;
    if (character >= 'A' && character <= 'Z') return character - 'A' + 10;
    return -1;
}

unsigned long strtoul(const char *text, char **end, int base)
{
    unsigned long value = 0;
    int digit;
    while (isspace((uint8_t)*text)) ++text;
    if (*text == '+') ++text;
    if ((base == 0 || base == 16) && text[0] == '0' &&
        (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text += 2;
    } else if (base == 0) {
        base = text[0] == '0' ? 8 : 10;
    }
    while ((digit = digit_value((uint8_t)*text)) >= 0 && digit < base) {
        value = value * (unsigned)base + (unsigned)digit;
        ++text;
    }
    if (end) *end = (char *)text;
    return value;
}

long strtol(const char *text, char **end, int base)
{
    int negative = 0;
    unsigned long value;
    while (isspace((uint8_t)*text)) ++text;
    if (*text == '-' || *text == '+') negative = *text++ == '-';
    value = strtoul(text, end, base);
    return negative ? -(long)value : (long)value;
}

int atoi(const char *text) { return (int)strtol(text, 0, 10); }
int abs(int value) { return value < 0 ? -value : value; }
long labs(long value) { return value < 0 ? -value : value; }

void srand(unsigned seed) { g_random_state = seed ? seed : 1u; }

int rand(void)
{
    g_random_state = g_random_state * 1103515245u + 12345u;
    return (int)((g_random_state >> 16) & RAND_MAX);
}

static void format_character(
    char *output, size_t size, size_t *offset, char value
)
{
    if (*offset + 1u < size) output[*offset] = value;
    ++*offset;
}

static void format_unsigned(
    char *output, size_t size, size_t *offset, uint64_t value,
    unsigned base, int width, char pad, int uppercase
)
{
    char digits[24];
    const char *alphabet = uppercase ?
        "0123456789ABCDEF" : "0123456789abcdef";
    int count = 0;
    do {
        digits[count++] = alphabet[value % base];
        value /= base;
    } while (value != 0u && count < (int)sizeof(digits));
    while (width-- > count) format_character(output, size, offset, pad);
    while (count != 0) {
        format_character(output, size, offset, digits[--count]);
    }
}

int vsnprintf(char *output, size_t size, const char *format, va_list args)
{
    size_t offset = 0;
    while (*format) {
        int width = 0;
        int long_count = 0;
        char pad = ' ';
        char specifier;
        if (*format != '%') {
            format_character(output, size, &offset, *format++);
            continue;
        }
        ++format;
        if (*format == '0') {
            pad = '0';
            ++format;
        }
        while (isdigit((uint8_t)*format)) {
            width = width * 10 + (*format++ - '0');
        }
        while (*format == 'l') {
            ++long_count;
            ++format;
        }
        if (*format == 'z') {
            long_count = 1;
            ++format;
        }
        specifier = *format ? *format++ : 0;
        if (specifier == 's') {
            const char *text = va_arg(args, const char *);
            if (!text) text = "(null)";
            while (*text) format_character(output, size, &offset, *text++);
        } else if (specifier == 'c') {
            format_character(output, size, &offset, (char)va_arg(args, int));
        } else if (specifier == 'd' || specifier == 'i') {
            long long value = long_count >= 2 ? va_arg(args, long long) :
                (long_count == 1 ? va_arg(args, long) : va_arg(args, int));
            uint64_t magnitude;
            if (value < 0) {
                format_character(output, size, &offset, '-');
                magnitude = (uint64_t)(-(value + 1)) + 1u;
            } else {
                magnitude = (uint64_t)value;
            }
            format_unsigned(output, size, &offset, magnitude, 10u,
                            width, pad, 0);
        } else if (specifier == 'u' || specifier == 'x' ||
                   specifier == 'X') {
            uint64_t value = long_count >= 2 ?
                va_arg(args, unsigned long long) :
                (long_count == 1 ? va_arg(args, unsigned long) :
                 va_arg(args, unsigned int));
            format_unsigned(output, size, &offset, value,
                specifier == 'u' ? 10u : 16u, width, pad,
                specifier == 'X');
        } else if (specifier == 'p') {
            format_character(output, size, &offset, '0');
            format_character(output, size, &offset, 'x');
            format_unsigned(output, size, &offset,
                (uintptr_t)va_arg(args, void *), 16u, 8, '0', 0);
        } else if (specifier == '%') {
            format_character(output, size, &offset, '%');
        }
    }
    if (size != 0u) output[offset < size ? offset : size - 1u] = 0;
    return (int)offset;
}

int snprintf(char *output, size_t size, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = vsnprintf(output, size, format, args);
    va_end(args);
    return result;
}

int vsprintf(char *output, const char *format, va_list args)
{
    return vsnprintf(output, (size_t)-1, format, args);
}

int sprintf(char *output, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = vsprintf(output, format, args);
    va_end(args);
    return result;
}

int printf(const char *format, ...)
{
    char ignored[256];
    int result;
    va_list args;
    va_start(args, format);
    result = vsnprintf(ignored, sizeof(ignored), format, args);
    va_end(args);
    return result;
}

int vfprintf(FILE *stream, const char *format, va_list args)
{
    char buffer[512];
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    size_t write_length;
    if (!stream || stream->standard) return length;
    write_length = length < (int)sizeof(buffer) ?
        (size_t)length : sizeof(buffer) - 1u;
    return fwrite(buffer, 1u, write_length, stream) == write_length ?
        length : -1;
}

int fprintf(FILE *stream, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = vfprintf(stream, format, args);
    va_end(args);
    return result;
}

int sscanf(const char *input, const char *format, ...)
{
    int assigned = 0;
    va_list args;
    va_start(args, format);
    while (*format) {
        if (isspace((uint8_t)*format)) {
            while (isspace((uint8_t)*format)) ++format;
            while (isspace((uint8_t)*input)) ++input;
            continue;
        }
        if (*format != '%') {
            if (*input != *format) break;
            ++input;
            ++format;
            continue;
        }
        ++format;
        if (*format == '%') {
            if (*input != '%') break;
            ++input;
            ++format;
            continue;
        }
        {
            int width = 0;
            int suppress = 0;
            if (*format == '*') {
                suppress = 1;
                ++format;
            }
            while (isdigit((uint8_t)*format)) {
                width = width * 10 + (*format++ - '0');
            }
            if (*format == 'l') ++format;
            while (isspace((uint8_t)*input)) ++input;
            if (*format == 's') {
                char *output = suppress ? 0 : va_arg(args, char *);
                int copied = 0;
                while (*input && !isspace((uint8_t)*input) &&
                       (width == 0 || copied < width)) {
                    if (output) output[copied] = *input;
                    ++input;
                    ++copied;
                }
                if (copied == 0) break;
                if (output) {
                    output[copied] = 0;
                    ++assigned;
                }
            } else if (*format == 'c') {
                char *output = suppress ? 0 : va_arg(args, char *);
                int count = width ? width : 1;
                int index;
                for (index = 0; index < count; ++index) {
                    if (!input[index]) break;
                    if (output) output[index] = input[index];
                }
                if (index != count) break;
                input += count;
                if (output) ++assigned;
            } else if (*format == 'd' || *format == 'u' ||
                       *format == 'x' || *format == 'X') {
                char *end;
                int base = (*format == 'x' || *format == 'X') ? 16 : 10;
                long value = strtol(input, &end, base);
                if (end == input) break;
                input = end;
                if (!suppress) {
                    int *output = va_arg(args, int *);
                    *output = (int)value;
                    ++assigned;
                }
            } else {
                break;
            }
            ++format;
        }
    }
    va_end(args);
    return assigned;
}

int getchar(void) { return EOF; }
int putchar(int character) { return character; }
int puts(const char *text) { return (int)strlen(text) + 1; }

FILE *fopen(const char *path, const char *mode)
{
    int handle = bda_fs_fopen_raw(path, mode);
    FILE *stream;
    if (!bda_fs_file_is_valid(handle)) return 0;
    stream = (FILE *)malloc(sizeof(*stream));
    if (!stream) {
        (void)bda_fs_close_raw(handle);
        return 0;
    }
    stream->handle = handle;
    stream->error = 0;
    stream->eof = 0;
    stream->standard = 0;
    return stream;
}

int fclose(FILE *stream)
{
    int result;
    if (!stream || stream->standard) return EOF;
    result = bda_fs_close_raw(stream->handle);
    free(stream);
    return result;
}

size_t fread(void *buffer, size_t size, size_t count, FILE *stream)
{
    int result;
    if (!stream || stream->standard || size > 0xffffffffu ||
        count > 0xffffffffu) return 0u;
    result = bda_fs_fread_raw(
        buffer, (bda_size_t)size, (bda_size_t)count, stream->handle
    );
    if (result < 0) {
        stream->error = 1;
        return 0u;
    }
    if ((size_t)result < count) stream->eof = 1;
    return (size_t)result;
}

size_t fwrite(
    const void *buffer, size_t size, size_t count, FILE *stream
)
{
    int result;
    if (!stream || stream->standard || size > 0xffffffffu ||
        count > 0xffffffffu) return 0u;
    result = bda_fs_fwrite_raw(
        buffer, (bda_size_t)size, (bda_size_t)count, stream->handle
    );
    if (result < 0) {
        stream->error = 1;
        return 0u;
    }
    if ((size_t)result < count) stream->error = 1;
    return (size_t)result;
}

int fgetc(FILE *stream)
{
    unsigned char character;
    return fread(&character, 1u, 1u, stream) == 1u ? character : EOF;
}

int fputc(int character, FILE *stream)
{
    unsigned char value = (unsigned char)character;
    return fwrite(&value, 1u, 1u, stream) == 1u ? value : EOF;
}

int fseek(FILE *stream, long offset, int origin)
{
    int result;
    if (!stream || stream->standard) return -1;
    result = bda_fs_seek_raw(stream->handle, (int32_t)offset, origin);
    if (result < 0) {
        stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    return 0;
}

long ftell(FILE *stream)
{
    int result;
    if (!stream || stream->standard) return -1;
    result = bda_fs_tell_raw(stream->handle);
    if (result < 0) stream->error = 1;
    return (long)result;
}

int ferror(FILE *stream)
{
    if (!stream || stream->standard) return 0;
    return stream->error || bda_fs_error(stream->handle) != 0;
}

int feof(FILE *stream) { return stream ? stream->eof : 0; }

int setvbuf(FILE *stream, char *buffer, int mode, size_t size)
{
    (void)stream;
    (void)buffer;
    (void)mode;
    (void)size;
    return 0;
}

int fflush(FILE *stream)
{
    (void)stream;
    return 0;
}

int fputs(const char *text, FILE *stream)
{
    size_t length = strlen(text);
    return fwrite(text, 1u, length, stream) == length ? 0 : EOF;
}

char *fgets(char *buffer, int size, FILE *stream)
{
    int index = 0;
    if (!buffer || size <= 0 || !stream) return 0;
    while (index + 1 < size) {
        char character;
        if (fread(&character, 1u, 1u, stream) != 1u) break;
        buffer[index++] = character;
        if (character == '\n') break;
    }
    if (index == 0) return 0;
    buffer[index] = 0;
    return buffer;
}

int access(const char *path, int mode)
{
    int file;
    (void)mode;
    file = bda_fs_fopen_raw(path, "rb");
    if (!bda_fs_file_is_valid(file)) return -1;
    (void)bda_fs_close_raw(file);
    return 0;
}

int chdir(const char *path) { return bda_fs_chdir(path); }

void abort(void)
{
    for (;;) bda_sys_delay(1u);
}

void exit(int status)
{
    (void)status;
    for (;;) bda_sys_delay(1u);
}

size_t wcslen(const wchar_t *text)
{
    const wchar_t *end = text;
    while (*end) ++end;
    return (size_t)(end - text);
}

wchar_t *wcscpy(wchar_t *destination, const wchar_t *source)
{
    wchar_t *result = destination;
    while ((*destination++ = *source++) != 0) {}
    return result;
}

wchar_t *wcsncpy(
    wchar_t *destination, const wchar_t *source, size_t count
)
{
    wchar_t *result = destination;
    while (count != 0u && *source) {
        *destination++ = *source++;
        --count;
    }
    while (count-- != 0u) *destination++ = 0;
    return result;
}

int wcscmp(const wchar_t *left, const wchar_t *right)
{
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return *left < *right ? -1 : (*left > *right ? 1 : 0);
}

wchar_t *wcschr(const wchar_t *text, wchar_t character)
{
    do {
        if (*text == character) return (wchar_t *)text;
    } while (*text++);
    return 0;
}

long wcstol(const wchar_t *text, wchar_t **end, int base)
{
    long value = 0;
    int negative = 0;
    while (*text == L' ' || *text == L'\t' || *text == L'\n' ||
           *text == L'\r') {
        ++text;
    }
    if (*text == L'-' || *text == L'+') negative = *text++ == L'-';
    if (base == 0) base = 10;
    while (*text >= L'0' && *text <= L'9' &&
           (*text - L'0') < base) {
        value = value * base + (*text++ - L'0');
    }
    if (end) *end = (wchar_t *)text;
    return negative ? -value : value;
}

static void wide_unsigned(
    wchar_t **output, wchar_t *end, unsigned long long value,
    unsigned base, int uppercase
)
{
    wchar_t digits[24];
    const wchar_t *alphabet = uppercase ?
        L"0123456789ABCDEF" : L"0123456789abcdef";
    int count = 0;
    do {
        digits[count++] = alphabet[value % base];
        value /= base;
    } while (value != 0u && count < (int)(sizeof(digits) / sizeof(digits[0])));
    while (count != 0 && *output < end) *(*output)++ = digits[--count];
}

int vswprintf(
    wchar_t *output, size_t size, const wchar_t *format, va_list args
)
{
    wchar_t *cursor = output;
    wchar_t *end;
    if (!output || !format || size == 0u) return -1;
    end = output + size - 1u;
    while (*format && cursor < end) {
        if (*format != L'%') {
            *cursor++ = *format++;
            continue;
        }
        ++format;
        if (*format == L'%') {
            *cursor++ = *format++;
            continue;
        }
        while (*format == L'-' || *format == L'+' || *format == L' ' ||
               *format == L'#' || *format == L'0' ||
               (*format >= L'0' && *format <= L'9') ||
               *format == L'.' || *format == L'*') {
            if (*format == L'*') (void)va_arg(args, int);
            ++format;
        }
        {
            int long_count = 0;
            while (*format == L'l') {
                ++long_count;
                ++format;
            }
            if (*format == L'd' || *format == L'i') {
                long long value = long_count >= 2 ?
                    va_arg(args, long long) :
                    (long_count == 1 ? va_arg(args, long) :
                     va_arg(args, int));
                if (value < 0) {
                    *cursor++ = L'-';
                    wide_unsigned(&cursor, end,
                        (unsigned long long)(-(value + 1)) + 1u, 10u, 0);
                } else {
                    wide_unsigned(
                        &cursor, end, (unsigned long long)value, 10u, 0
                    );
                }
            } else if (*format == L'u' || *format == L'x' ||
                       *format == L'X' || *format == L'o') {
                unsigned long long value = long_count >= 2 ?
                    va_arg(args, unsigned long long) :
                    (long_count == 1 ? va_arg(args, unsigned long) :
                     va_arg(args, unsigned int));
                unsigned base = *format == L'o' ? 8u :
                    ((*format == L'u') ? 10u : 16u);
                wide_unsigned(
                    &cursor, end, value, base, *format == L'X'
                );
            } else if (*format == L'p') {
                wide_unsigned(
                    &cursor, end,
                    (uintptr_t)va_arg(args, void *), 16u, 0
                );
            }
            if (*format) ++format;
        }
    }
    *cursor = 0;
    return (int)(cursor - output);
}

double fabs(double value) { return value < 0.0 ? -value : value; }
double floor(double value)
{
    long integer = (long)value;
    return value < 0.0 && (double)integer != value ?
        (double)(integer - 1) : (double)integer;
}
double ceil(double value) { return -floor(-value); }
double round(double value)
{
    return value < 0.0 ? ceil(value - 0.5) : floor(value + 0.5);
}
double fmax(double left, double right) { return left > right ? left : right; }
double fmin(double left, double right) { return left < right ? left : right; }

double sqrt(double value)
{
    double estimate;
    int iteration;
    if (value <= 0.0) return 0.0;
    estimate = value > 1.0 ? value : 1.0;
    for (iteration = 0; iteration < 14; ++iteration) {
        estimate = (estimate + value / estimate) * 0.5;
    }
    return estimate;
}

static double natural_log(double value)
{
    int exponent = 0;
    double y;
    double term;
    double sum = 0.0;
    int index;
    if (value <= 0.0) return -1.0e30;
    while (value > 2.0) {
        value *= 0.5;
        ++exponent;
    }
    while (value < 1.0) {
        value *= 2.0;
        --exponent;
    }
    y = (value - 1.0) / (value + 1.0);
    term = y;
    for (index = 1; index < 20; index += 2) {
        sum += term / (double)index;
        term *= y * y;
    }
    return 2.0 * sum + (double)exponent * 0.6931471805599453;
}

static double natural_exp(double value)
{
    int exponent = 0;
    double result = 1.0;
    double term = 1.0;
    int index;
    while (value > 0.6931471805599453) {
        value -= 0.6931471805599453;
        ++exponent;
    }
    while (value < -0.6931471805599453) {
        value += 0.6931471805599453;
        --exponent;
    }
    for (index = 1; index < 18; ++index) {
        term *= value / (double)index;
        result += term;
    }
    while (exponent > 0) {
        result *= 2.0;
        --exponent;
    }
    while (exponent < 0) {
        result *= 0.5;
        ++exponent;
    }
    return result;
}

double pow(double base, double exponent)
{
    if (exponent == 0.0) return 1.0;
    if (base <= 0.0) return 0.0;
    return natural_exp(natural_log(base) * exponent);
}

double log(double value)
{
    const double ln2 = 0.69314718055994530942;
    double y;
    double y2;
    double term;
    double sum;
    int exponent = 0;
    int divisor;
    if (value <= 0.0) return -1.0e308;
    while (value >= 1.4142135623730951) {
        value *= 0.5;
        ++exponent;
    }
    while (value < 0.7071067811865476) {
        value *= 2.0;
        --exponent;
    }
    y = (value - 1.0) / (value + 1.0);
    y2 = y * y;
    term = y;
    sum = 0.0;
    for (divisor = 1; divisor <= 19; divisor += 2) {
        sum += term / divisor;
        term *= y2;
    }
    return 2.0 * sum + exponent * ln2;
}

static double wrap_angle(double value)
{
    while (value > M_PI) value -= 2.0 * M_PI;
    while (value < -M_PI) value += 2.0 * M_PI;
    return value;
}

double sin(double value)
{
    double squared;
    value = wrap_angle(value);
    squared = value * value;
    return value * (1.0 - squared / 6.0 +
        squared * squared / 120.0 -
        squared * squared * squared / 5040.0);
}

double cos(double value) { return sin(value + M_PI * 0.5); }

int gettimeofday(struct timeval *time_value, struct timezone *zone)
{
    uint32_t milliseconds = bda_gui_tick_count_25ms() * 25u;
    if (time_value) {
        time_value->tv_sec = (long)(milliseconds / 1000u);
        time_value->tv_usec = (long)((milliseconds % 1000u) * 1000u);
    }
    if (zone) {
        zone->tz_minuteswest = 0;
        zone->tz_dsttime = 0;
    }
    return 0;
}

time_t time(time_t *result)
{
    time_t value = 946684800ll +
        (time_t)(bda_gui_tick_count_25ms() / 40u);
    if (result) *result = value;
    return value;
}

static int leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

struct tm *localtime(const time_t *value)
{
    static struct tm result;
    static const int month_days[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    time_t days = *value / 86400ll;
    time_t seconds = *value % 86400ll;
    int year = 1970;
    int month = 0;
    int day_of_year;
    if (seconds < 0) {
        seconds += 86400ll;
        --days;
    }
    result.tm_hour = (int)(seconds / 3600ll);
    result.tm_min = (int)((seconds / 60ll) % 60ll);
    result.tm_sec = (int)(seconds % 60ll);
    result.tm_wday = (int)((days + 4ll) % 7ll);
    if (result.tm_wday < 0) result.tm_wday += 7;
    while (days >= (leap_year(year) ? 366 : 365)) {
        days -= leap_year(year) ? 366 : 365;
        ++year;
    }
    day_of_year = (int)days;
    while (month < 11) {
        int length = month_days[month] +
            (month == 1 && leap_year(year));
        if (days < length) break;
        days -= length;
        ++month;
    }
    result.tm_year = year - 1900;
    result.tm_mon = month;
    result.tm_mday = (int)days + 1;
    result.tm_yday = day_of_year;
    result.tm_isdst = 0;
    return &result;
}
