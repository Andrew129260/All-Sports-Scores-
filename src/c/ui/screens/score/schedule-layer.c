#include "pebble.h"
#include "schedule-layer.h"

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
static const GPoint s_star_points[] = {
    {0, -8}, {2, -2}, {8, -2}, {3, 2}, {5, 8},
    {0, 4}, {-5, 8}, {-3, 2}, {-8, -2}, {-2, -2}
};

static const GPathInfo s_star_path_info = {
    .num_points = 10,
    .points = (GPoint *)s_star_points
};

static void draw_star(GContext *ctx, GPoint center, bool filled) {
    GPath *star_path = gpath_create(&s_star_path_info);
    if (!star_path) {
        return;
    }

    gpath_move_to(star_path, center);

    if (filled) {
        graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorBlack));
        gpath_draw_filled(ctx, star_path);
    }
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 1);
    gpath_draw_outline(ctx, star_path);
    gpath_destroy(star_path);
}
#endif

static void schedule_update_proc(Layer *layer, GContext *ctx) {
    Game **game_ptr = (Game **)layer_get_data(layer);
    if (!game_ptr || !*game_ptr) return;

    Game *game = *game_ptr;

    GRect layer_bounds = layer_get_bounds(layer);
    int half_w = layer_bounds.size.w / 2;

    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_context_set_text_color(ctx, GColorBlack);

    // VISUAL FIX: Shorten center line to perfectly match the live score layout
    int sep_h = layer_bounds.size.h;
    #if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
        sep_h = 100;
    #endif
    GRect separator_bounds = GRect(half_w - 1, 0, 2, sep_h);
    graphics_fill_rect(ctx, separator_bounds, 0, GCornerNone);

    int team_y = -4;
    int rec_y = 26;

    GFont font_team = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    GFont font_rec = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

    #if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
        font_team = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
        font_rec = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

        #if defined(PBL_PLATFORM_GABBRO)
            draw_star(ctx, GPoint(half_w / 2 + 10, 12), game->team1.favorite);
            draw_star(ctx, GPoint(half_w + (half_w / 2) - 10, 12), game->team2.favorite);
        #else
            draw_star(ctx, GPoint(half_w / 2, 12), game->team1.favorite);
            draw_star(ctx, GPoint(half_w + (half_w / 2), 12), game->team2.favorite);
        #endif

        team_y = 16;
        rec_y = 48;
    #endif

    int side_pad = 0;
    #if defined(PBL_PLATFORM_GABBRO)
        side_pad = 20;
    #endif

    const char *t1_name = game->team1.name ? game->team1.name : "";
    const char *t2_name = game->team2.name ? game->team2.name : "";
    const char *t1_rec = game->team1.record ? game->team1.record : "";
    const char *t2_rec = game->team2.record ? game->team2.record : "";

    graphics_draw_text(ctx, t1_name, font_team, GRect(side_pad, team_y, half_w - side_pad - 2, 32), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, t2_name, font_team, GRect(half_w + 2, team_y, half_w - side_pad - 2, 32), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_draw_text(ctx, t1_rec, font_rec, GRect(side_pad, rec_y, half_w - side_pad - 2, 28), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, t2_rec, font_rec, GRect(half_w + 2, rec_y, half_w - side_pad - 2, 28), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

Layer *schedule_layer_create(GRect bounds, Game *game) {
    if (!game) return NULL;

    // LAYER HEIGHT FIX: Ensure the layer is tall enough on Emery/Gabbro!
    #if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
        bounds.size.h = 130;
    #else
        bounds.size.h = 56;
    #endif

    #ifdef PBL_ROUND
        bounds.size.w -= 32;
        bounds.origin.x += 16;
    #endif

    Layer *schedule = layer_create_with_data(bounds, sizeof(Game *));
    if (!schedule) return NULL;

    Game **layer_data = (Game **)layer_get_data(schedule);
    *layer_data = game;
    layer_set_update_proc(schedule, schedule_update_proc);

    return schedule;
}