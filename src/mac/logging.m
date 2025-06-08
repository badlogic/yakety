#import <Foundation/Foundation.h>
#include "../logging.h"
#include "../app.h"
#include <stdio.h>
#include <stdarg.h>

extern bool g_is_console;  // Set by app_init

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    if (g_is_console) {
        vprintf(format, args);
        printf("\n");
        fflush(stdout);
    } else {
        NSString* formatStr = [NSString stringWithUTF8String:format];
        NSLogv(formatStr, args);
    }
    
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    if (g_is_console) {
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
        fflush(stderr);
    } else {
        NSString* formatStr = [NSString stringWithUTF8String:format];
        NSLogv(formatStr, args);
    }
    
    va_end(args);
}

void log_debug(const char* format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, format);
    
    if (g_is_console) {
        vprintf(format, args);
        printf("\n");
        fflush(stdout);
    } else {
        NSString* formatStr = [NSString stringWithUTF8String:format];
        NSLogv(formatStr, args);
    }
    
    va_end(args);
#else
    (void)format;
#endif
}