#include "../keylogger.h"
#include "../logging.h"
#include "permissions.h"
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <stdbool.h>
#include <stdio.h>

static KeyCallback g_on_press = NULL;
static KeyCallback g_on_release = NULL;
static KeyCallback g_on_cancel = NULL;
static void *g_userdata = NULL;
static bool comboPressed = false;
static bool isPaused = false;
static CFMachPortRef eventTap = NULL;
static CFRunLoopSourceRef runLoopSource = NULL;

// Current key combination to monitor (default is FN key)
// Note: kCGEventFlagMaskSecondaryFn = 0x800000
static KeyCombination g_target_combo = {0, kCGEventFlagMaskSecondaryFn};

// Key state tracking
static KeyloggerState g_state = KEYLOGGER_STATE_IDLE;
#define MAX_PRESSED_KEYS 32
static CGKeyCode g_pressed_keys[MAX_PRESSED_KEYS];
static int g_pressed_keys_count = 0;

// Special key codes for modifiers (use high values to avoid conflicts)
#define KEYCODE_FN      0x1000
#define KEYCODE_CTRL    0x1001
#define KEYCODE_ALT     0x1002
#define KEYCODE_SHIFT   0x1003
#define KEYCODE_CMD     0x1004

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

// Check if current pressed keys match our target combination
static bool matches_target_combination(void) {
    if (isPaused)
        return false;

    // Convert target combo to our virtual key codes
    CGKeyCode target_keys[MAX_PRESSED_KEYS];
    int target_count = 0;

    // Add regular key if specified
    if (g_target_combo.keys[0].code != 0) {
        target_keys[target_count++] = g_target_combo.keys[0].code;
    }

    // Add modifier keys based on flags
    if (g_target_combo.keys[0].flags & kCGEventFlagMaskSecondaryFn) {
        target_keys[target_count++] = KEYCODE_FN;
    }
    if (g_target_combo.keys[0].flags & kCGEventFlagMaskControl) {
        target_keys[target_count++] = KEYCODE_CTRL;
    }
    if (g_target_combo.keys[0].flags & kCGEventFlagMaskAlternate) {
        target_keys[target_count++] = KEYCODE_ALT;
    }
    if (g_target_combo.keys[0].flags & kCGEventFlagMaskShift) {
        target_keys[target_count++] = KEYCODE_SHIFT;
    }
    if (g_target_combo.keys[0].flags & kCGEventFlagMaskCommand) {
        target_keys[target_count++] = KEYCODE_CMD;
    }

    // Check if pressed keys exactly match target keys
    if (g_pressed_keys_count != target_count) {
        return false;
    }

    // Check each target key is pressed
    for (int i = 0; i < target_count; i++) {
        if (!is_key_pressed(target_keys[i])) {
            return false;
        }
    }

    return true;
}

static void update_single_modifier(CGEventFlags flags, CGEventFlags mask, CGKeyCode virtual_keycode) {
    bool is_pressed = (flags & mask) != 0;
    bool was_pressed = is_key_pressed(virtual_keycode);

    if (is_pressed && !was_pressed) {
        add_pressed_key(virtual_keycode);
    } else if (!is_pressed && was_pressed) {
        remove_pressed_key(virtual_keycode);
    }
}

// Track previous flags to detect actual FN key press
static CGEventFlags g_previous_flags = 0;

static void update_modifier_state(CGEventFlags flags) {
    // For FN key, only update if this is a flags-only change (no other key pressed)
    // This helps distinguish actual FN key press from arrow keys which also set the FN flag
    update_single_modifier(flags, kCGEventFlagMaskControl, KEYCODE_CTRL);
    update_single_modifier(flags, kCGEventFlagMaskAlternate, KEYCODE_ALT);
    update_single_modifier(flags, kCGEventFlagMaskShift, KEYCODE_SHIFT);
    update_single_modifier(flags, kCGEventFlagMaskCommand, KEYCODE_CMD);

    // Note: FN key handling is done separately in the event callback
}

