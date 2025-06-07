#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>
#include "../audio.h"
#include "../overlay.h"
#include "../menubar.h"

#include "../transcription.h"

// Forward declarations for keylogger functionality
extern int start_keylogger(void);
extern void stop_keylogger(void);

// Global state
static volatile bool running = true;
static AudioRecorder* recorder = NULL;
static bool whisper_initialized = false;

// Copy text to clipboard
void copy_to_clipboard(const char* text) {
    NSString* nsText = [NSString stringWithUTF8String:text];
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:nsText forType:NSPasteboardTypeString];
}

// Paste text by simulating Cmd+V
void paste_text() {
    // Create key events for Cmd+V
    CGEventRef cmdVDown = CGEventCreateKeyboardEvent(NULL, 9, true); // V key down
    CGEventRef cmdVUp = CGEventCreateKeyboardEvent(NULL, 9, false);   // V key up
    
    // Set Command modifier
    CGEventSetFlags(cmdVDown, kCGEventFlagMaskCommand);
    CGEventSetFlags(cmdVUp, kCGEventFlagMaskCommand);
    
    // Post the events
    CGEventPost(kCGHIDEventTap, cmdVDown);
    CGEventPost(kCGHIDEventTap, cmdVUp);
    
    // Clean up
    CFRelease(cmdVDown);
    CFRelease(cmdVUp);
}

// Handle FN key press - start recording
void fn_key_pressed() {
    NSLog(@"FN key pressed!");
    
    if (!recorder) {
        NSLog(@"Recorder is NULL!");
        return;
    }
    
    if (!audio_recorder_is_recording(recorder)) {
        if (audio_recorder_start_buffer(recorder) != 0) {
            NSLog(@"Failed to start recording");
        } else {
            NSLog(@"Recording started");
            // Show overlay only after recording successfully starts
            overlay_show("Recording");
        }
    }
}

// Handle FN key release - stop recording and transcribe
void fn_key_released() {
    NSLog(@"FN key released!");
    
    if (!recorder || !audio_recorder_is_recording(recorder)) {
        NSLog(@"Not recording or recorder is NULL");
        return;
    }
    
    if (audio_recorder_stop(recorder) != 0) {
        NSLog(@"Failed to stop recording");
        overlay_hide();
        return;
    }
    
    // Get the recorded audio data
    size_t data_size;
    const float* audio_data = audio_recorder_get_data(recorder, &data_size);
    double duration = audio_recorder_get_duration(recorder);
    
    NSLog(@"Recorded %.2f seconds (%zu samples)", duration, data_size);
    
    if (data_size == 0) {
        NSLog(@"No audio data recorded");
        overlay_hide();
        return;
    }
    
    if (!whisper_initialized) {
        NSLog(@"Whisper not initialized");
        overlay_hide();
        return;
    }
    
    // Show transcribing overlay
    overlay_show("Transcribing");
    
    // Transcribe the audio
    char result[1024];
    
    if (transcribe_audio(audio_data, (int)data_size, result, sizeof(result)) == 0) {
        if (strlen(result) > 0) {
            // Trim leading/trailing whitespace
            char* start = result;
            while (*start == ' ' || *start == '\t' || *start == '\n') start++;
            
            char* end = start + strlen(start) - 1;
            while (end > start && (*end == ' ' || *end == '\t' || *end == '\n')) end--;
            *(end + 1) = '\0';
            
            if (strlen(start) > 0) {
                // Check if the result is just non-speech tokens
                if (strcmp(start, "[BLANK_AUDIO]") == 0 || 
                    strcmp(start, "(blank audio)") == 0 ||
                    strcmp(start, "[BLANK AUDIO]") == 0) {
                    NSLog(@"No speech detected (blank audio)");
                    overlay_hide();
                } else {
                    NSLog(@"Transcription: \"%s\"", start);
                    
                    // Hide overlay before pasting
                    overlay_hide();
                    
                    // Copy to clipboard and paste
                    copy_to_clipboard(start);
                    
                    // Small delay then paste
                    usleep(100000); // 100ms
                    paste_text();
                }
            } else {
                overlay_hide();
            }
        } else {
            overlay_hide();
        }
    } else {
        NSLog(@"Transcription failed");
        overlay_hide();
    }
}

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = false;
        stop_keylogger();
        
        overlay_cleanup();
        menubar_cleanup();
        
        if (recorder) {
            audio_recorder_destroy(recorder);
            recorder = NULL;
        }
        
        if (whisper_initialized) {
            transcription_cleanup();
            whisper_initialized = false;
        }
        
        [NSApp terminate:nil];
    }
}

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSStatusItem *statusItem;
@end

@implementation AppDelegate

