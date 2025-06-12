#include "preferences.h"
#include "logging.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024
#define MAX_KEY_LENGTH 128
#define MAX_VALUE_LENGTH (MAX_LINE_LENGTH - MAX_KEY_LENGTH - 4)

typedef struct PreferencesEntry {
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
    struct PreferencesEntry *next;
} PreferencesEntry;

typedef struct {
    PreferencesEntry *entries;
    char *config_path;
} Preferences;

// Global static instance
static Preferences *g_preferences = NULL;
static char g_config_dir[1024] = {0};
static char g_config_path[1024] = {0};

static void create_default_preferences(void) {
    preferences_set_bool("launch_at_login", false);
    preferences_set_string("model", "");      // Empty means use embedded model
    preferences_set_string("language", "en"); // Default to English for low latency
}

static PreferencesEntry *find_entry(const char *key) {
    if (!g_preferences)
        return NULL;

    PreferencesEntry *entry = g_preferences->entries;
    while (entry) {
        if (utils_stricmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static void set_entry(const char *key, const char *value) {
    if (!g_preferences)
        return;

    PreferencesEntry *entry = find_entry(key);

    if (entry) {
        // Update existing entry
        strncpy(entry->value, value, MAX_VALUE_LENGTH - 1);
        entry->value[MAX_VALUE_LENGTH - 1] = '\0';
    } else {
        // Create new entry
        entry = (PreferencesEntry *) malloc(sizeof(PreferencesEntry));
        strncpy(entry->key, key, MAX_KEY_LENGTH - 1);
        entry->key[MAX_KEY_LENGTH - 1] = '\0';
        strncpy(entry->value, value, MAX_VALUE_LENGTH - 1);
        entry->value[MAX_VALUE_LENGTH - 1] = '\0';

        // Add to front of list
        entry->next = g_preferences->entries;
        g_preferences->entries = entry;
    }
}

static char *trim(char *str) {
    if (!str)
        return str;

    // Trim leading whitespace
    while (*str && (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')) {
        str++;
    }

    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }

    return str;
}

bool preferences_init(void) {
    if (g_preferences) {
        log_info("Preferences already initialized");
        return true;
    }

    g_preferences = (Preferences *) calloc(1, sizeof(Preferences));
    if (!g_preferences) {
        log_error("Failed to allocate preferences");
        return false;
    }

    // Get config directory from platform
    const char *dir = utils_get_config_dir();
    if (!dir) {
        log_error("Failed to get config directory");
        free(g_preferences);
        g_preferences = NULL;
        return false;
    }

    strncpy(g_config_dir, dir, sizeof(g_config_dir) - 1);
    g_config_dir[sizeof(g_config_dir) - 1] = '\0';

    // Construct config path
    snprintf(g_config_path, sizeof(g_config_path), "%s%cconfig.ini", g_config_dir,
#ifdef _WIN32
             '\\'
#else
             '/'
#endif
    );

    g_preferences->config_path = utils_strdup(g_config_path);

    // Ensure config directory exists
    if (!utils_ensure_dir_exists(g_config_dir)) {
        log_error("Failed to create config directory: %s", g_config_dir);
        free(g_preferences->config_path);
        free(g_preferences);
        g_preferences = NULL;
        return false;
    }

    // Try to load existing config
    FILE *file = utils_fopen_read(g_config_path);
    if (file) {
        char line[MAX_LINE_LENGTH];
        char section[MAX_KEY_LENGTH] = "";

        while (fgets(line, sizeof(line), file)) {
            char *trimmed = trim(line);

            // Skip empty lines and comments
            if (!trimmed[0] || trimmed[0] == '#' || trimmed[0] == ';') {
                continue;
            }

            // Check for section header
            if (trimmed[0] == '[' && trimmed[strlen(trimmed) - 1] == ']') {
                trimmed[strlen(trimmed) - 1] = '\0';
                strncpy(section, trimmed + 1, MAX_KEY_LENGTH - 1);
                section[MAX_KEY_LENGTH - 1] = '\0';
                continue;
            }

            // Parse key=value
            char *equals = strchr(trimmed, '=');
            if (equals) {
                *equals = '\0';
                char *key = trim(trimmed);
                char *value = trim(equals + 1);

                if (key[0] && value[0]) {
                    set_entry(key, value);
                }
            }
        }

        fclose(file);
        log_info("Loaded config from: %s", g_config_path);
    } else {
        // Create default config
        create_default_preferences();
        preferences_save();
        log_info("Created default config at: %s", g_config_path);
    }

    return true;
}

void preferences_cleanup(void) {
    if (!g_preferences)
        return;

    // Free all entries
    PreferencesEntry *entry = g_preferences->entries;
    while (entry) {
        PreferencesEntry *next = entry->next;
        free(entry);
        entry = next;
    }

    // Free config path
    if (g_preferences->config_path) {
        free(g_preferences->config_path);
    }

    free(g_preferences);
    g_preferences = NULL;
}

const char *preferences_get_string(const char *key) {
    if (!key)
        return NULL;

    PreferencesEntry *entry = find_entry(key);
    return entry ? entry->value : NULL;
}

int preferences_get_int(const char *key, int default_value) {
    const char *value = preferences_get_string(key);
    if (!value)
        return default_value;

    char *endptr;
    long result = strtol(value, &endptr, 10);

    // Check if conversion was successful
    if (endptr == value || *endptr != '\0') {
        return default_value;
    }

    return (int) result;
}

bool preferences_get_bool(const char *key, bool default_value) {
    const char *value = preferences_get_string(key);
    if (!value)
        return default_value;

    // True values: "true", "yes", "1", "on"
    if (utils_stricmp(value, "true") == 0 || utils_stricmp(value, "yes") == 0 || utils_stricmp(value, "1") == 0 ||
        utils_stricmp(value, "on") == 0) {
        return true;
    }

    // False values: "false", "no", "0", "off"
    if (utils_stricmp(value, "false") == 0 || utils_stricmp(value, "no") == 0 || utils_stricmp(value, "0") == 0 ||
        utils_stricmp(value, "off") == 0) {
        return false;
    }

    return default_value;
}

void preferences_set_string(const char *key, const char *value) {
    if (!key || !value)
        return;
    set_entry(key, value);
}

void preferences_set_int(const char *key, int value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    preferences_set_string(key, buffer);
}

void preferences_set_bool(const char *key, bool value) {
    preferences_set_string(key, value ? "true" : "false");
}

bool preferences_save(void) {
    if (!g_preferences || !g_preferences->config_path)
        return false;

    FILE *file = utils_fopen_write(g_preferences->config_path);
    if (!file) {
        log_error("Failed to open config file for writing: %s", g_preferences->config_path);
        return false;
    }

    // Write section header
    fprintf(file, "[yakety]\n");

    // Write all entries
    PreferencesEntry *entry = g_preferences->entries;
    while (entry) {
        fprintf(file, "%s = %s\n", entry->key, entry->value);
        entry = entry->next;
    }

    fclose(file);
    log_info("Saved config to: %s", g_preferences->config_path);
    return true;
}

const char *preferences_get_path(void) {
    return g_config_path;
}

// Key combination helpers
void preferences_save_key_combination(const KeyCombination *combo) {
    char buffer[256] = {0};
    for (int i = 0; i < combo->count; i++) {
        if (i > 0)
            strcat(buffer, ";");
        char pair[32];
        snprintf(pair, sizeof(pair), "%X:%X", combo->keys[i].code, combo->keys[i].flags);
        strcat(buffer, pair);
    }
    preferences_set_string("KeyCombo", buffer);
}

bool preferences_load_key_combination(KeyCombination *combo) {
    const char *key_str = preferences_get_string("KeyCombo");
    if (key_str) {
        // Parse format: code1:flags1;code2:flags2;...
        combo->count = 0;
        char *copy = strdup(key_str);
        char *token = strtok(copy, ";");

        while (token && combo->count < 4) {
            unsigned int code, flags;
            if (sscanf(token, "%x:%x", &code, &flags) == 2) {
                combo->keys[combo->count].code = code;
                combo->keys[combo->count].flags = flags;
                combo->count++;
            }
            token = strtok(NULL, ";");
        }
        free(copy);
        return combo->count > 0;
    }
    return false;
}