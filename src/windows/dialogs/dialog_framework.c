#include "dialog_framework.h"
#include "utils.h"
#include "logging.h"
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "Shcore.lib")

// Global framework state
static bool g_framework_initialized = false;
static ATOM g_dialog_class_atom = 0;

// Window class name for base dialogs
#define DIALOG_BASE_CLASS_NAME L"YaketyDialogFramework"

// Forward declarations
static LRESULT CALLBACK dialog_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static bool register_dialog_class(void);
static void unregister_dialog_class(void);
static float get_dpi_scale_for_window(HWND hwnd);
static int scale_by_dpi(int value, float dpi_scale);
static bool is_windows_dark_mode(void);
static wchar_t* utf8_to_wide(const char* utf8);

bool dialog_framework_init(void) {
    if (g_framework_initialized) {
        return true;
    }

    if (!register_dialog_class()) {
        log_error("Failed to register dialog window class");
        return false;
    }

    g_framework_initialized = true;
    log_info("Dialog framework initialized successfully");
    return true;
}

void dialog_framework_cleanup(void) {
    if (!g_framework_initialized) {
        return;
    }

    unregister_dialog_class();
    g_framework_initialized = false;
    log_info("Dialog framework cleaned up");
}

void dialog_get_system_theme(DialogTheme* theme) {
    if (!theme) return;

    memset(theme, 0, sizeof(DialogTheme));
    theme->is_dark_mode = is_windows_dark_mode();

    if (theme->is_dark_mode) {
        // Dark theme colors matching macOS
        theme->bg_color = RGB(40, 40, 42);
        theme->text_color = RGB(255, 255, 255);
        theme->secondary_text_color = RGB(152, 152, 157);
        theme->accent_color = RGB(0, 122, 255);
        theme->border_color = RGB(72, 72, 74);
        theme->control_bg_color = RGB(58, 58, 60);
        theme->button_bg_color = RGB(72, 72, 74);
        theme->button_text_color = RGB(255, 255, 255);
        theme->button_hover_color = RGB(99, 99, 102);
        theme->button_pressed_color = RGB(142, 142, 147);
    } else {
        // Light theme colors
        theme->bg_color = RGB(255, 255, 255);
        theme->text_color = RGB(0, 0, 0);
        theme->secondary_text_color = RGB(142, 142, 147);
        theme->accent_color = RGB(0, 122, 255);
        theme->border_color = RGB(200, 200, 200);
        theme->control_bg_color = RGB(248, 248, 248);
        theme->button_bg_color = RGB(240, 240, 240);
        theme->button_text_color = RGB(0, 0, 0);
        theme->button_hover_color = RGB(220, 220, 220);
        theme->button_pressed_color = RGB(180, 180, 180);
    }
}

static void dialog_init_resources(BaseDialog* dialog) {
    DialogResources* res = &dialog->resources;
    
    if (res->initialized) {
        return;
    }

    float dpi = dialog->dpi_scale;

    // Create fonts
    res->title_font = CreateFontW(
        scale_by_dpi(DIALOG_TITLE_FONT_SIZE, dpi), 0, 0, 0, FW_MEDIUM,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, DIALOG_FONT_FAMILY);

    res->body_font = CreateFontW(
        scale_by_dpi(DIALOG_BODY_FONT_SIZE, dpi), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, DIALOG_FONT_FAMILY);

    res->button_font = CreateFontW(
        scale_by_dpi(DIALOG_BUTTON_FONT_SIZE, dpi), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, DIALOG_FONT_FAMILY);

    // Create brushes
    res->bg_brush = CreateSolidBrush(dialog->theme.bg_color);
    res->control_bg_brush = CreateSolidBrush(dialog->theme.control_bg_color);
    res->button_bg_brush = CreateSolidBrush(dialog->theme.button_bg_color);
    res->button_hover_brush = CreateSolidBrush(dialog->theme.button_hover_color);

    // Create pens
    res->border_pen = CreatePen(PS_SOLID, 1, dialog->theme.border_color);
    res->accent_pen = CreatePen(PS_SOLID, 2, dialog->theme.accent_color);

    // Load app icon
    int icon_size = scale_by_dpi(APP_ICON_SIZE, dpi);
    res->app_icon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), 
                                     IMAGE_ICON, icon_size, icon_size, 0);
    if (!res->app_icon) {
        res->app_icon = LoadIcon(NULL, IDI_APPLICATION);
    }

    res->initialized = true;
}

static void dialog_cleanup_resources(BaseDialog* dialog) {
    DialogResources* res = &dialog->resources;
    
    if (!res->initialized) {
        return;
    }

    // Delete fonts
    if (res->title_font) DeleteObject(res->title_font);
    if (res->body_font) DeleteObject(res->body_font);
    if (res->button_font) DeleteObject(res->button_font);

    // Delete brushes
    if (res->bg_brush) DeleteObject(res->bg_brush);
    if (res->control_bg_brush) DeleteObject(res->control_bg_brush);
    if (res->button_bg_brush) DeleteObject(res->button_bg_brush);
    if (res->button_hover_brush) DeleteObject(res->button_hover_brush);

    // Delete pens
    if (res->border_pen) DeleteObject(res->border_pen);
    if (res->accent_pen) DeleteObject(res->accent_pen);

    // Don't delete app_icon as it may be shared

    memset(res, 0, sizeof(DialogResources));
}

