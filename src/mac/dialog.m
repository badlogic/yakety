#include "../dialog.h"
#import <Cocoa/Cocoa.h>

void dialog_error(const char *title, const char *message) {
    @autoreleasepool {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title]];
        [alert setInformativeText:[NSString stringWithUTF8String:message]];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert runModal];
    }
}

void dialog_info(const char *title, const char *message) {
    @autoreleasepool {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title]];
        [alert setInformativeText:[NSString stringWithUTF8String:message]];
        [alert setAlertStyle:NSAlertStyleInformational];
        [alert runModal];
    }
}

bool dialog_confirm(const char *title, const char *message) {
    @autoreleasepool {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title]];
        [alert setInformativeText:[NSString stringWithUTF8String:message]];
        [alert addButtonWithTitle:@"Yes"];
        [alert addButtonWithTitle:@"No"];
        return [alert runModal] == NSAlertFirstButtonReturn;
    }
}

int dialog_accessibility_permission(void) {
    @autoreleasepool {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Accessibility Permission Required"];
        [alert setInformativeText:@"Yakety needs accessibility permission to monitor the FN key.\n\n1. Click 'Open "
                                  @"System Preferences' to grant permission\n2. Then click 'I've Granted Permission' "
                                  @"to continue\n3. Or click 'Quit' to exit"];
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
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Waiting for Permission"];
        [alert setInformativeText:@"Please grant accessibility permission in System Preferences, then click Continue."];
        [alert addButtonWithTitle:@"Continue"];
        [alert addButtonWithTitle:@"Quit"];

        return [alert runModal] == NSAlertFirstButtonReturn;
    }
}

// Custom view for capturing key combinations
@interface HotkeyView : NSView
@property(nonatomic, strong) NSString *capturedKeys;
@property(nonatomic, assign) BOOL isCapturing;
@property(nonatomic, assign) KeyCombination capturedCombination;
@property(nonatomic, assign) BOOL hasValidCombination;
@property(nonatomic, assign) NSEventModifierFlags lastFlags;
@end

@implementation HotkeyView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        self.capturedKeys = @"Press keys...";
        self.isCapturing = NO;
        self.capturedCombination = (KeyCombination){{{0}}, 0};
        self.hasValidCombination = NO;
        self.lastFlags = 0;
    }
    return self;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)canBecomeKeyView {
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    // Draw border
    [[NSColor controlColor] setFill];
    NSRectFill(self.bounds);
    [[NSColor systemBlueColor] setStroke];
    [NSBezierPath strokeRect:self.bounds];

    // Draw text
    NSMutableDictionary *attrs = [NSMutableDictionary dictionary];
    attrs[NSForegroundColorAttributeName] = self.isCapturing ? [NSColor systemBlueColor] : [NSColor labelColor];
    attrs[NSFontAttributeName] = [NSFont systemFontOfSize:13];

    NSSize textSize = [self.capturedKeys sizeWithAttributes:attrs];
    NSRect textRect = NSMakeRect((self.bounds.size.width - textSize.width) / 2,
                                 (self.bounds.size.height - textSize.height) / 2, textSize.width, textSize.height);

    [self.capturedKeys drawInRect:textRect withAttributes:attrs];
}

- (void)mouseDown:(NSEvent *)event {
    [self.window makeFirstResponder:self];
    self.isCapturing = YES;
    self.capturedKeys = @"Press keys...";
    self.hasValidCombination = NO;
    self.capturedCombination = (KeyCombination){{{0}}, 0};
    self.lastFlags = 0;

    // Pause the keylogger while capturing
    keylogger_pause();
    [self setNeedsDisplay:YES];
}

