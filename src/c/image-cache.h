#pragma once
#include <pebble.h>
#include "data/model/models.h" 

void image_cache_init(void);
void image_cache_deinit(void);
GBitmap* image_cache_get_sport_icon(Sport sport);