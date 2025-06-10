#ifndef APP_H
#define APP_H

#include <stdbool.h>

// Entry point macro - handles all platform/build type variations
#ifdef _WIN32
    #ifdef YAKETY_TRAY_APP
        // Forward declarations for Windows types (only if not already included)
        #ifndef _WINDOWS_
            typedef void* HINSTANCE;
            typedef void* LPSTR;
            #define WINAPI __stdcall
        #endif
        #define APP_ENTRY_POINT \
            int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) { \
                (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow; \
                return app_main(0, NULL, false); \
            }
    #else
        #define APP_ENTRY_POINT \
            int main(int argc, char** argv) { \
                return app_main(argc, argv, true); \
            }
    #endif
#else
    #ifdef YAKETY_TRAY_APP
        #define APP_ENTRY_POINT \
            int main(int argc, char** argv) { \
                (void)argc; (void)argv; \
                return app_main(0, NULL, false); \
            }
    #else
        #define APP_ENTRY_POINT \
            int main(int argc, char** argv) { \
                return app_main(argc, argv, true); \
            }
    #endif
#endif

// Forward declaration for app_main
int app_main(int argc, char** argv, bool is_console);

typedef void (*AppReadyCallback)(void);

// Initialize the application
int app_init(const char* name, const char* version, bool is_console, AppReadyCallback on_ready);

// Cleanup the application
void app_cleanup(void);

// Run the application main loop
void app_run(void);

// Quit the application
void app_quit(void);

// Check if the app is running in console mode
bool app_is_console(void);

// Check if the app is running
bool app_is_running(void);

// Forward declare from utils.h to avoid duplicate typedef
#ifndef ASYNC_WORK_FN_DEFINED
#include "utils.h"
#endif

// Blocking async execution - pumps events to keep UI responsive while waiting
void* app_execute_async_blocking(async_work_fn work, void* arg);

// Promise.all() equivalent - execute multiple async tasks concurrently
// tasks: array of work functions
// args: array of arguments (one per task)
// count: number of tasks
// Returns: array of results (caller must free), NULL on failure
void** app_execute_async_blocking_all(async_work_fn* tasks, void** args, int count);

// Responsive sleep that pumps events to keep UI responsive
void app_sleep_responsive(int milliseconds);

#endif // APP_H