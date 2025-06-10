#include "../app.h"
#include "../logging.h"
#include "../utils.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <shellscalingapi.h>

#pragma comment(lib, "Shcore.lib")

// Internal AppConfig struct for Windows
typedef struct {
    const char* name;
    const char* version;
    bool is_console;
    AppReadyCallback on_ready;
    bool running;
} AppConfig;

// Define a custom message for the app ready event
#define WM_APP_READY (WM_USER + 3)

HWND g_hwnd = NULL;
static AppConfig g_config = {0};
static const char* WINDOW_CLASS_NAME = "YaketyAppWindow";

// Window procedure for message handling
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_CLOSE:
            utils_atomic_write_bool(&g_config.running, false);
            return 0;

        case WM_APP_READY:
            // Call on_ready callback when message loop is running
            if (g_config.on_ready) {
                g_config.on_ready();
            }
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int app_init(const char* name, const char* version, bool is_console, AppReadyCallback on_ready) {
    if (!name || !version) {
        log_error("Invalid app parameters");
        return -1;
    }

    // Enable DPI awareness for sharp rendering on high-DPI displays
    HRESULT hr = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    if (FAILED(hr)) {
        // Fallback to older API if available
        BOOL dpiResult = SetProcessDPIAware();
        if (!dpiResult) {
            log_info("Failed to set DPI awareness");
        }
    }

    // Store configuration
    g_config.name = utils_strdup(name);
    g_config.version = utils_strdup(version);
    g_config.is_console = is_console;
    g_config.on_ready = on_ready;

    // If this is a GUI app, create a hidden window for message processing
    if (!g_config.is_console) {
        HINSTANCE hInstance = GetModuleHandle(NULL);

        // Register window class
        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = WINDOW_CLASS_NAME;

        if (!RegisterClassExA(&wc)) {
            log_error("Failed to register window class: %lu", GetLastError());
            return -1;
        }

        // Create hidden window for message processing
        g_hwnd = CreateWindowExA(
            0,
            WINDOW_CLASS_NAME,
            g_config.name,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            NULL,
            NULL,
            hInstance,
            NULL
        );

        if (!g_hwnd) {
            log_error("Failed to create window: %lu", GetLastError());
            return -1;
        }

        // Don't show the window - it's just for message processing
        ShowWindow(g_hwnd, SW_HIDE);
    }

    utils_atomic_write_bool(&g_config.running, true);
    log_info("App initialized: %s v%s", g_config.name, g_config.version);

    // Call on_ready callback if provided
    if (g_config.on_ready) {
        if (!g_config.is_console) {
            // For GUI apps, post a message to call on_ready after message loop starts
            // See WindowProc above for handling of WM_APP_READY message
            PostMessage(g_hwnd, WM_APP_READY, 0, 0);
        } else {
            // For console apps, call it directly
            log_info("Calling on_ready callback");
            g_config.on_ready();
        }
    }
    return 0;
}

void app_cleanup(void) {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;

        UnregisterClassA(WINDOW_CLASS_NAME, GetModuleHandle(NULL));
    }

    if (g_config.name) {
        free((void*)g_config.name);
        g_config.name = NULL;
    }

    if (g_config.version) {
        free((void*)g_config.version);
        g_config.version = NULL;
    }

    log_info("App cleanup complete");
}

void app_run(void) {
    MSG msg;

    log_info("App message loop started");
    while (utils_atomic_read_bool(&g_config.running)) {
        // Process Windows messages (NULL = all windows for current thread)
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                utils_atomic_write_bool(&g_config.running, false);
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Small sleep to prevent CPU spinning
        Sleep(1);
    }

    log_info("App message loop ended");
}

void app_quit(void) {
    utils_atomic_write_bool(&g_config.running, false);

    if (g_hwnd) {
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
    }

    log_info("App quit requested");
}

bool app_is_console(void) {
    return g_config.is_console;
}

bool app_is_running(void) {
    return utils_atomic_read_bool(&g_config.running);
}