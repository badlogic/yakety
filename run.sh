#!/bin/bash

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
RUN=false
NOTARIZE=false

for arg in "$@"; do
    case $arg in
        clean)    CLEAN=true ;;
        debug)    DEBUG=true ;;
        run)      RUN=true ;;
        package)  PACKAGE=true ;;
        upload)   UPLOAD=true ;;
        *)
            echo "Usage: $0 [clean] [debug] [package] [upload] [run]"
            echo ""
            echo "Options:"
            echo "  clean    - Clean previous build directories"
            echo "  debug    - Build with debug preset"
            echo "  run      - Run the executable after building"
            echo "  package  - Create distribution packages (triggers notarization)"
            echo "  upload   - Upload packages to server (triggers notarization)"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ] || [ "$PACKAGE" = true ] || [ "$UPLOAD" = true ]; then
    echo "Cleaning previous build..."
    rm -rf build build-debug whisper.cpp/build
fi

# Determine build directory
BUILD_DIR=$([ "$DEBUG" = true ] && echo "build-debug" || echo "build")

# Configure and build
if [ "$DEBUG" = true ]; then
    echo "Building debug..."
    cmake --preset debug -DSKIP_ADHOC_SIGNING=ON
    cmake --build --preset debug
else
    echo "Building release..."
    cmake --preset release -DSKIP_ADHOC_SIGNING=ON
    cmake --build --preset release
fi

# Always sign with Developer ID for consistent permissions
echo "Signing with Developer ID..."
PROJECT_DIR="$(pwd)"

# Sign CLI
echo "Signing yakety-cli..."
codesign --force --sign "$DISTRIBUTION_CERT" --options runtime --timestamp "$BUILD_DIR/bin/yakety-cli"

if [ -d "$BUILD_DIR/bin/Yakety.app" ]; then
    # First sign all frameworks and nested binaries
    echo "Signing frameworks and binaries in Yakety.app..."
    find "$BUILD_DIR/bin/Yakety.app" -type f -perm +111 -exec codesign --force --sign "$DISTRIBUTION_CERT" --options runtime --timestamp {} \;
    
    # Then sign the app bundle with entitlements
    echo "Signing Yakety.app bundle..."
    codesign --force --sign "$DISTRIBUTION_CERT" --options runtime --timestamp --entitlements "$PROJECT_DIR/src/mac/yakety.entitlements" "$BUILD_DIR/bin/Yakety.app"
    
    # Verify the signature
    echo "Verifying signature..."
    codesign --verify --deep --strict "$BUILD_DIR/bin/Yakety.app"
fi

# Notarize if package/upload requested
if [ "$PACKAGE" = true ] || [ "$UPLOAD" = true ]; then
    if [ -z "$NOTARY_TOOL_PASSWORD" ]; then
        echo "❌ NOTARY_TOOL_PASSWORD not set"
        exit 1
    fi

    echo "Notarizing..."
    cd "$BUILD_DIR/bin"
    zip -r ../../yakety-notarize.zip yakety-cli Yakety.app
    cd ../..

    # Submit for notarization and capture output
    NOTARY_OUTPUT=$(xcrun notarytool submit yakety-notarize.zip \
        --apple-id "$APPLE_ID" \
        --team-id "$TEAM_ID" \
        --password "$NOTARY_TOOL_PASSWORD" \
        --wait 2>&1)
    
    echo "$NOTARY_OUTPUT"
    
    # Extract submission ID and status
    SUBMISSION_ID=$(echo "$NOTARY_OUTPUT" | grep -E "^\s*id:" | head -1 | awk '{print $2}')
    STATUS=$(echo "$NOTARY_OUTPUT" | grep -E "^\s*status:" | tail -1 | awk '{print $2}')
    
    # If status is Invalid, get the log
    if [ "$STATUS" = "Invalid" ] && [ -n "$SUBMISSION_ID" ]; then
        echo ""
        echo "❌ Notarization failed! Getting detailed log..."
        echo ""
        xcrun notarytool log "$SUBMISSION_ID" \
            --apple-id "$APPLE_ID" \
            --team-id "$TEAM_ID" \
            --password "$NOTARY_TOOL_PASSWORD"
        exit 1
    fi

    if [ -d "$BUILD_DIR/bin/Yakety.app" ]; then
        xcrun stapler staple "$BUILD_DIR/bin/Yakety.app"
    fi

    rm yakety-notarize.zip
fi

# Package if requested
if [ "$PACKAGE" = true ]; then
    echo "Creating packages..."
    cmake --build "$BUILD_DIR" --target package
fi

# Upload if requested
if [ "$UPLOAD" = true ]; then
    [ "$PACKAGE" = false ] && cmake --build "$BUILD_DIR" --target package
    echo "Uploading..."
    cmake --build "$BUILD_DIR" --target upload

    # Output download links
    echo ""
    echo "📥 Download links:"
    echo "https://mariozechner.at/uploads/yakety-cli-macos.zip"
    echo "https://mariozechner.at/uploads/Yakety-macos.dmg"
fi

# Run if requested - pick newest executable
if [ "$RUN" = true ]; then
    CLI_RELEASE="build/bin/yakety-cli"
    CLI_DEBUG="build-debug/bin/yakety-cli"

    # Find which exists and is newer
    if [ -f "$CLI_RELEASE" ] && [ -f "$CLI_DEBUG" ]; then
        if [ "$CLI_RELEASE" -nt "$CLI_DEBUG" ]; then
            EXEC="$CLI_RELEASE"
        else
            EXEC="$CLI_DEBUG"
        fi
    elif [ -f "$CLI_RELEASE" ]; then
        EXEC="$CLI_RELEASE"
    elif [ -f "$CLI_DEBUG" ]; then
        EXEC="$CLI_DEBUG"
    else
        echo "❌ No executable found"
        exit 1
    fi

    echo "Running $EXEC..."
    "$EXEC"
fi

echo "✅ Done"