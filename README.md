# Yakety

Voice-to-text input for any application. Hold a hotkey to record, release to transcribe and paste.

## Quick Start

```bash
git clone https://github.com/badlogic/yakety.git
cd yakety
cmake --preset release
cmake --build --preset release
```

**macOS:** Open `build/bin/Yakety.app`
**Windows:** Run `build\bin\Yakety.exe`

## Usage

The app runs in your system tray. Default hotkey:
- **macOS:** Fn key
- **Windows:** Right Ctrl

Hold to record, release to transcribe and paste.

## Requirements

- **macOS:** 10.15+, Xcode Command Line Tools
- **Windows:** Windows 10+, Visual Studio 2019+
- **Both:** CMake 3.20+, Ninja (`brew install ninja` / `choco install ninja`)

## Configuration

Config file location:
- **macOS:** `~/.yakety/config.ini`
- **Windows:** `%APPDATA%\Yakety\config.ini`

```ini
[yakety]
# Custom model path (optional)
model = /path/to/model.bin

# Launch at startup
launch_at_login = true

# Hotkey configuration (usually set via GUI)
# Format: KeyCombo = <keycode>:<flags> (hex values)
# macOS Fn key (default): KeyCombo = 0:800000
# Windows Right Ctrl (default): KeyCombo = 1D:1
KeyCombo = 0:800000
```

## Permissions

- **macOS:** Grand microphone permissions, grant accessibility permissions in System Preferences → Security & Privacy → Privacy → Accessibility
- **Windows:** Run as administrator if needed for hotkeys in elevated apps

## License

Proprietary © 2025 Mario Zechner

---

**Architecture:** [ARCHITECTURE.md](ARCHITECTURE.md)
**Remote Development:** [wsl/REMOTE_DEVELOPMENT.md](wsl/REMOTE_DEVELOPMENT.md)