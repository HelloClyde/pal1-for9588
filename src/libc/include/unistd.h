#ifndef PAL9588_UNISTD_H
#define PAL9588_UNISTD_H

#ifdef __cplusplus
extern "C" {
#endif
int access(const char *path, int mode);
int chdir(const char *path);

#ifdef __cplusplus
}
#endif

#endif
