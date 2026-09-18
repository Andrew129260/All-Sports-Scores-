#include <stdbool.h>
#include <stdio.h>
#include "pebble.h"
#include "../../../image-cache.h"
#include "games-menu.h"
#include "../../layers/progress/progress-layer.h"
#include "../../layers/error/error-layer.h"
#include "../../layers/header/header.h"
#include "../score/score-screen.h"
#include "../../../data/model/models.h"
#include "../../../data/comms/comms.h"
#include "../../../data/comms/games/games-handler.h"
#include "../../../utils/utils.h"

static Window *gamesWindow;
static StatusBarLayer *s_status_bar;
static HeaderLayer *s_header;
static MenuLayer *s_menu_layer;
static TextLayer *s_loading_text;
static ProgressLayer *s_loading_progress;
static ErrorLayer *s_error_layer;
#if defined(PBL_ROUND)
static ContentIndicator *s_content_indicator;
static Layer *s_indicator_layer;
#endif

static int game_count;
static Game *games;
static Sport s_sport;
static int s_league_index = -1; 
static bool refreshing;
static HeaderData s_header_data;

static MenuIndex s_saved_scroll_pos = {0, 0}; 

static AppTimer *s_push_timer = NULL;
static Game *s_pending_game = NULL;

static void build_menu_layer(Window *window);
static void destroy_menu_layer();
static void refresh_games(Sport sport);

static void clear_temporary_ui() {
    if (s_error_layer) {
        layer_remove_from_parent((Layer*)s_error_layer);
        error_layer_destroy(s_error_layer);
        s_error_layer = NULL;
    }
    if (s_loading_text) {
        layer_remove_from_parent(text_layer_get_layer(s_loading_text));
        text_layer_destroy(s_loading_text);
        s_loading_text = NULL;
    }
    if (s_loading_progress) {
        layer_remove_from_parent((Layer*)s_loading_progress);
        progress_layer_destroy(s_loading_progress);
        s_loading_progress = NULL;
    }
}

static void on_games_loaded(int loaded_game_count, Game *loaded_games) {
    game_count = loaded_game_count;
    games = loaded_games;
    clear_temporary_ui();

    if (game_count > 0 && games != NULL) {
        refreshing = false;
        if(s_menu_layer != NULL) {
            menu_layer_reload_data(s_menu_layer);
        }
        #if defined(PBL_ROUND)
        if (s_content_indicator) {
            content_indicator_set_content_available(s_content_indicator, ContentIndicatorDirectionDown, game_count > 2);
        }
        #endif
    } else if (game_count == 0) {
        refreshing = false;
        if(s_menu_layer != NULL) {
            menu_layer_reload_data(s_menu_layer);
        }
    }
}

static void on_games_error(AppError error) {
    game_count = 0;
    games = NULL;
    refreshing = false; 
    clear_temporary_ui(); 

    if (!s_error_layer) {
        Layer *window_layer = window_get_root_layer(gamesWindow);
        GRect bounds = layer_get_frame(window_layer);
        GRect error_layer_bounds = GRect(bounds.origin.x + PBL_IF_ROUND_ELSE(32, 8), bounds.origin.y + bounds.size.h / 2 - PBL_IF_ROUND_ELSE(24, 36), bounds.size.w - PBL_IF_ROUND_ELSE(64, 16), 75);
        s_error_layer = error_layer_create(error_layer_bounds);
        layer_add_child(window_layer, (Layer*)s_error_layer);
    }
    error_layer_set_error(s_error_layer, error, s_sport);

    if (s_menu_layer != NULL) {
        menu_layer_reload_data(s_menu_layer);
    }
}

static void refresh_games(Sport sport) {
    s_pending_game = NULL;
    refreshing = true;
    game_count = 0;
    if (s_menu_layer != NULL) { menu_layer_reload_data(s_menu_layer); }
    clear_temporary_ui(); 
    
    Layer *window_layer = window_get_root_layer(gamesWindow);
    
    GRect bounds = layer_get_frame(window_layer);
    GRect loading_section_bounds = bounds;
    loading_section_bounds.origin.y += bounds.size.h / 2 - 16;
    loading_section_bounds.origin.x += PBL_IF_ROUND_ELSE(32, 16);
    loading_section_bounds.size.w -= PBL_IF_ROUND_ELSE(64, 32);
    loading_section_bounds.size.h = 28;

    if (!s_loading_progress) {
        GRect loading_bar_bounds = loading_section_bounds;
        loading_bar_bounds.size.h = 4;
        s_loading_progress = progress_layer_create(loading_bar_bounds);
        progress_layer_set_background_color(s_loading_progress, GColorVeryLightBlue);
        progress_layer_set_foreground_color(s_loading_progress, GColorDukeBlue);
        progress_layer_set_corner_radius(s_loading_progress, 2);
        layer_add_child(window_layer, (Layer*)s_loading_progress);
    }

    if (!s_loading_text) {
        GRect loading_text_bounds = loading_section_bounds;
        loading_text_bounds.origin.y += 4;
        loading_text_bounds.size.h = 24;
        s_loading_text = text_layer_create(loading_text_bounds);
        text_layer_set_font(s_loading_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
        text_layer_set_text_alignment(s_loading_text, GTextAlignmentCenter);
        
        if (s_sport == 0) text_layer_set_text(s_loading_text, "Loading favorites");
        else text_layer_set_text(s_loading_text, "Loading games");

        layer_add_child(window_layer, text_layer_get_layer(s_loading_text));
    }

    handle_request_games(sport, s_league_index, on_games_loaded, on_games_error);
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
    if (refreshing) return 0;
    return game_count;
}

static int16_t menu_get_row_height_callback (MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
    return PBL_IF_ROUND_ELSE(66, 58);
}

static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
    return PBL_IF_ROUND_ELSE(32, 0); 
}