- (void)showAbout:(id)sender {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Yakety"];
    [alert setInformativeText:@"Voice transcription for macOS\n\nHold FN key to record and transcribe speech."];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

- (void)quitApp:(id)sender {
    // Send SIGTERM to trigger graceful shutdown
    kill(getpid(), SIGTERM);
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize UI systems
    overlay_init();
    
    // Create status bar item immediately like in the working test
    self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    
    // Load icon image
    NSString* iconPath = [[NSBundle mainBundle] pathForResource:@"menubar" ofType:@"png"];
    NSImage* icon = nil;
    
    if (iconPath) {
        icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
        [icon setTemplate:YES]; // Makes it adapt to dark/light mode
        self.statusItem.button.image = icon;
        NSLog(@"Loaded menubar icon from bundle");
    } else {
        // Fallback to emoji if icon not found
        self.statusItem.button.title = @"🎤";
        NSLog(@"Icon not found in bundle, using emoji fallback");
    }
    
    // Create menu
    NSMenu *menu = [[NSMenu alloc] init];
    [menu addItemWithTitle:@"About Yakety" action:@selector(showAbout:) keyEquivalent:@""];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItemWithTitle:@"Quit Yakety" action:@selector(quitApp:) keyEquivalent:@"q"];
    
    self.statusItem.menu = menu;
    
    NSLog(@"Status bar item created!");
    
    // Switch to accessory mode after menubar is created
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    });
    
    // Start initialization in background
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSLog(@"Starting background initialization...");
        
        // Request audio permissions
        if (audio_request_permissions() != 0) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert* alert = [[NSAlert alloc] init];
                [alert setMessageText:@"Microphone Permission Required"];
                [alert setInformativeText:@"Please grant microphone permission in System Preferences."];
                [alert addButtonWithTitle:@"Quit"];
                [alert runModal];
                [NSApp terminate:nil];
            });
            return;
        }
        
        NSLog(@"Audio permissions granted");
        
        // Create audio recorder with Whisper-compatible settings
        recorder = audio_recorder_create(&WHISPER_AUDIO_CONFIG);
        if (!recorder) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert* alert = [[NSAlert alloc] init];
                [alert setMessageText:@"Audio Initialization Failed"];
                [alert setInformativeText:@"Failed to initialize audio recorder."];
                [alert addButtonWithTitle:@"Quit"];
                [alert runModal];
                [NSApp terminate:nil];
            });
            return;
        }
        
        // Initialize Whisper
        // Try multiple locations for the model
        const char* env_model = getenv("YAKETY_MODEL_PATH");
        
        // For macOS app bundle, check bundle resources first
        NSString* bundleModelPath = [[NSBundle mainBundle] pathForResource:@"ggml-base.en" 
                                                                    ofType:@"bin" 
                                                               inDirectory:@"models"];
        const char* bundle_path = bundleModelPath ? [bundleModelPath UTF8String] : NULL;
        
        const char* static_paths[] = {
            // Bundle resource (if found)
            bundle_path,
            // App bundle Resources (relative path)
            "../Resources/models/ggml-base.en.bin",
            // Development path from build directory
            "../whisper.cpp/models/ggml-base.en.bin",
            // Development path from project root
            "whisper.cpp/models/ggml-base.en.bin",
            // Installed path
            "/Applications/Yakety.app/Contents/Resources/models/ggml-base.en.bin",
            NULL
        };
        
        // Build the full list with env var if set
        const char* model_paths[10];  // Max 10 paths
        int idx = 0;
        
        if (env_model != NULL) {
            model_paths[idx++] = env_model;
        }
        
        for (int i = 0; static_paths[i] != NULL && idx < 9; i++) {
            model_paths[idx++] = static_paths[i];
        }
        model_paths[idx] = NULL;
        
        bool model_loaded = false;
        for (int i = 0; model_paths[i] != NULL; i++) {
            if (model_paths[i]) {
                // Check if file exists before trying to load
                if (access(model_paths[i], F_OK) == 0) {
                    NSLog(@"🔍 Found model at: %s", model_paths[i]);
                    if (transcription_init(model_paths[i]) == 0) {
                        whisper_initialized = true;
                        model_loaded = true;
                        NSLog(@"✅ Model loaded from: %s", model_paths[i]);
                        break;
                    }
                }
            }
        }
        
        if (!model_loaded) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert* alert = [[NSAlert alloc] init];
                [alert setMessageText:@"Whisper Model Not Found"];
                [alert setInformativeText:@"Failed to load Whisper model. Make sure to run ./build.sh first."];
                [alert addButtonWithTitle:@"Quit"];
                [alert runModal];
                [NSApp terminate:nil];
            });
            return;
        }
        
        // Start the keylogger
        if (start_keylogger() != 0) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert* alert = [[NSAlert alloc] init];
                [alert setMessageText:@"Accessibility Permission Required"];
                [alert setInformativeText:@"Please grant accessibility permission in System Preferences → Security & Privacy → Privacy → Accessibility.\n\nAfter granting permission, please restart Yakety."];
                [alert addButtonWithTitle:@"Open System Preferences"];
                [alert addButtonWithTitle:@"Quit"];
                
                NSModalResponse response = [alert runModal];
                if (response == NSAlertFirstButtonReturn) {
                    // Open System Preferences to Accessibility
                    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"]];
                }
                [NSApp terminate:nil];
            });
            return;
        }
        
        // Show notification that we're ready (removed deprecated API)
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Yakety Ready - Hold FN key to record and transcribe speech.");
        });
    });
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return NO;  // Keep running in the background
}

@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Create the application
        NSApplication* app = [NSApplication sharedApplication];
        
        // Set activation policy - start as regular app to ensure UI works
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        // Create and set the app delegate
        AppDelegate* delegate = [[AppDelegate alloc] init];
        [app setDelegate:delegate];
        
        // Run the app
        [app run];
    }
    return 0;
}