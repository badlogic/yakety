#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "dialog.h"
#include "keylogger.h"
#include "menu.h"
#include "overlay.h"
#include "preferences.h"
#include "transcription.h"
#include "utils.h"

// Global variables for menu management
int g_launch_menu_index = -1;

// Global flag to track if model is being reloaded - use atomic access
static bool g_model_reloading = false;

// Helper function to show error dialog on main thread
static void show_error_dialog(void *message) {
    const char *error_msg = (const char *) message;
    overlay_hide(); // Hide overlay before showing error
    dialog_error("Model Error", error_msg);
}

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

// Async model reload function
static void *reload_model_async(void *arg) {
    (void) arg;

    // Pause keylogger to prevent recording during model reload
    keylogger_pause();

    // Cleanup current model
    transcription_cleanup();

    // Load new model
    const char *model_path = utils_get_model_path();
    if (!model_path) {
        // Schedule error dialog on main thread
        utils_execute_main_thread(0, show_error_dialog, (void *) "Could not find the specified model file.");
        keylogger_resume(); // Resume keylogger on error
        return (void *) 0;
    }

    if (transcription_init(model_path) != 0) {
        // Check if it's a corrupted downloaded file and delete it
        const char *current_model = preferences_get_string("model");
        if (current_model && strlen(current_model) > 0) {
            // This is a user model that failed to load - likely corrupted
            remove(current_model); // Delete the corrupted file

            // Clear the model preference to fall back to bundled model
            preferences_set_string("model", "");
            preferences_save();

            // Schedule error dialog on main thread
            utils_execute_main_thread(
                0, show_error_dialog,
                (void *) "Downloaded model was corrupted and has been removed. Using bundled model.");
        } else {
            // Schedule error dialog on main thread
            utils_execute_main_thread(0, show_error_dialog, (void *) "Failed to initialize the model.");
        }

        keylogger_resume(); // Resume keylogger on error
        return (void *) 0;
    }

    // Set language from preferences
    const char *language = preferences_get_string("language");
    if (language) {
        transcription_set_language(language);
    } else {
        transcription_set_language("en"); // Default to English
    }

    // Resume keylogger after successful reload
    keylogger_resume();

    return (void *) 1;
}

// Reload model and language settings
static void reload_model_and_language(void) {
    if (utils_atomic_read_bool(&g_model_reloading)) {
        return; // Already reloading, exit silently
    }

    utils_atomic_write_bool(&g_model_reloading, true);

    // Show loading overlay on main thread
    overlay_show("Loading model");

    // Start reload using cross-platform async execution
    void *result = app_execute_async_blocking(reload_model_async, NULL);

    // Hide overlay on main thread
    overlay_hide();

    // Always reset the flag on the main thread
    utils_atomic_write_bool(&g_model_reloading, false);

    if (!result) {
        // Show error on failure
        dialog_error("Model Reload Error", "Failed to reload model.");
    }
}

static void menu_models(void) {
    if (utils_atomic_read_bool(&g_model_reloading)) {
        dialog_info("Model Loading", "Model is currently being reloaded. Please wait before making changes.");
        return;
    }

    // Variables for model selection
    char selected_model[1024];
    char selected_language[64];
    char download_url[1024];

    // Loop until user cancels or successfully selects a model
    while (true) {
        // Clear buffers
        memset(selected_model, 0, sizeof(selected_model));
        memset(selected_language, 0, sizeof(selected_language));
        memset(download_url, 0, sizeof(download_url));

        if (!dialog_models_and_language("Models & Language Settings", selected_model, sizeof(selected_model),
                                        selected_language, sizeof(selected_language), download_url,
                                        sizeof(download_url))) {
            // User cancelled
            return;
        }

        // Check if model needs to be downloaded
        if (strlen(download_url) > 0) {
            // Extract model name from selected_model path
            const char *model_name = strrchr(selected_model, '/');
            if (model_name) {
                model_name++; // Skip the '/'
            } else {
                model_name = "Model"; // Default name if path parsing fails
            }

            // Remove file extension if present
            char display_name[256];
            strncpy(display_name, model_name, sizeof(display_name) - 1);
            display_name[sizeof(display_name) - 1] = '\0';

            char *dot = strrchr(display_name, '.');
            if (dot) {
                *dot = '\0'; // Remove extension
            }

            // Show download dialog
            int download_result = dialog_model_download(display_name, download_url, selected_model);

            if (download_result == 0) {
                // Download succeeded - proceed with loading the model (break out of dialog loop)
                // selected_model already contains the downloaded model path
                break;
            } else {
                // Download cancelled or failed - go back to models dialog
                continue;
            }
        } else {
            // Regular model selection without download
            break;
        }
    }


    // Get current settings to check for changes
    const char *current_model = preferences_get_string("model");
    const char *current_language = preferences_get_string("language");


    // Check if settings actually changed
    bool model_changed = (current_model == NULL && strlen(selected_model) > 0) ||
                         (current_model != NULL && strcmp(current_model, selected_model) != 0);
    bool language_changed = (current_language == NULL && strcmp(selected_language, "en") != 0) ||
                            (current_language != NULL && strcmp(current_language, selected_language) != 0);


    if (!model_changed && !language_changed) {
        return; // No changes, exit silently
    }

    // Update preferences with new settings
    preferences_set_string("model", selected_model);
    preferences_set_string("language", selected_language);
    preferences_save();

    // Reload transcription system with new model and language
    reload_model_and_language();
}

static void menu_configure_hotkey(void) {
    KeyCombination combo;
    bool result = dialog_keycombination_capture(
        "Configure Hotkey", "Click in the box below and press your desired key combination:", &combo);

    if (result) {
        // Build display message
        char message[256] = "Hotkey configured:\n";
        for (int i = 0; i < combo.count; i++) {
            char key_info[128];
#ifdef _WIN32
            snprintf(key_info, sizeof(key_info), "Key %d: scancode=0x%02X, extended=%d\n", i + 1, combo.keys[i].code,
                     combo.keys[i].flags);
#else
            // macOS format - show keycode and modifier flags
            snprintf(key_info, sizeof(key_info), "Key %d: keycode=%d, modifiers=0x%X\n", i + 1, combo.keys[i].code,
                     combo.keys[i].flags);
#endif
            strcat(message, key_info);
        }
        dialog_info("Hotkey Configured", message);

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
