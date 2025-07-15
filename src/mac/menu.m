#include "../menu.h"
#include "../dialog.h"
#include "../logging.h"
#include "../utils.h"
#include "dispatch.h"
#import <Cocoa/Cocoa.h>
#include <stdlib.h>
#include <string.h>

static NSStatusItem *statusItem = nil;
static NSMenu *statusMenu = nil;
static NSImage *statusIcon = nil;
static bool g_menu_showing = false;

@interface MenuBarDelegate : NSObject {
    MenuSystem *menuSystem;
}
- (instancetype)initWithMenuSystem:(MenuSystem *)system;
- (void)handleMenuItem:(id)sender;
@end

@implementation MenuBarDelegate

- (instancetype)initWithMenuSystem:(MenuSystem *)system {
    self = [super init];
    if (self) {
        menuSystem = system;
    }
    return self;
}

- (void)handleMenuItem:(id)sender {
    NSMenuItem *item = (NSMenuItem *) sender;
    int tag = (int) [item tag];

    // Find the menu item and call its callback
    for (int i = 0; i < menuSystem->item_count; i++) {
        if (i == tag && menuSystem->items[i].callback) {
            menuSystem->items[i].callback();
            break;
        }
    }
}

@end

static MenuBarDelegate *menuDelegate = nil;

MenuSystem *menu_create(void) {
    MenuSystem *menu = (MenuSystem *) calloc(1, sizeof(MenuSystem));
    if (!menu)
        return NULL;

    menu->items = (MenuItem *) calloc(MAX_MENU_ITEMS, sizeof(MenuItem));
    if (!menu->items) {
        free(menu);
        return NULL;
    }

    menu->max_items = MAX_MENU_ITEMS;
    menu->item_count = 0;

    return menu;
}

int menu_add_item(MenuSystem *menu, const char *title, MenuCallback callback) {
    if (!menu || !title || menu->item_count >= menu->max_items) {
        return -1;
    }

    MenuItem *item = &menu->items[menu->item_count];
    item->title = utils_strdup(title);
    item->callback = callback;
    item->is_separator = false;
    return menu->item_count++;
}

int menu_add_separator(MenuSystem *menu) {
    if (!menu || menu->item_count >= menu->max_items) {
        return -1;
    }

    MenuItem *item = &menu->items[menu->item_count];
    item->title = NULL;
    item->callback = NULL;
    item->is_separator = true;
    return menu->item_count++;
}

// Singleton menu system state
static MenuSystem *g_menu = NULL;

MenuSystem *menu_get_system(void) {
    return g_menu;
}

int menu_init(void) {
    if (g_menu) {
        return -1; // Already initialized
    }

    g_menu = menu_create();
    if (!g_menu) {
        return -1;
    }

    return 0;
}

void menu_cleanup(void) {
    if (g_menu) {
        menu_destroy(g_menu);
        g_menu = NULL;
    }
}

