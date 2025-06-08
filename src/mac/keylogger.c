#include <stdio.h>
#include <stdbool.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include "../keylogger.h"

static KeyCallback g_on_press = NULL;
static KeyCallback g_on_release = NULL;
static void* g_userdata = NULL;
static bool fnKeyPressed = false;
static CFMachPortRef eventTap = NULL;
static CFRunLoopSourceRef runLoopSource = NULL;

// Main callback function
static CGEventRef CGEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    (void)proxy;
    (void)refcon;
    
    if (type == kCGEventKeyDown) {
        CGKeyCode keyCode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        
        // Check for FN key (keycode 63)
        if (keyCode == 63) {
            if (!fnKeyPressed) {
                fnKeyPressed = true;
                if (g_on_press) {
                    g_on_press(g_userdata);
                }
            }
        }
    }
    else if (type == kCGEventKeyUp) {
        CGKeyCode keyCode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        
        // Check for FN key (keycode 63)
        if (keyCode == 63) {
            if (fnKeyPressed) {
                fnKeyPressed = false;
                if (g_on_release) {
                    g_on_release(g_userdata);
                }
            }
        }
    }
    else if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        // Re-enable the event tap
        CGEventTapEnable(eventTap, true);
    }
    
    return event;
}

int keylogger_init(KeyCallback on_press, KeyCallback on_release, void* userdata) {
    g_on_press = on_press;
    g_on_release = on_release;
    g_userdata = userdata;
    
    // Create event tap
    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1 << kCGEventFlagsChanged);
    
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
        return -1;
    }
    
    // Create run loop source
    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopCommonModes);
    
    // Enable the event tap
    CGEventTapEnable(eventTap, true);
    
    return 0;
}

void keylogger_cleanup(void) {
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
    
    g_on_press = NULL;
    g_on_release = NULL;
    g_userdata = NULL;
}