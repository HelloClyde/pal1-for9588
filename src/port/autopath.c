#include "main.h"
#include "platform.h"

#include <stdint.h>
#include <string.h>

#define PATH_RADIUS 24
#define PATH_DIM (PATH_RADIUS * 2 + 1)
#define PATH_NODE_COUNT (PATH_DIM * PATH_DIM)
#define PATH_MAX_STEPS (PATH_RADIUS * 4)
#define SEARCH_HIT_X 32
#define SEARCH_HIT_Y_ABOVE 64
#define SEARCH_HIT_Y_BELOW 20
#define REPLAN_LIMIT 2

typedef struct tagPAL9588_AUTOPATH
{
    PALDIRECTION steps[PATH_MAX_STEPS];
    int length;
    int next;
    int active;
    int injected;
    PALDIRECTION injected_direction;
    int expected_x;
    int expected_y;
    int target_x;
    int target_y;
    int interaction_object;
    int replans;
    WORD scene;
} PAL9588_AUTOPATH;

static PAL9588_AUTOPATH g_path;
static uint8_t g_visited[PATH_NODE_COUNT];
static int16_t g_parent[PATH_NODE_COUNT];
static uint8_t g_parent_direction[PATH_NODE_COUNT];
static uint16_t g_queue[PATH_NODE_COUNT];

extern VOID PAL9588_InternalStartFrame(VOID);

static int absolute_int(int value)
{
    return value < 0 ? -value : value;
}

static int party_x(void)
{
    return PAL_X(gpGlobals->viewport) + PAL_X(gpGlobals->partyoffset);
}

static int party_y(void)
{
    return PAL_Y(gpGlobals->viewport) + PAL_Y(gpGlobals->partyoffset);
}

static void direction_offset(
    PALDIRECTION direction, int *x_offset, int *y_offset
)
{
    *x_offset =
        (direction == kDirWest || direction == kDirSouth) ? -16 : 16;
    *y_offset =
        (direction == kDirWest || direction == kDirNorth) ? -8 : 8;
}

static void cancel_path(void)
{
    if (g_path.injected && g_InputState.dir == g_path.injected_direction) {
        g_InputState.dir = kDirUnknown;
        g_InputState.prevdir = kDirUnknown;
    }
    memset(&g_path, 0, sizeof(g_path));
    g_path.injected_direction = kDirUnknown;
}

static int game_accepts_path(void)
{
    return gpGlobals->fInMainGame && !gpGlobals->fInBattle &&
        !gpGlobals->fGameStart && !gpGlobals->fEnteringScene &&
        gpGlobals->wNumScene > 0 && PAL_GetCurrentMap() != NULL;
}

static int node_index(int a, int b)
{
    return (b + PATH_RADIUS) * PATH_DIM + (a + PATH_RADIUS);
}

static int node_a(int index)
{
    return index % PATH_DIM - PATH_RADIUS;
}

static int node_b(int index)
{
    return index / PATH_DIM - PATH_RADIUS;
}

static void node_world_position(
    int start_x, int start_y, int a, int b, int *world_x, int *world_y
)
{
    *world_x = start_x + 16 * (a + b);
    *world_y = start_y + 8 * (a - b);
}

static int point_score(int x, int y, int target_x, int target_y)
{
    return absolute_int(x - target_x) +
        absolute_int(y - target_y) * 2;
}

static int build_path(int target_x, int target_y)
{
    static const int delta_a[4] = {0, -1, 0, 1};
    static const int delta_b[4] = {-1, 0, 1, 0};
    static const PALDIRECTION directions[4] = {
        kDirSouth, kDirWest, kDirNorth, kDirEast
    };
    PALDIRECTION reverse[PATH_MAX_STEPS];
    int start_x = party_x();
    int start_y = party_y();
    int start = node_index(0, 0);
    int best = start;
    int best_score = point_score(start_x, start_y, target_x, target_y);
    int head = 0;
    int tail = 0;
    int reverse_length = 0;
    int index;

    memset(g_visited, 0, sizeof(g_visited));
    g_visited[start] = 1;
    g_parent[start] = -1;
    g_queue[tail++] = (uint16_t)start;

    while (head < tail) {
        int current = g_queue[head++];
        int a = node_a(current);
        int b = node_b(current);
        int neighbor;

        for (neighbor = 0; neighbor < 4; ++neighbor) {
            int next_a = a + delta_a[neighbor];
            int next_b = b + delta_b[neighbor];
            int next_index;
            int world_x;
            int world_y;
            int score;

            if (next_a < -PATH_RADIUS || next_a > PATH_RADIUS ||
                next_b < -PATH_RADIUS || next_b > PATH_RADIUS) {
                continue;
            }
            next_index = node_index(next_a, next_b);
            if (g_visited[next_index]) continue;

            node_world_position(
                start_x, start_y, next_a, next_b, &world_x, &world_y
            );
            if (PAL_CheckObstacle(PAL_XY(world_x, world_y), TRUE, 0)) {
                continue;
            }

            g_visited[next_index] = 1;
            g_parent[next_index] = (int16_t)current;
            g_parent_direction[next_index] = (uint8_t)directions[neighbor];
            g_queue[tail++] = (uint16_t)next_index;

            score = point_score(world_x, world_y, target_x, target_y);
            if (score < best_score) {
                best_score = score;
                best = next_index;
                if (score == 0) {
                    head = tail;
                    break;
                }
            }
        }
    }

    index = best;
    while (index != start && reverse_length < PATH_MAX_STEPS) {
        reverse[reverse_length++] =
            (PALDIRECTION)g_parent_direction[index];
        index = g_parent[index];
    }
    if (index != start) return 0;

    g_path.length = reverse_length;
    g_path.next = 0;
    for (index = 0; index < reverse_length; ++index) {
        g_path.steps[index] = reverse[reverse_length - 1 - index];
    }
    return reverse_length > 0 || best_score <= 16;
}

