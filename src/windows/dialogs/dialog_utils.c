#include "dialog_utils.h"
#include "logging.h"
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <math.h>
#include <stdlib.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "Shcore.lib")

// DPI Utilities

float dialog_get_dpi_scale(HWND hwnd) {
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    UINT dpiX, dpiY;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        return dpiX / 96.0f;
    }

    // Fallback to device context
    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi / 96.0f;
}

int dialog_scale_dpi(int value, float dpi_scale) {
    return (int)(value * dpi_scale + 0.5f);
}

void dialog_scale_rect(RECT* rect, float dpi_scale) {
    if (!rect) return;
    
    rect->left = dialog_scale_dpi(rect->left, dpi_scale);
    rect->top = dialog_scale_dpi(rect->top, dpi_scale);
    rect->right = dialog_scale_dpi(rect->right, dpi_scale);
    rect->bottom = dialog_scale_dpi(rect->bottom, dpi_scale);
}

// Theme Utilities

bool dialog_is_dark_mode(void) {
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

bool dialog_apply_dark_mode(HWND hwnd, bool enable) {
    if (!hwnd) return false;

    BOOL dark_mode = enable ? TRUE : FALSE;
    HRESULT hr = DwmSetWindowAttribute(hwnd, 20, &dark_mode, sizeof(dark_mode));
    return SUCCEEDED(hr);
}

// String Utilities

wchar_t* dialog_utf8_to_wide(const char* utf8) {
    if (!utf8) return NULL;

    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len == 0) return NULL;

    wchar_t* wide = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!wide) return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len) == 0) {
        free(wide);
        return NULL;
    }

    return wide;
}

char* dialog_wide_to_utf8(const wchar_t* wide) {
    if (!wide) return NULL;

    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (len == 0) return NULL;

    char* utf8 = (char*)malloc(len);
    if (!utf8) return NULL;

    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, len, NULL, NULL) == 0) {
        free(utf8);
        return NULL;
    }

    return utf8;
}

// Drawing Utilities

void dialog_draw_rounded_rect(HDC hdc, const RECT* rect, int radius, HPEN pen, HBRUSH brush) {
    if (!hdc || !rect) return;

    // Create rounded rectangle region
    HRGN rgn = CreateRoundRectRgn(rect->left, rect->top, rect->right, rect->bottom, 
                                 radius * 2, radius * 2);
    
    if (rgn) {
        // Fill with brush if provided
        if (brush) {
            FillRgn(hdc, rgn, brush);
        }

        // Draw border with pen if provided
        if (pen) {
            HPEN old_pen = (HPEN)SelectObject(hdc, pen);
            HBRUSH old_brush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            
            RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom, 
                     radius * 2, radius * 2);
            
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
        }

        DeleteObject(rgn);
    }
}

void dialog_draw_text_with_shadow(HDC hdc, const wchar_t* text, const RECT* rect, 
                                 UINT format, COLORREF text_color, COLORREF shadow_color, 
                                 HFONT font) {
    if (!hdc || !text || !rect) return;

    HFONT old_font = NULL;
    if (font) {
        old_font = (HFONT)SelectObject(hdc, font);
    }

    int old_mode = SetBkMode(hdc, TRANSPARENT);

    // Draw shadow (offset by 1 pixel)
    RECT shadow_rect = *rect;
    OffsetRect(&shadow_rect, 1, 1);
    SetTextColor(hdc, shadow_color);
    DrawTextW(hdc, text, -1, &shadow_rect, format);

    // Draw main text
    SetTextColor(hdc, text_color);
    DrawTextW(hdc, text, -1, rect, format);

    SetBkMode(hdc, old_mode);
    if (old_font) {
        SelectObject(hdc, old_font);
    }
}

// Window Utilities