static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
    #if defined(PBL_ROUND)
    GRect bounds = layer_get_bounds(cell_layer);
    graphics_context_set_text_color(ctx, GColorDukeBlue);
    
    const char *title = (s_sport == Favorites) ? "Favorites" : sport_get_name(s_sport);
    graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    #endif
}

static void menu_cell_game_large_draw(GContext* ctx, const Layer *cell_layer, bool selected, const Game *game) {
    GFont font_bold = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    GFont font_regular = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    GRect cell_bounds = layer_get_bounds(cell_layer);
    graphics_context_set_text_color(ctx, selected ? GColorWhite : GColorBlack);
  
    int horz_padding = PBL_IF_ROUND_ELSE(24, 16); 
    int vert_padding = PBL_IF_ROUND_ELSE(4, 0); 
    int screen_w = cell_bounds.size.w;
    
    #if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
        int name_width = 65;
        int score_width = 110; 
    #elif defined(PBL_PLATFORM_CHALK)
        int name_width = 55;
        int score_width = 85;
    #else
        int name_width = 46;
        int score_width = 74;
    #endif

    const char *t1_name = game->team1.name ? game->team1.name : "";
    const char *t2_name = game->team2.name ? game->team2.name : "";
    const char *t1_score = game->team1.score ? game->team1.score : "";
    const char *t2_score = game->team2.score ? game->team2.score : "";
    const char *details = game->details ? game->details : "";
    const char *time_str = game->time ? game->time : "";

    GRect team_1_name_bounds = GRect(horz_padding, vert_padding, name_width, 18);
    graphics_draw_text(ctx, t1_name, font_bold, team_1_name_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    
    GRect team_2_name_bounds = GRect(horz_padding, vert_padding + 18, name_width, 18);
    graphics_draw_text(ctx, t2_name, font_bold, team_2_name_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    GRect team_1_score_bounds = GRect(screen_w - horz_padding - score_width - 8, vert_padding, score_width, 18);
    graphics_draw_text(ctx, t1_score, font_bold, team_1_score_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

    GRect team_2_score_bounds = GRect(screen_w - horz_padding - score_width - 8, vert_padding + 18, score_width, 18);
    graphics_draw_text(ctx, t2_score, font_bold, team_2_score_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

    GRect details_bounds = GRect(horz_padding, vert_padding + 36, (screen_w / 2) - horz_padding, 14);
    graphics_draw_text(ctx, details, font_regular, details_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    GRect time_bounds = GRect(screen_w / 2, vert_padding + 36, (screen_w / 2) - horz_padding, 14);
    graphics_draw_text(ctx, time_str, font_regular, time_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

    graphics_context_set_fill_color(ctx, selected ? GColorWhite : GColorBlack);
    if (game->possession == Team1) {
        graphics_fill_circle(ctx, GPoint(horz_padding + name_width + 4, vert_padding + 12), 2);
    } else if (game->possession == Team2) {
        graphics_fill_circle(ctx, GPoint(horz_padding + name_width + 4, vert_padding + 30), 2);
    }
}

static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
    if (s_menu_layer == NULL) return;
    if (cell_index->row >= game_count || games == NULL) return;
    
    bool selected = menu_layer_is_index_selected(s_menu_layer, cell_index);

    // Draw the full, detailed layout for ALL rows on ALL platforms
    menu_cell_game_large_draw(ctx, cell_layer, selected, &games[cell_index->row]);
}

static void push_score_screen_callback(void *data) {
    if (s_pending_game != NULL) {
        show_score_screen(s_pending_game);
    }
    s_push_timer = NULL;
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
    if (game_count > 0 && cell_index->row < game_count && games != NULL) {
        s_pending_game = &games[cell_index->row];
        if (s_pending_game != NULL) {
            if (s_push_timer != NULL) {
                app_timer_cancel(s_push_timer);
            }
            s_push_timer = app_timer_register(100, push_score_screen_callback, NULL);
        }
    } else if (!refreshing) {
        refresh_games(s_sport);
    }
}

static void menu_selection_changed_callback(MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context) {
    #if defined(PBL_ROUND)
    if (s_content_indicator) {
        content_indicator_set_content_available(s_content_indicator, ContentIndicatorDirectionDown, new_index.row < game_count - 2);
    }
    #endif
}

static void build_menu_layer(Window *window) {
    if (s_menu_layer) return;
    Layer *window_layer = window_get_root_layer(window);
    GRect menu_bounds = layer_get_frame(window_layer);
    
    #if defined(PBL_ROUND)
    menu_bounds.origin.y += STATUS_BAR_LAYER_HEIGHT;
    menu_bounds.size.h -= STATUS_BAR_LAYER_HEIGHT;
    #else
    int header_height = layer_get_bounds((Layer*)s_header).size.h;
    menu_bounds.origin.y += header_height + 4;
    menu_bounds.size.h -= header_height + 4;
    #endif

    s_menu_layer = menu_layer_create(menu_bounds);

    #if defined(PBL_ROUND)
    menu_layer_set_center_focused(s_menu_layer, false);
    #endif

    menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
        .get_num_rows = menu_get_num_rows_callback,
        .get_cell_height = menu_get_row_height_callback,
        .get_header_height = menu_get_header_height_callback,
        .draw_header = menu_draw_header_callback,
        .draw_row = menu_draw_row_callback,
        .select_click = menu_select_callback,
        .selection_changed = menu_selection_changed_callback,
    });
    menu_layer_set_highlight_colors(s_menu_layer, GColorDukeBlue, GColorWhite);
    menu_layer_set_click_config_onto_window(s_menu_layer, window);
    layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

    if (game_count > 0) {
        menu_layer_reload_data(s_menu_layer);
        menu_layer_set_selected_index(s_menu_layer, s_saved_scroll_pos, MenuRowAlignCenter, false);
    }
}

static void destroy_menu_layer() {
    if (s_menu_layer) {
        s_saved_scroll_pos = menu_layer_get_selected_index(s_menu_layer);
        layer_remove_from_parent(menu_layer_get_layer(s_menu_layer));
        menu_layer_destroy(s_menu_layer);
        s_menu_layer = NULL;
    }
}

static void initialise_ui(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    
    s_status_bar = status_bar_layer_create();
    status_bar_layer_set_colors(s_status_bar, GColorDukeBlue, GColorWhite);

    #if !defined(PBL_ROUND)
    GRect bounds = layer_get_frame(window_layer);
    s_header_data.title = sport_get_name(s_sport);
    s_header_data.info = NULL;
    s_header_data.under_status_bar = true;
    s_header = create_header_layer(bounds, s_header_data);
    layer_add_child(window_layer, (Layer*)s_header);
    #endif
    
    layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

    build_menu_layer(window);
    refresh_games(s_sport);
}

static void destroy_ui(Window *window) {
    if (s_push_timer) {
        app_timer_cancel(s_push_timer);
        s_push_timer = NULL;
    }
    destroy_menu_layer();
    clear_temporary_ui();
    if(s_status_bar) { status_bar_layer_destroy(s_status_bar); s_status_bar = NULL; }
    #if !defined(PBL_ROUND)
    if(s_header) { layer_destroy((Layer*)s_header); s_header = NULL; }
    #endif
    #if defined(PBL_ROUND)
    if(s_indicator_layer) { layer_destroy(s_indicator_layer); s_indicator_layer = NULL; }
    if(s_content_indicator) { content_indicator_destroy(s_content_indicator); s_content_indicator = NULL; }
    #endif
}

static void window_appear(Window *window) {
    #if defined(PBL_PLATFORM_APLITE)
    build_menu_layer(window);
    #endif
}

static void window_disappear(Window *window) {
    #if defined(PBL_PLATFORM_APLITE)
    destroy_menu_layer();
    #endif
}

static void window_load(Window *window) { initialise_ui(window); }
static void window_unload(Window *window) { destroy_ui(window); }

void show_games_menu(Sport sport, int league_index) {
    if (s_sport != sport || s_league_index != league_index) {
        s_saved_scroll_pos.section = 0;
        s_saved_scroll_pos.row = 0;
    }
    s_league_index = league_index; 
    s_sport = sport;
    if (!gamesWindow) {
        gamesWindow = window_create();
        window_set_window_handlers(gamesWindow, (WindowHandlers){
            .load = window_load, 
            .unload = window_unload,
            .appear = window_appear,
            .disappear = window_disappear
        });
    }
    if (window_stack_get_top_window() != gamesWindow) {
        window_stack_push(gamesWindow, true);
    }
}

void hide_games_menu(void) { 
    if (gamesWindow) {
        window_stack_remove(gamesWindow, true);
        window_destroy(gamesWindow);
        gamesWindow = NULL;
    }
}