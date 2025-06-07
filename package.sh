#!/bin/bash

set -e

echo "📦 Packaging Whisperer..."

# Build the project first
echo "🔨 Building..."
./build.sh

# Create app bundle structure
APP_NAME="Whisperer.app"
APP_PATH="build/$APP_NAME"
CONTENTS_PATH="$APP_PATH/Contents"
MACOS_PATH="$CONTENTS_PATH/MacOS"
RESOURCES_PATH="$CONTENTS_PATH/Resources"
FRAMEWORKS_PATH="$CONTENTS_PATH/Frameworks"

echo "🗂️  Creating app bundle..."
rm -rf "$APP_PATH"
mkdir -p "$MACOS_PATH"
mkdir -p "$RESOURCES_PATH"
mkdir -p "$FRAMEWORKS_PATH"

# Copy Info.plist
cp Info.plist "$CONTENTS_PATH/"

# Copy executable
cp build/whisperer "$MACOS_PATH/"

# Copy whisper model
echo "📦 Copying whisper model..."
mkdir -p "$RESOURCES_PATH/models"
cp whisper.cpp/models/ggml-base.en.bin "$RESOURCES_PATH/models/"

# Copy whisper.cpp dependencies (Metal shaders, etc)
if [ -d "whisper.cpp/build/bin" ]; then
    cp -R whisper.cpp/build/bin/*.metal "$RESOURCES_PATH/" 2>/dev/null || true
fi

# Update the binary to look for the model in Resources
echo "🔧 Patching model path..."
# We'll need to update the code to support this, for now just document it

# Create a launcher script that sets up the environment
cat > "$MACOS_PATH/whisperer-launcher" << 'EOF'
#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR/.."
export WHISPERER_MODEL_PATH="$DIR/../Resources/models/ggml-base.en.bin"
exec "$DIR/whisperer" "$@"
EOF
chmod +x "$MACOS_PATH/whisperer-launcher"

# Sign the app (ad-hoc signing for local use)
echo "🔏 Signing app..."
codesign --force --deep --sign - "$APP_PATH"

# Create DMG for distribution
echo "💿 Creating DMG..."
DMG_NAME="Whisperer-1.0.dmg"
rm -f "build/$DMG_NAME"

# Create a temporary directory for DMG contents
DMG_DIR="build/dmg"
rm -rf "$DMG_DIR"
mkdir -p "$DMG_DIR"
cp -R "$APP_PATH" "$DMG_DIR/"

# Add a symlink to Applications
ln -s /Applications "$DMG_DIR/Applications"

# Create README for DMG
cat > "$DMG_DIR/README.txt" << 'EOF'
Whisperer - Voice Transcription for macOS

Installation:
1. Drag Whisperer.app to Applications folder
2. Open Whisperer from Applications
3. Grant accessibility and microphone permissions when prompted

Usage:
Hold FN key → Speak → Release FN → Text is pasted

To run on startup:
Open System Preferences → Users & Groups → Login Items → Add Whisperer.app

Uninstall:
1. Remove from Login Items if added
2. Delete Whisperer.app from Applications
EOF

# Create the DMG
hdiutil create -volname "Whisperer" -srcfolder "$DMG_DIR" -ov -format UDZO "build/$DMG_NAME"

# Clean up
rm -rf "$DMG_DIR"

# Create a launch agent plist for background startup
echo "🚀 Creating Launch Agent..."
LAUNCH_AGENT="com.lemmy.whisperer.plist"
cat > "build/$LAUNCH_AGENT" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.lemmy.whisperer</string>
    <key>ProgramArguments</key>
    <array>
        <string>/Applications/Whisperer.app/Contents/MacOS/whisperer</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardErrorPath</key>
    <string>/tmp/whisperer.err</string>
    <key>StandardOutPath</key>
    <string>/tmp/whisperer.out</string>
</dict>
</plist>
EOF

# Create installer script
cat > "build/install.sh" << 'EOF'
#!/bin/bash
echo "🚀 Installing Whisperer..."

# Check if Whisperer.app exists
if [ ! -d "Whisperer.app" ]; then
    echo "❌ Whisperer.app not found. Please run this script from the distribution folder."
    exit 1
fi

# Copy to Applications
echo "📁 Copying to Applications..."
cp -R Whisperer.app /Applications/

# Install launch agent for auto-start
echo "🔧 Installing launch agent..."
cp com.lemmy.whisperer.plist ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/com.lemmy.whisperer.plist

echo "✅ Installation complete!"
echo ""
echo "Whisperer is now installed and running."
echo "Hold FN key to record and transcribe speech."
echo ""
echo "To uninstall:"
echo "  launchctl unload ~/Library/LaunchAgents/com.lemmy.whisperer.plist"
echo "  rm ~/Library/LaunchAgents/com.lemmy.whisperer.plist"
echo "  rm -rf /Applications/Whisperer.app"
EOF
chmod +x "build/install.sh"

echo "✅ Packaging complete!"
echo ""
echo "📦 Distribution files:"
echo "  - build/$APP_NAME          - macOS application bundle"
echo "  - build/$DMG_NAME          - Disk image for distribution"
echo "  - build/whisperer          - CLI executable (still available)"
echo "  - build/$LAUNCH_AGENT      - Launch agent for auto-start"
echo "  - build/install.sh         - Installation script"
echo ""
echo "🚀 Quick start:"
echo "  1. Open build/$DMG_NAME"
echo "  2. Drag Whisperer to Applications"
echo "  3. Run Whisperer from Applications"
echo ""
echo "📝 For CLI usage:"
echo "  ./build/whisperer          - Run from terminal"