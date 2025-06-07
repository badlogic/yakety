#include <windows.h>
#include <stdbool.h>
#include <stdio.h>

// Global variables
static HHOOK g_keyboard_hook = NULL;
static bool g_right_ctrl_pressed = false;

// Low-level keyboard hook procedure
LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        
        // Check for Right Ctrl key (VK_RCONTROL)
        if (kbdStruct->vkCode == VK_RCONTROL) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                g_right_ctrl_pressed = true;
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                g_right_ctrl_pressed = false;
            }
        }
    }
    
    // Pass the event to the next hook
    return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
}

int keylogger_init(void) {
    // Install low-level keyboard hook
    g_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_proc, GetModuleHandle(NULL), 0);
    
    if (!g_keyboard_hook) {
        DWORD error = GetLastError();
        fprintf(stderr, "Failed to install keyboard hook. Error: %lu\n", error);
        return -1;
    }
    
    return 0;
}

void keylogger_cleanup(void) {
    if (g_keyboard_hook) {
        UnhookWindowsHookEx(g_keyboard_hook);
        g_keyboard_hook = NULL;
    }
}

bool keylogger_is_fn_pressed(void) {
    return g_right_ctrl_pressed;
}