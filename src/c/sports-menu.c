#include "pebble.h"
#include "sports-menu.h"
#include "src/c/ui/screens/leagues/league-menu.h"
#include "ui/screens/games/games-menu.h"
#include "data/model/models.h"
#include "ui/layers/header/header.h"
#include "image-cache.h" 

static Window *s_window;
static MenuLayer *s_menu_layer;
static StatusBarLayer *s_status_bar;
static HeaderLayer *s_header;

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
    return 11; // Favorites + 10 Sports
}

static int16_t menu_get_row_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    return PBL_IF_ROUND_ELSE(60, 44);
}

static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
    Sport sport = (Sport)cell_index->row;
    
    // Pull the icon safely from our global cache! No more local decoding.
    GBitmap *icon = image_cache_get_sport_icon(sport);
    
    menu_cell_basic_draw(ctx, cell_layer, sport_get_name(sport), NULL, icon);
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    Sport sport = (Sport)cell_index->row;
    
    if (sport == Favorites) {
        // Skip the folder system entirely and just load all pinned games
        show_games_menu(Favorites, -1);
    } else {
        // Open the nested folder menu for that specific sport
        show_league_menu(sport);
    }
}

static void initialise_ui(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_frame(window_layer);
    
    s_status_bar = status_bar_layer_create();
    status_bar_layer_set_colors(s_status_bar, GColorDukeBlue, GColorWhite);

    s_header = create_header_layer(bounds, (HeaderData) {
        .title = "All Sports",
        .under_status_bar = true,
    });
    
    int header_height = PBL_IF_RECT_ELSE(layer_get_bounds(s_header).size.h, 8);
    bounds.origin.y += header_height + 4;
    bounds.size.h -= header_height + 4; 

    s_menu_layer = menu_layer_create(bounds);
    menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
        .get_num_rows = menu_get_num_rows_callback,
        .get_cell_height = menu_get_row_height_callback,
        .draw_row = menu_draw_row_callback,
        .select_click = menu_select_callback,
    });

    menu_layer_set_highlight_colors(s_menu_layer, GColorDukeBlue, GColorWhite);
    menu_layer_set_click_config_onto_window(s_menu_layer, window);

    layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
    layer_add_child(window_layer, s_header);
    layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));
}

static void destroy_ui(Window *window) {
    menu_layer_destroy(s_menu_layer);
    status_bar_layer_destroy(s_status_bar);
    layer_destroy(s_header);
}

static void handle_window_unload(Window *window) {
    destroy_ui(window);
}

void show_sports_menu(void) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
        .load = initialise_ui,
        .unload = handle_window_unload,
    });
    window_stack_push(s_window, true);
}

void hide_sports_menu(void) {
    window_stack_remove(s_window, true);
    window_destroy(s_window);
}
