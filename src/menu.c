#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "dialog.h"
#include "keylogger.h"
#include "logging.h"
#include "menu.h"
#include "models.h"
#include "overlay.h"
#include "preferences.h"
#include "transcription.h"
#include "utils.h"

// Global variables for menu management
int g_launch_menu_index = -1;

// Menu callback functions
static void menu_about(void) {
    dialog_info("About Yakety", "Yakety v1.0\n"
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
    dialog_info("Licenses", "This software includes:\n"
                            "- Whisper.cpp by ggml authors (MIT License)\n"
                            "- ggml by ggml authors (MIT License)\n"
                            "- Whisper base.en model by OpenAI (MIT License)\n"
                            "- miniaudio by David Reid (Public Domain)\n\n"
                            "See LICENSES.md for full details.");
}

static void menu_models(void) {
    // Variables for model selection
    char selected_model[1024] = {0};
    char selected_language[64] = {0};
    char download_url[1024] = {0};

    // Show models dialog
    if (!dialog_models_and_language("Models & Language Settings", selected_model, sizeof(selected_model),
                                    selected_language, sizeof(selected_language), download_url, sizeof(download_url))) {
        // User cancelled
        return;
    }

    // Handle download if needed
    if (strlen(download_url) > 0) {
        // Extract model name for display
        const char *model_name = strrchr(selected_model, '/');
        model_name = model_name ? model_name + 1 : "Model";

        char display_name[256];
        strncpy(display_name, model_name, sizeof(display_name) - 1);
        display_name[sizeof(display_name) - 1] = '\0';

        // Remove file extension
        char *dot = strrchr(display_name, '.');
        if (dot)
            *dot = '\0';

        // Disable menu and download
        menu_set_enabled(false);
        int download_result = dialog_model_download(display_name, download_url, selected_model);
        menu_set_enabled(true);

        if (download_result != 0) {
            return; // Download cancelled or failed
        }
    }

    // Check if anything actually changed
    const char *current_model = preferences_get_string("model");
    const char *current_language = preferences_get_string("language");

    bool model_changed = (current_model == NULL && strlen(selected_model) > 0) ||
                         (current_model != NULL && strcmp(current_model, selected_model) != 0);
    bool language_changed = (current_language == NULL && strcmp(selected_language, "en") != 0) ||
                            (current_language != NULL && strcmp(current_language, selected_language) != 0);

    if (!model_changed && !language_changed) {
        return; // No changes
    }

    // Update preferences
    preferences_set_string("model", selected_model);
    preferences_set_string("language", selected_language);
    preferences_save();

    // Pause keylogger during reload
    keylogger_pause();

    // Load the model using THE ONE function
    int result = models_load();

    // Resume keylogger
    keylogger_resume();

    if (result != 0) {
        log_error("Failed to load model after settings change");
    }
}

static void menu_configure_hotkey(void) {
    KeyCombination combo;
    bool result = dialog_keycombination_capture(
        "Configure Hotkey", "Click in the box below and press your desired key combination:", &combo);

    if (result) {
        // Update the keylogger to monitor this combination
        keylogger_set_combination(&combo);

        // Save to preferences
        preferences_save_key_combination(&combo);
        preferences_save();
    }
}

static void menu_toggle_launch_at_login(void) {
    bool is_enabled = utils_is_launch_at_login_enabled();
    bool success = utils_set_launch_at_login(!is_enabled);

    if (success) {
        const char *status = is_enabled ? "disabled" : "enabled";
        char message[256];
        snprintf(message, sizeof(message), "Launch at login has been %s.", status);
        dialog_info("Launch Settings", message);

        // Update the menu item title
        if (g_launch_menu_index >= 0) {
            const char *new_label = is_enabled ? "Enable Launch at Login" : "Disable Launch at Login";
            menu_update_item(g_launch_menu_index, new_label);
        }
    } else {
        dialog_error("Launch Settings", "Failed to change launch at login setting.");
    }
}

static void menu_quit(void) {
    app_quit();
}

// Shared menu setup logic (used by platform implementations)
int menu_setup_items(MenuSystem *menu) {
    if (!menu) {
        return -1;
    }

    menu_add_item(menu, "About Yakety", menu_about);
    menu_add_item(menu, "Licenses", menu_licenses);
    menu_add_separator(menu);
    menu_add_item(menu, "Models & Languages", menu_models);
    menu_add_item(menu, "Configure Hotkey", menu_configure_hotkey);
    menu_add_separator(menu);

    // Add launch at login toggle and track its index
    const char *launch_label =
        utils_is_launch_at_login_enabled() ? "Disable Launch at Login" : "Enable Launch at Login";
    g_launch_menu_index = menu_add_item(menu, launch_label, menu_toggle_launch_at_login);

    menu_add_separator(menu);
    menu_add_item(menu, "Quit", menu_quit);

    return 0;
}
