#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../audio.h"
#include "../transcription.h"
#include "../menubar.h"

// Function prototypes
int keylogger_init(void);
void keylogger_cleanup(void);
bool keylogger_is_fn_pressed(void);
void clipboard_paste_text(const char* text);

// Include overlay functions
#include "../overlay.h"

// Global variables
static bool g_running = true;
static AudioRecorder* g_recorder = NULL;
static void* g_whisper_ctx = NULL;  // Using void* since TranscriptionContext is not defined

void cleanup() {
    if (g_recorder) {
        if (audio_recorder_is_recording(g_recorder)) {
            audio_recorder_stop(g_recorder);
        }
        audio_recorder_destroy(g_recorder);
        g_recorder = NULL;
    }
    
    if (g_whisper_ctx) {
        transcription_cleanup();
        g_whisper_ctx = NULL;
    }
    
    keylogger_cleanup();
    menubar_cleanup();
}

void process_recording() {
    overlay_show("Transcribing");
    
    size_t audio_size;
    const float* audio_data = audio_recorder_get_data(g_recorder, &audio_size);
    
    if (!audio_data || audio_size == 0) {
        overlay_hide();
        return;
    }
    
    // Transcribe
    char result[1024];
    if (transcribe_audio(audio_data, (int)audio_size, result, sizeof(result)) == 0) {
        if (strlen(result) > 0) {
            overlay_show_result(result);
            clipboard_paste_text(result);
        } else {
            overlay_hide();
        }
    } else {
        overlay_hide();
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Hide console window for GUI app
    FreeConsole();
    
    // Initialize system tray
    if (menubar_init() != 0) {
        MessageBoxW(NULL, L"Failed to create system tray icon", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Request audio permissions (no-op on Windows)
    if (audio_request_permissions() != 0) {
        MessageBoxW(NULL, L"Failed to get audio permissions", L"Error", MB_OK | MB_ICONERROR);
        cleanup();
        return 1;
    }
    
    // Initialize whisper
    if (transcription_init("models/ggml-base.en.bin") != 0) {
        MessageBoxW(NULL, L"Failed to load Whisper model\nPlease ensure the model file is in the 'models' directory next to the executable.", L"Error", MB_OK | MB_ICONERROR);
        cleanup();
        return 1;
    }
    g_whisper_ctx = (void*)1;  // Non-null to indicate initialized
    
    // Create audio recorder
    g_recorder = audio_recorder_create(&WHISPER_AUDIO_CONFIG);
    if (!g_recorder) {
        MessageBoxW(NULL, L"Failed to create audio recorder", L"Error", MB_OK | MB_ICONERROR);
        cleanup();
        return 1;
    }
    
    // Initialize keylogger
    if (keylogger_init() != 0) {
        MessageBoxW(NULL, 
                 L"Failed to initialize keyboard monitoring.\nMake sure to run as administrator.", 
                 L"Error", MB_OK | MB_ICONERROR);
        cleanup();
        return 1;
    }
    
    bool was_recording = false;
    
    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) {
            break;
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        // Check key state
        bool is_key_pressed = keylogger_is_fn_pressed();
        
        if (is_key_pressed && !was_recording) {
            // Start recording
            overlay_show("Recording");
            audio_recorder_start_buffer(g_recorder);
            was_recording = true;
        } else if (!is_key_pressed && was_recording) {
            // Stop recording and process
            audio_recorder_stop(g_recorder);
            double duration = audio_recorder_get_duration(g_recorder);
            was_recording = false;
            
            if (duration > 0.1) {  // Process only if recording is long enough
                process_recording();
            } else {
                overlay_hide();
            }
        }
    }
    
    cleanup();
    return 0;
}