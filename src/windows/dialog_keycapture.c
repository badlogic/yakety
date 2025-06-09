#include "../dialog.h"
#include "../keylogger.h"
#include "../logging.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Key capture dialog data
typedef struct {
    const char* title;
    const char* message;
    KeyCombination* result;
    bool capturing;
    bool captured;
    HWND hwnd;
    uint32_t current_modifiers;
    uint16_t current_keycode;
} KeyCaptureData;

// Convert Windows virtual key to display string
static const char* vk_to_string(UINT vk) {
    static char buffer[32];
    
    switch (vk) {
    case VK_CONTROL: return "Ctrl";
    case VK_LCONTROL: return "Left Ctrl";
    case VK_RCONTROL: return "Right Ctrl";
    case VK_MENU: return "Alt";
    case VK_LMENU: return "Left Alt";
    case VK_RMENU: return "Right Alt";
    case VK_SHIFT: return "Shift";
    case VK_LSHIFT: return "Left Shift";
    case VK_RSHIFT: return "Right Shift";
    case VK_LWIN:
    case VK_RWIN: return "Win";
    case VK_SPACE: return "Space";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_ESCAPE: return "Esc";
    case VK_BACK: return "Backspace";
    case VK_DELETE: return "Delete";
    case VK_INSERT: return "Insert";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_F1: case VK_F2: case VK_F3: case VK_F4:
    case VK_F5: case VK_F6: case VK_F7: case VK_F8:
    case VK_F9: case VK_F10: case VK_F11: case VK_F12:
        sprintf(buffer, "F%d", vk - VK_F1 + 1);
        return buffer;
    default:
        if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
            sprintf(buffer, "%c", (char)vk);
            return buffer;
        }
        sprintf(buffer, "VK_%02X", vk);
        return buffer;
    }
}

// Build display string for current key combination
static void build_combo_string(char* buffer, size_t size, uint32_t modifiers, uint16_t keycode) {
    buffer[0] = '\0';
    
    if (modifiers) {
        if (modifiers & 0x0001) strcat(buffer, "Ctrl+");
        if (modifiers & 0x0002) strcat(buffer, "Alt+");
        if (modifiers & 0x0004) strcat(buffer, "Shift+");
        if (modifiers & 0x0008) strcat(buffer, "Win+");
    }
    
    if (keycode) {
        strcat(buffer, vk_to_string(keycode));
    } else if (modifiers) {
        // Remove trailing +
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '+') {
            buffer[len-1] = '\0';
        }
    }
}

