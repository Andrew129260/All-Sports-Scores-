#include "pebble.h"
#include <stdio.h>
#include <string.h>

#include "breakdown-window.h"
#include "ui/layers/header/header.h"
#include "data/model/models.h"

static Window *s_window = NULL;

static TextLayer *s_text_layer = NULL;
static Layer *s_header_layer = NULL;

static char s_breakdown_buffer[512];
static char s_header_info_buffer[32];
static char s_title_buffer[32];

static HeaderData s_header_data;
static GBitmap *s_sport_icon = NULL;

// =========================================================
// SPORT ICONS
// =========================================================

static uint32_t get_sport_icon(Sport sport) {
    switch (sport) {
        case SportNFL:
            return RESOURCE_ID_FOOTBALL_16;

        case SportMLB:
            return RESOURCE_ID_BASEBALL_16;

        case SportNHL:
            return RESOURCE_ID_HOCKEY_16;

        case SportNBA:
            return RESOURCE_ID_BASKETBALL_16;

        case SportMLS:
            return RESOURCE_ID_SOCCER_16;

        case SportRugby:
            return RESOURCE_ID_Rugby_16;

        case SportCricket:
            return RESOURCE_ID_Cricket_16;

        case SportTennis:
            return RESOURCE_ID_IMAGE_SPORT_TENNIS16;

        case SportAFL:
            return RESOURCE_ID_IMAGE_SPORT_AFL16;

        case SportMMA:
            return RESOURCE_ID_IMAGE_SPORT_MMA16;

        default:
            return RESOURCE_ID_STAR_16;
    }
}

// =========================================================
// SPORT TITLES
// =========================================================

static const char *get_breakdown_title(Sport sport) {
    switch (sport) {
        case SportNFL: return "NFL BREAKDOWN";
        case SportMLB: return "MLB BREAKDOWN";
        case SportNHL: return "NHL BREAKDOWN";
        case SportNBA: return "NBA BREAKDOWN";
        case SportMLS: return "SOCCER BREAKDOWN";
        case SportRugby: return "RUGBY BREAKDOWN";
        case SportCricket: return "CRICKET BREAKDOWN";
        case SportTennis: return "TENNIS BREAKDOWN";
        case SportAFL: return "AFL BREAKDOWN";
        case SportMMA: return "MMA BREAKDOWN";
        default: return "BREAKDOWN";
    }
}

// =========================================================
// WINDOW LOADING
// =========================================================

static void window_load(Window *window) {
    Layer *window_layer =
        window_get_root_layer(window);

    GRect bounds = layer_get_bounds(window_layer);

    s_header_layer = create_header_layer(
        bounds,
        s_header_data
    );

    int header_height = 0;

    if (s_header_layer) {
        layer_add_child(
            window_layer,
            s_header_layer
        );

        header_height =
            layer_get_bounds(s_header_layer).size.h;
    }

    GRect text_bounds = GRect(
        12,
        header_height + 12,
        bounds.size.w - 24,
        bounds.size.h - header_height - 24
    );

    s_text_layer = text_layer_create(text_bounds);

    if (!s_text_layer) {
        APP_LOG(
            APP_LOG_LEVEL_ERROR,
            "Breakdown TextLayer allocation failed"
        );
        return;
    }

    text_layer_set_font(
        s_text_layer,
        fonts_get_system_font(
            FONT_KEY_GOTHIC_18_BOLD
        )
    );

    text_layer_set_text_alignment(
        s_text_layer,
        GTextAlignmentCenter
    );

    text_layer_set_overflow_mode(
        s_text_layer,
        GTextOverflowModeWordWrap
    );

    text_layer_set_background_color(
        s_text_layer,
        GColorClear
    );

    text_layer_set_text_color(
        s_text_layer,
        GColorBlack
    );

    text_layer_set_text(
        s_text_layer,
        s_breakdown_buffer
    );

    layer_add_child(
        window_layer,
        text_layer_get_layer(s_text_layer)
    );
}

// =========================================================
// WINDOW CLEANUP
// =========================================================

static void window_unload(Window *window) {
    if (s_text_layer) {
        text_layer_destroy(s_text_layer);
        s_text_layer = NULL;
    }

    // Destroy the header before its bitmap.
    if (s_header_layer) {
        layer_destroy(s_header_layer);
        s_header_layer = NULL;
    }

    if (s_sport_icon) {
        gbitmap_destroy(s_sport_icon);
        s_sport_icon = NULL;
    }
}

// =========================================================
// SHOW BREAKDOWN
// =========================================================

void show_breakdown_window(
    const char *breakdown_text,
    Game *game
) {
    if (!game) {
        APP_LOG(
            APP_LOG_LEVEL_WARNING,
            "Breakdown requested without game"
        );
        return;
    }

    const char *text = breakdown_text
        ? breakdown_text
        : "Breakdown unavailable.";

    const char *team1 =
        game->team1.name ? game->team1.name : "TBD";

    const char *team2 =
        game->team2.name ? game->team2.name : "TBD";

    // If an older window exists, fully release it first.
    // This prevents headers retaining destroyed bitmaps.
    if (s_window) {
        window_stack_remove(s_window, false);
        window_destroy(s_window);
        s_window = NULL;
    }

    snprintf(
        s_breakdown_buffer,
        sizeof(s_breakdown_buffer),
        "%s",
        text
    );

    snprintf(
        s_header_info_buffer,
        sizeof(s_header_info_buffer),
        "%s vs %s",
        team1,
        team2
    );

    snprintf(
        s_title_buffer,
        sizeof(s_title_buffer),
        "%s",
        get_breakdown_title(game->sport)
    );

    s_sport_icon = gbitmap_create_with_resource(
        get_sport_icon(game->sport)
    );

    // A missing icon is acceptable; display text anyway.
    if (!s_sport_icon) {
        APP_LOG(
            APP_LOG_LEVEL_WARNING,
            "Breakdown sport icon unavailable"
        );
    }

    s_header_data = (HeaderData) {
        .title = s_title_buffer,
        .info = s_header_info_buffer,
        .under_status_bar = true,
        .icon = s_sport_icon
    };

    s_window = window_create();

    if (!s_window) {
        APP_LOG(
            APP_LOG_LEVEL_ERROR,
            "Breakdown window allocation failed"
        );

        if (s_sport_icon) {
            gbitmap_destroy(s_sport_icon);
            s_sport_icon = NULL;
        }

        return;
    }

    window_set_window_handlers(
        s_window,
        (WindowHandlers) {
            .load = window_load,
            .unload = window_unload
        }
    );

    window_stack_push(s_window, true);
}

// =========================================================
// EXPLICIT CLEANUP
// =========================================================

void hide_breakdown_window(void) {
    if (!s_window) return;

    window_stack_remove(s_window, true);
    window_destroy(s_window);

    s_window = NULL;
}