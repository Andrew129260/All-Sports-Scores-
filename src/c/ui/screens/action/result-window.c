#include "pebble.h"
#include "../../../data/model/models.h"
#include "../../../data/comms/favorites/favorites-handler.h"
#include "action-menu.h"

// Milliseconds between frames
#define DELTA 13

// Safely tracked UI elements
typedef struct {
    Layer *content_layer;
    TextLayer *text_layer;
    AppTimer *close_timer;
} ResultWindowData;

typedef struct {
    GDrawCommandSequence *command_seq;
    GDrawCommandImage* command_img;
    int frame;
    char *label;
} ResultLayerData;

static void on_unload(Window *window) {
    ResultWindowData *window_data = (ResultWindowData *)window_get_user_data(window);
    
    if (window_data) {
        // Safely check for content layer before destroying graphics
        if (window_data->content_layer) {
            ResultLayerData *layer_data = (ResultLayerData *)layer_get_data(window_data->content_layer);
            if (layer_data) {
                if (layer_data->command_seq) {
                    gdraw_command_sequence_destroy(layer_data->command_seq);
                }
                if (layer_data->command_img) {
                    gdraw_command_image_destroy(layer_data->command_img);
                }
                if (layer_data->label) {
                    free(layer_data->label);
                }
            }
            layer_destroy(window_data->content_layer);
        }
        
        // Safely destroy text layer used by Broadcast
        if (window_data->text_layer) {
            text_layer_destroy(window_data->text_layer);
        }
        
        // Safely cancel the timer
        if (window_data->close_timer) {
            app_timer_cancel(window_data->close_timer);
        }
        
        free(window_data);
    }
}

static void close_timer_callback(void *context) {
    Window *window = (Window *) context;
    ResultWindowData *window_data = (ResultWindowData *)window_get_user_data(window);
    
    // Clear the timer pointer so on_unload ignores it and doesn't trigger "Timer does not exist"
    if (window_data) {
        window_data->close_timer = NULL;
    }
    
    window_stack_remove(window, true);
}

static void next_frame_handler(void *context) {
    Window *window = (Window *)context;
    ResultWindowData *window_data = (ResultWindowData *)window_get_user_data(window);
    
    // Extra safety checks for Aplite
    if (!window_data || !window_data->content_layer) return;
    
    ResultLayerData *layer_data = (ResultLayerData *)layer_get_data(window_data->content_layer);
    
    if (!layer_data || !layer_data->command_seq) return;
    
    layer_data->frame++;
    layer_mark_dirty(window_data->content_layer);
    
    if (layer_data->frame < (int)gdraw_command_sequence_get_num_frames(layer_data->command_seq)) {
        app_timer_register(DELTA, next_frame_handler, window);
    } else {
        window_data->close_timer = app_timer_register(2000, close_timer_callback, window);
    }
}

