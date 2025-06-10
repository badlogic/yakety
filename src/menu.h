#ifndef MENU_H
#define MENU_H

#include <stdbool.h>

// Forward declarations - none needed now

#define MAX_MENU_ITEMS 20

typedef void (*MenuCallback)(void);

typedef struct {
    const char* title;
    MenuCallback callback;
    bool is_separator;
} MenuItem;

typedef struct {
    MenuItem* items;
    int item_count;
    int max_items;
} MenuSystem;

// Singleton menu system API
int menu_init(void);
void menu_cleanup(void);

// Show the menu (creates the system tray/menubar with standard items)
int menu_show(void);

// Hide the menu
void menu_hide(void);

// Update a menu item's title by index
void menu_update_item(int index, const char* new_title);

// Internal functions (for platform implementations)
MenuSystem* menu_create(void);
int menu_add_item(MenuSystem* menu, const char* title, MenuCallback callback);
int menu_add_separator(MenuSystem* menu);
void menu_destroy(MenuSystem* menu);

// Shared menu setup (called by platform implementations)
int menu_setup_items(MenuSystem* menu);

// Global launch menu index (for platform implementations)
extern int g_launch_menu_index;

#endif // MENU_H