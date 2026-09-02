#include "pebble.h"
#include "sports-menu.h"
#include "src/c/data/comms/comms.h"
#include "data/comms/prefs/prefs-handler.h"
#include "image-cache.h"

static void init() {
    srand(time(NULL));

    load_settings();
    setup_comms();
    
    // Boot the global cache before any menus open!
    image_cache_init(); 
    
    // Opt-in to the native Pebble OS touch bridge for MenuLayers!
    #if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    app_touch_navigation_enable(true);
    #endif
    
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