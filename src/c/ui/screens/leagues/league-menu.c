#include "pebble.h"
#include "league-menu.h"
#include "../../../image-cache.h"
#include "../games/games-menu.h"
#include "../../layers/header/header.h"
#include "../../../utils/utils.h"

static Window *s_window;
static MenuLayer *s_menu_layer;
static StatusBarLayer *s_status_bar;
static HeaderLayer *s_header;

static Sport s_current_sport;

// INCREASED TO 12: We now have 9 soccer leagues, so we need a slightly larger memory block!
static char* s_leagues[12]; 
static int s_num_leagues = 0;

// THE FIX: Static permanent memory to prevent stack corruption
static HeaderData s_header_data;

static void load_league_folders() {
    // CRITICAL: These indices must perfectly match the getEndpointsForSport array in api.js!
    switch (s_current_sport) {
        case SportNFL:
            s_leagues[0] = "NFL";
            s_leagues[1] = "College Football";
            s_leagues[2] = "UFL";
            s_leagues[3] = "CFL";
            s_num_leagues = 4;
            break;
        case SportMLB:
            s_leagues[0] = "MLB";
            s_leagues[1] = "College Baseball";
            s_leagues[2] = "World Baseball Classic";
            s_num_leagues = 3;
            break;
        case SportNHL:
            s_leagues[0] = "NHL";
            s_leagues[1] = "College Hockey";
            s_num_leagues = 2;
            break;
        case SportNBA:
            s_leagues[0] = "NBA";
            s_leagues[1] = "WNBA";
            s_leagues[2] = "College Basketball";
            s_leagues[3] = "FIBA Men's WC";
            s_leagues[4] = "FIBA Women's WC";
            s_num_leagues = 5;
            break;
        case SportMLS: // Soccer
            s_leagues[0] = "MLS";
            s_leagues[1] = "Premier League";
            s_leagues[2] = "La Liga";
            s_leagues[3] = "Bundesliga";
            s_leagues[4] = "Serie A";
            s_leagues[5] = "Liga MX";
            s_leagues[6] = "Champions League";
            s_leagues[7] = "World Cup";
            s_leagues[8] = "Women's World Cup";
            s_num_leagues = 9;
            break;
        case SportRugby:
            s_leagues[0] = "NRL";
            s_leagues[1] = "Six Nations";
            s_leagues[2] = "Rugby World Cup";
            s_num_leagues = 3;
            break;
        case SportCricket:
            s_leagues[0] = "International";
            s_leagues[1] = "IPL";
            s_leagues[2] = "Major League Cricket";
            s_num_leagues = 3;                     
            break;
        case SportTennis:
            s_leagues[0] = "ATP";
            s_leagues[1] = "WTA";
            s_num_leagues = 2;
            break;
        case SportAFL:
            s_leagues[0] = "AFL";
            s_num_leagues = 1;
            break;
        case SportMMA:
            s_leagues[0] = "UFC";
            s_num_leagues = 1;
            break;
        default:
            s_leagues[0] = "All Games";
            s_num_leagues = 1;
            break;
    }
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
    return s_num_leagues;
}

static int16_t menu_get_row_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    return PBL_IF_ROUND_ELSE(60, 44);
}

static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
    menu_cell_basic_draw(ctx, cell_layer, s_leagues[cell_index->row], "View Games", NULL);
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    // The exact row clicked here is sent back to the phone to match the api.js endpoint array!
    show_games_menu(s_current_sport, cell_index->row);
}

static void initialise_ui(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_frame(window_layer);
    
    s_status_bar = status_bar_layer_create();
    status_bar_layer_set_colors(s_status_bar, GColorDukeBlue, GColorWhite);

    // Safely assign to the static global block
    s_header_data.icon = image_cache_get_sport_icon(s_current_sport);
    s_header_data.title = sport_get_name(s_current_sport);
    s_header_data.info = NULL;
    s_header_data.under_status_bar = true;

    s_header = create_header_layer(bounds, s_header_data);
    
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
    if(s_menu_layer) { menu_layer_destroy(s_menu_layer); s_menu_layer = NULL; }
    if(s_status_bar) { status_bar_layer_destroy(s_status_bar); s_status_bar = NULL; }
    if(s_header) { layer_destroy(s_header); s_header = NULL; }
}

static void window_load(Window *window) {
    initialise_ui(window);
}

static void window_unload(Window *window) {
    destroy_ui(window);
}

void show_league_menu(Sport sport) {
    s_current_sport = sport;
    load_league_folders();
    
    // MEMORY IMPROVEMENT: Singleton Window Pattern
    if(!s_window) {
        s_window = window_create();
        WindowHandlers handlers = {0};
        handlers.load = window_load;
        handlers.unload = window_unload;
        window_set_window_handlers(s_window, handlers);
    }
    
    window_stack_push(s_window, true);
}

void hide_league_menu(void) {
    // DO NOT DESTROY THE WINDOW HERE! Let the OS keep the singleton alive to prevent fragmentation.
    window_stack_remove(s_window, true);
}
