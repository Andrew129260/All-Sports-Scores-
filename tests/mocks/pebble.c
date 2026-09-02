#include "pebble.h"
#include <stddef.h>

AppMessageResult mock_app_message_outbox_begin_result = APP_MSG_OK;
AppMessageResult mock_app_message_outbox_send_result = APP_MSG_OK;
DictionaryIterator *mock_outbox_iterator = NULL;

int mock_written_sport = -1;
int mock_written_team_id = -1;
int mock_written_request_id = -1;
bool mock_outbox_sent = false;

// We will simulate dict_find returns using an array of Tuples
#define MAX_MOCK_TUPLES 10
Tuple mock_tuples[MAX_MOCK_TUPLES];
int mock_tuple_count = 0;

void mock_pebble_reset(void) {
    mock_app_message_outbox_begin_result = APP_MSG_OK;
    mock_app_message_outbox_send_result = APP_MSG_OK;
    mock_outbox_iterator = NULL;
    mock_written_sport = -1;
    mock_written_team_id = -1;
    mock_written_request_id = -1;
    mock_outbox_sent = false;
    mock_tuple_count = 0;
}

AppMessageResult app_message_outbox_begin(DictionaryIterator **iterator) {
    if (iterator != NULL) {
        *iterator = mock_outbox_iterator;
    }
    return mock_app_message_outbox_begin_result;
}

AppMessageResult app_message_outbox_send(void) {
    mock_outbox_sent = true;
    return mock_app_message_outbox_send_result;
}

AppMessageResult dict_write_tuplet(DictionaryIterator *iter, const Tuplet *tuplet) {
    if (tuplet->key == MESSAGE_KEY_ADD_FAVORITE_SPORT) {
        mock_written_sport = tuplet->integer;
    } else if (tuplet->key == MESSAGE_KEY_ADD_FAVORITE_TEAM_ID) {
        mock_written_team_id = tuplet->integer;
    } else if (tuplet->key == MESSAGE_KEY_REQUEST_ID) {
        mock_written_request_id = tuplet->integer;
    }
    return APP_MSG_OK;
}

Tuple *dict_find(const DictionaryIterator *iter, uint32_t key) {
    for (int i = 0; i < mock_tuple_count; i++) {
        if (mock_tuples[i].key == key) {
            return &mock_tuples[i];
        }
    }
    return NULL;
}
