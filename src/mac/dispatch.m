#include "dispatch.h"
#include "../app.h"
#import <Foundation/Foundation.h>

void app_dispatch_main(void (^block)(void)) {
    if (app_is_console()) {
        // For console apps, use CFRunLoop scheduling which works with manual CFRunLoopRunInMode calls
        CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopCommonModes, block);
        CFRunLoopWakeUp(CFRunLoopGetMain());
    } else {
        // For GUI apps, use GCD main queue which works with NSApp event processing
        dispatch_async(dispatch_get_main_queue(), block);
    }
}