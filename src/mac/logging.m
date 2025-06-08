#import <Foundation/Foundation.h>
#include "../logging.h"
#include "../app.h"
#include "../utils.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>

extern bool g_is_console;  // Set by app_init

static FILE* g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_log_path[PATH_MAX] = {0};

static const char* get_log_path(void) {
    if (g_log_path[0] == '\0') {
        @autoreleasepool {
            NSString* home = NSHomeDirectory();
            NSString* logPath = [home stringByAppendingPathComponent:@".yakety/log.txt"];
            strncpy(g_log_path, [logPath UTF8String], PATH_MAX - 1);
        }
    }
    return g_log_path;
}

void log_init(void) {
    // Ensure .yakety directory exists
    @autoreleasepool {
        NSString* home = NSHomeDirectory();
        NSString* yaketyDir = [home stringByAppendingPathComponent:@".yakety"];
        
        NSFileManager* fm = [NSFileManager defaultManager];
        NSError* error = nil;
        
        if (![fm fileExistsAtPath:yaketyDir]) {
            [fm createDirectoryAtPath:yaketyDir 
                withIntermediateDirectories:YES 
                attributes:nil 
                error:&error];
        }
    }
    
    // Open log file (truncate on startup)
    const char* log_path = get_log_path();
    g_log_file = utils_fopen_write(log_path);
    if (g_log_file) {
        // Write session header
        time_t now = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(g_log_file, "\n=== Yakety Session Started: %s ===\n", time_str);
        fflush(g_log_file);
    }
}

void log_cleanup(void) {
    pthread_mutex_lock(&g_log_mutex);
    if (g_log_file) {
        // Write session footer
        time_t now = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(g_log_file, "=== Yakety Session Ended: %s ===\n\n", time_str);
        fclose(g_log_file);
        g_log_file = NULL;
    }
    pthread_mutex_unlock(&g_log_mutex);
}

static void log_to_file(const char* level, const char* format, va_list args) {
    if (!g_log_file) return;
    
    pthread_mutex_lock(&g_log_mutex);
    
    // Write timestamp
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(g_log_file, "[%s] %s: ", time_str, level);
    
    // Write message
    vfprintf(g_log_file, format, args);
    if (format[strlen(format) - 1] != '\n') {
        fprintf(g_log_file, "\n");
    }
    fflush(g_log_file);
    
    pthread_mutex_unlock(&g_log_mutex);
}

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    // Log to file
    va_list args_copy;
    va_copy(args_copy, args);
    log_to_file("INFO", format, args_copy);
    va_end(args_copy);
    
    // Also log to console/NSLog
    if (g_is_console) {
        vprintf(format, args);
        if (format[strlen(format) - 1] != '\n') {
            printf("\n");
        }
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
    
    // Log to file
    va_list args_copy;
    va_copy(args_copy, args);
    log_to_file("ERROR", format, args_copy);
    va_end(args_copy);
    
    // Also log to console/NSLog
    if (g_is_console) {
        vfprintf(stderr, format, args);
        if (format[strlen(format) - 1] != '\n') {
            fprintf(stderr, "\n");
        }
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
    
    // Log to file
    va_list args_copy;
    va_copy(args_copy, args);
    log_to_file("DEBUG", format, args_copy);
    va_end(args_copy);
    
    // Also log to console/NSLog
    if (g_is_console) {
        vprintf(format, args);
        if (format[strlen(format) - 1] != '\n') {
            printf("\n");
        }
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