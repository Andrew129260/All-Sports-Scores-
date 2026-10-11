#include "pebble.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../../data/model/models.h"
#include "../../../data/comms/comms.h"
#include "../../../data/comms/favorites/favorites-handler.h"
#include "../../../data/comms/games/games-handler.h"

#include "score-screen.h"
#include "score-layer.h"
#include "schedule-layer.h"

#include "../../layers/header/header.h"
#include "../action/action-menu.h"
#include "../action/result-window.h"
#include "../../../utils/utils.h"
#include "../action/action-indicator-layer.h"
#include "../../../image-cache.h"
#include "src/c/breakdown-window.h"

// =========================================================
// GLOBAL SCREEN STATE
// =========================================================

static Window *scoreWindow = NULL;
static StatusBarLayer *s_status_bar = NULL;

static Layer *s_action_indicator = NULL;
static Layer *s_header = NULL;
static Layer *s_score = NULL;

static TextLayer *s_time = NULL;
static TextLayer *s_details = NULL;
static TextLayer *s_broadcast = NULL;
static TextLayer *s_footer_label = NULL;
static TextLayer *s_footer_line = NULL;

static HeaderData s_header_data;

Game *g_game = NULL;

static bool s_ui_built = false;

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)

static char s_date_buffer[32];
static char s_broad_buffer[32];

static Window *s_pending_result_window = NULL;
static AppTimer *s_result_push_timer = NULL;

static int16_t s_touch_start_x = 0;
static int16_t s_touch_start_y = 0;

static bool s_touch_subscribed = false;

#if defined(PBL_ROUND)
static char s_round_footer_buffer[64];

static void append_round_footer(
    const char *text,
    bool newline
) {
    if (!text || !text[0]) return;

    size_t used = strlen(s_round_footer_buffer);

    if (used >= sizeof(s_round_footer_buffer) - 1) {
        return;
    }

    snprintf(
        s_round_footer_buffer + used,
        sizeof(s_round_footer_buffer) - used,
        "%s%s",
        newline ? "\n" : "",
        text
    );
}
#endif

#endif

// =========================================================
// FORWARD DECLARATIONS
// =========================================================

static void build_ui(Window *window);
static void tear_down_ui(void);

// =========================================================
// FAVORITES CALLBACK
// =========================================================

static void silent_favorite_change_callback(
    int team_id,
    FavoriteChangeResult result
) {
    APP_LOG(
        APP_LOG_LEVEL_DEBUG,
        "Favorite sync result = %d",
        (int)result
    );
}

// =========================================================
// REFRESH CALLBACK
// =========================================================

static void refresh_callback(GameUpdateResult state) {
    if (state != GameUpdated) {
        APP_LOG(APP_LOG_LEVEL_WARNING,
                "Game refresh failed");

        if (scoreWindow &&
            window_stack_get_top_window() == scoreWindow) {
            vibes_double_pulse();
        }

        return;
    }

    APP_LOG(APP_LOG_LEVEL_INFO,
            "Game refreshed successfully");

    if (!scoreWindow || !g_game) return;

    // Destroy old TextLayers before another draw can use
    // string pointers replaced by games-handler.c.
    tear_down_ui();

    // Rebuild both the text and the appropriate game layer.
    // This also handles scheduled -> live transitions.
    build_ui(scoreWindow);

    if (s_score) {
        layer_mark_dirty(s_score);
    }

    if (window_stack_get_top_window() == scoreWindow) {
        vibes_short_pulse();
    }
}

// =========================================================
// BUILD UI
// =========================================================

