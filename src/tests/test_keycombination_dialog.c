#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../app.h"
#include "../dialog.h"
#include "../keylogger.h"

// Dummy callbacks for keylogger
void dummy_press(void *userdata) { (void)userdata; }
void dummy_release(void *userdata) { (void)userdata; }

void test_ready(void) {
    printf("About to show dialog...\n");
    
    // Initialize keylogger singleton with dummy callbacks
    if (keylogger_init(dummy_press, dummy_release, NULL, NULL) != 0) {
        printf("Failed to initialize keylogger\n");
        app_cleanup();
        exit(1);
    }
    
    KeyCombination result = {0};
    
    bool success = dialog_keycombination_capture(
        "Test Key Combination Dialog",
        "Press a key combination to test the dialog functionality",
        &result
    );
    
    printf("Dialog function returned: %s\n", success ? "true" : "false");
    
    if (success) {
        printf("Dialog completed successfully!\n");
        printf("Captured key combination:\n");
        for (int i = 0; i < MAX_KEYS_IN_COMBINATION && result.keys[i].code != 0; i++) {
            printf("  Key %d: code=%u, flags=0x%x\n", i + 1, result.keys[i].code, result.keys[i].flags);
        }
        printf("Key count: %d\n", result.count);
    } else {
        printf("Dialog was cancelled.\n");
    }
    
    // Clean up keylogger
    keylogger_cleanup();
    app_cleanup();
    exit(0);
}

int main() {
    printf("Testing SwiftUI Key Combination Dialog...\n");
    
    // Initialize the platform app (handles Cocoa setup on macOS)
    if (app_init("Hotkey Dialog Test", "1.0", false, test_ready) != 0) {
        printf("Failed to initialize app\n");
        return 1;
    }
    
    // Start the app run loop - this will call test_ready when ready
    app_run();
    return 0;
}