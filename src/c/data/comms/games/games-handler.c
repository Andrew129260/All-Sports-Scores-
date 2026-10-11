#include "pebble.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../comms.h"
#include "games-handler.h"
#include "../../model/models.h"

// Separate request tracking
static int current_list_request = -1;
static int current_update_request = -1;
static int current_breakdown_request = -1;

static int games_count = 0;
static int expected_games = 0;

#if defined(PBL_PLATFORM_APLITE)
    #define MAX_GAMES 5
#elif defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    #define MAX_GAMES 150
#else
    #define MAX_GAMES 50
#endif

// Aplite's fixed-size string pool
#if defined(PBL_PLATFORM_APLITE)
typedef struct {
    char league[16];
    char t1_name[6];
    char t2_name[6];
    char t1_score[12];
    char t2_score[12];
    char t1_record[12];
    char t2_record[12];
    char time[22];
    char details[32];
    char broadcast[16];
} GameStringPool;

static GameStringPool s_string_pool[MAX_GAMES];
#endif

static Game s_games[MAX_GAMES];

static GameListSuccessCallback on_games_success = NULL;
static GameListErrorCallback on_games_error = NULL;
static GameUpdateCallback on_game_update = NULL;

static BreakdownSuccessCallback on_breakdown_success = NULL;
static GameListErrorCallback on_breakdown_error = NULL;

static char empty_string[] = "";

// =========================================================
// MEMORY MANAGEMENT
// =========================================================

static void safe_free(char *str) {
    if (!str || str == empty_string) {
        return;
    }

#if defined(PBL_PLATFORM_APLITE)
    uintptr_t address = (uintptr_t)str;
    uintptr_t pool_start = (uintptr_t)&s_string_pool[0];
    uintptr_t pool_end =
        (uintptr_t)&s_string_pool[MAX_GAMES];

    if (address >= pool_start && address < pool_end) {
        return;
    }
#endif

    free(str);
}

static void clear_game(Game *game) {
    if (!game) return;

    safe_free(game->league);

    safe_free(game->team1.name);
    safe_free(game->team1.score);
    safe_free(game->team1.record);

    safe_free(game->team2.name);
    safe_free(game->team2.score);
    safe_free(game->team2.record);

    safe_free(game->time);
    safe_free(game->summary);
    safe_free(game->details);
    safe_free(game->broadcast);

    memset(game, 0, sizeof(Game));
}

static void free_games_array(void) {
    for (int i = 0; i < games_count; i++) {
        clear_game(&s_games[i]);
    }

    games_count = 0;
    expected_games = 0;
}

void handle_cancel_breakdown_request(void) {
    current_breakdown_request = -1;
    on_breakdown_success = NULL;
    on_breakdown_error = NULL;
}

void handle_clear_games(void) {
    current_list_request = -1;
    current_update_request = -1;

    handle_cancel_breakdown_request();
    free_games_array();

    on_games_success = NULL;
    on_games_error = NULL;
    on_game_update = NULL;
}

// =========================================================
// DICTIONARY HELPERS
// =========================================================

static int32_t get_dict_int_safe(
    DictionaryIterator *iter,
    uint32_t key,
    int32_t default_val
) {
    if (!iter) return default_val;

    Tuple *t = dict_find(iter, key);
    if (!t) return default_val;

    if (t->type == TUPLE_INT) {
        switch (t->length) {
            case 1: return t->value->int8;
            case 2: return t->value->int16;
            case 4: return t->value->int32;
            default: return default_val;
        }
    }

    if (t->type == TUPLE_UINT) {
        switch (t->length) {
            case 1: return t->value->uint8;
            case 2: return t->value->uint16;
            case 4:
                if (t->value->uint32 <= INT32_MAX) {
                    return (int32_t)t->value->uint32;
                }
                return default_val;
            default: return default_val;
        }
    }

    return default_val;
}

