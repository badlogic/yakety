#include "../dialog.h"
#include "../keylogger.h"
#include "../logging.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <dwmapi.h>
#include <shellscalingapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "Shcore.lib")

#define DIALOG_CLASS_NAME L"YaketyKeyCaptureDialog"
#define BUTTON_HEIGHT 32
#define BUTTON_WIDTH 80
#define DIALOG_PADDING 20
#define BUTTON_SPACING 10
#define TITLE_BAR_HEIGHT 32
#define CAPTURE_AREA_HEIGHT 100

typedef struct {
    const char* title;
    const char* message;
    KeyCombination* result;
    bool capturing;
    bool captured;
    HWND hwnd;
    uint32_t current_modifiers;
    uint16_t current_keycode;
    uint16_t last_keycode;
    bool is_dark_mode;
    COLORREF bg_color;
    COLORREF text_color;
    COLORREF button_bg_color;
    COLORREF button_text_color;
    COLORREF button_hover_color;
    COLORREF capture_bg_color;
    COLORREF capture_border_color;
    COLORREF capture_text_color;
    int button_hover_id;
    HFONT message_font;
    HFONT button_font;
    HFONT capture_font;
} KeyCaptureData;

// Button IDs
#define ID_BUTTON_OK     1001
#define ID_BUTTON_CANCEL 1002

// Helper to convert UTF-8 to wide string
static wchar_t* utf8_to_wide(const char* utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len == 0) return NULL;
    
    wchar_t* wide = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!wide) return NULL;
    
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return wide;
}

// Get DPI scale for the monitor
static float get_dpi_scale_for_window(HWND hwnd) {
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    UINT dpiX, dpiY;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        return dpiX / 96.0f;
    }
    
    // Fallback
    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi / 96.0f;
}

// Scale value by DPI
static int scale_by_dpi(int value, float dpi_scale) {
    return (int)(value * dpi_scale);
}

