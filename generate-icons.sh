#!/bin/bash

set -e

# Only run on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "⚠️  Icon generation only runs on macOS"
    exit 0
fi

echo "🎨 Checking if icon generation is needed..."

# Check if SVG is newer than generated files
SVG_FILE="assets/yakety.svg"
ICNS_FILE="assets/yakety.icns"
ICO_FILE="assets/yakety.ico"

# Function to check if generation is needed
needs_generation() {
    # If any output file doesn't exist, we need to generate
    if [ ! -f "$ICNS_FILE" ] || [ ! -f "$ICO_FILE" ]; then
        return 0
    fi
    
    # Check if SVG is newer than output files
    if [ "$SVG_FILE" -nt "$ICNS_FILE" ] || [ "$SVG_FILE" -nt "$ICO_FILE" ]; then
        return 0
    fi
    
    # Check if menubar icons exist
    if [ ! -f "assets/generated/menubar.png" ] || [ ! -f "assets/generated/menubar@2x.png" ]; then
        return 0
    fi
    
    return 1
}

# Skip if generation not needed
if ! needs_generation; then
    echo "✅ Icons are up to date"
    exit 0
fi

echo "🔄 Generating icons from yakety.svg..."

# Check if required tools are installed
if ! command -v convert &> /dev/null; then
    echo "❌ ImageMagick not found. Please install it:"
    echo "   brew install imagemagick"
    exit 1
fi

if ! command -v rsvg-convert &> /dev/null; then
    echo "❌ librsvg not found. Please install it:"
    echo "   brew install librsvg"
    exit 1
fi

# Create assets directory for generated icons
mkdir -p assets/generated

# Generate PNG sizes for both macOS and Windows
echo "📐 Generating PNG sizes..."
for size in 16 32 48 64 128 256 512 1024; do
    echo "  - ${size}x${size}"
    rsvg-convert -w $size -h $size assets/yakety.svg -o assets/generated/icon_${size}x${size}.png
done

# Generate @2x versions for Retina displays
echo "📐 Generating @2x versions for macOS..."
echo "  - 16x16@2x"
rsvg-convert -w 32 -h 32 assets/yakety.svg -o assets/generated/icon_16x16@2x.png
echo "  - 32x32@2x"
rsvg-convert -w 64 -h 64 assets/yakety.svg -o assets/generated/icon_32x32@2x.png
echo "  - 128x128@2x"
rsvg-convert -w 256 -h 256 assets/yakety.svg -o assets/generated/icon_128x128@2x.png
echo "  - 256x256@2x"
rsvg-convert -w 512 -h 512 assets/yakety.svg -o assets/generated/icon_256x256@2x.png
echo "  - 512x512@2x"
rsvg-convert -w 1024 -h 1024 assets/yakety.svg -o assets/generated/icon_512x512@2x.png

# Generate menubar icons (22pt for standard, 44pt for @2x)
echo "📐 Generating menubar icons..."
rsvg-convert -w 22 -h 22 assets/yakety.svg -o assets/generated/menubar.png
rsvg-convert -w 44 -h 44 assets/yakety.svg -o assets/generated/menubar@2x.png

# Create iconset directory
echo "📦 Creating iconset..."
mkdir -p assets/yakety.iconset

# Copy all icons to iconset with proper names
cp assets/generated/icon_16x16.png assets/yakety.iconset/icon_16x16.png
cp assets/generated/icon_16x16@2x.png assets/yakety.iconset/icon_16x16@2x.png
cp assets/generated/icon_32x32.png assets/yakety.iconset/icon_32x32.png
cp assets/generated/icon_32x32@2x.png assets/yakety.iconset/icon_32x32@2x.png
cp assets/generated/icon_128x128.png assets/yakety.iconset/icon_128x128.png
cp assets/generated/icon_128x128@2x.png assets/yakety.iconset/icon_128x128@2x.png
cp assets/generated/icon_256x256.png assets/yakety.iconset/icon_256x256.png
cp assets/generated/icon_256x256@2x.png assets/yakety.iconset/icon_256x256@2x.png
cp assets/generated/icon_512x512.png assets/yakety.iconset/icon_512x512.png
cp assets/generated/icon_512x512@2x.png assets/yakety.iconset/icon_512x512@2x.png

# Generate .icns file
echo "🎯 Generating yakety.icns..."
iconutil -c icns assets/yakety.iconset -o assets/yakety.icns

# Clean up iconset directory
rm -rf assets/yakety.iconset

# Windows ICO generation
echo "🎯 Generating yakety.ico..."
convert assets/generated/icon_16x16.png \
        assets/generated/icon_32x32.png \
        assets/generated/icon_48x48.png \
        assets/generated/icon_64x64.png \
        assets/generated/icon_128x128.png \
        assets/generated/icon_256x256.png \
        assets/yakety.ico

echo "✅ Icon generation complete!"
echo ""
echo "Generated files:"
echo "  - assets/yakety.icns               (macOS app icon)"
echo "  - assets/yakety.ico                (Windows icon)"
echo "  - assets/generated/menubar.png     (22x22 menubar icon)"
echo "  - assets/generated/menubar@2x.png  (44x44 menubar icon for Retina)"
echo "  - assets/generated/icon_*.png      (Various PNG sizes)"