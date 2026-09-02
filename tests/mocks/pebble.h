#ifndef PEBBLE_H
#define PEBBLE_H

#include <stdint.h>
#include <stdbool.h>

// Mock Pebble resource IDs for testing
#define RESOURCE_ID_FOOTBALL_16 1
#define RESOURCE_ID_BASEBALL_16 2
#define RESOURCE_ID_HOCKEY_16 3
#define RESOURCE_ID_BASKETBALL_16 4
#define RESOURCE_ID_Mls_16 5
#define RESOURCE_ID_Rugby_16 6
#define RESOURCE_ID_Cricket_16 7
#define RESOURCE_ID_IMAGE_SPORT_TENNIS16 8
#define RESOURCE_ID_IMAGE_SPORT_AFL16 9
#define RESOURCE_ID_IMAGE_SPORT_MMA16 10
#define RESOURCE_ID_STAR_16 11

#define RESOURCE_ID_FOOTBALL_25 101
#define RESOURCE_ID_BASEBALL_25 102
#define RESOURCE_ID_HOCKEY_25 103
#define RESOURCE_ID_BASKETBALL_25 104
#define RESOURCE_ID_Mls_25 105
#define RESOURCE_ID_Rugby_25 106
#define RESOURCE_ID_Cricket_25 107
#define RESOURCE_ID_IMAGE_SPORT_TENNIS25 108
#define RESOURCE_ID_IMAGE_SPORT_AFL25 109
#define RESOURCE_ID_IMAGE_SPORT_MMA25 110
#define RESOURCE_ID_STAR_25 111

// Logging
#define APP_LOG_LEVEL_ERROR 1
#define APP_LOG(level, fmt, ...)

// Dictionary / Tuplet definitions
typedef enum {
    TUPLE_INT = 1,
    TUPLE_CSTRING = 2
} TupleType;

typedef struct {
    uint32_t key;
    TupleType type;
    union {
        uint32_t integer;
        struct {
            uint16_t length;
            const char *data;
        } cstring;
    };
} Tuplet;

typedef union {
    int8_t int8;
    int16_t int16;
    int32_t int32;
    uint8_t uint8;
    uint16_t uint16;
    uint32_t uint32;
} TupleValue;

typedef struct {
    uint32_t key;
    TupleType type;
    uint16_t length;
    TupleValue *value;
} Tuple;

typedef struct {
    void *dictionary;
    void *end;
    Tuple *cursor;
} DictionaryIterator;

#define TupletInteger(k, v) \
    ((Tuplet){ .type = TUPLE_INT, .key = (k), .integer = (v) })

typedef enum {
    APP_MSG_OK = 0,
    APP_MSG_SEND_TIMEOUT = 1,
    APP_MSG_SEND_REJECTED = 2,
    APP_MSG_NOT_CONNECTED = 3,
    APP_MSG_APP_NOT_RUNNING = 4,
    APP_MSG_INVALID_ARGS = 5,
    APP_MSG_BUSY = 6,
    APP_MSG_BUFFER_OVERFLOW = 7,
    APP_MSG_ALREADY_RELEASED = 8,
    APP_MSG_CALLBACK_ALREADY_REGISTERED = 9,
    APP_MSG_CALLBACK_NOT_REGISTERED = 10,
    APP_MSG_OUT_OF_MEMORY = 11,
    APP_MSG_CLOSED = 12,
    APP_MSG_INTERNAL_ERROR = 13
} AppMessageResult;

// Function Prototypes for Mock
AppMessageResult app_message_outbox_begin(DictionaryIterator **iterator);
AppMessageResult app_message_outbox_send(void);
AppMessageResult dict_write_tuplet(DictionaryIterator *iter, const Tuplet *tuplet);
Tuple *dict_find(const DictionaryIterator *iter, uint32_t key);

// Configurable returns for tests
extern AppMessageResult mock_app_message_outbox_begin_result;
extern AppMessageResult mock_app_message_outbox_send_result;
extern DictionaryIterator *mock_outbox_iterator;
extern Tuple *mock_dict_find_result;

// Track what was written
extern int mock_written_sport;
extern int mock_written_team_id;
extern int mock_written_request_id;
extern bool mock_outbox_sent;

void mock_pebble_reset(void);

// Constants
#define MESSAGE_KEY_ADD_FAVORITE_SPORT 100
#define MESSAGE_KEY_ADD_FAVORITE_TEAM_ID 101
#define MESSAGE_KEY_REQUEST_ID 102
#define MESSAGE_KEY_CONFIRM_FAVORITE 103

#endif
