#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// Configuration API
typedef struct Config Config;

// Create/load configuration
// Creates default config file if it doesn't exist
Config* config_create(void);

// Destroy configuration
void config_destroy(Config* config);

// Get string value (returns NULL if not found)
const char* config_get_string(Config* config, const char* key);

// Get integer value (returns default_value if not found)
int config_get_int(Config* config, const char* key, int default_value);

// Get boolean value (returns default_value if not found)
bool config_get_bool(Config* config, const char* key, bool default_value);

// Set string value
void config_set_string(Config* config, const char* key, const char* value);

// Set integer value
void config_set_int(Config* config, const char* key, int value);

// Set boolean value
void config_set_bool(Config* config, const char* key, bool value);

// Save configuration to disk
bool config_save(Config* config);

// Get config file path (for debugging)
const char* config_get_path(void);

#endif // CONFIG_H