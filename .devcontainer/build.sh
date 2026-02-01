#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
IMAGE_NAME="yakety-dev"

echo "Building container..."
docker build -t "$IMAGE_NAME" "$SCRIPT_DIR"

echo "Entering container..."
# --device /dev/input enables keyboard access for keylogger
# --group-add input grants input group permissions
docker run -it --rm \
    -v "$PROJECT_DIR:/workspace" \
    -w /workspace \
    "$IMAGE_NAME" \
    bash