static char *copy_dict_string(
    DictionaryIterator *dict,
    uint32_t key,
    const char *debug_name,
    char *aplite_buf,
    size_t aplite_buf_size
) {
    Tuple *tuple = dict_find(dict, key);

    if (!tuple ||
        tuple->type != TUPLE_CSTRING ||
        tuple->length == 0) {
        return empty_string;
    }

    // Validate that the tuple really contains a terminator.
    const char *source = tuple->value->cstring;

    const char *end = memchr(
        source,
        '\0',
        tuple->length
    );

    if (!end) {
        APP_LOG(APP_LOG_LEVEL_WARNING,
                "Invalid string: %s", debug_name);
        return empty_string;
    }

    size_t len = (size_t)(end - source);

#if defined(PBL_PLATFORM_APLITE)
    if (aplite_buf && aplite_buf_size > 0) {
        size_t safe_len = len;

        if (safe_len >= aplite_buf_size) {
            safe_len = aplite_buf_size - 1;
        }

        memcpy(aplite_buf, source, safe_len);
        aplite_buf[safe_len] = '\0';

        return aplite_buf;
    }
#endif

    char *str = malloc(len + 1);

    if (!str) {
        APP_LOG(APP_LOG_LEVEL_ERROR,
                "MALLOC FAILED: %s (%d)",
                debug_name, (int)len);

        return empty_string;
    }

    memcpy(str, source, len);
    str[len] = '\0';

    return str;
}

// =========================================================
// CALLBACK COMPLETION
// =========================================================

static void finish_list_error(AppError error) {
    GameListErrorCallback callback = on_games_error;

    current_list_request = -1;
    on_games_success = NULL;
    on_games_error = NULL;

    if (callback) callback(error);
}

static void finish_list_success(void) {
    GameListSuccessCallback callback = on_games_success;

    current_list_request = -1;
    on_games_success = NULL;
    on_games_error = NULL;

    if (callback) callback(games_count, s_games);
}

static void finish_update(GameUpdateResult result) {
    GameUpdateCallback callback = on_game_update;

    current_update_request = -1;
    on_game_update = NULL;

    if (callback) callback(result);
}

// =========================================================
// GAME LIST REQUEST
// =========================================================

void handle_request_games(
    Sport sport,
    int league_index,
    GameListSuccessCallback on_success,
    GameListErrorCallback on_error
) {
    handle_clear_games();

    int request_id = rand();

    DictionaryIterator *out_iter = NULL;

    AppMessageResult result =
        app_message_outbox_begin(&out_iter);

    if (result != APP_MSG_OK || !out_iter) {
        if (on_error) on_error(ConnectionError);
        return;
    }

    Tuplet sport_tuple =
        TupletInteger(MESSAGE_KEY_LOAD_GAMES, sport);

    Tuplet request_tuple =
        TupletInteger(MESSAGE_KEY_REQUEST_ID, request_id);

    Tuplet league_tuple =
        TupletInteger(MESSAGE_KEY_LEAGUE_INDEX, league_index);

    if (dict_write_tuplet(out_iter, &sport_tuple) != DICT_OK ||
        dict_write_tuplet(out_iter, &request_tuple) != DICT_OK ||
        dict_write_tuplet(out_iter, &league_tuple) != DICT_OK) {

        if (on_error) on_error(ConnectionError);
        return;
    }

    result = app_message_outbox_send();

    if (result != APP_MSG_OK) {
        APP_LOG(APP_LOG_LEVEL_ERROR,
                "Game request send failed: %d", (int)result);

        if (on_error) on_error(ConnectionError);
        return;
    }

    current_list_request = request_id;
    on_games_success = on_success;
    on_games_error = on_error;
}

// =========================================================
// BREAKDOWN REQUEST
// =========================================================