static void update_key_state(CGEventType type, CGKeyCode keyCode, CGEventFlags flags) {
    // Handle regular keys
    if (type == kCGEventKeyDown) {
        add_pressed_key(keyCode);
    } else if (type == kCGEventKeyUp) {
        remove_pressed_key(keyCode);
    }

    // Handle modifiers from flags
    update_modifier_state(flags);
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
        log_error("Event tap was disabled! Re-enabling...");
        // Re-enable the event tap
        CGEventTapEnable(eventTap, true);
        return event;
    }

    if (isPaused)
        return event;

    // Track all key presses/releases
    if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
        CGKeyCode keyCode = (CGKeyCode) CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        CGEventFlags flags = CGEventGetFlags(event);
        update_key_state(type, keyCode, flags);
    } else if (type == kCGEventFlagsChanged) {
        CGEventFlags flags = CGEventGetFlags(event);
        // Only update FN key state on pure flag changes (not accompanying a key press)
        // This distinguishes actual FN key from arrow keys that also set the FN flag
        bool fn_was_pressed = (g_previous_flags & kCGEventFlagMaskSecondaryFn) != 0;
        bool fn_is_pressed = (flags & kCGEventFlagMaskSecondaryFn) != 0;

        if (fn_is_pressed && !fn_was_pressed) {
            add_pressed_key(KEYCODE_FN);
        } else if (!fn_is_pressed && fn_was_pressed) {
            remove_pressed_key(KEYCODE_FN);
        }

        // Update other modifiers
        update_modifier_state(flags);
        g_previous_flags = flags;
    }

    // State machine logic
    switch (g_state) {
        case KEYLOGGER_STATE_IDLE: {
            // Check if hotkey combo is pressed
            if (matches_target_combination()) {
                g_state = KEYLOGGER_STATE_COMBO_ACTIVE;
                comboPressed = true;
                if (g_on_press) {
                    g_on_press(g_userdata);
                }
            }
            break;
        }

        case KEYLOGGER_STATE_COMBO_ACTIVE: {
            // Check if combo is still held
            if (!matches_target_combination()) {
                // Combo no longer held (key released or extra key pressed)
                g_state = KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED;
                comboPressed = false;
                if (g_on_release) {
                    g_on_release(g_userdata);
                }

                // If all keys are already released, transition immediately to IDLE
                if (g_pressed_keys_count == 0) {
                    g_state = KEYLOGGER_STATE_IDLE;
                }
            }
            break;
        }

        case KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED: {
            // Wait until all keys are released
            if (g_pressed_keys_count == 0) {
                g_state = KEYLOGGER_STATE_IDLE;
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

    if (!check_and_wait_for_permission(PERMISSION_ACCESSIBILITY)) {
        return -1;
    }

    // Create event tap
    CGEventMask eventMask =
        CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) | CGEventMaskBit(kCGEventFlagsChanged);

    log_info("Creating event tap with mask: 0x%llx", (unsigned long long)eventMask);

    // Use kCGSessionEventTap which works without special privileges for accessibility
    eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask,
                                CGEventCallback, NULL);

    log_info("Event tap created: %p", eventTap);

    if (!eventTap) {
        log_error("Unable to create event tap - this should not happen if permissions were granted");
        return -1;
    }

    // Create run loop source
    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    if (!runLoopSource) {
        log_error("Failed to create run loop source");
        CFRelease(eventTap);
        return -1;
    }

    CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopCommonModes);
    log_info("Added event tap to run loop");

    // Enable the event tap
    CGEventTapEnable(eventTap, true);
    log_info("Event tap enabled");

    // Test if the event tap is actually enabled
    bool isEnabled = CGEventTapIsEnabled(eventTap);
    log_info("Event tap enabled status: %s", isEnabled ? "YES" : "NO");

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
    if (eventTap) {
        CGEventTapEnable(eventTap, false);
    }
}

void keylogger_resume(void) {
    isPaused = false;
    comboPressed = false; // Reset state when resuming

    // Reset key tracking state since we missed events while paused
    g_pressed_keys_count = 0;
    g_state = KEYLOGGER_STATE_IDLE;

    if (eventTap) {
        CGEventTapEnable(eventTap, true);
    }
}

void keylogger_set_combination(const KeyCombination *combo) {
    if (combo) {
        g_target_combo = *combo;
        comboPressed = false; // Reset state when changing combination
    }
}

KeyCombination keylogger_get_fn_combination(void) {
    KeyCombination fn_combo = {{0}};
    fn_combo.keys[0].code = 0; // No specific key, modifier only
    fn_combo.keys[0].flags = kCGEventFlagMaskSecondaryFn;
    fn_combo.count = 1;
    return fn_combo;
}