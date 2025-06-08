#!/bin/bash

set -e

echo "🔏 Signing Yakety app..."

# Check if app exists
if [ ! -d "./build/Yakety.app" ]; then
    echo "❌ App not found. Run ./build.sh first."
    exit 1
fi

# Sign with ad-hoc signature (for local use)
echo "📝 Signing with ad-hoc signature..."
codesign --force --deep --sign - "./build/Yakety.app"

# Verify signature
echo "✅ Verifying signature..."
codesign --verify --verbose "./build/Yakety.app"

# Remove quarantine
echo "🔓 Removing quarantine..."
xattr -cr "./build/Yakety.app"

echo "✅ App signed successfully!"
echo ""
echo "You can now run:"
echo "  open ./build/Yakety.app"