int menu_show(void) {
    if (!g_menu || g_menu_showing) {
        return -1; // Already showing a menu
    }

    g_menu_showing = true;

    // Create a block to ensure menu creation happens on main thread
    void (^createMenuBlock)(void) = ^{
      log_info("Creating status bar item...");

      // Make sure NSApp is initialized
      [NSApplication sharedApplication];

      NSStatusBar *statusBar = [NSStatusBar systemStatusBar];
      statusItem = [statusBar statusItemWithLength:NSVariableStatusItemLength];

      if (!statusItem) {
          log_error("Failed to create status item!");
          return;
      }

      // CRITICAL: Retain the status item to prevent it from being deallocated
      [statusItem retain];

      log_info("Created status item successfully");

      // First set the button to have some content so it appears
      statusItem.button.title = @""; // Empty title to start

      // Load icon image
      NSString *iconPath = [[NSBundle mainBundle] pathForResource:@"menubar" ofType:@"png"];

      if (iconPath) {
          statusIcon = [[NSImage alloc] initWithContentsOfFile:iconPath];
          if (statusIcon) {
              [statusIcon retain];
              [statusIcon setTemplate:YES];            // Makes it adapt to dark/light mode
              [statusIcon setSize:NSMakeSize(18, 18)]; // Standard menu bar icon size
              statusItem.button.image = statusIcon;
              log_info("Loaded menubar icon from bundle: %s", [iconPath UTF8String]);

              // Debug: check icon properties
              log_info("Icon size: %.0fx%.0f", statusIcon.size.width, statusIcon.size.height);
              log_info("Button frame: %.0fx%.0f", statusItem.button.frame.size.width,
                       statusItem.button.frame.size.height);
          } else {
              log_error("Failed to load icon from path: %s", [iconPath UTF8String]);
              statusItem.button.title = @"🎤";
          }
      } else {
          // Fallback to emoji if icon not found
          statusItem.button.title = @"🎤";
          log_info("Icon not found in bundle, using emoji fallback");

          // List bundle resources for debugging
          NSArray *pngFiles = [[NSBundle mainBundle] pathsForResourcesOfType:@"png" inDirectory:nil];
          log_info("PNG files in bundle: %lu", (unsigned long) [pngFiles count]);
          for (NSString *path in pngFiles) {
              log_info("  - %s", [path UTF8String]);
          }
      }

      // Create menu - already retained by alloc/init
      statusMenu = [[NSMenu alloc] init];
      [statusMenu retain];
      menuDelegate = [[MenuBarDelegate alloc] initWithMenuSystem:g_menu];
      [menuDelegate retain];

      // Add all menu items
      for (int i = 0; i < g_menu->item_count; i++) {
          if (g_menu->items[i].is_separator) {
              [statusMenu addItem:[NSMenuItem separatorItem]];
          } else {
              NSMenuItem *item =
                  [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:g_menu->items[i].title]
                                             action:@selector(handleMenuItem:)
                                      keyEquivalent:@""];
              [item setTarget:menuDelegate];
              [item setTag:i];
              [statusMenu addItem:item];
          }
      }

      [statusItem setMenu:statusMenu];
      [statusItem setVisible:YES];

      // Force the button to redraw
      [statusItem.button setNeedsDisplay:YES];

      log_info("Menu bar setup complete - icon should be visible");
      log_info("Status item visible: %d", statusItem.visible);
      log_info("Status item button: %p", statusItem.button);
    };

    // If we're already on the main thread, execute directly
    if ([NSThread isMainThread]) {
        createMenuBlock();
    } else {
        // Otherwise dispatch to main thread
        dispatch_sync(dispatch_get_main_queue(), createMenuBlock);
    }

    return 0;
}

void menu_show_context_menu(void) {
    if (!g_menu_showing || !statusItem || !statusMenu) {
        return;
    }
    
    app_dispatch_main(^{
        // Get the status item button
        NSStatusBarButton *button = statusItem.button;
        if (button) {
            // Modern way to show context menu - simulate a click
            // First get the button's frame in screen coordinates
            NSRect buttonFrame = [button convertRect:button.bounds toView:nil];
            NSRect screenFrame = [button.window convertRectToScreen:buttonFrame];
            
            // Show the menu at the button's location
            [statusMenu popUpMenuPositioningItem:nil atLocation:NSMakePoint(NSMidX(screenFrame), NSMinY(screenFrame)) inView:nil];
        }
    });
}

void menu_hide(void) {
    if (!g_menu || !g_menu_showing) {
        return;
    }

    if (statusItem) {
        [[NSStatusBar systemStatusBar] removeStatusItem:statusItem];
        [statusItem release];
        statusItem = nil;
    }

    if (statusMenu) {
        [statusMenu release];
        statusMenu = nil;
    }

    if (statusIcon) {
        [statusIcon release];
        statusIcon = nil;
    }

    if (menuDelegate) {
        [menuDelegate release];
        menuDelegate = nil;
    }
    g_menu_showing = false;
}

void menu_update_item(int index, const char *new_title) {
    if (!g_menu || index < 0 || index >= g_menu->item_count || !new_title) {
        return;
    }

    // Update the stored title
    if (g_menu->items[index].title) {
        free((void *) g_menu->items[index].title);
    }
    g_menu->items[index].title = utils_strdup(new_title);

    // If menu is currently showing, update the actual menu item
    if (g_menu_showing && statusMenu) {
        app_dispatch_main(^{
          NSMenuItem *item = [statusMenu itemAtIndex:index];
          if (item && ![item isSeparatorItem]) {
              [item setTitle:[NSString stringWithUTF8String:new_title]];
          }
        });
    }
}

void menu_set_enabled(bool enabled) {
    if (!g_menu_showing || !statusMenu) {
        return;
    }
    
    app_dispatch_main(^{
        for (NSMenuItem *item in [statusMenu itemArray]) {
            if (![item isSeparatorItem]) {
                [item setEnabled:enabled];
            }
        }
    });
}

void menu_destroy(MenuSystem *menu) {
    if (!menu)
        return;

    // Hide if currently showing
    if (g_menu_showing) {
        menu_hide();
    }

    // Free all titles
    for (int i = 0; i < menu->item_count; i++) {
        if (menu->items[i].title) {
            free((void *) menu->items[i].title);
        }
    }

    free(menu->items);
    free(menu);
}