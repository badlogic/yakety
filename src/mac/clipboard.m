#include "../clipboard.h"
#include "../logging.h"
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

void clipboard_copy(const char *text) {
    @autoreleasepool {
        NSString *string = [NSString stringWithUTF8String:text];
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        [pasteboard setString:string forType:NSPasteboardTypeString];
    }
}

void clipboard_paste(void) {
    @autoreleasepool {
        CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

        CGEventRef cmdDown = CGEventCreateKeyboardEvent(source, kVK_Command, true);
        CGEventRef vDown = CGEventCreateKeyboardEvent(source, kVK_ANSI_V, true);
        CGEventRef vUp = CGEventCreateKeyboardEvent(source, kVK_ANSI_V, false);
        CGEventRef cmdUp = CGEventCreateKeyboardEvent(source, kVK_Command, false);

        CGEventSetFlags(vDown, kCGEventFlagMaskCommand);
        CGEventSetFlags(vUp, kCGEventFlagMaskCommand);

        CGEventPost(kCGHIDEventTap, cmdDown);
        CGEventPost(kCGHIDEventTap, vDown);
        CGEventPost(kCGHIDEventTap, vUp);
        CGEventPost(kCGHIDEventTap, cmdUp);

        CFRelease(cmdDown);
        CFRelease(vDown);
        CFRelease(vUp);
        CFRelease(cmdUp);
        CFRelease(source);

        // Give a small delay for the paste to complete
        [NSThread sleepForTimeInterval:0.1];
    }
}