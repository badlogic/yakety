#include "../platform.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_copy_to_clipboard(const char* text) {
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
        } else {
            GlobalFree(hMem);
        }
    } else {
        GlobalFree(hMem);
    }
}

void platform_paste_text(void) {
    // Simulate Ctrl+V
    INPUT inputs[4] = {0};
    
    // Press Ctrl
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    
    // Press V
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    
    // Release V
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    
    // Release Ctrl
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    
    SendInput(4, inputs, sizeof(INPUT));
}

void platform_show_error(const char* title, const char* message) {
    // Convert to wide strings
    wchar_t wtitle[256] = {0};
    wchar_t wmessage[1024] = {0};
    
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, 256);
    MultiByteToWideChar(CP_UTF8, 0, message, -1, wmessage, 1024);
    
    MessageBoxW(NULL, wmessage, wtitle, MB_OK | MB_ICONERROR);
}

void platform_show_info(const char* title, const char* message) {
    // Convert to wide strings
    wchar_t wtitle[256] = {0};
    wchar_t wmessage[1024] = {0};
    
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, 256);
    MultiByteToWideChar(CP_UTF8, 0, message, -1, wmessage, 1024);
    
    MessageBoxW(NULL, wmessage, wtitle, MB_OK | MB_ICONINFORMATION);
}

int platform_init(void) {
    // Check if running as administrator
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isAdmin = elevation.TokenIsElevated;
        }
        
        CloseHandle(hToken);
    }
    
    if (!isAdmin) {
        fprintf(stderr, "Warning: Not running as administrator. Keyboard monitoring may not work properly.\n");
    }
    
    return 0;
}

const char* platform_find_model_path(void) {
    static char model_path[MAX_PATH] = {0};
    
    // Check in current directory
    if (GetFileAttributesA("ggml-base.en.bin") != INVALID_FILE_ATTRIBUTES) {
        return "ggml-base.en.bin";
    }
    
    // Check in models subdirectory of current directory
    if (GetFileAttributesA("models/ggml-base.en.bin") != INVALID_FILE_ATTRIBUTES) {
        return "models/ggml-base.en.bin";
    }
    
    // Check in executable directory
    if (GetModuleFileNameA(NULL, model_path, MAX_PATH)) {
        char* last_slash = strrchr(model_path, '\\');
        if (last_slash) {
            strcpy(last_slash + 1, "ggml-base.en.bin");
            if (GetFileAttributesA(model_path) != INVALID_FILE_ATTRIBUTES) {
                return model_path;
            }
            
            // Check in models subdirectory of executable directory
            strcpy(last_slash + 1, "models\\ggml-base.en.bin");
            if (GetFileAttributesA(model_path) != INVALID_FILE_ATTRIBUTES) {
                return model_path;
            }
        }
    }
    
    return NULL;
}

void platform_console_print(const char* message) {
    printf("%s", message);
    fflush(stdout);
}

bool platform_console_init(void) {
    // Enable UTF-8 console output
    SetConsoleOutputCP(CP_UTF8);
    return true;
}