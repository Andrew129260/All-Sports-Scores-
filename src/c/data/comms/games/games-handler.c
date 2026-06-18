#include "pebble.h"
#include "../comms.h"
#include "games-handler.h"
#include "../../model/models.h"

static int current_request = -1;
static int games_count = 0;
static Game **games = NULL;

static GameListSuccessCallback on_games_success;
static GameListErrorCallback on_games_error;
static GameUpdateCallback on_game_update;

static char empty_string[] = "";

#define GAMES_LIST_INIT_ARRAY 4

static void safe_free(char *str) {
    if (str != NULL && str != empty_string) {
        free(str);
    }
}

static void clear_game(Game *game) {
    if (game == NULL) return;
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
    free(game);
}

static void free_games_array() {
    if (games != NULL) {
        for (int i = 0; i < games_count; i++) {
            clear_game(games[i]);
        } 
        free(games);
        games = NULL;
    }
    games_count = 0;
}

void handle_clear_games() {
    current_request = -1;
    free_games_array();
    on_games_success = NULL;
    on_games_error = NULL;
}

void handle_request_games(Sport sport, int league_index, GameListSuccessCallback on_success, GameListErrorCallback on_error) {
    handle_clear_games();
    int request_id = rand();
    
    DictionaryIterator *out_iter;
    AppMessageResult result = app_message_outbox_begin(&out_iter);

    if(result == APP_MSG_OK) { 
        Tuplet update_game_tuple = TupletInteger(MESSAGE_KEY_LOAD_GAMES, sport);
        dict_write_tuplet(out_iter, &update_game_tuple);

        Tuplet request_id_tuple = TupletInteger(MESSAGE_KEY_REQUEST_ID, request_id);
        dict_write_tuplet(out_iter, &request_id_tuple);

        Tuplet league_index_tuple = TupletInteger(MESSAGE_KEY_LEAGUE_INDEX, league_index);
        dict_write_tuplet(out_iter, &league_index_tuple);

        result = app_message_outbox_send();

        if(result != APP_MSG_OK) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending outbox: %d", (int)result);
            if(on_error) on_error(ConnectionError);
        } else {
            current_request = request_id;
            on_games_success = on_success;
            on_games_error = on_error;
        }
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing outbox: %d", (int)result);
        if(on_error) on_error(ConnectionError);
    }
}

void update_game(Game *game, GameUpdateCallback on_update) {
    int request_id = rand();
    DictionaryIterator *out_iter;
    AppMessageResult result = app_message_outbox_begin(&out_iter);

    if(result == APP_MSG_OK) { 
        Tuplet update_game_id_tuple = TupletInteger(MESSAGE_KEY_UPDATE_GAME_ID, game->id);
        Tuplet update_game_sport_tuple = TupletInteger(MESSAGE_KEY_UPDATE_GAME_SPORT, game->sport);
        dict_write_tuplet(out_iter, &update_game_id_tuple);
        dict_write_tuplet(out_iter, &update_game_sport_tuple);

        Tuplet request_id_tuple = TupletInteger(MESSAGE_KEY_REQUEST_ID, request_id);
        dict_write_tuplet(out_iter, &request_id_tuple);

        result = app_message_outbox_send();

        if(result != APP_MSG_OK) {
            if (on_update) on_update(GameUpdateNetworkError);
        } else {
            current_request = request_id;
            on_game_update = on_update;
        }
    } else {
        if (on_update) on_update(GameUpdateNetworkError);
    }
}

static char *memorize_dict_string(const DictionaryIterator *dict, uint32_t key, const char* debug_name) {
    Tuple *tuple = dict_find(dict, key);
    
    // DATA ARMOR: Reject anything that isn't a string to stop strlen() crashes
    if (!tuple) {
        return empty_string;
    }
    if (tuple->type != TUPLE_CSTRING) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "DIAGNOSTIC: Key %s is NOT a string! Type: %d", debug_name, tuple->type);
        return empty_string;
    }
    if (strlen(tuple->value->cstring) == 0) {
        return empty_string;
    }
    
    int len = strlen(tuple->value->cstring);
    char *str = malloc(len + 1);
    
    if (str != NULL) {
        strcpy(str, tuple->value->cstring);
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "DIAGNOSTIC: MALLOC FAILED for %s (Len: %d)", debug_name, len);
        return empty_string; 
    }
    
    return str;
}

static int32_t get_dict_int_safe(DictionaryIterator *iter, uint32_t key, int32_t default_val) {
    Tuple *t = dict_find(iter, key);
    if (!t) return default_val;
    switch(t->length) {
        case 1: return t->value->int8;
        case 2: return t->value->int16;
        case 4: return t->value->int32;
        default: return default_val;
    }
}

