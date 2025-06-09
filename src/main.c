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
#include "config.h"

#ifdef YAKETY_TRAY_APP
#include "menu.h"
#include "dialog.h"
#endif


typedef struct {
    AudioRecorder* recorder;
    bool recording;
    double recording_start_time;
} AppState;

static AppState* g_state = NULL;
static volatile bool g_running = true;
static Config* g_config = NULL;
static AudioRecorder* g_recorder = NULL;

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
    g_running = false;
    app_quit();
}

// Async model loading work function
static void* load_model_async(void* arg) {
    Config* config = (Config*)arg;

    log_info("Loading Whisper model in background thread...");

    const char* model_path = utils_get_model_path_with_config(config);
    if (!model_path) {
        log_error("Could not find Whisper model file");
        return (void*)0; // Return 0 for failure
    }

    if (transcription_init(model_path) != 0) {
        log_error("Failed to initialize transcription");
        return (void*)0; // Return 0 for failure
    }
    
    // Set language from config
    const char* language = config_get_string(config, "language");
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
    utils_async_execute(load_model_async, g_config, on_model_loaded);
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
            const char* failed_model = config_get_string(g_config, "model");
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

            // Clear the model from config to use default search
            config_set_string(g_config, "model", "");
            config_save(g_config);

            // Set fallback flag and try again after delay
            g_is_fallback_attempt = true;
            utils_delay_on_main_thread(3000, delayed_retry_callback, NULL);
            return;
        } else {
            // Fallback also failed - show error and quit
            const char* model_path = utils_get_model_path_with_config(g_config);
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
            utils_delay_on_main_thread(3000, delayed_quit_callback, NULL);
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

        if (audio_recorder_start(state->recorder) == 0) {
            log_info("🔴 Recording...");
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
            audio_recorder_stop(state->recorder);
            overlay_hide();
            return;
        }

        log_info("⏹️  Stopping recording after %.2f seconds", duration);
        double stop_start = utils_now();
        audio_recorder_stop(state->recorder);
        double stop_duration = utils_now() - stop_start;
        log_info("⏱️  Audio stop took: %.0f ms", stop_duration * 1000.0);

        // Get recorded audio
        double get_samples_start = utils_now();
        int sample_count = 0;
        float* samples = audio_recorder_get_samples(state->recorder, &sample_count);
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
                // Process the transcription
                char* trimmed = text;
                while (*trimmed == ' ') trimmed++;

                if (strlen(trimmed) > 0) {
                    // Add space and copy to clipboard
                    size_t len = strlen(trimmed) + 2;
                    char* with_space = malloc(len);
                    snprintf(with_space, len, "%s ", trimmed);

                    double clipboard_start = utils_now();
                    clipboard_copy(with_space);
                    clipboard_paste();
                    double clipboard_duration = utils_now() - clipboard_start;

                    log_info("📝 \"%s\"", trimmed);
                    log_info("✅ Text pasted! (clipboard operations took %.0f ms)", clipboard_duration * 1000.0);
                    
                    double total_time = utils_now() - stop_start;
                    log_info("⏱️  Total time from stop to paste: %.0f ms", total_time * 1000.0);

                    free(with_space);
                } else {
                    log_info("⚠️  No speech detected");
                }

                free(text);
            } else {
                log_info("⚠️  No speech detected");
            }

            free(samples);
        }
    }
}

#ifdef YAKETY_TRAY_APP
static MenuSystem* g_menu = NULL;
static int g_launch_menu_index = -1;

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
    g_running = false;
    app_quit();
}
#endif

// Called when app is ready - for both CLI and tray apps
static void on_app_ready(void) {
    log_info("on_app_ready called - starting model loading (%.0f ms since app start)", utils_now() * 1000.0);
    
    // Show loading overlay
    if (config_get_bool(g_config, "show_notifications", true)) {
        overlay_show("Loading model");
    }

    // Start loading model asynchronously
    utils_async_execute(load_model_async, g_config, on_model_loaded);
}

