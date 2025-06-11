# Remote Development with Claude Code

This guide explains how to develop Yakety on Windows from a remotely connected macOS machine using Claude Code.

## Overview

You can use Claude Code running on macOS to:
- Edit code locally with fast, native tools
- Sync changes to your Windows development environment
- Build and test on Windows via SSH
- Debug and troubleshoot cross-platform issues

## Prerequisites

1. **Windows machine** with WSL2 and development tools
2. **macOS machine** with Claude Code
3. **Network connectivity** between the machines
4. **SSH access** configured from macOS to Windows WSL

## Setup Process

### 1. Configure WSL SSH Access

On your Windows machine:

1. **Run the setup script** (one-time):
   ```bash
   cd /mnt/c/workspaces/yakety
   ./setup-wsl-ssh.sh
   ```

2. **Start SSH forwarding** (after each reboot):
   - Right-click `start-wsl-ssh.bat` 
   - Select "Run as administrator"
   - Note the displayed Windows IP address

### 2. Configure Claude Code

Tell Claude Code about your Windows environment:

```
My Windows machine is accessible at 192.168.1.21 via SSH using username 'badlogic'.
The Yakety workspace is at: /mnt/c/workspaces/yakety/
I can run Windows commands using: ssh badlogic@192.168.1.21 "cmd.exe /c command"
```

## Development Workflow

### 1. Edit Code Locally
- Work on the macOS copy of the repository
- Use native macOS tools for fast editing
- Take advantage of Claude Code's local file access

### 2. Sync Changes to Windows
```bash
# Sync source files only (excludes build artifacts)
rsync -av --exclude='build/' --exclude='whisper.cpp/' \
  src/ cmake/ CMakeLists.txt CMakePresets.json assets/ \
  badlogic@192.168.1.21:/mnt/c/workspaces/yakety/
```

### 3. Build on Windows
```bash
# Build via SSH
ssh badlogic@192.168.1.21 "cd /mnt/c/workspaces/yakety && cmd.exe /c 'c:\workspaces\winvs.bat' cmake --build --preset release"
```

### 4. Test and Debug
```bash
# Run tests
ssh badlogic@192.168.1.21 "cd /mnt/c/workspaces/yakety && build/bin/yakety-cli.exe"

# Check logs
ssh badlogic@192.168.1.21 "cat /mnt/c/Users/badlogic/.yakety/log.txt"
```

## Claude Code Integration

Claude Code can execute all these operations automatically. Example prompts:

### Basic Development
```
"Please sync my local changes to Windows and build the release version."
```

### Cross-Platform Testing
```
"Build on both macOS and Windows, then compare the outputs."
```

### Debugging Issues
```
"The Windows build is failing. Please sync changes, attempt the build, 
and analyze any error messages."
```

### File Operations
```
"Read the Windows log file and check for any keyboard hook errors."
```

## Advanced Operations

### Reading Windows Files
```bash
ssh badlogic@192.168.1.21 "cat /mnt/c/workspaces/yakety/src/windows/app.c"
```

### Writing Windows Files
```bash
echo "content" | ssh badlogic@192.168.1.21 "cat > /mnt/c/workspaces/yakety/test.txt"
```

### Running Windows Tools
```bash
ssh badlogic@192.168.1.21 "cmd.exe /c 'dir c:\workspaces\yakety'"
```

## Benefits

1. **Best of Both Worlds**: Native macOS development speed + Windows testing
2. **Unified Workflow**: Single Claude Code session manages both platforms  
3. **Real-time Sync**: Changes propagate immediately to Windows
4. **Automated Operations**: Claude Code handles complex multi-step workflows
5. **Cross-Platform Verification**: Ensure changes work on both platforms

## Troubleshooting

### SSH Connection Issues
- Restart `start-wsl-ssh.bat` as administrator
- Check Windows firewall settings
- Verify WSL is running: `wsl --list --running`

### Build Issues
- Ensure `winvs.bat` sets up the MSVC environment correctly
- Check that CMake presets are synchronized
- Verify whisper.cpp dependencies are available

### File Sync Issues
- Use `--dry-run` flag with rsync to preview changes
- Check file permissions in WSL
- Ensure paths use forward slashes in WSL context

## Security Notes

- SSH keys are recommended over password authentication
- Consider using VPN for connections outside local network
- Regularly update WSL and Windows security patches
- Limit SSH access to specific IP ranges if possible

## Example Session

A typical Claude Code development session:

1. **Edit code** on macOS using local tools
2. **"Sync changes to Windows and build"** → Claude Code executes rsync + build
3. **"Test the CLI version"** → Claude Code runs Windows executable via SSH  
4. **"Check the logs for errors"** → Claude Code reads Windows log files
5. **"Fix the issue and rebuild"** → Claude Code makes changes and rebuilds
6. **"Commit the working version"** → Claude Code commits from macOS

This workflow provides seamless cross-platform development with the power and convenience of Claude Code automation.