#include "pebble.h"
#include "sports-menu.h"
#include "src/c/ui/screens/leagues/league-menu.h"
#include "ui/screens/games/games-menu.h"
#include "data/model/models.h"
#include "ui/layers/header/header.h"
#include "image-cache.h" 

#define NUM_SPORTS 11

static Window *s_window;
static MenuLayer *s_menu_layer;
static StatusBarLayer *s_status_bar;
static HeaderLayer *s_header;
static MenuIndex s_saved_scroll_pos = {0, 0};

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
    return NUM_SPORTS; 
}

static int16_t menu_get_row_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    return PBL_IF_ROUND_ELSE(60, 44);
}

static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
    Sport sport = (Sport)cell_index->row;
    GBitmap *icon = image_cache_get_sport_icon(sport);
    menu_cell_basic_draw(ctx, cell_layer, sport_get_name(sport), NULL, icon);
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    Sport sport = (Sport)cell_index->row;
    if (sport == Favorites) {
        show_games_menu(Favorites, -1);
    } else {
        show_league_menu(sport);
    }
}

static void build_menu_layer(Window *window) {
    if (s_menu_layer) return;

    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_frame(window_layer);
    
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

    // Restore scroll position smoothly upon returning
    menu_layer_set_selected_index(s_menu_layer, s_saved_scroll_pos, MenuRowAlignCenter, false);
}

static void destroy_menu_layer(void) {
    if (s_menu_layer) {
        s_saved_scroll_pos = menu_layer_get_selected_index(s_menu_layer);
        layer_remove_from_parent(menu_layer_get_layer(s_menu_layer));
        menu_layer_destroy(s_menu_layer);
        s_menu_layer = NULL;
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
    
    layer_add_child(window_layer, s_header);
    layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

    build_menu_layer(window);
}

static void destroy_ui(Window *window) {
    destroy_menu_layer();
    if (s_status_bar) { status_bar_layer_destroy(s_status_bar); s_status_bar = NULL; }
    if (s_header) { layer_destroy(s_header); s_header = NULL; }
}

static void window_appear(Window *window) {
    #if defined(PBL_PLATFORM_APLITE)
    // Rebuild the heavy menu layer when navigating back
    build_menu_layer(window);
    #endif
}

static void window_disappear(Window *window) {
    #if defined(PBL_PLATFORM_APLITE)
    // Flush all loaded bitmaps from the heap when leaving the main menu
    image_cache_deinit();
    image_cache_init();
    
    // Destroy the MenuLayer to save RAM while in sub-menus
    destroy_menu_layer();
    #endif
}

void show_sports_menu(void) {
    if (!s_window) {
        s_window = window_create();
        window_set_window_handlers(s_window, (WindowHandlers){
            .load = initialise_ui,
            .unload = destroy_ui,
            .appear = window_appear,
            .disappear = window_disappear
        });
    }
    
    if (window_stack_get_top_window() != s_window) {
        window_stack_push(s_window, true);
    }
}

void hide_sports_menu(void) {
    if (s_window) {
        window_stack_remove(s_window, true);
        window_destroy(s_window);
        s_window = NULL;
    }
}