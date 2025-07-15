#include "../keylogger.h"
#include "../logging.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

// Keylogger context structure
typedef struct {
    // Hook and callback state (main thread only)
    HHOOK keyboard_hook;
    KeyCallback on_press;
    KeyCallback on_release;
    KeyCallback on_cancel;
    void *userdata;

    // Keylogger state (all accessed from main thread)
    bool paused;
    KeyCombination target_combo;
    bool combo_pressed;
    int pressed_count;
    KeyInfo pressed_keys[32];  // Increased to track more keys
    KeyloggerState state;       // State machine state
} KeyloggerContext;

static KeyloggerContext *g_keylogger = NULL;

// Check if two key infos match
static bool key_info_matches(const KeyInfo *a, const KeyInfo *b) {
    return a->code == b->code && a->flags == b->flags;
}

// Check if a key is currently in our pressed list
static bool is_key_pressed(DWORD scanCode, DWORD flags) {
    if (!g_keylogger)
        return false;

    uint32_t extended = (flags & LLKHF_EXTENDED) ? 1 : 0;
    for (int i = 0; i < g_keylogger->pressed_count; i++) {
        if (g_keylogger->pressed_keys[i].code == scanCode && g_keylogger->pressed_keys[i].flags == extended) {
            return true;
        }
    }
    return false;
}

// Add a key to pressed list
static void add_pressed_key(DWORD scanCode, DWORD flags) {
    if (!g_keylogger)
        return;

    if (g_keylogger->pressed_count < 4 && !is_key_pressed(scanCode, flags)) {
        g_keylogger->pressed_keys[g_keylogger->pressed_count].code = scanCode;
        g_keylogger->pressed_keys[g_keylogger->pressed_count].flags = (flags & LLKHF_EXTENDED) ? 1 : 0;
        g_keylogger->pressed_count++;
    }
}

// Remove a key from pressed list
static void remove_pressed_key(DWORD scanCode, DWORD flags) {
    if (!g_keylogger)
        return;

    uint32_t extended = (flags & LLKHF_EXTENDED) ? 1 : 0;
    for (int i = 0; i < g_keylogger->pressed_count; i++) {
        if (g_keylogger->pressed_keys[i].code == scanCode && g_keylogger->pressed_keys[i].flags == extended) {
            // Shift remaining keys
            for (int j = i; j < g_keylogger->pressed_count - 1; j++) {
                g_keylogger->pressed_keys[j] = g_keylogger->pressed_keys[j + 1];
            }
            g_keylogger->pressed_count--;
            break;
        }
    }
}

// Check if a key is part of the target combination
static bool is_combo_key(uint32_t scanCode, uint32_t flags) {
    if (!g_keylogger)
        return false;
    
    uint32_t extended = (flags & LLKHF_EXTENDED) ? 1 : 0;
    for (int i = 0; i < g_keylogger->target_combo.count; i++) {
        if (g_keylogger->target_combo.keys[i].code == scanCode && 
            g_keylogger->target_combo.keys[i].flags == extended) {
            return true;
        }
    }
    return false;
}

