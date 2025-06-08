#import <Foundation/Foundation.h>
#include "../utils.h"
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

double utils_get_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

void utils_sleep_ms(int milliseconds) {
    usleep(milliseconds * 1000);
}

const char* utils_get_model_path(void) {
    static char model_path[PATH_MAX] = {0};
    
    @autoreleasepool {
        // First check current directory
        if (access("ggml-base.en.bin", F_OK) == 0) {
            return "ggml-base.en.bin";
        }
        
        // Check in app bundle Resources
        NSBundle* bundle = [NSBundle mainBundle];
        NSString* resourcePath = [bundle pathForResource:@"ggml-base.en" ofType:@"bin"];
        if (resourcePath) {
            strncpy(model_path, [resourcePath UTF8String], PATH_MAX - 1);
            return model_path;
        }
        
        // Check in executable directory
        NSString* execPath = [bundle executablePath];
        NSString* execDir = [execPath stringByDeletingLastPathComponent];
        NSString* modelInExecDir = [execDir stringByAppendingPathComponent:@"ggml-base.en.bin"];
        
        if ([[NSFileManager defaultManager] fileExistsAtPath:modelInExecDir]) {
            strncpy(model_path, [modelInExecDir UTF8String], PATH_MAX - 1);
            return model_path;
        }
        
        // Check in build directory
        NSString* buildPath = [[execDir stringByDeletingLastPathComponent] 
                               stringByAppendingPathComponent:@"ggml-base.en.bin"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:buildPath]) {
            strncpy(model_path, [buildPath UTF8String], PATH_MAX - 1);
            return model_path;
        }
    }
    
    return NULL;
}