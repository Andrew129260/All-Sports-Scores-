#ifndef PEBBLE_H
#define PEBBLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
#define APP_LOG_LEVEL_ERROR 0
#define APP_LOG_LEVEL_WARNING 1
#define APP_LOG_LEVEL_INFO 2
#define APP_LOG_LEVEL_DEBUG 3

#define APP_LOG(level, fmt, ...) printf("[%d] " fmt "\n", level, ##__VA_ARGS__)

// Memory functions
static inline uint32_t heap_bytes_free() { return 1024 * 1024; }

// AppMessage
typedef enum {
  APP_MSG_OK = 0,
  APP_MSG_SEND_TIMEOUT,
  APP_MSG_SEND_REJECTED,
  APP_MSG_NOT_CONNECTED,
  APP_MSG_APP_NOT_RUNNING,
  APP_MSG_INVALID_ARGS,
  APP_MSG_BUSY,
  APP_MSG_BUFFER_OVERFLOW,
  APP_MSG_ALREADY_RELEASED,
  APP_MSG_CALLBACK_ALREADY_REGISTERED,
  APP_MSG_CALLBACK_NOT_REGISTERED,
  APP_MSG_OUT_OF_MEMORY,
  APP_MSG_CLOSED,
  APP_MSG_INTERNAL_ERROR,
} AppMessageResult;

// Tuplets and Dictionary
typedef enum {
  TUPLE_BYTE_ARRAY = 0,
  TUPLE_CSTRING = 1,
  TUPLE_UINT = 2,
  TUPLE_INT = 3,
} TupleType;

typedef struct {
  uint32_t key;
  TupleType type;
  uint16_t length;
  union {
    uint8_t *data;
    char *cstring;
    uint32_t uint32;
    uint16_t uint16;
    uint8_t uint8;
    int32_t int32;
    int16_t int16;
    int8_t int8;
  } value[];
} Tuple;

typedef struct {
    uint32_t key;
    TupleType type;
    union {
        const char *cstring;
        int32_t integer;
    };
    uint16_t integer_width;
} Tuplet;

#define TupletInteger(k, v) ((Tuplet){.key = (k), .type = TUPLE_INT, .integer = (v), .integer_width = 4})

typedef struct DictionaryIterator {
    // A simple mock for dictionary iterator
    void *dictionary;
    const void *end;
    Tuple *cursor;
} DictionaryIterator;

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
// Error definition is in models.h, so we remove it here to avoid duplication
// (I accidentally restored it earlier)

// Additional keys for tests
#define MESSAGE_KEY_LOAD_GAMES 100
#define MESSAGE_KEY_REQUEST_ID 101
#define MESSAGE_KEY_LEAGUE_INDEX 102
#define MESSAGE_KEY_UPDATE_GAME_ID 103
#define MESSAGE_KEY_UPDATE_GAME_SPORT 104
#define MESSAGE_KEY_SEND_GAME_ID 105
#define MESSAGE_KEY_SEND_GAME_SPORT 106
#define MESSAGE_KEY_SEND_GAME_LEAGUE 107
#define MESSAGE_KEY_SEND_GAME_TEAM_1_NAME 108
#define MESSAGE_KEY_SEND_GAME_TEAM_2_NAME 109
#define MESSAGE_KEY_SEND_GAME_TEAM_1_SCORE 110
#define MESSAGE_KEY_SEND_GAME_TEAM_2_SCORE 111
#define MESSAGE_KEY_SEND_GAME_TEAM_1_ID 112
#define MESSAGE_KEY_SEND_GAME_TEAM_1_FAVORITE 113
#define MESSAGE_KEY_SEND_GAME_TEAM_1_WINNER 114
#define MESSAGE_KEY_SEND_GAME_TEAM_1_RECORD 115
#define MESSAGE_KEY_SEND_GAME_TEAM_2_ID 116
#define MESSAGE_KEY_SEND_GAME_TEAM_2_FAVORITE 117
#define MESSAGE_KEY_SEND_GAME_TEAM_2_WINNER 118
#define MESSAGE_KEY_SEND_GAME_TEAM_2_RECORD 119
#define MESSAGE_KEY_SEND_GAME_POSSESSION 120
#define MESSAGE_KEY_SEND_GAME_TIME 121
#define MESSAGE_KEY_SEND_GAME_DETAILS 122
#define MESSAGE_KEY_SEND_GAME_BROADCAST 123
#define MESSAGE_KEY_SEND_GAME_LIST 124
#define MESSAGE_KEY_SEND_GAME_UPDATE 125

// Redefine rand to a known value to make tests deterministic
#define rand() 12345

#endif
