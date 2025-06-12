#include "../keylogger.h"
#include "../logging.h"
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdbool.h>
#include <stdio.h>

static KeyCallback g_on_press = NULL;
static KeyCallback g_on_release = NULL;
static void *g_userdata = NULL;
static bool keyPressed = false;
static bool isPaused = false;
static CFMachPortRef eventTap = NULL;
static CFRunLoopSourceRef runLoopSource = NULL;

// Current key combination to monitor (default is FN key)
static KeyCombination g_target_combo = {0, kCGEventFlagMaskSecondaryFn};

// Check if current event matches our target combination
static bool matches_target_combination(CGEventType type, CGEventRef event) {
    if (isPaused)
        return false;

    CGKeyCode keyCode = 0;
    CGEventFlags flags = 0;

    if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
        keyCode = (CGKeyCode) CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        flags = CGEventGetFlags(event);
    } else if (type == kCGEventFlagsChanged) {
        flags = CGEventGetFlags(event);
    }

    // Filter flags to only include the modifier keys we care about
    CGEventFlags relevantFlags =
        flags & (kCGEventFlagMaskControl | kCGEventFlagMaskAlternate | kCGEventFlagMaskShift | kCGEventFlagMaskCommand |
                 kCGEventFlagMaskSecondaryFn | kCGEventFlagMaskAlphaShift);

    CGEventFlags targetFlags = g_target_combo.keys[0].flags &
                               (kCGEventFlagMaskControl | kCGEventFlagMaskAlternate | kCGEventFlagMaskShift |
                                kCGEventFlagMaskCommand | kCGEventFlagMaskSecondaryFn | kCGEventFlagMaskAlphaShift);

    // Check if this matches our target combination
    bool keyMatches;
    if (g_target_combo.keys[0].code == 0) {
        // Modifier-only combination (like FN key) - only match when NO specific key is pressed
        keyMatches = (keyCode == 0);
    } else {
        // Specific key combination - match the exact key
        keyMatches = (keyCode == g_target_combo.keys[0].code);
    }
    bool modifiersMatch = (relevantFlags == targetFlags);

    return keyMatches && modifiersMatch;
}

// Main callback function
static CGEventRef CGEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    (void) proxy;
    (void) refcon;

    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        // Re-enable the event tap
        CGEventTapEnable(eventTap, true);
        return event;
    }

    if (isPaused)
        return event;

    bool currentlyMatches = matches_target_combination(type, event);

    if (type == kCGEventKeyDown) {
        // Key down event
        if (currentlyMatches && !keyPressed) {
            keyPressed = true;
            if (g_on_press) {
                g_on_press(g_userdata);
            }
        }
    } else if (type == kCGEventKeyUp) {
        // Key up event
        if (g_target_combo.keys[0].code != 0) {
            // Specific key combination (not modifier-only)
            CGKeyCode keyCode = (CGKeyCode) CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
            if (keyCode == g_target_combo.keys[0].code && keyPressed) {
                keyPressed = false;
                if (g_on_release) {
                    g_on_release(g_userdata);
                }
            }
        }
        // For modifier-only combinations (like FN), handle in kCGEventFlagsChanged section
    } else if (type == kCGEventFlagsChanged) {
        // Modifier flags changed
        if (g_target_combo.keys[0].code == 0) {
            // Modifier-only combination
            if (currentlyMatches && !keyPressed) {
                keyPressed = true;
                if (g_on_press) {
                    g_on_press(g_userdata);
                }
            } else if (!currentlyMatches && keyPressed) {
                keyPressed = false;
                if (g_on_release) {
                    g_on_release(g_userdata);
                }
            }
        }
    }

    return event;
}

int keylogger_init(KeyCallback on_press, KeyCallback on_release, void *userdata) {
    g_on_press = on_press;
    g_on_release = on_release;
    g_userdata = userdata;

    // Create event tap
    CGEventMask eventMask =
        CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) | CGEventMaskBit(kCGEventFlagsChanged);

    eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask,
                                CGEventCallback, NULL);

    if (!eventTap) {
        fprintf(stderr,
                "ERROR: Unable to create event tap. This usually means accessibility permission is not granted.\n");
        fprintf(stderr, "Please grant accessibility permission in System Preferences → Security & Privacy → Privacy → "
                        "Accessibility\n");
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

void keylogger_pause(void) {
    isPaused = true;
}

void keylogger_resume(void) {
    isPaused = false;
    keyPressed = false; // Reset state when resuming
}

void keylogger_set_combination(const KeyCombination *combo) {
    if (combo) {
        g_target_combo = *combo;
        keyPressed = false; // Reset state when changing combination
    }
}

KeyCombination keylogger_get_fn_combination(void) {
    KeyCombination fn_combo = {{0}};
    fn_combo.keys[0].code = 0; // No specific key, modifier only
    fn_combo.keys[0].flags = kCGEventFlagMaskSecondaryFn;
    fn_combo.count = 1;
    return fn_combo;
}