void dialog_center_window(HWND hwnd, HWND parent) {
    if (!hwnd) return;

    RECT window_rect, parent_rect;
    GetWindowRect(hwnd, &window_rect);

    int width = window_rect.right - window_rect.left;
    int height = window_rect.bottom - window_rect.top;

    if (parent) {
        GetWindowRect(parent, &parent_rect);
    } else {
        parent_rect.left = 0;
        parent_rect.top = 0;
        parent_rect.right = GetSystemMetrics(SM_CXSCREEN);
        parent_rect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    int parent_width = parent_rect.right - parent_rect.left;
    int parent_height = parent_rect.bottom - parent_rect.top;

    int x = parent_rect.left + (parent_width - width) / 2;
    int y = parent_rect.top + (parent_height - height) / 2;

    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

HMONITOR dialog_get_monitor(HWND hwnd) {
    return MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
}

bool dialog_get_work_area(HMONITOR monitor, RECT* work_area) {
    if (!monitor || !work_area) return false;

    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(monitor, &mi)) {
        *work_area = mi.rcWork;
        return true;
    }

    return false;
}

// Animation Utilities

void dialog_animate_fade_in(HWND hwnd, DWORD duration) {
    if (!hwnd) return;

    // Set initial transparency
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);

    ShowWindow(hwnd, SW_SHOW);

    // Animate to full opacity
    for (int alpha = 0; alpha <= 255; alpha += 15) {
        SetLayeredWindowAttributes(hwnd, 0, (BYTE)alpha, LWA_ALPHA);
        Sleep(duration / 17); // ~60fps
    }

    // Remove layered style for better performance
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
    RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
}

void dialog_animate_fade_out(HWND hwnd, DWORD duration) {
    if (!hwnd) return;

    // Set layered style
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    // Animate to transparent
    for (int alpha = 255; alpha >= 0; alpha -= 15) {
        SetLayeredWindowAttributes(hwnd, 0, (BYTE)alpha, LWA_ALPHA);
        Sleep(duration / 17); // ~60fps
    }
}

// Color Utilities

COLORREF dialog_blend_colors(COLORREF color1, COLORREF color2, float alpha) {
    if (alpha <= 0.0f) return color1;
    if (alpha >= 1.0f) return color2;

    int r1 = GetRValue(color1);
    int g1 = GetGValue(color1);
    int b1 = GetBValue(color1);

    int r2 = GetRValue(color2);
    int g2 = GetGValue(color2);
    int b2 = GetBValue(color2);

    int r = (int)(r1 + (r2 - r1) * alpha);
    int g = (int)(g1 + (g2 - g1) * alpha);
    int b = (int)(b1 + (b2 - b1) * alpha);

    return RGB(r, g, b);
}

COLORREF dialog_lighten_color(COLORREF color, float amount) {
    return dialog_blend_colors(color, RGB(255, 255, 255), amount);
}

COLORREF dialog_darken_color(COLORREF color, float amount) {
    return dialog_blend_colors(color, RGB(0, 0, 0), amount);
}

// Control Utilities

HWND dialog_create_button(HWND parent, const char* text, int x, int y, 
                         int width, int height, int id, bool is_default) {
    if (!parent || !text) return NULL;

    wchar_t* wide_text = dialog_utf8_to_wide(text);
    if (!wide_text) return NULL;

    DWORD style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
    if (is_default) {
        style |= BS_DEFPUSHBUTTON;
    }

    HWND button = CreateWindowExW(0, L"BUTTON", wide_text, style,
                                 x, y, width, height, parent, 
                                 (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);

    free(wide_text);

    if (button) {
        // Set modern font
        HFONT font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        if (font) {
            SendMessage(button, WM_SETFONT, (WPARAM)font, TRUE);
        }
    }

    return button;
}

HWND dialog_create_label(HWND parent, const char* text, int x, int y, 
                        int width, int height, int id) {
    if (!parent || !text) return NULL;

    wchar_t* wide_text = dialog_utf8_to_wide(text);
    if (!wide_text) return NULL;

    HWND label = CreateWindowExW(0, L"STATIC", wide_text, 
                                WS_CHILD | WS_VISIBLE | SS_LEFT,
                                x, y, width, height, parent, 
                                (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);

    free(wide_text);

    if (label) {
        // Set modern font
        HFONT font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        if (font) {
            SendMessage(label, WM_SETFONT, (WPARAM)font, TRUE);
        }
    }

    return label;
}