static int find_search_object(int click_x, int click_y)
{
    int first =
        gpGlobals->g.rgScene[gpGlobals->wNumScene - 1].wEventObjectIndex;
    int end = gpGlobals->g.rgScene[gpGlobals->wNumScene].wEventObjectIndex;
    int best = 0;
    int best_score = 0x7fffffff;
    int index;

    for (index = first; index < end; ++index) {
        LPEVENTOBJECT object = &gpGlobals->g.lprgEventObject[index];
        int dx;
        int dy;
        int score;
        if (object->sState <= 0 || object->sVanishTime != 0 ||
            object->wTriggerMode < kTriggerSearchNear ||
            object->wTriggerMode > kTriggerSearchFar) {
            continue;
        }
        dx = click_x - object->x;
        dy = click_y - object->y;
        if (absolute_int(dx) > SEARCH_HIT_X ||
            dy < -SEARCH_HIT_Y_ABOVE || dy > SEARCH_HIT_Y_BELOW) {
            continue;
        }
        score = absolute_int(dx) + absolute_int(dy);
        if (score < best_score) {
            best_score = score;
            best = index + 1;
        }
    }
    return best;
}

static PALDIRECTION direction_toward(int from_x, int from_y, int to_x, int to_y)
{
    if (to_x >= from_x) {
        return to_y >= from_y ? kDirEast : kDirNorth;
    }
    return to_y >= from_y ? kDirSouth : kDirWest;
}

static int search_object_is_valid(int object_id)
{
    int first =
        gpGlobals->g.rgScene[gpGlobals->wNumScene - 1].wEventObjectIndex;
    int end = gpGlobals->g.rgScene[gpGlobals->wNumScene].wEventObjectIndex;
    LPEVENTOBJECT object;
    if (object_id <= first || object_id > end) return 0;
    object = &gpGlobals->g.lprgEventObject[object_id - 1];
    return object->sState > 0 && object->sVanishTime == 0 &&
        object->wTriggerMode >= kTriggerSearchNear &&
        object->wTriggerMode <= kTriggerSearchFar;
}

static void begin_tap_path(int screen_x, int screen_y)
{
    int click_x = PAL_X(gpGlobals->viewport) + screen_x;
    int click_y = PAL_Y(gpGlobals->viewport) + screen_y;
    int object_id = find_search_object(click_x, click_y);

    cancel_path();
    if (object_id != 0) {
        LPEVENTOBJECT object = &gpGlobals->g.lprgEventObject[object_id - 1];
        click_x = object->x;
        click_y = object->y;
    }

    g_path.scene = gpGlobals->wNumScene;
    g_path.target_x = click_x;
    g_path.target_y = click_y;
    g_path.interaction_object = object_id;
    if (build_path(click_x, click_y)) {
        g_path.active = 1;
    } else {
        cancel_path();
    }
}

static void before_frame(void)
{
    const DWORD cancel_keys =
        kKeyMenu | kKeySearch | kKeyDown | kKeyLeft | kKeyUp | kKeyRight;
    int screen_x;
    int screen_y;

    if (!game_accepts_path()) {
        cancel_path();
        return;
    }

    if (g_InputState.dwKeyPress & cancel_keys) {
        cancel_path();
        return;
    }
    if (g_path.injected && g_InputState.dir != g_path.injected_direction) {
        cancel_path();
        return;
    }

    if (pal9588_platform_take_tap(&screen_x, &screen_y)) {
        if (!g_path.active && g_InputState.dir != kDirUnknown) return;
        begin_tap_path(screen_x, screen_y);
    }

    if (!g_path.active) return;
    if (g_path.scene != gpGlobals->wNumScene) {
        cancel_path();
        return;
    }

    if (g_path.next < g_path.length) {
        int x_offset;
        int y_offset;
        PALDIRECTION direction = g_path.steps[g_path.next];
        direction_offset(direction, &x_offset, &y_offset);
        g_path.expected_x = party_x() + x_offset;
        g_path.expected_y = party_y() + y_offset;
        g_path.injected_direction = direction;
        g_path.injected = 1;
        g_InputState.prevdir = kDirUnknown;
        g_InputState.dir = direction;
        return;
    }

    if (g_path.interaction_object != 0 &&
        search_object_is_valid(g_path.interaction_object)) {
        LPEVENTOBJECT object =
            &gpGlobals->g.lprgEventObject[g_path.interaction_object - 1];
        gpGlobals->wPartyDirection = direction_toward(
            party_x(), party_y(), object->x, object->y
        );
        g_InputState.dir = kDirUnknown;
        g_InputState.prevdir = kDirUnknown;
        g_InputState.dwKeyPress |= kKeySearch;
    }
    cancel_path();
}

static void after_frame(void)
{
    if (!g_path.injected) return;

    if (g_InputState.dir == g_path.injected_direction) {
        g_InputState.dir = kDirUnknown;
        g_InputState.prevdir = kDirUnknown;
    }
    g_path.injected = 0;

    if (!game_accepts_path() || g_path.scene != gpGlobals->wNumScene) {
        cancel_path();
        return;
    }
    if (party_x() == g_path.expected_x && party_y() == g_path.expected_y) {
        ++g_path.next;
        return;
    }

    if (g_path.replans++ < REPLAN_LIMIT &&
        build_path(g_path.target_x, g_path.target_y)) {
        return;
    }
    cancel_path();
}

VOID PAL_StartFrame(VOID)
{
    before_frame();
    PAL9588_InternalStartFrame();
    after_frame();
}
