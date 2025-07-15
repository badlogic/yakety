#!/bin/bash

# Build script that demonstrates Whisper.cpp Ninja generator preference

set -e

# Parse command line arguments
CLEAN=false
PACKAGE=false
UPLOAD=false
HELP=false

for arg in "$@"; do
    case $arg in
        clean)
            CLEAN=true
            ;;
        package)
            PACKAGE=true
            ;;
        upload)
            UPLOAD=true
            ;;
        help|--help|-h)
            HELP=true
            ;;
        *)
            echo "Unknown argument: $arg"
            HELP=true
            ;;
    esac
done

if [ "$HELP" = true ]; then
    echo "Usage: $0 [clean] [package] [upload]"
    echo ""
    echo "Options:"
    echo "  clean    - Clean previous build directories"
    echo "  package  - Create distribution packages after building"
    echo "  upload   - Upload packages to server after building and packaging"
    echo ""
    echo "Examples:"
    echo "  $0                    # Just build"
    echo "  $0 clean             # Clean and build"
    echo "  $0 package           # Build and package"
    echo "  $0 clean package     # Clean, build, and package"
    echo "  $0 clean package upload  # Clean, build, package, and upload"
    exit 0
fi

echo "=== Yakety Build Script ==="
echo "This script builds Yakety, preferring Ninja generator for Whisper.cpp"
echo

# Clean previous build if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning previous build..."
    rm -rf build whisper.cpp/build
    echo "Clean complete"
    echo
fi

# Check if Ninja is available
if command -v ninja &> /dev/null; then
    echo "✓ Ninja found at: $(which ninja)"
else
    echo "⚠ Ninja not found. Whisper.cpp will use the default generator."
    echo "  Install Ninja for faster builds: brew install ninja (macOS) or apt-get install ninja-build (Linux)"
fi
echo

# Configure with release preset (uses Ninja by default)
echo "Configuring build..."
cmake --preset release

echo
echo "Building..."
cmake --build --preset release

echo
echo "✅ Build complete!"
echo "   CLI executable: ./build/bin/yakety-cli"
echo "   App bundle: ./build/bin/Yakety.app (macOS only)"

# Package if requested
if [ "$PACKAGE" = true ]; then
    echo
    echo "Creating distribution packages..."
    cmake --build build --target package
    echo "✅ Packaging complete!"
    echo "   Packages created in build directory"
fi

# Upload if requested (requires package)
if [ "$UPLOAD" = true ]; then
    if [ "$PACKAGE" = false ]; then
        echo
        echo "Creating distribution packages (required for upload)..."
        cmake --build build --target package
        echo "✅ Packaging complete!"
    fi
    echo
    echo "Uploading packages to server..."
    cmake --build build --target upload
    echo "✅ Upload complete!"
fi

# Show final summary
echo
if [ "$UPLOAD" = true ]; then
    echo "🚀 All done! Built, packaged, and uploaded successfully."
elif [ "$PACKAGE" = true ]; then
    echo "📦 All done! Built and packaged successfully."
    echo "   To upload packages, run: ./build.sh upload"
else
    echo "🔨 Build complete!"
    echo "   To create packages, run: ./build.sh package"
    echo "   To upload packages, run: ./build.sh package upload"
fi