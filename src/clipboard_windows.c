#include <windows.h>
#include <stdio.h>
#include <string.h>

void clipboard_paste_text(const char* text) {
    if (!text || strlen(text) == 0) return;
    
    // First, copy text to clipboard
    size_t len = strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hMem) return;
    
    char* pMem = (char*)GlobalLock(hMem);
    if (pMem) {
        strcpy_s(pMem, len, text);
        GlobalUnlock(hMem);
        
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            SetClipboardData(CF_TEXT, hMem);
            CloseClipboard();
            
            // Now simulate Ctrl+V to paste
            // Key down events
            INPUT inputs[4] = {0};
            
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
            
            // Small delay to ensure clipboard is ready
            Sleep(50);
            
            // Send the input
            SendInput(4, inputs, sizeof(INPUT));
        } else {
            GlobalFree(hMem);
        }
    } else {
        GlobalFree(hMem);
    }
}