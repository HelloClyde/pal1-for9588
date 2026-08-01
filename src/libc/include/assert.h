#ifndef PAL9588_ASSERT_H
#define PAL9588_ASSERT_H

#include <stdlib.h>

#define assert(condition) ((condition) ? (void)0 : abort())

#endif
