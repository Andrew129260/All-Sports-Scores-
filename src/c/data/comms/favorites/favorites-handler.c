#include <stdlib.h>
#include <stdint.h>
#include "pebble.h"
#include "../../model/models.h"
#include "favorites-handler.h"

#define FAVORITE_TIMEOUT_MS 10000

static FavoriteChangeCallback s_callback = NULL;
static Game *s_favoriting_game = NULL;
static int s_favoriting_game_id = 0;
static int s_favoriting_team_id = -1;
static int current_request = -1;
static AppTimer *s_timeout_timer = NULL;

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

static void favorite_timeout_callback(void *data) {
    s_timeout_timer = NULL;

    if (current_request == -1) {
        return;
    }

    APP_LOG(
        APP_LOG_LEVEL_WARNING,
        "Favorite request timed out"
    );

    FavoriteChangeCallback callback = s_callback;
    int team_id = s_favoriting_team_id;

    s_callback = NULL;
    s_favoriting_game = NULL;
    s_favoriting_game_id = 0;
    s_favoriting_team_id = -1;
    current_request = -1;

    if (callback) {
        callback(team_id, FavoriteChangeFailed);
    }
}

void handle_request_change_favorite(
    Game *favoriting_game,
    int team_id,
    FavoriteChangeCallback callback
) {
    if (!favoriting_game) {
        if (callback) callback(team_id, FavoriteChangeFailed);
        return;
    }

    if (current_request != -1) {
        APP_LOG(
            APP_LOG_LEVEL_WARNING,
            "Favorite request already in progress"
        );

        if (callback) {
            callback(team_id, FavoriteChangeFailed);
        }

        return;
    }

    int request_id = rand();

    DictionaryIterator *out_iter = NULL;
    AppMessageResult result = app_message_outbox_begin(&out_iter);

    if (result == APP_MSG_OK && out_iter) {
        Tuplet favorite_sport_tuple =
            TupletInteger(MESSAGE_KEY_ADD_FAVORITE_SPORT, favoriting_game->sport);
        Tuplet favorite_team_id_tuple =
            TupletInteger(MESSAGE_KEY_ADD_FAVORITE_TEAM_ID, team_id);
        Tuplet request_id_tuple =
            TupletInteger(MESSAGE_KEY_REQUEST_ID, request_id);

        if (dict_write_tuplet(out_iter, &favorite_sport_tuple) != DICT_OK ||
            dict_write_tuplet(out_iter, &favorite_team_id_tuple) != DICT_OK ||
            dict_write_tuplet(out_iter, &request_id_tuple) != DICT_OK) {
            if (callback) callback(team_id, FavoriteChangeFailed);
            return;
        }

        result = app_message_outbox_send();

        if (result != APP_MSG_OK) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result);
            if (callback) callback(team_id, FavoriteChangeFailed);
        } else {
            current_request = request_id;
            s_favoriting_game = favoriting_game;
            s_favoriting_game_id = favoriting_game->id;
            s_favoriting_team_id = team_id;
            s_callback = callback;

            if (s_timeout_timer) {
                app_timer_cancel(s_timeout_timer);
            }
            s_timeout_timer = app_timer_register(
                FAVORITE_TIMEOUT_MS,
                favorite_timeout_callback,
                NULL
            );

            if (!s_timeout_timer) {
                APP_LOG(
                    APP_LOG_LEVEL_ERROR,
                    "Failed to create favorite timeout timer"
                );

                FavoriteChangeCallback failed_callback = s_callback;
                int failed_team_id = s_favoriting_team_id;

                s_callback = NULL;
                s_favoriting_game = NULL;
                s_favoriting_game_id = 0;
                s_favoriting_team_id = -1;
                current_request = -1;

                if (failed_callback) {
                    failed_callback(
                        failed_team_id,
                        FavoriteChangeFailed
                    );
                }
            }
        }
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result);
        if (callback) callback(team_id, FavoriteChangeFailed);
    }
}

void handle_favorite_change_result(DictionaryIterator *iter) {
    if (!iter || current_request == -1) return;

    int request_id = get_dict_int_safe(iter, MESSAGE_KEY_REQUEST_ID, -1);

    // If phone is sending favorites from a stale request, discard it.
    if (request_id != current_request) {
        return;
    }

    if (s_timeout_timer) {
        app_timer_cancel(s_timeout_timer);
        s_timeout_timer = NULL;
    }

    int32_t raw_result = get_dict_int_safe(
        iter,
        MESSAGE_KEY_CONFIRM_FAVORITE,
        FavoriteChangeFailed
    );

    FavoriteChangeResult result;

    if (raw_result == FavoriteAdded ||
        raw_result == FavoriteRemoved) {
        result = (FavoriteChangeResult)raw_result;
    } else {
        result = FavoriteChangeFailed;
    }

    if (result != FavoriteChangeFailed &&
        s_favoriting_game != NULL &&
        s_favoriting_game->id == s_favoriting_game_id) {
        bool is_favorite = (result == FavoriteAdded);

        if (s_favoriting_team_id == s_favoriting_game->team1.id) {
            s_favoriting_game->team1.favorite = is_favorite;
        }
        if (s_favoriting_team_id == s_favoriting_game->team2.id) {
            s_favoriting_game->team2.favorite = is_favorite;
        }
    }

    FavoriteChangeCallback callback = s_callback;
    int team_id = s_favoriting_team_id;

    s_callback = NULL;
    s_favoriting_game = NULL;
    s_favoriting_game_id = 0;
    s_favoriting_team_id = -1;
    current_request = -1;

    if (callback) {
        callback(team_id, result);
    }
}