#include "pebble.h"
#include "image-cache.h"

#define NUM_SPORTS 11
static GBitmap *s_sport_icons[NUM_SPORTS];

void image_cache_init(void) {
    // Initialize the array to NULL, but do NOT load images yet!
    for (int i = 0; i < NUM_SPORTS; i++) {
        s_sport_icons[i] = NULL;
    }
}

void image_cache_deinit(void) {
    // Safely destroy only the images that were actually loaded
    for (int i = 0; i < NUM_SPORTS; i++) {
        if (s_sport_icons[i] != NULL) {
            gbitmap_destroy(s_sport_icons[i]);
            s_sport_icons[i] = NULL;
        }
    }
}

GBitmap* image_cache_get_sport_icon(Sport sport) {
    if (sport >= NUM_SPORTS) {
        return NULL; 
    }

    // LAZY LOAD: If the image hasn't been loaded into RAM yet, load it now.
    if (s_sport_icons[sport] == NULL) {
        s_sport_icons[sport] = gbitmap_create_with_resource(sport_get_icon_res_small((Sport)sport));
    }
    
    return s_sport_icons[sport];
}