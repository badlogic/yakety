# Keyboard Handling Research Report

## 1. Where keyboard events are captured for the hotkey functionality

### Global Structure
The keyboard handling in Yakety is platform-specific and uses a global keyboard hook system:

**macOS Implementation** (`/Users/badlogic/workspaces/yakety/src/mac/keylogger.c`):
- Uses `CGEventTapCreate()` to create a global event tap
- Monitors `kCGEventKeyDown`, `kCGEventKeyUp`, and `kCGEventFlagsChanged` events
- Event tap is created with `kCGSessionEventTap` and `kCGHeadInsertEventTap` for system-wide capture
- Requires accessibility permissions to function

**Windows Implementation** (`/Users/badlogic/workspaces/yakety/src/windows/keylogger.c`):
- Uses `SetWindowsHookEx()` with `WH_KEYBOARD_LL` for low-level keyboard hook
- Monitors `WM_KEYDOWN`, `WM_KEYUP`, `WM_SYSKEYDOWN`, and `WM_SYSKEYUP` messages
- Tracks multiple keys simultaneously in a pressed keys array (up to 4 keys)

### Key Combination Detection
- **Default hotkeys**: FN key (macOS) or Right Ctrl (Windows)
- **Customizable**: Users can set custom key combinations via preferences
- **Storage**: Key combinations are saved/loaded via `preferences_save_key_combination()` and `preferences_load_key_combination()`

### Event Flow
1. **Initialization**: `keylogger_init()` called from `main.c:setup_keylogger()`
2. **Callbacks**: Two callback functions registered:
   - `on_key_press()` - triggers when hotkey combination is pressed
   - `on_key_release()` - triggers when hotkey combination is released
3. **State management**: Global `AppState` tracks recording status

## 2. How the recording overlay is shown/hidden

### Overlay System
**Header**: `/Users/badlogic/workspaces/yakety/src/overlay.h`
**macOS Implementation**: `/Users/badlogic/workspaces/yakety/src/mac/overlay.m`

### Key Functions:
- `overlay_init()` - Creates floating window with dark background, green border, and microphone icon
- `overlay_show(message)` - Shows overlay with green styling (normal recording)
- `overlay_show_error(message)` - Shows overlay with red styling (error states)
- `overlay_hide()` - Hides overlay with fade animation

### Overlay Properties:
- **Position**: Center-bottom of main screen (30px from bottom)
- **Styling**: Dark background (85% opacity), rounded corners, animated fade in/out
- **Content**: Microphone icon + status text
- **Window Level**: `NSFloatingWindowLevel` (always on top)
- **Behavior**: Ignores mouse events, appears on all spaces

### Current Usage in Recording Flow:
- `overlay_show("Recording")` - shown when recording starts
- `overlay_show("Transcribing")` - shown during transcription
- `overlay_hide()` - hidden when recording stops or fails

## 3. Where recording is started and stopped

### Recording Control Flow (`/Users/badlogic/workspaces/yakety/src/main.c`)

**Recording Start** (`on_key_press()` - lines 224-238):
```c
static void on_key_press(void *userdata) {
    AppState *state = (AppState *) userdata;
    
    if (!state->recording) {
        state->recording = true;
        state->recording_start_time = utils_get_time();
        
        if (audio_recorder_start() == 0) {
            overlay_show("Recording");
        } else {
            log_error("Failed to start recording");
            state->recording = false;
        }
    }
}
```

**Recording Stop** (`on_key_release()` - lines 240-258):
```c
static void on_key_release(void *userdata) {
    AppState *state = (AppState *) userdata;
    
    if (state->recording) {
        state->recording = false;
        double duration = utils_get_time() - state->recording_start_time;
        
        // Minimum recording duration check
        if (duration < MIN_RECORDING_DURATION) {
            log_info("Recording too brief (%.2f seconds), ignoring", duration);
            audio_recorder_stop();
            overlay_hide();
            return;
        }
        
        // Process the recorded audio
        process_recorded_audio(duration);
    }
}
```

### Audio Recording System (`/Users/badlogic/workspaces/yakety/src/audio.c`)
- Uses MiniAudio library for cross-platform audio capture
- Fixed to Whisper-compatible format: 16kHz mono
- Records to memory buffer (not file) for real-time processing
- Functions: `audio_recorder_start()`, `audio_recorder_stop()`, `audio_recorder_get_samples()`

## 4. How additional key presses during recording are currently handled

### Current Behavior: **NO CANCELLATION HANDLING**

**Current State**: The system currently does **NOT** handle additional key presses during recording. Once recording starts, it will continue until the original hotkey combination is released, regardless of any other keys pressed.

**Key Findings**:

1. **macOS**: The `matches_target_combination()` function only checks if the exact target combination is active. Additional keys don't interfere with this check.

2. **Windows**: The system tracks all pressed keys but only triggers release when a key from the original combination is released. Additional keys are tracked but ignored.

3. **No Cancellation Logic**: There's no code path that checks for additional key presses during recording to cancel the operation.

4. **State Management**: The `AppState.recording` flag is only set to `false` when the original hotkey is released, not when additional keys are pressed.

### Implementation Opportunity
The task.md file indicates this is exactly what needs to be implemented:
> "If the user pressed key combination but then also pressed additional keys, we should hide recording overlay, and ignore this recording. Ideally we can stop the recording too."

### Suggested Implementation Points:
1. **Detection**: Add logic to detect when additional keys are pressed during recording
2. **Cancellation**: Cancel recording when additional keys are detected
3. **UI Feedback**: Hide overlay immediately when cancellation occurs
4. **Audio Cleanup**: Stop audio recording and discard buffer

