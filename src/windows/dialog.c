#include "../dialog.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>

// Helper to convert UTF-8 to wide string
static wchar_t* utf8_to_wide(const char* utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len == 0) return NULL;
    
    wchar_t* wide = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!wide) return NULL;
    
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return wide;
}

// Helper to create a custom message box with app icon
static int show_custom_dialog(const char* title, const char* message, UINT type) {
    wchar_t* wide_title = utf8_to_wide(title);
    wchar_t* wide_message = utf8_to_wide(message);
    
    if (!wide_title || !wide_message) {
        free(wide_title);
        free(wide_message);
        return MessageBoxA(NULL, message, title, type);
    }
    
    // Get the main window handle to center dialog and use its icon
    HWND hwnd = GetActiveWindow();
    
    int result = MessageBoxW(hwnd, wide_message, wide_title, type);
    
    free(wide_title);
    free(wide_message);
    
    return result;
}

void dialog_error(const char* title, const char* message) {
    show_custom_dialog(title, message, MB_OK | MB_ICONERROR);
}

void dialog_info(const char* title, const char* message) {
    show_custom_dialog(title, message, MB_OK | MB_ICONINFORMATION);
}

bool dialog_confirm(const char* title, const char* message) {
    int result = show_custom_dialog(title, message, MB_YESNO | MB_ICONQUESTION);
    return result == IDYES;
}