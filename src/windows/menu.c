#include "../menu.h"
#include "../logging.h"
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_BASE 1000

struct MenuSystem {
    MenuItem* items;
    int item_count;
    int max_items;
    HWND hidden_window;
    NOTIFYICONDATAW tray_icon;
    HMENU tray_menu;
};

static const wchar_t* WINDOW_CLASS_NAME = L"YaketyMenuWindow";

// Window procedure for hidden window
static LRESULT CALLBACK menu_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MenuSystem* menu = (MenuSystem*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    switch (msg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP && menu) {
                // Show context menu
                POINT pt;
                GetCursorPos(&pt);
                
                SetForegroundWindow(hwnd);  // Required for menu to disappear when clicking away
                
                TrackPopupMenu(menu->tray_menu, TPM_RIGHTBUTTON, 
                             pt.x, pt.y, 0, hwnd, NULL);
                
                PostMessage(hwnd, WM_NULL, 0, 0);  // Force menu to disappear
            }
            break;
            
        case WM_COMMAND:
            if (menu) {
                int item_id = LOWORD(wParam) - ID_TRAY_BASE;
                if (item_id >= 0 && item_id < menu->item_count) {
                    MenuItem* item = &menu->items[item_id];
                    if (item->callback && !item->is_separator) {
                        item->callback();
                    }
                }
            }
            break;
            
        case WM_DESTROY:
            if (menu) {
                // Remove tray icon
                Shell_NotifyIconW(NIM_DELETE, &menu->tray_icon);
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
    menu->hidden_window = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        L"Yakety Menu",
        0,
        0, 0, 0, 0,
        NULL, NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!menu->hidden_window) {
        log_error("Failed to create menu window");
        return -1;
    }
    
    // Store menu pointer in window
    SetWindowLongPtr(menu->hidden_window, GWLP_USERDATA, (LONG_PTR)menu);
    
    // Create tray menu
    menu->tray_menu = CreatePopupMenu();
    
    // Add menu items
    for (int i = 0; i < menu->item_count; i++) {
        MenuItem* item = &menu->items[i];
        if (item->is_separator) {
            AppendMenuW(menu->tray_menu, MF_SEPARATOR, 0, NULL);
        } else if (item->title) {
            // Convert title to wide string
            int len = MultiByteToWideChar(CP_UTF8, 0, item->title, -1, NULL, 0);
            wchar_t* wide_title = (wchar_t*)malloc(len * sizeof(wchar_t));
            if (wide_title) {
                MultiByteToWideChar(CP_UTF8, 0, item->title, -1, wide_title, len);
                AppendMenuW(menu->tray_menu, MF_STRING, ID_TRAY_BASE + i, wide_title);
                free(wide_title);
            }
        }
    }
    
    // Create tray icon
    menu->tray_icon.cbSize = sizeof(NOTIFYICONDATAW);
    menu->tray_icon.hWnd = menu->hidden_window;
    menu->tray_icon.uID = 1;
    menu->tray_icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    menu->tray_icon.uCallbackMessage = WM_TRAYICON;
    
    // Load application icon from resources
    HINSTANCE hInstance = GetModuleHandle(NULL);
    menu->tray_icon.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    
    // Fallback to default if custom icon not found
    if (!menu->tray_icon.hIcon) {
        menu->tray_icon.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
    wcscpy_s(menu->tray_icon.szTip, 128, L"Yakety - Right Ctrl to record");
    
    // Add to system tray
    if (!Shell_NotifyIconW(NIM_ADD, &menu->tray_icon)) {
        log_error("Failed to add tray icon");
        DestroyWindow(menu->hidden_window);
        menu->hidden_window = NULL;
        return -1;
    }
    
    log_info("Menu shown in system tray");
    return 0;
}

void menu_hide(MenuSystem* menu) {
    if (!menu) return;
    
    if (menu->tray_icon.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &menu->tray_icon);
    }
    
    if (menu->hidden_window) {
        DestroyWindow(menu->hidden_window);
        menu->hidden_window = NULL;
    }
    
    log_info("Menu hidden");
}

void menu_destroy(MenuSystem* menu) {
    if (!menu) return;
    
    menu_hide(menu);
    
    if (menu->tray_menu) {
        DestroyMenu(menu->tray_menu);
        menu->tray_menu = NULL;
    }
    
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