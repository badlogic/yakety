#include "../utils.h"
#include "../logging.h"
#include "../config.h"
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
    return utils_get_model_path_with_config(NULL);
}

const char* utils_get_model_path_with_config(void* config) {
    static char model_path[MAX_PATH] = {0};
    
    log_info("🔍 Searching for Whisper model...\n");
    
    // Check if we have a config to get model path from
    if (config) {
        Config* cfg = (Config*)config;
        const char* config_model = config_get_string(cfg, "model");
        if (config_model && strlen(config_model) > 0) {
            log_info("  Checking config model: %s\n", config_model);
            if (GetFileAttributesA(config_model) != INVALID_FILE_ATTRIBUTES) {
                log_info("  ✅ Found model from config\n");
                strncpy_s(model_path, MAX_PATH, config_model, _TRUNCATE);
                return model_path;
            } else {
                log_info("  ❌ Config model not found or not accessible\n");
            }
        }
    }
    
    // Check current directory first
    log_info("  Checking current directory: ggml-base.en.bin\n");
    if (GetFileAttributesA("ggml-base.en.bin") != INVALID_FILE_ATTRIBUTES) {
        log_info("  ✅ Found in current directory\n");
        return "ggml-base.en.bin";
    }
    
    // Get the executable directory
    if (GetModuleFileNameA(NULL, model_path, MAX_PATH) == 0) {
        log_error("  ❌ Failed to get executable path\n");
        return NULL;
    }
    
    // Find the last backslash to get directory
    char* last_slash = strrchr(model_path, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
    
    log_info("  Executable directory: %s\n", model_path);
    
    // Check models subdirectory in executable directory
    char test_path[MAX_PATH];
    snprintf(test_path, MAX_PATH, "%s\\models\\ggml-base.en.bin", model_path);
    log_info("  Checking: %s\n", test_path);
    
    if (GetFileAttributesA(test_path) != INVALID_FILE_ATTRIBUTES) {
        log_info("  ✅ Found in executable directory\\models\n");
        strcpy(model_path, test_path);  // Return full path with filename
        return model_path;
    }
    
    // Check directly in executable directory
    snprintf(test_path, MAX_PATH, "%s\\ggml-base.en.bin", model_path);
    log_info("  Checking: %s\n", test_path);
    
    if (GetFileAttributesA(test_path) != INVALID_FILE_ATTRIBUTES) {
        log_info("  ✅ Found in executable directory\n");
        strcpy(model_path, test_path);  // Return full path with filename
        return model_path;
    }
    
    // Try parent directory (for development builds)
    snprintf(test_path, MAX_PATH, "%s\\..\\ggml-base.en.bin", model_path);
    log_info("  Checking: %s\n", test_path);
    
    if (GetFileAttributesA(test_path) != INVALID_FILE_ATTRIBUTES) {
        // Normalize the path
        char full_path[MAX_PATH];
        if (GetFullPathNameA(test_path, MAX_PATH, full_path, NULL)) {
            // Remove the filename part to get just the directory
            last_slash = strrchr(full_path, '\\');
            if (last_slash) {
                *last_slash = '\0';
            }
            log_info("  ✅ Found in parent directory: %s\n", full_path);
            strcpy(model_path, full_path);
            return model_path;
        }
    }
    
    // Try whisper.cpp subdirectory
    snprintf(test_path, MAX_PATH, "%s\\whisper.cpp\\models\\ggml-base.en.bin", model_path);
    log_info("  Checking: %s\n", test_path);
    
    if (GetFileAttributesA(test_path) != INVALID_FILE_ATTRIBUTES) {
        log_info("  ✅ Found in whisper.cpp\\models\n");
        strcpy(model_path, test_path);  // Return full path with filename
        return model_path;
    }
    
    // Check whisper.cpp/models relative to current directory
    log_info("  Checking: whisper.cpp\\models\\ggml-base.en.bin\n");
    if (GetFileAttributesA("whisper.cpp\\models\\ggml-base.en.bin") != INVALID_FILE_ATTRIBUTES) {
        log_info("  ✅ Found in whisper.cpp\\models\n");
        return "whisper.cpp\\models";
    }
    
    log_error("  ❌ Model not found in any location!\n");
    return NULL;
}

void utils_open_accessibility_settings(void) {
    // Windows doesn't have a specific accessibility settings page for this
    // Open general privacy settings
    ShellExecuteA(NULL, "open", "ms-settings:privacy", NULL, NULL, SW_SHOW);
}

bool utils_is_launch_at_login_enabled(void) {
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, 
                               "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                               0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    // Check if our app is in the registry
    char value[MAX_PATH];
    DWORD size = sizeof(value);
    DWORD type;
    
    result = RegQueryValueExA(hKey, "Yakety", NULL, &type, (LPBYTE)value, &size);
    RegCloseKey(hKey);
    
    return result == ERROR_SUCCESS;
}

bool utils_set_launch_at_login(bool enabled) {
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, 
                               "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                               0, KEY_WRITE, &hKey);
    
    if (result != ERROR_SUCCESS) {
        log_error("Failed to open registry key\n");
        return false;
    }
    
    if (enabled) {
        // Get the executable path
        char exe_path[MAX_PATH];
        if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
            RegCloseKey(hKey);
            log_error("Failed to get executable path\n");
            return false;
        }
        
        // Add to registry
        result = RegSetValueExA(hKey, "Yakety", 0, REG_SZ, 
                               (BYTE*)exe_path, strlen(exe_path) + 1);
        
        if (result == ERROR_SUCCESS) {
            log_info("Successfully enabled launch at login\n");
        } else {
            log_error("Failed to set registry value\n");
        }
    } else {
        // Remove from registry
        result = RegDeleteValueA(hKey, "Yakety");
        
        if (result == ERROR_SUCCESS) {
            log_info("Successfully disabled launch at login\n");
        } else if (result == ERROR_FILE_NOT_FOUND) {
            // Already removed
            result = ERROR_SUCCESS;
        } else {
            log_error("Failed to delete registry value\n");
        }
    }
    
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

// Async work implementation
#include <process.h>

typedef struct {
    async_work_fn work;
    void* arg;
    async_callback_fn callback;
    HWND message_window;
} AsyncWorkData;

#define WM_ASYNC_CALLBACK (WM_USER + 1)

static HWND g_message_window = NULL;

static LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ASYNC_CALLBACK) {
        async_callback_fn callback = (async_callback_fn)wParam;
        void* result = (void*)lParam;
        callback(result);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void ensure_message_window(void) {
    if (g_message_window) return;
    
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = MessageWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "YaketyAsyncMessageWindow";
    RegisterClassA(&wc);
    
    g_message_window = CreateWindowExA(0, wc.lpszClassName, "", 0, 0, 0, 0, 0, 
                                      HWND_MESSAGE, NULL, wc.hInstance, NULL);
}

static unsigned __stdcall async_work_thread(void* data) {
    AsyncWorkData* work_data = (AsyncWorkData*)data;
    void* result = work_data->work(work_data->arg);
    
    // Post message to main thread
    PostMessage(work_data->message_window, WM_ASYNC_CALLBACK, 
                (WPARAM)work_data->callback, (LPARAM)result);
    
    free(work_data);
    return 0;
}

void utils_async_execute(async_work_fn work, void* arg, async_callback_fn callback) {
    ensure_message_window();
    
    AsyncWorkData* work_data = malloc(sizeof(AsyncWorkData));
    work_data->work = work;
    work_data->arg = arg;
    work_data->callback = callback;
    work_data->message_window = g_message_window;
    
    _beginthreadex(NULL, 0, async_work_thread, work_data, 0, NULL);
}

// Delay execution implementation
#define WM_DELAY_CALLBACK (WM_USER + 2)

typedef struct {
    delay_callback_fn callback;
    void* arg;
} DelayCallbackData;

static VOID CALLBACK DelayTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    (void)hwnd;
    (void)uMsg;
    (void)dwTime;
    
    // Kill the timer
    KillTimer(NULL, idEvent);
    
    // The timer ID is actually a pointer to our data
    DelayCallbackData* data = (DelayCallbackData*)idEvent;
    data->callback(data->arg);
    free(data);
}

