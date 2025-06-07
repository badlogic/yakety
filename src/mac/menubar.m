#import <Cocoa/Cocoa.h>
#include "../menubar.h"
#include <signal.h>

static NSStatusItem* statusItem = nil;

@interface MenuBarDelegate : NSObject
- (void)quit:(id)sender;
- (void)about:(id)sender;
@end

@implementation MenuBarDelegate

- (void)quit:(id)sender {
    // Send SIGTERM to trigger graceful shutdown
    kill(getpid(), SIGTERM);
}

- (void)about:(id)sender {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Whisperer"];
    [alert setInformativeText:@"Voice transcription for macOS\n\nHold FN key to record and transcribe speech."];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

@end

static MenuBarDelegate* menuDelegate = nil;

void menubar_init(void) {
    NSLog(@"menubar_init called");
    
    @autoreleasepool {
        NSLog(@"Creating status bar item...");
        
        NSStatusBar *statusBar = [NSStatusBar systemStatusBar];
        statusItem = [statusBar statusItemWithLength:NSVariableStatusItemLength];
        
        if (!statusItem) {
            NSLog(@"Failed to create status item!");
            return;
        }
        
        // Set text-based icon for reliability
        statusItem.button.title = @"🎤";
        NSLog(@"Set microphone emoji as icon");
        
        // Create menu
        NSMenu* menu = [[NSMenu alloc] init];
            
        // Add menu items
        menuDelegate = [[MenuBarDelegate alloc] init];
        
        NSMenuItem* aboutItem = [[NSMenuItem alloc] initWithTitle:@"About Whisperer" 
                                                          action:@selector(about:) 
                                                   keyEquivalent:@""];
        [aboutItem setTarget:menuDelegate];
        [menu addItem:aboutItem];
        
        [menu addItem:[NSMenuItem separatorItem]];
        
        NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit Whisperer" 
                                                         action:@selector(quit:) 
                                                  keyEquivalent:@"q"];
        [quitItem setTarget:menuDelegate];
        [menu addItem:quitItem];
            
        statusItem.menu = menu;
        
        NSLog(@"Menu bar initialization complete");
        
        // Make the status item visible
        statusItem.visible = YES;
    }
}

void menubar_cleanup(void) {
    if (statusItem) {
        [[NSStatusBar systemStatusBar] removeStatusItem:statusItem];
        statusItem = nil;
        menuDelegate = nil;
    }
}