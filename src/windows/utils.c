#include "../utils.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

// High-resolution timer frequency
static LARGE_INTEGER g_frequency = {0};
static bool g_timer_initialized = false;

// Initialize high-resolution timer
static void init_timer(void) {
    if (!g_timer_initialized) {
        QueryPerformanceFrequency(&g_frequency);
        g_timer_initialized = true;
    }
}

double utils_get_time(void) {
    init_timer();
    
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    
    // Return time in seconds as a double
    return (double)counter.QuadPart / (double)g_frequency.QuadPart;
}

void utils_sleep_ms(int milliseconds) {
    Sleep(milliseconds);
}

const char* utils_get_model_path(void) {
    static char model_path[MAX_PATH] = {0};
    
    // If already calculated, return cached path
    if (model_path[0] != '\0') {
        return model_path;
    }
    
    // Get the executable directory
    if (GetModuleFileNameA(NULL, model_path, MAX_PATH) == 0) {
        return NULL;
    }
    
    // Find the last backslash to get directory
    char* last_slash = strrchr(model_path, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
    
    // Check for model in executable directory first
    char test_path[MAX_PATH];
    snprintf(test_path, MAX_PATH, "%s\\ggml-tiny.bin", model_path);
    
    if (GetFileAttributesA(test_path) != INVALID_FILE_ATTRIBUTES) {
        return model_path;
    }
    
    // Try parent directory (for development builds)
    snprintf(test_path, MAX_PATH, "%s\\..\\ggml-tiny.bin", model_path);
    
    if (GetFileAttributesA(test_path) != INVALID_FILE_ATTRIBUTES) {
        // Normalize the path
        char full_path[MAX_PATH];
        if (GetFullPathNameA(test_path, MAX_PATH, full_path, NULL)) {
            // Remove the filename part to get just the directory
            last_slash = strrchr(full_path, '\\');
            if (last_slash) {
                *last_slash = '\0';
            }
            strcpy(model_path, full_path);
            return model_path;
        }
    }
    
    // Try whisper.cpp subdirectory
    snprintf(test_path, MAX_PATH, "%s\\whisper.cpp\\models\\ggml-tiny.bin", model_path);
    
    if (GetFileAttributesA(test_path) != INVALID_FILE_ATTRIBUTES) {
        snprintf(model_path, MAX_PATH, "%s\\whisper.cpp\\models", model_path);
        return model_path;
    }
    
    // Default to current directory
    return ".";