
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>

#include "app.h"
#include "logging.h"
#include "audio.h"
#include "transcription.h"
#include "keylogger.h"
#include "clipboard.h"
#include "overlay.h"
#include "utils.h"
#include "preferences.h"
#include "menu.h"

#ifdef YAKETY_TRAY_APP
#include "dialog.h"
#endif


typedef struct {
    bool recording;
    double recording_start_time;
} AppState;

static AppState* g_state = NULL;

// Forward declarations
static void continue_app_initialization(void);
static void on_model_loaded(void* result);
static bool g_is_fallback_attempt = false;

// Callback data structures
typedef struct {
    bool is_error;
} DelayedRetryData;

static void signal_handler(int sig) {
    (void)sig;
    app_quit();
}

// Async model loading work function
static void* load_model_async(void* arg) {
    (void)arg;  // No longer need config parameter

    log_info("Loading Whisper model in background thread...");

    const char* model_path = utils_get_model_path();
    if (!model_path) {
        log_error("Could not find Whisper model file");
        return (void*)0; // Return 0 for failure
    }

    if (transcription_init(model_path) != 0) {
        log_error("Failed to initialize transcription");
        return (void*)0; // Return 0 for failure
    }

    // Set language from preferences
    const char* language = preferences_get_string("language");
    if (language) {
        transcription_set_language(language);
    } else {
        transcription_set_language("en"); // Default to English
    }

    log_info("Model loaded successfully");
    return (void*)1; // Return 1 for success
}

// Callback for delayed retry
static void delayed_retry_callback(void* arg) {
    (void)arg;
    overlay_show("Loading base model");
    utils_execute_async(load_model_async, NULL, on_model_loaded);
}

// Callback for delayed quit
static void delayed_quit_callback(void* arg) {
    (void)arg;
    overlay_hide();
    app_quit();
}

// Called when model loading completes
static void on_model_loaded(void* result) {
    bool success = (intptr_t)result != 0;

    log_info("Model loading completed at %.3f seconds (success=%d)", utils_now(), success);

    if (!success) {
        if (!g_is_fallback_attempt) {
            // First failure - try fallback to base model
            const char* failed_model = preferences_get_string("model");
            char fallback_msg[256];

            if (failed_model && strlen(failed_model) > 0) {
                // Extract filename for message
                const char* filename = strrchr(failed_model, '/');
                if (!filename) filename = strrchr(failed_model, '\\');
                if (filename) filename++; else filename = failed_model;
                snprintf(fallback_msg, sizeof(fallback_msg),
                        "Failed to load %s, falling back to base model", filename);
            } else {
                snprintf(fallback_msg, sizeof(fallback_msg),
                        "Failed to load model, falling back to base model");
            }

            // Show fallback message
            overlay_show_error(fallback_msg);

            // Clear the model from preferences to use default search
            preferences_set_string("model", "");
            preferences_save();

            // Set fallback flag and try again after delay
            g_is_fallback_attempt = true;
            utils_execute_main_thread(3000, delayed_retry_callback, NULL);
            return;
        } else {
            // Fallback also failed - show error and quit
            const char* model_path = utils_get_model_path();
            char error_msg[256];
            if (model_path) {
                const char* filename = strrchr(model_path, '/');
                if (!filename) filename = strrchr(model_path, '\\');
                if (filename) filename++; else filename = model_path;
                snprintf(error_msg, sizeof(error_msg), "Failed to load %s", filename);
            } else {
                snprintf(error_msg, sizeof(error_msg), "Model not found");
            }

            overlay_show_error(error_msg);
            utils_execute_main_thread(3000, delayed_quit_callback, NULL);
            return;
        }
    }

    // Hide loading overlay for success case
    overlay_hide();

    // Continue with rest of initialization
    continue_app_initialization();
}

static void on_key_press(void* userdata) {
    AppState* state = (AppState*)userdata;

    if (!state->recording) {
        state->recording = true;
        state->recording_start_time = utils_get_time();

        if (audio_recorder_start() == 0) {
            overlay_show("Recording");
        } else {
            log_error("Failed to start recording");
            state->recording = false;
        }
    }
}