// Helper method to get display name for a key
- (NSString *)getKeyDisplayName:(unsigned short)keyCode {
    switch (keyCode) {
    // Function keys
    case 122:
        return @"F1";
    case 120:
        return @"F2";
    case 99:
        return @"F3";
    case 118:
        return @"F4";
    case 96:
        return @"F5";
    case 97:
        return @"F6";
    case 98:
        return @"F7";
    case 100:
        return @"F8";
    case 101:
        return @"F9";
    case 109:
        return @"F10";
    case 111:
        return @"F12";
    // Arrow keys
    case 126:
        return @"↑";
    case 125:
        return @"↓";
    case 123:
        return @"←";
    case 124:
        return @"→";
    // Special keys
    case 51:
        return @"⌫"; // Delete
    case 36:
        return @"↩"; // Return
    case 48:
        return @"⇥"; // Tab
    case 53:
        return @"⎋"; // Escape
    case 49:
        return @"Space";
    // Numbers
    case 29:
        return @"0";
    case 18:
        return @"1";
    case 19:
        return @"2";
    case 20:
        return @"3";
    case 21:
        return @"4";
    case 23:
        return @"5";
    case 22:
        return @"6";
    case 26:
        return @"7";
    case 28:
        return @"8";
    case 25:
        return @"9";
    // Letters (A-Z)
    case 0:
        return @"A";
    case 11:
        return @"B";
    case 8:
        return @"C";
    case 2:
        return @"D";
    case 14:
        return @"E";
    case 3:
        return @"F";
    case 5:
        return @"G";
    case 4:
        return @"H";
    case 34:
        return @"I";
    case 38:
        return @"J";
    case 40:
        return @"K";
    case 37:
        return @"L";
    case 46:
        return @"M";
    case 45:
        return @"N";
    case 31:
        return @"O";
    case 35:
        return @"P";
    case 12:
        return @"Q";
    case 15:
        return @"R";
    case 1:
        return @"S";
    case 17:
        return @"T";
    case 32:
        return @"U";
    case 9:
        return @"V";
    case 13:
        return @"W";
    case 7:
        return @"X";
    case 16:
        return @"Y";
    case 6:
        return @"Z";
    // Punctuation and symbols
    case 27:
        return @"-";
    case 24:
        return @"=";
    case 33:
        return @"[";
    case 30:
        return @"]";
    case 42:
        return @"\\";
    case 41:
        return @";";
    case 39:
        return @"'";
    case 50:
        return @"`";
    case 43:
        return @",";
    case 47:
        return @".";
    case 44:
        return @"/";
    // Additional keys that might include ^
    case 10:
        return @"§"; // Section sign (varies by layout)
    default:
        return [NSString stringWithFormat:@"Key%d", keyCode];
    }
}

- (BOOL)isSpecialKey:(unsigned short)keyCode {
    // Function keys F1-F12
    if (keyCode == 122 || keyCode == 120 || keyCode == 99 || keyCode == 118 || keyCode == 96 || keyCode == 97 ||
        keyCode == 98 || keyCode == 100 || keyCode == 101 || keyCode == 109 || keyCode == 111) {
        return YES;
    }

    // Arrow keys
    if (keyCode == 126 || keyCode == 125 || keyCode == 123 || keyCode == 124) {
        return YES;
    }

    return NO;
}

// Convert NSEvent modifier flags to CGEvent flags
- (uint32_t)convertToCGEventFlags:(NSEventModifierFlags)nsFlags {
    uint32_t cgFlags = 0;

    if (nsFlags & NSEventModifierFlagControl) {
        cgFlags |= kCGEventFlagMaskControl;
    }
    if (nsFlags & NSEventModifierFlagOption) {
        cgFlags |= kCGEventFlagMaskAlternate;
    }
    if (nsFlags & NSEventModifierFlagShift) {
        cgFlags |= kCGEventFlagMaskShift;
    }
    if (nsFlags & NSEventModifierFlagCommand) {
        cgFlags |= kCGEventFlagMaskCommand;
    }
    if (nsFlags & NSEventModifierFlagFunction) {
        cgFlags |= kCGEventFlagMaskSecondaryFn;
    }
    if (nsFlags & NSEventModifierFlagCapsLock) {
        cgFlags |= kCGEventFlagMaskAlphaShift;
    }

    return cgFlags;
}

