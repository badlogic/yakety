#import <Cocoa/Cocoa.h>
#include "../dialog.h"

void dialog_error(const char* title, const char* message) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title]];
        [alert setInformativeText:[NSString stringWithUTF8String:message]];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert runModal];
    }
}

void dialog_info(const char* title, const char* message) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title]];
        [alert setInformativeText:[NSString stringWithUTF8String:message]];
        [alert setAlertStyle:NSAlertStyleInformational];
        [alert runModal];
    }
}

bool dialog_confirm(const char* title, const char* message) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title]];
        [alert setInformativeText:[NSString stringWithUTF8String:message]];
        [alert addButtonWithTitle:@"Yes"];
        [alert addButtonWithTitle:@"No"];
        return [alert runModal] == NSAlertFirstButtonReturn;
    }
}

int dialog_accessibility_permission(void) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Accessibility Permission Required"];
        [alert setInformativeText:@"Yakety needs accessibility permission to monitor the FN key.\n\n1. Click 'Open System Preferences' to grant permission\n2. Then click 'I've Granted Permission' to continue\n3. Or click 'Quit' to exit"];
        [alert addButtonWithTitle:@"I've Granted Permission"];
        [alert addButtonWithTitle:@"Open System Preferences"];
        [alert addButtonWithTitle:@"Quit"];
        
        NSModalResponse response = [alert runModal];
        
        if (response == NSAlertFirstButtonReturn) {
            return 0; // Granted permission
        } else if (response == NSAlertSecondButtonReturn) {
            return 1; // Open preferences
        } else {
            return 2; // Quit
        }
    }
}

bool dialog_wait_for_permission(void) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Waiting for Permission"];
        [alert setInformativeText:@"Please grant accessibility permission in System Preferences, then click Continue."];
        [alert addButtonWithTitle:@"Continue"];
        [alert addButtonWithTitle:@"Quit"];
        
        return [alert runModal] == NSAlertFirstButtonReturn;
    }
}