#include "../overlay.h"
#include "../logging.h"
#include "dispatch.h"
#import <Cocoa/Cocoa.h>

static NSWindow *overlayWindow = nil;
static NSTextField *messageLabel = nil;
static NSImageView *iconView = nil;
static CALayer *borderLayer = nil;
static NSColor *g_currentTintColor = nil;

void overlay_init(void) {
    app_dispatch_main(^{
      @autoreleasepool {
          log_info("Overlay window creation starting");
          // Create a more compact window with icon + text
          NSRect frame = NSMakeRect(0, 0, 140, 36);
          overlayWindow = [[NSWindow alloc] initWithContentRect:frame
                                                      styleMask:NSWindowStyleMaskBorderless
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO];

          // Configure window properties
          [overlayWindow setOpaque:NO];
          [overlayWindow setBackgroundColor:[NSColor clearColor]];
          [overlayWindow setLevel:NSFloatingWindowLevel];
          [overlayWindow setIgnoresMouseEvents:YES];
          [overlayWindow
              setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary];

          // Create the background view with rounded corners and green border
          NSView *contentView = [[NSView alloc] initWithFrame:frame];
          contentView.wantsLayer = YES;

          CALayer *layer = contentView.layer;
          layer.backgroundColor = [[NSColor colorWithWhite:0.0 alpha:0.85] CGColor];
          layer.cornerRadius = 10;

          // Store reference to layer for border color changes
          borderLayer = layer;

          // Default to Yakety green border
          g_currentTintColor = [NSColor colorWithRed:0x22 / 255.0 green:0xC5 / 255.0 blue:0x5E / 255.0 alpha:1.0];
          layer.borderColor = [g_currentTintColor CGColor];
          layer.borderWidth = 1.0;

          [overlayWindow setContentView:contentView];

          // Load app icon - try multiple approaches
          NSImage *appIcon = nil;

          // First try bundle resources (for .app)
          NSString *iconPath = [[NSBundle mainBundle] pathForResource:@"menubar" ofType:@"png"];
          if (iconPath) {
              appIcon = [[NSImage alloc] initWithContentsOfFile:iconPath];
              [appIcon setTemplate:YES]; // Make it adapt to the dark background
              log_info("Loaded menubar icon from bundle: %s", [iconPath UTF8String]);
          }

          // If not in bundle, try next to executable (for CLI)
          if (!appIcon) {
              NSString *executablePath = [[NSBundle mainBundle] executablePath];
              NSString *executableDir = [executablePath stringByDeletingLastPathComponent];
              NSString *cliIconPath = [executableDir stringByAppendingPathComponent:@"menubar.png"];

              if ([[NSFileManager defaultManager] fileExistsAtPath:cliIconPath]) {
                  appIcon = [[NSImage alloc] initWithContentsOfFile:cliIconPath];
                  [appIcon setTemplate:YES]; // Make it adapt to the dark background
                  log_info("Loaded menubar icon from CLI directory: %s", [cliIconPath UTF8String]);
              }
          }

          // If still no icon, try system microphone icon as fallback
          if (!appIcon) {
              // Try system microphone icon which is appropriate for voice recording
              appIcon = [NSImage imageWithSystemSymbolName:@"mic.fill" accessibilityDescription:@"Recording"];
              if (appIcon) {
                  // Configure the SF Symbol
                  NSImageSymbolConfiguration *config =
                      [NSImageSymbolConfiguration configurationWithPointSize:16 weight:NSFontWeightRegular];
                  appIcon = [appIcon imageWithSymbolConfiguration:config];
                  log_info("Using system microphone icon");
              }
          }

          // For CLI or if no icon available, we'll just skip it
          if (!appIcon) {
              log_info("No icon available for overlay");
          }

          // Calculate vertical center for both icon and text
          CGFloat contentHeight = 20; // Height of icon/text
          CGFloat verticalMargin = (frame.size.height - contentHeight) / 2;

          // Create icon view (small icon on the left) - only if we have an icon
          if (appIcon) {
              iconView = [[NSImageView alloc] initWithFrame:NSMakeRect(8, verticalMargin, 20, 20)];
              [iconView setImage:appIcon];
              [iconView setImageScaling:NSImageScaleProportionallyDown];

              // Make icon Yakety green instead of white
              if ([appIcon isTemplate]) {
                  [iconView setContentTintColor:[NSColor colorWithRed:0x22 / 255.0
                                                                green:0xC5 / 255.0
                                                                 blue:0x5E / 255.0
                                                                alpha:1.0]];
              }
              [contentView addSubview:iconView];
          }

          // Create the message label - adjust for baseline offset
          CGFloat labelX = appIcon ? 32 : 8;
          CGFloat labelWidth = appIcon ? 100 : 124;

          // Adjust Y position to compensate for baseline alignment
          // Text baseline sits about 1/3 up from bottom of the text bounds
          CGFloat labelHeight = 20;
          CGFloat labelY = (frame.size.height - labelHeight) / 2 - 2; // Move up to optically center

          messageLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(labelX, labelY, labelWidth, labelHeight)];
          [messageLabel setEditable:NO];
          [messageLabel setBordered:NO];
          [messageLabel setDrawsBackground:NO];
          [messageLabel setAlignment:NSTextAlignmentCenter];
          [messageLabel setTextColor:[NSColor whiteColor]];
          [messageLabel setFont:[NSFont systemFontOfSize:12 weight:NSFontWeightMedium]];

          // Configure the cell for better vertical alignment
          NSTextFieldCell *cell = [messageLabel cell];
          [cell setLineBreakMode:NSLineBreakByClipping];
          [cell setTruncatesLastVisibleLine:YES];

          [contentView addSubview:messageLabel];

          // Position window at center-bottom of main screen
          NSScreen *mainScreen = [NSScreen mainScreen];
          NSRect screenFrame = mainScreen.frame;
          NSPoint position = NSMakePoint((screenFrame.size.width - frame.size.width) / 2,
                                         30 // 30 pixels from bottom
          );
          [overlayWindow setFrameOrigin:position];
          log_info("Overlay window creation completed");
      }
    });
}