- (void)keyDown:(NSEvent *)event {
    if (!self.isCapturing)
        return;

    // Capture immediately - any key press completes the capture
    unsigned short keyCode = [event keyCode];
    NSEventModifierFlags flags = [event modifierFlags];

    // For special keys (F1-F12, arrows), don't include the Function modifier flag
    // since it's automatically set by the system
    NSEventModifierFlags displayFlags = flags;
    if ([self isSpecialKey:keyCode]) {
        displayFlags &= ~NSEventModifierFlagFunction;
    }

    // Store the raw combination with converted CGEvent flags for keylogger compatibility
    KeyCombination combo = {{{0}}};
    combo.keys[0].code = keyCode;
    combo.keys[0].flags = [self convertToCGEventFlags:flags];
    combo.count = 1;
    self.capturedCombination = combo;
    self.hasValidCombination = YES;

    // Create display string using filtered flags
    NSMutableArray *displayKeys = [NSMutableArray array];

    // Add modifier symbols (using filtered flags for display)
    if (displayFlags & NSEventModifierFlagControl)
        [displayKeys addObject:@"⌃"];
    if (displayFlags & NSEventModifierFlagOption)
        [displayKeys addObject:@"⌥"];
    if (displayFlags & NSEventModifierFlagShift)
        [displayKeys addObject:@"⇧"];
    if (displayFlags & NSEventModifierFlagCommand)
        [displayKeys addObject:@"⌘"];
    if (displayFlags & NSEventModifierFlagFunction)
        [displayKeys addObject:@"fn"];
    if (displayFlags & NSEventModifierFlagCapsLock)
        [displayKeys addObject:@"⇪"];

    // Add the main key
    [displayKeys addObject:[self getKeyDisplayName:keyCode]];

    self.capturedKeys = [displayKeys componentsJoinedByString:@""];
    self.isCapturing = NO;

    // Resume keylogger
    keylogger_resume();
    [self setNeedsDisplay:YES];
}

- (void)flagsChanged:(NSEvent *)event {
    if (!self.isCapturing)
        return;

    NSEventModifierFlags flags = [event modifierFlags];

    // Check if modifiers were pressed (not released)
    NSEventModifierFlags pressed = flags & ~self.lastFlags;
    NSEventModifierFlags released = self.lastFlags & ~flags;

    if (pressed != 0) {
        // Something was pressed - show it immediately
        NSMutableArray *displayKeys = [NSMutableArray array];

        if (flags & NSEventModifierFlagControl)
            [displayKeys addObject:@"⌃"];
        if (flags & NSEventModifierFlagOption)
            [displayKeys addObject:@"⌥"];
        if (flags & NSEventModifierFlagShift)
            [displayKeys addObject:@"⇧"];
        if (flags & NSEventModifierFlagCommand)
            [displayKeys addObject:@"⌘"];
        if (flags & NSEventModifierFlagFunction)
            [displayKeys addObject:@"fn"];
        if (flags & NSEventModifierFlagCapsLock)
            [displayKeys addObject:@"⇪"];

        self.capturedKeys = [displayKeys componentsJoinedByString:@""];
        [self setNeedsDisplay:YES];
    }

    if (released != 0 && ![self.capturedKeys isEqualToString:@"Press keys..."]) {
        // Modifier was released and we have something captured - complete the capture
        KeyCombination combo = {{{0}}};
        combo.keys[0].code = 0;                                            // No specific key, modifier only
        combo.keys[0].flags = [self convertToCGEventFlags:self.lastFlags]; // Use the flags before release
        combo.count = 1;
        self.capturedCombination = combo;
        self.hasValidCombination = YES;
        self.isCapturing = NO;

        // Resume keylogger
        keylogger_resume();
    }

    self.lastFlags = flags;
}

@end

bool dialog_keycombination_capture(const char *title, const char *message, KeyCombination *result) {
    @autoreleasepool {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title]];
        [alert setInformativeText:[NSString stringWithUTF8String:message]];
        [alert addButtonWithTitle:@"OK"];
        [alert addButtonWithTitle:@"Cancel"];

        // Create the hotkey capture view
        HotkeyView *hotkeyView = [[HotkeyView alloc] initWithFrame:NSMakeRect(0, 0, 200, 30)];
        [alert setAccessoryView:hotkeyView];

        NSModalResponse response = [alert runModal];

        // Make sure keylogger is resumed in case dialog was cancelled during capture
        keylogger_resume();

        if (response == NSAlertFirstButtonReturn && hotkeyView.hasValidCombination) {
            *result = hotkeyView.capturedCombination;
            return true;
        }

        return false;
    }
}