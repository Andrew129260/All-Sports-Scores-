#include "unity/unity.h"
#include "../src/c/data/model/models.h"
#include "../src/c/data/comms/games/games-handler.h"
#include "mocks/pebble.h"

// Mocks for pebble.h
static DictionaryIterator s_mock_iter;
static AppMessageResult s_mock_outbox_begin_result = APP_MSG_OK;
static AppMessageResult s_mock_outbox_send_result = APP_MSG_OK;
static int s_mock_dict_write_tuplet_count = 0;

static int s_success_callback_count = 0;
static int s_error_callback_count = 0;
static int s_update_callback_count = 0;
static Game *s_last_games = NULL;
static int s_last_game_count = 0;
static AppError s_last_error;
static GameUpdateResult s_last_update_result;

// Helper to construct mock tuples
#define MAX_MOCK_TUPLES 32
static Tuple *s_mock_tuples[MAX_MOCK_TUPLES];
static int s_mock_tuple_count = 0;

// Implementation of mocks in pebble.h
AppMessageResult app_message_outbox_begin(DictionaryIterator **iterator) {
    *iterator = &s_mock_iter;
    return s_mock_outbox_begin_result;
}

AppMessageResult app_message_outbox_send(void) {
    return s_mock_outbox_send_result;
}

AppMessageResult dict_write_tuplet(DictionaryIterator *iter, const Tuplet *tuplet) {
    s_mock_dict_write_tuplet_count++;
    return APP_MSG_OK;
}

Tuple *dict_find(const DictionaryIterator *iter, uint32_t key) {
    for (int i = 0; i < s_mock_tuple_count; i++) {
        if (s_mock_tuples[i] != NULL && s_mock_tuples[i]->key == key) {
            return s_mock_tuples[i];
        }
    }
    return NULL;
}

// Helper to reset mocks
static void reset_mocks() {
    s_mock_outbox_begin_result = APP_MSG_OK;
    s_mock_outbox_send_result = APP_MSG_OK;
    s_mock_dict_write_tuplet_count = 0;

    s_success_callback_count = 0;
    s_error_callback_count = 0;
    s_update_callback_count = 0;
    s_last_games = NULL;
    s_last_game_count = 0;

    for (int i = 0; i < s_mock_tuple_count; i++) {
        if (s_mock_tuples[i]) {
            free(s_mock_tuples[i]);
            s_mock_tuples[i] = NULL;
        }
    }
    s_mock_tuple_count = 0;
}

static void add_mock_tuple_int(uint32_t key, int32_t value) {
    if (s_mock_tuple_count >= MAX_MOCK_TUPLES) return;

    // Size of Tuple + union size
    Tuple *t = malloc(sizeof(Tuple) + sizeof(int32_t));
    t->key = key;
    t->type = TUPLE_INT;
    t->length = 4;
    t->value[0].int32 = value; // This writes to the union

    s_mock_tuples[s_mock_tuple_count++] = t;
}

static void add_mock_tuple_string(uint32_t key, const char *value) {
    if (s_mock_tuple_count >= MAX_MOCK_TUPLES) return;

    int len = strlen(value) + 1;
    // We allocate enough space for the struct + pointer to string
    // because union value[] is flexible array member and cstring is a char*
    Tuple *t = malloc(sizeof(Tuple) + sizeof(char*));
    t->key = key;
    t->type = TUPLE_CSTRING;
    t->length = len;

    // Allocate string and set pointer
    t->value[0].cstring = malloc(len);
    strcpy(t->value[0].cstring, value);

    s_mock_tuples[s_mock_tuple_count++] = t;
}

// Memory cleanup for tests specifically
static void cleanup_mock_tuples() {
    for (int i = 0; i < s_mock_tuple_count; i++) {
        if (s_mock_tuples[i]) {
            if (s_mock_tuples[i]->type == TUPLE_CSTRING && s_mock_tuples[i]->value[0].cstring) {
                free(s_mock_tuples[i]->value[0].cstring);
            }
            free(s_mock_tuples[i]);
            s_mock_tuples[i] = NULL;
        }
    }
    s_mock_tuple_count = 0;
}

