#include "../config.h"
#include "../logging.h"
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LINE_LENGTH 1024
#define MAX_KEY_LENGTH 128
#define MAX_VALUE_LENGTH 512

typedef struct ConfigEntry {
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
    struct ConfigEntry* next;
} ConfigEntry;

struct Config {
    ConfigEntry* entries;
    char* config_path;
};

static char g_config_dir[MAX_PATH] = {0};
static char g_config_path[MAX_PATH] = {0};

// Get the config directory path (~/.yakety on Windows = %USERPROFILE%\.yakety)
static const char* get_config_dir(void) {
    if (g_config_dir[0] == '\0') {
        char* userprofile = getenv("USERPROFILE");
        if (userprofile) {
            snprintf(g_config_dir, MAX_PATH, "%s\\.yakety", userprofile);
        } else {
            // Fallback to HOMEDRIVE + HOMEPATH
            char* homedrive = getenv("HOMEDRIVE");
            char* homepath = getenv("HOMEPATH");
            if (homedrive && homepath) {
                snprintf(g_config_dir, MAX_PATH, "%s%s\\.yakety", homedrive, homepath);
            } else {
                // Last resort - use current directory
                strcpy(g_config_dir, ".yakety");
            }
        }
    }
    return g_config_dir;
}

const char* config_get_path(void) {
    if (g_config_path[0] == '\0') {
        snprintf(g_config_path, MAX_PATH, "%s\\config.ini", get_config_dir());
    }
    return g_config_path;
}

// Ensure config directory exists
static bool ensure_config_dir(void) {
    const char* dir = get_config_dir();
    
    // Check if directory exists
    DWORD attrs = GetFileAttributesA(dir);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        // Create directory
        if (!CreateDirectoryA(dir, NULL)) {
            log_error("Failed to create config directory: %s\n", dir);
            return false;
        }
    }
    
    return true;
}

// Create default configuration
static void create_default_config(Config* config) {
    config_set_bool(config, "first_run", true);
    config_set_bool(config, "launch_at_login", false);
    config_set_string(config, "model", "ggml-base.en.bin");
    config_set_bool(config, "show_notifications", true);
    config_set_int(config, "log_level", 1); // 0=error, 1=info, 2=debug
}

// Find entry by key
static ConfigEntry* find_entry(Config* config, const char* key) {
    ConfigEntry* entry = config->entries;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

// Add or update entry
static void set_entry(Config* config, const char* key, const char* value) {
    ConfigEntry* entry = find_entry(config, key);
    
    if (entry) {
        // Update existing entry
        strncpy(entry->value, value, MAX_VALUE_LENGTH - 1);
        entry->value[MAX_VALUE_LENGTH - 1] = '\0';
    } else {
        // Add new entry
        entry = (ConfigEntry*)calloc(1, sizeof(ConfigEntry));
        if (!entry) return;
        
        strncpy(entry->key, key, MAX_KEY_LENGTH - 1);
        entry->key[MAX_KEY_LENGTH - 1] = '\0';
        
        strncpy(entry->value, value, MAX_VALUE_LENGTH - 1);
        entry->value[MAX_VALUE_LENGTH - 1] = '\0';
        
        // Add to front of list
        entry->next = config->entries;
        config->entries = entry;
    }
}

// Trim whitespace from string
static char* trim(char* str) {
    char* start = str;
    char* end;
    
    // Trim leading whitespace
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;
    }
    
    // All whitespace
    if (*start == '\0') {
        return start;
    }
    
    // Trim trailing whitespace
    end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    
    // Write null terminator
    *(end + 1) = '\0';
    
    return start;
}

Config* config_create(void) {
    Config* config = (Config*)calloc(1, sizeof(Config));
    if (!config) {
        log_error("Failed to allocate config\n");
        return NULL;
    }
    
    // Ensure config directory exists
    if (!ensure_config_dir()) {
        free(config);
        return NULL;
    }
    
    config->config_path = _strdup(config_get_path());
    
    // Try to load existing config
    FILE* file;
    errno_t err = fopen_s(&file, config->config_path, "r");
    if (err == 0 && file) {
        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), file)) {
            char* trimmed = trim(line);
            
            // Skip empty lines and comments
            if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
                continue;
            }
            
            // Find the equals sign
            char* equals = strchr(trimmed, '=');
            if (!equals) {
                continue;
            }
            
            // Split key and value
            *equals = '\0';
            char* key = trim(trimmed);
            char* value = trim(equals + 1);
            
            // Store the entry
            set_entry(config, key, value);
        }
        fclose(file);
        log_info("Loaded config from: %s\n", config->config_path);
    } else {
        // Create default config
        log_info("Creating default config: %s\n", config->config_path);
        create_default_config(config);
        config_save(config);
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
    
    free(config->config_path);
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
    
    // Check for conversion errors
    if (*endptr != '\0') {
        return default_value;
    }
    
    return (int)result;
}

bool config_get_bool(Config* config, const char* key, bool default_value) {
    const char* value = config_get_string(config, key);
    if (!value) return default_value;
    
    // Check for common boolean values
    if (_stricmp(value, "true") == 0 || 
        _stricmp(value, "yes") == 0 || 
        _stricmp(value, "1") == 0 ||
        _stricmp(value, "on") == 0) {
        return true;
    }
    
    if (_stricmp(value, "false") == 0 || 
        _stricmp(value, "no") == 0 || 
        _stricmp(value, "0") == 0 ||
        _stricmp(value, "off") == 0) {
        return false;
    }
    
    return default_value;
}

void config_set_string(Config* config, const char* key, const char* value) {
    if (!config || !key || !value) return;
    set_entry(config, key, value);
}

void config_set_int(Config* config, const char* key, int value) {
    if (!config || !key) return;
    
    char str_value[32];
    snprintf(str_value, sizeof(str_value), "%d", value);
    set_entry(config, key, str_value);
}

void config_set_bool(Config* config, const char* key, bool value) {
    if (!config || !key) return;
    set_entry(config, key, value ? "true" : "false");
}

bool config_save(Config* config) {
    if (!config || !config->config_path) return false;
    
    FILE* file;
    errno_t err = fopen_s(&file, config->config_path, "w");
    if (err != 0 || !file) {
        log_error("Failed to open config file for writing: %s\n", config->config_path);
        return false;
    }
    
    // Write header
    fprintf(file, "# Yakety Configuration File\n");
    fprintf(file, "# Generated automatically - manual edits are preserved\n\n");
    
    // Write all entries
    ConfigEntry* entry = config->entries;
    while (entry) {
        fprintf(file, "%s=%s\n", entry->key, entry->value);
        entry = entry->next;
    }
    
    fclose(file);
    return true;
}