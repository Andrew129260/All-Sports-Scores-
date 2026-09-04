#include "pebble.h"
#include "prefs-handler.h"

ClaySettings clay_settings;

static void save_settings(ClaySettings new) {
    clay_settings = new;
    persist_write_data(SETTINGS_KEY, &clay_settings, sizeof(ClaySettings));
}

void load_settings() {
    clay_settings = (ClaySettings) {
        .show_record = ShowRecordFinalOnly,
    };
    // No longer persisting or loading settings since settings UI is removed.
    // Default to ShowRecordFinalOnly.
}