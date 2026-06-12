#include "pebble.h"
#include "schedule-layer.h"

static void schedule_update_proc(Layer *layer, GContext *ctx) {
    Game *game = *(Game **)layer_get_data(layer);
    GRect layer_bounds = layer_get_bounds(layer);

    GFont font_team = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    GFont font_record = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

    graphics_context_set_fill_color(ctx, GColorBlack);
    GRect separator_bounds = GRect(layer_bounds.size.w / 2 - 1, 4, 2, 48);
    graphics_fill_rect(ctx, separator_bounds, 0, GCornerNone);
    graphics_context_set_text_color(ctx, GColorBlack);

    int half_w = separator_bounds.origin.x;
    int right_x = separator_bounds.origin.x + separator_bounds.size.w;

    // Team 1
    GSize tm1_sz = graphics_text_layout_get_content_size(game->team1.name, font_team, GRect(0, 0, half_w, 32), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    GRect tm1_bnds = GRect((half_w / 2) - (tm1_sz.w / 2), 0, tm1_sz.w + 10, 32);
    graphics_draw_text(ctx, game->team1.name, font_team, tm1_bnds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

    // Team 2
    GSize tm2_sz = graphics_text_layout_get_content_size(game->team2.name, font_team, GRect(0, 0, half_w, 32), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    GRect tm2_bnds = GRect(right_x + (half_w / 2) - (tm2_sz.w / 2), 0, tm2_sz.w + 10, 32);
    graphics_draw_text(ctx, game->team2.name, font_team, tm2_bnds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

    // Record 1
    GSize rec1_sz = graphics_text_layout_get_content_size(game->team1.record, font_record, GRect(0, 0, half_w, 18), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    GRect rec1_bnds = GRect((half_w / 2) - (rec1_sz.w / 2), 28, rec1_sz.w + 10, 18);
    graphics_draw_text(ctx, game->team1.record, font_record, rec1_bnds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

    // Record 2
    GSize rec2_sz = graphics_text_layout_get_content_size(game->team2.record, font_record, GRect(0, 0, half_w, 18), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    GRect rec2_bnds = GRect(right_x + (half_w / 2) - (rec2_sz.w / 2), 28, rec2_sz.w + 10, 18);
    graphics_draw_text(ctx, game->team2.record, font_record, rec2_bnds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

Layer *schedule_layer_create(GRect bounds, Game *game) {
    Layer *schedule; 
    bounds.size.h = 56;

    #ifdef PBL_ROUND
        bounds.size.w -= 32;
        bounds.origin.x += 16;
    #endif

    schedule = layer_create_with_data(bounds, sizeof(Game*));
    Game **layer_data = (Game **)layer_get_data(schedule);
    *layer_data = game;
    layer_set_update_proc(schedule, schedule_update_proc);

    return schedule;
}
