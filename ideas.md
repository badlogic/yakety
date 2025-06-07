# Yakety Ideas

## Features

- [ ] **Model Downloads & Management**
  - Add "Select Model..." menu item to tray icon
  - Simple UI for downloading additional whisper models
  - Set default model (ships with base.en by default)
  - Models stored in ~/.yakety/
  - Support for custom user-provided models (gguf format)
  - Auto-discovery of models in ~/.yakety/
  - Store selected model in ~/.yakety/config.ini
  - On startup: read config, verify model exists, fallback to base.en if needed
  - Update config.ini when model selection changes

- [ ] **Automatic Submission Word**
  - User can set a trigger word (e.g., "send", "submit", "enter")
  - When detected at end of transcription, automatically send Enter key
  - Useful for chat apps like Claude - no need to manually hit Enter
  - Configurable through UI (settings dialog or menu)
  - Store trigger word in ~/.yakety/config.ini
  - Option to enable/disable this feature

- [ ] **Custom Transcription Processing**
  - User can set external command to process transcriptions
  - Command receives transcription text via stdin
  - Command returns modified text via stdout
  - Return empty string to prevent pasting
  - Simple stdio communication
  - Configurable through UI (text field for command/binary path + arguments)
  - Store command in ~/.yakety/config.ini
  - Use case: custom text formatting, replacements, AI processing, etc.

## Bugs

- [ ] **Fix Transcription Buffer Overflow**
  - Currently passing fixed 1024 char array to transcription
  - Overflow breaks recording (hotkey stops working)
  - Transcription method should return char* or NULL on error
  - Implement internal dynamic buffer that resizes as needed
  - Reuse buffer across transcription calls for efficiency