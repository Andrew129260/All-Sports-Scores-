#include "pebble.h"
#include "image-cache.h"

#define NUM_SPORTS 11
static GBitmap *s_sport_icons[NUM_SPORTS];

void image_cache_init(void) {
    for (int i = 0; i < NUM_SPORTS; i++) {
        // This runs only ONCE at boot, securing the memory safely
        s_sport_icons[i] = gbitmap_create_with_resource(sport_get_icon_res_small((Sport)i));
    }
}

void image_cache_deinit(void) {
    for (int i = 0; i < NUM_SPORTS; i++) {
        if (s_sport_icons[i] != NULL) {
            gbitmap_destroy(s_sport_icons[i]);
            s_sport_icons[i] = NULL;
        }
    }
}

GBitmap* image_cache_get_sport_icon(Sport sport) {
    // Safely return the cached image if it falls within our 11 initialized sports
    if (sport < NUM_SPORTS) {
        return s_sport_icons[sport]; 
    }
    return NULL;
}
