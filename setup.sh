#!/bin/bash

set -e

echo "🚀 Setting up Whisperer..."

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Please run this script from the whisperer directory"
    exit 1
fi

# Make scripts executable
chmod +x build.sh
chmod +x package.sh

echo "✅ Setup complete!"
echo ""
echo "Next steps:"
echo "1. Run ./build.sh to build the project"
echo "2. Run ./package.sh to create the app bundle"
echo "3. Or use the pre-built app in dist/Whisperer.app"
