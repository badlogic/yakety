# Yakety

Cross-platform speech-to-text application for instant voice transcription through global keyboard shortcuts. Press and hold FN (macOS) or Right Ctrl (Windows) to record, transcription is processed locally using Whisper models and automatically pasted into app with focus.

## Quick Start

```bash
# Build release version
./run.sh

# Build and run CLI
./run.sh cli

# Build debug version
./run.sh debug

# Build and run GUI app
./run.sh app
```

## Core Files

- **Entry Point**: `src/main.c` - Application lifecycle and transcription workflow
- **Platform Layer**: `src/mac/`, `src/windows/` - OS-specific implementations
- **Build Config**: `run.sh` - Build script with whisper.cpp integration
- **Audio**: `src/audio.c` - MiniAudio-based recording (16kHz mono)
- **Transcription**: `src/transcription.cpp` - Whisper.cpp integration
- **Models**: `src/models.c` - Model loading with fallback system

## Requirements

- **macOS**: 14.0+, Apple Silicon, accessibility permissions
- **Linux**: Cmake, Ninja, optional Glslc, optional Vulkan SDK
- **Windows**: Visual Studio or Ninja, optional Vulkan SDK
- **Dependencies**: whisper.cpp (auto-downloaded)

## macOS Permissions

Yakety requires three permissions on macOS to function properly:

1. **Accessibility** - For global keyboard event monitoring (detecting hotkey press/release)
2. **Input Monitoring** - For capturing keyboard events system-wide
3. **Microphone** - For recording audio

### Permission Inheritance

⚠️ **Important**: macOS enforces permission inheritance between parent and child processes. If you run `yakety-cli` from within another application (like VS Code's terminal), the CLI inherits the parent app's permission status.

**Example scenarios:**
- Running `yakety-cli` from VS Code terminal → VS Code needs accessibility/input monitoring permissions
- Running `yakety-cli` from iTerm2 → iTerm2 needs the permissions
- Running `yakety-cli` from Terminal.app → Terminal.app needs the permissions

### Managing Permissions

```bash
# Check current permission status
./permissions.sh

# Clear all permissions (requires sudo)
./permissions.sh clear
```

### Common Issues

1. **"Permission NOT GRANTED" despite adding yakety-cli to System Settings**
   - Check which app you're running the CLI from
   - Grant permissions to the parent app (e.g., VS Code, iTerm2)
   - Or run directly from Terminal.app with its permissions

2. **Permissions work for GUI app but not CLI**
   - GUI app (Yakety.app) has its own bundle ID: `com.yakety.app`
   - CLI inherits permissions from its parent process
   - They are tracked separately in macOS

3. **Permission dialogs not appearing**
   - Some permissions (like Input Monitoring) cannot be requested programmatically
   - Must be granted manually in System Settings
   - Yakety will guide you through the process with dialogs

### Granting Permissions Manually

1. Open **System Settings** → **Privacy & Security**
2. Navigate to the relevant section:
   - **Accessibility** → Add your terminal app or Yakety.app
   - **Input Monitoring** → Add your terminal app or Yakety.app
   - **Microphone** → Grant when prompted or add manually

### For Developers

When debugging permission issues:
- The app logs which permissions are granted/denied
- Parent process detection helps identify inheritance issues
- Use `./permissions.sh` to verify TCC database entries
- Run from different terminal apps to test various scenarios
- To build Linux devcontainer, run `./.devcontainer/build.sh`


## Distribution

```bash
# Package for distribution
./run.sh package
# Outputs in build/bin/: CLI tools, app bundles with embedded Whisper models
```
