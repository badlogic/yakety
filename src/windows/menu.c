#include "../menu.h"
#include "../logging.h"
#include "../utils.h"
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
#include <uxtheme.h>
#include <dwmapi.h>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_BASE 1000

// Dark mode support - undocumented Windows APIs
typedef enum {
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
    Max
} PreferredAppMode;

typedef BOOL (WINAPI *fnAllowDarkModeForWindow)(HWND hWnd, BOOL allow);
typedef PreferredAppMode (WINAPI *fnSetPreferredAppMode)(PreferredAppMode appMode);
typedef void (WINAPI *fnFlushMenuThemes)(void);
typedef BOOL (WINAPI *fnShouldAppsUseDarkMode)(void);

static fnAllowDarkModeForWindow AllowDarkModeForWindow = NULL;
static fnSetPreferredAppMode SetPreferredAppMode = NULL;
static fnFlushMenuThemes FlushMenuThemes = NULL;
static fnShouldAppsUseDarkMode ShouldAppsUseDarkMode = NULL;

static HWND g_hidden_window = NULL;
static NOTIFYICONDATAW g_tray_icon = {0};
static HMENU g_tray_menu = NULL;
static bool g_menu_showing = false;

static const wchar_t* WINDOW_CLASS_NAME = L"YaketyMenuWindow";

// Initialize dark mode APIs
static void init_dark_mode() {
    HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!hUxtheme) return;
    
    AllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133));
    SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
    FlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136));
    ShouldAppsUseDarkMode = (fnShouldAppsUseDarkMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132));
    
    // Enable dark mode for the app
    if (SetPreferredAppMode && ShouldAppsUseDarkMode && ShouldAppsUseDarkMode()) {
        SetPreferredAppMode(AllowDark);
        if (FlushMenuThemes) {
            FlushMenuThemes();
        }
    }
}

// Window procedure for hidden window
static LRESULT CALLBACK menu_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP && g_tray_menu) {
                // Show context menu
                POINT pt;
                GetCursorPos(&pt);
                
                SetForegroundWindow(hwnd);  // Required for menu to disappear when clicking away
                
                TrackPopupMenu(g_tray_menu, TPM_RIGHTBUTTON, 
                             pt.x, pt.y, 0, hwnd, NULL);
                
                PostMessage(hwnd, WM_NULL, 0, 0);  // Force menu to disappear
            }
            break;
            
        case WM_COMMAND:
            if (g_menu) {
                int item_id = LOWORD(wParam) - ID_TRAY_BASE;
                if (item_id >= 0 && item_id < g_menu->item_count) {
                    MenuItem* item = &g_menu->items[item_id];
                    if (item->callback && !item->is_separator) {
                        item->callback();
                    }
                }
            }
            break;
            
        case WM_DESTROY:
            if (g_tray_icon.cbSize > 0) {
                // Remove tray icon
                Shell_NotifyIconW(NIM_DELETE, &g_tray_icon);
            }
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return 0;
}

MenuSystem* menu_create(void) {
    MenuSystem* menu = (MenuSystem*)calloc(1, sizeof(MenuSystem));
    if (!menu) {
        log_error("Failed to allocate menu system");
        return NULL;
    }
    
    menu->max_items = MAX_MENU_ITEMS;
    menu->items = (MenuItem*)calloc(menu->max_items, sizeof(MenuItem));
    if (!menu->items) {
        log_error("Failed to allocate menu items");
        free(menu);
        return NULL;
    }
    
    return menu;
}

int menu_add_item(MenuSystem* menu, const char* title, MenuCallback callback) {
    if (!menu || menu->item_count >= menu->max_items) {
        log_error("Cannot add menu item: menu full or invalid");
        return -1;
    }
    
    MenuItem* item = &menu->items[menu->item_count];
    item->title = utils_strdup(title);
    item->callback = callback;
    item->is_separator = false;
    return menu->item_count++;
}

int menu_add_separator(MenuSystem* menu) {
    if (!menu || menu->item_count >= menu->max_items) {
        log_error("Cannot add separator: menu full or invalid");
        return -1;
    }
    
    MenuItem* item = &menu->items[menu->item_count];
    item->title = NULL;
    item->callback = NULL;
    item->is_separator = true;
    return menu->item_count++;
}

// Singleton menu system state
static MenuSystem* g_menu = NULL;

int menu_init(void) {
    if (g_menu) {
        return -1; // Already initialized
    }
    
    g_menu = menu_create();
    if (!g_menu) {
        return -1;
    }
    
    return menu_setup_items(g_menu);
}

void menu_cleanup(void) {
    if (g_menu) {
        menu_destroy(g_menu);
        g_menu = NULL;
    }
}

