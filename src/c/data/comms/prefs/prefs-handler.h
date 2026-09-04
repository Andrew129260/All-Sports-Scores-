#include "pebble.h"
#include <stdbool.h>

#define SETTINGS_KEY 1

#ifndef PREFS_HANDLER_H_
    typedef enum {
        ShowRecordNever, ShowRecordFinalOnly, ShowRecordAlways
    } ShowRecordSetting;

    typedef struct {
        ShowRecordSetting show_record;
    } ClaySettings;
    
#endif

void load_settings();
ClaySettings clay_settings;
