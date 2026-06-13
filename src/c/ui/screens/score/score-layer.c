#include "pebble.h"
#include "score-layer.h"
#include "../../../data/comms/prefs/prefs-handler.h"

static void score_update_proc(Layer *layer, GContext *ctx) {
    Game *game = *(Game **)layer_get_data(layer);
    bool has_long_score = strlen(game->team1.score) > 2 || strlen(game->team2.score) > 2;

    GRect layer_bounds = layer_get_bounds(layer);

    // ==========================================
    // PLATFORM SPECIFIC LAYOUT SCALING
    // ==========================================
    GFont font_score;
    int score_y;
    int record_score_y;

    #if PBL_DISPLAY_WIDTH > 144
        // Time 2 (Emery) - 200px Wide Displays
        font_score = fonts_get_system_font(has_long_score ? FONT_KEY_LECO_36_BOLD_NUMBERS : FONT_KEY_LECO_42_NUMBERS);
        score_y = has_long_score ? 4 : 0;
        record_score_y = has_long_score ? 11 : 7;
    #else
        // Pebble Classic, Time, Time Steel - 144px Wide Displays
        // We use Gothic 24 here for long scores so 5-character strings (like 241/4) never word-wrap!
        font_score = fonts_get_system_font(has_long_score ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_LECO_32_BOLD_NUMBERS);
        score_y = has_long_score ? 8 : 4;
        record_score_y = has_long_score ? 15 : 11;
    #endif

    GFont font_team = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    GFont font_record = fonts_get_system_font(FONT_KEY_GOTHIC_14);

    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_context_set_text_color(ctx, GColorBlack);
    
    GRect separator_bounds = GRect(layer_bounds.size.w / 2 - 1, 12, 2, 48);
    
    // Layout helpers
    int half_w = separator_bounds.origin.x;
    int right_x = separator_bounds.origin.x + separator_bounds.size.w;

    bool record_showing = clay_settings.show_record == ShowRecordAlways || (clay_settings.show_record == ShowRecordFinalOnly && strcmp(game->time, "Final")==0);
    
    if (record_showing) {
        // Record 1 (Manually centered, left-aligned)
        GSize rec1_sz = graphics_text_layout_get_content_size(game->team1.record, font_record, GRect(0, 0, half_w, 14), GTextOverflowModeWordWrap, GTextAlignmentLeft);
        GRect rec1_bnds = GRect((half_w / 2) - (rec1_sz.w / 2), 0, rec1_sz.w + 10, 14);
        graphics_draw_text(ctx, game->team1.record, font_record, rec1_bnds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

        // Record 2 (Manually centered, left-aligned)
        GSize rec2_sz = graphics_text_layout_get_content_size(game->team2.record, font_record, GRect(0, 0, half_w, 14), GTextOverflowModeWordWrap, GTextAlignmentLeft);
        GRect rec2_bnds = GRect(right_x + (half_w / 2) - (rec2_sz.w / 2), 0, rec2_sz.w + 10, 14);
        graphics_draw_text(ctx, game->team2.record, font_record, rec2_bnds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    }

    int final_score_y = record_showing ? record_score_y : score_y;
    int score_height = has_long_score ? 30 : 42; 
    
    // Score 1 (Manually centered, left-aligned)
    GSize sc1_sz = graphics_text_layout_get_content_size(game->team1.score, font_score, GRect(0, 0, half_w, score_height), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    GRect sc1_bnds = GRect((half_w / 2) - (sc1_sz.w / 2), final_score_y, sc1_sz.w + 10, score_height);
    graphics_draw_text(ctx, game->team1.score, font_score, sc1_bnds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

    // Score 2 (Manually centered, left-aligned)
    GSize sc2_sz = graphics_text_layout_get_content_size(game->team2.score, font_score, GRect(0, 0, half_w, score_height), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    GRect sc2_bnds = GRect(right_x + (half_w / 2) - (sc2_sz.w / 2), final_score_y, sc2_sz.w + 10, score_height);
    graphics_draw_text(ctx, game->team2.score, font_score, sc2_bnds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

    int possession_offset = 5;
    int possession_y = record_showing ? 50 : 43;

    // Team 1 (Manually centered, left-aligned)
    bool team_1_possession = (game->possession) == Team1;
    GSize tm1_sz = graphics_text_layout_get_content_size(game->team1.name, font_team, GRect(0, 0, half_w, 26), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    GRect tm1_bnds = GRect((half_w / 2) - (tm1_sz.w / 2) - (team_1_possession ? possession_offset : 0), possession_y, tm1_sz.w + 10, 26);
    graphics_draw_text(ctx, game->team1.name, font_team, tm1_bnds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    if (team_1_possession) {
        graphics_fill_circle(ctx, GPoint(tm1_bnds.origin.x + tm1_sz.w + 6 - 2, tm1_bnds.origin.y + tm1_bnds.size.h / 2), 2);
    }

    // Team 2 (Manually centered, left-aligned)
    bool team_2_possession = (game->possession) == Team2;
    GSize tm2_sz = graphics_text_layout_get_content_size(game->team2.name, font_team, GRect(0, 0, half_w, 26), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    GRect tm2_bnds = GRect(right_x + (half_w / 2) - (tm2_sz.w / 2) - (team_2_possession ? possession_offset : 0), possession_y, tm2_sz.w + 10, 26);
    graphics_draw_text(ctx, game->team2.name, font_team, tm2_bnds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    if (team_2_possession) {
        graphics_fill_circle(ctx, GPoint(tm2_bnds.origin.x + tm2_sz.w + 6 - 2, tm2_bnds.origin.y + tm2_bnds.size.h / 2), 2);
    }
}

Layer *score_layer_create(GRect bounds, Game *game) {
    Layer *score; 
    bounds.size.h = 74;

    #ifdef PBL_ROUND
        bounds.size.w -= 32;
        bounds.origin.x += 16;
    #endif

    score = layer_create_with_data(bounds, sizeof(Game*));
    Game **layer_data = (Game **)layer_get_data(score);
    *layer_data = game;
    layer_set_update_proc(score, score_update_proc);

    return score;
}