#include "../app.h"
#include "../logging.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>

static HWND g_hwnd = NULL;
static bool g_running = false;
static AppConfig g_config = {0};
static const char* WINDOW_CLASS_NAME = "YaketyAppWindow";

// Window procedure for message handling
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_CLOSE:
            g_running = false;
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int app_init(const AppConfig* config) {
    if (!config) {
        log_error("Invalid app configuration");
        return -1;
    }
    
    // Copy configuration
    g_config.name = config->name ? _strdup(config->name) : "Yakety";
    g_config.version = config->version ? _strdup(config->version) : "1.0";
    g_config.is_console = config->is_console;
    
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
    
    g_running = true;
    log_info("App initialized: %s v%s", g_config.name, g_config.version);
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
    
    while (g_running) {
        // Process Windows messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Small sleep to prevent CPU spinning
        Sleep(1);
    }
}

void app_quit(void) {
    g_running = false;
    
    if (g_hwnd) {
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
    }
    
    log_info("App quit requested");
}