This research provides the foundation for implementing the additional key cancellation feature. The current architecture is well-structured to support this enhancement by adding cancellation logic to the existing keyboard event handling system.

# Recording State Management in Yakety

## 1. Recording State Tracking

### Primary State Variables

**Location:** `/Users/badlogic/workspaces/yakety/todos/worktrees/2025-07-15-18-28-42-additional-key-cancellation/src/main.c` (lines 28-31)

```c
typedef struct {
    bool recording;
    double recording_start_time;
} AppState;

static AppState *g_state = NULL;
```

**Location:** `/Users/badlogic/workspaces/yakety/todos/worktrees/2025-07-15-18-28-42-additional-key-cancellation/src/audio.c` (lines 24-26)

```c
typedef struct {
    // State - accessed from both audio and main threads
    bool is_recording;      // Atomic access required
    bool is_file_recording; // Atomic access required
    // ... other fields
} AudioRecorder;
```

### State Transitions

Recording state transitions are managed through:

1. **Start Recording** (`on_key_press` in `main.c:224-238`):
   - Sets `state->recording = true`
   - Records `state->recording_start_time = utils_get_time()`
   - Calls `audio_recorder_start()` which sets atomic `is_recording = true`
   - Shows "Recording" overlay

2. **Stop Recording** (`on_key_release` in `main.c:240-258`):
   - Sets `state->recording = false`
   - Calculates duration
   - Checks minimum recording duration (0.1 seconds)
   - Processes recorded audio or cancels if too short

3. **Audio Level State** (`audio.c:155-175` for memory recording):
   - Uses atomic operations via `utils_atomic_read_bool()` and `utils_atomic_write_bool()`
   - Manages `is_recording` and `is_file_recording` flags

## 2. Recording Overlay Control

### Overlay Management

**Location:** `/Users/badlogic/workspaces/yakety/todos/worktrees/2025-07-15-18-28-42-additional-key-cancellation/src/mac/overlay.m`

**Key Functions:**
- `overlay_show(const char *message)` - Shows overlay with green border
- `overlay_show_error(const char *message)` - Shows overlay with red border  
- `overlay_hide()` - Hides overlay with fade animation
- `overlay_init()` - Initializes overlay window system

**State Variables:**
```objc
static NSWindow *overlayWindow = nil;
static NSTextField *messageLabel = nil;
static NSImageView *iconView = nil;
static CALayer *borderLayer = nil;
static NSColor *g_currentTintColor = nil;
```

### Overlay Usage in Recording Flow

1. **Recording Start:** `overlay_show("Recording")` (line 232 in main.c)
2. **Transcription:** `overlay_show("Transcribing")` (line 192 in main.c)
3. **Error States:** `overlay_show_error(message)` available for error conditions
4. **Recording End:** `overlay_hide()` (line 197 in main.c)

## 3. Recording Cancellation Detection

### Current Key Detection System

**Location:** `/Users/badlogic/workspaces/yakety/todos/worktrees/2025-07-15-18-28-42-additional-key-cancellation/src/mac/keylogger.c`

**Key Components:**
- `matches_target_combination()` (lines 20-55) - Checks if current event matches target hotkey
- `CGEventCallback()` (lines 58-113) - Main event handler for key presses
- Global state variables:
  ```c
  static bool keyPressed = false;
  static bool isPaused = false;
  static KeyCombination g_target_combo = {0, kCGEventFlagMaskSecondaryFn};
  ```

### Current Limitations
The current system only detects the specific hotkey combination (FN key by default) and doesn't detect additional key presses that should cancel recording.

## 4. Recording Interruption Handling

### Minimum Duration Check
**Location:** `main.c:248-253`
```c
// Minimum recording duration check
if (duration < MIN_RECORDING_DURATION) {
    log_info("⚠️  Recording too brief (%.2f seconds), ignoring", duration);
    audio_recorder_stop();
    overlay_hide();
    return;
}
```

### Audio System Interruption
The audio system (via miniaudio) has built-in interruption handling:
- **Location:** `miniaudio.h` contains interruption notification types
- Audio device can be stopped/restarted on system interruptions
- The `audio_recorder_stop()` function cleanly stops recording

### Signal Handling
**Location:** `main.c:39-42`
```c
static void signal_handler(int sig) {
    (void) sig;
    app_quit();
}
```

## 5. What Happens During Recording Interruption

### Clean Shutdown Process
1. **Audio Recording Stop:** `audio_recorder_stop()` stops the audio device and cleans up
2. **Overlay Hide:** `overlay_hide()` fades out the recording indicator
3. **State Reset:** Recording flags are set to false
4. **Buffer Cleanup:** Audio buffer is preserved for potential use or freed

### Recovery Mechanisms
- The keylogger can be paused/resumed via `keylogger_pause()` and `keylogger_resume()`
- Event tap can be re-enabled if disabled: `CGEventTapEnable(eventTap, true)`
- Audio device can be restarted for new recordings

## Key Findings for Additional Key Cancellation

Based on this analysis, to implement additional key cancellation, you would need to:

1. **Modify the keylogger** to detect any additional key presses during recording
2. **Add cancellation logic** in the event callback to stop recording when additional keys are detected
3. **Update the overlay** to show cancellation state (possibly with red styling)
4. **Enhance state management** to handle cancelled recordings differently from completed ones

The current architecture already supports the necessary components - the main addition would be expanding the key detection logic in `keylogger.c` to monitor for additional keypresses beyond the target combination.