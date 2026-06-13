#ifndef RESULT_WINDOW_H_
#define RESULT_WINDOW_H_

#include "pebble.h"
#include "../../../data/model/models.h"
#include "action-menu.h"

Window *result_window_create_favorite(Game *game, MenuAction action, FavoriteChangeResult result);
Window *result_window_create_refresh(Game *game, GameUpdateResult result);
Window *result_window_create_broadcast(Game *game);

#endif
