#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdio.h>  // For FILE*

double utils_get_time(void);
void utils_sleep_ms(int milliseconds);
const char* utils_get_model_path(void);
const char* utils_get_model_path_with_config(void* config);

// Platform-specific utilities
void utils_open_accessibility_settings(void);
bool utils_is_launch_at_login_enabled(void);
bool utils_set_launch_at_login(bool enabled);

// Async work execution
typedef void* (*async_work_fn)(void* arg);
typedef void (*async_callback_fn)(void* result);

// Execute work on background thread and call callback on main thread with result
void utils_async_execute(async_work_fn work, void* arg, async_callback_fn callback);

// Delay execution
typedef void (*delay_callback_fn)(void* arg);

// Execute callback on main thread after delay_ms milliseconds
void utils_delay_on_main_thread(int delay_ms, delay_callback_fn callback, void* arg);

// Platform abstraction functions
const char* utils_get_config_dir(void);
bool utils_ensure_dir_exists(const char* path);
FILE* utils_fopen_read(const char* path);
FILE* utils_fopen_read_binary(const char* path);
FILE* utils_fopen_write(const char* path);
FILE* utils_fopen_write_binary(const char* path);
FILE* utils_fopen_append(const char* path);
char* utils_strdup(const char* str);
int utils_stricmp(const char* s1, const char* s2);

#endif // UTILS_H