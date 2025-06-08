#include "../dialog.h"
#include <windows.h>

void dialog_error(const char* title, const char* message) {
    MessageBoxA(NULL, message, title, MB_OK | MB_ICONERROR);
}

void dialog_info(const char* title, const char* message) {
    MessageBoxA(NULL, message, title, MB_OK | MB_ICONINFORMATION);
}

bool dialog_confirm(const char* title, const char* message) {
    int result = MessageBoxA(NULL, message, title, MB_YESNO | MB_ICONQUESTION);
    return result == IDYES;
}