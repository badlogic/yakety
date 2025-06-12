#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "keylogger.h"
#include <stdbool.h>

// Preferences API - singleton pattern
// Initialize preferences system (loads or creates config file)
bool preferences_init(void);

// Cleanup preferences system
void preferences_cleanup(void);

// Get string value (returns NULL if not found)
const char *preferences_get_string(const char *key);

// Get integer value (returns default_value if not found)
int preferences_get_int(const char *key, int default_value);

// Get boolean value (returns default_value if not found)
bool preferences_get_bool(const char *key, bool default_value);

// Set string value
void preferences_set_string(const char *key, const char *value);

// Set integer value
void preferences_set_int(const char *key, int value);

// Set boolean value
void preferences_set_bool(const char *key, bool value);

// Save preferences to disk
bool preferences_save(void);

// Get config file path (for debugging)
const char *preferences_get_path(void);

// Key combination helpers
void preferences_save_key_combination(const KeyCombination *combo);
bool preferences_load_key_combination(KeyCombination *combo);

#endif // PREFERENCES_H