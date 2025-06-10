#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "menu.h"
#include "dialog.h"
#include "keylogger.h"
#include "config.h"
#include "utils.h"
#include "app.h"

// Static variables for menu management
static Config* s_config = NULL;
static volatile bool* s_running = NULL;

// Global variables for menu management
static MenuSystem* g_menu = NULL;
static int g_launch_menu_index = -1;

// Helper to save key combination to config
static void save_key_combination(Config* config, const KeyCombination* combo) {
    char buffer[256] = {0};
    for (int i = 0; i < combo->count; i++) {
        if (i > 0) strcat(buffer, ";");
        char pair[32];
        snprintf(pair, sizeof(pair), "%X:%X", combo->keys[i].code, combo->keys[i].flags);
        strcat(buffer, pair);
    }
    config_set_string(config, "KeyCombo", buffer);
}

// Menu callback functions
static void menu_about(void) {
    dialog_info("About Yakety",
        "Yakety v1.0\n"
        "Voice-to-text input for any application\n\n"
        #ifdef _WIN32
        "Hold Right Ctrl key to record,\n"
        #else
        "Hold FN key to record,\n"
        #endif
        "release to transcribe and paste.\n\n"
        "© 2025 Mario Zechner");
}

static void menu_licenses(void) {
    dialog_info("Licenses",
        "This software includes:\n"
        "- Whisper.cpp by ggml authors (MIT License)\n"
        "- ggml by ggml authors (MIT License)\n"
        "- Whisper base.en model by OpenAI (MIT License)\n"
        "- miniaudio by David Reid (Public Domain)\n\n"
        "See LICENSES.md for full details.");
}

static void menu_configure_hotkey(void) {
    KeyCombination combo;
    bool result = dialog_keycombination_capture(
        "Configure Hotkey",
        "Click in the box below and press your desired key combination:",
        &combo
    );
    
    if (result) {
        // Build display message
        char message[256] = "Hotkey configured:\n";
        for (int i = 0; i < combo.count; i++) {
            char key_info[128];
            #ifdef _WIN32
            snprintf(key_info, sizeof(key_info), "Key %d: scancode=0x%02X, extended=%d\n",
                    i + 1, combo.keys[i].code, combo.keys[i].flags);
            #else
            // macOS format - show keycode and modifier flags
            snprintf(key_info, sizeof(key_info), "Key %d: keycode=%d, modifiers=0x%X\n",
                    i + 1, combo.keys[i].code, combo.keys[i].flags);
            #endif
            strcat(message, key_info);
        }
        dialog_info("Hotkey Configured", message);
        
        // Update the keylogger to monitor this combination
        keylogger_set_combination(&combo);
        
        // Save to config
        if (s_config) {
            save_key_combination(s_config, &combo);
            config_save(s_config);
        }
    }
}

static void menu_toggle_launch_at_login(void) {
    bool is_enabled = utils_is_launch_at_login_enabled();
    bool success = utils_set_launch_at_login(!is_enabled);

    if (success) {
        const char* status = is_enabled ? "disabled" : "enabled";
        char message[256];
        snprintf(message, sizeof(message), "Launch at login has been %s.", status);
        dialog_info("Launch Settings", message);

        // Update the menu item title
        if (g_menu && g_launch_menu_index >= 0) {
            const char* new_label = is_enabled ? "Enable Launch at Login" : "Disable Launch at Login";
            menu_update_item(g_menu, g_launch_menu_index, new_label);
        }
    } else {
        dialog_error("Launch Settings", "Failed to change launch at login setting.");
    }
}

static void menu_quit(void) {
    if (s_running) *s_running = false;
    app_quit();
}

// Initialize menu system with external state
void menu_init(Config* config, volatile bool* running) {
    s_config = config;
    s_running = running;
}

// Create and configure the application menu
MenuSystem* menu_setup(void) {
    g_menu = menu_create();
    if (!g_menu) {
        return NULL;
    }

    menu_add_item(g_menu, "About Yakety", menu_about);
    menu_add_item(g_menu, "Licenses", menu_licenses);
    menu_add_separator(g_menu);
    menu_add_item(g_menu, "Configure Hotkey", menu_configure_hotkey);
    menu_add_separator(g_menu);

    // Add launch at login toggle and track its index
    const char* launch_label = utils_is_launch_at_login_enabled()
        ? "Disable Launch at Login"
        : "Enable Launch at Login";
    g_launch_menu_index = g_menu->item_count;  // Store the index before adding
    menu_add_item(g_menu, launch_label, menu_toggle_launch_at_login);

    menu_add_separator(g_menu);
    menu_add_item(g_menu, "Quit", menu_quit);

    return g_menu;
}