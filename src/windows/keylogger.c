#include "../keylogger.h"
#include "../logging.h"
#include <windows.h>
#include <stdbool.h>
#include <string.h>

// Global variables
static HHOOK g_keyboard_hook = NULL;
static KeyCallback g_on_press = NULL;
static KeyCallback g_on_release = NULL;
static void* g_userdata = NULL;
static bool g_paused = false;

// Key combination tracking
static KeyCombination g_target_combo = {VK_RCONTROL, 0}; // Default to Right Ctrl
static bool g_combo_pressed = false;
static uint32_t g_current_modifiers = 0;
static bool g_tracking_modifier_only = false;

// Update current modifier state
static void update_modifiers(void) {
    g_current_modifiers = 0;
    if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) || (GetAsyncKeyState(VK_RCONTROL) & 0x8000)) {
        g_current_modifiers |= 0x0001;
    }
    if ((GetAsyncKeyState(VK_LMENU) & 0x8000) || (GetAsyncKeyState(VK_RMENU) & 0x8000)) {
        g_current_modifiers |= 0x0002;
    }
    if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) || (GetAsyncKeyState(VK_RSHIFT) & 0x8000)) {
        g_current_modifiers |= 0x0004;
    }
    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) {
        g_current_modifiers |= 0x0008;
    }
}

// Check if current state matches target combination
static bool matches_combination(DWORD vkCode, bool keyDown) {
    // Special case: Single modifier keys without additional modifiers
    if (g_target_combo.modifier_flags == 0 && g_target_combo.keycode != 0) {
        // Direct key match for single modifier keys
        return (vkCode == g_target_combo.keycode);
    }
    
    update_modifiers();
    
    // Check modifier-only combinations (deprecated path, shouldn't be used anymore)
    if (g_target_combo.keycode == 0) {
        // For modifier-only, we need to check on key release
        if (!keyDown) {
            // Check if the released key is one of our target modifiers
            bool is_target_modifier = false;
            if ((g_target_combo.modifier_flags & 0x0001) && 
                (vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL)) {
                is_target_modifier = true;
            } else if ((g_target_combo.modifier_flags & 0x0002) && 
                       (vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_RMENU)) {
                is_target_modifier = true;
            } else if ((g_target_combo.modifier_flags & 0x0004) && 
                       (vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT)) {
                is_target_modifier = true;
            } else if ((g_target_combo.modifier_flags & 0x0008) && 
                       (vkCode == VK_LWIN || vkCode == VK_RWIN)) {
                is_target_modifier = true;
            }
            
            // If this is a target modifier being released and no other keys are pressed
            if (is_target_modifier && g_tracking_modifier_only) {
                g_tracking_modifier_only = false;
                return true;
            }
        } else if (keyDown) {
            // On key down, check if we should start tracking modifier-only
            bool is_modifier = (vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL ||
                                vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_RMENU ||
                                vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT ||
                                vkCode == VK_LWIN || vkCode == VK_RWIN);
            
            if (is_modifier && g_current_modifiers == g_target_combo.modifier_flags) {
                g_tracking_modifier_only = true;
            } else if (!is_modifier) {
                // Non-modifier key pressed, cancel modifier-only tracking
                g_tracking_modifier_only = false;
            }
        }
        return false;
    }
    
    // Regular key + modifier combination
    if (vkCode == g_target_combo.keycode) {
        return g_current_modifiers == g_target_combo.modifier_flags;
    }
    
    return false;
}

// Low-level keyboard hook procedure
LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && !g_paused) {
        KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        bool keyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool keyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
        
        // Convert generic VK codes to specific L/R versions based on extended key flag
        DWORD vkCode = kbdStruct->vkCode;
        DWORD origVkCode = vkCode;
        if (vkCode == VK_CONTROL) {
            vkCode = (kbdStruct->flags & LLKHF_EXTENDED) ? VK_RCONTROL : VK_LCONTROL;
        } else if (vkCode == VK_MENU) {
            vkCode = (kbdStruct->flags & LLKHF_EXTENDED) ? VK_RMENU : VK_LMENU;
        } else if (vkCode == VK_SHIFT) {
            // Shift keys need scan code check
            vkCode = (kbdStruct->scanCode == 0x36) ? VK_RSHIFT : VK_LSHIFT;
        }
        
        // Debug log for modifier keys
        if (keyDown && (origVkCode == VK_CONTROL || origVkCode == VK_MENU || origVkCode == VK_SHIFT ||
                        vkCode == VK_LCONTROL || vkCode == VK_RCONTROL || 
                        vkCode == VK_LMENU || vkCode == VK_RMENU ||
                        vkCode == VK_LSHIFT || vkCode == VK_RSHIFT)) {
            log_info("Key pressed: orig=0x%02X, converted=0x%02X, extended=%d, scanCode=0x%02X, target=0x%02X",
                     origVkCode, vkCode, (kbdStruct->flags & LLKHF_EXTENDED) ? 1 : 0, 
                     kbdStruct->scanCode, g_target_combo.keycode);
        }
        
        if (matches_combination(vkCode, keyDown)) {
            if (keyDown && !g_combo_pressed) {
                g_combo_pressed = true;
                if (g_on_press) {
                    g_on_press(g_userdata);
                }
            } else if (keyUp && g_combo_pressed) {
                g_combo_pressed = false;
                if (g_on_release) {
                    g_on_release(g_userdata);
                }
            }
        } else if (keyUp && g_combo_pressed) {
            // Key combination broken (modifier released while key still held)
            if (g_target_combo.keycode != 0) {
                // Only for non-modifier-only combinations
                g_combo_pressed = false;
                if (g_on_release) {
                    g_on_release(g_userdata);
                }
            }
        }
    }
    
    // Pass the event to the next hook
    return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
}

int keylogger_init(KeyCallback on_press, KeyCallback on_release, void* userdata) {
    // Store callbacks
    g_on_press = on_press;
    g_on_release = on_release;
    g_userdata = userdata;
    
    // Install low-level keyboard hook
    g_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_proc, GetModuleHandle(NULL), 0);
    
    if (!g_keyboard_hook) {
        DWORD error = GetLastError();
        log_error("Failed to install keyboard hook. Error: %lu", error);
        return -1;
    }
    
    log_info("Keylogger initialized successfully");
    return 0;
}

void keylogger_cleanup(void) {
    if (g_keyboard_hook) {
        UnhookWindowsHookEx(g_keyboard_hook);
        g_keyboard_hook = NULL;
    }
    
    g_on_press = NULL;
    g_on_release = NULL;
    g_userdata = NULL;
    g_combo_pressed = false;
    g_current_modifiers = 0;
    g_tracking_modifier_only = false;
    
    log_info("Keylogger cleaned up");
}

void keylogger_pause(void) {
    g_paused = true;
    log_info("Keylogger paused");
}

void keylogger_resume(void) {
    g_paused = false;
    log_info("Keylogger resumed");
}

void keylogger_set_combination(const KeyCombination* combo) {
    if (combo) {
        g_target_combo = *combo;
        g_combo_pressed = false;
        g_tracking_modifier_only = false;
        
        log_info("Keylogger combination set: keycode=%u, modifiers=0x%04X", 
                 combo->keycode, combo->modifier_flags);
    }
}

KeyCombination keylogger_get_fn_combination(void) {
    // Windows doesn't have FN key like macOS, return Right Ctrl as default
    KeyCombination ctrl_combo = {VK_RCONTROL, 0};
    return ctrl_combo;
}