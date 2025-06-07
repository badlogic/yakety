#import <Cocoa/Cocoa.h>
#include "../menubar.h"
#include <signal.h>

static NSStatusItem* statusItem = nil;

@interface MenuBarDelegate : NSObject
- (void)quit:(id)sender;
- (void)about:(id)sender;
- (void)showLicenses:(id)sender;
@end

@implementation MenuBarDelegate

- (void)quit:(id)sender {
    // Send SIGTERM to trigger graceful shutdown
    kill(getpid(), SIGTERM);
}

- (void)about:(id)sender {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Yakety"];
    [alert setInformativeText:@"Voice transcription for macOS\n\nHold FN key to record and transcribe speech."];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

- (void)showLicenses:(id)sender {
    NSString* licenseText = @"Third-Party Licenses\n\n"
        @"miniaudio v0.11.22\n"
        @"Copyright 2025 David Reid\n"
        @"License: Public Domain (Unlicense) or MIT-0\n"
        @"https://github.com/mackron/miniaudio\n\n"
        @"whisper.cpp\n"
        @"Copyright 2023-2024 The ggml authors\n"
        @"License: MIT License\n"
        @"https://github.com/ggerganov/whisper.cpp\n\n"
        @"Whisper Model (base.en)\n"
        @"Copyright OpenAI\n"
        @"License: MIT License\n"
        @"https://github.com/openai/whisper\n\n"
        @"For full license texts, see LICENSES.md in the source repository.";
    
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Open Source Licenses"];
    [alert setInformativeText:licenseText];
    [alert addButtonWithTitle:@"OK"];
    [alert setAlertStyle:NSAlertStyleInformational];
    
    // Make the window wider to better display license info
    NSWindow* window = alert.window;
    NSRect frame = window.frame;
    frame.size.width = 600;
    [window setFrame:frame display:YES];
    
    [alert runModal];
}

@end

static MenuBarDelegate* menuDelegate = nil;

int menubar_init(void) {
    NSLog(@"menubar_init called");
    
    @autoreleasepool {
        NSLog(@"Creating status bar item...");
        
        NSStatusBar *statusBar = [NSStatusBar systemStatusBar];
        statusItem = [statusBar statusItemWithLength:NSVariableStatusItemLength];
        
        if (!statusItem) {
            NSLog(@"Failed to create status item!");
            return -1;
        }
        
        // Load icon image
        NSString* iconPath = [[NSBundle mainBundle] pathForResource:@"menubar" ofType:@"png"];
        NSImage* icon = nil;
        
        if (iconPath) {
            icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
            [icon setTemplate:YES]; // Makes it adapt to dark/light mode
            statusItem.button.image = icon;
            NSLog(@"Loaded menubar icon from bundle");
        } else {
            // Fallback to emoji if icon not found
            statusItem.button.title = @"🎤";
            NSLog(@"Icon not found in bundle, using emoji fallback");
        }
        
        // Create menu
        NSMenu* menu = [[NSMenu alloc] init];
            
        // Add menu items
        menuDelegate = [[MenuBarDelegate alloc] init];
        
        NSMenuItem* aboutItem = [[NSMenuItem alloc] initWithTitle:@"About Yakety" 
                                                          action:@selector(about:) 
                                                   keyEquivalent:@""];
        [aboutItem setTarget:menuDelegate];
        [menu addItem:aboutItem];
        
        NSMenuItem* licensesItem = [[NSMenuItem alloc] initWithTitle:@"Open Source Licenses" 
                                                             action:@selector(showLicenses:) 
                                                      keyEquivalent:@""];
        [licensesItem setTarget:menuDelegate];
        [menu addItem:licensesItem];
        
        [menu addItem:[NSMenuItem separatorItem]];
        
        NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit Yakety" 
                                                         action:@selector(quit:) 
                                                  keyEquivalent:@"q"];
        [quitItem setTarget:menuDelegate];
        [menu addItem:quitItem];
            
        statusItem.menu = menu;
        
        NSLog(@"Menu bar initialization complete");
        
        // Make the status item visible
        statusItem.visible = YES;
    }
    
    return 0;
}

void menubar_cleanup(void) {
    if (statusItem) {
        [[NSStatusBar systemStatusBar] removeStatusItem:statusItem];
        statusItem = nil;
        menuDelegate = nil;
    }
}