static void build_ui(Window *window) {
    if (s_ui_built || !window || !g_game) {
        return;
    }

    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_frame(window_layer);

    const char *time_str =
        g_game->time ? g_game->time : "";

    const char *details_str =
        g_game->details ? g_game->details : "";

    const char *t1_score =
        g_game->team1.score ? g_game->team1.score : "";

    bool is_scheduled = (t1_score[0] == '\0');

    // -----------------------------------------------------
    // STATUS BAR AND HEADER
    // -----------------------------------------------------

    s_status_bar = status_bar_layer_create();

    if (s_status_bar) {
        status_bar_layer_set_colors(
            s_status_bar,
            GColorDukeBlue,
            GColorWhite
        );
    }

#if defined(PBL_PLATFORM_APLITE)
    s_header_data.icon = NULL;
#else
    s_header_data.icon =
        image_cache_get_sport_icon(g_game->sport);
#endif

    s_header_data.title =
        sport_get_name(g_game->sport);

    s_header_data.info = time_str;
    s_header_data.under_status_bar = true;

    s_header = create_header_layer(bounds, s_header_data);

    if (s_header) {
        layer_add_child(window_layer, s_header);
    }

    if (s_status_bar) {
        layer_add_child(
            window_layer,
            status_bar_layer_get_layer(s_status_bar)
        );
    }

    int header_height = s_header ?
        layer_get_bounds(s_header).size.h : 0;

    // -----------------------------------------------------
    // EMERY AND GABBRO
    // -----------------------------------------------------

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)

    GRect full_bounds = layer_get_frame(window_layer);

    const char *broadcast_str =
        (g_game->broadcast &&
         g_game->broadcast[0] &&
         strcmp(g_game->broadcast, "-") != 0)
            ? g_game->broadcast
            : NULL;

#if defined(PBL_ROUND)

    // GABBRO ROUND FOOTER

    int line_y = full_bounds.size.h - 90;

    s_footer_line = text_layer_create(
        GRect(40, line_y, full_bounds.size.w - 80, 2)
    );

    if (s_footer_line) {
        text_layer_set_background_color(
            s_footer_line, GColorDarkGray
        );

        layer_add_child(
            window_layer,
            text_layer_get_layer(s_footer_line)
        );
    }

    s_footer_label = text_layer_create(
        GRect(0, line_y + 4, full_bounds.size.w, 14)
    );

    if (s_footer_label) {
        text_layer_set_font(
            s_footer_label,
            fonts_get_system_font(FONT_KEY_GOTHIC_14)
        );

        text_layer_set_text_alignment(
            s_footer_label,
            GTextAlignmentCenter
        );

        text_layer_set_text(
            s_footer_label,
            "GAME INFO"
        );

        text_layer_set_text_color(
            s_footer_label,
            GColorDarkGray
        );

        text_layer_set_background_color(
            s_footer_label,
            GColorClear
        );

        layer_add_child(
            window_layer,
            text_layer_get_layer(s_footer_label)
        );
    }

    s_round_footer_buffer[0] = '\0';

    int line_count = 0;

    if (details_str[0]) {
        append_round_footer(details_str, false);
        line_count++;
    }

    if (time_str[0] &&
        strcmp(time_str, details_str) != 0) {

        append_round_footer(
            time_str,
            line_count > 0
        );

        line_count++;
    }

    if (broadcast_str) {
        append_round_footer(
            broadcast_str,
            line_count > 0
        );

        line_count++;
    }

    int text_start_y = line_y + 16;

    if (line_count == 2) {
        text_start_y += 8;
    } else if (line_count == 1) {
        text_start_y += 18;
    }

    s_time = text_layer_create(
        GRect(
            20,
            text_start_y,
            full_bounds.size.w - 40,
            68
        )
    );

    if (s_time) {
        text_layer_set_font(
            s_time,
            fonts_get_system_font(
                FONT_KEY_GOTHIC_18_BOLD
            )
        );

        text_layer_set_text_alignment(
            s_time,
            GTextAlignmentCenter
        );

        text_layer_set_text(
            s_time,
            s_round_footer_buffer
        );

        text_layer_set_background_color(
            s_time,
            GColorClear
        );

        layer_add_child(
            window_layer,
            text_layer_get_layer(s_time)
        );
    }

    bounds.size.h -= header_height + 98;

