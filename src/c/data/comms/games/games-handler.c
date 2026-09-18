#include "pebble.h"
#include "../comms.h"
#include "games-handler.h"
#include "../../model/models.h"

static int current_request = -1;
static int games_count = 0;
static int expected_games = 0;

#if defined(PBL_PLATFORM_APLITE)
    #define MAX_GAMES 5
#elif defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    #define MAX_GAMES 150
#else
    #define MAX_GAMES 50
#endif

// --- STATIC STRING POOL ---
// Eliminates 50 malloc() calls per sync on legacy hardware
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

static GameListSuccessCallback on_games_success;
static GameListErrorCallback on_games_error;
static GameUpdateCallback on_game_update;

static char empty_string[] = "";

#define GAMES_LIST_INIT_ARRAY 4

static void safe_free(char *str) {
    if (str != NULL && str != empty_string) {
        #if defined(PBL_PLATFORM_APLITE)
        // Never attempt to free pointers that belong to the static string pool
        if ((void*)str >= (void*)s_string_pool && (void*)str < (void*)(s_string_pool + MAX_GAMES)) {
            return;
        }
        #endif
        free(str);
    }
}

static void clear_game(Game *game) {
    if (game == NULL) {
        return;
    }

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

    game->league = NULL;
    game->team1.name = NULL;
    game->team1.score = NULL;
    game->team1.record = NULL;
    game->team2.name = NULL;
    game->team2.score = NULL;
    game->team2.record = NULL;

    game->time = NULL;
    game->summary = NULL;
    game->details = NULL;
    game->broadcast = NULL;
}

static void free_games_array(void) {
    for (int i = 0; i < games_count; i++) {
        clear_game(&s_games[i]);
    }

    games_count = 0;
    expected_games = 0;
}

void handle_clear_games(void) {
    current_request = -1;
    free_games_array();
    on_games_success = NULL;
    on_games_error = NULL;
    on_game_update = NULL;
}

void handle_request_games(
    Sport sport,
    int league_index,
    GameListSuccessCallback on_success,
    GameListErrorCallback on_error
) {
    handle_clear_games();

    int request_id = rand();

    DictionaryIterator *out_iter;
    AppMessageResult result = app_message_outbox_begin(&out_iter);

    if (result == APP_MSG_OK) {
        Tuplet update_game_tuple = TupletInteger(MESSAGE_KEY_LOAD_GAMES, sport);
        dict_write_tuplet(out_iter, &update_game_tuple);

        Tuplet request_id_tuple = TupletInteger(MESSAGE_KEY_REQUEST_ID, request_id);
        dict_write_tuplet(out_iter, &request_id_tuple);

        Tuplet league_index_tuple = TupletInteger(MESSAGE_KEY_LEAGUE_INDEX, league_index);
        dict_write_tuplet(out_iter, &league_index_tuple);

        result = app_message_outbox_send();

        if (result != APP_MSG_OK) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending outbox: %d", (int)result);
            if (on_error) on_error(ConnectionError);
        } else {
            current_request = request_id;
            on_games_success = on_success;
            on_games_error = on_error;
        }
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing outbox: %d", (int)result);
        if (on_error) on_error(ConnectionError);
    }
}

void update_game(Game *game, GameUpdateCallback on_update) {
    if (game == NULL) {
        if (on_update) on_update(GameUpdateNetworkError);
        return;
    }

    int request_id = rand();

    DictionaryIterator *out_iter;
    AppMessageResult result = app_message_outbox_begin(&out_iter);

    if (result == APP_MSG_OK) {
        Tuplet update_game_id_tuple = TupletInteger(MESSAGE_KEY_UPDATE_GAME_ID, game->id);
        Tuplet update_game_sport_tuple = TupletInteger(MESSAGE_KEY_UPDATE_GAME_SPORT, game->sport);

        dict_write_tuplet(out_iter, &update_game_id_tuple);
        dict_write_tuplet(out_iter, &update_game_sport_tuple);

        Tuplet request_id_tuple = TupletInteger(MESSAGE_KEY_REQUEST_ID, request_id);
        dict_write_tuplet(out_iter, &request_id_tuple);

        result = app_message_outbox_send();

        if (result != APP_MSG_OK) {
            if (on_update) on_update(GameUpdateNetworkError);
        } else {
            current_request = request_id;
            on_game_update = on_update;
        }
    } else {
        if (on_update) on_update(GameUpdateNetworkError);
    }
}

