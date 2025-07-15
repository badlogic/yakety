# Additional Key Cancellation
**Status:** Done
**Agent PID:** 81906

## Original Todo
If the user pressed key combination but then also pressed additional keys, we should hide recording overlay, and ignore this recording. Ideally we can stop the recording too.

## Description
Implement additional key cancellation functionality with precise key state tracking. The system must distinguish between valid hotkey releases and cancellation scenarios, ensuring clean state management.

**Key Logic:**
- **Valid Release**: If all pressed keys are part of the key combo and one combo key is released → send release callback, stop detecting until no keys pressed
- **Cancellation**: If all pressed keys are part of key combo and an additional non-combo key is pressed → send cancel callback, stop detecting until no keys pressed

## Implementation Plan
1. **Add cancellation callback and state tracking to keylogger interface**
   - [x] Add `on_key_cancel` callback parameter to `keylogger_init()` in `src/keylogger.h`
   - [x] Add state enum to track: IDLE, COMBO_ACTIVE, WAITING_FOR_ALL_RELEASED
   - [x] Update platform-specific keylogger implementations to support cancellation callback

2. **Implement precise key state tracking in macOS keylogger**
   - [x] Add pressed keys tracking array to monitor all currently pressed keys in `src/mac/keylogger.c`
   - [x] Modify `CGEventCallback()` to track key presses/releases and classify keys as combo/non-combo
   - [x] Implement logic: combo key released while only combo keys pressed → send release callback
   - [x] Implement logic: non-combo key pressed while combo active → send cancel callback
   - [x] Add "wait for all keys released" state before accepting new hotkey combinations

3. **Implement precise key state tracking in Windows keylogger**
   - [x] Add pressed keys tracking array to monitor all currently pressed keys in `src/windows/keylogger.c`
   - [x] Modify `LowLevelKeyboardProc()` to track key presses/releases and classify keys as combo/non-combo
   - [x] Implement logic: combo key released while only combo keys pressed → send release callback
   - [x] Implement logic: non-combo key pressed while combo active → send cancel callback
   - [x] Add "wait for all keys released" state before accepting new hotkey combinations

4. **Add cancellation handler in main application**
   - [x] Implement `on_key_cancel()` function in `src/main.c` that stops recording, hides overlay, and cleans up audio
   - [x] Register cancellation callback when initializing keylogger

5. **Update recording state management**
   - [x] Add `cancelled` flag to `AppState` struct to distinguish cancelled from completed recordings
   - [x] Ensure cancelled recordings don't trigger transcription or text insertion

6. **Test precise key state handling**
   - [x] Manual test: FN → release FN (normal case) → verify release callback and transcription
   - [x] Manual test: FN → L → verify cancel callback, no transcription, overlay hidden
   - [x] Manual test: FN → L → release L → release FN → verify no additional callbacks until all keys released
   - [x] Manual test: After cancellation, verify system waits for all keys released before accepting new combos

## Notes
**State Machine Logic:**
- IDLE: No keys pressed, ready to detect combo
- COMBO_ACTIVE: Combo keys pressed, recording active
- WAITING_FOR_ALL_RELEASED: After release/cancel, wait for all keys up before returning to IDLE

**Key Scenarios:**
- FN → release FN: Release callback → WAITING_FOR_ALL_RELEASED → IDLE
- FN → L: Cancel callback → WAITING_FOR_ALL_RELEASED → (wait for FN+L release) → IDLE
- FN → L → release L → release FN: Cancel on L, ignore subsequent releases until all up