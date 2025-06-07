#!/bin/bash

set -e

echo "🔨 Building Yakety..."

# Generate icons if SVG exists and tools are available
if [ -f "assets/yakety.svg" ] && command -v rsvg-convert &> /dev/null && command -v iconutil &> /dev/null; then
    echo "🎨 Generating icons from SVG..."
    ./generate-icons.sh
elif [ -f "assets/yakety.svg" ]; then
    echo "⚠️  SVG found but icon generation tools not installed."
    echo "   Install with: brew install librsvg"
    echo "   Continuing without icon generation..."
fi

# Create build directory
mkdir -p build

# Clone and build whisper.cpp if not exists
if [ ! -d "whisper.cpp" ]; then
    echo "📥 Cloning whisper.cpp..."
    git clone https://github.com/ggerganov/whisper.cpp.git
fi

# Build whisper.cpp with Metal support
echo "🔨 Building whisper.cpp with Metal support..."
cd whisper.cpp
mkdir -p build
cd build

# Configure whisper.cpp with Metal support and static libraries
cmake -G Ninja .. -DGGML_METAL=1 -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release

# Build whisper.cpp
ninja

cd ../../

# Download base.en model if not exists
MODEL_DIR="whisper.cpp/models"
MODEL_FILE="$MODEL_DIR/ggml-base.en.bin"

if [ ! -f "$MODEL_FILE" ]; then
    echo "📥 Downloading base.en model (~150MB)..."
    mkdir -p "$MODEL_DIR"
    
    # Direct download since the script location may have changed
    echo "   Downloading from Hugging Face..."
    curl -L -o "$MODEL_FILE" "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin"
    
    if [ ! -f "$MODEL_FILE" ]; then
        echo "❌ Failed to download model"
        exit 1
    fi
    echo "✅ Model downloaded successfully"
fi

# Now build main yakety application
cd build

# Run CMake configuration with Ninja generator
echo "⚙️  Configuring CMake..."
cmake -G Ninja ..

# Build the project
echo "🔨 Compiling..."
ninja

echo "✅ Build complete!"

# Go back to project root
cd ..

# Bundle dynamic libraries for distribution
if [ -d "./build/yakety-app.app" ]; then
    echo "📦 Bundling dynamic libraries..."
    mkdir -p "./build/yakety-app.app/Contents/Frameworks"
    
    # Copy all dynamic libraries
    find ./whisper.cpp/build -name "*.dylib" -type f | while read lib; do
        cp "$lib" "./build/yakety-app.app/Contents/Frameworks/"
    done
    
    # Create symlinks for versioned libraries
    cd "./build/yakety-app.app/Contents/Frameworks"
    if [ -f "libwhisper.1.7.5.dylib" ]; then
        ln -sf libwhisper.1.7.5.dylib libwhisper.1.dylib
        ln -sf libwhisper.1.7.5.dylib libwhisper.dylib
    fi
    cd - > /dev/null
    
    # Add rpath to find libraries
    install_name_tool -add_rpath @executable_path/../Frameworks "./build/yakety-app.app/Contents/MacOS/yakety-app" 2>/dev/null || true
    
    echo "✅ Libraries bundled"
fi

# Sign the app bundle to avoid "damaged" error
if [ -d "./build/yakety-app.app" ]; then
    echo "🔏 Signing app bundle..."
    codesign --force --deep --sign - "./build/yakety-app.app"
    xattr -cr "./build/yakety-app.app"
    echo "✅ App signed with ad-hoc signature"
else
    echo "⚠️  No app bundle found to sign"
fi

echo ""
echo "Binaries:"
echo "  ./build/yakety             - CLI version (no tray icon)"
echo "  ./build/yakety-app.app     - macOS app bundle (with tray icon)"
echo "  ./build/recorder           - Audio recording utility"
echo "  ./build/test_transcription - Test whisper.cpp transcription"
echo ""
echo "To run:"
echo "  ./build/yakety                           # CLI version - runs in terminal"
echo "  open ./build/yakety-app.app              # App version - runs with tray icon"
echo "  ./build/recorder output.wav              # Record audio to file"
echo "  ./build/test_transcription test/test1.wav # Test transcription"
echo ""
echo "⚠️  Note: This application requires accessibility permissions to monitor keyboard events."
echo "   Go to System Preferences → Security & Privacy → Privacy → Accessibility"
echo "   and add your terminal application (for CLI) or yakety-app (for app version) to the list."