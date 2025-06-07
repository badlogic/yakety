#!/bin/bash

set -e

echo "🔨 Building Whisperer..."

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
    cd whisper.cpp
    bash ./models/download-ggml-model.sh base.en
    cd ..
fi

# Now build main whisperer application
cd build

# Run CMake configuration with Ninja generator
echo "⚙️  Configuring CMake..."
cmake -G Ninja ..

# Build the project
echo "🔨 Compiling..."
ninja

echo "✅ Build complete! Binaries:"
echo "  ./build/whisperer         - Main FN key audio transcription app"
echo "  ./build/recorder          - Audio recording utility"
echo "  ./build/test_transcription - Test whisper.cpp transcription"
echo ""
echo "To run:"
echo "  ./build/whisperer                        # Start FN key transcription"
echo "  ./build/recorder output.wav              # Record audio to file"
echo "  ./build/test_transcription test/test1.wav # Test transcription"
echo ""
echo "⚠️  Note: This application requires accessibility permissions to monitor keyboard events."
echo "   Go to System Preferences → Security & Privacy → Privacy → Accessibility"
echo "   and add your terminal application to the list."