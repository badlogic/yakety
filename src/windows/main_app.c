#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../audio.h"
#include "../transcription.h"

// Function prototypes
int keylogger_init(void);
void keylogger_cleanup(void);
bool keylogger_is_fn_pressed(void);
void overlay_show_recording(void);
void overlay_show_processing(void);
void overlay_show_result(const char* text);
void overlay_hide(void);
void clipboard_paste_text(const char* text);
int menubar_init(void);
void menubar_cleanup(void);

// Global variables
static bool g_running = true;
static AudioRecorder* g_recorder = NULL;
static TranscriptionContext* g_whisper_ctx = NULL;

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
    menubar_cleanup();
}

void process_recording() {
    overlay_show_processing();
    
    size_t audio_size;
    const float* audio_data = audio_recorder_get_data(g_recorder, &audio_size);
    
    if (!audio_data || audio_size == 0) {
        overlay_hide();
        return;
    }
    
    // Transcribe
    char* text = transcription_process(g_whisper_ctx, audio_data, audio_size);
    
    if (text && strlen(text) > 0) {
        overlay_show_result(text);
        clipboard_paste_text(text);
        free(text);
    } else {
        overlay_hide();
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Hide console window for GUI app
    FreeConsole();
    
    // Initialize system tray
    if (menubar_init() != 0) {
        MessageBox(NULL, L"Failed to create system tray icon", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Request audio permissions (no-op on Windows)
    if (audio_request_permissions() != 0) {
        MessageBox(NULL, L"Failed to get audio permissions", L"Error", MB_OK | MB_ICONERROR);
        cleanup();
        return 1;
    }
    
    // Initialize whisper
    g_whisper_ctx = transcription_context_init(WHISPER_MODEL_BASE_EN);
    if (!g_whisper_ctx) {
        MessageBox(NULL, L"Failed to load Whisper model", L"Error", MB_OK | MB_ICONERROR);
        cleanup();
        return 1;
    }
    
    // Create audio recorder
    g_recorder = audio_recorder_create(&WHISPER_AUDIO_CONFIG);
    if (!g_recorder) {
        MessageBox(NULL, L"Failed to create audio recorder", L"Error", MB_OK | MB_ICONERROR);
        cleanup();
        return 1;
    }
    
    // Initialize keylogger
    if (keylogger_init() != 0) {
        MessageBox(NULL, 
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
            overlay_show_recording();
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