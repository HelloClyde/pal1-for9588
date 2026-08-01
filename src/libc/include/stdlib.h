#ifndef PAL9588_STDLIB_H
#define PAL9588_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *pointer, size_t size);
void free(void *pointer);
long strtol(const char *text, char **end, int base);
unsigned long strtoul(const char *text, char **end, int base);
int atoi(const char *text);
int abs(int value);
long labs(long value);
int rand(void);
void srand(unsigned seed);
void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));

#define RAND_MAX 32767
#define alloca(size) __builtin_alloca(size)

#ifdef __cplusplus
}
#endif

#endif
