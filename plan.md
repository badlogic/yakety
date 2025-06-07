# Whisperer - FN Key Audio Transcription

Simple macOS app: Hold FN key → record audio → transcribe with local Whisper → paste text

## Architecture

- **Pure C application** with minimal Objective-C for macOS system APIs
- **whisper.cpp**: GPU-accelerated transcription via Metal on Apple Silicon
- **Single binary**: No Node.js, no dependencies, just compile and run

## Files

```
whisperer/
├── src/
│   ├── main.c            # Main whisperer application loop
│   ├── keylogger.c       # FN key detection (CGEventTap)
│   ├── audio.h/.m        # Audio recording module (AVAudioEngine)
│   ├── Recorder.c        # Standalone audio recording utility
│   ├── transcription.c   # whisper.cpp integration (planned)
│   ├── overlay.m         # Status overlay (planned)
│   └── clipboard.m       # Clipboard + paste (planned)
├── test/                 # Test audio files for whisper development
│   ├── test1.wav         # Sample audio for testing transcription
│   └── test2.wav         # Sample audio for testing transcription
├── whisper.cpp/          # Cloned repo for GPU transcription (planned)
├── CMakeLists.txt        # CMake build configuration
├── build.sh              # Build script with Ninja generator
├── README.md             # Documentation and usage guide
└── plan.md               # This development plan
```

## Flow

1. `./build.sh` → clones whisper.cpp, cmake build with Metal + main binary
2. `./build/whisperer` → starts app, listens for FN key
3. FN keydown → start audio recording + show "Recording..." overlay
4. FN keyup → stop recording, show "Transcribing..." overlay, run whisper.cpp, paste result, hide overlay

## Dependencies

- **System Frameworks**: ApplicationServices, AVFoundation, AppKit
- **whisper.cpp**: GPU-accelerated transcription (cloned + compiled during build)
- **No runtime dependencies**: Single binary, no Node.js/npm needed

## Build Process (CMake)

- Clone whisper.cpp repo during build
- CMake configure with Metal support and framework linking
- Download base.en model (~150MB)
- Build whisper.cpp as library and link into main application
- Output: Single `build/whisperer` binary

## Status

### Phase 1: Foundation ✅

- [x] Native keylogger (C) - detects FN key, outputs JSON to stdout
- [x] Node.js app scaffold - spawns keylogger, parses key events (deprecated)
- [x] Package.json with ESM + TypeScript setup (deprecated)
- [x] Build script compiles native keylogger + TypeScript (deprecated)

### Phase 2: Pure C Rewrite ✅

- [x] Rewrite as single C application (no Node.js)
- [x] Integrate keylogger.c directly into main application
- [x] Create CMakeLists.txt with framework linking
- [x] Update build.sh for CMake workflow with Ninja generator
- [x] **BONUS**: Reusable audio recording module (audio.h/.m)
- [x] **BONUS**: Standalone Recorder utility for testing
- [x] **BONUS**: Comprehensive README.md documentation

### Phase 3: whisper.cpp Integration ✅

- [x] Update build script to clone whisper.cpp repo
- [x] Compile whisper.cpp with Metal support (GGML_METAL=1, BUILD_SHARED_LIBS=OFF)
- [x] Download base.en model (~150MB) during build
- [x] Create transcription.cpp wrapper for whisper.cpp functions (C-style C++)
- [ ] Test transcription with test/test1.wav and test/test2.wav files

**Note**: Use `GGML_METAL=1` instead of deprecated `WHISPER_METAL=1`. Use `BUILD_SHARED_LIBS=OFF` for static libraries.

### Phase 4: Audio Recording Integration ✅ (Foundation Complete)

- [x] Implement audio recording using AVAudioEngine
- [x] Reusable audio module with file and buffer recording
- [x] Handle audio format requirements (16kHz mono for whisper)
- [ ] Integrate audio module into main whisperer application
- [ ] Start/stop recording on FN key press/release events

### Phase 4.5: Status Overlay 🚧

- [ ] Create overlay window using NSWindow (transparent, always on top)
- [ ] Position at center-bottom of screen
- [ ] Show "🎤 Recording..." when FN key pressed
- [ ] Show "🧠 Transcribing..." during whisper processing
- [ ] Hide overlay when transcription complete

### Phase 5: Transcription Pipeline 🚧

- [ ] Link whisper.cpp as library (not separate binary)
- [ ] Call whisper.cpp functions directly from C
- [ ] Handle transcription errors and empty results
- [ ] Process audio data from memory buffer (not temp files)
- [ ] Test with provided test/test1.wav and test/test2.wav

### Phase 6: Main Application Integration 🚧

- [ ] Integrate audio recording into main whisperer app
- [ ] Connect FN key events to start/stop recording
- [ ] Connect transcription pipeline to audio data
- [ ] Add basic console output for transcribed text

### Phase 7: Clipboard Integration (Objective-C) 🚧

- [ ] Implement clipboard operations using NSPasteboard
- [ ] Simulate paste operation using CGEvent
- [ ] Test with various applications (TextEdit, browser, etc.)

### Phase 8: Status Overlay & Polish 🚧

- [ ] Create overlay window using NSWindow (transparent, always on top)
- [ ] Position at center-bottom of screen
- [ ] Show "🎤 Recording..." when FN key pressed
- [ ] Show "🧠 Transcribing..." during whisper processing
- [ ] Add error handling and user feedback
- [ ] Test end-to-end flow: FN key → record → transcribe → paste
- [ ] Optimize performance and reliability

## Testing Resources

- **test/test1.wav** and **test/test2.wav** - Sample audio files for whisper development
- **./build/recorder** - Utility for creating test recordings
- **Recorder utility examples**:
   ```bash
   ./build/recorder -w test/speech.wav      # Whisper-optimized recording
   ./build/recorder test/high_quality.wav  # High quality recording
   ```

## Testing Note

⚠️ **Claude cannot test this application** - Claude cannot press keys, record audio, or interact with macOS UI. All testing must be done by the human user. When implementing features, Claude should ask the human to test functionality and report results.

The provided test audio files in `test/` directory enable development and testing of whisper functionality without requiring live recording.

## Key Technical Decisions

- **Pure C over Node.js**: Simpler, faster, single binary, no runtime dependencies
- **CMake build system**: Better IDE support, cleaner dependency management
- **whisper.cpp with Metal**: 2-10x faster GPU acceleration on Apple Silicon
- **Minimal Objective-C**: Only for system APIs that require it (clipboard, audio)
- **Library linking**: Link whisper.cpp as library for direct function calls
- **C-style C++**: Use C++ compilation for whisper.cpp integration but maintain C-style code conventions
