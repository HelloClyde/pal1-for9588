#ifndef PAL9588_STDIO_H
#define PAL9588_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IONBF 2

typedef struct pal9588_file FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#ifdef __cplusplus
extern "C" {
#endif
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list args);
int sprintf(char *output, const char *format, ...);
int vsprintf(char *output, const char *format, va_list args);
int snprintf(char *output, size_t size, const char *format, ...);
int vsnprintf(char *output, size_t size, const char *format, va_list args);
int sscanf(const char *input, const char *format, ...);
int getchar(void);
int putchar(int character);
int puts(const char *text);
int fputs(const char *text, FILE *stream);
int fgetc(FILE *stream);
int fputc(int character, FILE *stream);
char *fgets(char *buffer, int size, FILE *stream);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream);
int fseek(FILE *stream, long offset, int origin);
long ftell(FILE *stream);
int ferror(FILE *stream);
int feof(FILE *stream);
int setvbuf(FILE *stream, char *buffer, int mode, size_t size);
int fflush(FILE *stream);

#ifdef __cplusplus
}
#endif

#endif