// Callbacks
static void on_games_success(int game_count, Game *games) {
    s_success_callback_count++;
    s_last_game_count = game_count;
    s_last_games = games;
}

static void on_games_error(AppError error) {
    s_error_callback_count++;
    s_last_error = error;
}

static void on_game_update(GameUpdateResult result) {
    s_update_callback_count++;
    s_last_update_result = result;
}

void setUp(void) {
    reset_mocks();
    handle_clear_games();
}

void tearDown(void) {
    handle_clear_games();
    cleanup_mock_tuples();
}

void test_handle_request_games_success(void) {
    handle_request_games(SportNFL, 0, on_games_success, on_games_error);

    TEST_ASSERT_EQUAL(3, s_mock_dict_write_tuplet_count); // load_games, request_id, league_index
    TEST_ASSERT_EQUAL(0, s_error_callback_count);
}

void test_handle_request_games_outbox_begin_error(void) {
    s_mock_outbox_begin_result = APP_MSG_BUSY;
    handle_request_games(SportNFL, 0, on_games_success, on_games_error);

    TEST_ASSERT_EQUAL(1, s_error_callback_count);
    TEST_ASSERT_EQUAL(ConnectionError, s_last_error);
}

void test_handle_request_games_outbox_send_error(void) {
    s_mock_outbox_send_result = APP_MSG_SEND_REJECTED;
    handle_request_games(SportNFL, 0, on_games_success, on_games_error);

    TEST_ASSERT_EQUAL(1, s_error_callback_count);
    TEST_ASSERT_EQUAL(ConnectionError, s_last_error);
}

void test_handle_games_recieved_init(void) {
    // 1. Request games to set current_request
    handle_request_games(SportNFL, 0, on_games_success, on_games_error);

    // 2. Prepare init message
    add_mock_tuple_int(MESSAGE_KEY_REQUEST_ID, 12345);
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_LIST, 4); // GAMES_LIST_INIT_ARRAY = 4
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_ID, 2); // 2 games expected

    // 3. Process
    handle_games_recieved(&s_mock_iter);

    // 4. Assert memory is prepared for 2 games
    // Since we don't have direct access to `games_count` (it's static),
    // we can test the behavior by feeding actual games next.
}

void test_handle_games_recieved_item(void) {
    handle_request_games(SportNFL, 0, on_games_success, on_games_error);

    // Init array of 1 game
    add_mock_tuple_int(MESSAGE_KEY_REQUEST_ID, 12345);
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_LIST, 4);
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_ID, 1);
    handle_games_recieved(&s_mock_iter);

    // Reset mocks for item
    cleanup_mock_tuples();

    // Send item
    add_mock_tuple_int(MESSAGE_KEY_REQUEST_ID, 12345);
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_LIST, 0); // GamesListItem = 0
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_ID, 999);
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_SPORT, SportNFL);
    add_mock_tuple_string(MESSAGE_KEY_SEND_GAME_LEAGUE, "NFL");
    add_mock_tuple_string(MESSAGE_KEY_SEND_GAME_TEAM_1_NAME, "Eagles");
    add_mock_tuple_string(MESSAGE_KEY_SEND_GAME_TEAM_1_SCORE, "10");
    add_mock_tuple_string(MESSAGE_KEY_SEND_GAME_TEAM_2_NAME, "Chiefs");
    add_mock_tuple_string(MESSAGE_KEY_SEND_GAME_TEAM_2_SCORE, "7");

    handle_games_recieved(&s_mock_iter);

    // Send last item
    cleanup_mock_tuples();
    add_mock_tuple_int(MESSAGE_KEY_REQUEST_ID, 12345);
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_LIST, 1); // GamesListLastItem = 1

    handle_games_recieved(&s_mock_iter);

    TEST_ASSERT_EQUAL(1, s_success_callback_count);
    TEST_ASSERT_EQUAL(1, s_last_game_count);
    TEST_ASSERT_NOT_NULL(s_last_games);
    TEST_ASSERT_EQUAL(999, s_last_games[0].id);
    TEST_ASSERT_EQUAL(SportNFL, s_last_games[0].sport);
    TEST_ASSERT_EQUAL_STRING("NFL", s_last_games[0].league);
    TEST_ASSERT_EQUAL_STRING("Eagles", s_last_games[0].team1.name);
    TEST_ASSERT_EQUAL_STRING("10", s_last_games[0].team1.score);
    TEST_ASSERT_EQUAL_STRING("Chiefs", s_last_games[0].team2.name);
    TEST_ASSERT_EQUAL_STRING("7", s_last_games[0].team2.score);
}

