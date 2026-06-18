#include "pebble.h"
#include "score-screen.h"
#include "score-layer.h"
#include "schedule-layer.h"
#include "../../layers/header/header.h"
#include "../action/action-menu.h"
#include "../../../data/model/models.h"
#include "../../../data/comms/comms.h"
#include "../../../utils/utils.h"
#include "../action/action-indicator-layer.h"
#include "../../../image-cache.h" 

static Window *scoreWindow;
static StatusBarLayer *s_status_bar;
static Layer *s_action_indicator;
static Layer *s_header;
static TextLayer *s_time;
static TextLayer *s_details;
static Layer *s_score;
static HeaderData s_header_data;

// THE FIX: s_game is now global so it persists across memory swaps!
Game *s_game = NULL; 

// RAM Heist state tracker
static bool s_ui_built = false;

static void build_ui(Window *window) {
    if (s_ui_built) return;
    
    // SAFETY GUARD: If the pointer is lost during a memory swap, abort gracefully!
    if (s_game == NULL) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "CRITICAL: s_game is NULL. Aborting UI load.");
        return;
    }

    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_frame(window_layer);

    s_status_bar = status_bar_layer_create();
    status_bar_layer_set_colors(s_status_bar, GColorDukeBlue, GColorWhite);

    const char *time_str = s_game->time ? s_game->time : "";
    const char *details_str = s_game->details ? s_game->details : "";
    const char *t1_score = s_game->team1.score ? s_game->team1.score : "";

    #if defined(PBL_PLATFORM_APLITE)
        s_header_data.icon = NULL; // Save massive PNG RAM on Aplite!
    #else
        s_header_data.icon = image_cache_get_sport_icon(s_game->sport);
    #endif
    
    s_header_data.title = sport_get_name(s_game->sport);
    s_header_data.info = time_str;
    s_header_data.under_status_bar = true;

    s_header = create_header_layer(bounds, s_header_data);

    layer_add_child(window_layer, s_header);
    layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

    int header_height = layer_get_bounds(s_header).size.h;
    bool is_scheduled = (strlen(t1_score) == 0);

    bounds.origin.y += header_height + (is_scheduled ? 12 : 4);
    bounds.size.h -= header_height + (is_scheduled ? 12 : 4); 

    s_score = is_scheduled ? schedule_layer_create(bounds, s_game) : score_layer_create(bounds, s_game);
    
    if (s_score != NULL) {
        layer_add_child(window_layer, s_score);
        bounds.origin.y += layer_get_bounds(s_score).size.h + 4;
        bounds.size.h -= layer_get_bounds(s_score).size.h + 4; 
    }

    if (strlen(details_str) > 0) {
        GRect details_bounds = bounds;
        details_bounds.size.h = 18;
        s_details = text_layer_create(details_bounds);
        text_layer_set_font(s_details, fonts_get_system_font(FONT_KEY_GOTHIC_14));
        text_layer_set_text_alignment(s_details, GTextAlignmentCenter);
        text_layer_set_text(s_details, details_str);
        Layer *details_layer = text_layer_get_layer(s_details);
        layer_set_clips(details_layer, false);
        layer_add_child(window_layer, details_layer);
        bounds.origin.y += 14;
        bounds.size.h -= 14; 
    }

    GRect time_bounds = bounds;
    time_bounds.size.h = 24;
    s_time = text_layer_create(time_bounds);
    text_layer_set_font(s_time, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(s_time, GTextAlignmentCenter);
    text_layer_set_text(s_time, time_str);
    Layer *time_layer = text_layer_get_layer(s_time);
    layer_set_clips(time_layer, false);
    layer_add_child(window_layer, time_layer);

    s_action_indicator = action_indicator_layer_create();
    if (s_action_indicator != NULL) {
        layer_add_child(window_layer, s_action_indicator);
    }

    s_ui_built = true;
}

static void tear_down_ui() {
    if (!s_ui_built) return;
    if (s_status_bar) { status_bar_layer_destroy(s_status_bar); s_status_bar = NULL; }
    if (s_action_indicator) { layer_destroy(s_action_indicator); s_action_indicator = NULL; }
    if (s_header) { layer_destroy(s_header); s_header = NULL; }
    if (s_details) { text_layer_destroy(s_details); s_details = NULL; }
    if (s_time) { text_layer_destroy(s_time); s_time = NULL; }
    if (s_score) { layer_destroy(s_score); s_score = NULL; }
    s_ui_built = false;
}

// Fire the action menu AFTER safely disassembling the UI
static void deferred_action_menu(void *data) {
    #if defined(PBL_PLATFORM_APLITE)
    // APLITE RAM HEIST: Destroy the score screen UI to make room for ActionMenu
    tear_down_ui();
    #endif
    
    game_action_menu_open(s_game);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    // Let the click event resolve before pulling the rug out!
    app_timer_register(50, deferred_action_menu, NULL);
}

static void click_config_provider(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void window_load(Window *window) {
    build_ui(window);
}

static void window_unload(Window *window) {
    tear_down_ui();
}

static void window_appear(Window *window) {
    #if defined(PBL_PLATFORM_APLITE)
    // Seamlessly rebuild UI when returning from the ActionMenu!
    build_ui(window);
    #endif
}

void show_score_screen(Game *game)
{
    // 1. Assign it first to ensure the pointer is set BEFORE window logic runs
    s_game = game;
    
    if (!scoreWindow) {
        scoreWindow = window_create();
        WindowHandlers handlers = {0};
        handlers.load = window_load;
        handlers.unload = window_unload;
        handlers.appear = window_appear;
        window_set_window_handlers(scoreWindow, handlers);
        window_set_click_config_provider(scoreWindow, click_config_provider);
    }
    
    window_stack_push(scoreWindow, true);
}

void hide_score_screen(void)
{
    window_stack_remove(scoreWindow, true);
}
