#include <stdbool.h>
#include "pebble.h"
#include "../../../data/model/models.h"
#include "../../../data/comms/favorites/favorites-handler.h"
#include "../../../data/comms/games/games-handler.h"
#include "action-menu.h"
#include "result-window.h"

static ActionMenu *s_action_menu;
static ActionMenuLevel *s_level;
static ActionMenuConfig config;
static ActionMenuLabels s_labels;
static Game *s_game;
static Window *s_result_window;
static MenuAction current_action;

static MenuAction refresh_game = ACTION_REFRESH_GAME;
static MenuAction team_1 = ACTION_TEAM_1;
static MenuAction team_2 = ACTION_TEAM_2;
static MenuAction broadcast = ACTION_BROADCAST;

// 1. New Silent Callback: Lets the phone do the heavy lifting in the background without freezing the UI!
static void silent_favorite_change_callback(int team_id, FavoriteChangeResult result) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Background favorite sync complete, result = %d", result);
}

static void game_update_callback(GameUpdateResult state) {
    if (state == GameUpdated) {
        action_menu_unfreeze(s_action_menu);
    } else {
        s_result_window = result_window_create_refresh(s_game, state);
        action_menu_set_result_window(s_action_menu, s_result_window);
        action_menu_unfreeze(s_action_menu);
    }
}

static void action_click_callback(ActionMenu *menu, const ActionMenuItem *performed_action, void *context) {
    MenuAction action = *(MenuAction *)action_menu_item_get_action_data(performed_action);
    current_action = action;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "action click callback, action = %d", action);
    
    switch (action) {
        case ACTION_REFRESH_GAME: 
            // Only freeze the UI for a refresh, since we actually need to wait for the new score payload
            action_menu_freeze(s_action_menu);
            update_game(s_game, game_update_callback);
            break;
            
        case ACTION_TEAM_1: 
            // Optimistic UI: Send request in background, instantly show animation
            handle_request_change_favorite(s_game, s_game->team1.id, silent_favorite_change_callback);
            s_result_window = result_window_create_favorite(s_game, current_action, s_game->team1.favorite ? 0 : 1);
            action_menu_set_result_window(s_action_menu, s_result_window);
            break;
            
        case ACTION_TEAM_2: 
            // Optimistic UI: Send request in background, instantly show animation
            handle_request_change_favorite(s_game, s_game->team2.id, silent_favorite_change_callback);
            s_result_window = result_window_create_favorite(s_game, current_action, s_game->team2.favorite ? 0 : 1);
            action_menu_set_result_window(s_action_menu, s_result_window);
            break;
            
        case ACTION_BROADCAST: 
            s_result_window = result_window_create_broadcast(s_game);
            action_menu_set_result_window(s_action_menu, s_result_window);
            break;
            
        default:
            break;
    }
}

static void action_menu_close_callback (ActionMenu *menu, const ActionMenuItem *performed_action, void *context) {
    ActionMenuLabels *labels = (ActionMenuLabels *) context;
    if (labels->team_1_label) {
        free(labels->team_1_label);
        labels->team_1_label = NULL;
    }
    if (labels->team_2_label) {
        free(labels->team_2_label);
        labels->team_2_label = NULL;
    }
}

static char *create_label(char *team_name, bool is_favorite) {
    char *before = "Add ";
    char *after = " to Favorites";
    if(is_favorite) {
        before = "Remove ";
        after = " from Favorites";
    }
    char *label = malloc(strlen(team_name) + strlen(before) + strlen(after) + 1);
    strcpy(label, before);
    strcat(label, team_name);
    strcat(label, after);
    return label;
}

void game_action_menu_open(Game *game) {
    s_level = action_menu_level_create(4);
    s_game = game;

    char *team_1_label = create_label(s_game->team1.name, s_game->team1.favorite);
    char *team_2_label = create_label(s_game->team2.name, s_game->team2.favorite);
    
    s_labels = (ActionMenuLabels) {
        .game = s_game,
        .team_1_label = team_1_label,
        .team_2_label = team_2_label
    };

    action_menu_level_add_action(s_level, "Refresh", action_click_callback, &refresh_game);
    action_menu_level_add_action(s_level, s_labels.team_1_label, action_click_callback, &team_1);
    action_menu_level_add_action(s_level, s_labels.team_2_label, action_click_callback, &team_2);
    action_menu_level_add_action(s_level, "Where to Watch", action_click_callback, &broadcast);

    config = (ActionMenuConfig) {
        .root_level = s_level,
        .colors = {
            .background = GColorDukeBlue,
            .foreground = GColorWhite
        },
        .context = &s_labels,
        .will_close = action_menu_close_callback
    };

    s_action_menu = action_menu_open(&config);
}
