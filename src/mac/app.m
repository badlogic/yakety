#include "../app.h"
#include "../logging.h"
#include "../overlay.h"
#include "../utils.h"
#include "dispatch.h"
#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>
#include <pthread.h>

// Internal AppConfig struct for macOS
typedef struct {
    const char *name;
    const char *version;
    bool is_console;
    AppReadyCallback on_ready;
    bool running; // Application running state
} AppConfig;

static AppConfig g_config = {0};

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void) notification;

    // Switch to accessory mode after launch for tray apps
    if (!g_config.is_console) {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    }

    // The app is now ready - call the callback if provided
    if (g_config.on_ready) {
        g_config.on_ready();
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void) sender;
    return NO; // Keep running in the background
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    (void) sender;
    utils_atomic_write_bool(&g_config.running, false);
    return NSTerminateNow;
}

@end

static AppDelegate *g_app_delegate = nil;

int app_init(const char *name, const char *version, bool is_console, AppReadyCallback on_ready) {
    g_config.name = name;
    g_config.version = version;
    g_config.is_console = is_console;
    g_config.on_ready = on_ready;
    utils_atomic_write_bool(&g_config.running, true);

    // Initialize NSApplication
    [NSApplication sharedApplication];

    if (is_console) {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];

        // For console apps, finish launching to ensure proper initialization
        [NSApp finishLaunching];

        // For console apps, call on_ready immediately after init
        if (g_config.on_ready) {
            // Schedule on_ready to run on the next iteration of the run loop
            dispatch_async(dispatch_get_main_queue(), ^{
              g_config.on_ready();
            });
        }
    } else {
        // For tray apps, start as regular to ensure UI works properly
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        g_app_delegate = [[AppDelegate alloc] init];
        [NSApp setDelegate:g_app_delegate];
    }

    return 0;
}

void app_cleanup(void) {
    g_app_delegate = nil;
}

void app_run(void) {
    if (g_config.is_console) {
        // For console apps, run the main run loop
        // This will block until CFRunLoopStop is called in app_quit()
        CFRunLoopRun();
    } else {
        // For tray apps, run the full NSApplication run loop
        [NSApp run];
    }
}

void app_quit(void) {
    utils_atomic_write_bool(&g_config.running, false);

    if (g_config.is_console) {
        // Stop the run loop for console apps
        CFRunLoopStop(CFRunLoopGetMain());
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
          [NSApp terminate:nil];
        });
    }
}

bool app_is_console(void) {
    return g_config.is_console;
}

bool app_is_running(void) {
    return utils_atomic_read_bool(&g_config.running);
}

// Blocking async execution with event pumping
typedef struct {
    volatile bool completed;
    void *result;
} BlockingAsyncContext;

// Multiple tasks context for Promise.all() equivalent
typedef struct {
    int completed_count;
    int total_count;
    void **results;
    bool all_completed;
} BlockingAsyncAllContext;

// Completion callback for concurrent tasks - uses context passed in block
static void blocking_all_completion_callback(void *result, int task_index, BlockingAsyncAllContext *ctx) {
    if (ctx) {
        ctx->results[task_index] = result;

        // Atomically increment completed count
        int completed = __atomic_add_fetch(&ctx->completed_count, 1, __ATOMIC_SEQ_CST);

        // Check if all tasks are done
        if (completed >= ctx->total_count) {
            ctx->all_completed = true;
        }
    }
}

// Wrapper for concurrent task execution
typedef struct {
    async_work_fn work;
    void *arg;
    int task_index;
    BlockingAsyncAllContext *ctx;
} ConcurrentTaskData;

static void *concurrent_task_wrapper(void *data) {
    ConcurrentTaskData *task_data = (ConcurrentTaskData *) data;
    void *result = task_data->work(task_data->arg);

    // Schedule completion callback on main thread with context
    int task_index = task_data->task_index;
    BlockingAsyncAllContext *ctx = task_data->ctx;
    app_dispatch_main(^{
      blocking_all_completion_callback(result, task_index, ctx);
      free(task_data);
    });

    return NULL;
}

// Global context for single async execution (non-reentrant by design)
static BlockingAsyncContext *g_blocking_ctx = NULL;

static void blocking_completion_callback_simple(void *result) {
    if (g_blocking_ctx) {
        g_blocking_ctx->result = result;
        g_blocking_ctx->completed = true;
    }
}

void *app_execute_async_blocking(async_work_fn work, void *arg) {
    // This function is NOT reentrant by design - cannot be called from event handlers
    if (g_blocking_ctx != NULL) {
        log_error("app_execute_async_blocking called while another blocking operation is in progress");
        return NULL;
    }

    // For GUI apps, use the async approach with event pumping
    BlockingAsyncContext ctx = {false, NULL};
    g_blocking_ctx = &ctx;

    // Start the async work with a completion callback
    utils_execute_async(work, arg, blocking_completion_callback_simple);

    // Pump events until completion
    @autoreleasepool {
        while (!ctx.completed && app_is_running()) {
            if (!g_config.is_console) {
                // For GUI apps, process NSApp events
                NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if (event) {
                    [NSApp sendEvent:event];
                }
            } else {
                CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
            }
            usleep(1000); // 1ms
        }
    }

    g_blocking_ctx = NULL;
    return ctx.result;
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

    // Start all tasks concurrently
    pthread_t *threads = malloc(count * sizeof(pthread_t));
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

        if (pthread_create(&threads[i], NULL, concurrent_task_wrapper, task_data) != 0) {
            // Clean up on failure
            free(task_data);
            free(threads);
            free(results);
            return NULL;
        }

        pthread_detach(threads[i]); // We don't need to join
    }

    free(threads); // Don't need the thread array anymore

    // Pump events until all tasks complete
    @autoreleasepool {
        while (!ctx.all_completed && app_is_running()) {
            if (!g_config.is_console) {
                // For GUI apps, process NSApp events
                NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if (event) {
                    [NSApp sendEvent:event];
                }
            } else {
                CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
            }
            usleep(1000); // 1ms yield
        }
    }

    // Return results array (caller must free)
    return results;
}

void app_sleep_responsive(int milliseconds) {
    // For GUI apps, sleep while keeping UI responsive
    double start_time = utils_now();
    double target_duration = milliseconds / 1000.0; // Convert to seconds

    @autoreleasepool {
        while ((utils_now() - start_time) < target_duration && app_is_running()) {
            if (!g_config.is_console) {
                // For GUI apps, process NSApp events
                NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if (event) {
                    [NSApp sendEvent:event];
                }
            } else {
                CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
            }

            // Small yield
            usleep(1000); // 1ms
        }
    }
}