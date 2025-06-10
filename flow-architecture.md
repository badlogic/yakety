# Yakety Application Flow Diagrams

This directory contains flow diagrams for the Yakety voice-to-text application showing the execution flow for both CLI and tray app versions.

## Files

- `flow-tray.mermaid` - Tray application flow with main thread vs auxiliary threads
- `flow-cli.mermaid` - CLI application flow with main thread vs auxiliary threads

## Viewing the Diagrams

These Mermaid diagrams can be viewed in several ways:

### Online Viewers
- [Mermaid Live Editor](https://mermaid.live/)
- [GitHub](https://github.com) (automatically renders .mermaid files)

### VS Code
- Install the "Mermaid Markdown Syntax Highlighting" extension
- Install the "Markdown Preview Mermaid Support" extension

### Command Line
```bash
# Install mermaid-cli
npm install -g @mermaid-js/mermaid-cli

# Generate PNG images
mmdc -i flow-tray.mermaid -o flow-tray.png
mmdc -i flow-cli.mermaid -o flow-cli.png

# Generate SVG images  
mmdc -i flow-tray.mermaid -o flow-tray.svg
mmdc -i flow-cli.mermaid -o flow-cli.svg
```

## Key Architecture Notes

### Thread Separation
- **Main Thread**: UI events, app lifecycle, coordination
- **Auxiliary Threads**: CPU-intensive tasks (model loading, transcription)
- **System Events**: External inputs (keyboard, signals, menu clicks)

### Common Flow (Both Apps)
1. Initialize app components
2. Load Whisper model asynchronously 
3. Set up keylogger with hotkey monitoring
4. Main loop: hotkey press → record → transcribe → paste

### Key Differences
- **Tray App**: Menu system, GUI dialogs, first-run setup
- **CLI App**: Command-line parsing, console-only output, simpler error handling

### Async Operations
- Model loading happens on background thread to avoid blocking UI
- Transcription processing runs on separate thread
- Main thread coordinates and handles UI updates