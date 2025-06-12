#include "../app.h"
#include "../logging.h"
#include "../utils.h"
#include <shellscalingapi.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#pragma comment(lib, "Shcore.lib")

// Internal AppConfig struct for Windows
typedef struct {
    const char *name;
    const char *version;
    bool is_console;
    AppReadyCallback on_ready;
    bool running;
} AppConfig;

// Define a custom message for the app ready event
#define WM_APP_READY (WM_USER + 3)

HWND g_hwnd = NULL;
static AppConfig g_config = {0};
static const char *WINDOW_CLASS_NAME = "YaketyAppWindow";

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

int app_init(const char *name, const char *version, bool is_console, AppReadyCallback on_ready) {
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

    // Create a hidden window for message processing (needed for keyboard hooks even in console mode)
    {
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
        g_hwnd = CreateWindowExA(0, WINDOW_CLASS_NAME, g_config.name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                 CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

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
        // Both GUI and console apps should defer on_ready until message loop starts
        // This ensures keyboard hooks are installed after the message loop is running
        PostMessage(g_hwnd, WM_APP_READY, 0, 0);
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
        free((void *) g_config.name);
        g_config.name = NULL;
    }

    if (g_config.version) {
        free((void *) g_config.version);
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

// Blocking async execution with event pumping
typedef struct {
    bool completed;
    void *result;
} BlockingAsyncContext;

// Multiple tasks context for Promise.all() equivalent
typedef struct {
    int completed_count;
    int total_count;
    void **results;
    bool all_completed;
} BlockingAsyncAllContext;

// Global context for the blocking callback (Windows doesn't have closures)
static BlockingAsyncContext *g_blocking_ctx = NULL;

static void blocking_completion_callback(void *result) {
    if (g_blocking_ctx) {
        g_blocking_ctx->result = result;
        utils_atomic_write_bool(&g_blocking_ctx->completed, true);
    }
}

void *app_execute_async_blocking(async_work_fn work, void *arg) {
    if (g_config.is_console) {
        // For console apps, just call the work function directly
        // No need for async since console apps don't have UI to keep responsive
        return work(arg);
    }

    // For GUI apps, use the async approach with message pumping
    BlockingAsyncContext ctx = {false, NULL};
    g_blocking_ctx = &ctx; // Set global context

    // Start the async work
    utils_execute_async(work, arg, blocking_completion_callback);

    // Pump messages until completion
    while (!utils_atomic_read_bool(&ctx.completed) && app_is_running()) {
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Small yield to prevent burning CPU
        Sleep(1);
    }

    g_blocking_ctx = NULL; // Clear global context
    return ctx.result;
}

void app_sleep_responsive(int milliseconds) {
    if (g_config.is_console) {
        // For console apps, just sleep normally - no UI to keep responsive
        Sleep(milliseconds);
        return;
    }

    // For GUI apps, sleep while keeping UI responsive
    DWORD start_time = GetTickCount();
    DWORD target_time = start_time + milliseconds;

    while (GetTickCount() < target_time && app_is_running()) {
        // Process messages to keep UI responsive
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Small yield
        Sleep(1);
    }
}

// Helper structures for concurrent execution
typedef struct {
    async_work_fn work;
    void *arg;
    int task_index;
    BlockingAsyncAllContext *ctx;
} ConcurrentTaskData;

// Completion callback for concurrent tasks
static void blocking_all_completion_callback(void *result, int task_index, BlockingAsyncAllContext *ctx) {
    if (ctx) {
        ctx->results[task_index] = result;

        // Atomically increment completed count using Windows interlocked functions
        int completed = InterlockedIncrement((LONG *) &ctx->completed_count);

        // Check if all tasks are done
        if (completed >= ctx->total_count) {
            utils_atomic_write_bool(&ctx->all_completed, true);
        }
    }
}

// Thread function wrapper for concurrent task execution
static DWORD WINAPI concurrent_task_wrapper(LPVOID data) {
    ConcurrentTaskData *task_data = (ConcurrentTaskData *) data;
    void *result = task_data->work(task_data->arg);

    // Post completion message to main thread
    int task_index = task_data->task_index;
    BlockingAsyncAllContext *ctx = task_data->ctx;

    // For Windows, we'll use PostMessage to schedule the completion callback on main thread
    if (g_hwnd) {
        // Create a custom message structure
        typedef struct {
            void *result;
            int task_index;
            BlockingAsyncAllContext *ctx;
            ConcurrentTaskData *task_data;
        } CompletionMessage;

        CompletionMessage *msg = malloc(sizeof(CompletionMessage));
        if (msg) {
            msg->result = result;
            msg->task_index = task_index;
            msg->ctx = ctx;
            msg->task_data = task_data;

            // Use a custom message for completion
            PostMessage(g_hwnd, WM_USER + 100, 0, (LPARAM) msg);
        }
    } else {
        // If no window, call directly (console mode)
        blocking_all_completion_callback(result, task_index, ctx);
        free(task_data);
    }

    return 0;
}

void **app_execute_async_blocking_all(async_work_fn *tasks, void **args, int count) {
    if (!tasks || count <= 0) {
        return NULL;
    }

    // Allocate results array
    void **results = calloc(count, sizeof(void *));
    if (!results) {
        return NULL;
    }

    // Setup context for all tasks
    BlockingAsyncAllContext ctx = {
        .completed_count = 0, .total_count = count, .results = results, .all_completed = false};

    // Start all tasks concurrently using Windows threads
    HANDLE *threads = malloc(count * sizeof(HANDLE));
    if (!threads) {
        free(results);
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        ConcurrentTaskData *task_data = malloc(sizeof(ConcurrentTaskData));
        if (!task_data) {
            // Clean up on failure
            free(threads);
            free(results);
            return NULL;
        }

        task_data->work = tasks[i];
        task_data->arg = args ? args[i] : NULL;
        task_data->task_index = i;
        task_data->ctx = &ctx;

        threads[i] = CreateThread(NULL, 0, concurrent_task_wrapper, task_data, 0, NULL);
        if (!threads[i]) {
            // Clean up on failure
            free(task_data);
            free(threads);
            free(results);
            return NULL;
        }
    }

    // We don't need to hold thread handles - they're detached
    for (int i = 0; i < count; i++) {
        if (threads[i]) {
            CloseHandle(threads[i]);
        }
    }
    free(threads);

    // Pump messages until all tasks complete
    while (!utils_atomic_read_bool(&ctx.all_completed) && app_is_running()) {
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            // Handle our custom completion message
            if (msg.message == WM_USER + 100) {
                typedef struct {
                    void *result;
                    int task_index;
                    BlockingAsyncAllContext *ctx;
                    ConcurrentTaskData *task_data;
                } CompletionMessage;

                CompletionMessage *completion = (CompletionMessage *) msg.lParam;
                if (completion) {
                    blocking_all_completion_callback(completion->result, completion->task_index, completion->ctx);
                    free(completion->task_data);
                    free(completion);
                }
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        // Small yield to prevent CPU spinning
        Sleep(1);
    }

    // Return results array (caller must free)
    return results;
}