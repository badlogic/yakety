#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "app.h"
#include "logging.h"
#include "audio.h"
#include "transcription.h"
#include "keylogger.h"
#include "clipboard.h"
#include "overlay.h"
#include "utils.h"

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

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
    app_quit();
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
        
        log_info("⏹️  Stopping recording...");
        audio_recorder_stop(state->recorder);
        
        // Get recorded audio
        int sample_count = 0;
        float* samples = audio_recorder_get_samples(state->recorder, &sample_count);
        
        if (samples && sample_count > 0) {
            log_info("🧠 Transcribing...");
            overlay_show("Transcribing");
            
            char* text = transcription_process(samples, sample_count, 16000);
            overlay_hide();
            
            if (text && strlen(text) > 0) {
                // Process the transcription
                char* trimmed = text;
                while (*trimmed == ' ') trimmed++;
                
                if (strlen(trimmed) > 0) {
                    // Add space and copy to clipboard
                    size_t len = strlen(trimmed) + 2;
                    char* with_space = malloc(len);
                    snprintf(with_space, len, "%s ", trimmed);
                    
                    clipboard_copy(with_space);
                    utils_sleep_ms(100);
                    clipboard_paste();
                    
                    log_info("📝 \"%s\"", trimmed);
                    log_info("✅ Text pasted!");
                    
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

static void menu_about(void) {
    dialog_info("About Yakety", 
        "Yakety v1.0\n"
        "Voice-to-text input for any application\n\n"
        "Hold FN key (or Right Ctrl on Windows) to record,\n"
        "release to transcribe and paste.\n\n"
        "© 2024 Yakety Contributors");
}

static void menu_licenses(void) {
    dialog_info("Licenses",
        "Yakety is open source software.\n\n"
        "This software includes:\n"
        "- Whisper.cpp (MIT License)\n"
        "- miniaudio (Public Domain)\n\n"
        "See LICENSES.md for full details.");
}

static void menu_quit(void) {
    g_running = false;
    app_quit();
}
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize app
    AppConfig config = {
        .name = "Yakety",
        .version = "1.0",
        #ifdef YAKETY_TRAY_APP
        .is_console = false
        #else
        .is_console = true
        #endif
    };
    
    if (app_init(&config) != 0) {
        fprintf(stderr, "Failed to initialize app\n");
        return 1;
    }
    
    // Initialize overlay
    overlay_init();
    
    // Initialize transcription
    if (transcription_init() != 0) {
        log_error("Failed to initialize transcription");
        overlay_cleanup();
        app_cleanup();
        return 1;
    }
    
    // Create audio recorder
    AudioConfig audio_config = {
        .sample_rate = 16000,
        .channels = 1,
        .bits_per_sample = 16
    };
    
    AppState state = {
        .recorder = audio_recorder_create(&audio_config),
        .recording = false,
        .recording_start_time = 0
    };
    
    if (!state.recorder) {
        log_error("Failed to create audio recorder");
        transcription_cleanup();
        overlay_cleanup();
        app_cleanup();
        return 1;
    }
    
    g_state = &state;
    
    #ifdef YAKETY_TRAY_APP
    // Create and show menu
    g_menu = menu_create();
    if (!g_menu) {
        log_error("Failed to create menu");
        audio_recorder_destroy(state.recorder);
        transcription_cleanup();
        overlay_cleanup();
        app_cleanup();
        return 1;
    }
    
    menu_add_item(g_menu, "About Yakety", menu_about);
    menu_add_item(g_menu, "Licenses", menu_licenses);
    menu_add_separator(g_menu);
    menu_add_item(g_menu, "Quit", menu_quit);
    
    if (menu_show(g_menu) != 0) {
        log_error("Failed to show menu");
        menu_destroy(g_menu);
        audio_recorder_destroy(state.recorder);
        transcription_cleanup();
        overlay_cleanup();
        app_cleanup();
        return 1;
    }
    #endif
    
    // Initialize keylogger
    if (keylogger_init(on_key_press, on_key_release, &state) != 0) {
        log_error("Failed to initialize keyboard monitoring");
        #ifdef YAKETY_TRAY_APP
        if (g_menu) {
            menu_destroy(g_menu);
        }
        #endif
        audio_recorder_destroy(state.recorder);
        transcription_cleanup();
        overlay_cleanup();
        app_cleanup();
        return 1;
    }
    
    log_info("Yakety is running. Press and hold FN to record.");
    
    // Run the app
    while (g_running) {
        app_run();
    }
    
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
    
    return 0;
}