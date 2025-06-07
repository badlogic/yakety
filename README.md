# Yakety

Cross-platform instant voice-to-text application. Hold a key, speak, release to paste transcribed text at cursor.

## Build Dependencies

### macOS
- Xcode Command Line Tools
- CMake 3.20+
- Ninja (optional: `brew install ninja`)

### Windows
- Visual Studio 2019+ with C++ tools OR MinGW-w64
- CMake 3.20+
- Ninja (optional)

### Linux (planned)
- GCC/Clang
- CMake 3.20+
- X11/Wayland dev libraries

## Build Steps

```bash
# Clone and initialize
git clone <repository>
cd yakety
git submodule update --init --recursive

# macOS
./build.sh

# Windows (native)
build.bat

# Windows (cross-compile from macOS)
./build-windows.sh
```

## Build Artifacts

- `yakety` - CLI version for terminal use
- `yakety-app` - GUI app with menu bar/system tray
- `recorder` - Standalone audio recording utility
- `test_transcription` - Whisper integration test
- `ggml-base.en.bin` - Whisper AI model (~150MB, auto-downloaded)

### Platform Packages
- **macOS**: `dist/Yakety.app` - Self-contained app bundle
- **Windows**: `dist/yakety.exe` + `dist/yakety-app.exe` - Standalone executables

## Architecture

### Core Components
- `src/audio.c/h` - Cross-platform audio capture (miniaudio)
- `src/transcription.cpp/h` - Whisper AI integration
- `src/keylogger.c` - Global hotkey monitoring
- `src/overlay.h` - Recording/transcribing visual feedback
- `src/menubar.h` - System tray/menu bar interface

### Platform-Specific
- `src/mac/` - macOS implementations (Objective-C)
  - `main.m` - CLI entry point
  - `main_app.m` - GUI app entry point
  - `menubar.m` - macOS menu bar
  - `overlay.m` - macOS overlay window
  - `audio_permissions.m` - macOS permission handling

- `src/windows/` - Windows implementations (Win32)
  - `main.c` - CLI entry point
  - `main_app.c` - GUI app entry point
  - `menubar.c` - Windows system tray
  - `overlay.c` - Windows overlay window
  - `keylogger.c` - Windows keyboard hooks
  - `clipboard.c` - Windows clipboard handling

### External Dependencies
- `whisper.cpp/` - Local AI speech recognition (submodule)
- `src/miniaudio.h` - Audio library (header-only)

### Build System
- `CMakeLists.txt` - Main build configuration
- `build.sh` - macOS build script
- `build.bat` - Windows build script
- `build-windows.sh` - Cross-compile for Windows from macOS
- `package.sh` - macOS app bundle packaging