void handle_request_breakdown(
    Game *game,
    BreakdownSuccessCallback on_success,
    GameListErrorCallback on_error
) {
    if (!game) {
        if (on_error) on_error(ConnectionError);
        return;
    }

    // Invalidate any older breakdown request.
    handle_cancel_breakdown_request();

    int request_id = rand();

    DictionaryIterator *out_iter = NULL;

    AppMessageResult result =
        app_message_outbox_begin(&out_iter);

    if (result != APP_MSG_OK || !out_iter) {
        if (on_error) on_error(ConnectionError);
        return;
    }

    Tuplet sport_tuple =
        TupletInteger(MESSAGE_KEY_REQUEST_BREAKDOWN, game->sport);

    Tuplet game_tuple =
        TupletInteger(MESSAGE_KEY_UPDATE_GAME_ID, game->id);

    Tuplet request_tuple =
        TupletInteger(MESSAGE_KEY_REQUEST_ID, request_id);

    bool writes_ok =
        dict_write_tuplet(out_iter, &sport_tuple) == DICT_OK &&
        dict_write_tuplet(out_iter, &game_tuple) == DICT_OK &&
        dict_write_tuplet(out_iter, &request_tuple) == DICT_OK;

    if (writes_ok && game->league && game->league[0]) {
        writes_ok =
            dict_write_cstring(
                out_iter,
                MESSAGE_KEY_SEND_GAME_LEAGUE,
                game->league
            ) == DICT_OK;
    }

    if (!writes_ok) {
        if (on_error) on_error(ConnectionError);
        return;
    }

    result = app_message_outbox_send();

    if (result != APP_MSG_OK) {
        APP_LOG(APP_LOG_LEVEL_ERROR,
                "Breakdown send failed: %d", (int)result);

        if (on_error) on_error(ConnectionError);
        return;
    }

    current_breakdown_request = request_id;
    on_breakdown_success = on_success;
    on_breakdown_error = on_error;
}

void handle_breakdown_received(DictionaryIterator *iter) {
    int request_id =
        get_dict_int_safe(iter, MESSAGE_KEY_REQUEST_ID, -1);

    if (current_breakdown_request == -1 ||
        request_id != current_breakdown_request) {

        APP_LOG(APP_LOG_LEVEL_WARNING,
                "Ignoring stale breakdown response");
        return;
    }

    Tuple *tuple =
        dict_find(iter, MESSAGE_KEY_LOAD_BREAKDOWN);

    BreakdownSuccessCallback success = on_breakdown_success;
    GameListErrorCallback error = on_breakdown_error;

    // Prevent duplicate callbacks before invoking the UI.
    handle_cancel_breakdown_request();

    if (tuple &&
        tuple->type == TUPLE_CSTRING &&
        tuple->length > 0 &&
        memchr(tuple->value->cstring, '\0', tuple->length)) {

        if (success) {
            // Callback must copy this string immediately.
            success(tuple->value->cstring);
        }

    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR,
                "Invalid breakdown response");

        if (error) error(NetworkError);
    }
}

// =========================================================
// GAME REFRESH REQUEST
// =========================================================

void update_game(Game *game, GameUpdateCallback on_update) {
    if (!game) {
        if (on_update) on_update(GameUpdateNetworkError);
        return;
    }

    // Don't overlap refresh operations.
    if (current_update_request != -1) {
        APP_LOG(APP_LOG_LEVEL_WARNING,
                "Game refresh already pending");
        if (on_update) on_update(GameUpdateNetworkError);
        return;
    }

    int request_id = rand();

    DictionaryIterator *out_iter = NULL;

    AppMessageResult result =
        app_message_outbox_begin(&out_iter);

    if (result != APP_MSG_OK || !out_iter) {
        if (on_update) on_update(GameUpdateNetworkError);
        return;
    }

    Tuplet game_tuple =
        TupletInteger(MESSAGE_KEY_UPDATE_GAME_ID, game->id);

    Tuplet sport_tuple =
        TupletInteger(MESSAGE_KEY_UPDATE_GAME_SPORT, game->sport);

    Tuplet request_tuple =
        TupletInteger(MESSAGE_KEY_REQUEST_ID, request_id);

    if (dict_write_tuplet(out_iter, &game_tuple) != DICT_OK ||
        dict_write_tuplet(out_iter, &sport_tuple) != DICT_OK ||
        dict_write_tuplet(out_iter, &request_tuple) != DICT_OK) {

        if (on_update) on_update(GameUpdateNetworkError);
        return;
    }

    result = app_message_outbox_send();

    if (result != APP_MSG_OK) {
        if (on_update) on_update(GameUpdateNetworkError);
        return;
    }

    current_update_request = request_id;
    on_game_update = on_update;
}

// =========================================================
// GAME DATA PARSING
// =========================================================

