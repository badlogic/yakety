#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../overlay.h"

#define OVERLAY_CLASS_NAME L"YaketyOverlay"
#define OVERLAY_WIDTH 180
#define OVERLAY_HEIGHT 40

static HWND g_overlay_window = NULL;
static HINSTANCE g_instance = NULL;
static wchar_t g_display_text[256] = L"";
static COLORREF g_text_color = RGB(255, 255, 255);
static COLORREF g_bg_color = RGB(0, 0, 0);  // Pure black like macOS

// Window procedure for the overlay
LRESULT CALLBACK overlay_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Create rounded rectangle region (12px radius like macOS)
            HRGN hRgn = CreateRoundRectRgn(0, 0, OVERLAY_WIDTH, OVERLAY_HEIGHT, 12, 12);
            SetWindowRgn(hwnd, hRgn, TRUE);
            
            // Fill background
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH brush = CreateSolidBrush(g_bg_color);
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            
            // Draw text
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, g_text_color);
            
            HFONT font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
            HFONT oldFont = (HFONT)SelectObject(hdc, font);
            
            DrawTextW(hdc, g_display_text, -1, &rect, 
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
        // Set transparency to match macOS (75% opacity = 192/255)
        SetLayeredWindowAttributes(g_overlay_window, 0, 192, LWA_ALPHA);
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
}

void overlay_show(const char* text) {
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


void overlay_hide(void) {
    if (g_overlay_window) {
        ShowWindow(g_overlay_window, SW_HIDE);
    }
}