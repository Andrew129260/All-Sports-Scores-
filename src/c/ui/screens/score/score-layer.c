#include "pebble.h"
#include "score-layer.h"
#include "../../../data/comms/prefs/prefs-handler.h"

static void score_update_proc(Layer *layer, GContext *ctx) {
    Game *game = *(Game **)layer_get_data(layer);
    
    if (game == NULL) {
        return;
    }

    const char *t1_score = game->team1.score ? game->team1.score : "";
    const char *t2_score = game->team2.score ? game->team2.score : "";
    const char *t1_name = game->team1.name ? game->team1.name : "";
    const char *t2_name = game->team2.name ? game->team2.name : "";
    const char *t1_rec = game->team1.record ? game->team1.record : "";
    const char *t2_rec = game->team2.record ? game->team2.record : "";
    const char *time_str = game->time ? game->time : "";

    // 3-TIER LENGTH CHECK
    bool has_very_long_score = strlen(t1_score) > 3 || strlen(t2_score) > 3; 
    bool has_long_score = strlen(t1_score) > 2 || strlen(t2_score) > 2;

    GRect layer_bounds = layer_get_bounds(layer);

    GFont font_score;
    int score_y;
    int record_score_y;
    int score_height;

    #if PBL_DISPLAY_WIDTH > 144
        // Time 2 (Emery) - 200px Wide Displays
        if (has_very_long_score) {
            font_score = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
            score_y = 10;
            record_score_y = 17;
            score_height = 28;
        } else if (has_long_score) {
            font_score = fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
            score_y = 4;
            record_score_y = 11;
            score_height = 36;
        } else {
            font_score = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
            score_y = 0;
            record_score_y = 7;
            score_height = 42;
        }
    #else
        // Pebble Classic, Time, Time Steel - 144px Wide Displays
        if (has_very_long_score || has_long_score) {
            font_score = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
            score_y = 8;
            record_score_y = 15;
            score_height = 30;
        } else {
            font_score = fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
            score_y = 4;
            record_score_y = 11;
            score_height = 32;
        }
    #endif

    bool is_tennis = (game->sport == SportTennis);
    bool has_winner = game->team1.winner || game->team2.winner;

    GFont font_team;
    GFont font_winner = fonts_get_system_font(FONT_KEY_GOTHIC_14);

    if (is_tennis && has_winner) {
        font_team = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    } else {
        font_team = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    }

    GFont font_record = fonts_get_system_font(FONT_KEY_GOTHIC_14);

    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_context_set_text_color(ctx, GColorBlack);
    
    // --- CENTER DIVIDER MATH ---
    GRect separator_bounds = GRect(layer_bounds.size.w / 2 - 1, 12, 2, 48);
    int half_w = separator_bounds.origin.x; // Strict left half width
    int right_x = separator_bounds.origin.x + separator_bounds.size.w; // Strict right half origin
    
    bool record_showing = clay_settings.show_record == ShowRecordAlways || (clay_settings.show_record == ShowRecordFinalOnly && strcmp(time_str, "Final") == 0);
    
    // --- DRAW RECORDS ---
    if (record_showing) {
        GRect rec1_bnds = GRect(0, 0, half_w, 14);
        graphics_draw_text(ctx, t1_rec, font_record, rec1_bnds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

        GRect rec2_bnds = GRect(right_x, 0, half_w, 14);
        graphics_draw_text(ctx, t2_rec, font_record, rec2_bnds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }

    // --- DRAW SCORES ---
    int final_score_y = record_showing ? record_score_y : score_y;
    
    // Lock bounds strictly to their screen half and let SDK auto-center
    GRect sc1_bnds = GRect(0, final_score_y, half_w, score_height);
    graphics_draw_text(ctx, t1_score, font_score, sc1_bnds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    GRect sc2_bnds = GRect(right_x, final_score_y, half_w, score_height);
    graphics_draw_text(ctx, t2_score, font_score, sc2_bnds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // --- DRAW TEAM NAMES & POSSESSION ---
    int possession_y = record_showing ? 50 : 43;
    bool team_1_possession = (game->possession) == Team1;
    bool team_2_possession = (game->possession) == Team2;

    GRect tm1_bnds = GRect(0, possession_y, half_w, 26);
    graphics_draw_text(ctx, t1_name, font_team, tm1_bnds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    
    if (is_tennis && game->team1.winner) {
        GRect win1_bnds = GRect(0, possession_y + 14, half_w, 14);
        graphics_draw_text(ctx, "Winner", font_winner, win1_bnds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }

    if (team_1_possession && !(is_tennis && game->team1.winner)) {
        // Measure text just to position the dot, but cap it so it never crosses the center divider
        GSize tm1_sz = graphics_text_layout_get_content_size(t1_name, font_team, tm1_bnds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
        int dot_x = (half_w / 2) + (tm1_sz.w / 2) + 6;
        if (dot_x > half_w - 3) dot_x = half_w - 3; 
        graphics_fill_circle(ctx, GPoint(dot_x, possession_y + 13), 2);
    }

    GRect tm2_bnds = GRect(right_x, possession_y, half_w, 26);
    graphics_draw_text(ctx, t2_name, font_team, tm2_bnds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    
    if (is_tennis && game->team2.winner) {
        GRect win2_bnds = GRect(right_x, possession_y + 14, half_w, 14);
        graphics_draw_text(ctx, "Winner", font_winner, win2_bnds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }

    if (team_2_possession && !(is_tennis && game->team2.winner)) {
        GSize tm2_sz = graphics_text_layout_get_content_size(t2_name, font_team, tm2_bnds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
        int dot_x = right_x + (half_w / 2) + (tm2_sz.w / 2) + 6;
        if (dot_x > right_x + half_w - 3) dot_x = right_x + half_w - 3;
        graphics_fill_circle(ctx, GPoint(dot_x, possession_y + 13), 2);
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
