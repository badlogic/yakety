#include "../menu.h"
#include "../logging.h"
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_BASE 1000

static HWND g_hidden_window = NULL;
static NOTIFYICONDATAW g_tray_icon = {0};
static HMENU g_tray_menu = NULL;
static MenuSystem* g_menu_system = NULL;

static const wchar_t* WINDOW_CLASS_NAME = L"YaketyMenuWindow";

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
            if (g_menu_system) {
                int item_id = LOWORD(wParam) - ID_TRAY_BASE;
                if (item_id >= 0 && item_id < g_menu_system->item_count) {
                    MenuItem* item = &g_menu_system->items[item_id];
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

void menu_add_item(MenuSystem* menu, const char* title, MenuCallback callback) {
    if (!menu || menu->item_count >= menu->max_items) {
        log_error("Cannot add menu item: menu full or invalid");
        return;
    }
    
    MenuItem* item = &menu->items[menu->item_count];
    item->title = _strdup(title);
    item->callback = callback;
    item->is_separator = false;
    menu->item_count++;
}

void menu_add_separator(MenuSystem* menu) {
    if (!menu || menu->item_count >= menu->max_items) {
        log_error("Cannot add separator: menu full or invalid");
        return;
    }
    
    MenuItem* item = &menu->items[menu->item_count];
    item->title = NULL;
    item->callback = NULL;
    item->is_separator = true;
    menu->item_count++;
}

int menu_show(MenuSystem* menu) {
    if (!menu) {
        log_error("Invalid menu system");
        return -1;
    }
    
    g_menu_system = menu;
    
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
    
    // Create tray menu
    g_tray_menu = CreatePopupMenu();
    
    // Add menu items
    for (int i = 0; i < menu->item_count; i++) {
        MenuItem* item = &menu->items[i];
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
    
    log_info("Menu shown in system tray");
    return 0;
}

void menu_hide(MenuSystem* menu) {
    if (!menu) return;
    
    if (g_tray_icon.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &g_tray_icon);
    }
    
    if (g_hidden_window) {
        DestroyWindow(g_hidden_window);
        g_hidden_window = NULL;
    }
    
    log_info("Menu hidden");
}

void menu_update_item(MenuSystem* menu, int index, const char* new_title) {
    if (!menu || index < 0 || index >= menu->item_count || !new_title) {
        return;
    }
    
    // Update the stored title
    if (menu->items[index].title) {
        free((void*)menu->items[index].title);
    }
    menu->items[index].title = _strdup(new_title);
    
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
    
    menu_hide(menu);
    
    if (g_tray_menu) {
        DestroyMenu(g_tray_menu);
        g_tray_menu = NULL;
    }
    
    g_menu_system = NULL;
    
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