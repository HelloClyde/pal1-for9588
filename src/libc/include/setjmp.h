#ifndef PAL9588_SETJMP_H
#define PAL9588_SETJMP_H

typedef unsigned long jmp_buf[12];

#ifdef __cplusplus
extern "C" {
#endif
int setjmp(jmp_buf environment);
void longjmp(jmp_buf environment, int value) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif
