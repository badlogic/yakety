#include "../logging.h"
#include "../utils.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <direct.h>
#include <stdbool.h>

static FILE* g_log_file = NULL;
static CRITICAL_SECTION g_log_mutex;
static bool g_mutex_initialized = false;
static char g_log_path[MAX_PATH] = {0};

static const char* get_log_path(void) {
    if (g_log_path[0] == '\0') {
        char* userprofile = getenv("USERPROFILE");
        if (userprofile) {
            snprintf(g_log_path, MAX_PATH, "%s\\.yakety\\log.txt", userprofile);
        } else {
            strcpy(g_log_path, ".yakety\\log.txt");
        }
    }
    return g_log_path;
}

void log_init(void) {
    // Initialize critical section
    if (!g_mutex_initialized) {
        InitializeCriticalSection(&g_log_mutex);
        g_mutex_initialized = true;
    }
    
    // Ensure .yakety directory exists
    char dir_path[MAX_PATH];
    char* userprofile = getenv("USERPROFILE");
    if (userprofile) {
        snprintf(dir_path, MAX_PATH, "%s\\.yakety", userprofile);
    } else {
        strcpy(dir_path, ".yakety");
    }
    
    // Create directory if it doesn't exist
    utils_ensure_dir_exists(dir_path);
    
    // Open log file (truncate on startup)
    const char* log_path = get_log_path();
    g_log_file = utils_fopen_write(log_path);
    if (g_log_file) {
        // Write session header
        time_t now = time(NULL);
        char time_str[64];
        struct tm tm_info;
        localtime_s(&tm_info, &now);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);
        fprintf(g_log_file, "\n=== Yakety Session Started: %s ===\n", time_str);
        fflush(g_log_file);
    }
}

void log_cleanup(void) {
    if (g_mutex_initialized) {
        EnterCriticalSection(&g_log_mutex);
        
        if (g_log_file) {
            // Write session footer
            time_t now = time(NULL);
            char time_str[64];
            struct tm tm_info;
            localtime_s(&tm_info, &now);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);
            fprintf(g_log_file, "=== Yakety Session Ended: %s ===\n\n", time_str);
            fclose(g_log_file);
            g_log_file = NULL;
        }
        
        LeaveCriticalSection(&g_log_mutex);
        DeleteCriticalSection(&g_log_mutex);
        g_mutex_initialized = false;
    }
}

static void log_to_file(const char* level, const char* format, va_list args) {
    if (!g_log_file || !g_mutex_initialized) return;
    
    EnterCriticalSection(&g_log_mutex);
    
    // Write timestamp
    time_t now = time(NULL);
    char time_str[64];
    struct tm tm_info;
    localtime_s(&tm_info, &now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);
    fprintf(g_log_file, "[%s] %s: ", time_str, level);
    
    // Write message
    vfprintf(g_log_file, format, args);
    if (format[strlen(format) - 1] != '\n') {
        fprintf(g_log_file, "\n");
    }
    fflush(g_log_file);
    
    LeaveCriticalSection(&g_log_mutex);
}

// Helper function to format and output log messages
static void log_output(const char* level, const char* format, va_list args) {
    // Log to file first
    va_list args_copy;
    va_copy(args_copy, args);
    log_to_file(level, format, args_copy);
    va_end(args_copy);
    
    // Then to console/debugger
    char buffer[1024];
    char message[1024];
    
    // Format the message
    vsnprintf(message, sizeof(message), format, args);
    
    // Create full log string with level prefix
    snprintf(buffer, sizeof(buffer), "[%s] %s", level, message);
    if (message[strlen(message) - 1] != '\n') {
        strcat(buffer, "\n");
    }
    
    // Check if we're running as a console app
    if (GetConsoleWindow() != NULL) {
        // Console app - use printf
        printf("%s", buffer);
        fflush(stdout);
    } else {
        // GUI app - use OutputDebugString
        OutputDebugStringA(buffer);
    }
}

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_output("INFO", format, args);
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_output("ERROR", format, args);
    va_end(args);
}

void log_debug(const char* format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, format);
    log_output("DEBUG", format, args);
    va_end(args);
#else
    (void)format;
#endif
}