// Continue with app initialization after model loads
static void continue_app_initialization(void) {

    #ifdef YAKETY_TRAY_APP
    // Create and show menu for tray apps
    g_menu = menu_create();
    if (!g_menu) {
        log_error("Failed to create menu");
        app_quit();
        return;
    }

    menu_add_item(g_menu, "About Yakety", menu_about);
    menu_add_item(g_menu, "Licenses", menu_licenses);
    menu_add_separator(g_menu);

    // Add launch at login toggle and track its index
    const char* launch_label = utils_is_launch_at_login_enabled()
        ? "Disable Launch at Login"
        : "Enable Launch at Login";
    g_launch_menu_index = g_menu->item_count;  // Store the index before adding
    menu_add_item(g_menu, launch_label, menu_toggle_launch_at_login);

    menu_add_separator(g_menu);
    menu_add_item(g_menu, "Quit", menu_quit);

    if (menu_show(g_menu) != 0) {
        log_error("Failed to show menu");
        menu_destroy(g_menu);
        g_menu = NULL;
        app_quit();
        return;
    }

    log_info("Menu created successfully");
    #endif

    // Initialize keylogger for all apps (after run loop is active)
    bool keylogger_started = false;
    while (!keylogger_started) {
        if (keylogger_init(on_key_press, on_key_release, g_state) == 0) {
            keylogger_started = true;
            log_info("✅ Keylogger started successfully");
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
    if (config_get_bool(g_config, "first_run", true)) {
        config_set_bool(g_config, "first_run", false);
        config_save(g_config);

        if (dialog_confirm("Welcome to Yakety",
                          "Would you like Yakety to start automatically when you log in?")) {
            if (utils_set_launch_at_login(true)) {
                dialog_info("Launch Settings", "Yakety will now start automatically when you log in.");
            }
        }
    }
    #endif
}

#ifndef YAKETY_TRAY_APP
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
#endif

#ifdef _WIN32
#ifdef YAKETY_TRAY_APP
int WINAPI APP_MAIN(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
#else
int APP_MAIN(int argc, char** argv) {
#endif
#else
int APP_MAIN(int argc, char** argv) {
#endif

    // Parse command line arguments for CLI version
    #ifndef YAKETY_TRAY_APP
    const char* custom_model_path = parse_cli_args(argc, argv);
    #else
    const char* custom_model_path = NULL;
        #ifndef _WIN32
        (void)argc;
        (void)argv;
        #endif
    #endif

    // Initialize logging system
    log_init();
    log_info("=== Yakety startup timing ===");
    log_info("App started at %.3f seconds", utils_now());

    // Load configuration
    g_config = config_create();
    if (!g_config) {
        fprintf(stderr, "Failed to load configuration\n");
        log_cleanup();
        return 1;
    }
    
    #ifndef YAKETY_TRAY_APP
    // For CLI version, override model path if provided
    if (custom_model_path) {
        log_info("Using custom model path: %s", custom_model_path);
        config_set_string(g_config, "model", custom_model_path);
    }
    #endif

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize app
    AppConfig config = {
        .name = "Yakety",
        .version = "1.0",
        #ifdef YAKETY_TRAY_APP
        .is_console = false,
        #else
        .is_console = true,
        #endif
        .on_ready = on_app_ready
    };

    if (app_init(&config) != 0) {
        fprintf(stderr, "Failed to initialize app\n");
        return 1;
    }

    // Initialize overlay
    overlay_init();

    // Create audio recorder
    AudioConfig audio_config = {
        .sample_rate = 16000,
        .channels = 1,
        .bits_per_sample = 16
    };

    g_recorder = audio_recorder_create(&audio_config);
    if (!g_recorder) {
        log_error("Failed to create audio recorder");
        overlay_cleanup();
        app_cleanup();
        config_destroy(g_config);
        log_cleanup();
        return 1;
    }

    AppState state = {
        .recorder = g_recorder,
        .recording = false,
        .recording_start_time = 0
    };

    g_state = &state;

    log_info("Starting app_run() at %.3f seconds", utils_now());
    
    // Run the app
    app_run();

    // Cleanup
    keylogger_cleanup();
    #ifdef YAKETY_TRAY_APP
    if (g_menu) {
        menu_destroy(g_menu);
    }
    #endif
    audio_recorder_destroy(state.recorder);
    transcription_cleanup();
    overlay_cleanup();
    app_cleanup();
    config_destroy(g_config);
    log_cleanup();

    return 0;
}

