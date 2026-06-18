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

// Static Buffers: Pre-allocate memory so we NEVER have to call malloc() or free()!
static char s_team_1_buffer[64];
static char s_team_2_buffer[64];
static char s_broadcast_buffer[32]; // Unified broadcast buffer

static void silent_favorite_change_callback(int team_id, FavoriteChangeResult result) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Background favorite sync complete, result = %d", result);
}

static void game_update_callback(GameUpdateResult state) {
    if (state == GameUpdated) {
        action_menu_unfreeze(s_action_menu);
    } else {
        #if defined(PBL_PLATFORM_APLITE)
        vibes_short_pulse();
        #else
        s_result_window = result_window_create_refresh(s_game, state);
        action_menu_set_result_window(s_action_menu, s_result_window);
        #endif
        action_menu_unfreeze(s_action_menu);
    }
}

static void action_click_callback(ActionMenu *menu, const ActionMenuItem *performed_action, void *context) {
    MenuAction action = *(MenuAction *)action_menu_item_get_action_data(performed_action);
    current_action = action;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "action click callback, action = %d", action);
    
    switch (action) {
        case ACTION_REFRESH_GAME: 
            action_menu_freeze(s_action_menu);
            update_game(s_game, game_update_callback);
            break;
            
        case ACTION_TEAM_1: 
            handle_request_change_favorite(s_game, s_game->team1.id, silent_favorite_change_callback);
            s_game->team1.favorite = !s_game->team1.favorite; 

            #if defined(PBL_PLATFORM_APLITE)
            vibes_short_pulse();
            #else
            s_result_window = result_window_create_favorite(s_game, current_action, s_game->team1.favorite ? 1 : 0);
            action_menu_set_result_window(s_action_menu, s_result_window);
            #endif
            break;
            
        case ACTION_TEAM_2: 
            handle_request_change_favorite(s_game, s_game->team2.id, silent_favorite_change_callback);
            s_game->team2.favorite = !s_game->team2.favorite;

            #if defined(PBL_PLATFORM_APLITE)
            vibes_short_pulse();
            #else
            s_result_window = result_window_create_favorite(s_game, current_action, s_game->team2.favorite ? 1 : 0);
            action_menu_set_result_window(s_action_menu, s_result_window);
            #endif
            break;
            
        case ACTION_BROADCAST: 
            // UNIVERSAL UX: They already see the network on the button. Just vibrate and close!
            vibes_short_pulse();
            break;
            
        default:
            break;
    }
}

static void action_menu_did_close(ActionMenu *action_menu, const ActionMenuItem *item, void *context) {
    action_menu_hierarchy_destroy(s_level, NULL, NULL);
}

static void populate_label_buffer(char *buffer, size_t buffer_size, char *team_name, bool is_favorite) {
    const char *safe_name = team_name ? team_name : "Team";
    
    if(is_favorite) {
        snprintf(buffer, buffer_size, "Remove %s", safe_name); 
    } else {
        snprintf(buffer, buffer_size, "Add %s", safe_name);
    }
}

void game_action_menu_open(Game *game) {
    s_level = action_menu_level_create(4);
    s_game = game;

    populate_label_buffer(s_team_1_buffer, sizeof(s_team_1_buffer), s_game->team1.name, s_game->team1.favorite);
    populate_label_buffer(s_team_2_buffer, sizeof(s_team_2_buffer), s_game->team2.name, s_game->team2.favorite);
    
    // Universal Dynamic Broadcast Label
    if (s_game->broadcast && strlen(s_game->broadcast) > 0) {
        snprintf(s_broadcast_buffer, sizeof(s_broadcast_buffer), "TV: %s", s_game->broadcast);
    } else {
        snprintf(s_broadcast_buffer, sizeof(s_broadcast_buffer), "TV: Unlisted");
    }

    s_labels.game = s_game;
    s_labels.team_1_label = s_team_1_buffer;
    s_labels.team_2_label = s_team_2_buffer;

    action_menu_level_add_action(s_level, "Refresh", action_click_callback, &refresh_game);
    action_menu_level_add_action(s_level, s_labels.team_1_label, action_click_callback, &team_1);
    action_menu_level_add_action(s_level, s_labels.team_2_label, action_click_callback, &team_2);
    
    // UNIVERSAL UI: Apply the TV Network display directly to the button for all watches
    action_menu_level_add_action(s_level, s_broadcast_buffer, action_click_callback, &broadcast);

    ActionMenuConfig config_temp = {0};
    config_temp.root_level = s_level;
    config_temp.colors.background = GColorDukeBlue;
    config_temp.colors.foreground = GColorWhite;
    config_temp.context = &s_labels;
    config_temp.did_close = action_menu_did_close; 
    
    config = config_temp;

    s_action_menu = action_menu_open(&config);
}
