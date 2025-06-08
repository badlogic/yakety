#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>
#include "../app.h"
#include "../overlay.h"

static AppConfig g_config = {0};
static CFRunLoopTimerRef g_timer = NULL;
static volatile bool g_running = true;
bool g_is_console = false;  // Global for logging module

static void timer_callback(CFRunLoopTimerRef timer, void *info) {
    (void)timer;
    (void)info;
    
    if (!g_running) {
        CFRunLoopStop(CFRunLoopGetCurrent());
        return;
    }
    
    // Process any pending events for GUI apps
    if (!g_config.is_console) {
        @autoreleasepool {
            NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate dateWithTimeIntervalSinceNow:0]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES];
            if (event) {
                [NSApp sendEvent:event];
            }
        }
    }
}

int app_init(const AppConfig* config) {
    g_config = *config;
    g_is_console = config->is_console;
    
    // Initialize NSApplication
    [NSApplication sharedApplication];
    
    if (config->is_console) {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
    } else {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    }
    
    // Request microphone permission
    @autoreleasepool {
        AVAuthorizationStatus authStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
        if (authStatus == AVAuthorizationStatusNotDetermined) {
            dispatch_semaphore_t sema = dispatch_semaphore_create(0);
            __block bool granted = false;
            
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL g) {
                granted = g;
                dispatch_semaphore_signal(sema);
            }];
            
            dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
            if (!granted) {
                return -1;
            }
        } else if (authStatus != AVAuthorizationStatusAuthorized) {
            return -1;
        }
    }
    
    // Set up timer for main loop
    g_timer = CFRunLoopTimerCreate(
        kCFAllocatorDefault,
        CFAbsoluteTimeGetCurrent() + 0.01,
        0.01,  // 10ms interval
        0,
        0,
        timer_callback,
        NULL
    );
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), g_timer, kCFRunLoopCommonModes);
    
    return 0;
}

void app_cleanup(void) {
    if (g_timer) {
        CFRunLoopTimerInvalidate(g_timer);
        CFRelease(g_timer);
        g_timer = NULL;
    }
}

void app_run(void) {
    // Just pump the run loop once
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
}

void app_quit(void) {
    g_running = false;
    
    if (!g_config.is_console) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSApp terminate:nil];
        });
    }
}