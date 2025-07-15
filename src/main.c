
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "app.h"
#include "audio.h"
#include "clipboard.h"
#include "keylogger.h"
#include "logging.h"
#include "menu.h"
#include "models.h"
#include "overlay.h"
#include "preferences.h"
#include "transcription.h"
#include "utils.h"


#include "dialog.h"

// Constants
#define PERMISSION_RETRY_DELAY_MS 500
#define MIN_RECORDING_DURATION 0.1

typedef struct {
    bool recording;
    double recording_start_time;
} AppState;

static AppState *g_state = NULL;

// Forward declarations
static void on_key_press(void *userdata);
static void on_key_release(void *userdata);
static void on_key_cancel(void *userdata);

static void signal_handler(int sig) {
    (void) sig;
    app_quit();
}

// Model loading with unified system
static bool load_model_with_fallback(void) {
    if (models_load() == 0) {
        return true; // Success
    } else {
        app_quit(); // models_load handles all error display
        return false;
    }
}

// Setup menu system for tray apps
static bool setup_menu_if_needed(void) {
    if (app_is_console()) {
        return true; // No menu needed for console apps
    }

    if (menu_init() != 0) {
        log_error("Failed to create menu");
        app_quit();
        return false;
    }

    if (menu_setup_items(menu_get_system()) != 0) {
        log_error("Failed to setup menu items");
        menu_cleanup();
        app_quit();
        return false;
    }

    if (menu_show() != 0) {
        log_error("Failed to show menu");
        menu_cleanup();
        app_quit();
        return false;
    }

    log_info("Menu created successfully");
    return true;
}

// Handle keylogger permission issues on macOS
static bool handle_keylogger_permissions(void) {
#ifdef __APPLE__
    if (!app_is_console()) {
        // Need accessibility permission on macOS tray apps
        int response = dialog_accessibility_permission();
        if (response == 0) {
            // User says they granted permission - retry
            app_sleep_responsive(PERMISSION_RETRY_DELAY_MS);
            return true; // Should retry
        } else if (response == 1) {
            // Open System Preferences and wait
            utils_open_accessibility_settings();
            if (dialog_wait_for_permission()) {
                app_sleep_responsive(PERMISSION_RETRY_DELAY_MS);
                return true; // Should retry
            } else {
                // User chose to quit
                app_quit();
                return false;
            }
        } else {
            // User chose to quit
            app_quit();
            return false;
        }
    } else {
        // CLI app - just log and exit
        log_error("Failed to initialize keyboard monitoring");
        log_error("Please grant accessibility permission in System Preferences → Security & Privacy → Privacy → "
                  "Accessibility");
        app_quit();
        return false;
    }
#else
    // Non-macOS platforms
    log_error("Failed to initialize keyboard monitoring");
    app_quit();
    return false;
#endif
}

// Setup keylogger with permission handling
static bool setup_keylogger(void) {
    while (true) {
        if (keylogger_init(on_key_press, on_key_release, on_key_cancel, g_state) == 0) {
            log_info("✅ Keylogger started successfully");

            // Load saved hotkey from preferences
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
            return true;
        } else {
            // Keylogger init failed - handle permissions
            if (!handle_keylogger_permissions()) {
                return false; // User quit or error
            }
            // Continue loop to retry
        }
    }
}

// Handle first run dialog for tray apps
static void handle_first_run(void) {
    if (app_is_console()) {
        return; // No first run dialog for console apps
    }

    if (preferences_get_bool("first_run", true)) {
        preferences_set_bool("first_run", false);
        preferences_save();

        if (dialog_confirm("Welcome to Yakety", "Would you like Yakety to start automatically when you log in?")) {
            if (utils_set_launch_at_login(true)) {
                dialog_info("Launch Settings", "Yakety will now start automatically when you log in.");
            }
        }
    }
}

// Process recorded audio - extract from on_key_release
static void process_recorded_audio(double duration) {
    log_info("🔴 Recorded for %.2f seconds", duration);
    double stop_start = utils_now();
    audio_recorder_stop();
    double stop_duration = utils_now() - stop_start;
    log_info("⏱️  Audio stop took: %.0f ms", stop_duration * 1000.0);

    // Get recorded audio
    double get_samples_start = utils_now();
    int sample_count = 0;
    float *samples = audio_recorder_get_samples(&sample_count);
    double get_samples_duration = utils_now() - get_samples_start;
    log_info("⏱️  Getting audio samples took: %.0f ms (%d samples)", get_samples_duration * 1000.0, sample_count);

    if (samples && sample_count > 0) {
        log_info("🧠 Starting transcription of %.2f seconds of audio...", (float) sample_count / 16000.0f);
        overlay_show("Transcribing");

        double transcribe_start = utils_now();
        char *text = transcription_process(samples, sample_count, 16000);
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
            if (text)
                free(text);
        }

        free(samples);
    }
}

static void on_key_press(void *userdata) {
    AppState *state = (AppState *) userdata;

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

static void on_key_release(void *userdata) {
    AppState *state = (AppState *) userdata;

    if (state->recording) {
        state->recording = false;
        double duration = utils_get_time() - state->recording_start_time;

        // Minimum recording duration check
        if (duration < MIN_RECORDING_DURATION) {
            log_info("⚠️  Recording too brief (%.2f seconds), ignoring", duration);
            audio_recorder_stop();
            overlay_hide();
            return;
        }

        // Process the recorded audio
        process_recorded_audio(duration);
    }
}

static void on_key_cancel(void *userdata) {
    AppState *state = (AppState *) userdata;

    if (state->recording) {
        state->recording = false;
        log_info("❌ Recording cancelled - additional key pressed");
        
        // Stop recording and clean up
        audio_recorder_stop();
        overlay_hide();
        
        // No transcription or text insertion
    }
}

// Called when app is ready - for both CLI and tray apps
static void on_app_ready(void) {
    log_info("on_app_ready called - starting initialization (%.0f ms since app start)", utils_now() * 1000.0);

    // Step 1: Load model with fallback
    if (!load_model_with_fallback()) {
        return; // Model loading failed and quit was called
    }

    // Step 2: Setup menu system
    if (!setup_menu_if_needed()) {
        return; // Menu setup failed and quit was called
    }

    // Step 3: Setup keylogger with permission handling
    if (!setup_keylogger()) {
        return; // Keylogger setup failed and quit was called
    }

// Step 4: Log startup completion
#ifdef _WIN32
    log_info("Yakety is running. Press and hold Right Ctrl to record.");
#else
    log_info("Yakety is running. Press and hold FN to record.");
#endif

    // Step 5: Handle first run dialog
    handle_first_run();

    log_info("App initialization completed successfully");
}

static const char *parse_cli_args(int argc, char **argv) {
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

// Cleanup all modules in proper order
static void cleanup_all(void) {
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
}

int app_main(int argc, char **argv, bool is_console) {
    const char *custom_model_path = NULL;
    // Parse command line arguments for CLI version
    if (is_console) {
        custom_model_path = parse_cli_args(argc, argv);
    } else {
        (void) argc;
        (void) argv;
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
    log_info("Initializing overlay");
    overlay_init();

    // Initialize audio recorder
    if (!audio_recorder_init()) {
        log_error("Failed to initialize audio recorder");
        cleanup_all();
        return 1;
    }

    AppState state = {0};
    g_state = &state;

    log_info("Starting app_run() at %.3f seconds", utils_now());

    // Run the app
    app_run();

    // Cleanup
    cleanup_all();
    return 0;
}

APP_ENTRY_POINT
