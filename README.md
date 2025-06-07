# Yakety - Instant Voice-to-Text

Cross-platform app that lets you instantly convert speech to text with a simple hotkey. Hold the key, speak naturally, release to paste transcribed text anywhere.

- **macOS**: Hold FN key
- **Windows**: Hold Right Ctrl key
- **Linux**: Hold Right Ctrl key (planned)

## Quick Start

### macOS
```bash
# Build both applications
./build.sh

# Start FN key transcription (requires accessibility permissions)
./build/yakety

# Record audio to file
./build/recorder output.wav
```

### Windows
```batch
# Build both applications (run as administrator)
build.bat

# Start Right Ctrl transcription (requires admin privileges)
build\Release\yakety.exe

# Record audio to file
build\Release\recorder.exe output.wav
```

## Applications

### 🎤 Yakety

Main application that converts your speech to text instantly. Just hold the hotkey, speak, and release to paste.

**Features:**

- Detects hotkey press/release events (FN on macOS, Right Ctrl on Windows)
- Records audio while hotkey is held
- Transcribes speech using whisper.cpp
- Automatically pastes transcribed text at cursor position
- Shows visual feedback overlay during recording/transcribing
- Menu bar/system tray icon (GUI version)

**Versions:**

- **CLI Version** (`yakety`) - Runs in terminal
- **GUI Version** (`yakety-app`) - Runs with menu bar/system tray icon

**Usage:**

```bash
# macOS CLI
./build/yakety

# macOS GUI (recommended)
open ./build/yakety-app.app

# Windows CLI (run as administrator)
build\Release\yakety.exe

# Windows GUI (run as administrator)
build\Release\yakety-app.exe
```

### 🎵 Recorder (`./build/recorder`)

Standalone audio recording utility with flexible configuration.

**Usage:**

```bash
# High quality recording (44.1kHz stereo)
./build/recorder recording.wav

# Whisper-optimized (16kHz mono)
./build/recorder -w speech.wav

# Custom settings
./build/recorder -r 48000 -c 1 -b 16 custom.wav

# Show all options
./build/recorder --help
```

**Options:**

- `-r, --rate RATE` - Sample rate (default: 44100)
- `-c, --channels CHAN` - Channels: 1=mono, 2=stereo (default: 2)
- `-b, --bits BITS` - Bits per sample: 16, 24, or 32 (default: 16)
- `-w, --whisper` - Use Whisper settings (16kHz mono)
- `-h, --help` - Show help

## Build Instructions

### macOS

```bash
# One-step build (recommended)
./build.sh

# This will:
# 1. Generate icons from SVG (if tools available)
# 2. Clone and build whisper.cpp with Metal support
# 3. Download the Whisper model (~150MB)
# 4. Build Yakety with all features
# 5. Sign the app bundle (ad-hoc signature)
```

### Windows

```batch
# Build on Windows (run as administrator)
build.bat
```

### Windows Cross-Compilation (from macOS)

```bash
# Install MinGW-w64
brew install mingw-w64

# Cross-compile for Windows
./build-windows.sh
```

### Cleaning Build Artifacts

```bash
# Clean only Yakety build files
./clean.sh

# Clean Yakety and whisper.cpp builds
./clean.sh --whisper

# Clean everything (asks about models)
./clean.sh --all

# Nuclear option - remove everything including whisper.cpp source
./deep-clean.sh
```

## Custom Icons

Yakety uses your custom SVG icon for both the app icon and menu bar/tray icon.

```bash
# Generate icons from SVG (requires librsvg and imagemagick)
./generate-icons.sh

# This creates:
# - assets/yakety.icns (macOS app icon)
# - assets/generated/menubar.png (menu bar icon)
# - assets/generated/menubar@2x.png (Retina menu bar icon)
```

The build process automatically includes these icons in the app bundle.

## Audio Recording Module

Reusable C API in `src/audio.h` and `src/audio.c`:

```c
// Create recorder with configuration
AudioRecorder* recorder = audio_recorder_create(&WHISPER_AUDIO_CONFIG);

// Record to file
audio_recorder_start_file(recorder, "output.wav");
// ... recording happens ...
audio_recorder_stop(recorder);

// Record to memory buffer (for whisper processing)
audio_recorder_start_buffer(recorder);
// ... recording happens ...
audio_recorder_stop(recorder);
const float* data = audio_recorder_get_data(recorder, &size);

// Cleanup
audio_recorder_destroy(recorder);
```

**Configurations:**

- `WHISPER_AUDIO_CONFIG` - 16kHz mono (optimal for Whisper)
- `HIGH_QUALITY_AUDIO_CONFIG` - 44.1kHz stereo

## Requirements

### Build Requirements

- **CMake** 3.20 or later
- **Ninja** (recommended) or Make
- **C/C++ Compiler**:
  - macOS: Xcode Command Line Tools (`xcode-select --install`)
  - Windows: Visual Studio 2019+ or MinGW-w64
  - Linux: GCC or Clang

### Optional (for icons on macOS)

```bash
brew install librsvg imagemagick
```

### Runtime Requirements

- **macOS**: 10.14 (Mojave) or later
- **Windows**: Windows 10 or later
- **Linux**: Ubuntu 20.04+ or equivalent (planned)
- **RAM**: 4GB minimum (8GB recommended for faster transcription)
- **Storage**: ~200MB for Whisper model

### Permissions

#### macOS
**Accessibility Permission** (for yakety):
- System Preferences → Security & Privacy → Privacy → Accessibility
- Add your Terminal application

**Microphone Permission** (for recorder):
- Automatically requested on first run
- System Preferences → Security & Privacy → Privacy → Microphone

#### Windows
**Administrator Privileges** (for yakety):
- Right-click executable → Run as administrator
- Required for keyboard monitoring (SetWindowsHookEx)

**Microphone Permission**:
- No explicit permission needed on Windows
- Check Windows Settings → Privacy → Microphone if issues occur

## Project Status

### ✅ Completed Features

- Native keyboard monitoring (FN key on macOS, Right Ctrl on Windows)
- Cross-platform audio recording with miniaudio
- whisper.cpp integration with Metal acceleration (macOS)
- Real-time speech-to-text transcription
- Automatic clipboard paste functionality
- Visual feedback overlay during recording/transcribing
- Menu bar (macOS) and system tray (Windows) integration
- Custom SVG icon support with automatic generation
- Standalone audio recorder utility

### 🚧 Planned Features

- Linux support
- Multiple language support
- Custom hotkey configuration
- Transcription history
- Settings UI for model selection
- CUDA acceleration (Windows/Linux)

## Project Structure

```
yakety/
├── assets/
│   ├── yakety.svg          # Source SVG icon
│   ├── yakety.icns         # Generated macOS app icon
│   └── generated/          # Generated PNG icons
├── src/
│   ├── mac/                # macOS-specific code
│   │   ├── main.m          # macOS CLI entry point
│   │   ├── main_app.m      # macOS GUI app entry point
│   │   ├── overlay.m       # macOS overlay window
│   │   ├── menubar.m       # macOS menu bar/status item
│   │   └── audio_permissions.m  # macOS audio permissions
│   ├── windows/            # Windows-specific code
│   │   ├── main.c          # Windows CLI entry point
│   │   ├── main_app.c      # Windows GUI app entry point
│   │   ├── keylogger.c     # Windows keyboard hooks
│   │   ├── overlay.c       # Windows overlay window
│   │   ├── menubar.c       # Windows system tray
│   │   └── clipboard.c     # Windows clipboard operations
│   ├── keylogger.c         # macOS FN key detection
│   ├── audio.h             # Audio recording API
│   ├── audio.c             # Cross-platform audio (miniaudio)
│   ├── miniaudio.h         # miniaudio library header
│   ├── transcription.h     # Whisper transcription API
│   ├── transcription.cpp   # Whisper integration
│   ├── recorder.c          # Standalone recording utility
│   └── test_transcription.c  # Transcription test utility
├── cmake/
│   └── toolchain-mingw64.cmake  # Windows cross-compilation
├── whisper.cpp/            # Whisper AI (cloned by build script)
├── CMakeLists.txt          # Cross-platform build configuration
├── Info.plist              # macOS app bundle metadata
├── build.sh                # macOS/Linux build script
├── build.bat               # Windows build script
├── build-windows.sh        # Windows cross-compilation script
├── clean.sh                # Clean build artifacts
├── clean.bat               # Windows clean script
├── deep-clean.sh           # Remove everything including deps
├── generate-icons.sh       # Generate icons from SVG
├── sign-app.sh             # Sign the macOS app bundle
└── README.md               # This file
```

