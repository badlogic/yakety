#!/usr/bin/env bash

set -e
[ "${DEBUG:-0}" = "1" ] && set -x

# Signing configuration
DISTRIBUTION_CERT="${DISTRIBUTION_CERT-Developer ID Application: Mario Zechner (7F5Y92G2Z4)}"
TEAM_ID="7F5Y92G2Z4"
APPLE_ID="contact@badlogicgames.com"

# Parse command line arguments
CLEAN=false
PACKAGE=false
UPLOAD=false
DEBUG=false
CLI=false
APP=false
NOTARIZE=false

for arg in "$@"; do
    case $arg in
        clean)    CLEAN=true ;;
        debug)    DEBUG=true ;;
        cli)  CLI=true ;;
        app)  APP=true ;;
        package)  PACKAGE=true ;;
        upload)   UPLOAD=true ;;
        *)
            echo "Usage: $0 [clean] [debug] [package] [upload] [cli] [app]"
            echo ""
            echo "Options:"
            echo "  clean    - Clean previous build directories"
            echo "  debug    - Build with debug preset"
            echo "  cli      - Run yakety-cli after building"
            echo "  app      - Run Yakety.app and tail logs after building"
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

PROJECT_DIR="$(pwd)"

# Sign only if `codesign` is available (macOS).
if command -v codesign >/dev/null 2>&1; then
    echo "Signing..."

    declare -a codesign_sign_args=(codesign --force)
    [ -n "${DISTRIBUTION_CERT:-}" ] || DISTRIBUTION_CERT="-"
    codesign_sign_args+=(--sign "$DISTRIBUTION_CERT")

    # Only use hardened runtime + timestamp with a real Developer ID certificate.
    # Ad-hoc signing ("-") has no keychain identity, so --timestamp would fail.
    if [ ! "$DISTRIBUTION_CERT" = "-" ]; then
        codesign_sign_args+=(--options runtime --timestamp)
    fi

    # Sign CLI
    echo "Signing yakety-cli..."
    "${codesign_sign_args[@]}" "$BUILD_DIR/bin/yakety-cli"

    if [ -d "$BUILD_DIR/bin/Yakety.app" ]; then
        echo "Signing frameworks and binaries in Yakety.app..."
        find "$BUILD_DIR/bin/Yakety.app" -type f -perm +111 -exec "${codesign_sign_args[@]}" {} \;

        echo "Signing Yakety.app bundle..."
        "${codesign_sign_args[@]}" --entitlements "$PROJECT_DIR/src/mac/yakety.entitlements" "$BUILD_DIR/bin/Yakety.app"

        echo "Verifying signature..."
        codesign --verify --deep --strict "$BUILD_DIR/bin/Yakety.app"
    fi
else
    echo "codesign not found — skipping signing steps on this platform"
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

# Run CLI if requested
if [ "$CLI" = true ]; then
    EXEC="$BUILD_DIR/bin/yakety-cli"
    if [ ! -f "$EXEC" ]; then
        echo "❌ No CLI executable found at $EXEC"
        exit 1
    fi
    
    echo "Running yakety-cli..."
    "$EXEC"
fi

# Run app if requested
if [ "$APP" = true ]; then
    APP="$BUILD_DIR/bin/Yakety.app"
    if [ ! -d "$APP" ]; then
        echo "❌ No app bundle found at $APP"
        exit 1
    fi
    
    echo "Running Yakety.app..."
    open "$APP"
    echo "Tailing logs..."
    tail -f ~/.yakety/log.txt
fi

echo "✅ Done"
