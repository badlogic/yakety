# Yakety Architecture

## Overview

Yakety uses a sophisticated modular architecture that separates platform-specific code from business logic while introducing innovative patterns for cross-platform development. The architecture consists of:

1. **Module Headers** (`src/*.h`) - Define platform-agnostic interfaces
2. **Platform Implementations** (`src/mac/*.m`, `src/windows/*.c`) - Platform-specific implementations
3. **Business Logic** (`src/audio.c`, `src/transcription.cpp`, `src/menu.c`) - Platform-agnostic application code
4. **Unified Main** (`src/main.c`) - Single entry point for both CLI and tray apps
5. **Cross-Platform Utilities** - Advanced async patterns and thread management
6. **Configuration System** (`src/preferences.c`) - INI-based settings management

## Modules

### Core Platform Modules

- **`logging`** - Console/GUI logging abstraction with file rotation
- **`clipboard`** - Copy and paste operations with automatic text insertion
- **`overlay`** - On-screen status overlays with transparency
- **`dialog`** - Message boxes and alerts with permission handling
- **`menu`** - System tray/menubar management (singleton pattern) with dark mode support
- **`keylogger`** - Keyboard event monitoring (singleton pattern) with FN key detection
- **`app`** - Application lifecycle management with atomic state and unified run loops
- **`utils`** - Platform utilities (time, sleep, paths, atomic operations, responsive operations)
- **`preferences`** - INI-based configuration management with hotkey persistence

### Business Logic Modules

- **`audio`** - Audio recording using miniaudio (16kHz mono, Whisper-optimized)
- **`transcription`** - Speech-to-text using OpenAI Whisper with GPU acceleration (Metal/Vulkan)
- **`menu`** - Cross-platform menu business logic with hotkey configuration

## Build System

The advanced CMake build system creates:

1. **`platform`** - Static library containing all platform implementations
2. **`yakety-cli`** - CLI executable for terminal use
3. **`yakety-app`** - GUI/tray executable (.app bundle on macOS)
4. **`recorder`** - Standalone audio recording utility
5. **`transcribe`** - File-based transcription utility

**Build Features:**
- **Preset-based builds** - `cmake --preset release/debug/vs-debug`
- **Universal binaries** - macOS builds for Intel and Apple Silicon
- **Automated dependencies** - Whisper.cpp auto-download and compilation
- **Icon generation** - Automated asset creation from master icon
- **GPU acceleration** - Metal (macOS) and Vulkan (Windows) with fallbacks
- **Distribution packaging** - Automated DMG/ZIP creation with code signing

## Key Design Principles

1. **No Platform Code in Business Logic** - All platform operations go through module interfaces
2. **Single Source Truth** - One `main.c` for both app types using `APP_ENTRY_POINT` macro
3. **Clean Interfaces** - Well-defined module APIs with no platform leakage
4. **Singleton Patterns** - Global state managed through clean singleton APIs (menu, keylogger)
5. **Thread Safety** - Atomic operations for cross-thread state management
6. **Robust Error Handling** - Proper error propagation and cleanup on failure
7. **Platform-Agnostic Entry Points** - `APP_ENTRY_POINT` handles all platform variations automatically
8. **Blocking Async Pattern** - Revolutionary approach combining sync code simplicity with UI responsiveness
9. **Graceful Degradation** - Automatic fallbacks for model loading, GPU acceleration, and permissions
10. **Configuration Persistence** - INI-based settings with platform-appropriate storage locations

## Adding New Platforms

To add a new platform (e.g., Linux):

1. Create `src/linux/` directory
2. Implement all module interfaces for the platform
3. Update CMakeLists.txt to include the new platform sources
4. No changes needed to business logic or main.c

## Entry Point Architecture

The `APP_ENTRY_POINT` macro in `app.h` provides a clean abstraction for platform-specific entry points:

```c
// In main.c - simple, clean entry point
int app_main(int argc, char** argv, bool is_console) {
    // Main application logic here
}

APP_ENTRY_POINT  // Expands to platform-specific main/WinMain
```

**Platform Expansions:**
- **Windows Tray**: `WinMain()` → `app_main(0, NULL, false)`
- **Windows CLI**: `main()` → `app_main(argc, argv, true)`
- **macOS Tray**: `main()` → `app_main(0, NULL, false)`
- **macOS CLI**: `main()` → `app_main(argc, argv, true)`

## Singleton Pattern Implementation

Key modules use singleton patterns for clean global state management:

### Menu System
```c
// Clean singleton API
int menu_init(void);           // Initialize with default items
int menu_show(void);           // Show in system tray/menubar
void menu_hide(void);          // Hide menu
void menu_update_item(int index, const char* title);  // Update item by index
void menu_cleanup(void);       // Cleanup resources
```

### Keylogger System
```c
// Singleton with struct-based state (Windows)
typedef struct {
    HHOOK keyboard_hook;
    KeyCallback on_press;
    KeyCallback on_release;
    void* userdata;
    bool paused;
    KeyCombination target_combo;
    // ... other state
} KeyloggerState;
```

### Application State
```c
// Atomic state management
bool app_is_running(void);     // Thread-safe running state check
void app_quit(void);           // Thread-safe quit operation
bool app_is_console(void);     // Application type query
```

## Thread Safety

The architecture ensures thread safety through:

1. **Atomic Operations** - `utils_atomic_read_bool()` / `utils_atomic_write_bool()`
2. **Main Thread Operations** - Keyboard hooks and UI operations run on main thread
3. **Background Processing** - Model loading and transcription on worker threads
4. **Proper Synchronization** - Thread-safe callbacks and state updates

## Blocking Async Pattern

Yakety implements a unique "blocking async" pattern that combines the simplicity of synchronous code with responsive UI:

### Traditional Callback Approach (Complex)
```c
// Complex callback chain for model loading
utils_execute_async(load_model_async, NULL, on_model_loaded);
  -> on_model_loaded checks success
    -> if failure: utils_execute_main_thread(3000, delayed_retry, NULL)
      -> delayed_retry calls utils_execute_async again
    -> if success: continue_app_initialization()
```

### Blocking Async Approach (Simple)
```c
// Clean sequential code that keeps UI responsive
static void app_initialization_blocking(void) {
    overlay_show("Loading model");
    
    // This blocks BUT keeps UI responsive via event pumping!
    void* result = utils_execute_async_blocking(load_model_async, NULL);
    
    if (!result && !g_is_fallback_attempt) {
        overlay_show_error("Falling back to base model");
        g_is_fallback_attempt = true;
        
        // Responsive sleep - UI stays alive
        utils_sleep_responsive(3000);
        
        result = utils_execute_async_blocking(load_model_async, NULL);
    }
    
    if (!result) {
        overlay_show_error("Failed to load model");
        utils_sleep_responsive(3000);
        app_quit();
        return;
    }
    
    overlay_hide();
    continue_app_initialization();
}
```

### How Event Pumping Works

The blocking async functions maintain UI responsiveness by continuously processing platform events:

**macOS Implementation:**
```objc
while (!ctx.completed) {
    // Process UI events with short timeout
    NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES];
    if (event) {
        [NSApp sendEvent:event];  // Keep overlays updating, handle quit, etc.
    }
    
    if (!app_is_running()) break;  // Respect quit requests
    usleep(100);  // Small yield
}
```

**Windows Implementation:**
```c
while (InterlockedOr(&ctx.completed, 0) == 0) {
    MSG msg;
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);  // Keep UI responsive
    }
    
    if (!app_is_running()) break;  // Respect quit requests
    Sleep(1);  // Small yield
}
```

### Benefits

1. **Sequential Logic** - No callback spaghetti, reads like synchronous code
2. **UI Responsiveness** - Overlays update, user can quit, tray icon works
3. **Error Handling** - Clean if/else flow instead of nested callbacks
4. **Debugging** - Stack traces show the actual flow, not callback indirection
5. **Maintainability** - Easy to understand and modify the initialization sequence

### API Functions

- `app_execute_async_blocking(work_fn, arg)` - Execute async work, block with event pumping (**NOT reentrant**)
- `app_execute_async_blocking_all(tasks[], args[], count)` - Execute multiple async tasks concurrently (Promise.all() equivalent)
- `app_sleep_responsive(milliseconds)` - Sleep while keeping UI responsive

### Important Constraints

**`app_execute_async_blocking()` is NOT reentrant** - it cannot be called from:
- Event handlers (menu callbacks, key events, etc.)
- Completion callbacks from other async operations  
- Any code that runs during event pumping

This is by design - the function pumps the main event loop and only one blocking operation can be active at a time. For concurrent execution, use `app_execute_async_blocking_all()` instead.

### Concurrent Execution (Promise.all() Equivalent)

For scenarios where multiple independent async operations can run in parallel:

```c
// Example: Load multiple models concurrently
static void* load_base_model(void* arg) { /* ... */ }
static void* load_large_model(void* arg) { /* ... */ }
static void* verify_audio_devices(void* arg) { /* ... */ }

// Set up concurrent tasks
async_work_fn tasks[] = { load_base_model, load_large_model, verify_audio_devices };
void* args[] = { NULL, NULL, NULL };

// Execute all tasks concurrently while keeping UI responsive
void** results = app_execute_async_blocking_all(tasks, args, 3);

if (results) {
    // Check individual results
    bool base_loaded = (bool)(intptr_t)results[0];
    bool large_loaded = (bool)(intptr_t)results[1]; 
    bool audio_ready = (bool)(intptr_t)results[2];
    
    free(results); // Caller must free results array
}
```

**Benefits of Concurrent Execution:**
- **Parallelism** - Multiple CPU-intensive tasks run simultaneously
- **Faster Initialization** - Reduces total startup time
- **UI Responsiveness** - Event pumping keeps overlays and user interactions working
- **Simple Error Handling** - Check individual results after all complete
- **Resource Efficiency** - Better CPU utilization across cores

This pattern is particularly useful for initialization sequences, file operations, or any async work where you want the simplicity of blocking code without freezing the UI.

## Remote Development Infrastructure

Yakety includes a sophisticated remote development setup for cross-platform development, particularly useful for developing Windows features from macOS:

### WSL-Based Development Bridge

**Components:**
- **`wsl/setup-wsl-ssh.sh`** - One-time SSH server configuration in WSL
- **`wsl/start-wsl-ssh.bat`** - SSH port forwarding startup script
- **`wsl/REMOTE_DEVELOPMENT.md`** - Comprehensive development guide

### Development Workflow

```bash
# On Windows (one-time setup)
wsl/setup-wsl-ssh.sh

# On Windows (after each reboot) - run as administrator
wsl/start-wsl-ssh.bat

# From macOS (automated via Claude Code)
rsync -av src/ badlogic@192.168.1.21:/mnt/c/workspaces/yakety/src/
ssh badlogic@192.168.1.21 "cd /mnt/c/workspaces/yakety && cmd.exe /c 'build.bat'"
ssh badlogic@192.168.1.21 "build/bin/yakety-cli.exe"
```

### Benefits

1. **Native macOS Development** - Use familiar tools and IDE
2. **Windows Build Testing** - Immediate feedback on Windows-specific issues
3. **Claude Code Integration** - Seamless automated workflow
4. **SSH-Based Sync** - Fast, secure file synchronization
5. **Cross-Platform Debugging** - Test both platforms without context switching

### Network Architecture

- **WSL Ubuntu** - Linux development environment on Windows
- **SSH Bridge** - Secure connection between macOS and Windows WSL
- **Port Forwarding** - Windows batch script handles SSH port mapping
- **Rsync Protocol** - Efficient file synchronization

## Cross-Platform Main Thread Dispatching

### Problem

Different macOS app types require different main thread dispatching approaches:
- **GUI Apps**: Use `dispatch_async(dispatch_get_main_queue())` which integrates with NSApp event processing
- **Console Apps**: Need `CFRunLoopPerformBlock()` + `CFRunLoopWakeUp()` to work with manual `CFRunLoopRunInMode()` calls

### Solution: `app_dispatch_main()`

A centralized dispatching function that automatically chooses the right method:

```c
// In src/mac/dispatch.h
void app_dispatch_main(void (^block)(void));

// Usage throughout macOS modules
app_dispatch_main(^{
    // This block runs on main thread using the optimal method
    overlay_window.alpha = 1.0;
});
```

### Implementation

```c
void app_dispatch_main(void (^block)(void)) {
    if (app_is_console()) {
        // For console apps: CFRunLoop integration
        CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopCommonModes, block);
        CFRunLoopWakeUp(CFRunLoopGetMain());
    } else {
        // For GUI apps: GCD integration  
        dispatch_async(dispatch_get_main_queue(), block);
    }
}
```

### Benefits

1. **Automatic Selection** - No need for app type checking in every module
2. **Code Consistency** - All macOS modules use the same dispatching API
3. **Maintainability** - Dispatch logic centralized in one location
4. **Performance** - Each app type uses the optimal scheduling method

### Used By

- `utils.m` - Async work completion callbacks
- `overlay.m` - All UI update operations  
- `menu.m` - Menu item updates
- Any future macOS modules requiring main thread execution

## Enhanced Transcription System

Yakety includes a sophisticated transcription system built on OpenAI's Whisper model with several advanced features:

### GPU Acceleration Support

**Automatic Backend Selection:**
- **macOS**: Metal acceleration with CoreML fallback
- **Windows**: Vulkan acceleration with CPU fallback
- **Fallback Logic**: Automatically degrades to CPU if GPU initialization fails

