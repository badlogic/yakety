#import <Cocoa/Cocoa.h>
#include "../menu.h"
#include "../dialog.h"
#include <stdlib.h>
#include <string.h>

static NSStatusItem* statusItem = nil;
static MenuSystem* g_menu_system = NULL;

@interface MenuBarDelegate : NSObject {
    MenuSystem* menuSystem;
}
- (instancetype)initWithMenuSystem:(MenuSystem*)system;
- (void)handleMenuItem:(id)sender;
@end

@implementation MenuBarDelegate

- (instancetype)initWithMenuSystem:(MenuSystem*)system {
    self = [super init];
    if (self) {
        menuSystem = system;
    }
    return self;
}

- (void)handleMenuItem:(id)sender {
    NSMenuItem* item = (NSMenuItem*)sender;
    int tag = (int)[item tag];
    
    // Find the menu item and call its callback
    for (int i = 0; i < menuSystem->item_count; i++) {
        if (i == tag && menuSystem->items[i].callback) {
            menuSystem->items[i].callback();
            break;
        }
    }
}

@end

static MenuBarDelegate* menuDelegate = nil;

MenuSystem* menu_create(void) {
    MenuSystem* menu = (MenuSystem*)calloc(1, sizeof(MenuSystem));
    if (!menu) return NULL;
    
    menu->items = (MenuItem*)calloc(MAX_MENU_ITEMS, sizeof(MenuItem));
    if (!menu->items) {
        free(menu);
        return NULL;
    }
    
    menu->max_items = MAX_MENU_ITEMS;
    menu->item_count = 0;
    
    return menu;
}

void menu_add_item(MenuSystem* menu, const char* title, MenuCallback callback) {
    if (!menu || !title || menu->item_count >= menu->max_items) {
        return;
    }
    
    MenuItem* item = &menu->items[menu->item_count];
    item->title = strdup(title);
    item->callback = callback;
    item->is_separator = false;
    menu->item_count++;
}

void menu_add_separator(MenuSystem* menu) {
    if (!menu || menu->item_count >= menu->max_items) {
        return;
    }
    
    MenuItem* item = &menu->items[menu->item_count];
    item->title = NULL;
    item->callback = NULL;
    item->is_separator = true;
    menu->item_count++;
}

int menu_show(MenuSystem* menu) {
    if (!menu || g_menu_system) {
        return -1; // Already showing a menu
    }
    
    g_menu_system = menu;
    
    @autoreleasepool {
        NSStatusBar *statusBar = [NSStatusBar systemStatusBar];
        statusItem = [statusBar statusItemWithLength:NSVariableStatusItemLength];
        
        if (!statusItem) {
            return -1;
        }
        
        // Load icon image
        NSString* iconPath = [[NSBundle mainBundle] pathForResource:@"menubar" ofType:@"png"];
        NSImage* icon = nil;
        
        if (iconPath) {
            icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
            [icon setTemplate:YES]; // Makes it adapt to dark/light mode
            statusItem.button.image = icon;
        } else {
            // Fallback to emoji if icon not found
            statusItem.button.title = @"🎤";
        }
        
        // Create menu
        NSMenu* nsMenu = [[NSMenu alloc] init];
        menuDelegate = [[MenuBarDelegate alloc] initWithMenuSystem:menu];
        
        // Add all menu items
        for (int i = 0; i < menu->item_count; i++) {
            if (menu->items[i].is_separator) {
                [nsMenu addItem:[NSMenuItem separatorItem]];
            } else {
                NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:menu->items[i].title]
                                                             action:@selector(handleMenuItem:)
                                                      keyEquivalent:@""];
                [item setTarget:menuDelegate];
                [item setTag:i];
                [nsMenu addItem:item];
            }
        }
        
        statusItem.menu = nsMenu;
        statusItem.visible = YES;
    }
    
    return 0;
}

void menu_hide(MenuSystem* menu) {
    if (!menu || menu != g_menu_system) {
        return;
    }
    
    if (statusItem) {
        [[NSStatusBar systemStatusBar] removeStatusItem:statusItem];
        statusItem = nil;
        menuDelegate = nil;
    }
    
    g_menu_system = NULL;
}

void menu_destroy(MenuSystem* menu) {
    if (!menu) return;
    
    // Hide if currently showing
    if (menu == g_menu_system) {
        menu_hide(menu);
    }
    
    // Free all titles
    for (int i = 0; i < menu->item_count; i++) {
        if (menu->items[i].title) {
            free((void*)menu->items[i].title);
        }
    }
    
    free(menu->items);
    free(menu);
}