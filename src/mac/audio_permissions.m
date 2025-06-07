#ifdef __APPLE__
#import <AVFoundation/AVFoundation.h>
#include "../audio.h"

int audio_request_permissions(void) {
    __block int result = -1;
    
    if (@available(macOS 10.14, *)) {
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio 
                                 completionHandler:^(BOOL granted) {
            result = granted ? 0 : -1;
            dispatch_semaphore_signal(semaphore);
        }];
        
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    } else {
        // On older macOS versions, assume permission is granted
        result = 0;
    }
    
    return result;
}
#endif