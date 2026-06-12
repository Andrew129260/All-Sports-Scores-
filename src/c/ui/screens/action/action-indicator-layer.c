#include "pebble.h"

static void action_indicator_update_proc(Layer *layer, GContext *ctx) {
    GRect layer_bounds = layer_get_bounds(layer);

    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, GPoint(layer_bounds.origin.x + 12, layer_bounds.origin.y + 12), 12);
}

Layer *action_indicator_layer_create() {
    // Dynamically calculate the position based on the actual screen resolution
    // Subtracts 6 pixels for rectangular screens, 10 for round to maintain original padding
    int x_pos = PBL_DISPLAY_WIDTH - PBL_IF_RECT_ELSE(6, 10);
    int y_pos = (PBL_DISPLAY_HEIGHT / 2) - 10;
    
    GRect bounds = GRect(x_pos, y_pos, 12, 24);
    Layer *indicator = layer_create(bounds);
    layer_set_update_proc(indicator, action_indicator_update_proc);
    
    return indicator;
}
