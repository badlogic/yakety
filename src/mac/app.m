#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>
#include "../app.h"
#include "../overlay.h"
#include "../utils.h"

// Internal AppConfig struct for macOS
typedef struct {
    const char* name;
    const char* version;
    bool is_console;
    AppReadyCallback on_ready;
    bool running;  // Application running state
} AppConfig;

static AppConfig g_config = {0};

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;

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
    (void)sender;
    return NO;  // Keep running in the background
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    (void)sender;
    utils_atomic_write_bool(&g_config.running, false);
    return NSTerminateNow;
}

@end

static AppDelegate* g_app_delegate = nil;

int app_init(const char* name, const char* version, bool is_console, AppReadyCallback on_ready) {
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