### Advanced Model Management

**Features:**
- **Flash Attention**: Enabled for improved performance and memory efficiency
- **Custom Model Support**: User-configurable model paths via preferences
- **Automatic Fallback**: Failed custom models automatically fall back to bundled base model
- **Model Validation**: Comprehensive error handling during model loading
- **Language Configuration**: Configurable transcription language settings

### Text Processing Pipeline

**Smart Filtering:**
```c
// Removes non-speech artifacts
- Bracket notation: [MUSIC], [NOISE], [LAUGHTER]
- Star notation: *coughs*, *clears throat*
- Whisper timestamps and metadata
```

**Text Enhancement:**
- Automatic whitespace cleanup and normalization
- Trailing space addition for seamless pasting
- UTF-8 encoding consistency across platforms

### Error Recovery

**Robust Fallback System:**
1. **Primary Model Load** - Attempt user-configured model
2. **Fallback Model Load** - Use bundled base model on failure
3. **GPU Fallback** - Degrade to CPU processing if GPU fails
4. **Graceful Error Display** - User-friendly error messages via overlay system

## Configuration Management System

Yakety implements a comprehensive configuration system using platform-appropriate storage:

### Storage Locations

**macOS:**
- **Config**: `~/.yakety/config.ini`
- **Logs**: `~/Library/Logs/Yakety/`
- **Models**: `~/.yakety/models/`

**Windows:**
- **Config**: `%APPDATA%\Yakety\config.ini`
- **Logs**: `%LOCALAPPDATA%\Yakety\Logs\`
- **Models**: `%APPDATA%\Yakety\models\`

### Configuration Features

**Hotkey Persistence:**
```ini
[yakety]
# Platform-specific hotkey storage
# macOS: Fn key = keycode 63, modifiers 8388608
# Windows: Right Ctrl = scancode 29, extended flag 1
hotkey_keycode = 63
hotkey_modifiers = 8388608
```

**Advanced Settings:**
- **Custom Model Paths**: Full path specification with validation
- **Launch at Login**: OS integration for startup behavior
- **Transcription Language**: Whisper language model configuration
- **Debug Logging**: Configurable log levels and rotation

### Platform Integration

**macOS Integration:**
- **Launch Agent**: Automatic plist generation for login items
- **Sandbox Compatibility**: Proper path handling for App Store distribution
- **Accessibility Permissions**: Automated permission request flow

**Windows Integration:**
- **Registry Integration**: Windows startup folder management
- **UAC Compatibility**: Elevation-aware permission handling
- **File Association**: Optional file type associations for transcription

## Module Dependencies

```
main.c
  ├── app.h (lifecycle with atomic state)
  ├── logging.h (output)
  ├── audio.h (recording)
  ├── transcription.h (speech-to-text)
  ├── keylogger.h (singleton hotkey monitoring)
  ├── clipboard.h (paste results)
  ├── overlay.h (status display)
  ├── utils.h (timing, paths, atomic ops)
  └── menu.h (singleton tray management)
      └── dialog.h (menu actions)
```

## Platform Library Contents

### macOS (`libplatform.a`)
- `logging.m` - NSLog for GUI, printf for console with file rotation
- `clipboard.m` - NSPasteboard + CGEvent simulation with automatic pasting
- `overlay.m` - NSWindow overlay with transparency and multi-display support
- `dialog.m` - NSAlert with comprehensive accessibility permission handling
- `menu.m` - NSStatusItem singleton with proper retention, dark mode, and error handling
- `keylogger.c` - CGEventTap with sophisticated FN key detection via flags
- `app.m` - NSApplication with atomic state management and unified run loop
- `utils.m` - Foundation utilities including accessibility settings, atomic operations, and responsive timing
- `dispatch.m` - Centralized main thread dispatching that automatically chooses optimal method
- `preferences.m` - NSUserDefaults and file-based INI configuration management

### Windows (`platform.lib`)
- `logging.c` - OutputDebugString for GUI, printf for console with file rotation
- `clipboard.c` - Windows clipboard API + SendInput with automatic pasting
- `overlay.c` - Layered window with transparency and multi-monitor support
- `dialog.c` - MessageBox with comprehensive key combination capture support
- `menu.c` - System tray singleton with dark mode support, proper error handling, and icon management
- `keylogger.c` - Low-level keyboard hook with sophisticated struct-based state management
- `app.c` - Windows message pump with atomic state management and responsive event processing
- `utils.c` - Windows timer, file APIs, atomic operations, and responsive timing functions
- `preferences.c` - Registry and INI-based configuration management