static void result_layer_update_proc(Layer *layer, GContext *ctx) {
    ResultLayerData *layer_data = (ResultLayerData *)layer_get_data(layer);
    if (!layer_data) return;
    
    GRect bounds = layer_get_bounds(layer);
    
    graphics_context_set_fill_color(ctx, GColorDukeBlue);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    
    GPoint origin = GPoint(bounds.size.w / 2 - 40, bounds.size.h / 2 - 50);
    
    if (layer_data->command_seq) {
        GDrawCommandFrame *frame = gdraw_command_sequence_get_frame_by_index(layer_data->command_seq, layer_data->frame);
        if (frame) {
            gdraw_command_frame_draw(ctx, layer_data->command_seq, frame, origin);
        }
    } else if (layer_data->command_img) {
        gdraw_command_image_draw(ctx, layer_data->command_img, origin);
    }
    
    graphics_context_set_text_color(ctx, GColorWhite);
    GRect text_bounds = GRect(0, bounds.size.h / 2 + 30, bounds.size.w, 30);
    
    if (layer_data->label) {
        graphics_draw_text(ctx, layer_data->label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), text_bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
}

Window *result_window_create_favorite(Game *game, MenuAction action, FavoriteChangeResult result) {
    Window *result_window = window_create();
    window_set_background_color(result_window, GColorDukeBlue);
    
    Layer *root_layer = window_get_root_layer(result_window);
    GRect content_bounds = layer_get_bounds(root_layer);
    content_bounds.origin.x += PBL_IF_RECT_ELSE(16, 32);
    content_bounds.size.w -= PBL_IF_RECT_ELSE(32, 64);
    
    Layer *content_layer = layer_create_with_data(content_bounds, sizeof(ResultLayerData));
    ResultLayerData *layer_data = (ResultLayerData *)layer_get_data(content_layer);
    layer_data->frame = 0;
    layer_data->command_img = NULL;
    layer_data->command_seq = NULL;
    layer_data->label = malloc(32);
    
    if (result != FavoriteChangeFailed) {
        layer_data->command_seq = gdraw_command_sequence_create_with_resource(action == ACTION_TEAM_1 || action == ACTION_TEAM_2 ? RESOURCE_ID_ANIM_CONFIRM : RESOURCE_ID_ANIM_DELETED);
        strcpy(layer_data->label, "Favorites Updated");
    } else {
        layer_data->command_img = gdraw_command_image_create_with_resource(RESOURCE_ID_ERROR_80);
        strcpy(layer_data->label, "Update Failed");
    }
    
    layer_set_update_proc(content_layer, result_layer_update_proc);
    layer_add_child(root_layer, content_layer);
    
    ResultWindowData *data = malloc(sizeof(ResultWindowData));
    data->content_layer = content_layer;
    data->text_layer = NULL;
    data->close_timer = NULL;
    window_set_user_data(result_window, data);
    
    window_set_window_handlers(result_window, (WindowHandlers){
        .unload = on_unload,
    });
    
    if (layer_data->command_seq) {
        app_timer_register(DELTA, next_frame_handler, result_window);
    } else {
        data->close_timer = app_timer_register(3000, close_timer_callback, result_window);
    }
    
    return result_window;
}

Window *result_window_create_refresh(Game *game, GameUpdateResult result) {
    Window *result_window = window_create();
    window_set_background_color(result_window, GColorDukeBlue);
    
    Layer *root_layer = window_get_root_layer(result_window);
    GRect content_bounds = layer_get_bounds(root_layer);
    content_bounds.origin.x += PBL_IF_RECT_ELSE(16, 32);
    content_bounds.size.w -= PBL_IF_RECT_ELSE(32, 64);
    
    Layer *content_layer = layer_create_with_data(content_bounds, sizeof(ResultLayerData));
    ResultLayerData *layer_data = (ResultLayerData *)layer_get_data(content_layer);
    layer_data->frame = 0;
    layer_data->command_seq = NULL;
    layer_data->command_img = NULL;
    
    if (result == GameUpdateNetworkError) {
        layer_data->command_img = gdraw_command_image_create_with_resource(RESOURCE_ID_ERROR_80);
        layer_data->label = malloc(16);
        strcpy(layer_data->label, "No connection");
    } else {
        layer_data->command_img = NULL;
        layer_data->label = malloc(16);
        strcpy(layer_data->label, "Refreshed");
    }
    
    layer_set_update_proc(content_layer, result_layer_update_proc);
    layer_add_child(root_layer, content_layer);
    
    ResultWindowData *data = malloc(sizeof(ResultWindowData));
    data->content_layer = content_layer;
    data->text_layer = NULL;
    data->close_timer = app_timer_register(3000, close_timer_callback, result_window);
    window_set_user_data(result_window, data);
    
    window_set_window_handlers(result_window, (WindowHandlers){
        .unload = on_unload,
    });
    
    return result_window;
}

Window *result_window_create_broadcast(Game *game) {
    Window *result_window = window_create();
    window_set_background_color(result_window, GColorDukeBlue);
    
    Layer *root_layer = window_get_root_layer(result_window);
    GRect bounds = layer_get_bounds(root_layer);
    
    GRect text_bounds = GRect(
        PBL_IF_RECT_ELSE(10, 20), 
        PBL_IF_RECT_ELSE(30, 40), 
        bounds.size.w - PBL_IF_RECT_ELSE(20, 40), 
        bounds.size.h - PBL_IF_RECT_ELSE(40, 80)
    );
    
    TextLayer *text_layer = text_layer_create(text_bounds);
    text_layer_set_background_color(text_layer, GColorClear);
    text_layer_set_text_color(text_layer, GColorWhite);
    text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
    
    static char s_broadcast_buffer[64];
    if (game->broadcast && strlen(game->broadcast) > 0) {
        snprintf(s_broadcast_buffer, sizeof(s_broadcast_buffer), "Watch on:\n%s", game->broadcast);
    } else {
        snprintf(s_broadcast_buffer, sizeof(s_broadcast_buffer), "Broadcast:\nNot Available");
    }
    text_layer_set_text(text_layer, s_broadcast_buffer);
    layer_add_child(root_layer, text_layer_get_layer(text_layer));
    
    ResultWindowData *data = malloc(sizeof(ResultWindowData));
    data->content_layer = NULL;
    data->text_layer = text_layer;
    data->close_timer = app_timer_register(5000, close_timer_callback, result_window);
    window_set_user_data(result_window, data);
    
    window_set_window_handlers(result_window, (WindowHandlers){
        .unload = on_unload,
    });
    
    return result_window;
}