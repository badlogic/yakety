#import <Cocoa/Cocoa.h>
#include "../overlay.h"

static NSWindow* overlayWindow = nil;
static NSTextField* messageLabel = nil;

void overlay_init(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            // Create a smaller, more subtle window
            NSRect frame = NSMakeRect(0, 0, 180, 40);
            overlayWindow = [[NSWindow alloc] initWithContentRect:frame
                                            styleMask:NSWindowStyleMaskBorderless
                                            backing:NSBackingStoreBuffered
                                            defer:NO];
            
            // Configure window properties
            [overlayWindow setOpaque:NO];
            [overlayWindow setBackgroundColor:[NSColor clearColor]];
            [overlayWindow setLevel:NSFloatingWindowLevel];
            [overlayWindow setIgnoresMouseEvents:YES];
            [overlayWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | 
                                               NSWindowCollectionBehaviorStationary];
            
            // Create the background view with rounded corners
            NSView* contentView = [[NSView alloc] initWithFrame:frame];
            contentView.wantsLayer = YES;
            contentView.layer.backgroundColor = [[NSColor colorWithWhite:0.0 alpha:0.75] CGColor];
            contentView.layer.cornerRadius = 12;
            [overlayWindow setContentView:contentView];
            
            // Create the message label - centered vertically
            messageLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 10, 160, 20)];
            [messageLabel setEditable:NO];
            [messageLabel setBordered:NO];
            [messageLabel setDrawsBackground:NO];
            [messageLabel setAlignment:NSTextAlignmentCenter];
            [messageLabel setTextColor:[NSColor whiteColor]];
            [messageLabel setFont:[NSFont systemFontOfSize:13 weight:NSFontWeightRegular]];
            [contentView addSubview:messageLabel];
            
            // Position window at center-bottom of main screen
            NSScreen* mainScreen = [NSScreen mainScreen];
            NSRect screenFrame = mainScreen.frame;
            NSPoint position = NSMakePoint(
                (screenFrame.size.width - frame.size.width) / 2,
                30  // 30 pixels from bottom
            );
            [overlayWindow setFrameOrigin:position];
        }
    });
}

void overlay_show(const char* message) {
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            if (overlayWindow && messageLabel) {
                [messageLabel setStringValue:[NSString stringWithUTF8String:message]];
                [overlayWindow orderFront:nil];
            }
        }
    });
}

void overlay_hide(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            [overlayWindow orderOut:nil];
        }
    });
}

void overlay_cleanup(void) {
    if (overlayWindow) {
        [overlayWindow close];
        overlayWindow = nil;
        messageLabel = nil;
    }
}