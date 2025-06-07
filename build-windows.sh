#!/bin/bash

set -e

echo "🔨 Cross-compiling Yakety for Windows on macOS..."

# Check if MinGW-w64 is installed
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "❌ MinGW-w64 not found. Please install it with: brew install mingw-w64"
    exit 1
fi

# Clone whisper.cpp if needed
if [ ! -d "whisper.cpp" ]; then
    echo "📥 Cloning whisper.cpp..."
    git clone https://github.com/ggerganov/whisper.cpp.git
fi

# Download model if needed
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

# Build whisper.cpp for Windows
echo "🔨 Building whisper.cpp for Windows..."
mkdir -p whisper.cpp/build-windows
cd whisper.cpp/build-windows

# Configure whisper.cpp for Windows cross-compilation
cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/toolchain-mingw64.cmake \
      -DBUILD_SHARED_LIBS=OFF \
      -DWHISPER_BUILD_TESTS=OFF \
      -DWHISPER_BUILD_EXAMPLES=OFF \
      -DWHISPER_NO_ACCELERATE=ON \
      -DWHISPER_NO_AVX=OFF \
      -DWHISPER_NO_AVX2=OFF \
      -DWHISPER_NO_FMA=OFF \
      -DWHISPER_NO_F16C=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -G Ninja ..

# Build whisper.cpp
ninja

cd ../..

# Build yakety with whisper.cpp support
echo "🔨 Building Yakety for Windows..."
mkdir -p build-windows
cd build-windows

# Configure with MinGW toolchain
echo "⚙️  Configuring CMake for Windows cross-compilation..."
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-mingw64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -G Ninja ..

# Build the project
echo "🔨 Compiling..."
ninja

echo "✅ Cross-compilation complete! Windows binaries:"
echo "  ./build-windows/yakety.exe         - CLI version"
echo "  ./build-windows/yakety-app.exe     - App version (with system tray)"
echo "  ./build-windows/recorder.exe       - Audio recording utility"
echo ""
echo "📝 These binaries include whisper.cpp support for transcription!"