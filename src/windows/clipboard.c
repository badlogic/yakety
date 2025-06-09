#include "../clipboard.h"
#include "../logging.h"
#include <windows.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>

// Structure to hold clipboard data for a single format
typedef struct {
    UINT format;
    HGLOBAL data;
    SIZE_T size;
} ClipboardFormat;

// Array to hold all clipboard formats
static ClipboardFormat* saved_formats = NULL;
static int saved_format_count = 0;

// Save current clipboard contents
static void save_clipboard(void) {
    if (!OpenClipboard(NULL)) {
        log_error("Failed to open clipboard for saving");
        return;
    }
    
    // Count available formats
    int format_count = CountClipboardFormats();
    if (format_count == 0) {
        CloseClipboard();
        return;
    }
    
    // Allocate array for formats
    saved_formats = (ClipboardFormat*)calloc(format_count, sizeof(ClipboardFormat));
    saved_format_count = 0;
    
    // Enumerate and save all formats
    UINT format = 0;
    while ((format = EnumClipboardFormats(format)) != 0) {
        HANDLE hData = GetClipboardData(format);
        if (hData) {
            SIZE_T size = GlobalSize(hData);
            if (size > 0) {
                // Allocate memory for this format
                HGLOBAL hCopy = GlobalAlloc(GMEM_MOVEABLE, size);
                if (hCopy) {
                    void* src = GlobalLock(hData);
                    void* dst = GlobalLock(hCopy);
                    if (src && dst) {
                        memcpy(dst, src, size);
                        
                        saved_formats[saved_format_count].format = format;
                        saved_formats[saved_format_count].data = hCopy;
                        saved_formats[saved_format_count].size = size;
                        saved_format_count++;
                    }
                    if (src) GlobalUnlock(hData);
                    if (dst) GlobalUnlock(hCopy);
                    
                    if (!dst) {
                        GlobalFree(hCopy);
                    }
                }
            }
        }
    }
    
    CloseClipboard();
    log_info("Saved %d clipboard formats", saved_format_count);
}

// Restore saved clipboard contents
static void restore_clipboard(void) {
    if (saved_format_count == 0 || !saved_formats) {
        return;
    }
    
    if (!OpenClipboard(NULL)) {
        log_error("Failed to open clipboard for restoring");
        return;
    }
    
    EmptyClipboard();
    
    // Restore all saved formats
    for (int i = 0; i < saved_format_count; i++) {
        SetClipboardData(saved_formats[i].format, saved_formats[i].data);
        // Note: Windows takes ownership of the data, so we don't free it
    }
    
    CloseClipboard();
    log_info("Restored %d clipboard formats", saved_format_count);
    
    // Clean up our array (but not the data, as Windows owns it now)
    free(saved_formats);
    saved_formats = NULL;
    saved_format_count = 0;
}

void clipboard_copy(const char* text) {
    if (!text || strlen(text) == 0) {
        log_error("Invalid text for clipboard copy");
        return;
    }
    
    // Convert to wide string for Unicode
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wlen == 0) {
        log_error("Failed to get wide string length");
        return;
    }
    
    // Allocate global memory for the Unicode text
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(WCHAR));
    if (!hMem) {
        log_error("Failed to allocate memory for clipboard");
        return;
    }
    
    // Convert and copy text to global memory
    WCHAR* pMem = (WCHAR*)GlobalLock(hMem);
    if (pMem) {
        MultiByteToWideChar(CP_UTF8, 0, text, -1, pMem, wlen);
        GlobalUnlock(hMem);
        
        // Save current clipboard contents
        save_clipboard();
        
        // Open clipboard and set data
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            if (SetClipboardData(CF_UNICODETEXT, hMem)) {
                log_info("Text copied to clipboard as Unicode");
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
    char className[256] = {0};
    int classNameLength = GetClassNameA(foregroundWindow, className, sizeof(className));
    
    bool isPutty = (classNameLength > 0 && strcmp(className, "PuTTY") == 0);
    
    INPUT inputs[4] = {0};
    
    if (isPutty) {
        // For PuTTY, try right-click (default paste in PuTTY)
        log_info("Detected PuTTY, using right-click for paste");
        
        // Get cursor position
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        
        // Convert to window coordinates
        ScreenToClient(foregroundWindow, &cursorPos);
        
        // Send right-click
        PostMessage(foregroundWindow, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(cursorPos.x, cursorPos.y));
        PostMessage(foregroundWindow, WM_RBUTTONUP, 0, MAKELPARAM(cursorPos.x, cursorPos.y));
        
        return; // Don't use SendInput for PuTTY
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
        
        // Give a small delay for the paste to complete
        Sleep(100);
        
        // Restore the original clipboard contents
        restore_clipboard();
    } else {
        log_error("Failed to send paste command");
    }
}