void dialog_update_resources(BaseDialog* dialog) {
    if (!dialog) return;

    dialog_cleanup_resources(dialog);
    dialog->dpi_scale = get_dpi_scale_for_window(dialog->hwnd);
    dialog_get_system_theme(&dialog->theme);
    dialog_init_resources(dialog);
}

BaseDialog* dialog_create(const DialogConfig* config) {
    if (!g_framework_initialized) {
        log_error("Dialog framework not initialized");
        return NULL;
    }

    if (!config) {
        log_error("Invalid dialog configuration");
        return NULL;
    }

    BaseDialog* dialog = (BaseDialog*)calloc(1, sizeof(BaseDialog));
    if (!dialog) {
        log_error("Failed to allocate dialog memory");
        return NULL;
    }

    // Initialize dialog structure
    dialog->width = config->width;
    dialog->height = config->height;
    dialog->modal = config->modal;
    dialog->parent_hwnd = config->parent;
    dialog->event_callback = config->event_callback;
    dialog->close_callback = config->close_callback;
    dialog->user_data = config->user_data;
    dialog->result = DIALOG_RESULT_CANCEL;
    dialog->dpi_scale = 1.0f;

    // Get system theme
    dialog_get_system_theme(&dialog->theme);

    // Convert title to wide string
    wchar_t* wide_title = NULL;
    if (config->title) {
        wide_title = utf8_to_wide(config->title);
    }

    // Calculate DPI scale for positioning
    HDC hdc = GetDC(NULL);
    float dpi_scale = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, hdc);

    // Scale window size
    int scaled_width = scale_by_dpi(dialog->width, dpi_scale);
    int scaled_height = scale_by_dpi(dialog->height, dpi_scale);

    // Center on screen or parent
    int x, y;
    if (config->parent) {
        RECT parent_rect;
        GetWindowRect(config->parent, &parent_rect);
        x = parent_rect.left + (parent_rect.right - parent_rect.left - scaled_width) / 2;
        y = parent_rect.top + (parent_rect.bottom - parent_rect.top - scaled_height) / 2;
    } else {
        x = (GetSystemMetrics(SM_CXSCREEN) - scaled_width) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - scaled_height) / 2;
    }

    // Create window
    DWORD ex_style = WS_EX_DLGMODALFRAME;
    if (dialog->modal) {
        ex_style |= WS_EX_TOPMOST;
    }

    dialog->hwnd = CreateWindowExW(
        ex_style,
        DIALOG_BASE_CLASS_NAME,
        wide_title ? wide_title : L"",
        WS_POPUP,
        x, y, scaled_width, scaled_height,
        config->parent,
        NULL,
        GetModuleHandle(NULL),
        dialog  // Pass dialog as creation parameter
    );

    free(wide_title);

    if (!dialog->hwnd) {
        DWORD error = GetLastError();
        log_error("Failed to create dialog window: %lu", error);
        free(dialog);
        return NULL;
    }

    log_info("Created dialog: %dx%d at (%d,%d)", scaled_width, scaled_height, x, y);
    return dialog;
}

int dialog_show_modal(BaseDialog* dialog) {
    if (!dialog || !dialog->hwnd) {
        return DIALOG_RESULT_CANCEL;
    }

    // Enable dark mode if applicable
    if (dialog->theme.is_dark_mode) {
        BOOL dark_mode = TRUE;
        DwmSetWindowAttribute(dialog->hwnd, 20, &dark_mode, sizeof(dark_mode));
    }

    ShowWindow(dialog->hwnd, SW_SHOW);
    UpdateWindow(dialog->hwnd);

    // Message loop for modal dialog
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    int result = dialog->result;
    log_info("Modal dialog closed with result: %d", result);
    return result;
}

bool dialog_show(BaseDialog* dialog) {
    if (!dialog || !dialog->hwnd) {
        return false;
    }

    // Enable dark mode if applicable
    if (dialog->theme.is_dark_mode) {
        BOOL dark_mode = TRUE;
        DwmSetWindowAttribute(dialog->hwnd, 20, &dark_mode, sizeof(dark_mode));
    }

    ShowWindow(dialog->hwnd, SW_SHOW);
    UpdateWindow(dialog->hwnd);
    return true;
}

void dialog_destroy(BaseDialog* dialog) {
    if (!dialog) return;

    if (dialog->hwnd) {
        DestroyWindow(dialog->hwnd);
        dialog->hwnd = NULL;
    }

    dialog_cleanup_resources(dialog);
    free(dialog);
}

