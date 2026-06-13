#ifndef ACTION_MENU_H_
#define ACTION_MENU_H_

#include "../../../data/model/models.h"

typedef enum  {
    ACTION_REFRESH_GAME, ACTION_TEAM_1, ACTION_TEAM_2, ACTION_BROADCAST
} MenuAction;

typedef struct {
    Game *game;
    char *team_1_label;
    char *team_2_label;
} ActionMenuLabels;

void game_action_menu_open(Game *game);

#endif