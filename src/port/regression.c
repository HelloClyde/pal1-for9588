#include "main.h"
#include "pal_config.h"
#include "platform.h"
#include "regression.h"

#include <stdio.h>

#define SAVE_MARKER   PAL_9588_DATA_PATH "PORTTEST.SAVE"
#define BATTLE_MARKER PAL_9588_DATA_PATH "PORTTEST.BATTLE"
#define VIDEO_MARKER  PAL_9588_DATA_PATH "PORTTEST.VIDEO"
#define SOUND_MARKER  PAL_9588_DATA_PATH "PORTTEST.SOUND"
#define TEST_LOG      PAL_9588_DATA_PATH "PORTTEST.LOG"
#define SAVE_EXPECT   PAL_9588_DATA_PATH "PORTTEST.EXPECT"

#define SAVE_EXPECT_MAGIC 0x31545350u

#define BATTLE_TEST_FIELD 2u
#define BATTLE_TEST_MUSIC 37u

typedef struct pal9588_save_expect
{
    DWORD magic;
    WORD scene;
    WORD hp;
    DWORD cash;
} pal9588_save_expect_t;

enum
{
    REGRESSION_NONE,
    REGRESSION_SAVE,
    REGRESSION_BATTLE,
    REGRESSION_VIDEO,
    REGRESSION_SOUND
};

static int g_regression_mode;

extern VOID PAL9588_InternalGameMain(VOID);
extern unsigned pal9588_avi_last_frame_count(void);
extern unsigned pal9588_avi_last_exit_keys(void);

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file);
    return 1;
}

int pal9588_regression_active(void)
{
    return file_exists(SAVE_MARKER) || file_exists(BATTLE_MARKER) ||
        file_exists(VIDEO_MARKER) || file_exists(SOUND_MARKER);
}

int pal9588_regression_auto_battle(void)
{
    return g_regression_mode == REGRESSION_BATTLE && gpGlobals &&
        gpGlobals->fInBattle &&
        g_Battle.BattleResult == kBattleResultOnGoing;
}

static void reset_log(const char *heading)
{
    FILE *file = fopen(TEST_LOG, "wb");
    if (!file) return;
    fputs(heading, file);
    fputs("\r\n", file);
    fclose(file);
}

static void append_log(const char *line)
{
    FILE *file = fopen(TEST_LOG, "ab");
    if (!file) return;
    fputs(line, file);
    fputs("\r\n", file);
    fclose(file);
}

static int saved_file_header_valid(void)
{
    unsigned char header[2];
    FILE *file = fopen(PAL_9588_DATA_PATH "2.rpg", "rb");
    size_t count;
    long length;
    if (!file) return 0;
    count = fread(header, 1, sizeof(header), file);
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fclose(file);
    return count == sizeof(header) && header[0] == 0x34u &&
        header[1] == 0x12u && length > 100000;
}

static int read_save_expect(pal9588_save_expect_t *expect)
{
    FILE *file = fopen(SAVE_EXPECT, "rb");
    size_t count;
    if (!file) return 0;
    count = fread(expect, 1, sizeof(*expect), file);
    fclose(file);
    return count == sizeof(*expect) && expect->magic == SAVE_EXPECT_MAGIC;
}

static int write_save_expect(const pal9588_save_expect_t *expect)
{
    FILE *file = fopen(SAVE_EXPECT, "wb");
    size_t count;
    if (!file) return 0;
    count = fwrite(expect, 1, sizeof(*expect), file);
    fclose(file);
    return count == sizeof(*expect);
}

static void run_save_regression(void)
{
    pal9588_save_expect_t expect;
    int passed;

    if (read_save_expect(&expect)) {
        append_log("RELOAD START");
        gpGlobals->fInMainGame = TRUE;
        PAL_InitGameData(2);
        passed = saved_file_header_valid() &&
            gpGlobals->wNumScene == expect.scene &&
            gpGlobals->dwCash == expect.cash &&
            gpGlobals->g.PlayerRoles.rgwHP[0] == expect.hp;
        append_log(passed ? "RELOAD PASS" : "RELOAD FAIL");
        PAL_Shutdown(passed ? 0 : 2);
    }

    reset_log("SAVE START");
    gpGlobals->fInMainGame = TRUE;
    PAL_InitGameData(1);
    expect.magic = SAVE_EXPECT_MAGIC;
    expect.scene = gpGlobals->wNumScene;
    expect.cash = gpGlobals->dwCash;
    expect.hp = gpGlobals->g.PlayerRoles.rgwHP[0];

    PAL_SaveGame(2, 0x1234u);
    passed = saved_file_header_valid() && write_save_expect(&expect);
    append_log(passed ? "SAVE PASS" : "SAVE FAIL");
    PAL_Shutdown(passed ? 0 : 2);
}