static void on_key_release(void* userdata) {
    AppState* state = (AppState*)userdata;

    if (state->recording) {
        state->recording = false;
        double duration = utils_get_time() - state->recording_start_time;

        // Minimum recording duration check
        if (duration < 0.1) {
            log_info("⚠️  Recording too brief (%.2f seconds), ignoring", duration);
            audio_recorder_stop();
            overlay_hide();
            return;
        }

        log_info("🔴 Recorded for %.2f seconds", duration);
        double stop_start = utils_now();
        audio_recorder_stop();
        double stop_duration = utils_now() - stop_start;
        log_info("⏱️  Audio stop took: %.0f ms", stop_duration * 1000.0);

        // Get recorded audio
        double get_samples_start = utils_now();
        int sample_count = 0;
        float* samples = audio_recorder_get_samples(&sample_count);
        double get_samples_duration = utils_now() - get_samples_start;
        log_info("⏱️  Getting audio samples took: %.0f ms (%d samples)", get_samples_duration * 1000.0, sample_count);

        if (samples && sample_count > 0) {
            log_info("🧠 Starting transcription of %.2f seconds of audio...", (float)sample_count / 16000.0f);
            overlay_show("Transcribing");

            double transcribe_start = utils_now();
            char* text = transcription_process(samples, sample_count, 16000);
            double transcribe_duration = utils_now() - transcribe_start;
            overlay_hide();
            log_info("⏱️  Full transcription pipeline took: %.0f ms", transcribe_duration * 1000.0);

            if (text && strlen(text) > 0) {
                // Text is already cleaned and has trailing space from transcription_process
                double clipboard_start = utils_now();
                clipboard_copy(text);
                clipboard_paste();
                double clipboard_duration = utils_now() - clipboard_start;

                log_info("📝 \"%s\"", text);
                log_info("✅ Text pasted! (clipboard operations took %.0f ms)", clipboard_duration * 1000.0);

                double total_time = utils_now() - stop_start;
                log_info("⏱️  Total time from stop to paste: %.0f ms", total_time * 1000.0);

                free(text);
            } else {
                log_info("⚠️  No speech detected");
                if (text) free(text);
            }

            free(samples);
        }
    }
}




// Called when app is ready - for both CLI and tray apps
static void on_app_ready(void) {
    log_info("on_app_ready called - starting model loading (%.0f ms since app start)", utils_now() * 1000.0);

    // Show loading overlay
    if (preferences_get_bool("show_notifications", true)) {
        overlay_show("Loading model");
    }

    // Start loading model asynchronously
    utils_execute_async(load_model_async, NULL, on_model_loaded);
}

