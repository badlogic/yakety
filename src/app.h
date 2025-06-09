#ifndef APP_H
#define APP_H

#include <stdbool.h>

// Entry point macro
#ifdef _WIN32
    #ifdef YAKETY_TRAY_APP
        // Forward declarations for Windows types
        typedef void* HINSTANCE;
        typedef void* LPSTR;
        #define WINAPI __stdcall
        #define APP_MAIN WinMain
    #else
        #define APP_MAIN main
    #endif
#else
    #define APP_MAIN main
#endif

typedef void (*AppReadyCallback)(void);

typedef struct {
    const char* name;
    const char* version;
    bool is_console;
    AppReadyCallback on_ready;
} AppConfig;

int app_init(const AppConfig* config);
void app_cleanup(void);
void app_run(void);
void app_quit(void);

#endif // APP_H