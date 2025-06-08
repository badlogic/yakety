#include "../keylogger.h"
#include "../logging.h"
#include <windows.h>
#include <stdbool.h>

// Global variables
static HHOOK g_keyboard_hook = NULL;
static bool g_right_ctrl_pressed = false;
static KeyCallback g_on_press = NULL;
static KeyCallback g_on_release = NULL;
static void* g_userdata = NULL;

// Low-level keyboard hook procedure
LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        
        // Check for Right Ctrl key (VK_RCONTROL)
        if (kbdStruct->vkCode == VK_RCONTROL) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                if (!g_right_ctrl_pressed) {
                    g_right_ctrl_pressed = true;
                    if (g_on_press) {
                        g_on_press(g_userdata);
                    }
                }
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                if (g_right_ctrl_pressed) {
                    g_right_ctrl_pressed = false;
                    if (g_on_release) {
                        g_on_release(g_userdata);
                    }
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
    g_right_ctrl_pressed = false;
    
    log_info("Keylogger cleaned up");
}