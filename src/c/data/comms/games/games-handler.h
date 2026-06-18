#include "../../model/models.h"
#include "../comms.h"

#ifndef GAMES_HANDLER_H_

typedef void (*GameListErrorCallback)(AppError error);
typedef void (*GameListSuccessCallback)(int game_count, Game **games);
typedef void (*GameUpdateCallback)(GameUpdateResult state);

#endif

#define GAMES_HANDLER_H_

void handle_request_games(Sport sport, int league_index, GameListSuccessCallback on_success, GameListErrorCallback on_error);
void handle_clear_games();
void handle_games_recieved(DictionaryIterator *iter);
void update_game(Game *game, GameUpdateCallback on_update);
void handle_game_update_recieved(DictionaryIterator *iter);
