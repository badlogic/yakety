#!/bin/bash

# Build script that demonstrates Whisper.cpp Ninja generator preference

set -e

# Signing configuration
DISTRIBUTION_CERT="Developer ID Application: Mario Zechner (7F5Y92G2Z4)"
TEAM_ID="7F5Y92G2Z4"
APPLE_ID="contact@badlogicgames.com"

# Parse command line arguments
CLEAN=false
PACKAGE=false
UPLOAD=false
DEBUG=false
HELP=false
NOTARIZE=false

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
        debug)
            DEBUG=true
            ;;
        notarize)
            NOTARIZE=true
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
    echo "Usage: $0 [clean] [debug] [package] [upload] [notarize]"
    echo ""
    echo "Options:"
    echo "  clean    - Clean previous build directories"
    echo "  debug    - Use debug preset instead of release"
    echo "  package  - Create distribution packages (includes notarization for release builds)"
    echo "  upload   - Upload packages to server (includes notarization and packaging)"
    echo "  notarize - Build, sign, and notarize for distribution (macOS only)"
    echo ""
    echo "Examples:"
    echo "  $0                    # Just build (release)"
    echo "  $0 debug             # Build with debug preset"
    echo "  $0 clean             # Clean and build"
    echo "  $0 clean debug       # Clean and build with debug preset"
    echo "  $0 package           # Build and package"
    echo "  $0 clean package     # Clean, build, and package"
    echo "  $0 clean package upload  # Clean, build, package, and upload"
    echo "  $0 notarize          # Build, sign, and notarize for distribution"
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

# Configure with appropriate preset
if [ "$DEBUG" = true ]; then
    echo "Configuring debug build..."
    cmake --preset debug
    echo
    echo "Building..."
    cmake --build --preset debug
else
    echo "Configuring release build..."
    # If we're going to notarize, skip ad-hoc signing
    if [ "$NOTARIZE" = true ] || [ "$PACKAGE" = true ] || [ "$UPLOAD" = true ]; then
        cmake --preset release -DSKIP_ADHOC_SIGNING=ON
    else
        cmake --preset release
    fi
    echo
    echo "Building..."
    cmake --build --preset release
fi

echo
echo "✅ Build complete!"
if [ "$DEBUG" = true ]; then
    echo "   CLI executable: ./build-debug/bin/yakety-cli"
    echo "   App bundle: ./build-debug/bin/Yakety.app (macOS only)"
else
    echo "   CLI executable: ./build/bin/yakety-cli"
    echo "   App bundle: ./build/bin/Yakety.app (macOS only)"
fi

# Determine if notarization is needed
if [ "$PACKAGE" = true ] || [ "$UPLOAD" = true ]; then
    # For release builds, notarize before packaging
    if [ "$DEBUG" = false ] && [ "$NOTARIZE" = false ]; then
        NOTARIZE=true
    fi
fi

