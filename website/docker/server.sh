#!/bin/bash
set -e

if [ "$DEV" = "true" ]; then
    echo "🚀 Starting Yakety website server in development mode"
    node --enable-source-maps --inspect=0.0.0.0:9230 server.cjs
else
    echo "🚀 Starting Yakety website server in production mode"
    node --enable-source-maps server.cjs
fi