#else

    // EMERY RECTANGULAR FOOTER

    snprintf(
        s_date_buffer,
        sizeof(s_date_buffer),
        "%s",
        details_str
    );

    snprintf(
        s_broad_buffer,
        sizeof(s_broad_buffer),
        "%s",
        broadcast_str ? broadcast_str : ""
    );

    bool has_newline_broad = false;

    for (int i = 0; s_broad_buffer[i]; i++) {
        if (s_broad_buffer[i] == ' ') {
            s_broad_buffer[i] = '\n';
            has_newline_broad = true;
            break;
        }
    }

    int line_y = full_bounds.size.h - 60;

    int side_w = 68;
    int margin = 2;

    int center_w =
        full_bounds.size.w -
        (side_w * 2) -
        (margin * 2);

    s_footer_line = text_layer_create(
        GRect(0, line_y, full_bounds.size.w, 2)
    );

    if (s_footer_line) {
        text_layer_set_background_color(
            s_footer_line,
            GColorDarkGray
        );

        layer_add_child(
            window_layer,
            text_layer_get_layer(s_footer_line)
        );
    }

    s_footer_label = text_layer_create(
        GRect(0, line_y + 4, full_bounds.size.w, 14)
    );

    if (s_footer_label) {
        text_layer_set_font(
            s_footer_label,
            fonts_get_system_font(FONT_KEY_GOTHIC_14)
        );

        text_layer_set_text_alignment(
            s_footer_label,
            GTextAlignmentCenter
        );

        text_layer_set_text(
            s_footer_label,
            "GAME INFO"
        );

        text_layer_set_text_color(
            s_footer_label,
            GColorDarkGray
        );

        text_layer_set_background_color(
            s_footer_label,
            GColorClear
        );

        layer_add_child(
            window_layer,
            text_layer_get_layer(s_footer_label)
        );
    }

    // Center: game time

    s_time = text_layer_create(
        GRect(
            margin + side_w,
            line_y + 16,
            center_w,
            48
        )
    );

    if (s_time) {
        GFont t_font =
            strlen(time_str) >= 9
                ? fonts_get_system_font(
                    FONT_KEY_GOTHIC_14_BOLD)
                : fonts_get_system_font(
                    FONT_KEY_GOTHIC_18_BOLD);

        text_layer_set_font(s_time, t_font);

        text_layer_set_text_alignment(
            s_time,
            GTextAlignmentCenter
        );

        text_layer_set_overflow_mode(
            s_time,
            GTextOverflowModeTrailingEllipsis
        );

        text_layer_set_text(s_time, time_str);

        text_layer_set_background_color(
            s_time,
            GColorClear
        );

        layer_add_child(
            window_layer,
            text_layer_get_layer(s_time)
        );
    }

    // Left: date/details

    GFont d_font =
        strlen(s_date_buffer) >= 9
            ? fonts_get_system_font(
                FONT_KEY_GOTHIC_14_BOLD)
            : fonts_get_system_font(
                FONT_KEY_GOTHIC_18_BOLD);

    s_details = text_layer_create(
        GRect(2, line_y + 12, side_w - 2, 48)
    );

    if (s_details) {
        text_layer_set_font(s_details, d_font);

        text_layer_set_text_alignment(
            s_details,
            GTextAlignmentLeft
        );

        text_layer_set_overflow_mode(
            s_details,
            GTextOverflowModeWordWrap
        );

        text_layer_set_text(
            s_details,
            s_date_buffer
        );

        text_layer_set_background_color(
            s_details,
            GColorClear
        );

        layer_add_child(
            window_layer,
            text_layer_get_layer(s_details)
        );
    }

    // Right: broadcast

    GFont b_font =
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

    if (!has_newline_broad &&
        strlen(s_broad_buffer) >= 10) {
        b_font =
            fonts_get_system_font(FONT_KEY_GOTHIC_14);
    } else if (!has_newline_broad &&
               strlen(s_broad_buffer) >= 8) {
        b_font =
            fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    }

    s_broadcast = text_layer_create(
        GRect(
            full_bounds.size.w - side_w,
            line_y + 12,
            side_w - 2,
            48
        )
    );

    if (s_broadcast) {
        text_layer_set_font(s_broadcast, b_font);

        text_layer_set_text_alignment(
            s_broadcast,
            GTextAlignmentRight
        );

        text_layer_set_overflow_mode(
            s_broadcast,
            has_newline_broad
                ? GTextOverflowModeWordWrap
                : GTextOverflowModeTrailingEllipsis
        );

        text_layer_set_text(
            s_broadcast,
            s_broad_buffer
        );

        text_layer_set_background_color(
            s_broadcast,
            GColorClear
        );

        layer_add_child(
            window_layer,
            text_layer_get_layer(s_broadcast)
        );
    }

    bounds.size.h -= header_height + 74;

