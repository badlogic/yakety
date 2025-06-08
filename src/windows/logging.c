#include "../logging.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

// Helper function to format and output log messages
static void log_output(const char* level, const char* format, va_list args) {
    char buffer[1024];
    char message[1024];
    
    // Format the message
    vsnprintf(message, sizeof(message), format, args);
    
    // Create full log string with level prefix
    snprintf(buffer, sizeof(buffer), "[%s] %s\n", level, message);
    
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
#endif
}