#ifndef PREFS_HANDLER_H_
#define PREFS_HANDLER_H_

#include "pebble.h"
#include <stdbool.h>

#define SETTINGS_KEY 1

typedef enum {
    ShowRecordNever, ShowRecordFinalOnly, ShowRecordAlways
} ShowRecordSetting;

typedef struct {
    ShowRecordSetting show_record;
} ClaySettings;

void load_settings(void);
void handle_config_recieved(DictionaryIterator *iter);

extern ClaySettings clay_settings;

#endif