#endif

    bounds.origin.y += header_height;

    // Critical: reevaluate scheduled/live every rebuild.
    s_score = is_scheduled
        ? schedule_layer_create(bounds, g_game)
        : score_layer_create(bounds, g_game);

    if (s_score) {
        layer_add_child(window_layer, s_score);
    }

#else

    // -----------------------------------------------------
    // LEGACY PEBBLES
    // -----------------------------------------------------

    bounds.origin.y +=
        header_height + (is_scheduled ? 12 : 4);

    bounds.size.h -=
        header_height + (is_scheduled ? 12 : 4);

    s_score = is_scheduled
        ? schedule_layer_create(bounds, g_game)
        : score_layer_create(bounds, g_game);

    if (s_score) {
        layer_add_child(window_layer, s_score);

        bounds.origin.y +=
            layer_get_bounds(s_score).size.h + 4;

        bounds.size.h -=
            layer_get_bounds(s_score).size.h + 4;
    }

    if (details_str[0]) {
        GRect details_bounds = bounds;
        details_bounds.size.h = 18;

        s_details = text_layer_create(details_bounds);

        if (s_details) {
            text_layer_set_font(
                s_details,
                fonts_get_system_font(FONT_KEY_GOTHIC_14)
            );

            text_layer_set_text_alignment(
                s_details,
                GTextAlignmentCenter
            );

            text_layer_set_text(
                s_details,
                details_str
            );

            layer_add_child(
                window_layer,
                text_layer_get_layer(s_details)
            );
        }

        bounds.origin.y += 14;
    }

    s_time = text_layer_create(
        GRect(
            bounds.origin.x,
            bounds.origin.y,
            bounds.size.w,
            24
        )
    );

    if (s_time) {
        text_layer_set_font(
            s_time,
            fonts_get_system_font(
                FONT_KEY_GOTHIC_18_BOLD
            )
        );

        text_layer_set_text_alignment(
            s_time,
            GTextAlignmentCenter
        );

        text_layer_set_text(s_time, time_str);

        layer_add_child(
            window_layer,
            text_layer_get_layer(s_time)
        );
    }

    s_action_indicator = action_indicator_layer_create();

    if (s_action_indicator) {
        layer_add_child(
            window_layer,
            s_action_indicator
        );
    }

#endif

    s_ui_built = true;
}

// =========================================================
// UI CLEANUP
// =========================================================

static void tear_down_ui(void) {
    if (!s_ui_built) return;

    if (s_status_bar) {
        status_bar_layer_destroy(s_status_bar);
        s_status_bar = NULL;
    }

    if (s_action_indicator) {
        layer_destroy(s_action_indicator);
        s_action_indicator = NULL;
    }

    if (s_header) {
        layer_destroy(s_header);
        s_header = NULL;
    }

    if (s_details) {
        text_layer_destroy(s_details);
        s_details = NULL;
    }

    if (s_time) {
        text_layer_destroy(s_time);
        s_time = NULL;
    }

    if (s_broadcast) {
        text_layer_destroy(s_broadcast);
        s_broadcast = NULL;
    }

    if (s_footer_label) {
        text_layer_destroy(s_footer_label);
        s_footer_label = NULL;
    }

    if (s_footer_line) {
        text_layer_destroy(s_footer_line);
        s_footer_line = NULL;
    }

    if (s_score) {
        layer_destroy(s_score);
        s_score = NULL;
    }

    s_ui_built = false;
}

// =========================================================
// BREAKDOWN SUPPORT — EMERY / GABBRO
// =========================================================

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)

static void on_breakdown_loaded(
    const char *breakdown_text
) {
    if (!g_game || !scoreWindow) return;

    // Don't open an unexpected window after navigation.
    if (window_stack_get_top_window() != scoreWindow) {
        return;
    }

    show_breakdown_window(breakdown_text, g_game);
}

static void on_breakdown_error(AppError error) {
    APP_LOG(
        APP_LOG_LEVEL_ERROR,
        "Breakdown failed: %d",
        (int)error
    );

    if (scoreWindow &&
        window_stack_get_top_window() == scoreWindow) {
        vibes_double_pulse();
    }
}

