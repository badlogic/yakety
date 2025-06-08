#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>
#include "../utils.h"
#include "../logging.h"
#include "../config.h"
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

double utils_get_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

void utils_sleep_ms(int milliseconds) {
    usleep(milliseconds * 1000);
}

const char* utils_get_model_path_with_config(void* config) {
    static char model_path[PATH_MAX] = {0};
    
    log_info("🔍 Searching for Whisper model...\n");
    
    // Check if we have a config to get model path from
    if (config) {
        Config* cfg = (Config*)config;
        const char* config_model = config_get_string(cfg, "model");
        if (config_model && strlen(config_model) > 0) {
            log_info("  Checking config model: %s\n", config_model);
            if (access(config_model, F_OK) == 0) {
                log_info("  ✅ Found model from config\n");
                strncpy(model_path, config_model, PATH_MAX - 1);
                return model_path;
            } else {
                log_info("  ❌ Config model not found or not accessible\n");
            }
        }
    }
    
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

const char* utils_get_model_path(void) {
    return utils_get_model_path_with_config(NULL);
}

void utils_open_accessibility_settings(void) {
    @autoreleasepool {
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"]];
    }
}

bool utils_is_launch_at_login_enabled(void) {
    @autoreleasepool {
        // Use SMAppService for macOS 13.0+, fall back to older method for earlier versions
        if (@available(macOS 13.0, *)) {
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wunguarded-availability-new"
            NSError *error = nil;
            SMAppService *service = [SMAppService mainAppService];
            SMAppServiceStatus status = [service status];
            return status == SMAppServiceStatusEnabled;
            #pragma clang diagnostic pop
        } else {
            // For older macOS versions, check LaunchAgents
            NSString* bundleID = [[NSBundle mainBundle] bundleIdentifier];
            NSString* launchAgentPath = [NSString stringWithFormat:@"%@/Library/LaunchAgents/%@.plist", 
                                       NSHomeDirectory(), bundleID];
            return [[NSFileManager defaultManager] fileExistsAtPath:launchAgentPath];
        }
    }
}

bool utils_set_launch_at_login(bool enabled) {
    @autoreleasepool {
        // Use SMAppService for macOS 13.0+, fall back to older method for earlier versions
        if (@available(macOS 13.0, *)) {
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wunguarded-availability-new"
            NSError *error = nil;
            SMAppService *service = [SMAppService mainAppService];
            
            if (enabled) {
                if ([service registerAndReturnError:&error]) {
                    log_info("Successfully enabled launch at login\n");
                    return true;
                } else {
                    log_error("Failed to enable launch at login: %s\n", 
                            [[error localizedDescription] UTF8String]);
                    return false;
                }
            } else {
                if ([service unregisterAndReturnError:&error]) {
                    log_info("Successfully disabled launch at login\n");
                    return true;
                } else {
                    log_error("Failed to disable launch at login: %s\n", 
                            [[error localizedDescription] UTF8String]);
                    return false;
                }
            }
            #pragma clang diagnostic pop
        } else {
            // For older macOS versions, use LaunchAgents
            NSString* bundleID = [[NSBundle mainBundle] bundleIdentifier];
            NSString* appPath = [[NSBundle mainBundle] bundlePath];
            NSString* launchAgentPath = [NSString stringWithFormat:@"%@/Library/LaunchAgents/%@.plist", 
                                       NSHomeDirectory(), bundleID];
            
            if (enabled) {
                // Create LaunchAgent plist
                NSDictionary* plist = @{
                    @"Label": bundleID,
                    @"ProgramArguments": @[appPath],
                    @"RunAtLoad": @YES,
                    @"LSUIElement": @YES
                };
                
                // Create LaunchAgents directory if needed
                NSString* launchAgentsDir = [launchAgentPath stringByDeletingLastPathComponent];
                [[NSFileManager defaultManager] createDirectoryAtPath:launchAgentsDir 
                                          withIntermediateDirectories:YES 
                                                           attributes:nil 
                                                                error:nil];
                
                // Write plist
                return [plist writeToFile:launchAgentPath atomically:YES];
            } else {
                // Remove LaunchAgent plist
                NSError* error = nil;
                return [[NSFileManager defaultManager] removeItemAtPath:launchAgentPath error:&error];
            }
        }
    }
}

// Async work implementation
#include <pthread.h>

typedef struct {
    async_work_fn work;
    void* arg;
    async_callback_fn callback;
} AsyncWorkData;

static void* async_work_thread(void* data) {
    AsyncWorkData* work_data = (AsyncWorkData*)data;
    void* result = work_data->work(work_data->arg);
    
    // Schedule callback on main thread
    dispatch_async(dispatch_get_main_queue(), ^{
        work_data->callback(result);
        free(work_data);
    });
    
    return NULL;
}

void utils_async_execute(async_work_fn work, void* arg, async_callback_fn callback) {
    AsyncWorkData* work_data = malloc(sizeof(AsyncWorkData));
    work_data->work = work;
    work_data->arg = arg;
    work_data->callback = callback;
    
    pthread_t thread;
    pthread_create(&thread, NULL, async_work_thread, work_data);
    pthread_detach(thread);
}

// Delay execution implementation
typedef struct {
    delay_callback_fn callback;
    void* arg;
} DelayCallbackData;

void utils_delay_on_main_thread(int delay_ms, delay_callback_fn callback, void* arg) {
    DelayCallbackData* data = malloc(sizeof(DelayCallbackData));
    data->callback = callback;
    data->arg = arg;
    
    dispatch_time_t delay = dispatch_time(DISPATCH_TIME_NOW, delay_ms * NSEC_PER_MSEC);
    dispatch_after(delay, dispatch_get_main_queue(), ^{
        data->callback(data->arg);
        free(data);
    });
}

// Platform abstraction implementations
static char g_config_dir_buffer[PATH_MAX] = {0};

const char* utils_get_config_dir(void) {
    @autoreleasepool {
        NSString* home = NSHomeDirectory();
        NSString* configDir = [home stringByAppendingPathComponent:@".yakety"];
        strncpy(g_config_dir_buffer, [configDir UTF8String], PATH_MAX - 1);
        g_config_dir_buffer[PATH_MAX - 1] = '\0';
        return g_config_dir_buffer;
    }
}

bool utils_ensure_dir_exists(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    
    // Create directory with 0755 permissions
    return mkdir(path, 0755) == 0;
}

FILE* utils_fopen_read(const char* path) {
    return fopen(path, "r");
}

FILE* utils_fopen_read_binary(const char* path) {
    return fopen(path, "rb");
}

FILE* utils_fopen_write(const char* path) {
    return fopen(path, "w");
}

FILE* utils_fopen_write_binary(const char* path) {
    return fopen(path, "wb");
}

FILE* utils_fopen_append(const char* path) {
    return fopen(path, "a");
}

char* utils_strdup(const char* str) {
    return strdup(str);
}

int utils_stricmp(const char* s1, const char* s2) {
    return strcasecmp(s1, s2);
}