// Check if current pressed keys match the target combination
static bool check_combination_match(void) {
    if (!g_keylogger)
        return false;

    // Must have exact number of keys
    if (g_keylogger->pressed_count != g_keylogger->target_combo.count) {
        return false;
    }

    // Check if all target keys are pressed
    for (int i = 0; i < g_keylogger->target_combo.count; i++) {
        bool found = false;
        for (int j = 0; j < g_keylogger->pressed_count; j++) {
            if (key_info_matches(&g_keylogger->target_combo.keys[i], &g_keylogger->pressed_keys[j])) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

// Low-level keyboard hook procedure
LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_keylogger && !g_keylogger->paused) {
        KBDLLHOOKSTRUCT *kbdStruct = (KBDLLHOOKSTRUCT *) lParam;
        bool keyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool keyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        // Track all key presses/releases
        if (keyDown) {
            add_pressed_key(kbdStruct->scanCode, kbdStruct->flags);
        } else if (keyUp) {
            remove_pressed_key(kbdStruct->scanCode, kbdStruct->flags);
        }

        // State machine logic
        switch (g_keylogger->state) {
            case KEYLOGGER_STATE_IDLE: {
                // Check if hotkey combo is pressed
                if (keyDown && check_combination_match()) {
                    g_keylogger->state = KEYLOGGER_STATE_COMBO_ACTIVE;
                    g_keylogger->combo_pressed = true;
                    if (g_keylogger->on_press) {
                        g_keylogger->on_press(g_keylogger->userdata);
                    }
                }
                break;
            }
            
            case KEYLOGGER_STATE_COMBO_ACTIVE: {
                if (keyDown) {
                    // A new key was pressed - check if it's part of the combo
                    if (!is_combo_key(kbdStruct->scanCode, kbdStruct->flags)) {
                        // Non-combo key pressed - cancel
                        g_keylogger->state = KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED;
                        g_keylogger->combo_pressed = false;
                        if (g_keylogger->on_cancel) {
                            g_keylogger->on_cancel(g_keylogger->userdata);
                        }
                    }
                } else if (keyUp) {
                    // Check if combo is still held
                    if (!check_combination_match()) {
                        // Combo released normally
                        g_keylogger->state = KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED;
                        g_keylogger->combo_pressed = false;
                        if (g_keylogger->on_release) {
                            g_keylogger->on_release(g_keylogger->userdata);
                        }
                    }
                }
                break;
            }
            
            case KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED: {
                // Wait until all keys are released
                if (g_keylogger->pressed_count == 0) {
                    g_keylogger->state = KEYLOGGER_STATE_IDLE;
                }
                break;
            }
        }
    }

    // Pass the event to the next hook
    return CallNextHookEx(g_keylogger ? g_keylogger->keyboard_hook : NULL, nCode, wParam, lParam);
}

int keylogger_init(KeyCallback on_press, KeyCallback on_release, KeyCallback on_key_cancel, void *userdata) {
    if (g_keylogger) {
        return -1; // Already initialized
    }

    // Allocate keylogger state
    g_keylogger = calloc(1, sizeof(KeyloggerContext));
    if (!g_keylogger) {
        return -1;
    }

    // Store callbacks
    g_keylogger->on_press = on_press;
    g_keylogger->on_release = on_release;
    g_keylogger->on_cancel = on_key_cancel;
    g_keylogger->userdata = userdata;
    g_keylogger->state = KEYLOGGER_STATE_IDLE;

    // Install low-level keyboard hook
    log_info("Installing keyboard hook...");
    g_keylogger->keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_proc, GetModuleHandle(NULL), 0);

    if (!g_keylogger->keyboard_hook) {
        DWORD error = GetLastError();
        log_error("Failed to install keyboard hook. Error: %lu (0x%08lX)", error, error);
        if (error == ERROR_ACCESS_DENIED) {
            log_error("Access denied - try running as administrator");
        } else if (error == ERROR_HOOK_NOT_INSTALLED) {
            log_error("Hook type not supported");
        }
        free(g_keylogger);
        g_keylogger = NULL;
        return -1;
    }

    log_info("Keylogger initialized successfully - hook handle: %p", g_keylogger->keyboard_hook);
    return 0;
}

void keylogger_cleanup(void) {
    if (!g_keylogger) {
        return;
    }

    if (g_keylogger->keyboard_hook) {
        UnhookWindowsHookEx(g_keylogger->keyboard_hook);
    }

    free(g_keylogger);
    g_keylogger = NULL;

    log_info("Keylogger cleaned up");
}

void keylogger_pause(void) {
    if (g_keylogger) {
        g_keylogger->paused = true;
        log_info("Keylogger paused");
    }
}

void keylogger_resume(void) {
    if (g_keylogger) {
        g_keylogger->paused = false;
        log_info("Keylogger resumed");
    }
}

void keylogger_set_combination(const KeyCombination *combo) {
    if (combo && g_keylogger) {
        g_keylogger->target_combo = *combo;
        g_keylogger->combo_pressed = false;
        g_keylogger->pressed_count = 0;

        // Log the combination for debugging
        log_info("Keylogger combination set with %d keys:", combo->count);
        for (int i = 0; i < combo->count; i++) {
            log_info("  Key %d: scancode=0x%02X, extended=%d", i + 1, combo->keys[i].code, combo->keys[i].flags);
        }
    }
}

KeyCombination keylogger_get_fn_combination(void) {
    // Windows doesn't have FN key like macOS, return Right Ctrl as default
    // Right Ctrl: scancode 0x1D with extended flag
    KeyCombination ctrl_combo = {.keys = {{0x1D, 1}}, // Right Ctrl
                                 .count = 1};
    return ctrl_combo;
}