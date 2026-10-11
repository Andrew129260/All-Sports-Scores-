#include "pebble.h"
#include <string.h>

#include "score-layer.h"
#include "../../../data/comms/prefs/prefs-handler.h"

// =========================================================
// FAVORITE STARS — EMERY / GABBRO
// =========================================================

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)

static const GPoint s_star_points[] = {
    { 0, -8 },
    { 2, -2 },
    { 8, -2 },
    { 3, 2 },
    { 5, 8 },
    { 0, 4 },
    { -5, 8 },
    { -3, 2 },
    { -8, -2 },
    { -2, -2 }
};

static const GPathInfo s_star_path_info = {
    .num_points = 10,
    .points = (GPoint *)s_star_points
};

static void draw_star(
    GContext *ctx,
    GPoint center,
    bool filled
) {
    GPath *star_path = gpath_create(&s_star_path_info);

    // Guard against heap allocation failure.
    if (!star_path) {
        return;
    }

    gpath_move_to(star_path, center);

    if (filled) {
        graphics_context_set_fill_color(
            ctx,
            PBL_IF_COLOR_ELSE(
                GColorChromeYellow,
                GColorBlack
            )
        );

        gpath_draw_filled(ctx, star_path);
    }

    graphics_context_set_stroke_color(
        ctx,
        GColorBlack
    );

    graphics_context_set_stroke_width(ctx, 1);

    gpath_draw_outline(ctx, star_path);

    gpath_destroy(star_path);
}

#endif

// =========================================================
// SCORE FONT VALIDATION
// =========================================================

static bool is_strict_digits(const char *str) {
    if (!str || !str[0]) {
        return false;
    }

    bool has_digit = false;

    for (int i = 0; str[i]; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            has_digit = true;
        } else if (str[i] != ':' && str[i] != '.') {
            return false;
        }
    }

    return has_digit;
}

// =========================================================
// COMPLETED GAME DETECTION
// =========================================================

static bool is_final_status(const char *time_str) {
    if (!time_str) return false;

    if (strcmp(time_str, "Final") == 0 ||
        strcmp(time_str, "FT") == 0) {
        return true;
    }

    if (strncmp(time_str, "Final/", 6) == 0 ||
        strncmp(time_str, "Final ", 6) == 0) {
        return true;
    }

    return false;
}

// =========================================================
// MAIN SCORE RENDERER
// =========================================================

static void score_update_proc(
    Layer *layer,
    GContext *ctx
) {
    Game **game_ptr =
        (Game **)layer_get_data(layer);

    if (!game_ptr || !*game_ptr) {
        return;
    }

    Game *game = *game_ptr;

    const char *t1_score =
        game->team1.score ? game->team1.score : "";

    const char *t2_score =
        game->team2.score ? game->team2.score : "";

    const char *t1_name =
        game->team1.name ? game->team1.name : "";

    const char *t2_name =
        game->team2.name ? game->team2.name : "";

    const char *t1_rec =
        game->team1.record ? game->team1.record : "";

    const char *t2_rec =
        game->team2.record ? game->team2.record : "";

    const char *time_str =
        game->time ? game->time : "";

    bool has_extreme_score =
        strlen(t1_score) > 5 ||
        strlen(t2_score) > 5;

    bool has_very_long_score =
        strlen(t1_score) > 3 ||
        strlen(t2_score) > 3;

    bool has_long_score =
        strlen(t1_score) > 2 ||
        strlen(t2_score) > 2;

    bool record_showing =
        clay_settings.show_record == ShowRecordAlways ||
        (
            clay_settings.show_record == ShowRecordFinalOnly &&
            is_final_status(time_str)
        );

    GRect layer_bounds = layer_get_bounds(layer);
    int half_w = layer_bounds.size.w / 2;

    graphics_context_set_fill_color(
        ctx,
        GColorBlack
    );

    graphics_context_set_text_color(
        ctx,
        GColorBlack
    );

    // -----------------------------------------------------
    // CENTER SEPARATOR
    // -----------------------------------------------------

    int sep_h = layer_bounds.size.h;

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    sep_h = 100;
#endif

    GRect separator_bounds = GRect(
        half_w - 1,
        0,
        2,
        sep_h
    );

    graphics_fill_rect(
        ctx,
        separator_bounds,
        0,
        GCornerNone
    );

    // -----------------------------------------------------
    // LAYOUT
    // -----------------------------------------------------

    int team_y = -4;
    int rec_y = 22;
    int score_y = 28;

    GFont font_team =
        fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

    GFont font_rec =
        fonts_get_system_font(FONT_KEY_GOTHIC_14);

    GFont font_score;

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)

    font_team =
        fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

#if defined(PBL_PLATFORM_GABBRO)

    draw_star(
        ctx,
        GPoint(half_w / 2 + 10, 12),
        game->team1.favorite
    );

    draw_star(
        ctx,
        GPoint(half_w + half_w / 2 - 10, 12),
        game->team2.favorite
    );

#else

    draw_star(
        ctx,
        GPoint(half_w / 2, 12),
        game->team1.favorite
    );

    draw_star(
        ctx,
        GPoint(half_w + half_w / 2, 12),
        game->team2.favorite
    );

#endif

    team_y = 16;
    rec_y = 48;
    score_y = 52;

