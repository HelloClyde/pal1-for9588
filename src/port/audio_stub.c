#include "players.h"

/* Compressed soundtrack backends are not used by the classic RIX build. */
LPAUDIOPLAYER MP3_Init(VOID) { return 0; }
LPAUDIOPLAYER OGG_Init(VOID) { return 0; }
