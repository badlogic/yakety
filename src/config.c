#include "config.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024
#define MAX_KEY_LENGTH 128
// MAX_VALUE_LENGTH = MAX_LINE_LENGTH - MAX_KEY_LENGTH - 3 (for " = ") - 1 (null terminator)
#define MAX_VALUE_LENGTH (MAX_LINE_LENGTH - MAX_KEY_LENGTH - 4)

typedef struct ConfigEntry {
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
    struct ConfigEntry* next;
} ConfigEntry;

struct Config {
    ConfigEntry* entries;
    char* config_path;
};

// Platform functions are implemented in utils
#include "utils.h"

// Static variables
static char g_config_dir[1024] = {0};
static char g_config_path[1024] = {0};

static void create_default_config(Config* config) {
    // Add default settings
    config_set_bool(config, "show_notifications", true);
    config_set_bool(config, "launch_at_login", false);
    config_set_string(config, "model", "");  // Empty means use default search
}

static ConfigEntry* find_entry(Config* config, const char* key) {
    ConfigEntry* entry = config->entries;
    while (entry) {
        if (utils_stricmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static void set_entry(Config* config, const char* key, const char* value) {
    ConfigEntry* entry = find_entry(config, key);
    
    if (entry) {
        // Update existing entry
        strncpy(entry->value, value, MAX_VALUE_LENGTH - 1);
        entry->value[MAX_VALUE_LENGTH - 1] = '\0';
    } else {
        // Create new entry
        entry = (ConfigEntry*)malloc(sizeof(ConfigEntry));
        strncpy(entry->key, key, MAX_KEY_LENGTH - 1);
        entry->key[MAX_KEY_LENGTH - 1] = '\0';
        strncpy(entry->value, value, MAX_VALUE_LENGTH - 1);
        entry->value[MAX_VALUE_LENGTH - 1] = '\0';
        
        // Add to front of list
        entry->next = config->entries;
        config->entries = entry;
    }
}

static char* trim(char* str) {
    if (!str) return str;
    
    // Trim leading whitespace
    while (*str && (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')) {
        str++;
    }
    
    // Trim trailing whitespace
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
    
    return str;
}

Config* config_create(void) {
    Config* config = (Config*)calloc(1, sizeof(Config));
    if (!config) {
        log_error("Failed to allocate config");
        return NULL;
    }
    
    // Get config directory from platform
    const char* dir = utils_get_config_dir();
    if (!dir) {
        log_error("Failed to get config directory");
        free(config);
        return NULL;
    }
    
    strncpy(g_config_dir, dir, sizeof(g_config_dir) - 1);
    g_config_dir[sizeof(g_config_dir) - 1] = '\0';
    
    // Construct config path
    snprintf(g_config_path, sizeof(g_config_path), "%s%cconfig.ini", 
             g_config_dir, 
#ifdef _WIN32
             '\\'
#else
             '/'
#endif
    );
    
    config->config_path = utils_strdup(g_config_path);
    
    // Ensure config directory exists
    if (!utils_ensure_dir_exists(g_config_dir)) {
        log_error("Failed to create config directory: %s", g_config_dir);
        free(config->config_path);
        free(config);
        return NULL;
    }
    
    // Try to load existing config
    FILE* file = utils_fopen_read(g_config_path);
    if (file) {
        char line[MAX_LINE_LENGTH];
        char section[MAX_KEY_LENGTH] = "";
        
        while (fgets(line, sizeof(line), file)) {
            char* trimmed = trim(line);
            
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
            char* equals = strchr(trimmed, '=');
            if (equals) {
                *equals = '\0';
                char* key = trim(trimmed);
                char* value = trim(equals + 1);
                
                if (key[0] && value[0]) {
                    set_entry(config, key, value);
                }
            }
        }
        
        fclose(file);
        log_info("Loaded config from: %s", g_config_path);
    } else {
        // Create default config
        create_default_config(config);
        config_save(config);
        log_info("Created default config at: %s", g_config_path);
    }
    
    return config;
}

void config_destroy(Config* config) {
    if (!config) return;
    
    // Free all entries
    ConfigEntry* entry = config->entries;
    while (entry) {
        ConfigEntry* next = entry->next;
        free(entry);
        entry = next;
    }
    
    // Free config path
    if (config->config_path) {
        free(config->config_path);
    }
    
    free(config);
}

const char* config_get_string(Config* config, const char* key) {
    if (!config || !key) return NULL;
    
    ConfigEntry* entry = find_entry(config, key);
    return entry ? entry->value : NULL;
}

int config_get_int(Config* config, const char* key, int default_value) {
    const char* value = config_get_string(config, key);
    if (!value) return default_value;
    
    char* endptr;
    long result = strtol(value, &endptr, 10);
    
    // Check if conversion was successful
    if (endptr == value || *endptr != '\0') {
        return default_value;
    }
    
    return (int)result;
}

bool config_get_bool(Config* config, const char* key, bool default_value) {
    const char* value = config_get_string(config, key);
    if (!value) return default_value;
    
    // True values: "true", "yes", "1", "on"
    if (utils_stricmp(value, "true") == 0 ||
        utils_stricmp(value, "yes") == 0 ||
        utils_stricmp(value, "1") == 0 ||
        utils_stricmp(value, "on") == 0) {
        return true;
    }
    
    // False values: "false", "no", "0", "off"
    if (utils_stricmp(value, "false") == 0 ||
        utils_stricmp(value, "no") == 0 ||
        utils_stricmp(value, "0") == 0 ||
        utils_stricmp(value, "off") == 0) {
        return false;
    }
    
    return default_value;
}

void config_set_string(Config* config, const char* key, const char* value) {
    if (!config || !key || !value) return;
    set_entry(config, key, value);
}

void config_set_int(Config* config, const char* key, int value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    config_set_string(config, key, buffer);
}

void config_set_bool(Config* config, const char* key, bool value) {
    config_set_string(config, key, value ? "true" : "false");
}

bool config_save(Config* config) {
    if (!config || !config->config_path) return false;
    
    FILE* file = utils_fopen_write(config->config_path);
    if (!file) {
        log_error("Failed to open config file for writing: %s", config->config_path);
        return false;
    }
    
    // Write section header
    fprintf(file, "[yakety]\n");
    
    // Write all entries
    ConfigEntry* entry = config->entries;
    while (entry) {
        fprintf(file, "%s = %s\n", entry->key, entry->value);
        entry = entry->next;
    }
    
    fclose(file);
    log_info("Saved config to: %s", config->config_path);
    return true;
}

const char* config_get_path(void) {
    return g_config_path;
}