void dialog_paint_background(BaseDialog* dialog, HDC hdc, const char* title) {
    if (!dialog || !hdc) return;

    RECT rect;
    GetClientRect(dialog->hwnd, &rect);

    // Fill background
    FillRect(hdc, &rect, dialog->resources.bg_brush);

    // Draw custom title bar
    int title_height = scale_by_dpi(DIALOG_TITLE_BAR_HEIGHT, dialog->dpi_scale);
    RECT title_rect = {0, 0, rect.right, title_height};
    
    HBRUSH title_brush = CreateSolidBrush(
        dialog->theme.is_dark_mode ? RGB(50, 50, 52) : RGB(230, 230, 230)
    );
    FillRect(hdc, &title_rect, title_brush);
    DeleteObject(title_brush);

    // Draw header with icon and title
    int padding = scale_by_dpi(DIALOG_PADDING, dialog->dpi_scale);
    int header_y = title_height + padding;
    int icon_size = scale_by_dpi(APP_ICON_SIZE, dialog->dpi_scale);

    // Draw app icon
    if (dialog->resources.app_icon) {
        DrawIconEx(hdc, padding, header_y, dialog->resources.app_icon, 
                  icon_size, icon_size, 0, NULL, DI_NORMAL);
    }

    // Draw title text
    if (title) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, dialog->theme.text_color);
        HFONT old_font = (HFONT)SelectObject(hdc, dialog->resources.title_font);

        wchar_t* wide_title = utf8_to_wide(title);
        if (wide_title) {
            RECT title_text_rect = {
                padding + icon_size + padding/2,
                header_y,
                rect.right - padding,
                header_y + icon_size
            };
            DrawTextW(hdc, wide_title, -1, &title_text_rect, 
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            free(wide_title);
        }

        SelectObject(hdc, old_font);
    }
}

static LRESULT CALLBACK dialog_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BaseDialog* dialog = (BaseDialog*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        dialog = (BaseDialog*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)dialog);
        
        dialog->dpi_scale = get_dpi_scale_for_window(hwnd);
        dialog_init_resources(dialog);

        if (dialog->event_callback) {
            dialog->event_callback(dialog, DIALOG_EVENT_CREATE, NULL);
        }
        return 0;
    }

    case WM_DESTROY: {
        if (dialog) {
            if (dialog->event_callback) {
                dialog->event_callback(dialog, DIALOG_EVENT_DESTROY, NULL);
            }
            dialog_cleanup_resources(dialog);
        }
        
        if (dialog && dialog->modal) {
            PostQuitMessage(dialog->result);
        }
        return 0;
    }

    case WM_CLOSE: {
        if (dialog && dialog->close_callback) {
            dialog->close_callback(dialog, DIALOG_RESULT_CANCEL);
        }
        return 0;
    }

    case WM_PAINT: {
        if (dialog) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            if (dialog->event_callback) {
                dialog->event_callback(dialog, DIALOG_EVENT_PAINT, hdc);
            }
            
            EndPaint(hwnd, &ps);
        }
        return 0;
    }

    case WM_DPICHANGED: {
        if (dialog) {
            dialog_update_resources(dialog);
            if (dialog->event_callback) {
                dialog->event_callback(dialog, DIALOG_EVENT_DPI_CHANGED, NULL);
            }
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (dialog) {
            // Handle title bar dragging
            int title_height = scale_by_dpi(DIALOG_TITLE_BAR_HEIGHT, dialog->dpi_scale);
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            
            if (pt.y < title_height) {
                dialog->is_dragging = TRUE;
                GetCursorPos(&dialog->drag_start);
                RECT window_rect;
                GetWindowRect(hwnd, &window_rect);
                dialog->drag_start.x -= window_rect.left;
                dialog->drag_start.y -= window_rect.top;
                SetCapture(hwnd);
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (dialog && dialog->is_dragging) {
            POINT pt;
            GetCursorPos(&pt);
            SetWindowPos(hwnd, NULL, 
                        pt.x - dialog->drag_start.x, 
                        pt.y - dialog->drag_start.y, 
                        0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (dialog && dialog->is_dragging) {
            dialog->is_dragging = FALSE;
            ReleaseCapture();
        }
        return 0;
    }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static bool register_dialog_class(void) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = dialog_window_proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = DIALOG_BASE_CLASS_NAME;
    wc.style = CS_DROPSHADOW;
    wc.hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), 
                               IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

    g_dialog_class_atom = RegisterClassExW(&wc);
    return g_dialog_class_atom != 0;
}

static void unregister_dialog_class(void) {
    if (g_dialog_class_atom) {
        UnregisterClassW(DIALOG_BASE_CLASS_NAME, GetModuleHandle(NULL));
        g_dialog_class_atom = 0;
    }
}

// Utility functions
static float get_dpi_scale_for_window(HWND hwnd) {
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    UINT dpiX, dpiY;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        return dpiX / 96.0f;
    }

    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi / 96.0f;
}

static int scale_by_dpi(int value, float dpi_scale) {
    return (int)(value * dpi_scale);
}

static bool is_windows_dark_mode(void) {
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

static wchar_t* utf8_to_wide(const char* utf8) {
    if (!utf8) return NULL;

    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len == 0) return NULL;

    wchar_t* wide = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!wide) return NULL;

    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return wide;
}