void test_handle_games_recieved_no_games(void) {
    handle_request_games(SportNFL, 0, on_games_success, on_games_error);

    add_mock_tuple_int(MESSAGE_KEY_REQUEST_ID, 12345);
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_LIST, 2); // GamesListNoGames = 2

    handle_games_recieved(&s_mock_iter);

    TEST_ASSERT_EQUAL(1, s_error_callback_count);
    TEST_ASSERT_EQUAL(NoGames, s_last_error);
}

void test_handle_games_recieved_network_error(void) {
    handle_request_games(SportNFL, 0, on_games_success, on_games_error);

    add_mock_tuple_int(MESSAGE_KEY_REQUEST_ID, 12345);
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_LIST, 3); // GamesListNetworkError = 3

    handle_games_recieved(&s_mock_iter);

    TEST_ASSERT_EQUAL(1, s_error_callback_count);
    TEST_ASSERT_EQUAL(NetworkError, s_last_error);
}

void test_handle_games_recieved_wrong_request_id(void) {
    handle_request_games(SportNFL, 0, on_games_success, on_games_error);

    add_mock_tuple_int(MESSAGE_KEY_REQUEST_ID, 99999); // Wrong ID
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_LIST, 2); // NoGames

    handle_games_recieved(&s_mock_iter);

    // Should return early, no callback
    TEST_ASSERT_EQUAL(0, s_error_callback_count);
}

void test_update_game_success(void) {
    Game game = {.id = 111, .sport = SportMLB};
    update_game(&game, on_game_update);

    TEST_ASSERT_EQUAL(3, s_mock_dict_write_tuplet_count); // id, sport, request_id
}

void test_handle_game_update_recieved_success(void) {
    // First, set up a game
    handle_request_games(SportNFL, 0, on_games_success, on_games_error);

    add_mock_tuple_int(MESSAGE_KEY_REQUEST_ID, 12345);
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_LIST, 4); // Init
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_ID, 1);
    handle_games_recieved(&s_mock_iter);

    cleanup_mock_tuples();
    add_mock_tuple_int(MESSAGE_KEY_REQUEST_ID, 12345);
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_LIST, 0); // Item
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_ID, 111);
    add_mock_tuple_string(MESSAGE_KEY_SEND_GAME_TEAM_1_NAME, "OldName");
    handle_games_recieved(&s_mock_iter);

    // Request update
    Game game = {.id = 111, .sport = SportNFL};
    update_game(&game, on_game_update);

    // Receive update
    cleanup_mock_tuples();
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_UPDATE, 0); // GameUpdated
    add_mock_tuple_int(MESSAGE_KEY_SEND_GAME_ID, 111);
    add_mock_tuple_string(MESSAGE_KEY_SEND_GAME_TEAM_1_NAME, "NewName");
    handle_game_update_recieved(&s_mock_iter);

    TEST_ASSERT_EQUAL(1, s_update_callback_count);
    TEST_ASSERT_EQUAL(GameUpdated, s_last_update_result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_handle_request_games_success);
    RUN_TEST(test_handle_request_games_outbox_begin_error);
    RUN_TEST(test_handle_request_games_outbox_send_error);
    RUN_TEST(test_handle_games_recieved_init);
    RUN_TEST(test_handle_games_recieved_item);
    RUN_TEST(test_handle_games_recieved_no_games);
    RUN_TEST(test_handle_games_recieved_network_error);
    RUN_TEST(test_handle_games_recieved_wrong_request_id);
    RUN_TEST(test_update_game_success);
    RUN_TEST(test_handle_game_update_recieved_success);
    return UNITY_END();
}
