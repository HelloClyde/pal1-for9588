#include "pal_config.h"

int sdlpal_main(int argc, char *argv[]);

__attribute__((section(".text.bda_main")))
int bda_main(void)
{
    char executable[] = PAL_9588_DATA_PATH "SDLPAL.BDA";
    char *arguments[] = { executable, 0 };
    return sdlpal_main(1, arguments);
}