// Continue with app initialization after model loads
static void continue_app_initialization(void) {

    // Initialize menu system for tray apps
    if (!app_is_console()) {
        if (menu_init() != 0) {
            log_error("Failed to create menu");
            app_quit();
            return;
        }

        if (menu_show() != 0) {
            log_error("Failed to show menu");
            menu_cleanup();
            app_quit();
            return;
        }

        log_info("Menu created successfully");
    }

    // Initialize keylogger for all apps (after run loop is active)
    bool keylogger_started = false;
    while (!keylogger_started) {
        if (keylogger_init(on_key_press, on_key_release, g_state) == 0) {
            keylogger_started = true;
            log_info("✅ Keylogger started successfully");

            // Load saved hotkey from preferences
            {
                KeyCombination combo;
                if (preferences_load_key_combination(&combo)) {
                    keylogger_set_combination(&combo);
                } else {
                    // Use default
                    KeyCombination default_combo = keylogger_get_fn_combination();
                    keylogger_set_combination(&default_combo);
                    #ifdef _WIN32
                    log_info("Using default Right Ctrl hotkey");
                    #else
                    log_info("Using default FN key hotkey");
                    #endif
                }
            }
        } else {
            #ifdef __APPLE__
            #ifdef YAKETY_TRAY_APP
            // Need accessibility permission on macOS
            int response = dialog_accessibility_permission();
            if (response == 0) {
                // User says they granted permission - retry
                utils_sleep_ms(500);
                continue;
            } else if (response == 1) {
                // Open System Preferences and wait
                utils_open_accessibility_settings();
                if (dialog_wait_for_permission()) {
                    utils_sleep_ms(500);
                    continue;
                } else {
                    // User chose to quit
                    app_quit();
                    return;
                }
            } else {
                // User chose to quit
                app_quit();
                return;
            }
            #else
            // CLI app - just log and exit
            log_error("Failed to initialize keyboard monitoring");
            log_error("Please grant accessibility permission in System Preferences → Security & Privacy → Privacy → Accessibility");
            app_quit();
            return;
            #endif
            #else
            // Non-macOS platforms
            log_error("Failed to initialize keyboard monitoring");
            app_quit();
            return;
            #endif
        }
    }

    #ifdef _WIN32
    log_info("Yakety is running. Press and hold Right Ctrl to record.");
    #else
    log_info("Yakety is running. Press and hold FN to record.");
    #endif

    #ifdef YAKETY_TRAY_APP
    // Check for first run and offer auto-launch
    if (preferences_get_bool("first_run", true)) {
        preferences_set_bool("first_run", false);
        preferences_save();

        if (dialog_confirm("Welcome to Yakety",
                          "Would you like Yakety to start automatically when you log in?")) {
            if (utils_set_launch_at_login(true)) {
                dialog_info("Launch Settings", "Yakety will now start automatically when you log in.");
            }
        }
    }
    #endif
}

static const char* parse_cli_args(int argc, char** argv) {
    if (argc <= 1) {
        return NULL;
    }

    // Check for help
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s [model_path | --model <path>]\n", argv[0]);
        printf("Options:\n");
        printf("  model_path        Direct path to Whisper model file\n");
        printf("  --model <path>    Use a specific Whisper model file\n");
        printf("  -h, --help        Show this help message\n");
        exit(0);
    }

    // Check for --model flag
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--model") == 0) {
            return argv[i + 1];
        }
    }

    // If no --model flag and first arg doesn't start with -, treat it as model path
    if (argv[1][0] != '-') {
        return argv[1];
    }

    return NULL;
}

int app_main(int argc, char** argv, bool is_console) {
    const char* custom_model_path = NULL;
    // Parse command line arguments for CLI version
    if (is_console) {
        custom_model_path = parse_cli_args(argc, argv);
    } else {
        (void)argc;
        (void)argv;
    }

    // Initialize logging system
    log_init();
    log_info("=== Yakety startup timing ===");
    log_info("App started at %.3f seconds", utils_now());

    // Initialize preferences
    if (!preferences_init()) {
        fprintf(stderr, "Failed to initialize preferences\n");
        log_cleanup();
        return 1;
    }

    if (custom_model_path) {
        log_info("Using custom model path: %s", custom_model_path);
        preferences_set_string("model", custom_model_path);
    }

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize app
    if (app_init("Yakety", "1.0", is_console, on_app_ready) != 0) {
        fprintf(stderr, "Failed to initialize app\n");
        return 1;
    }

    // Initialize overlay
    overlay_init();

    // Initialize audio recorder
    if (!audio_recorder_init()) {
        log_error("Failed to initialize audio recorder");
        overlay_cleanup();
        app_cleanup();
        preferences_cleanup();
        log_cleanup();
        return 1;
    }

    AppState state = {0};
    g_state = &state;

    log_info("Starting app_run() at %.3f seconds", utils_now());

    // Run the app
    app_run();

    // Cleanup
    keylogger_cleanup();
    if (!app_is_console()) {
        menu_cleanup();
    }
    audio_recorder_cleanup();
    transcription_cleanup();
    overlay_cleanup();
    app_cleanup();
    preferences_cleanup();
    log_cleanup();

    return 0;
}

APP_ENTRY_POINT

