#include "../keylogger.h"
#include "../logging.h"
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdbool.h>
#include <stdio.h>

static KeyCallback g_on_press = NULL;
static KeyCallback g_on_release = NULL;
static KeyCallback g_on_cancel = NULL;
static void *g_userdata = NULL;
static bool keyPressed = false;
static bool isPaused = false;
static CFMachPortRef eventTap = NULL;
static CFRunLoopSourceRef runLoopSource = NULL;

// Current key combination to monitor (default is FN key)
static KeyCombination g_target_combo = {0, kCGEventFlagMaskSecondaryFn};

// Key state tracking
static KeyloggerState g_state = KEYLOGGER_STATE_IDLE;
#define MAX_PRESSED_KEYS 32
static CGKeyCode g_pressed_keys[MAX_PRESSED_KEYS];
static int g_pressed_keys_count = 0;

// Check if a key is currently pressed
static bool is_key_pressed(CGKeyCode keyCode) {
    for (int i = 0; i < g_pressed_keys_count; i++) {
        if (g_pressed_keys[i] == keyCode) {
            return true;
        }
    }
    return false;
}

// Add a key to the pressed keys array
static void add_pressed_key(CGKeyCode keyCode) {
    if (!is_key_pressed(keyCode) && g_pressed_keys_count < MAX_PRESSED_KEYS) {
        g_pressed_keys[g_pressed_keys_count++] = keyCode;
    }
}

// Remove a key from the pressed keys array
static void remove_pressed_key(CGKeyCode keyCode) {
    for (int i = 0; i < g_pressed_keys_count; i++) {
        if (g_pressed_keys[i] == keyCode) {
            // Shift remaining keys
            for (int j = i; j < g_pressed_keys_count - 1; j++) {
                g_pressed_keys[j] = g_pressed_keys[j + 1];
            }
            g_pressed_keys_count--;
            break;
        }
    }
}

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

// Check if a key is part of the target combination
static bool is_combo_key(CGKeyCode keyCode, CGEventFlags flags) {
    // For modifier-only combos (like FN), any regular key is NOT a combo key
    if (g_target_combo.keys[0].code == 0) {
        return keyCode == 0; // Only modifier changes are combo keys
    }
    
    // For specific key combos, check if this is the target key
    return keyCode == g_target_combo.keys[0].code;
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

    // Track all key presses/releases
    if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
        CGKeyCode keyCode = (CGKeyCode) CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        
        if (type == kCGEventKeyDown) {
            add_pressed_key(keyCode);
        } else {
            remove_pressed_key(keyCode);
        }
    }

    // State machine logic
    switch (g_state) {
        case KEYLOGGER_STATE_IDLE: {
            // Check if hotkey combo is pressed
            if (matches_target_combination(type, event)) {
                g_state = KEYLOGGER_STATE_COMBO_ACTIVE;
                keyPressed = true;
                if (g_on_press) {
                    g_on_press(g_userdata);
                }
            }
            break;
        }
        
        case KEYLOGGER_STATE_COMBO_ACTIVE: {
            if (type == kCGEventKeyDown) {
                // A new key was pressed
                CGKeyCode keyCode = (CGKeyCode) CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
                CGEventFlags flags = CGEventGetFlags(event);
                
                if (!is_combo_key(keyCode, flags) && keyCode != 0) {
                    // Non-combo key pressed - cancel
                    g_state = KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED;
                    keyPressed = false;
                    if (g_on_cancel) {
                        g_on_cancel(g_userdata);
                    }
                }
            } else if (type == kCGEventKeyUp || type == kCGEventFlagsChanged) {
                // Check if combo is still held
                if (!matches_target_combination(type, event)) {
                    // Combo released normally
                    g_state = KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED;
                    keyPressed = false;
                    if (g_on_release) {
                        g_on_release(g_userdata);
                    }
                }
            }
            break;
        }
        
        case KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED: {
            // Wait until all keys are released
            if (g_pressed_keys_count == 0 && type == kCGEventFlagsChanged) {
                // Also check that no modifier keys are pressed
                CGEventFlags flags = CGEventGetFlags(event);
                CGEventFlags relevantFlags = flags & (kCGEventFlagMaskControl | kCGEventFlagMaskAlternate | 
                                                     kCGEventFlagMaskShift | kCGEventFlagMaskCommand |
                                                     kCGEventFlagMaskSecondaryFn | kCGEventFlagMaskAlphaShift);
                if (relevantFlags == 0) {
                    g_state = KEYLOGGER_STATE_IDLE;
                }
            }
            break;
        }
    }

    return event;
}

int keylogger_init(KeyCallback on_press, KeyCallback on_release, KeyCallback on_key_cancel, void *userdata) {
    g_on_press = on_press;
    g_on_release = on_release;
    g_on_cancel = on_key_cancel;
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