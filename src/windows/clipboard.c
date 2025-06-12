#include "../clipboard.h"
#include "../logging.h"
#include "../utils.h"
#include <stdbool.h>
#include <string.h>
#include <windows.h>


extern HWND g_hwnd;

static bool open_clipboard_with_retry(HWND hwnd) {
    int elapsed_ms = 0;
    const int max_wait_ms = 1000;
    const int retry_delay_ms = 5;

    while (elapsed_ms < max_wait_ms) {
        if (OpenClipboard(hwnd)) {
            return true;
        }

        // If we can't open it immediately, sleep and retry
        utils_sleep_ms(retry_delay_ms);
        elapsed_ms += retry_delay_ms;
    }

    log_error("Failed to open clipboard after %d ms", elapsed_ms);
    return false;
}

void clipboard_copy(const char *text) {
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
    WCHAR *pMem = (WCHAR *) GlobalLock(hMem);
    if (pMem) {
        MultiByteToWideChar(CP_UTF8, 0, text, -1, pMem, wlen);
        GlobalUnlock(hMem);

        // Open clipboard and set data
        if (open_clipboard_with_retry(g_hwnd)) {
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
        // For PuTTY, use Shift+Insert
        log_info("Detected PuTTY, using Shift+Insert for paste");

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

        // Give a small delay for the paste to complete
        Sleep(100);
    } else {
        log_error("Failed to send paste command");
    }
}