static void trigger_breakdown(void) {
    if (!g_game || !scoreWindow) return;

    vibes_short_pulse();

    const char *score =
        g_game->team1.score ? g_game->team1.score : "";

    const char *time =
        g_game->time ? g_game->time : "";

    const char *details =
        g_game->details ? g_game->details : "";

    /*
     * Do not assume that an empty numeric score means the
     * event is scheduled.
     *
     * MMA fights can be Final without having a traditional
     * team score, and other ESPN sports could behave similarly.
     */
    const bool is_final =
        strcmp(
            time,
            "Final"
        ) == 0;

    const bool is_live =
        strcmp(
            details,
            "LIVE"
        ) == 0;

    const bool is_scheduled =
        !score[0] &&
        !is_final &&
        !is_live;


    /*
     * Truly scheduled games can use the information already
     * available on the watch. This avoids an unnecessary
     * ESPN request just to show team records.
     */
    if (is_scheduled) {
        static char s_pregame_buffer[128];

        snprintf(
            s_pregame_buffer,
            sizeof(s_pregame_buffer),
            "Match Scheduled\n%s\n\n"
            "Team Records:\n%s: %s\n%s: %s",

            time[0] ?
                time : "TBA",

            g_game->team1.name ?
                g_game->team1.name : "TBD",

            (g_game->team1.record &&
             g_game->team1.record[0])
                ? g_game->team1.record : "N/A",

            g_game->team2.name ?
                g_game->team2.name : "TBD",

            (g_game->team2.record &&
             g_game->team2.record[0])
                ? g_game->team2.record : "N/A"
        );

        show_breakdown_window(
            s_pregame_buffer,
            g_game
        );

        return;
    }


    /*
     * Live or completed events should request the real ESPN
     * breakdown even when there is no traditional numeric score.
     *
     * This fixes MMA Final events in particular.
     */
    handle_request_breakdown(
        g_game,
        on_breakdown_loaded,
        on_breakdown_error
    );
}

// Select: breakdown
static void select_click_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
    trigger_breakdown();
}

// Long Select: refresh
static void select_long_click_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
    if (!g_game) return;

    update_game(g_game, refresh_callback);
}

#else

// =========================================================
// LEGACY ACTION MENU
// =========================================================

static void deferred_action_menu(void *data) {
    if (!g_game) return;

#if defined(PBL_PLATFORM_APLITE)
    tear_down_ui();
#endif

    game_action_menu_open(g_game);
}

static void select_click_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
    app_timer_register(
        50,
        deferred_action_menu,
        NULL
    );
}

#endif

// =========================================================
// BUTTON CONFIGURATION
// =========================================================

static void click_config_provider(void *context) {
    window_single_click_subscribe(
        BUTTON_ID_SELECT,
        select_click_handler
    );

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    window_long_click_subscribe(
        BUTTON_ID_SELECT,
        500,
        select_long_click_handler,
        NULL
    );
#endif
}

// =========================================================
// TOUCH SUPPORT
// =========================================================

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)

static void push_result_window_callback(void *data) {
    s_result_push_timer = NULL;

    Window *pending = s_pending_result_window;
    s_pending_result_window = NULL;

    if (!pending) return;

    // Don't push a result after the score screen was closed.
    if (!scoreWindow ||
        window_stack_get_top_window() != scoreWindow) {

        window_destroy(pending);
        return;
    }

    window_stack_push(pending, true);
}

static void open_favorite_result(
    int team_index
) {
    if (!g_game || !s_score ||
        s_pending_result_window) {
        return;
    }

    bool is_team1 = (team_index == 1);

    bool *favorite = is_team1
        ? &g_game->team1.favorite
        : &g_game->team2.favorite;

    int team_id = is_team1
        ? g_game->team1.id
        : g_game->team2.id;

    *favorite = !*favorite;

    handle_request_change_favorite(
        g_game,
        team_id,
        silent_favorite_change_callback
    );

    s_pending_result_window =
        result_window_create_favorite(
            g_game,
            is_team1 ? ACTION_TEAM_1 : ACTION_TEAM_2,
            *favorite ? 1 : 0
        );

    layer_mark_dirty(s_score);
    vibes_short_pulse();

    if (s_pending_result_window) {
        s_result_push_timer = app_timer_register(
            50,
            push_result_window_callback,
            NULL
        );

        if (!s_result_push_timer) {
            Window *pending = s_pending_result_window;
            s_pending_result_window = NULL;
            window_destroy(pending);
        }
    }
}