void game_set(Game *game, DictionaryIterator *iter) {
    if (game == NULL) return;

    APP_LOG(APP_LOG_LEVEL_DEBUG, "DIAGNOSTIC: Parsing ints...");
    game->id = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_ID, 0);
    game->sport = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_SPORT, 0);
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "DIAGNOSTIC: Parsing strings...");
    game->league = memorize_dict_string(iter, MESSAGE_KEY_SEND_GAME_LEAGUE, "League"); 
    char *team_1_name = memorize_dict_string(iter, MESSAGE_KEY_SEND_GAME_TEAM_1_NAME, "Team1Name");
    char *team_2_name = memorize_dict_string(iter, MESSAGE_KEY_SEND_GAME_TEAM_2_NAME, "Team2Name");
    char *team_1_score = memorize_dict_string(iter, MESSAGE_KEY_SEND_GAME_TEAM_1_SCORE, "Team1Score");
    char *team_2_score = memorize_dict_string(iter, MESSAGE_KEY_SEND_GAME_TEAM_2_SCORE, "Team2Score");

    game->team1 = (Team) {
        .name = team_1_name,
        .score = team_1_score,
        .id = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_TEAM_1_ID, 0),
        .favorite = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_TEAM_1_FAVORITE, 0),
        .record = memorize_dict_string(iter, MESSAGE_KEY_SEND_GAME_TEAM_1_RECORD, "Team1Record"),
    };
    
    game->team2 = (Team) {
        .name = team_2_name,
        .score = team_2_score,
        .id = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_TEAM_2_ID, 0),
        .favorite = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_TEAM_2_FAVORITE, 0),
        .record = memorize_dict_string(iter, MESSAGE_KEY_SEND_GAME_TEAM_2_RECORD, "Team2Record"),
    };
    
    game->possession = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_POSSESSION, 2); 
    game->time = memorize_dict_string(iter, MESSAGE_KEY_SEND_GAME_TIME, "Time");
    game->details = memorize_dict_string(iter, MESSAGE_KEY_SEND_GAME_DETAILS, "Details");
    game->broadcast = memorize_dict_string(iter, MESSAGE_KEY_SEND_GAME_BROADCAST, "Broadcast");

    #if defined(PBL_ROUND)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "DIAGNOSTIC: Building round summary string...");
        char *summary = malloc(strlen(team_1_name) + strlen(team_1_score) + strlen(team_2_name) + strlen(team_2_score) + 6);
        if (summary != NULL) {
            strcpy(summary, team_1_name);
            strcat(summary, " ");
            strcat(summary, team_1_score);
            strcat(summary, " - ");
            strcat(summary, team_2_score);
            strcat(summary, " ");
            strcat(summary, team_2_name);
            game->summary = summary;
        } else {
            game->summary = empty_string;
        }
    #else
        game->summary = empty_string;
    #endif
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "DIAGNOSTIC: Game %d parsed successfully.", games_count);
}

void handle_games_recieved(DictionaryIterator *iter) {
    int request_id = get_dict_int_safe(iter, MESSAGE_KEY_REQUEST_ID, 0);

    if (request_id != current_request){ 
        return;
    }

    GamesListState data = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_LIST, GamesListNetworkError);

    APP_LOG(APP_LOG_LEVEL_INFO, "--> RECV Item. Index: %d, State: %d, Free RAM: %d", games_count, data, (int)heap_bytes_free());

    if (data == GamesListNoGames) {
        if(on_games_error) on_games_error(NoGames);
        return;
    }
    if (data == GamesListNetworkError) {
        if(on_games_error) on_games_error(NetworkError);
        return;
    }

    if (data == GAMES_LIST_INIT_ARRAY) {
        int total_games = get_dict_int_safe(iter, MESSAGE_KEY_SEND_GAME_ID, 0);
        APP_LOG(APP_LOG_LEVEL_INFO, "DIAGNOSTIC: Init Array. Total expected: %d", total_games);
        
        free_games_array(); 

        if (total_games > 0) {
            games = malloc(total_games * sizeof(Game*));
            if (games == NULL) {
                APP_LOG(APP_LOG_LEVEL_ERROR, "!!! CRITICAL OOM: Array Allocation Failed !!!");
            }
        }
        return;
    }

    if (games != NULL) {
        Game *new_game = malloc(sizeof(Game));
        if (new_game != NULL) {
            game_set(new_game, iter);
            games[games_count] = new_game;
            games_count++;
            APP_LOG(APP_LOG_LEVEL_INFO, "DIAGNOSTIC: Stored game. Free RAM: %d", (int)heap_bytes_free());
        } else {
            APP_LOG(APP_LOG_LEVEL_ERROR, "!!! CRITICAL OOM: Struct Allocation Failed for game %d !!!", games_count);
        }
    }

    if (data == GamesListLastItem) {
        APP_LOG(APP_LOG_LEVEL_INFO, "DIAGNOSTIC: Last item received. Triggering UI callback.");
        if (on_games_success) {
            on_games_success(games_count, games);
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
        if (games[i] != NULL && games[i]->id == game_id) {
            safe_free(games[i]->league); 
            safe_free(games[i]->team1.name);
            safe_free(games[i]->team1.score);
            safe_free(games[i]->team1.record);
            safe_free(games[i]->team2.name);
            safe_free(games[i]->team2.score);
            safe_free(games[i]->team2.record);
            safe_free(games[i]->time);
            safe_free(games[i]->summary);
            safe_free(games[i]->details);
            safe_free(games[i]->broadcast); 

            game_set(games[i], iter);

            if (on_game_update) on_game_update(GameUpdated);
            return;
        }
    }
}
