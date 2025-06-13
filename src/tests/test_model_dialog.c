#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../app.h"
#include "../dialog.h"

void test_ready(void) {
    char selected_model[256];
    char selected_language[64];
    char download_url[512];
    
    printf("Opening models dialog...\n");
    
    if (dialog_models_and_language("Models & Language Settings", selected_model, sizeof(selected_model), 
                                   selected_language, sizeof(selected_language),
                                   download_url, sizeof(download_url))) {
        printf("User selected model: %s\n", selected_model);
        printf("User selected language: %s\n", selected_language);
        printf("Download URL: %s\n", download_url);
    } else {
        printf("User cancelled or no model/language selected\n");
    }
    
    app_cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    // Initialize the platform app (handles Cocoa setup on macOS)
    if (app_init("Model Dialog Test", "1.0", false, test_ready) != 0) {
        printf("Failed to initialize app\n");
        return 1;
    }
    
    // Start the app run loop - this will call test_ready when ready
    app_run();
    return 0;
}