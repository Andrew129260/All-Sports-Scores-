#include "pebble.h"

#include "../model/models.h"
#include "comms.h"

#include "games/games-handler.h"
#include "favorites/favorites-handler.h"
#include "prefs/prefs-handler.h"

// =========================================================
// MEMORY CONFIGURATION
// =========================================================

#if defined(PBL_PLATFORM_APLITE)
static const uint32_t inbox_size = 640;
#else
static const uint32_t inbox_size = 1024;
#endif

static const uint32_t outbox_size = 128;

static bool s_js_ready = false;

// =========================================================
// INCOMING MESSAGES
// =========================================================

static void inbox_received_callback(
    DictionaryIterator *iter,
    void *context
) {
    if (!iter) return;

    APP_LOG(
        APP_LOG_LEVEL_DEBUG,
        "Received JS message"
    );

    if (dict_find(iter, MESSAGE_KEY_READY)) {
        s_js_ready = true;

        APP_LOG(
            APP_LOG_LEVEL_DEBUG,
            "JS Ready"
        );
    }

    if (dict_find(iter, MESSAGE_KEY_SEND_GAME_LIST)) {
        handle_games_recieved(iter);
    }

    if (dict_find(iter, MESSAGE_KEY_SEND_GAME_UPDATE)) {
        handle_game_update_recieved(iter);
    }

    if (dict_find(iter, MESSAGE_KEY_LOAD_BREAKDOWN)) {
        handle_breakdown_received(iter);
    }

    if (dict_find(iter, MESSAGE_KEY_CONFIRM_FAVORITE)) {
        handle_favorite_change_result(iter);
    }

    if (dict_find(iter, MESSAGE_KEY_CONFIG_SHOW_RECORDS)) {
        handle_config_recieved(iter);
    }
}

// =========================================================
// INBOX ERRORS
// =========================================================

static void inbox_dropped_callback(
    AppMessageResult reason,
    void *context
) {
    APP_LOG(
        APP_LOG_LEVEL_ERROR,
        "Message dropped: %d",
        (int)reason
    );
}

// =========================================================
// OUTBOX CALLBACKS
// =========================================================

static void outbox_sent_callback(
    DictionaryIterator *iter,
    void *context
) {
    // Delivery acknowledged by phone.
}

static void outbox_failed_callback(
    DictionaryIterator *iter,
    AppMessageResult reason,
    void *context
) {
    APP_LOG(
        APP_LOG_LEVEL_ERROR,
        "Outbox delivery failed: %d",
        (int)reason
    );

    // Only handle game-related requests here.
    // Favorite and preference handlers remain independent.
    handle_game_outbox_failed(iter);
}

// =========================================================
// INITIALIZATION
// =========================================================

void setup_comms(void) {
    s_js_ready = false;

    app_message_register_inbox_received(
        inbox_received_callback
    );

    app_message_register_inbox_dropped(
        inbox_dropped_callback
    );

    app_message_register_outbox_sent(
        outbox_sent_callback
    );

    app_message_register_outbox_failed(
        outbox_failed_callback
    );

    AppMessageResult result = app_message_open(
        inbox_size,
        outbox_size
    );

    if (result != APP_MSG_OK) {
        APP_LOG(
            APP_LOG_LEVEL_ERROR,
            "AppMessage open failed: %d",
            (int)result
        );
    }
}

// =========================================================
// SHUTDOWN
// =========================================================

void destroy_comms(void) {
    app_message_deregister_callbacks();
    s_js_ready = false;
}