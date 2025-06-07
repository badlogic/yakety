#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include "audio.h"
#include "transcription.h"

// Global variables
static bool g_running = true;
static AudioRecorder* g_recorder = NULL;
static TranscriptionContext* g_whisper_ctx = NULL;

// Function prototypes
int keylogger_init(void);
void keylogger_cleanup(void);
bool keylogger_is_fn_pressed(void);
void overlay_show_recording(void);
void overlay_show_processing(void);
void overlay_show_result(const char* text);
void overlay_hide(void);
void clipboard_paste_text(const char* text);

// Console control handler for graceful shutdown
BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        printf("\nShutting down...\n");
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

void cleanup() {
    if (g_recorder) {
        if (audio_recorder_is_recording(g_recorder)) {
            audio_recorder_stop(g_recorder);
        }
        audio_recorder_destroy(g_recorder);
        g_recorder = NULL;
    }
    
    if (g_whisper_ctx) {
        transcription_context_free(g_whisper_ctx);
        g_whisper_ctx = NULL;
    }
    
    keylogger_cleanup();
}

void process_recording() {
    printf("Processing audio...\n");
    overlay_show_processing();
    
    size_t audio_size;
    const float* audio_data = audio_recorder_get_data(g_recorder, &audio_size);
    
    if (!audio_data || audio_size == 0) {
        printf("No audio data captured\n");
        overlay_hide();
        return;
    }
    
    // Transcribe
    char* text = transcription_process(g_whisper_ctx, audio_data, audio_size);
    
    if (text && strlen(text) > 0) {
        printf("Transcription: %s\n", text);
        overlay_show_result(text);
        clipboard_paste_text(text);
        free(text);
    } else {
        printf("No transcription result\n");
        overlay_hide();
    }
}

int main(int argc, char* argv[]) {
    printf("Whisperer - Hold Right Ctrl key to record and transcribe speech\n");
    printf("Note: Using Right Ctrl instead of FN key on Windows\n");
    
    // Set up console control handler
    SetConsoleCtrlHandler(console_handler, TRUE);
    
    // Request audio permissions (no-op on Windows)
    if (audio_request_permissions() != 0) {
        fprintf(stderr, "Failed to get audio permissions\n");
        return 1;
    }
    
    // Initialize whisper
    printf("Loading Whisper model...\n");
    g_whisper_ctx = transcription_context_init(WHISPER_MODEL_BASE_EN);
    if (!g_whisper_ctx) {
        fprintf(stderr, "Failed to initialize Whisper\n");
        return 1;
    }
    printf("Whisper model loaded successfully\n");
    
    // Create audio recorder
    g_recorder = audio_recorder_create(&WHISPER_AUDIO_CONFIG);
    if (!g_recorder) {
        fprintf(stderr, "Failed to create audio recorder\n");
        cleanup();
        return 1;
    }
    
    // Initialize keylogger
    if (keylogger_init() != 0) {
        fprintf(stderr, "Failed to initialize keyboard monitoring\n");
        fprintf(stderr, "Make sure to run as administrator on Windows\n");
        cleanup();
        return 1;
    }
    
    printf("Ready! Hold Right Ctrl to record.\n");
    
    bool was_recording = false;
    
    // Main loop
    MSG msg;
    while (g_running) {
        // Process Windows messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        bool is_key_pressed = keylogger_is_fn_pressed();
        
        if (is_key_pressed && !was_recording) {
            // Start recording
            printf("Recording started...\n");
            overlay_show_recording();
            audio_recorder_start_buffer(g_recorder);
            was_recording = true;
        } else if (!is_key_pressed && was_recording) {
            // Stop recording and process
            audio_recorder_stop(g_recorder);
            double duration = audio_recorder_get_duration(g_recorder);
            printf("Recording stopped. Duration: %.2f seconds\n", duration);
            was_recording = false;
            
            if (duration > 0.1) {  // Process only if recording is long enough
                process_recording();
            } else {
                printf("Recording too short, ignoring\n");
                overlay_hide();
            }
        }
        
        Sleep(10);  // Small delay to prevent high CPU usage
    }
    
    cleanup();
    printf("Goodbye!\n");
    return 0;
}