void game_set(Game *game, DictionaryIterator *iter) {
    if (!game || !iter) return;

#if defined(PBL_PLATFORM_APLITE)
    // Only pool storage for entries in our static array.
    GameStringPool *pool = NULL;

    uintptr_t address = (uintptr_t)game;
    uintptr_t start = (uintptr_t)&s_games[0];
    uintptr_t end = (uintptr_t)&s_games[MAX_GAMES];

    if (address >= start && address < end) {
        size_t offset = (size_t)(address - start);

        if (offset % sizeof(Game) == 0) {
            size_t index = offset / sizeof(Game);

            if (index < MAX_GAMES) {
                pool = &s_string_pool[index];
            }
        }
    }

#define GET_STR(key, name, field) \
    copy_dict_string(iter, key, name, \
        pool ? pool->field : NULL, \
        pool ? sizeof(pool->field) : 0)
#else
#define GET_STR(key, name, field) \
    copy_dict_string(iter, key, name, NULL, 0)
#endif

    Game parsed = {0};

    parsed.id = get_dict_int_safe(
        iter, MESSAGE_KEY_SEND_GAME_ID, 0);

    parsed.sport = get_dict_int_safe(
        iter, MESSAGE_KEY_SEND_GAME_SPORT, 0);

    parsed.league =
        GET_STR(MESSAGE_KEY_SEND_GAME_LEAGUE,
                "League", league);

    char *t1_name =
        GET_STR(MESSAGE_KEY_SEND_GAME_TEAM_1_NAME,
                "Team1Name", t1_name);

    char *t2_name =
        GET_STR(MESSAGE_KEY_SEND_GAME_TEAM_2_NAME,
                "Team2Name", t2_name);

    char *t1_score =
        GET_STR(MESSAGE_KEY_SEND_GAME_TEAM_1_SCORE,
                "Team1Score", t1_score);

    char *t2_score =
        GET_STR(MESSAGE_KEY_SEND_GAME_TEAM_2_SCORE,
                "Team2Score", t2_score);

    parsed.team1 = (Team) {
        .name = t1_name,
        .score = t1_score,
        .id = get_dict_int_safe(
            iter, MESSAGE_KEY_SEND_GAME_TEAM_1_ID, 0),
        .favorite = get_dict_int_safe(
            iter, MESSAGE_KEY_SEND_GAME_TEAM_1_FAVORITE, 0),
        .winner = get_dict_int_safe(
            iter, MESSAGE_KEY_SEND_GAME_TEAM_1_WINNER, 0),
        .record = GET_STR(
            MESSAGE_KEY_SEND_GAME_TEAM_1_RECORD,
            "Team1Record", t1_record)
    };

    parsed.team2 = (Team) {
        .name = t2_name,
        .score = t2_score,
        .id = get_dict_int_safe(
            iter, MESSAGE_KEY_SEND_GAME_TEAM_2_ID, 0),
        .favorite = get_dict_int_safe(
            iter, MESSAGE_KEY_SEND_GAME_TEAM_2_FAVORITE, 0),
        .winner = get_dict_int_safe(
            iter, MESSAGE_KEY_SEND_GAME_TEAM_2_WINNER, 0),
        .record = GET_STR(
            MESSAGE_KEY_SEND_GAME_TEAM_2_RECORD,
            "Team2Record", t2_record)
    };

    parsed.possession = get_dict_int_safe(
        iter, MESSAGE_KEY_SEND_GAME_POSSESSION, 2);

    parsed.time =
        GET_STR(MESSAGE_KEY_SEND_GAME_TIME,
                "Time", time);

    parsed.details =
        GET_STR(MESSAGE_KEY_SEND_GAME_DETAILS,
                "Details", details);

    parsed.broadcast =
        GET_STR(MESSAGE_KEY_SEND_GAME_BROADCAST,
                "Broadcast", broadcast);

#if defined(PBL_ROUND)
    size_t summary_len =
        strlen(t1_name) + strlen(t1_score) +
        strlen(t2_score) + strlen(t2_name) + 6;

    char *summary = malloc(summary_len);

    if (summary) {
        snprintf(
            summary,
            summary_len,
            "%s %s - %s %s",
            t1_name, t1_score,
            t2_score, t2_name
        );

        parsed.summary = summary;
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR,
                "MALLOC FAILED: Summary");

        parsed.summary = empty_string;
    }
#else
    parsed.summary = empty_string;
#endif

    *game = parsed;

#undef GET_STR
}

// =========================================================
// RECEIVE GAME LIST
// =========================================================