void utils_delay_on_main_thread(int delay_ms, delay_callback_fn callback, void* arg) {
    ensure_message_window();
    
    DelayCallbackData* data = malloc(sizeof(DelayCallbackData));
    data->callback = callback;
    data->arg = arg;
    
    // Use SetTimer with the data pointer as the timer ID
    SetTimer(NULL, (UINT_PTR)data, delay_ms, DelayTimerProc);
}

// Platform abstraction implementations
static char g_config_dir_buffer[MAX_PATH] = {0};

const char* utils_get_config_dir(void) {
    // Try to get APPDATA directory
    char* appdata = getenv("APPDATA");
    if (appdata) {
        snprintf(g_config_dir_buffer, MAX_PATH, "%s\\Yakety", appdata);
        return g_config_dir_buffer;
    }
    
    // Fallback to USERPROFILE
    char* userprofile = getenv("USERPROFILE");
    if (userprofile) {
        snprintf(g_config_dir_buffer, MAX_PATH, "%s\\.yakety", userprofile);
        return g_config_dir_buffer;
    }
    
    // Last resort: try HOMEDRIVE + HOMEPATH
    char* homedrive = getenv("HOMEDRIVE");
    char* homepath = getenv("HOMEPATH");
    if (homedrive && homepath) {
        snprintf(g_config_dir_buffer, MAX_PATH, "%s%s\\.yakety", homedrive, homepath);
        return g_config_dir_buffer;
    }
    
    return NULL;
}

bool utils_ensure_dir_exists(const char* path) {
    DWORD attrib = GetFileAttributesA(path);
    
    if (attrib != INVALID_FILE_ATTRIBUTES) {
        return (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    
    // Create directory
    if (CreateDirectoryA(path, NULL)) {
        return true;
    }
    
    // Check if it failed because it already exists
    DWORD error = GetLastError();
    return error == ERROR_ALREADY_EXISTS;
}

FILE* utils_fopen_read(const char* path) {
    FILE* file = NULL;
    errno_t err = fopen_s(&file, path, "r");
    return (err == 0) ? file : NULL;
}

FILE* utils_fopen_read_binary(const char* path) {
    FILE* file = NULL;
    errno_t err = fopen_s(&file, path, "rb");
    return (err == 0) ? file : NULL;
}

FILE* utils_fopen_write(const char* path) {
    FILE* file = NULL;
    errno_t err = fopen_s(&file, path, "w");
    return (err == 0) ? file : NULL;
}

FILE* utils_fopen_write_binary(const char* path) {
    FILE* file = NULL;
    errno_t err = fopen_s(&file, path, "wb");
    return (err == 0) ? file : NULL;
}

FILE* utils_fopen_append(const char* path) {
    FILE* file = NULL;
    errno_t err = fopen_s(&file, path, "a");
    return (err == 0) ? file : NULL;
}

char* utils_strdup(const char* str) {
    return _strdup(str);
}

int utils_stricmp(const char* s1, const char* s2) {
    return _stricmp(s1, s2);
}