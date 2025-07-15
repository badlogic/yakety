#ifndef MENU_H
#define MENU_H

#include <stdbool.h>

#define MAX_MENU_ITEMS 20

typedef void (*MenuCallback)(void);

typedef struct {
    const char *title;
    MenuCallback callback;
    bool is_separator;
} MenuItem;

typedef struct {
    MenuItem *items;
    int item_count;
    int max_items;
} MenuSystem;

// Singleton menu system API
int menu_init(void);
void menu_cleanup(void);

// Show the menu (creates the system tray/menubar with standard items)
int menu_show(void);

// Show the context menu programmatically (as if user clicked the tray icon)
void menu_show_context_menu(void);

// Hide the menu
void menu_hide(void);

// Update a menu item's title by index
void menu_update_item(int index, const char *new_title);

// Enable/disable the entire menu
void menu_set_enabled(bool enabled);

// Internal functions (for platform implementations)
MenuSystem *menu_create(void);
int menu_add_item(MenuSystem *menu, const char *title, MenuCallback callback);
int menu_add_separator(MenuSystem *menu);
void menu_destroy(MenuSystem *menu);

// Shared menu setup (called by platform implementations)
int menu_setup_items(MenuSystem *menu);

// Global launch menu index (for platform implementations)
extern int g_launch_menu_index;

// Function to reload hotkey settings (implemented in main.c)
void reload_hotkey_settings(void);

#endif // MENU_H