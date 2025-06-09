#include "../clipboard.h"
#include "../logging.h"
#include <windows.h>
#include <string.h>
#include <stdbool.h>

void clipboard_copy(const char* text) {
    if (!text || strlen(text) == 0) {
        log_error("Invalid text for clipboard copy");
        return;
    }
    
    // Allocate global memory for the text
    size_t len = strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hMem) {
        log_error("Failed to allocate memory for clipboard");
        return;
    }
    
    // Copy text to global memory
    char* pMem = (char*)GlobalLock(hMem);
    if (pMem) {
        strcpy_s(pMem, len, text);
        GlobalUnlock(hMem);
        
        // Open clipboard and set data
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            if (SetClipboardData(CF_TEXT, hMem)) {
                log_info("Text copied to clipboard");
            } else {
                log_error("Failed to set clipboard data");
                GlobalFree(hMem);
            }
            CloseClipboard();
        } else {
            log_error("Failed to open clipboard");
            GlobalFree(hMem);
        }
    } else {
        log_error("Failed to lock memory for clipboard");
        GlobalFree(hMem);
    }
}

void clipboard_paste(void) {
    // Check if the foreground window is PuTTY
    HWND foregroundWindow = GetForegroundWindow();
    char className[256];
    int classNameLength = GetClassName(foregroundWindow, className, sizeof(className));
    
    bool isPutty = (classNameLength > 0 && strcmp(className, "PuTTY") == 0);
    
    INPUT inputs[4] = {0};
    
    if (isPutty) {
        // For PuTTY, use Shift+Insert
        log_info("Detected PuTTY, using Shift+Insert");
        
        // Shift down
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_SHIFT;
        
        // Insert down
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_INSERT;
        
        // Insert up
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = VK_INSERT;
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
        
        // Shift up
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_SHIFT;
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    } else {
        // For other applications, use Ctrl+V
        log_info("Using standard Ctrl+V");
        
        // Ctrl down
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;
        
        // V down
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 'V';
        
        // V up
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'V';
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
        
        // Ctrl up
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_CONTROL;
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    
    // Send the input
    UINT sent = SendInput(4, inputs, sizeof(INPUT));
    if (sent == 4) {
        log_info("Paste command sent");
    } else {
        log_error("Failed to send paste command");
    }
}