## Troubleshooting

**"yakety-app is damaged and can't be opened" error:**

This happens when macOS quarantines unsigned apps. The build script now automatically signs the app, but if you still see this:

```bash
# Option 1: Remove quarantine and sign manually
./sign-app.sh
open ./build/yakety-app.app

# Option 2: Allow in System Settings
# Go to System Settings → Privacy & Security → "Open Anyway"

# Option 3: Run from command line (always works)
./build/yakety-app.app/Contents/MacOS/yakety-app
```

**Build fails:**

```bash
# Install dependencies
brew install cmake ninja
xcode-select --install

# Clean rebuild
./clean.sh --all
./build.sh
```

**Keylogger not working:**

- Grant Accessibility permission to Terminal or Yakety app
- Check Console.app for error messages
- Make sure to grant permission to the correct app:
  - For CLI: Your terminal app (Terminal.app, iTerm2, etc.)
  - For GUI: yakety-app.app

**Audio recording fails:**

- Grant Microphone permission when prompted
- Check audio input device in System Preferences → Sound

**Empty audio files:**

- Ensure microphone is working in other apps
- Try different sample rates: `./build/recorder -r 44100 test.wav`

## Architecture

- **Core**: C with minimal platform-specific code (Objective-C for macOS, Win32 for Windows)
- **Audio**: Cross-platform via miniaudio (single-header library)
- **AI**: whisper.cpp for local speech recognition
- **Build**: CMake with Ninja for fast builds
- **GPU**: Metal acceleration on macOS, CUDA on Windows (optional)
- **Distribution**: Single binary with embedded resources

## Build Configuration

The build system automatically:
- Downloads and builds whisper.cpp with GPU support
- Downloads the Whisper AI model (~150MB)
- Bundles the model inside the app (macOS)
- Generates app icons from SVG
- Creates proper app bundles on macOS
- Signs the app with ad-hoc signature
- Links all dependencies statically

## Distribution

### macOS App Bundle

The macOS app is fully self-contained:
- Whisper model embedded at `Yakety.app/Contents/Resources/models/ggml-base.en.bin`
- All icons and resources included
- No external dependencies required
- Ready for distribution after proper code signing

### Code Signing

For local use, the build script signs with an ad-hoc signature. For distribution:

```bash
# Sign with Developer ID (requires Apple Developer account)
codesign --force --deep --sign "Developer ID Application: Your Name" ./build/yakety-app.app

# Notarize for distribution
xcrun notarytool submit ./build/yakety-app.app --wait

# Create DMG for distribution
hdiutil create -volname "Yakety" -srcfolder ./build/yakety-app.app -ov -format UDZO yakety.dmg
```

---

## About Yakety

Yakety transforms the way you input text by making voice transcription as simple as holding a key. Perfect for:
- Quick messages and emails
- Taking notes during meetings
- Accessibility needs
- Reducing typing strain
- Capturing thoughts instantly

Using state-of-the-art Whisper AI running locally on your machine, Yakety ensures your voice data never leaves your computer.

⚠️ **Note**: Claude cannot test this application. All testing requires the human user to run the programs and report results.
