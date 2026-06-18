#include <pebble.h>
#include "header.h"

static void rect_header_update_proc(Layer *layer, GContext *ctx) {

    HeaderData *data = (HeaderData *)layer_get_data(layer);

    graphics_context_set_fill_color(ctx, GColorDukeBlue);
    GRect layer_bounds = layer_get_bounds(layer);
    graphics_fill_rect(ctx, layer_bounds, 0, GCornerNone);

    GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    int icon_padding = 8;
    
    // 1. Safe Icon Check & Native Bounds Measurement
    if (data->icon != NULL) {
        GRect image_bounds = gbitmap_get_bounds(data->icon);
        if (image_bounds.size.w > 0 && image_bounds.size.h > 0) {
            icon_padding = 32;
        }
    }
    
    GSize title_size = graphics_text_layout_get_content_size(data->title, title_font, layer_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    GRect title_bounds = GRect(icon_padding, STATUS_BAR_LAYER_HEIGHT, title_size.w, 18);
    graphics_draw_text(ctx, data->title, title_font, title_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // 2. Safe Bitmap Drawing
    if (data->icon != NULL) {
        GRect image_bounds = gbitmap_get_bounds(data->icon);
        
        // Only attempt to composite the image if it has valid geometry!
        if (image_bounds.size.w > 0 && image_bounds.size.h > 0) {
            // Center the 16x16 or 25x25 image properly in the header
            int y_offset = STATUS_BAR_LAYER_HEIGHT + ((28 - image_bounds.size.h) / 2);
            GRect icon_bounds = GRect(8, y_offset, image_bounds.size.w, image_bounds.size.h);
            
            graphics_context_set_compositing_mode(ctx, GCompOpSet);
            graphics_draw_bitmap_in_rect(ctx, data->icon, icon_bounds);
        }
    }   
}

static void circle_header_update_proc(Layer *layer, GContext *ctx) {

    HeaderData *data = (HeaderData *)layer_get_data(layer);
    GRect layer_bounds = layer_get_bounds(layer);

    int menu_bottom = data->under_status_bar ? (28 + STATUS_BAR_LAYER_HEIGHT) : 32;
    //Mask any long menu items
    GRect mask_bounds = GRect(layer_bounds.origin.x, layer_bounds.origin.y, layer_bounds.size.w, menu_bottom);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, mask_bounds, 0, GCornerNone);

    graphics_context_set_fill_color(ctx, GColorDukeBlue);
    int radius = layer_bounds.size.w;
    int horz_center = layer_bounds.origin.x + (layer_bounds.size.w/2);
    int vert_center = menu_bottom - radius;
    graphics_fill_circle(ctx, GPoint(horz_center, vert_center), radius);

    GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    GSize title_size = graphics_text_layout_get_content_size(data->title, title_font, layer_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    
    int icon_padding = 0;
    // Safe Round Icon Check
    if (data->icon != NULL) {
        GRect image_bounds = gbitmap_get_bounds(data->icon);
        if (image_bounds.size.w > 0) {
            icon_padding = 8;
        }
    }
    
    GRect title_bounds = GRect(layer_bounds.size.w / 2 - title_size.w /2 + icon_padding, data->under_status_bar ? 2 + STATUS_BAR_LAYER_HEIGHT : 4, title_size.w, 18);
    graphics_draw_text(ctx, data->title, title_font, title_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // Safe Round Bitmap Drawing
    if (data->icon != NULL) {
        GRect image_bounds = gbitmap_get_bounds(data->icon);
        if (image_bounds.size.w > 0 && image_bounds.size.h > 0) {
            int x_offset = (layer_bounds.size.w / 2) - (title_size.w / 2) - image_bounds.size.w - 4;
            int y_offset = data->under_status_bar ? 6 + STATUS_BAR_LAYER_HEIGHT : 8;
            GRect icon_bounds = GRect(x_offset, y_offset, image_bounds.size.w, image_bounds.size.h);
            
            graphics_context_set_compositing_mode(ctx, GCompOpSet);
            graphics_draw_bitmap_in_rect(ctx, data->icon, icon_bounds);
        }
    }   
}

HeaderLayer *create_header_layer(GRect window_bounds, HeaderData data) {

    HeaderLayer *header; 

    GRect header_bounds = window_bounds; 

    header_bounds.size.h = 28 + STATUS_BAR_LAYER_HEIGHT;
    header = layer_create_with_data(header_bounds, sizeof(HeaderData));
    HeaderData *layer_data = (HeaderData *)layer_get_data(header);
    layer_data->icon = data.icon;
    layer_data->title = data.title;
    layer_data->under_status_bar = data.under_status_bar;

    layer_set_update_proc(header, PBL_IF_RECT_ELSE(rect_header_update_proc, circle_header_update_proc));

    return header;
}

void header_layer_set_under_status_bar(HeaderLayer *header_layer, bool under_status_bar) {
    HeaderData *layer_data = (HeaderData *) layer_get_data(header_layer);
    layer_data->under_status_bar = under_status_bar;
    layer_mark_dirty(header_layer);
}