// Window procedure for the dialog
static LRESULT CALLBACK KeyCaptureProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    KeyCaptureData* data = (KeyCaptureData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        data = (KeyCaptureData*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
        data->hwnd = hwnd;
        
        // Create controls
        HFONT font = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        
        // Message label
        HWND msgLabel = CreateWindowA("STATIC", data->message,
                                      WS_CHILD | WS_VISIBLE | SS_CENTER,
                                      20, 20, 360, 30,
                                      hwnd, NULL, cs->hInstance, NULL);
        SendMessage(msgLabel, WM_SETFONT, (WPARAM)font, TRUE);
        
        // Instructions
        HWND instructions = CreateWindowA("STATIC", 
                                          data->capturing ? "Press any key combination..." : "Click below to start capturing",
                                          WS_CHILD | WS_VISIBLE | SS_CENTER,
                                          20, 70, 360, 60,
                                          hwnd, (HMENU)1001, cs->hInstance, NULL);
        SendMessage(instructions, WM_SETFONT, (WPARAM)font, TRUE);
        
        // OK button (disabled initially)
        HWND okBtn = CreateWindowA("BUTTON", "OK",
                                   WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_DISABLED,
                                   140, 150, 80, 30,
                                   hwnd, (HMENU)IDOK, cs->hInstance, NULL);
        SendMessage(okBtn, WM_SETFONT, (WPARAM)font, TRUE);
        
        // Cancel button
        HWND cancelBtn = CreateWindowA("BUTTON", "Cancel",
                                       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       230, 150, 80, 30,
                                       hwnd, (HMENU)IDCANCEL, cs->hInstance, NULL);
        SendMessage(cancelBtn, WM_SETFONT, (WPARAM)font, TRUE);
        
        // Set focus to the window to receive keyboard input
        SetFocus(hwnd);
        
        return 0;
    }
    
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        // Draw capture area
        RECT rect = {20, 60, 380, 130};
        HBRUSH bgBrush = CreateSolidBrush(RGB(45, 45, 48));
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);
        
        // Draw border
        HPEN pen = CreatePen(PS_SOLID, 2, data->capturing ? RGB(0, 122, 204) : RGB(100, 100, 100));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);
        
        // Draw text in capture area
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        
        HFONT font = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT oldFont = (HFONT)SelectObject(hdc, font);
        
        char display[256] = {0};
        if (!data->capturing && !data->captured) {
            strcpy(display, "Click here to start capturing");
        } else if (data->capturing) {
            strcpy(display, "Press a key combination...\n\n");
            if (data->current_modifiers || data->current_keycode) {
                char combo[128];
                build_combo_string(combo, sizeof(combo), data->current_modifiers, data->current_keycode);
                strcat(display, combo);
            }
        } else if (data->captured) {
            strcpy(display, "Captured: ");
            char combo[128];
            build_combo_string(combo, sizeof(combo), data->result->modifier_flags, data->result->keycode);
            strcat(display, combo);
        }
        
        DrawTextA(hdc, display, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        SelectObject(hdc, oldFont);
        DeleteObject(font);
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    
    case WM_LBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT rect = {20, 60, 380, 130};
        if (PtInRect(&rect, pt) && !data->capturing) {
            data->capturing = true;
            data->current_modifiers = 0;
            data->current_keycode = 0;
            SetWindowTextA(GetDlgItem(hwnd, 1001), "Press any key combination...");
            InvalidateRect(hwnd, NULL, TRUE);
            SetFocus(hwnd);
        }
        return 0;
    }
    
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && data->captured) {
            PostMessage(hwnd, WM_CLOSE, 1, 0);
        } else if (LOWORD(wParam) == IDCANCEL) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        return 0;
    
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (data->capturing) {
            UINT vk = (UINT)wParam;
            
            // Update modifiers
            data->current_modifiers = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) data->current_modifiers |= 0x0001;
            if (GetKeyState(VK_MENU) & 0x8000) data->current_modifiers |= 0x0002;
            if (GetKeyState(VK_SHIFT) & 0x8000) data->current_modifiers |= 0x0004;
            if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) {
                data->current_modifiers |= 0x0008;
            }
            
            // Check if it's a modifier key
            bool is_modifier = (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
                                vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
                                vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
                                vk == VK_LWIN || vk == VK_RWIN);
            
            if (!is_modifier) {
                // Regular key pressed - capture complete
                data->current_keycode = (uint16_t)vk;
                data->result->keycode = data->current_keycode;
                data->result->modifier_flags = data->current_modifiers;
                data->capturing = false;
                data->captured = true;
                SetWindowTextA(GetDlgItem(hwnd, 1001), "Key combination captured!");
                EnableWindow(GetDlgItem(hwnd, IDOK), TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                // Update display
                data->current_keycode = 0;
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        return 0;
    
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (data->capturing) {
            UINT vk = (UINT)wParam;
            
            // Check if it's a modifier key being released
            bool is_modifier = (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
                                vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
                                vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
                                vk == VK_LWIN || vk == VK_RWIN);
            
            if (is_modifier) {
                // Update modifiers
                data->current_modifiers = 0;
                if (GetKeyState(VK_CONTROL) & 0x8000) data->current_modifiers |= 0x0001;
                if (GetKeyState(VK_MENU) & 0x8000) data->current_modifiers |= 0x0002;
                if (GetKeyState(VK_SHIFT) & 0x8000) data->current_modifiers |= 0x0004;
                if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) {
                    data->current_modifiers |= 0x0008;
                }
                
                // If no modifiers are pressed anymore, capture modifier-only combination
                if (data->current_modifiers == 0 && !data->current_keycode) {
                    // Determine which modifier was released
                    uint32_t released_modifier = 0;
                    if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) {
                        released_modifier = 0x0001;
                    } else if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) {
                        released_modifier = 0x0002;
                    } else if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
                        released_modifier = 0x0004;
                    } else if (vk == VK_LWIN || vk == VK_RWIN) {
                        released_modifier = 0x0008;
                    }
                    
                    // For modifier-only
                    if (vk == VK_RCONTROL) {
                        // Special case for right control
                        data->result->keycode = VK_RCONTROL;
                        data->result->modifier_flags = 0;
                    } else {
                        data->result->keycode = 0;
                        data->result->modifier_flags = released_modifier;
                    }
                    
                    data->capturing = false;
                    data->captured = true;
                    SetWindowTextA(GetDlgItem(hwnd, 1001), "Key combination captured!");
                    EnableWindow(GetDlgItem(hwnd, IDOK), TRUE);
                    InvalidateRect(hwnd, NULL, TRUE);
                } else {
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
        }
        return 0;
    
    case WM_CLOSE: {
        BOOL success = (BOOL)wParam;
        DestroyWindow(hwnd);
        PostQuitMessage(success ? 1 : 0);
        return 0;
    }
    
    case WM_DESTROY:
        return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool dialog_keycombination_capture(const char* title, const char* message, KeyCombination* result) {
    log_info("Opening key combination capture dialog");
    
    // Pause keylogger during capture
    keylogger_pause();
    
    KeyCaptureData data = {
        .title = title,
        .message = message,
        .result = result,
        .capturing = false,
        .captured = false,
        .current_modifiers = 0,
        .current_keycode = 0
    };
    
    // Register window class
    const char* CLASS_NAME = "YaketyKeyCaptureDialog";
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = KeyCaptureProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    if (!RegisterClassA(&wc)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            log_error("Failed to register window class: %lu", error);
            keylogger_resume();
            return false;
        }
    }
    
    // Create window
    int width = 400;
    int height = 200;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    
    HWND hwnd = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        CLASS_NAME,
        title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, width, height,
        NULL, NULL, GetModuleHandle(NULL), &data
    );
    
    if (!hwnd) {
        DWORD error = GetLastError();
        log_error("Failed to create window: %lu", error);
        DeleteObject(wc.hbrBackground);
        UnregisterClassA(CLASS_NAME, GetModuleHandle(NULL));
        keylogger_resume();
        return false;
    }
    
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    // Message loop
    MSG msg;
    int result = 0;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    result = (int)msg.wParam;
    
    // Cleanup
    DeleteObject(wc.hbrBackground);
    UnregisterClassA(CLASS_NAME, GetModuleHandle(NULL));
    
    // Resume keylogger
    keylogger_resume();
    
    log_info("Key capture dialog closed with result: %d", result);
    return result == 1;
}