static char *copy_dict_string(
    DictionaryIterator *dict,
    uint32_t key,
    const char *debug_name,
    char *aplite_buf,
    size_t aplite_buf_size
) {
    Tuple *tuple = dict_find(dict, key);

    if (!tuple || tuple->type != TUPLE_CSTRING || tuple->length == 0) {
        return empty_string;
    }

    // SECURITY PATCH: Rely strictly on the dictionary's byte-length header, not an assumed null terminator.
    size_t len = tuple->length > 0 ? tuple->length - 1 : 0;

    #if defined(PBL_PLATFORM_APLITE)
    if (aplite_buf != NULL && aplite_buf_size > 0) {
        // Enforce the static pool boundary regardless of the incoming packet length
        size_t safe_len = len < (aplite_buf_size - 1) ? len : (aplite_buf_size - 1);
        memcpy(aplite_buf, tuple->value->cstring, safe_len);
        aplite_buf[safe_len] = '\0';
        return aplite_buf;
    }
    #endif

    char *str = malloc(len + 1);

    if (str != NULL) {
        memcpy(str, tuple->value->cstring, len);
        str[len] = '\0'; // Force null termination securely
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "DIAGNOSTIC: MALLOC FAILED for %s (Len: %d)", debug_name, (int)len);
        return empty_string;
    }

    return str;
}

static int32_t get_dict_int_safe(DictionaryIterator *iter, uint32_t key, int32_t default_val) {
    Tuple *t = dict_find(iter, key);
    if (!t) return default_val;

    switch (t->length) {
        case 1: return t->value->int8;
        case 2: return t->value->int16;
        case 4: return t->value->int32;
        default: return default_val;
    }
}

void game_set(Game *game, DictionaryIterator *iter) {
    if (game == NULL) return;

    // Use pointer math to safely grab the correct string pool index
    #if defined(PBL_PLATFORM_APLITE)
        int pool_idx = game - s_games;
        bool use_pool = (pool_idx >= 0 && pool_idx < MAX_GAMES);
        GameStringPool *pool = use_pool ? &s_string_pool[pool_idx] : NULL;
        #define GET_STR(key, debug, field) copy_dict_string(iter, key, debug, pool ? pool->field : NULL, pool ? sizeof(pool->field) : 0)
    #else
        #define GET_STR(key, debug, field) copy_dict_string(iter, key, debug, NULL, 0)
    #endif

    Game parsed = {0};

    parsed.id = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_ID, 0);
    parsed.sport = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_SPORT, 0);
    parsed.league = GET_STR(MESSAGE_KEY_SEND_GAME_LEAGUE, "League", league);

    char *team_1_name = GET_STR(MESSAGE_KEY_SEND_GAME_TEAM_1_NAME, "Team1Name", t1_name);
    char *team_2_name = GET_STR(MESSAGE_KEY_SEND_GAME_TEAM_2_NAME, "Team2Name", t2_name);
    char *team_1_score = GET_STR(MESSAGE_KEY_SEND_GAME_TEAM_1_SCORE, "Team1Score", t1_score);
    char *team_2_score = GET_STR(MESSAGE_KEY_SEND_GAME_TEAM_2_SCORE, "Team2Score", t2_score);

    parsed.team1 = (Team) {
        .name = team_1_name,
        .score = team_1_score,
        .id = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_TEAM_1_ID, 0),
        .favorite = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_TEAM_1_FAVORITE, 0),
        .winner = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_TEAM_1_WINNER, 0),
        .record = GET_STR(MESSAGE_KEY_SEND_GAME_TEAM_1_RECORD, "Team1Record", t1_record),
    };

    parsed.team2 = (Team) {
        .name = team_2_name,
        .score = team_2_score,
        .id = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_TEAM_2_ID, 0),
        .favorite = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_TEAM_2_FAVORITE, 0),
        .winner = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_TEAM_2_WINNER, 0),
        .record = GET_STR(MESSAGE_KEY_SEND_GAME_TEAM_2_RECORD, "Team2Record", t2_record),
    };

    parsed.possession = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_POSSESSION, 2);
    parsed.time = GET_STR(MESSAGE_KEY_SEND_GAME_TIME, "Time", time);
    parsed.details = GET_STR(MESSAGE_KEY_SEND_GAME_DETAILS, "Details", details);
    parsed.broadcast = GET_STR(MESSAGE_KEY_SEND_GAME_BROADCAST, "Broadcast", broadcast);

