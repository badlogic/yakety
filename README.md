# Yakety

A voice-to-text input tool that works in any application. Hold a hotkey to record your voice, release to transcribe and paste the text.

## Features

- 🎤 **Universal Voice Input** - Works in any application
- 🚀 **Fast Local Transcription** - Uses OpenAI's Whisper model running locally
- 🔒 **Privacy First** - All processing happens on your device
- ⚡ **GPU Accelerated** - Metal on macOS, Vulkan on Windows (optional)
- 🖥️ **Cross-Platform** - macOS and Windows support

## Prerequisites

### macOS
- macOS 10.15 or later
- CMake 3.20+
- Ninja build system (`brew install ninja`)
- Xcode Command Line Tools
- ImageMagick (optional, for icon generation): `brew install imagemagick`

### Windows
- Windows 10 or later
- CMake 3.20+
- Ninja build system (`choco install ninja` or `winget install Ninja-build.Ninja`)
- Visual Studio 2019 or later (MSVC compiler)
- Vulkan SDK (optional, for GPU acceleration)
- ImageMagick (optional, for icon generation)


## Building

The build process is fully automated through CMake. It will:
1. Generate icons from the master icon (if ImageMagick is available)
2. Clone and build whisper.cpp with appropriate acceleration
3. Download the Whisper model (~150MB)
4. Build Yakety

### Quick Build

```bash
# Clone the repository
git clone https://github.com/badlogic/yakety.git
cd yakety

# Build with release settings
cmake --preset=release
cmake --build --preset=release

# Or build with debug symbols and sanitizers
cmake --preset=debug
cmake --build --preset=debug
```

### Manual Build

```bash
# Configure
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build
```

## Build Outputs

After building, you'll find the following in the `build/bin` directory:

- **yakety-cli** - CLI version that runs in terminal
- **yakety-app** (.app on macOS, .exe on Windows) - GUI version with system tray icon
- **recorder** - Standalone audio recording utility
- **test_transcription** - Test utility for verifying transcription

### macOS
```
build/bin/
├── yakety-cli                # CLI executable
├── Yakety.app/              # macOS app bundle with tray icon
├── recorder                 # Audio recorder
└── test_transcription       # Transcription test tool
```

### Windows
```
build\bin\
├── yakety-cli.exe           # CLI executable
├── Yakety.exe              # Windows tray application
├── recorder.exe            # Audio recorder
└── test_transcription.exe  # Transcription test tool
```

## Usage

### GUI Version (Recommended)
```bash
# macOS
open build/bin/Yakety.app

# Windows
build\bin\Yakety.exe
```

The app will appear in your system tray/menubar. Hold the **Fn** key (macOS) or **Right Ctrl** (Windows) to record, release to transcribe and paste.

### CLI Version
```bash
# Run in terminal
./build/bin/yakety-cli
```

### Testing
```bash
# Record audio to file
./build/bin/recorder test.wav

# Test transcription
./build/bin/test_transcription test.wav
```

## Permissions

### macOS
Grant accessibility permissions:
1. System Preferences → Security & Privacy → Privacy → Accessibility
2. Add Terminal (for CLI) or Yakety (for GUI) to the allowed list

### Windows
Run as administrator if hotkeys don't work in elevated applications.

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for details on the modular architecture and how to extend the application.

## Troubleshooting

### Build Issues
- **"whisper.cpp not found"** - The build will automatically clone it
- **"Model download failed"** - Check internet connection, the build will download ~150MB
- **"Ninja not found"** - Install Ninja or use `cmake -G "Unix Makefiles"` instead

### Runtime Issues
- **"No speech detected"** - Speak clearly and ensure microphone permissions are granted
- **"Hotkey not working"** - Check accessibility/admin permissions
- **"App damaged" (macOS)** - The build automatically signs the app, but you may need to allow it in Security settings

## License

Proprietary software © 2025 Mario Zechner. All rights reserved.

## Acknowledgments

- [OpenAI Whisper](https://github.com/openai/whisper) for the speech recognition model (MIT License)
- [whisper.cpp](https://github.com/ggerganov/whisper.cpp) by ggml authors for the C++ implementation (MIT License)
- [ggml](https://github.com/ggerganov/ggml) by ggml authors for the tensor library (MIT License)
- [miniaudio](https://github.com/mackron/miniaudio) by David Reid for cross-platform audio (Public Domain)