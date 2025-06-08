# Yakety

A voice-to-text input tool that works in any application. Hold a hotkey to record your voice, release to transcribe and paste the text.

## Features

- 🎤 **Universal Voice Input** - Works in any application
- 🚀 **Fast Local Transcription** - Uses OpenAI's Whisper model running locally
- 🔒 **Privacy First** - All processing happens on your device
- ⚡ **GPU Accelerated** - Metal on macOS, Vulkan on Windows (optional)
- 🖥️ **Cross-Platform** - macOS and Windows support
- ⚙️ **Configurable** - Custom model paths, notification preferences
- 📝 **File Logging** - Automatic log rotation with debugging info
- 🔄 **Smart Model Loading** - Automatic fallback to base model if custom model fails

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

### Creating macOS Distribution Packages

After building, you can create distributable packages:

```bash
# Build the release version first
cmake --preset=release
cmake --build --preset=release

# Create a ZIP file (simplest option)
cmake --build --preset=release --target zip

# Create a DMG file
cmake --build --preset=release --target dmg

# Or create a fancy DMG with custom styling (requires: brew install create-dmg)
cmake --build --preset=release --target dmg-fancy
```

The packages will be created as:
- `build/Yakety-1.0.0.zip` - Simple ZIP archive
- `build/Yakety-1.0.0.dmg` - Standard disk image

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

Right-click the tray icon for options:
- **Launch at Login** - Start Yakety automatically when you log in
- **About** - Version and license information
- **Quit** - Exit the application

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

## Configuration

Yakety stores its configuration in an INI file:
- **macOS**: `~/.yakety/config.ini`
- **Windows**: `%APPDATA%\Yakety\config.ini`

### Configuration Options

```ini
[yakety]
# Path to custom Whisper model (optional)
model = /path/to/your/model.bin

# Show notification overlays (true/false)
show_notifications = true

# Launch at login (true/false)
launch_at_login = false
```

### Using Custom Models

You can use larger or specialized Whisper models:

```bash
# Download a model (example: large-v3-turbo)
curl -L -o ~/.yakety/models/ggml-large-v3-turbo-q8_0.bin \
  "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q8_0.bin"

# Update config.ini
[yakety]
model = ~/.yakety/models/ggml-large-v3-turbo-q8_0.bin
```

If a custom model fails to load, Yakety automatically falls back to the bundled base model.

### Logs

Debug logs are stored with automatic rotation (10MB max, 5 files):
- **macOS**: `~/Library/Logs/Yakety/`
- **Windows**: `%LOCALAPPDATA%\Yakety\Logs\`

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