#if defined(PBL_ROUND)
    size_t summary_len = strlen(team_1_name) + strlen(team_1_score) + strlen(team_2_score) + strlen(team_2_name) + 6;
    char *summary = malloc(summary_len);

    if (summary != NULL) {
        snprintf(summary, summary_len, "%s %s - %s %s", team_1_name, team_1_score, team_2_score, team_2_name);
        parsed.summary = summary;
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "DIAGNOSTIC: MALLOC FAILED for Summary");
        parsed.summary = empty_string;
    }
#else
    parsed.summary = empty_string;
#endif

    *game = parsed;
}

void handle_games_recieved(DictionaryIterator *iter) {
    int request_id = get_dict_int_safe(iter, MESSAGE_KEY_REQUEST_ID, 0);

    if (request_id != current_request) {
        return;
    }

    GamesListState data = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_LIST, GamesListNetworkError);

    APP_LOG(APP_LOG_LEVEL_INFO, "--> RECV Item. Index: %d, State: %d, Free RAM: %d", games_count, data, (int)heap_bytes_free());

    if (data == GamesListNoGames) {
        if (on_games_error) on_games_error(NoGames);
        return;
    }

    if (data == GamesListNetworkError) {
        if (on_games_error) on_games_error(NetworkError);
        return;
    }

    if (data == GAMES_LIST_INIT_ARRAY) {
        int total_games = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_ID, 0);
        
        APP_LOG(APP_LOG_LEVEL_INFO, "DIAGNOSTIC: Init Array. Total expected: %d", total_games);

        free_games_array();

        if (total_games <= 0) {
            expected_games = 0;
            return;
        }

        if (total_games > MAX_GAMES) {
            expected_games = MAX_GAMES;
        } else {
            expected_games = total_games;
        }
        return;
    }

    if (games_count >= MAX_GAMES) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "CRITICAL: Prevented out-of-bounds array write! Index: %d / %d", games_count, MAX_GAMES);
    } else {
        Game *new_game = &s_games[games_count];
        memset(new_game, 0, sizeof(Game));
        
        game_set(new_game, iter);
        games_count++;

        APP_LOG(APP_LOG_LEVEL_INFO, "DIAGNOSTIC: Stored game %d/%d. Free RAM: %d", games_count, expected_games > 0 ? expected_games : MAX_GAMES, (int)heap_bytes_free());
    }

    if (data == GamesListLastItem) {
        APP_LOG(APP_LOG_LEVEL_INFO, "DIAGNOSTIC: Last item received. Triggering UI callback.");
        if (on_games_success) {
            on_games_success(games_count, s_games);
        } else {
            APP_LOG(APP_LOG_LEVEL_ERROR, "!!! CRITICAL: UI callback pointer is NULL !!!");
        }
    }
}

void handle_game_update_recieved(DictionaryIterator *iter) {
    GameUpdateResult data = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_UPDATE, GameUpdateNetworkError);

    if (data == GameUpdateNetworkError) {
        if (on_game_update) on_game_update(GameUpdateNetworkError);
        return;
    }

    int game_id = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_ID, 0);

    for (int i = 0; i < games_count; i++) {
        if (s_games[i].id == game_id) {
            clear_game(&s_games[i]);
            game_set(&s_games[i], iter);
            
            if (on_game_update) {
                on_game_update(GameUpdated);
            }
            return;
        }
    }
}