# Yakety

Cross-platform speech-to-text application for instant voice transcription through global keyboard shortcuts. Press and hold FN (macOS) or Right Ctrl (Windows) to record, transcription is processed locally using Whisper models and automatically pasted into app with focus.

## Quick Start

```bash
# Build and run
./build.sh
./build/bin/yakety-cli

# Build debug version
./build.sh debug

# Build release version
./build.sh release
```

## Core Files

- **Entry Point**: `src/main.c` - Application lifecycle and transcription workflow
- **Platform Layer**: `src/mac/`, `src/windows/` - OS-specific implementations
- **Build Config**: `build.sh` - Build script with whisper.cpp integration
- **Audio**: `src/audio.c` - MiniAudio-based recording (16kHz mono)
- **Transcription**: `src/transcription.cpp` - Whisper.cpp integration
- **Models**: `src/models.c` - Model loading with fallback system

## Requirements

- **macOS**: 14.0+, Apple Silicon, accessibility permissions
- **Windows**: Visual Studio or Ninja, optional Vulkan SDK
- **Dependencies**: whisper.cpp (auto-downloaded)

## Distribution

```bash
# Package for distribution
./build.sh release
# Outputs in build/bin/: CLI tools, app bundles with embedded Whisper models
```