static void run_video_regression(void)
{
    static const char *const movies[] = {
        "3.avi", "4.avi", "5.avi", "6.avi"
    };
    unsigned i;
    int passed = 1;

    reset_log("VIDEO START");
    for (i = 0; i < sizeof(movies) / sizeof(movies[0]); ++i) {
        int played = PAL_PlayAVI(movies[i]);
        unsigned frames = pal9588_avi_last_frame_count();
        unsigned exit_keys = pal9588_avi_last_exit_keys();
        char line[64];
        snprintf(line, sizeof(line), "VIDEO %u FRAMES %u KEYS %u %s",
            i + 3u, frames, exit_keys, played ? "PASS" : "FAIL");
        append_log(line);
        if (!played) {
            passed = 0;
        }
    }
    append_log(passed ? "VIDEO PASS" : "VIDEO FAIL");
    PAL_Shutdown(passed ? 0 : 3);
}

static void run_battle_regression(void)
{
    BATTLERESULT result;

    reset_log("BATTLE START");
    gpGlobals->fInMainGame = TRUE;
    /*
     * Use a deterministic new-game state instead of depending on a loose
     * 1.rpg save.  The default state intentionally leaves the battlefield at
     * FBP chunk 0, which is the player-status background rather than a battle
     * scene; normal gameplay sets both values through scripts before combat.
     */
    PAL_InitGameData(0);
    gpGlobals->wNumBattleField = BATTLE_TEST_FIELD;
    gpGlobals->wNumBattleMusic = BATTLE_TEST_MUSIC;
    PAL_SetPalette(gpGlobals->wNumPalette, gpGlobals->fNightPalette);
    PAL_SetLoadFlags(kLoadScene | kLoadPlayerSprite);
    PAL_LoadResources();
    append_log("BATTLE READY");
    AUDIO_PlayMusic(gpGlobals->wNumBattleMusic, TRUE, 0);
    result = PAL_StartBattle(1, FALSE);
    if (result == kBattleResultWon) append_log("BATTLE WON");
    else if (result == kBattleResultFleed) append_log("BATTLE FLED");
    else append_log("BATTLE OTHER");
    PAL_Shutdown(result == kBattleResultWon ? 0 : 4);
}

static void run_sound_regression(void)
{
    const Uint32 duration = 5000u;
    Uint32 started;
    Uint32 maximum_iteration = 0u;
    unsigned iterations = 0u;
    unsigned key_events = 0u;
    unsigned taps = 0u;
    SDL_Event event;
    char line[96];

    reset_log("SOUND START");
    AUDIO_PlayMusic(0, FALSE, 0);
    /* The six night-watch drum hits before meeting the Sword Master use 93. */
    AUDIO_PlaySound(93);
    started = SDL_GetTicks();
    while ((Uint32)(SDL_GetTicks() - started) < duration) {
        Uint32 before = SDL_GetTicks();
        int x;
        int y;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN) ++key_events;
        }
        if (pal9588_platform_take_tap(&x, &y)) ++taps;
        SDL_Delay(1u);
        before = SDL_GetTicks() - before;
        if (before > maximum_iteration) maximum_iteration = before;
        ++iterations;
    }
    snprintf(line, sizeof(line),
        "SOUND ITERATIONS %u MAX_MS %u KEYS %u TAPS %u",
        iterations, maximum_iteration, key_events, taps);
    append_log(line);
    append_log((key_events || taps) ? "SOUND INPUT PASS" : "SOUND INPUT NONE");
    PAL_Shutdown(0);
}

VOID PAL_GameMain(VOID)
{
    if (file_exists(SAVE_MARKER)) {
        g_regression_mode = REGRESSION_SAVE;
        run_save_regression();
    }
    if (file_exists(VIDEO_MARKER)) {
        g_regression_mode = REGRESSION_VIDEO;
        run_video_regression();
    }
    if (file_exists(BATTLE_MARKER)) {
        g_regression_mode = REGRESSION_BATTLE;
        run_battle_regression();
    }
    if (file_exists(SOUND_MARKER)) {
        g_regression_mode = REGRESSION_SOUND;
        run_sound_regression();
    }
    g_regression_mode = REGRESSION_NONE;
    PAL9588_InternalGameMain();
}