// Check if Windows is in dark mode
static bool is_windows_dark_mode() {
    HKEY hKey;
    bool dark_mode = false;
    
    if (RegOpenKeyExW(HKEY_CURRENT_USER, 
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        
        DWORD value = 0;
        DWORD size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, NULL, 
                            (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            dark_mode = (value == 0);
        }
        RegCloseKey(hKey);
    }
    
    return dark_mode;
}

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
static void build_combo_string(wchar_t* buffer, size_t size, uint32_t modifiers, uint16_t keycode, bool show_symbols) {
    buffer[0] = L'\0';
    
    // Special case: single modifier key without additional modifiers
    if (modifiers == 0 && keycode != 0) {
        const char* key_str = vk_to_string(keycode);
        wchar_t wide_key[32];
        MultiByteToWideChar(CP_UTF8, 0, key_str, -1, wide_key, 32);
        wcscat_s(buffer, size, wide_key);
        return;
    }
    
    // Use text (symbols can cause font issues)
    if (modifiers & 0x0001) wcscat_s(buffer, size, L"Ctrl+");
    if (modifiers & 0x0002) wcscat_s(buffer, size, L"Alt+");
    if (modifiers & 0x0004) wcscat_s(buffer, size, L"Shift+");
    if (modifiers & 0x0008) wcscat_s(buffer, size, L"Win+");
    
    if (keycode) {
        const char* key_str = vk_to_string(keycode);
        wchar_t wide_key[32];
        MultiByteToWideChar(CP_UTF8, 0, key_str, -1, wide_key, 32);
        wcscat_s(buffer, size, wide_key);
    } else if (modifiers) {
        // Remove trailing +
        size_t len = wcslen(buffer);
        if (len > 0 && buffer[len-1] == L'+') {
            buffer[len-1] = L'\0';
        }
    }
}

// Custom button window procedure
static LRESULT CALLBACK button_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, 
                                   UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    KeyCaptureData* data = (KeyCaptureData*)dwRefData;
    
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // Determine button state
            int button_id = GetDlgCtrlID(hwnd);
            bool is_hover = (data->button_hover_id == button_id);
            bool is_pressed = (GetKeyState(VK_LBUTTON) & 0x8000) && is_hover;
            
            // Fill background
            COLORREF bg_color = is_pressed ? data->button_bg_color : 
                               (is_hover ? data->button_hover_color : data->button_bg_color);
            HBRUSH brush = CreateSolidBrush(bg_color);
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            
            // Draw border
            HPEN pen = CreatePen(PS_SOLID, 1, data->text_color);
            HPEN old_pen = (HPEN)SelectObject(hdc, pen);
            HBRUSH null_brush = (HBRUSH)GetStockObject(NULL_BRUSH);
            HBRUSH old_brush = (HBRUSH)SelectObject(hdc, null_brush);
            Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(pen);
            
            // Draw text
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, data->button_text_color);
            HFONT old_font = (HFONT)SelectObject(hdc, data->button_font);
            
            wchar_t text[256];
            GetWindowTextW(hwnd, text, 256);
            DrawTextW(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdc, old_font);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            // Track mouse hover
            int button_id = GetDlgCtrlID(hwnd);
            if (data->button_hover_id != button_id) {
                data->button_hover_id = button_id;
                InvalidateRect(hwnd, NULL, FALSE);
                
                // Track mouse leave
                TRACKMOUSEEVENT tme = {0};
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
            }
            break;
        }
        
        case WM_MOUSELEAVE: {
            data->button_hover_id = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
        
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, button_proc, uIdSubclass);
            break;
    }
    
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// Window procedure for the dialog
static LRESULT CALLBACK KeyCaptureProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    KeyCaptureData* data = (KeyCaptureData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    static BOOL is_dragging = FALSE;
    static POINT drag_start;
    
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        data = (KeyCaptureData*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
        data->hwnd = hwnd;
        
        // Setup colors based on theme
        if (data->is_dark_mode) {
            data->bg_color = RGB(32, 32, 32);
            data->text_color = RGB(255, 255, 255);
            data->button_bg_color = RGB(60, 60, 60);
            data->button_text_color = RGB(255, 255, 255);
            data->button_hover_color = RGB(80, 80, 80);
            data->capture_bg_color = RGB(45, 45, 48);
            data->capture_border_color = RGB(100, 100, 100);
            data->capture_text_color = RGB(255, 255, 255);
        } else {
            data->bg_color = RGB(255, 255, 255);
            data->text_color = RGB(0, 0, 0);
            data->button_bg_color = RGB(240, 240, 240);
            data->button_text_color = RGB(0, 0, 0);
            data->button_hover_color = RGB(220, 220, 220);
            data->capture_bg_color = RGB(248, 248, 248);
            data->capture_border_color = RGB(200, 200, 200);
            data->capture_text_color = RGB(0, 0, 0);
        }
        
        // Create fonts
        float dpi_scale = get_dpi_scale_for_window(hwnd);
        data->message_font = CreateFontW(
            scale_by_dpi(15, dpi_scale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        
        data->button_font = CreateFontW(
            scale_by_dpi(14, dpi_scale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            
        data->capture_font = CreateFontW(
            scale_by_dpi(24, dpi_scale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        
        // Create buttons
        HWND ok_btn = CreateWindowW(L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
            0, 0, 0, 0,  // Position set in WM_SIZE
            hwnd, (HMENU)ID_BUTTON_OK, cs->hInstance, NULL);
        SetWindowSubclass(ok_btn, button_proc, 0, (DWORD_PTR)data);
        
        HWND cancel_btn = CreateWindowW(L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,  // Position set in WM_SIZE
            hwnd, (HMENU)ID_BUTTON_CANCEL, cs->hInstance, NULL);
        SetWindowSubclass(cancel_btn, button_proc, 0, (DWORD_PTR)data);
        
        // Set focus to the window to receive keyboard input
        SetFocus(hwnd);
        
        return 0;
    }
    
    case WM_SIZE: {
        // Layout controls
        float dpi_scale = get_dpi_scale_for_window(hwnd);
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        int button_width = scale_by_dpi(BUTTON_WIDTH, dpi_scale);
        int button_height = scale_by_dpi(BUTTON_HEIGHT, dpi_scale);
        int padding = scale_by_dpi(DIALOG_PADDING, dpi_scale);
        int spacing = scale_by_dpi(BUTTON_SPACING, dpi_scale);
        
        // Position buttons at bottom
        int button_y = rect.bottom - padding - button_height;
        int button_x = rect.right - padding - button_width;
        
        SetWindowPos(GetDlgItem(hwnd, ID_BUTTON_CANCEL), NULL,
            button_x, button_y, button_width, button_height,
            SWP_NOZORDER);
            
        button_x -= button_width + spacing;
        SetWindowPos(GetDlgItem(hwnd, ID_BUTTON_OK), NULL,
            button_x, button_y, button_width, button_height,
            SWP_NOZORDER);
            
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        // Fill background
        HBRUSH brush = CreateSolidBrush(data->bg_color);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        
        float dpi_scale = get_dpi_scale_for_window(hwnd);
        int title_height = scale_by_dpi(TITLE_BAR_HEIGHT, dpi_scale);
        int padding = scale_by_dpi(DIALOG_PADDING, dpi_scale);
        int capture_height = scale_by_dpi(CAPTURE_AREA_HEIGHT, dpi_scale);
        
        // Draw custom title bar
        RECT title_rect = {0, 0, rect.right, title_height};
        HBRUSH title_brush = CreateSolidBrush(data->is_dark_mode ? RGB(40, 40, 40) : RGB(230, 230, 230));
        FillRect(hdc, &title_rect, title_brush);
        DeleteObject(title_brush);
        
        // Draw title text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, data->text_color);
        HFONT title_font = CreateFontW(
            scale_by_dpi(14, dpi_scale), 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT old_title_font = (HFONT)SelectObject(hdc, title_font);
        
        wchar_t* wide_title = utf8_to_wide(data->title);
        RECT title_text_rect = {scale_by_dpi(10, dpi_scale), 0, rect.right - scale_by_dpi(40, dpi_scale), title_height};
        DrawTextW(hdc, wide_title, -1, &title_text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        free(wide_title);
        
        SelectObject(hdc, old_title_font);
        DeleteObject(title_font);
        
        // Draw close button X
        RECT close_rect = {rect.right - scale_by_dpi(30, dpi_scale), 0, rect.right, title_height};
        SetTextColor(hdc, data->text_color);
        DrawTextW(hdc, L"\u00D7", -1, &close_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        // Draw message
        SetTextColor(hdc, data->text_color);
        HFONT old_font = (HFONT)SelectObject(hdc, data->message_font);
        
        wchar_t* wide_message = utf8_to_wide(data->message);
        RECT message_rect = {
            padding,
            title_height + padding,
            rect.right - padding,
            title_height + padding + scale_by_dpi(30, dpi_scale)
        };
        DrawTextW(hdc, wide_message, -1, &message_rect, DT_CENTER | DT_TOP);
        free(wide_message);
        
        SelectObject(hdc, old_font);
        
        // Draw capture area
        RECT capture_rect = {
            padding,
            title_height + padding + scale_by_dpi(40, dpi_scale),
            rect.right - padding,
            title_height + padding + scale_by_dpi(40, dpi_scale) + capture_height
        };
        
        // Fill capture area background
        HBRUSH capture_brush = CreateSolidBrush(data->capture_bg_color);
        FillRect(hdc, &capture_rect, capture_brush);
        DeleteObject(capture_brush);
        
        // Draw capture area border
        HPEN pen = CreatePen(PS_SOLID, scale_by_dpi(2, dpi_scale), 
                            data->capturing ? RGB(0, 122, 204) : data->capture_border_color);
        HPEN old_pen = (HPEN)SelectObject(hdc, pen);
        HBRUSH null_brush = (HBRUSH)GetStockObject(NULL_BRUSH);
        HBRUSH old_brush = (HBRUSH)SelectObject(hdc, null_brush);
        Rectangle(hdc, capture_rect.left, capture_rect.top, capture_rect.right, capture_rect.bottom);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(pen);
        
        // Draw capture area text
        SetTextColor(hdc, data->capture_text_color);
        HFONT old_capture_font = (HFONT)SelectObject(hdc, data->capture_font);
        
        wchar_t display[256] = {0};
        if (!data->capturing && !data->captured) {
            wcscpy_s(display, 256, L"Click here to start capturing");
        } else if (data->capturing) {
            if (data->current_modifiers || data->current_keycode) {
                build_combo_string(display, 256, data->current_modifiers, data->current_keycode, true);
            } else {
                wcscpy_s(display, 256, L"Press a key combination...");
            }
        } else if (data->captured) {
            build_combo_string(display, 256, data->result->modifier_flags, data->result->keycode, true);
        }
        
        DrawTextW(hdc, display, -1, &capture_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        SelectObject(hdc, old_capture_font);
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    
    case WM_LBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        float dpi_scale = get_dpi_scale_for_window(hwnd);
        int title_height = scale_by_dpi(TITLE_BAR_HEIGHT, dpi_scale);
        int padding = scale_by_dpi(DIALOG_PADDING, dpi_scale);
        int capture_height = scale_by_dpi(CAPTURE_AREA_HEIGHT, dpi_scale);
        
        // Check if click is in title bar (for dragging)
        if (pt.y < title_height) {
            // Check if close button
            RECT rect;
            GetClientRect(hwnd, &rect);
            if (pt.x > rect.right - scale_by_dpi(30, dpi_scale)) {
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            
            // Start dragging
            is_dragging = TRUE;
            GetCursorPos(&drag_start);
            RECT window_rect;
            GetWindowRect(hwnd, &window_rect);
            drag_start.x -= window_rect.left;
            drag_start.y -= window_rect.top;
            SetCapture(hwnd);
            return 0;
        }
        
        // Check if click is in capture area
        RECT rect;
        GetClientRect(hwnd, &rect);
        RECT capture_rect = {
            padding,
            title_height + padding + scale_by_dpi(40, dpi_scale),
            rect.right - padding,
            title_height + padding + scale_by_dpi(40, dpi_scale) + capture_height
        };
        
        if (PtInRect(&capture_rect, pt) && !data->capturing) {
            data->capturing = true;
            data->current_modifiers = 0;
            data->current_keycode = 0;
            data->last_keycode = 0;
            SetFocus(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }
    
    case WM_MOUSEMOVE: {
        if (is_dragging) {
            POINT pt;
            GetCursorPos(&pt);
            SetWindowPos(hwnd, NULL, 
                pt.x - drag_start.x, pt.y - drag_start.y, 
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }
    
    case WM_LBUTTONUP: {
        if (is_dragging) {
            is_dragging = FALSE;
            ReleaseCapture();
        }
        return 0;
    }
    
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BUTTON_OK && data->captured) {
            PostMessage(hwnd, WM_CLOSE, 1, 0);
        } else if (LOWORD(wParam) == ID_BUTTON_CANCEL) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        return 0;
    
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (data->capturing) {
            UINT vk = (UINT)wParam;
            
            // Convert generic modifier keys to specific L/R versions
            if (vk == VK_CONTROL) {
                // Check which control key is actually pressed
                if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) {
                    vk = VK_LCONTROL;
                } else if (GetAsyncKeyState(VK_RCONTROL) & 0x8000) {
                    vk = VK_RCONTROL;
                }
            } else if (vk == VK_MENU) {
                // Check which alt key is actually pressed
                if (GetAsyncKeyState(VK_LMENU) & 0x8000) {
                    vk = VK_LMENU;
                } else if (GetAsyncKeyState(VK_RMENU) & 0x8000) {
                    vk = VK_RMENU;
                }
            } else if (vk == VK_SHIFT) {
                // Check which shift key is actually pressed
                if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) {
                    vk = VK_LSHIFT;
                } else if (GetAsyncKeyState(VK_RSHIFT) & 0x8000) {
                    vk = VK_RSHIFT;
                }
            }
            
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
                EnableWindow(GetDlgItem(hwnd, ID_BUTTON_OK), TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                // Track which specific modifier key for proper display
                data->last_keycode = (uint16_t)vk;
                data->current_keycode = 0;
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        return 0;
    
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (data->capturing) {
            UINT vk = (UINT)wParam;
            
            // Convert generic modifier keys to specific L/R versions
            if (vk == VK_CONTROL) {
                // Use last_keycode if available, otherwise check which was released
                if (data->last_keycode == VK_LCONTROL || data->last_keycode == VK_RCONTROL) {
                    vk = data->last_keycode;
                } else {
                    // Check which control key was released
                    if (!(GetAsyncKeyState(VK_LCONTROL) & 0x8000)) {
                        vk = VK_LCONTROL;
                    } else if (!(GetAsyncKeyState(VK_RCONTROL) & 0x8000)) {
                        vk = VK_RCONTROL;
                    }
                }
            } else if (vk == VK_MENU) {
                if (data->last_keycode == VK_LMENU || data->last_keycode == VK_RMENU) {
                    vk = data->last_keycode;
                } else {
                    if (!(GetAsyncKeyState(VK_LMENU) & 0x8000)) {
                        vk = VK_LMENU;
                    } else if (!(GetAsyncKeyState(VK_RMENU) & 0x8000)) {
                        vk = VK_RMENU;
                    }
                }
            } else if (vk == VK_SHIFT) {
                if (data->last_keycode == VK_LSHIFT || data->last_keycode == VK_RSHIFT) {
                    vk = data->last_keycode;
                } else {
                    if (!(GetAsyncKeyState(VK_LSHIFT) & 0x8000)) {
                        vk = VK_LSHIFT;
                    } else if (!(GetAsyncKeyState(VK_RSHIFT) & 0x8000)) {
                        vk = VK_RSHIFT;
                    }
                }
            }
            
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
                    // Use the specific key that was released
                    if (vk == VK_RCONTROL || vk == VK_LCONTROL || vk == VK_CONTROL) {
                        // Store the specific control key
                        data->result->keycode = vk;
                        data->result->modifier_flags = 0;
                    } else if (vk == VK_RMENU || vk == VK_LMENU || vk == VK_MENU ||
                               vk == VK_RSHIFT || vk == VK_LSHIFT || vk == VK_SHIFT ||
                               vk == VK_LWIN || vk == VK_RWIN) {
                        // For other single modifiers, store the specific key
                        data->result->keycode = vk;
                        data->result->modifier_flags = 0;
                    } else {
                        // Shouldn't happen, but fallback
                        data->result->keycode = 0;
                        data->result->modifier_flags = 0;
                    }
                    
                    data->capturing = false;
                    data->captured = true;
                    EnableWindow(GetDlgItem(hwnd, ID_BUTTON_OK), TRUE);
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
    
    case WM_DESTROY: {
        // Clean up fonts
        if (data->message_font) DeleteObject(data->message_font);
        if (data->button_font) DeleteObject(data->button_font);
        if (data->capture_font) DeleteObject(data->capture_font);
        return 0;
    }
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
        .current_keycode = 0,
        .last_keycode = 0,
        .is_dark_mode = is_windows_dark_mode(),
        .button_hover_id = 0
    };
    
    // Register window class
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = KeyCaptureProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = DIALOG_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    if (!RegisterClassW(&wc)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            log_error("Failed to register window class: %lu", error);
            keylogger_resume();
            return false;
        }
    }
    
    // Get DPI scale for primary monitor to size window
    HDC hdc = GetDC(NULL);
    float dpi_scale = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, hdc);
    
    // Create window
    int width = scale_by_dpi(450, dpi_scale);
    int height = scale_by_dpi(280, dpi_scale);
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    
    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        DIALOG_CLASS_NAME,
        L"",  // Title drawn custom
        WS_POPUP,
        x, y, width, height,
        NULL, NULL, GetModuleHandle(NULL), &data
    );
    
    if (!hwnd) {
        DWORD error = GetLastError();
        log_error("Failed to create window: %lu", error);
        UnregisterClassW(DIALOG_CLASS_NAME, GetModuleHandle(NULL));
        keylogger_resume();
        return false;
    }
    
    // Enable dark mode for window if applicable
    if (data.is_dark_mode) {
        BOOL value = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value));  // DWMWA_USE_IMMERSIVE_DARK_MODE
    }
    
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    // Message loop
    MSG msg;
    int dialogResult = 0;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    dialogResult = (int)msg.wParam;
    
    // Cleanup
    UnregisterClassW(DIALOG_CLASS_NAME, GetModuleHandle(NULL));
    
    // Resume keylogger
    keylogger_resume();
    
    log_info("Key capture dialog closed with result: %d", dialogResult);
    return dialogResult == 1;
}