#ifndef GAMES_HANDLER_H_
#define GAMES_HANDLER_H_

#include "../../model/models.h"
#include "../comms.h"

typedef void (*GameListErrorCallback)(AppError error);
typedef void (*GameListSuccessCallback)(int game_count, Game *games);
typedef void (*GameUpdateCallback)(GameUpdateResult state);
typedef void (*BreakdownSuccessCallback)(const char *breakdown_text);

// Game list requests
void handle_request_games(
    Sport sport,
    int league_index,
    GameListSuccessCallback on_success,
    GameListErrorCallback on_error
);

void handle_clear_games(void);
void handle_games_recieved(DictionaryIterator *iter);

// Individual game refresh
void update_game(Game *game, GameUpdateCallback on_update);
void handle_game_update_recieved(DictionaryIterator *iter);

// ESPN game breakdown
void handle_request_breakdown(
    Game *game,
    BreakdownSuccessCallback on_success,
    GameListErrorCallback on_error
);

void handle_breakdown_received(DictionaryIterator *iter);

// New: prevent late responses from opening an old screen
void handle_cancel_breakdown_request(void);

// New: process failed outgoing AppMessage requests
void handle_game_outbox_failed(DictionaryIterator *iter);

#endif