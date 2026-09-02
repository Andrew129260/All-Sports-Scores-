#include "unity/unity.h"
#include "../src/c/data/comms/favorites/favorites-handler.h"
#include "../src/c/data/model/models.h"
#include "mocks/pebble.h"
#include <stdbool.h>

// Globals to track callback calls
static int callback_called_team_id = -1;
static FavoriteChangeResult callback_called_result = FavoriteChangeFailed;
static bool callback_called = false;

extern Tuple mock_tuples[];
extern int mock_tuple_count;

// Callback mock
void mock_favorite_change_callback(int teamID, FavoriteChangeResult result) {
    callback_called_team_id = teamID;
    callback_called_result = result;
    callback_called = true;
}

void setUp(void) {
    mock_pebble_reset();
    callback_called_team_id = -1;
    callback_called_result = FavoriteChangeFailed;
    callback_called = false;

    // We also need to reset the favorites-handler internal state between tests.
    // It's static, so we will do an initial failure call to try and clean it.
    // However, it doesn't clean the `current_request` if it fails immediately.
    // We'll write the tests carefully so they don't depend on stale `current_request`
    // unless we need to.
}

void tearDown(void) {
}

void test_handle_request_change_favorite_outbox_begin_fails(void) {
    Game game;
    game.sport = SportNFL;

    mock_app_message_outbox_begin_result = APP_MSG_OUT_OF_MEMORY;

    handle_request_change_favorite(&game, 123, mock_favorite_change_callback);

    TEST_ASSERT_TRUE(callback_called);
    TEST_ASSERT_EQUAL(123, callback_called_team_id);
    TEST_ASSERT_EQUAL(FavoriteChangeFailed, callback_called_result);
    TEST_ASSERT_FALSE(mock_outbox_sent);
}

void test_handle_request_change_favorite_outbox_send_fails(void) {
    Game game;
    game.sport = SportNFL;

    mock_app_message_outbox_begin_result = APP_MSG_OK;
    mock_app_message_outbox_send_result = APP_MSG_SEND_TIMEOUT;

    handle_request_change_favorite(&game, 123, mock_favorite_change_callback);

    TEST_ASSERT_TRUE(callback_called);
    TEST_ASSERT_EQUAL(123, callback_called_team_id);
    TEST_ASSERT_EQUAL(FavoriteChangeFailed, callback_called_result);

    TEST_ASSERT_EQUAL(SportNFL, mock_written_sport);
    TEST_ASSERT_EQUAL(123, mock_written_team_id);
    TEST_ASSERT_NOT_EQUAL(-1, mock_written_request_id); // ensure a random ID was generated
    TEST_ASSERT_TRUE(mock_outbox_sent);
}

void test_handle_request_change_favorite_success(void) {
    Game game;
    game.sport = SportNFL;

    mock_app_message_outbox_begin_result = APP_MSG_OK;
    mock_app_message_outbox_send_result = APP_MSG_OK;

    handle_request_change_favorite(&game, 123, mock_favorite_change_callback);

    TEST_ASSERT_FALSE(callback_called); // Callback shouldn't be called synchronously on success

    TEST_ASSERT_EQUAL(SportNFL, mock_written_sport);
    TEST_ASSERT_EQUAL(123, mock_written_team_id);
    TEST_ASSERT_NOT_EQUAL(-1, mock_written_request_id);
    TEST_ASSERT_TRUE(mock_outbox_sent);
}

void test_handle_favorite_change_result_success(void) {
    Game game;
    game.sport = SportNFL;
    game.team1.id = 123;
    game.team1.favorite = false;
    game.team2.id = 456;
    game.team2.favorite = false;

    // 1. Request
    mock_app_message_outbox_begin_result = APP_MSG_OK;
    mock_app_message_outbox_send_result = APP_MSG_OK;
    handle_request_change_favorite(&game, 123, mock_favorite_change_callback);

    // Prepare fake tuples for the response
    TupleValue request_id_val = { .int32 = mock_written_request_id };
    mock_tuples[0].key = MESSAGE_KEY_REQUEST_ID;
    mock_tuples[0].value = &request_id_val;

    TupleValue result_val = { .int8 = FavoriteAdded };
    mock_tuples[1].key = MESSAGE_KEY_CONFIRM_FAVORITE;
    mock_tuples[1].value = &result_val;

    mock_tuple_count = 2;

    // 2. Receive result
    DictionaryIterator iter;
    handle_favorite_change_result(&iter);

    TEST_ASSERT_TRUE(callback_called);
    TEST_ASSERT_EQUAL(123, callback_called_team_id);
    TEST_ASSERT_EQUAL(FavoriteAdded, callback_called_result);
    TEST_ASSERT_TRUE(game.team1.favorite);
    TEST_ASSERT_FALSE(game.team2.favorite);
}

void test_handle_favorite_change_result_mismatched_request_id(void) {
    Game game;
    game.sport = SportNFL;
    game.team1.id = 123;
    game.team1.favorite = false;

    mock_app_message_outbox_begin_result = APP_MSG_OK;
    mock_app_message_outbox_send_result = APP_MSG_OK;
    handle_request_change_favorite(&game, 123, mock_favorite_change_callback);

    // Prepare fake tuples for the response with a WRONG request id
    TupleValue request_id_val = { .int32 = mock_written_request_id + 1 };
    mock_tuples[0].key = MESSAGE_KEY_REQUEST_ID;
    mock_tuples[0].value = &request_id_val;

    TupleValue result_val = { .int8 = FavoriteAdded };
    mock_tuples[1].key = MESSAGE_KEY_CONFIRM_FAVORITE;
    mock_tuples[1].value = &result_val;

    mock_tuple_count = 2;

    DictionaryIterator iter;
    handle_favorite_change_result(&iter);

    // Should have ignored the result
    TEST_ASSERT_FALSE(callback_called);
    TEST_ASSERT_FALSE(game.team1.favorite);
}

void test_handle_favorite_change_result_failure(void) {
    Game game;
    game.sport = SportNFL;
    game.team1.id = 123;
    game.team1.favorite = true; // Was already favorited, say we tried to remove and it failed

    mock_app_message_outbox_begin_result = APP_MSG_OK;
    mock_app_message_outbox_send_result = APP_MSG_OK;
    handle_request_change_favorite(&game, 123, mock_favorite_change_callback);

    TupleValue request_id_val = { .int32 = mock_written_request_id };
    mock_tuples[0].key = MESSAGE_KEY_REQUEST_ID;
    mock_tuples[0].value = &request_id_val;

    TupleValue result_val = { .int8 = FavoriteChangeFailed };
    mock_tuples[1].key = MESSAGE_KEY_CONFIRM_FAVORITE;
    mock_tuples[1].value = &result_val;

    mock_tuple_count = 2;

    DictionaryIterator iter;
    handle_favorite_change_result(&iter);

    TEST_ASSERT_TRUE(callback_called);
    TEST_ASSERT_EQUAL(123, callback_called_team_id);
    TEST_ASSERT_EQUAL(FavoriteChangeFailed, callback_called_result);
    TEST_ASSERT_TRUE(game.team1.favorite); // Unchanged
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_handle_request_change_favorite_outbox_begin_fails);
    RUN_TEST(test_handle_request_change_favorite_outbox_send_fails);
    RUN_TEST(test_handle_request_change_favorite_success);
    RUN_TEST(test_handle_favorite_change_result_success);
    RUN_TEST(test_handle_favorite_change_result_mismatched_request_id);
    RUN_TEST(test_handle_favorite_change_result_failure);
    return UNITY_END();
}