static void score_touch_handler(
    const TouchEvent *event,
    void *context
) {
    if (!event || !scoreWindow || !g_game) {
        return;
    }

    if (event->type == TouchEvent_Touchdown) {
        s_touch_start_x = event->x;
        s_touch_start_y = event->y;
        return;
    }

    if (event->type != TouchEvent_Liftoff) {
        return;
    }

    int16_t dx = event->x - s_touch_start_x;
    int16_t dy = event->y - s_touch_start_y;

    if (dx > 40 && abs(dx) > abs(dy)) {
        window_stack_pop(true);
        return;
    }

    // Only short taps, not drags.
    if (abs(dx) >= 15 || abs(dy) >= 15) {
        return;
    }

    Layer *root = window_get_root_layer(scoreWindow);
    GRect bounds = layer_get_bounds(root);

    int screen_h = bounds.size.h;

    int footer_y =
        screen_h - PBL_IF_ROUND_ELSE(90, 60);

    if (event->y > footer_y) {
        trigger_breakdown();
        return;
    }

    if (!s_score) return;

    // Use actual score-layer coordinates for star hitboxes.
    GRect frame = layer_get_frame(s_score);

    int half_w = frame.size.w / 2;

    int star1_x =
        frame.origin.x + half_w / 2;

    int star2_x =
        frame.origin.x + half_w + half_w / 2;

#if defined(PBL_PLATFORM_GABBRO)
    star1_x += 10;
    star2_x -= 10;
#endif

    int star_y = frame.origin.y + 12;

    if (abs(event->y - star_y) <= 20) {
        if (abs(event->x - star1_x) <= 25) {
            open_favorite_result(1);
        } else if (abs(event->x - star2_x) <= 25) {
            open_favorite_result(2);
        }
    }
}

#endif

// =========================================================
// WINDOW LIFECYCLE
// =========================================================

static void window_load(Window *window) {
    build_ui(window);
}

static void window_unload(Window *window) {
    handle_cancel_breakdown_request();

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    if (s_result_push_timer) {
        app_timer_cancel(s_result_push_timer);
        s_result_push_timer = NULL;
    }

    if (s_pending_result_window) {
        window_destroy(s_pending_result_window);
        s_pending_result_window = NULL;
    }
#endif

    tear_down_ui();
}

static void window_disappear(Window *window) {
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)

    // A late ESPN response must not open a different game.
    handle_cancel_breakdown_request();

    if (s_touch_subscribed) {
        touch_service_unsubscribe();
        s_touch_subscribed = false;
    }

    app_touch_navigation_enable(true);

#endif
}

static void window_appear(Window *window) {
#if defined(PBL_PLATFORM_APLITE)
    build_ui(window);
#endif

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    if (touch_service_is_enabled() &&
        !s_touch_subscribed) {

        app_touch_navigation_enable(false);
        touch_service_subscribe(
            score_touch_handler,
            NULL
        );

        s_touch_subscribed = true;
    }
#endif
}

// =========================================================
// PUBLIC FUNCTIONS
// =========================================================

void show_score_screen(Game *game) {
    if (!game) return;

    handle_cancel_breakdown_request();

    bool already_loaded =
        scoreWindow && s_ui_built;

    if (already_loaded) {
        tear_down_ui();
    }

    g_game = game;

    if (!scoreWindow) {
        scoreWindow = window_create();

        if (!scoreWindow) {
            APP_LOG(
                APP_LOG_LEVEL_ERROR,
                "Failed to create score window"
            );
            return;
        }

        WindowHandlers handlers = {0};

        handlers.load = window_load;
        handlers.unload = window_unload;
        handlers.appear = window_appear;
        handlers.disappear = window_disappear;

        window_set_window_handlers(
            scoreWindow,
            handlers
        );

        window_set_click_config_provider(
            scoreWindow,
            click_config_provider
        );
    }

    if (already_loaded) {
        build_ui(scoreWindow);
    }

    if (!window_stack_contains_window(scoreWindow)) {
        window_stack_push(scoreWindow, true);
    }
}

void hide_score_screen(void) {
    if (!scoreWindow) return;

    handle_cancel_breakdown_request();

    window_stack_remove(scoreWindow, true);
}