void handle_games_recieved(DictionaryIterator *iter) {
    int request_id =
        get_dict_int_safe(iter, MESSAGE_KEY_REQUEST_ID, -1);

    if (current_list_request == -1 ||
        request_id != current_list_request) {
        return;
    }

    GamesListState data = get_dict_int_safe(
        iter,
        MESSAGE_KEY_SEND_GAME_LIST,
        GamesListNetworkError
    );

    if (data == GamesListNoGames) {
        finish_list_error(NoGames);
        return;
    }

    if (data == GamesListNetworkError) {
        finish_list_error(NetworkError);
        return;
    }

    // Retain your existing initialization marker.
    if ((int)data == 4) {
        int total_games = get_dict_int_safe(
            iter,
            MESSAGE_KEY_SEND_GAME_ID,
            0
        );

        free_games_array();

        if (total_games <= 0) {
            finish_list_error(NoGames);
            return;
        }

        expected_games =
            total_games > MAX_GAMES ? MAX_GAMES : total_games;

        return;
    }

    // A game item should never precede initialization.
    if (expected_games <= 0) {
        finish_list_error(NetworkError);
        return;
    }

    if (data != GamesListItem &&
        data != GamesListLastItem) {
        finish_list_error(NetworkError);
        return;
    }

    if (games_count >= expected_games ||
        games_count >= MAX_GAMES) {

        APP_LOG(APP_LOG_LEVEL_ERROR,
                "Unexpected extra game item");

        finish_list_error(NetworkError);
        return;
    }

    Game *new_game = &s_games[games_count];

    memset(new_game, 0, sizeof(Game));
    game_set(new_game, iter);
    games_count++;

    if (data == GamesListLastItem) {
        if (games_count != expected_games) {
            APP_LOG(APP_LOG_LEVEL_ERROR,
                    "Incomplete list: %d of %d",
                    games_count, expected_games);

            finish_list_error(NetworkError);
            return;
        }

        finish_list_success();
    }
}

// =========================================================
// RECEIVE GAME REFRESH
// =========================================================

void handle_game_update_recieved(DictionaryIterator *iter) {
    int request_id =
        get_dict_int_safe(iter, MESSAGE_KEY_REQUEST_ID, -1);

    if (current_update_request == -1 ||
        request_id != current_update_request) {

        APP_LOG(APP_LOG_LEVEL_WARNING,
                "Ignoring stale game update");
        return;
    }

    GameUpdateResult data = get_dict_int_safe(
        iter,
        MESSAGE_KEY_SEND_GAME_UPDATE,
        GameUpdateNetworkError
    );

    if (data != GameUpdated) {
        finish_update(GameUpdateNetworkError);
        return;
    }

    int game_id = get_dict_int_safe(
        iter,
        MESSAGE_KEY_SEND_GAME_ID,
        0
    );

    for (int i = 0; i < games_count; i++) {
        if (s_games[i].id == game_id) {

            // The Game address remains stable.
            // Its strings are replaced synchronously.
            clear_game(&s_games[i]);
            game_set(&s_games[i], iter);

            finish_update(GameUpdated);
            return;
        }
    }

    APP_LOG(APP_LOG_LEVEL_WARNING,
            "Refresh game ID not found: %d", game_id);

    finish_update(GameUpdateNetworkError);
}

// =========================================================
// OUTBOX FAILURE ROUTING
// =========================================================

void handle_game_outbox_failed(DictionaryIterator *iter) {
    if (!iter) return;

    int request_id =
        get_dict_int_safe(iter, MESSAGE_KEY_REQUEST_ID, -1);

    if (dict_find(iter, MESSAGE_KEY_REQUEST_BREAKDOWN) &&
        request_id == current_breakdown_request) {

        GameListErrorCallback callback = on_breakdown_error;

        handle_cancel_breakdown_request();

        if (callback) callback(ConnectionError);
        return;
    }

    if (dict_find(iter, MESSAGE_KEY_UPDATE_GAME_SPORT) &&
        request_id == current_update_request) {

        finish_update(GameUpdateNetworkError);
        return;
    }

    if (dict_find(iter, MESSAGE_KEY_LOAD_GAMES) &&
        request_id == current_list_request) {

        finish_list_error(ConnectionError);
        return;
    }
}