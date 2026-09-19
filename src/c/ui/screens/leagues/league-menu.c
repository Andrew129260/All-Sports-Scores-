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
static char* s_leagues[12]; 
static int s_num_leagues = 0;
static HeaderData s_header_data;
static MenuIndex s_saved_scroll_pos = {0, 0};

static void load_league_folders() {
    switch (s_current_sport) {
        case SportNFL:
            s_leagues[0] = "NFL"; s_leagues[1] = "College Football"; s_leagues[2] = "UFL"; s_leagues[3] = "CFL"; s_num_leagues = 4; break;
        case SportMLB:
            s_leagues[0] = "MLB"; s_leagues[1] = "College Baseball"; s_leagues[2] = "World Baseball Classic"; s_num_leagues = 3; break;
        case SportNHL:
            s_leagues[0] = "NHL"; s_leagues[1] = "College Hockey"; s_num_leagues = 2; break;
        case SportNBA:
            s_leagues[0] = "NBA"; s_leagues[1] = "WNBA"; s_leagues[2] = "College Basketball"; s_leagues[3] = "FIBA"; s_num_leagues = 4; break;
        case SportMLS:
            s_leagues[0] = "MLS"; s_leagues[1] = "Premier League"; s_leagues[2] = "La Liga"; s_leagues[3] = "Bundesliga"; s_leagues[4] = "Serie A"; s_leagues[5] = "Liga MX"; s_leagues[6] = "Champions League"; s_leagues[7] = "World Cup"; s_leagues[8] = "Women's World Cup"; s_num_leagues = 9; break;
        case SportRugby:
            s_leagues[0] = "NRL"; s_leagues[1] = "Six Nations"; s_leagues[2] = "Rugby World Cup"; s_num_leagues = 3; break;
        case SportCricket:
            s_leagues[0] = "International"; s_leagues[1] = "IPL"; s_leagues[2] = "Major League Cricket"; s_num_leagues = 3; break;
        case SportTennis:
            s_leagues[0] = "ATP"; s_leagues[1] = "WTA"; s_num_leagues = 2; break;
        case SportAFL:
            s_leagues[0] = "AFL"; s_num_leagues = 1; break;
        case SportMMA:
            s_leagues[0] = "UFC"; s_num_leagues = 1; break;
        default:
            s_leagues[0] = "All Games"; s_num_leagues = 1; break;
    }
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
    return s_num_leagues;
}

static int16_t menu_get_row_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    return PBL_IF_ROUND_ELSE(60, 44);
}

static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
    return PBL_IF_ROUND_ELSE(32, 0); 
}

static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
    #if defined(PBL_ROUND)
    GRect bounds = layer_get_bounds(cell_layer);
    graphics_context_set_text_color(ctx, GColorDukeBlue);
    graphics_draw_text(ctx, sport_get_name(s_current_sport), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    #endif
}

static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
    menu_cell_basic_draw(ctx, cell_layer, s_leagues[cell_index->row], "View Games", NULL);
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    show_games_menu(s_current_sport, cell_index->row);
}

static void build_menu_layer(Window *window) {
    if (s_menu_layer) return;

    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_frame(window_layer);
    
    #if defined(PBL_ROUND)
    bounds.origin.y += STATUS_BAR_LAYER_HEIGHT;
    bounds.size.h -= STATUS_BAR_LAYER_HEIGHT;
    #else
    int header_height = layer_get_bounds((Layer*)s_header).size.h;
    bounds.origin.y += header_height + 4;
    bounds.size.h -= header_height + 4; 
    #endif

    s_menu_layer = menu_layer_create(bounds);

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
    
    s_status_bar = status_bar_layer_create();
    status_bar_layer_set_colors(s_status_bar, GColorDukeBlue, GColorWhite);

    #if !defined(PBL_ROUND)
    GRect bounds = layer_get_frame(window_layer);
    s_header_data.title = sport_get_name(s_current_sport);
    s_header_data.info = NULL;
    s_header_data.under_status_bar = true;
    s_header = create_header_layer(bounds, s_header_data);
    layer_add_child(window_layer, (Layer*)s_header);
    #endif
    
    layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

    build_menu_layer(window);
}

static void destroy_ui(Window *window) {
    destroy_menu_layer();
    if(s_status_bar) { status_bar_layer_destroy(s_status_bar); s_status_bar = NULL; }
    #if !defined(PBL_ROUND)
    if(s_header) { layer_destroy((Layer*)s_header); s_header = NULL; }
    #endif
}

static void window_appear(Window *window) {
    #if defined(PBL_PLATFORM_APLITE)
    // Rebuild the heavy menu layer when navigating back
    build_menu_layer(window);
    #endif
}

static void window_disappear(Window *window) {
    #if defined(PBL_PLATFORM_APLITE)
    // Destroy the MenuLayer to save RAM while viewing games
    destroy_menu_layer();
    #endif
}

void show_league_menu(Sport sport) {
    if (s_current_sport != sport) {
        s_saved_scroll_pos.section = 0;
        s_saved_scroll_pos.row = 0;
    }
    s_current_sport = sport;
    load_league_folders();
    
    if (!s_window) {
        s_window = window_create();
        WindowHandlers handlers = {0};
        handlers.load = initialise_ui;
        handlers.unload = destroy_ui;
        handlers.appear = window_appear;
        handlers.disappear = window_disappear;
        window_set_window_handlers(s_window, handlers);
    }
    
    if (window_stack_get_top_window() != s_window) {
        window_stack_push(s_window, true);
    }
}

void hide_league_menu(void) {
    if (s_window) {
        window_stack_remove(s_window, true);
        window_destroy(s_window);
        s_window = NULL;
    }
}