static void overlay_show_with_color(const char *message, NSColor *tintColor) {
    if (!message) {
        log_error("overlay_show_with_color called with NULL message\n");
        return;
    }

    // Create NSString outside the block to ensure it's retained
    NSString *messageString = [NSString stringWithUTF8String:message];
    if (!messageString) {
        log_error("Failed to create NSString from message: '%s'\n", message);
        return;
    }

    app_dispatch_main(^{
      @autoreleasepool {
          log_info("Overlay show requested for: %s", message);
          if (overlayWindow && messageLabel) {
              log_info("Overlay window exists, showing");

              // Update colors if different
              if (![tintColor isEqual:g_currentTintColor]) {
                  g_currentTintColor = tintColor;

                  // Update border color
                  if (borderLayer) {
                      borderLayer.borderColor = [tintColor CGColor];
                  }

                  // Update icon tint
                  if (iconView && [[iconView image] isTemplate]) {
                      [iconView setContentTintColor:tintColor];
                  }
              }

              // Create attributed string with paragraph style for vertical centering
              NSMutableParagraphStyle *paragraphStyle = [[NSMutableParagraphStyle alloc] init];
              [paragraphStyle setAlignment:NSTextAlignmentCenter];

              NSDictionary *attributes = @{
                  NSFontAttributeName : [NSFont systemFontOfSize:12 weight:NSFontWeightMedium],
                  NSForegroundColorAttributeName : [NSColor whiteColor],
                  NSParagraphStyleAttributeName : paragraphStyle
              };

              NSAttributedString *attributedText = [[NSAttributedString alloc] initWithString:messageString
                                                                                   attributes:attributes];

              // Calculate required size for text
              NSSize textSize = [attributedText size];
              CGFloat textWidth = ceil(textSize.width);

              // Calculate window width (icon + spacing + text + margins)
              CGFloat iconSpace = iconView ? 32 : 0; // Icon width + spacing
              CGFloat minWidth = 140;
              CGFloat maxWidth = 400;                                                           // Cap at reasonable max
              CGFloat windowWidth = fmax(minWidth, fmin(maxWidth, iconSpace + textWidth + 20)); // 20px for margins
              CGFloat windowHeight = 36;

              // Update window frame
              NSRect newFrame = NSMakeRect(0, 0, windowWidth, windowHeight);
              [overlayWindow setFrame:newFrame display:NO];

              // Update content view frame
              NSView *contentView = [overlayWindow contentView];
              [contentView setFrame:newFrame];

              // Update label frame
              CGFloat labelX = iconView ? 32 : 10;
              CGFloat labelWidth = windowWidth - labelX - 10;
              CGFloat verticalMargin = (windowHeight - 20) / 2 - 2; // Same as in init
              [messageLabel setFrame:NSMakeRect(labelX, verticalMargin, labelWidth, 20)];
              [messageLabel setAttributedStringValue:attributedText];

              // Re-center window on screen
              NSScreen *mainScreen = [NSScreen mainScreen];
              NSRect screenFrame = mainScreen.frame;
              NSPoint position = NSMakePoint((screenFrame.size.width - windowWidth) / 2,
                                             30 // 30 pixels from bottom
              );
              [overlayWindow setFrameOrigin:position];

              // Fade in animation
              [overlayWindow setAlphaValue:0.0];
              [overlayWindow orderFront:nil];

              [NSAnimationContext
                  runAnimationGroup:^(NSAnimationContext *context) {
                    [context setDuration:0.15]; // Quick fade in
                    [[overlayWindow animator] setAlphaValue:1.0];
                  }
                  completionHandler:nil];
          } else {
              log_info("Overlay window not ready yet (overlayWindow=%p, messageLabel=%p)", overlayWindow, messageLabel);
          }
      }
    });
}

void overlay_show(const char *message) {
    NSColor *yakety_green = [NSColor colorWithRed:0x22 / 255.0 green:0xC5 / 255.0 blue:0x5E / 255.0 alpha:1.0];
    overlay_show_with_color(message, yakety_green);
}

void overlay_show_error(const char *message) {
    NSColor *error_red = [NSColor colorWithRed:0xEF / 255.0 green:0x44 / 255.0 blue:0x44 / 255.0 alpha:1.0];
    overlay_show_with_color(message, error_red);
}

void overlay_hide(void) {
    app_dispatch_main(^{
      @autoreleasepool {
          if (overlayWindow) {
              // Fade out animation
              [NSAnimationContext
                  runAnimationGroup:^(NSAnimationContext *context) {
                    [context setDuration:0.15]; // Quick fade out
                    [[overlayWindow animator] setAlphaValue:0.0];
                  }
                  completionHandler:^{
                    [overlayWindow orderOut:nil];
                    [overlayWindow setAlphaValue:1.0]; // Reset for next show
                  }];
          }
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