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
