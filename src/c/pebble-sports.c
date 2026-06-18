#include "pebble.h"
#include "sports-menu.h"
#include "src/c/data/comms/comms.h"
#include "data/comms/prefs/prefs-handler.h"
#include "image-cache.h"

static void init() {
    load_settings();
    setup_comms();
    
    // Boot the global cache before any menus open!
    image_cache_init(); 
    
    show_sports_menu();
}

static void deinit() {
    destroy_comms();
    hide_sports_menu();
    
    // Clean up memory on exit
    image_cache_deinit(); 
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}