# Notarize if requested (must happen before packaging/uploading)
if [ "$NOTARIZE" = true ]; then
    echo
    echo "🔏 Starting notarization process..."
    
    # Check prerequisites
    if [ -z "$NOTARY_TOOL_PASSWORD" ]; then
        echo "❌ NOTARY_TOOL_PASSWORD not set"
        echo "💡 Add 'export NOTARY_TOOL_PASSWORD=\"your-app-specific-password\"' to ~/.zshrc"
        exit 1
    fi
    
    # Ensure we have a release build
    if [ "$DEBUG" = true ]; then
        echo "❌ Cannot notarize debug builds. Use release build."
        exit 1
    fi
    
    BUILD_DIR="build"
    
    # Check if CLI is already properly signed
    NEEDS_CLI_SIGNING=true
    if codesign -vv "$BUILD_DIR/bin/yakety-cli" 2>&1 | grep -q "Developer ID Application: Mario Zechner"; then
        echo "✅ yakety-cli already properly signed"
        NEEDS_CLI_SIGNING=false
    fi
    
    # Sign the CLI executable if needed
    if [ "$NEEDS_CLI_SIGNING" = true ]; then
        echo "📝 Signing yakety-cli executable..."
        codesign --force --sign "$DISTRIBUTION_CERT" \
            --options runtime \
            --timestamp \
            "$BUILD_DIR/bin/yakety-cli"
        
        if [ $? -ne 0 ]; then
            echo "❌ Code signing yakety-cli failed"
            exit 1
        fi
    fi
    
    # Check if app bundle is already properly signed
    NEEDS_APP_SIGNING=true
    if [ -d "$BUILD_DIR/bin/Yakety.app" ]; then
        if codesign -vv "$BUILD_DIR/bin/Yakety.app" 2>&1 | grep -q "Developer ID Application: Mario Zechner"; then
            echo "✅ Yakety.app already properly signed"
            NEEDS_APP_SIGNING=false
        fi
        
        # Sign the app bundle if needed
        if [ "$NEEDS_APP_SIGNING" = true ]; then
            echo "📝 Signing Yakety.app bundle..."
            codesign --force --deep --sign "$DISTRIBUTION_CERT" \
                --options runtime \
                --timestamp \
                "$BUILD_DIR/bin/Yakety.app"
            
            if [ $? -ne 0 ]; then
                echo "❌ Code signing Yakety.app failed"
                exit 1
            fi
        fi
    fi
    
    # Create zip for notarization
    echo "📦 Creating zip for notarization..."
    cd "$BUILD_DIR/bin"
    zip -r ../../yakety-notarize.zip yakety-cli Yakety.app
    cd ../..
    
    # Submit for notarization
    echo "☁️  Submitting for notarization (this may take a few minutes)..."
    xcrun notarytool submit yakety-notarize.zip \
        --apple-id "$APPLE_ID" \
        --team-id "$TEAM_ID" \
        --password "$NOTARY_TOOL_PASSWORD" \
        --wait
    
    if [ $? -ne 0 ]; then
        echo "❌ Notarization failed"
        rm yakety-notarize.zip
        exit 1
    fi
    
    # Staple the app bundle
    if [ -d "$BUILD_DIR/bin/Yakety.app" ]; then
        echo "📎 Stapling notarization to Yakety.app..."
        xcrun stapler staple "$BUILD_DIR/bin/Yakety.app"
    fi
    
    # Clean up
    rm yakety-notarize.zip
    
    # Verify notarization
    echo "✅ Verifying notarization..."
    
    # Verify CLI
    echo "Verifying yakety-cli..."
    codesign -vv "$BUILD_DIR/bin/yakety-cli" 2>&1
    if [ $? -ne 0 ]; then
        echo "❌ yakety-cli signature verification failed"
        exit 1
    fi
    
    spctl -a -vvv -t install "$BUILD_DIR/bin/yakety-cli" 2>&1
    if [ $? -ne 0 ]; then
        echo "❌ yakety-cli failed Gatekeeper verification"
        exit 1
    fi
    
    # Verify app bundle
    if [ -d "$BUILD_DIR/bin/Yakety.app" ]; then
        echo "Verifying Yakety.app..."
        codesign -vv "$BUILD_DIR/bin/Yakety.app" 2>&1
        if [ $? -ne 0 ]; then
            echo "❌ Yakety.app signature verification failed"
            exit 1
        fi
        
        spctl -a -vvv -t install "$BUILD_DIR/bin/Yakety.app" 2>&1
        if [ $? -ne 0 ]; then
            echo "❌ Yakety.app failed Gatekeeper verification"
            exit 1
        fi
    fi
    
    echo "✅ Notarization completed successfully!"
    echo "   All executables are properly signed and notarized."
    echo
fi

# Package if requested (now with notarized binaries)
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
if [ "$NOTARIZE" = true ]; then
    echo "🔏 All done! Built, signed, and notarized successfully."
elif [ "$UPLOAD" = true ]; then
    echo "🚀 All done! Built, packaged, and uploaded successfully."
elif [ "$PACKAGE" = true ]; then
    echo "📦 All done! Built and packaged successfully."
    echo "   To upload packages, run: ./build.sh upload"
else
    echo "🔨 Build complete!"
    echo "   To create packages, run: ./build.sh package"
    echo "   To upload packages, run: ./build.sh package upload"
    echo "   To notarize for distribution, run: ./build.sh notarize"
fi