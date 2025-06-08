#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include "../utils.h"
#include "../logging.h"
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
    
    log_info("🔍 Searching for Whisper model...\n");
    
    @autoreleasepool {
        // First check current directory
        log_info("  Checking current directory: ggml-base.en.bin\n");
        if (access("ggml-base.en.bin", F_OK) == 0) {
            log_info("  ✅ Found in current directory\n");
            return "ggml-base.en.bin";
        }
        
        // Check in app bundle Resources
        NSBundle* bundle = [NSBundle mainBundle];
        NSString* bundlePath = [bundle bundlePath];
        log_info("  Bundle path: %s\n", [bundlePath UTF8String]);
        
        // Check Resources/models/ggml-base.en.bin (where CMake puts it)
        NSString* resourceModelPath = [bundle pathForResource:@"models/ggml-base.en" ofType:@"bin"];
        if (resourceModelPath) {
            log_info("  ✅ Found in bundle Resources/models: %s\n", [resourceModelPath UTF8String]);
            strncpy(model_path, [resourceModelPath UTF8String], PATH_MAX - 1);
            return model_path;
        }
        
        // Check Resources/ggml-base.en.bin
        NSString* resourcePath = [bundle pathForResource:@"ggml-base.en" ofType:@"bin"];
        if (resourcePath) {
            log_info("  ✅ Found in bundle Resources: %s\n", [resourcePath UTF8String]);
            strncpy(model_path, [resourcePath UTF8String], PATH_MAX - 1);
            return model_path;
        }
        
        // Check in executable directory
        NSString* execPath = [bundle executablePath];
        NSString* execDir = [execPath stringByDeletingLastPathComponent];
        log_info("  Executable directory: %s\n", [execDir UTF8String]);
        
        // Check models subdirectory in executable directory
        NSString* modelsDir = [execDir stringByAppendingPathComponent:@"models"];
        NSString* modelInModelsDir = [modelsDir stringByAppendingPathComponent:@"ggml-base.en.bin"];
        log_info("  Checking: %s\n", [modelInModelsDir UTF8String]);
        if ([[NSFileManager defaultManager] fileExistsAtPath:modelInModelsDir]) {
            log_info("  ✅ Found in executable directory/models\n");
            strncpy(model_path, [modelInModelsDir UTF8String], PATH_MAX - 1);
            return model_path;
        }
        
        // Check directly in executable directory
        NSString* modelInExecDir = [execDir stringByAppendingPathComponent:@"ggml-base.en.bin"];
        log_info("  Checking: %s\n", [modelInExecDir UTF8String]);
        if ([[NSFileManager defaultManager] fileExistsAtPath:modelInExecDir]) {
            log_info("  ✅ Found in executable directory\n");
            strncpy(model_path, [modelInExecDir UTF8String], PATH_MAX - 1);
            return model_path;
        }
        
        // Check in build directory (parent of executable directory)
        NSString* buildPath = [[execDir stringByDeletingLastPathComponent] 
                               stringByAppendingPathComponent:@"ggml-base.en.bin"];
        log_info("  Checking: %s\n", [buildPath UTF8String]);
        if ([[NSFileManager defaultManager] fileExistsAtPath:buildPath]) {
            log_info("  ✅ Found in build directory\n");
            strncpy(model_path, [buildPath UTF8String], PATH_MAX - 1);
            return model_path;
        }
        
        // Check whisper.cpp/models directory relative to current directory
        NSString* whisperModelsPath = @"whisper.cpp/models/ggml-base.en.bin";
        log_info("  Checking: %s\n", [whisperModelsPath UTF8String]);
        if ([[NSFileManager defaultManager] fileExistsAtPath:whisperModelsPath]) {
            log_info("  ✅ Found in whisper.cpp/models\n");
            strncpy(model_path, [whisperModelsPath UTF8String], PATH_MAX - 1);
            return model_path;
        }
        
        // Check relative to executable's parent parent (for build/bin structure)
        NSString* projectRoot = [[execDir stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
        NSString* whisperModelsFromRoot = [projectRoot stringByAppendingPathComponent:@"whisper.cpp/models/ggml-base.en.bin"];
        log_info("  Checking: %s\n", [whisperModelsFromRoot UTF8String]);
        if ([[NSFileManager defaultManager] fileExistsAtPath:whisperModelsFromRoot]) {
            log_info("  ✅ Found relative to project root\n");
            strncpy(model_path, [whisperModelsFromRoot UTF8String], PATH_MAX - 1);
            return model_path;
        }
    }
    
    log_error("  ❌ Model not found in any location!\n");
    return NULL;
}

void utils_open_accessibility_settings(void) {
    @autoreleasepool {
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"]];
    }
}