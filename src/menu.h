#ifndef MENU_H
#define MENU_H

#include <stdbool.h>

// Forward declarations
struct Config;

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

// Create a new menu system
MenuSystem* menu_create(void);

// Add a menu item
void menu_add_item(MenuSystem* menu, const char* title, MenuCallback callback);

// Add a separator
void menu_add_separator(MenuSystem* menu);

// Show the menu (creates the system tray/menubar)
int menu_show(MenuSystem* menu);

// Hide the menu
void menu_hide(MenuSystem* menu);

// Update a menu item's title
void menu_update_item(MenuSystem* menu, int index, const char* new_title);

// Destroy the menu system
void menu_destroy(MenuSystem* menu);

// Initialize menu system with external state (call before menu_setup)
void menu_init(struct Config* config, volatile bool* running);

// Create and configure the application menu with all standard items
MenuSystem* menu_setup(void);

#endif // MENU_H