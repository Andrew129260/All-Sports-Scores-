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

// --- CALLBACKS ---

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

// --- CLICK HANDLERS FOR WRAP-AROUND ---

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    MenuIndex current = menu_layer_get_selected_index(s_menu_layer);
    MenuIndex next;
    next.section = 0;
    
    if (current.row == 0) {
        next.row = NUM_SPORTS - 1; // We are at top, wrap to bottom
    } else {
        next.row = current.row - 1; // Move up one normally
    }
    menu_layer_set_selected_index(s_menu_layer, next, MenuRowAlignCenter, true);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    MenuIndex current = menu_layer_get_selected_index(s_menu_layer);
    MenuIndex next;
    next.section = 0;
    
    if (current.row == NUM_SPORTS - 1) {
        next.row = 0; // We are at bottom, wrap to top
    } else {
        next.row = current.row + 1; // Move down one normally
    }
    menu_layer_set_selected_index(s_menu_layer, next, MenuRowAlignCenter, true);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    MenuIndex current = menu_layer_get_selected_index(s_menu_layer);
    // Manually trigger the select behavior
    menu_select_callback(s_menu_layer, &current, context);
}

static void sports_click_config_provider(void *context) {
    // 100ms delay allows for smooth, fast scrolling when holding the button
    window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, up_click_handler);
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, down_click_handler);
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

// --- UI LIFECYCLE ---

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
    
    // Register custom click provider instead of auto-binding
    window_set_click_config_provider(window, sports_click_config_provider);

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