#endif

    // -----------------------------------------------------
    // SCORE FONT SELECTION
    // -----------------------------------------------------

    bool use_leco =
        is_strict_digits(t1_score) &&
        is_strict_digits(t2_score);

    if (!use_leco) {

        font_score =
            fonts_get_system_font(
                FONT_KEY_GOTHIC_24_BOLD
            );

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
        font_score =
            fonts_get_system_font(
                PBL_IF_ROUND_ELSE(
                    FONT_KEY_GOTHIC_24_BOLD,
                    FONT_KEY_GOTHIC_28_BOLD
                )
            );
#endif

        score_y += 6;

    } else if (has_extreme_score) {

        font_score =
            fonts_get_system_font(
                PBL_IF_ROUND_ELSE(
                    FONT_KEY_GOTHIC_14_BOLD,
                    FONT_KEY_GOTHIC_18_BOLD
                )
            );

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
        font_score =
            fonts_get_system_font(
                FONT_KEY_GOTHIC_24_BOLD
            );
#endif

        score_y += 10;

    } else if (has_very_long_score) {

        font_score =
            fonts_get_system_font(
                FONT_KEY_GOTHIC_24_BOLD
            );

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
        font_score =
            fonts_get_system_font(
                FONT_KEY_GOTHIC_28_BOLD
            );
#endif

        score_y += 6;

    } else if (has_long_score) {

        font_score =
            fonts_get_system_font(
                FONT_KEY_LECO_32_BOLD_NUMBERS
            );

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
        font_score =
            fonts_get_system_font(
                FONT_KEY_LECO_36_BOLD_NUMBERS
            );
#endif

        score_y += 2;

    } else {

        font_score =
            fonts_get_system_font(
                FONT_KEY_LECO_32_BOLD_NUMBERS
            );

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
        font_score =
            fonts_get_system_font(
                FONT_KEY_LECO_42_NUMBERS
            );
#endif

        score_y -= 4;
    }

    // -----------------------------------------------------
    // GABBRO EDGE PADDING
    // -----------------------------------------------------

    int side_pad = 0;

#if defined(PBL_PLATFORM_GABBRO)
    side_pad = 20;
#endif

    // -----------------------------------------------------
    // TEAM RECORDS
    // -----------------------------------------------------

    if (record_showing) {
        score_y += 12;

        graphics_draw_text(
            ctx,
            t1_rec,
            font_rec,
            GRect(
                side_pad,
                rec_y,
                half_w - side_pad - 4,
                18
            ),
            GTextOverflowModeTrailingEllipsis,
            GTextAlignmentCenter,
            NULL
        );

        graphics_draw_text(
            ctx,
            t2_rec,
            font_rec,
            GRect(
                half_w + 4,
                rec_y,
                half_w - side_pad - 4,
                18
            ),
            GTextOverflowModeTrailingEllipsis,
            GTextAlignmentCenter,
            NULL
        );
    }

    // -----------------------------------------------------
    // TEAM NAMES
    // -----------------------------------------------------

    graphics_draw_text(
        ctx,
        t1_name,
        font_team,
        GRect(
            side_pad,
            team_y,
            half_w - side_pad - 2,
            32
        ),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter,
        NULL
    );

    graphics_draw_text(
        ctx,
        t2_name,
        font_team,
        GRect(
            half_w + 2,
            team_y,
            half_w - side_pad - 2,
            32
        ),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter,
        NULL
    );

    // -----------------------------------------------------
    // SCORES
    // -----------------------------------------------------

    graphics_draw_text(
        ctx,
        t1_score,
        font_score,
        GRect(
            side_pad,
            score_y,
            half_w - side_pad - 2,
            48
        ),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter,
        NULL
    );

    graphics_draw_text(
        ctx,
        t2_score,
        font_score,
        GRect(
            half_w + 2,
            score_y,
            half_w - side_pad - 2,
            48
        ),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter,
        NULL
    );

    // -----------------------------------------------------
    // POSSESSION DOTS
    // -----------------------------------------------------

    int dot_y = team_y + 16;

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    dot_y = team_y + 18;
#endif

    if (game->possession == Team1) {
        graphics_fill_circle(
            ctx,
            GPoint(8 + side_pad, dot_y),
            3
        );
    }

    if (game->possession == Team2) {
        graphics_fill_circle(
            ctx,
            GPoint(
                layer_bounds.size.w - 8 - side_pad,
                dot_y
            ),
            3
        );
    }
}

// =========================================================
// LAYER CREATION
// =========================================================

Layer *score_layer_create(
    GRect bounds,
    Game *game
) {
    if (!game) return NULL;

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    bounds.size.h = 130;
#else
    bounds.size.h = 74;
#endif

#ifdef PBL_ROUND
    bounds.size.w -= 32;
    bounds.origin.x += 16;
#endif

    Layer *score = layer_create_with_data(
        bounds,
        sizeof(Game *)
    );

    if (!score) {
        APP_LOG(
            APP_LOG_LEVEL_ERROR,
            "Score layer allocation failed"
        );

        return NULL;
    }

    Game **layer_data =
        (Game **)layer_get_data(score);

    if (!layer_data) {
        layer_destroy(score);
        return NULL;
    }

    *layer_data = game;

    layer_set_update_proc(
        score,
        score_update_proc
    );

    return score;
}