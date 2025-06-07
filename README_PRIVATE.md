# Whisperer - Private Repository

This is the private repository for Whisperer, a macOS voice transcription app.

## Quick Start

### Using Pre-built App
1. Open `dist/Whisperer-1.0.dmg`
2. Drag Whisperer to Applications
3. Launch and grant permissions

### Building from Source
```bash
./setup.sh          # One-time setup
./build.sh          # Build the project
./package.sh        # Create app bundle and DMG
```

## Project Structure
- `src/` - Source code
- `test/` - Test audio files
- `dist/` - Pre-built distribution files
- `build/` - Build output (git ignored)
- `whisper.cpp/` - Cloned dependency (git ignored)

## Key Features
- Hold FN key to record and transcribe
- Local processing with whisper.cpp
- GPU acceleration via Metal
- Menu bar icon with quit option
- Recording/transcribing status overlay

## Distribution
The `dist/` folder contains:
- `Whisperer.app` - macOS application
- `Whisperer-1.0.dmg` - Disk image for distribution
- `com.lemmy.whisperer.plist` - Launch agent for auto-start
- `install.sh` - Installation script

## Development
- CLI binary: `./build/whisperer`
- Test recorder: `./build/recorder output.wav`
- Test transcription: `./build/test_transcription test/test1.wav`
