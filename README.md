# Whisperer - Audio Transcription Hotkey

Cross-platform app: Hold hotkey → record audio → transcribe with local Whisper → paste text

- **macOS**: Hold FN key
- **Windows**: Hold Right Ctrl key
- **Linux**: Hold Right Ctrl key (planned)

## Quick Start

### macOS
```bash
# Build both applications
./build.sh

# Start FN key transcription (requires accessibility permissions)
./build/whisperer

# Record audio to file
./build/recorder output.wav
```

### Windows
```batch
# Build both applications (run as administrator)
build.bat

# Start Right Ctrl transcription (requires admin privileges)
build\Release\whisperer.exe

# Record audio to file
build\Release\recorder.exe output.wav
```

## Applications

### 🎤 Whisperer (`./build/whisperer`)

Main application that listens for FN key press/release to record and transcribe audio.

**Features:**

- Detects hotkey press/release events (FN on macOS, Right Ctrl on Windows)
- Records audio while hotkey is held
- Will transcribe with whisper.cpp (Phase 3)
- Will paste transcribed text (Phase 6)

**Usage:**

```bash
# macOS
./build/whisperer
# Press and hold FN key to record

# Windows (run as administrator)
build\Release\whisperer.exe
# Press and hold Right Ctrl key to record
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

## Build System

- **CMake** with **Ninja** generator for fast builds
- **Pure C** implementation with minimal Objective-C for macOS APIs
- **Framework linking**: ApplicationServices, AppKit, CoreFoundation, CoreAudio (macOS)
- **Cross-platform audio**: miniaudio library for Windows/Linux/macOS support

```bash
# macOS - Clean rebuild
rm -rf build && ./build.sh

# Windows - Clean rebuild (run as administrator)
rmdir /s /q build && build.bat

# Manual build (macOS)
mkdir -p build && cd build
cmake -G Ninja ..
ninja

# Manual build (Windows)
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Audio Recording Module

Reusable C API in `src/audio.h` and `src/audio_miniaudio.c`:

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

- **macOS/Windows/Linux** (cross-platform with miniaudio)
- **Xcode Command Line Tools** (`xcode-select --install`)
- **CMake** (`brew install cmake`)
- **Ninja** (`brew install ninja`)

### Permissions

#### macOS
**Accessibility Permission** (for whisperer):
- System Preferences → Security & Privacy → Privacy → Accessibility
- Add your Terminal application

**Microphone Permission** (for recorder):
- Automatically requested on first run
- System Preferences → Security & Privacy → Privacy → Microphone

#### Windows
**Administrator Privileges** (for whisperer):
- Right-click executable → Run as administrator
- Required for keyboard monitoring (SetWindowsHookEx)

**Microphone Permission**:
- No explicit permission needed on Windows
- Check Windows Settings → Privacy → Microphone if issues occur

## Project Status

### ✅ Phase 1: Foundation (Complete)

- Native keylogger detects FN key events
- Node.js app scaffold (deprecated)

### ✅ Phase 2: Pure C Rewrite (Complete)

- Single C application with CMake build system
- Integrated keylogger with Core Foundation event loop
- Reusable audio recording module
- Standalone recorder utility

### 🚧 Phase 3: whisper.cpp Integration (Planned)

- Clone and build whisper.cpp with Metal support
- Download base.en model (~150MB)
- Link whisper.cpp as library for direct function calls

### 🚧 Phase 4: Audio Recording Integration (Planned)

- Integrate audio module into main whisperer app
- Start/stop recording on FN key events
- Handle audio format requirements for whisper.cpp

### 🚧 Phase 5: Transcription Pipeline (Planned)

- Call whisper.cpp functions directly from C
- Process recorded audio through Whisper model
- Handle errors and empty results

### 🚧 Phase 6: Clipboard Integration (Planned)

- Paste transcribed text using CGEvent simulation
- Test with various applications

## Files

```
whisperer/
├── src/
│   ├── main.c          # Main whisperer application
│   ├── keylogger.c     # FN key detection and handling
│   ├── audio.h         # Audio recording API
│   ├── audio_miniaudio.c  # miniaudio implementation
│   ├── audio_permissions_macos.m  # macOS permissions
│   ├── miniaudio.h     # miniaudio library header
│   └── Recorder.c      # Standalone recording utility
├── CMakeLists.txt      # Build configuration
├── build.sh            # Build script
└── README.md           # This file
```

## Troubleshooting

**Build fails:**

```bash
# Install dependencies
brew install cmake ninja
xcode-select --install

# Clean rebuild
rm -rf build && ./build.sh
```

**Keylogger not working:**

- Grant Accessibility permission to Terminal
- Check Console.app for error messages

**Audio recording fails:**

- Grant Microphone permission when prompted
- Check audio input device in System Preferences → Sound

**Empty audio files:**

- Ensure microphone is working in other apps
- Try different sample rates: `./build/recorder -r 44100 test.wav`

## Architecture Notes

- **Pure C** core with minimal Objective-C for system APIs
- **Single binary** output with no runtime dependencies
- **Cross-platform audio** via miniaudio library
- **Reusable audio module** supports both file and buffer recording
- **CMake + Ninja** for fast incremental builds
- **Metal-optimized** whisper.cpp integration (planned)

---

⚠️ **Note**: Claude cannot test this application. All testing requires the human user to run the programs and report results.