int menu_show(void) {
    if (!g_menu || g_menu_showing) {
        return -1;
    }
    
    // Initialize dark mode support
    init_dark_mode();
    
    // Register window class if not already registered
    WNDCLASSEXW wc = {0};
    if (!GetClassInfoExW(GetModuleHandle(NULL), WINDOW_CLASS_NAME, &wc)) {
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = menu_window_proc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = WINDOW_CLASS_NAME;
        
        if (!RegisterClassExW(&wc)) {
            log_error("Failed to register menu window class");
            return -1;
        }
    }
    
    // Create hidden window
    g_hidden_window = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        L"Yakety Menu",
        0,
        0, 0, 0, 0,
        NULL, NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!g_hidden_window) {
        log_error("Failed to create menu window");
        return -1;
    }
    
    // Enable dark mode for window if available
    if (AllowDarkModeForWindow) {
        AllowDarkModeForWindow(g_hidden_window, TRUE);
        SetWindowTheme(g_hidden_window, L"Explorer", NULL);
        
        // Also enable dark mode for title bar
        BOOL value = TRUE;
        DwmSetWindowAttribute(g_hidden_window, 20, &value, sizeof(value)); // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    }
    
    // Create tray menu
    g_tray_menu = CreatePopupMenu();
    
    // Add menu items
    for (int i = 0; i < g_menu->item_count; i++) {
        MenuItem* item = &g_menu->items[i];
        if (item->is_separator) {
            AppendMenuW(g_tray_menu, MF_SEPARATOR, 0, NULL);
        } else if (item->title) {
            // Convert title to wide string
            int len = MultiByteToWideChar(CP_UTF8, 0, item->title, -1, NULL, 0);
            wchar_t* wide_title = (wchar_t*)malloc(len * sizeof(wchar_t));
            if (wide_title) {
                MultiByteToWideChar(CP_UTF8, 0, item->title, -1, wide_title, len);
                AppendMenuW(g_tray_menu, MF_STRING, ID_TRAY_BASE + i, wide_title);
                free(wide_title);
            }
        }
    }
    
    // Create tray icon
    g_tray_icon.cbSize = sizeof(NOTIFYICONDATAW);
    g_tray_icon.hWnd = g_hidden_window;
    g_tray_icon.uID = 1;
    g_tray_icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_tray_icon.uCallbackMessage = WM_TRAYICON;
    
    // Load application icon from resources
    HINSTANCE hInstance = GetModuleHandle(NULL);
    g_tray_icon.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    
    // Fallback to default if custom icon not found
    if (!g_tray_icon.hIcon) {
        g_tray_icon.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
    wcscpy_s(g_tray_icon.szTip, 128, L"Yakety - Right Ctrl to record");
    
    // Add to system tray
    if (!Shell_NotifyIconW(NIM_ADD, &g_tray_icon)) {
        log_error("Failed to add tray icon");
        DestroyWindow(g_hidden_window);
        g_hidden_window = NULL;
        return -1;
    }
    
    g_menu_showing = true;
    log_info("Menu shown in system tray");
    return 0;
}

void menu_hide(void) {
    if (!g_menu || !g_menu_showing) return;
    
    if (g_tray_icon.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &g_tray_icon);
    }
    
    if (g_hidden_window) {
        DestroyWindow(g_hidden_window);
        g_hidden_window = NULL;
    }
    
    g_menu_showing = false;
    log_info("Menu hidden");
}

void menu_update_item(int index, const char* new_title) {
    if (!g_menu || index < 0 || index >= g_menu->item_count || !new_title) {
        return;
    }
    
    // Update the stored title
    if (g_menu->items[index].title) {
        free((void*)g_menu->items[index].title);
    }
    g_menu->items[index].title = utils_strdup(new_title);
    
    // If menu is currently showing, update the actual menu item
    if (g_tray_menu) {
        // Convert title to wide string
        int len = MultiByteToWideChar(CP_UTF8, 0, new_title, -1, NULL, 0);
        wchar_t* wide_title = (wchar_t*)malloc(len * sizeof(wchar_t));
        if (wide_title) {
            MultiByteToWideChar(CP_UTF8, 0, new_title, -1, wide_title, len);
            ModifyMenuW(g_tray_menu, ID_TRAY_BASE + index, MF_BYCOMMAND | MF_STRING, ID_TRAY_BASE + index, wide_title);
            free(wide_title);
        }
    }
}

void menu_destroy(MenuSystem* menu) {
    if (!menu) return;
    
    // Hide if currently showing
    if (g_menu_showing) {
        menu_hide();
    }
    
    if (g_tray_menu) {
        DestroyMenu(g_tray_menu);
        g_tray_menu = NULL;
    }
    
    g_menu_showing = false;
    
    // Free menu item titles
    for (int i = 0; i < menu->item_count; i++) {
        if (menu->items[i].title) {
            free((void*)menu->items[i].title);
        }
    }
    
    free(menu->items);
    free(menu);
    
    log_info("Menu destroyed");
}