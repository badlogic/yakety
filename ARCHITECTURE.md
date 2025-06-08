# Yakety Architecture

## Overview

Yakety uses a clean modular architecture that separates platform-specific code from business logic. The architecture consists of:

1. **Module Headers** (`src/*.h`) - Define platform-agnostic interfaces
2. **Platform Implementations** (`src/mac/*.m`, `src/windows/*.c`) - Platform-specific implementations
3. **Business Logic** (`src/audio.c`, `src/transcription.cpp`) - Platform-agnostic application code
4. **Unified Main** (`src/main.c`) - Single entry point for both CLI and tray apps

## Modules

### Core Platform Modules

- **`logging`** - Console/GUI logging abstraction
- **`clipboard`** - Copy and paste operations
- **`overlay`** - On-screen text overlay
- **`dialog`** - Message boxes and alerts
- **`menu`** - System tray/menubar management
- **`keylogger`** - Keyboard event monitoring
- **`app`** - Application lifecycle management
- **`utils`** - Platform utilities (time, sleep, paths)

### Business Logic Modules

- **`audio`** - Audio recording using miniaudio
- **`transcription`** - Speech-to-text using Whisper

## Build System

The CMake build system creates:

1. **`platform`** - Static library containing all platform implementations
2. **`yakety`** - CLI executable (links to platform library)
3. **`yakety-app`** - GUI/tray executable (links to platform library, defines YAKETY_TRAY_APP)

## Key Design Principles

1. **No Platform Code in Business Logic** - All platform operations go through module interfaces
2. **Single Source Truth** - One `main.c` for both app types using conditional compilation
3. **Clean Interfaces** - Well-defined module APIs with no platform leakage
4. **Consistent Patterns** - All modules follow similar initialization/cleanup patterns

## Adding New Platforms

To add a new platform (e.g., Linux):

1. Create `src/linux/` directory
2. Implement all module interfaces for the platform
3. Update CMakeLists.txt to include the new platform sources
4. No changes needed to business logic or main.c

## Module Dependencies

```
main.c
  ├── app.h (lifecycle)
  ├── logging.h (output)
  ├── audio.h (recording)
  ├── transcription.h (speech-to-text)
  ├── keylogger.h (hotkey monitoring)
  ├── clipboard.h (paste results)
  ├── overlay.h (status display)
  ├── utils.h (timing, paths)
  └── menu.h (tray app only)
      └── dialog.h (menu actions)
```

## Platform Library Contents

### macOS (`libplatform.a`)
- `logging.m` - NSLog for GUI, printf for console
- `clipboard.m` - NSPasteboard + CGEvent simulation
- `overlay.m` - NSWindow overlay
- `dialog.m` - NSAlert
- `menu.m` - NSStatusItem
- `keylogger.c` - CGEventTap
- `app.m` - NSApplication
- `utils.m` - Foundation utilities

### Windows (`platform.lib`)
- `logging.c` - OutputDebugString for GUI, printf for console
- `clipboard.c` - Windows clipboard API + SendInput
- `overlay.c` - Layered window
- `dialog.c` - MessageBox
- `menu.c` - System tray API
- `keylogger.c` - Low-level keyboard hook
- `app.c` - Windows message pump
- `utils.c` - Windows timer and file APIs