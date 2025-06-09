#include "../dialog.h"
#include "../keylogger.h"
#include <windows.h>
#include <windowsx.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <commctrl.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "Comctl32.lib")

#define DIALOG_CLASS_NAME L"YaketyDialog"
#define BUTTON_HEIGHT 32
#define BUTTON_WIDTH 80
#define DIALOG_PADDING 20
#define BUTTON_SPACING 10
#define ICON_SIZE 48
#define MIN_DIALOG_WIDTH 400
#define TITLE_BAR_HEIGHT 32

typedef struct {
    const wchar_t* title;
    const wchar_t* message;
    UINT type;
    int result;
    HICON icon;
    bool is_dark_mode;
    COLORREF bg_color;
    COLORREF text_color;
    COLORREF button_bg_color;
    COLORREF button_text_color;
    COLORREF button_hover_color;
    int button_hover_id;
    HFONT message_font;
    HFONT button_font;
} DialogData;

// Button IDs
#define ID_BUTTON_OK     1001
#define ID_BUTTON_YES    1002
#define ID_BUTTON_NO     1003
#define ID_BUTTON_CANCEL 1004

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

// Custom button window procedure
static LRESULT CALLBACK button_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, 
                                   UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    DialogData* data = (DialogData*)dwRefData;
    
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

