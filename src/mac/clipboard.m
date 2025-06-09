#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include "../clipboard.h"
#include "../logging.h"

// Static variable to hold saved pasteboard items
static NSArray* savedPasteboardItems = nil;

// Save current clipboard contents
static void save_clipboard(void) {
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        
        // Create array to hold all items with all their types
        NSMutableArray* items = [NSMutableArray array];
        
        // Get all pasteboard items
        for (NSPasteboardItem* item in pasteboard.pasteboardItems) {
            NSMutableDictionary* itemData = [NSMutableDictionary dictionary];
            
            // Save all types for this item
            for (NSString* type in item.types) {
                NSData* data = [item dataForType:type];
                if (data) {
                    itemData[type] = data;
                }
            }
            
            if (itemData.count > 0) {
                [items addObject:itemData];
            }
        }
        
        // Retain the saved items
        savedPasteboardItems = [items copy];
        log_info("Saved %lu clipboard items", (unsigned long)savedPasteboardItems.count);
    }
}

// Restore saved clipboard contents
static void restore_clipboard(void) {
    @autoreleasepool {
        if (!savedPasteboardItems || savedPasteboardItems.count == 0) {
            return;
        }
        
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        
        // Restore all items
        for (NSDictionary* itemData in savedPasteboardItems) {
            NSPasteboardItem* item = [[NSPasteboardItem alloc] init];
            
            // Restore all types for this item
            for (NSString* type in itemData) {
                [item setData:itemData[type] forType:type];
            }
            
            [pasteboard writeObjects:@[item]];
        }
        
        log_info("Restored %lu clipboard items", (unsigned long)savedPasteboardItems.count);
        
        // Clear saved items
        savedPasteboardItems = nil;
    }
}

void clipboard_copy(const char* text) {
    @autoreleasepool {
        // Save current clipboard contents first
        save_clipboard();
        
        NSString* string = [NSString stringWithUTF8String:text];
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
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
        
        // Restore the original clipboard contents
        restore_clipboard();
    }
}