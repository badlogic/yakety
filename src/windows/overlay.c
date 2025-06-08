#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../overlay.h"

#define OVERLAY_CLASS_NAME L"YaketyOverlay"
#define OVERLAY_WIDTH 140
#define OVERLAY_HEIGHT 36
#define ICON_SIZE 20
#define ICON_MARGIN 8
#define TEXT_LEFT_MARGIN 32

static HWND g_overlay_window = NULL;
static HINSTANCE g_instance = NULL;
static wchar_t g_display_text[256] = L"";
static COLORREF g_text_color = RGB(255, 255, 255);
static COLORREF g_bg_color = RGB(0, 0, 0);  // Black background
static COLORREF g_border_color = RGB(0x22, 0xC5, 0x5E);  // Yakety green
static HICON g_app_icon = NULL;

// Window procedure for the overlay
LRESULT CALLBACK overlay_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Create rounded rectangle region (10px radius like macOS)
            HRGN hRgn = CreateRoundRectRgn(0, 0, OVERLAY_WIDTH, OVERLAY_HEIGHT, 10, 10);
            SetWindowRgn(hwnd, hRgn, TRUE);
            
            // Fill background
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH brush = CreateSolidBrush(g_bg_color);
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            
            // Draw green border
            HPEN pen = CreatePen(PS_SOLID, 1, g_border_color);
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nullBrush);
            
            RoundRect(hdc, 1, 1, OVERLAY_WIDTH - 1, OVERLAY_HEIGHT - 1, 10, 10);
            
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(pen);
            
            // Draw icon if available (TODO: tint it green)
            if (g_app_icon) {
                int iconY = (OVERLAY_HEIGHT - ICON_SIZE) / 2;
                DrawIconEx(hdc, ICON_MARGIN, iconY, 
                          g_app_icon, ICON_SIZE, ICON_SIZE, 0, NULL, DI_NORMAL);
            }
            
            // Draw text (adjusted for icon)
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, g_text_color);
            
            HFONT font = CreateFontW(14, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
            HFONT oldFont = (HFONT)SelectObject(hdc, font);
            
            // Adjust text rectangle to account for icon
            RECT textRect = rect;
            textRect.left = g_app_icon ? TEXT_LEFT_MARGIN : 8;
            
            DrawTextW(hdc, g_display_text, -1, &textRect, 
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdc, oldFont);
            DeleteObject(font);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void create_overlay_window() {
    if (g_overlay_window) return;
    
    g_instance = GetModuleHandle(NULL);
    
    // Load application icon
    g_app_icon = LoadIcon(g_instance, MAKEINTRESOURCE(1));
    if (!g_app_icon) {
        // Fallback to default application icon
        g_app_icon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
    // Register window class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = overlay_proc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;  // No background brush to avoid white borders
    wc.lpszClassName = OVERLAY_CLASS_NAME;
    
    RegisterClassExW(&wc);
    
    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Calculate position (bottom center, like macOS)
    int x = (screenWidth - OVERLAY_WIDTH) / 2;
    int y = screenHeight - OVERLAY_HEIGHT - 30;  // 30 pixels from bottom
    
    // Create window (WS_POPUP with no borders, like macOS)
    g_overlay_window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        OVERLAY_CLASS_NAME,
        L"Yakety",
        WS_POPUP,
        x, y, OVERLAY_WIDTH, OVERLAY_HEIGHT,
        NULL, NULL, g_instance, NULL
    );
    
    if (g_overlay_window) {
        // Make window fully opaque for solid black background
        SetLayeredWindowAttributes(g_overlay_window, 0, 255, LWA_ALPHA);
    }
}

void overlay_init(void) {
    // Create overlay window on first use
}

void overlay_cleanup(void) {
    if (g_overlay_window) {
        DestroyWindow(g_overlay_window);
        g_overlay_window = NULL;
    }
    if (g_app_icon) {
        DestroyIcon(g_app_icon);
        g_app_icon = NULL;
    }
}

void overlay_show(const char* text) {
    if (!text) return;
    
    create_overlay_window();
    if (!g_overlay_window) return;
    
    // Convert text to wide string
    wchar_t wtext[256];
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 256);
    
    wcscpy_s(g_display_text, 256, wtext);
    g_bg_color = RGB(0, 0, 0);  // Pure black like macOS
    g_text_color = RGB(255, 255, 255);
    
    InvalidateRect(g_overlay_window, NULL, TRUE);
    ShowWindow(g_overlay_window, SW_SHOWNOACTIVATE);
    UpdateWindow(g_overlay_window);
}


void overlay_show_error(const char* text) {
    if (!text) return;
    
    create_overlay_window();
    if (!g_overlay_window) return;
    
    // Convert text to wide string
    wchar_t wtext[256];
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 256);
    
    wcscpy_s(g_display_text, 256, wtext);
    g_bg_color = RGB(0, 0, 0);  // Black background
    g_text_color = RGB(255, 255, 255);
    g_border_color = RGB(0xEF, 0x44, 0x44);  // Error red
    
    InvalidateRect(g_overlay_window, NULL, TRUE);
    ShowWindow(g_overlay_window, SW_SHOWNOACTIVATE);
    UpdateWindow(g_overlay_window);
}

void overlay_hide(void) {
    if (g_overlay_window) {
        ShowWindow(g_overlay_window, SW_HIDE);
        // Reset to default green for next time
        g_border_color = RGB(0x22, 0xC5, 0x5E);
    }
}