// Dialog window procedure
static LRESULT CALLBACK dialog_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogData* data = (DialogData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    static BOOL is_dragging = FALSE;
    static POINT drag_start;
    
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            data = (DialogData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
            
            // Setup colors based on theme
            if (data->is_dark_mode) {
                data->bg_color = RGB(32, 32, 32);
                data->text_color = RGB(255, 255, 255);
                data->button_bg_color = RGB(60, 60, 60);
                data->button_text_color = RGB(255, 255, 255);
                data->button_hover_color = RGB(80, 80, 80);
            } else {
                data->bg_color = RGB(255, 255, 255);
                data->text_color = RGB(0, 0, 0);
                data->button_bg_color = RGB(240, 240, 240);
                data->button_text_color = RGB(0, 0, 0);
                data->button_hover_color = RGB(220, 220, 220);
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
            
            // Load app icon with proper DPI scaling
            int icon_size_needed = scale_by_dpi(ICON_SIZE, dpi_scale);
            data->icon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), 
                                         IMAGE_ICON, icon_size_needed, icon_size_needed, 0);
            if (!data->icon) {
                data->icon = LoadIcon(NULL, IDI_APPLICATION);
            }
            
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
            
            RECT title_text_rect = {scale_by_dpi(10, dpi_scale), 0, rect.right - scale_by_dpi(40, dpi_scale), title_height};
            DrawTextW(hdc, data->title, -1, &title_text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdc, old_title_font);
            DeleteObject(title_font);
            
            // Draw close button X
            RECT close_rect = {rect.right - scale_by_dpi(30, dpi_scale), 0, rect.right, title_height};
            SetTextColor(hdc, data->text_color);
            DrawTextW(hdc, L"\u00D7", -1, &close_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // Draw icon below title bar
            int icon_x = scale_by_dpi(DIALOG_PADDING, dpi_scale);
            int icon_y = title_height + scale_by_dpi(DIALOG_PADDING, dpi_scale);
            int icon_size = scale_by_dpi(ICON_SIZE, dpi_scale);
            
            if (data->icon) {
                DrawIconEx(hdc, icon_x, icon_y, data->icon, 
                          icon_size, icon_size, 0, NULL, DI_NORMAL);
            }
            
            // Draw message text
            SetTextColor(hdc, data->text_color);
            HFONT old_font = (HFONT)SelectObject(hdc, data->message_font);
            
            RECT text_rect = {
                icon_x + icon_size + scale_by_dpi(DIALOG_PADDING, dpi_scale),
                title_height + scale_by_dpi(DIALOG_PADDING, dpi_scale),
                rect.right - scale_by_dpi(DIALOG_PADDING, dpi_scale),
                rect.bottom - scale_by_dpi(BUTTON_HEIGHT + DIALOG_PADDING * 2, dpi_scale)
            };
            
            DrawTextW(hdc, data->message, -1, &text_rect, 
                     DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EXPANDTABS);
            
            SelectObject(hdc, old_font);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id >= ID_BUTTON_OK && id <= ID_BUTTON_CANCEL) {
                data->result = id;
                DestroyWindow(hwnd);
            }
            return 0;
        }
        
        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                data->result = IDCANCEL;
                DestroyWindow(hwnd);
            } else if (wParam == VK_RETURN) {
                // Find the default button
                HWND def_button = GetDlgItem(hwnd, ID_BUTTON_OK);
                if (!def_button) def_button = GetDlgItem(hwnd, ID_BUTTON_YES);
                if (def_button) {
                    SendMessage(hwnd, WM_COMMAND, GetDlgCtrlID(def_button), 0);
                }
            }
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            float dpi_scale = get_dpi_scale_for_window(hwnd);
            int title_height = scale_by_dpi(TITLE_BAR_HEIGHT, dpi_scale);
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            
            // Check if click is on close button
            RECT rect;
            GetClientRect(hwnd, &rect);
            if (pt.x > rect.right - scale_by_dpi(30, dpi_scale) && pt.y < title_height) {
                data->result = IDCANCEL;
                DestroyWindow(hwnd);
            }
            // Check if click is on title bar for dragging
            else if (pt.y < title_height) {
                is_dragging = TRUE;
                GetCursorPos(&drag_start);
                RECT window_rect;
                GetWindowRect(hwnd, &window_rect);
                drag_start.x -= window_rect.left;
                drag_start.y -= window_rect.top;
                SetCapture(hwnd);
            }
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            if (is_dragging) {
                POINT pt;
                GetCursorPos(&pt);
                RECT rect;
                GetWindowRect(hwnd, &rect);
                
                // Calculate the offset from where we started dragging
                POINT client_pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ClientToScreen(hwnd, &client_pt);
                
                SetWindowPos(hwnd, NULL,
                    pt.x - drag_start.x,
                    pt.y - drag_start.y,
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
        
        case WM_DESTROY: {
            if (data) {
                if (data->message_font) DeleteObject(data->message_font);
                if (data->button_font) DeleteObject(data->button_font);
            }
            PostQuitMessage(0);
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Create and show custom dialog
static int show_custom_dialog(const char* title, const char* message, UINT type) {
    // Convert strings to wide
    wchar_t* wide_title = utf8_to_wide(title);
    wchar_t* wide_message = utf8_to_wide(message);
    
    if (!wide_title || !wide_message) {
        free(wide_title);
        free(wide_message);
        return MessageBoxA(NULL, message, title, type);
    }
    
    // Initialize dialog data
    DialogData data = {0};
    data.title = wide_title;
    data.message = wide_message;
    data.type = type;
    data.result = IDCANCEL;
    data.is_dark_mode = is_windows_dark_mode();
    
    // Register window class if needed
    WNDCLASSEXW wc = {0};
    if (!GetClassInfoExW(GetModuleHandle(NULL), DIALOG_CLASS_NAME, &wc)) {
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = dialog_proc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = DIALOG_CLASS_NAME;
        wc.style = CS_DROPSHADOW;  // Add drop shadow
        wc.hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), 
                                    IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
        
        if (!RegisterClassExW(&wc)) {
            free(wide_title);
            free(wide_message);
            return MessageBoxW(NULL, wide_message, wide_title, type);
        }
    }
    
    // Get DPI scale for positioning
    HDC hdc = GetDC(NULL);
    float dpi_scale = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, hdc);
    
    // Calculate text size
    HDC measure_dc = CreateCompatibleDC(NULL);
    HFONT measure_font = CreateFontW(
        scale_by_dpi(15, dpi_scale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT old_font = (HFONT)SelectObject(measure_dc, measure_font);
    
    RECT text_rect = {0, 0, scale_by_dpi(MIN_DIALOG_WIDTH - ICON_SIZE - DIALOG_PADDING * 4, dpi_scale), 0};
    DrawTextW(measure_dc, wide_message, -1, &text_rect, 
              DT_CALCRECT | DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EXPANDTABS);
    
    SelectObject(measure_dc, old_font);
    DeleteObject(measure_font);
    DeleteDC(measure_dc);
    
    // Calculate dialog size
    int content_width = scale_by_dpi(ICON_SIZE + DIALOG_PADDING, dpi_scale) + text_rect.right;
    int content_height = max(text_rect.bottom, scale_by_dpi(ICON_SIZE, dpi_scale));
    
    int client_width = max(scale_by_dpi(MIN_DIALOG_WIDTH, dpi_scale), 
                          content_width + scale_by_dpi(DIALOG_PADDING * 2, dpi_scale));
    int client_height = scale_by_dpi(TITLE_BAR_HEIGHT, dpi_scale) + content_height + scale_by_dpi(DIALOG_PADDING * 3 + BUTTON_HEIGHT, dpi_scale);
    
    int dialog_width = client_width;
    int dialog_height = client_height;
    
    // Center on screen
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_width - dialog_width) / 2;
    int y = (screen_height - dialog_height) / 2;
    
    // Create borderless dialog window
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        DIALOG_CLASS_NAME,
        L"",  // No title for borderless window
        WS_POPUP,
        x, y, dialog_width, dialog_height,
        NULL, NULL, GetModuleHandle(NULL), &data
    );
    
    if (!hwnd) {
        free(wide_title);
        free(wide_message);
        return MessageBoxW(NULL, wide_message, wide_title, type);
    }
    
    // Create buttons (position relative to client area)
    int button_y = client_height - scale_by_dpi(BUTTON_HEIGHT + DIALOG_PADDING, dpi_scale);
    int button_count = 0;
    int button_ids[4] = {0};
    const wchar_t* button_texts[4] = {0};
    
    if (type & MB_YESNO) {
        button_ids[0] = ID_BUTTON_YES;
        button_texts[0] = L"Yes";
        button_ids[1] = ID_BUTTON_NO;
        button_texts[1] = L"No";
        button_count = 2;
    } else {
        button_ids[0] = ID_BUTTON_OK;
        button_texts[0] = L"OK";
        button_count = 1;
    }
    
    // Position buttons from right to left
    int button_x = client_width - scale_by_dpi(DIALOG_PADDING + BUTTON_WIDTH, dpi_scale);
    
    for (int i = button_count - 1; i >= 0; i--) {
        HWND button = CreateWindowExW(
            0, L"BUTTON", button_texts[i],
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            button_x, button_y,
            scale_by_dpi(BUTTON_WIDTH, dpi_scale),
            scale_by_dpi(BUTTON_HEIGHT, dpi_scale),
            hwnd, (HMENU)(INT_PTR)button_ids[i],
            GetModuleHandle(NULL), NULL
        );
        
        if (button) {
            SetWindowSubclass(button, button_proc, 0, (DWORD_PTR)&data);
            SendMessage(button, WM_SETFONT, (WPARAM)data.button_font, TRUE);
        }
        
        button_x -= scale_by_dpi(BUTTON_WIDTH + BUTTON_SPACING, dpi_scale);
    }
    
    // Show dialog
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Cleanup
    free(wide_title);
    free(wide_message);
    
    // Convert result
    switch (data.result) {
        case ID_BUTTON_OK: return IDOK;
        case ID_BUTTON_YES: return IDYES;
        case ID_BUTTON_NO: return IDNO;
        case ID_BUTTON_CANCEL: return IDCANCEL;
        default: return IDCANCEL;
    }
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


// Implemented in dialog_keycapture.c