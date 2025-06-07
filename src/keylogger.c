#include <stdio.h>
#include <stdbool.h>
#include <ApplicationServices/ApplicationServices.h>
#include <sys/time.h>
#include <CoreFoundation/CoreFoundation.h>

// Forward declarations for functions in main.c
extern void fn_key_pressed(void);
extern void fn_key_released(void);

// Global state
static bool fnKeyPressed = false;
static CFMachPortRef eventTap = NULL;
static CFRunLoopSourceRef runLoopSource = NULL;

// Get timestamp in milliseconds
static long long getCurrentTimeMillis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (long long)(tv.tv_usec) / 1000;
}

// Handle FN key events
static void handleFnKeyEvent(bool pressed) {
    if (pressed) {
        fn_key_pressed();
    } else {
        fn_key_released();
    }
}

// Main callback function
static CGEventRef CGEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    if (type == kCGEventKeyDown) {
        CGKeyCode keyCode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        
        // Check for FN key (keycode 63)
        if (keyCode == 63) {
            if (!fnKeyPressed) {
                fnKeyPressed = true;
                handleFnKeyEvent(true);
            }
        }
    }
    else if (type == kCGEventKeyUp) {
        CGKeyCode keyCode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        
        // Check for FN key (keycode 63)
        if (keyCode == 63) {
            if (fnKeyPressed) {
                fnKeyPressed = false;
                handleFnKeyEvent(false);
            }
        }
    }
    else if (type == kCGEventFlagsChanged) {
        CGEventFlags flags = CGEventGetFlags(event);
        bool fnFlagPressed = (flags & kCGEventFlagMaskSecondaryFn) != 0;
        
        // Use flag method as backup if keycode didn't work
        if (fnFlagPressed != fnKeyPressed) {
            fnKeyPressed = fnFlagPressed;
            handleFnKeyEvent(fnFlagPressed);
        }
    }
    
    return event;
}

// Public interface functions for main.c
int start_keylogger(void) {
    // Create event tap
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | 
                           CGEventMaskBit(kCGEventKeyUp) | 
                           CGEventMaskBit(kCGEventFlagsChanged);
    
    eventTap = CGEventTapCreate(
        kCGSessionEventTap, 
        kCGHeadInsertEventTap, 
        kCGEventTapOptionDefault, 
        eventMask, 
        CGEventCallback, 
        NULL
    );
    
    if (!eventTap) {
        fprintf(stderr, "ERROR: Unable to create event tap. This usually means accessibility permission is not granted.\n");
        fprintf(stderr, "Please grant accessibility permission in System Preferences → Security & Privacy → Privacy → Accessibility\n");
        return 1;
    }
    
    // Create run loop source
    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    // Add to the main run loop instead of current
    CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);
    
    return 0;
}

void stop_keylogger(void) {
    if (eventTap) {
        CGEventTapEnable(eventTap, false);
        CFRelease(eventTap);
        eventTap = NULL;
    }
    
    if (runLoopSource) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopCommonModes);
        CFRelease(runLoopSource);
        runLoopSource = NULL;
    }
}