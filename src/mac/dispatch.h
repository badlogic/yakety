#ifndef MAC_DISPATCH_H
#define MAC_DISPATCH_H

// Cross-platform main thread dispatch for macOS
// Automatically chooses the right scheduling method based on app type
void app_dispatch_main(void (^block)(void));

#endif// MAC_DISPATCH_H